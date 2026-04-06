#include "core/crash_context.h"

extern "C" {
#include "core/log.h"
#include "platform/platform.h"
#include "platform/screen.h"
}

#include <cstdlib>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr int kMaxCrashScopes = 32;

enum class ErrorReportSeverity {
    Warning,
    Error,
    Fatal
};

struct ScopeState {
    int token = -1;
    int active = 0;
    char stage[256] = { 0 };
    char context[512] = { 0 };
    crash_context_log_callback callback = nullptr;
    void *userdata = nullptr;
};

struct IssueRecord {
    ErrorReportSeverity severity = ErrorReportSeverity::Error;
    std::string title;
    std::string message;
    std::string detail;
    unsigned int count = 0;
};

thread_local ScopeState g_scopes[kMaxCrashScopes];
thread_local int g_scope_depth = 0;
thread_local int g_next_token = 1;
std::unordered_set<std::string> g_reported_dialog_errors;
std::unordered_map<std::string, IssueRecord> g_reported_runtime_issues;
std::vector<std::string> g_runtime_issue_order;
int g_registered_issue_report_atexit = 0;
int g_runtime_issue_counts_flushed = 0;

static void copy_text(char *destination, size_t destination_size, const char *source)
{
    if (!destination || !destination_size) {
        return;
    }

    if (!source) {
        destination[0] = '\0';
        return;
    }

    snprintf(destination, destination_size, "%s", source);
}

static ScopeState *find_scope(int token)
{
    if (token < 0) {
        return nullptr;
    }

    for (int i = g_scope_depth - 1; i >= 0; --i) {
        if (g_scopes[i].active && g_scopes[i].token == token) {
            return &g_scopes[i];
        }
    }
    return nullptr;
}

static ScopeState *current_scope(void)
{
    return g_scope_depth > 0 ? &g_scopes[g_scope_depth - 1] : nullptr;
}

static int push_scope(
    const char *stage,
    const char *context,
    crash_context_log_callback callback,
    void *userdata)
{
    if (g_scope_depth >= kMaxCrashScopes) {
        log_error("Crash context scope stack overflow", stage, 0);
        return -1;
    }

    ScopeState &scope = g_scopes[g_scope_depth++];
    scope = {};
    scope.token = g_next_token++;
    scope.active = 1;
    copy_text(scope.stage, sizeof(scope.stage), stage);
    copy_text(scope.context, sizeof(scope.context), context);
    scope.callback = callback;
    scope.userdata = userdata;
    return scope.token;
}

static void compact_scopes(void)
{
    while (g_scope_depth > 0 && !g_scopes[g_scope_depth - 1].active) {
        g_scopes[g_scope_depth - 1] = {};
        --g_scope_depth;
    }
}

}

CrashContextScope::CrashContextScope(
    const char *stage,
    const char *context,
    crash_context_log_callback callback,
    void *userdata)
    : token_(crash_context_push_scope(stage, context, callback, userdata))
{
}

CrashContextScope::~CrashContextScope()
{
    if (token_ >= 0) {
        crash_context_pop_scope(token_);
    }
}

void CrashContextScope::set_context(const char *context)
{
    if (token_ >= 0) {
        crash_context_update_scope_context(token_, context);
    }
}

void CrashContextScope::set_callback(crash_context_log_callback callback, void *userdata)
{
    if (token_ >= 0) {
        crash_context_update_scope_callback(token_, callback, userdata);
    }
}

static const char *severity_label(ErrorReportSeverity severity)
{
    switch (severity) {
        case ErrorReportSeverity::Warning:
            return "Warning";
        case ErrorReportSeverity::Error:
            return "Error";
        case ErrorReportSeverity::Fatal:
            return "Fatal error";
        default:
            return "Error";
    }
}

static std::string make_issue_key(
    ErrorReportSeverity severity,
    const char *title,
    const char *message,
    const char *detail)
{
    return std::to_string(static_cast<int>(severity)) + "\n" +
        std::string(title ? title : "") + "\n" +
        std::string(message ? message : "") + "\n" +
        std::string(detail ? detail : "");
}

static void flush_issue_counts_internal(void);

static void flush_issue_counts_at_exit(void)
{
    flush_issue_counts_internal();
}

static void ensure_issue_count_flush_registered(void)
{
    if (!g_registered_issue_report_atexit) {
        std::atexit(flush_issue_counts_at_exit);
        g_registered_issue_report_atexit = 1;
    }
}

static void log_issue_message(ErrorReportSeverity severity, const char *message, const char *detail)
{
    switch (severity) {
        case ErrorReportSeverity::Warning:
            log_warning(message ? message : "Warning", detail, 0);
            break;
        case ErrorReportSeverity::Fatal:
        case ErrorReportSeverity::Error:
        default:
            log_error(message ? message : "Error", detail, 0);
            break;
    }
}

static void flush_issue_counts_internal(void)
{
    if (g_runtime_issue_counts_flushed) {
        return;
    }
    g_runtime_issue_counts_flushed = 1;

    for (const std::string &key : g_runtime_issue_order) {
        auto it = g_reported_runtime_issues.find(key);
        if (it == g_reported_runtime_issues.end()) {
            continue;
        }

        const IssueRecord &record = it->second;
        if (record.count <= 1) {
            continue;
        }

        char detail_buffer[1536];
        if (!record.detail.empty()) {
            snprintf(
                detail_buffer,
                sizeof(detail_buffer),
                "%s (occurred %u times total)",
                record.detail.c_str(),
                record.count);
        } else {
            snprintf(
                detail_buffer,
                sizeof(detail_buffer),
                "occurred %u times total",
                record.count);
        }

        log_issue_message(record.severity, record.message.c_str(), detail_buffer);
    }
}

static void report_issue_internal(
    ErrorReportSeverity severity,
    const char *title,
    const char *message,
    const char *detail,
    int show_dialog)
{
    const std::string dedupe_key = make_issue_key(severity, title, message, detail);
    ensure_issue_count_flush_registered();

    IssueRecord &record = g_reported_runtime_issues[dedupe_key];
    const int first_report = record.count == 0 ? 1 : 0;
    if (first_report) {
        record.severity = severity;
        record.title = title ? title : "";
        record.message = message ? message : "";
        record.detail = detail ? detail : "";
        g_runtime_issue_order.push_back(dedupe_key);
    }
    record.count++;

    if (first_report) {
        log_issue_message(severity, message ? message : severity_label(severity), detail);
        error_context_log_current();
    }

    if (!show_dialog) {
        return;
    }

    if (!g_reported_dialog_errors.insert(dedupe_key).second) {
        return;
    }

    flush_issue_counts_internal();

    char box_message[1024];
    snprintf(
        box_message,
        sizeof(box_message),
        "%s\n\n%s%s%s\n\nMore details were written to augustus-log.txt.",
        message ? message : severity_label(severity),
        detail && *detail ? detail : "",
        detail && *detail ? "\n\n" : "",
        "The game will now close after you press OK.");
    platform_screen_show_error_message_box(title ? title : "Vespasian Fatal Error", box_message);
    exit_with_status(1);
}

extern "C" void crash_context_clear(void)
{
    for (int i = 0; i < g_scope_depth; ++i) {
        g_scopes[i] = {};
    }
    g_scope_depth = 0;
    g_next_token = 1;
}

extern "C" int crash_context_push_scope(
    const char *stage,
    const char *context,
    crash_context_log_callback callback,
    void *userdata)
{
    return push_scope(stage, context, callback, userdata);
}

extern "C" void crash_context_pop_scope(int token)
{
    ScopeState *scope = find_scope(token);
    if (!scope) {
        return;
    }

    scope->active = 0;
    compact_scopes();
}

extern "C" void crash_context_update_scope_context(int token, const char *context)
{
    if (ScopeState *scope = find_scope(token)) {
        copy_text(scope->context, sizeof(scope->context), context);
    }
}

extern "C" void crash_context_update_scope_callback(
    int token,
    crash_context_log_callback callback,
    void *userdata)
{
    if (ScopeState *scope = find_scope(token)) {
        scope->callback = callback;
        scope->userdata = userdata;
    }
}

extern "C" void crash_context_set_stage(const char *stage, const char *context)
{
    ScopeState *scope = current_scope();
    if (!scope) {
        push_scope(stage, context, nullptr, nullptr);
        return;
    }

    copy_text(scope->stage, sizeof(scope->stage), stage);
    copy_text(scope->context, sizeof(scope->context), context);
}

extern "C" void error_context_log_current(void)
{
    if (g_scope_depth <= 0) {
        return;
    }

    log_info("Error context scopes", 0, g_scope_depth);
    for (int i = 0; i < g_scope_depth; ++i) {
        ScopeState &scope = g_scopes[i];
        if (!scope.active) {
            continue;
        }

        char label[32];
        snprintf(label, sizeof(label), "Error scope %d", i);
        if (scope.stage[0]) {
            log_info(label, scope.stage, 0);
        }
        if (scope.context[0]) {
            log_info("Error context", scope.context, 0);
        }
        if (scope.callback) {
            scope.callback(scope.userdata);
        }
    }
}

extern "C" void error_context_flush_report_counts(void)
{
    flush_issue_counts_internal();
}

extern "C" void crash_context_log_current(void)
{
    error_context_log_current();
}

extern "C" void error_context_report_warning(const char *message, const char *detail)
{
    report_issue_internal(ErrorReportSeverity::Warning, "Vespasian Warning", message, detail, 0);
}

extern "C" void error_context_report_error(const char *message, const char *detail)
{
    report_issue_internal(ErrorReportSeverity::Error, "Vespasian Error", message, detail, 0);
}

extern "C" void error_context_report_fatal_error_dialog(const char *title, const char *message, const char *detail)
{
    report_issue_internal(ErrorReportSeverity::Fatal, title, message, detail, 1);
}

extern "C" void crash_context_report_error(const char *message, const char *detail)
{
    error_context_report_error(message, detail);
}

extern "C" void crash_context_report_error_dialog(const char *title, const char *message, const char *detail)
{
    error_context_report_fatal_error_dialog(title, message, detail);
}

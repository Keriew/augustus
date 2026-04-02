#include "core/crash_context.h"

extern "C" {
#include "core/log.h"
#include "platform/platform.h"
#include "platform/screen.h"
}

#include <cstdio>
#include <string>
#include <unordered_set>

namespace {

constexpr int kMaxCrashScopes = 32;

struct ScopeState {
    int token = -1;
    int active = 0;
    char stage[256] = { 0 };
    char context[512] = { 0 };
    crash_context_log_callback callback = nullptr;
    void *userdata = nullptr;
};

thread_local ScopeState g_scopes[kMaxCrashScopes];
thread_local int g_scope_depth = 0;
thread_local int g_next_token = 1;
std::unordered_set<std::string> g_reported_dialog_errors;

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

static void report_error_internal(const char *title, const char *message, const char *detail, int show_dialog)
{
    log_error(message ? message : "Crash context error", detail, 0);
    crash_context_log_current();

    if (!show_dialog) {
        return;
    }

    std::string dedupe_key = std::string(title ? title : "") + "\n" +
        std::string(message ? message : "") + "\n" +
        std::string(detail ? detail : "");
    if (!g_reported_dialog_errors.insert(dedupe_key).second) {
        return;
    }

    char box_message[1024];
    snprintf(
        box_message,
        sizeof(box_message),
        "%s\n\n%s%s%s\n\nMore details were written to augustus-log.txt.",
        message ? message : "A runtime error occurred.",
        detail && *detail ? detail : "",
        detail && *detail ? "\n\n" : "",
        "The game will now close after you press OK.");
    platform_screen_show_error_message_box(title ? title : "Vespasian Runtime Error", box_message);
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

extern "C" void crash_context_log_current(void)
{
    if (g_scope_depth <= 0) {
        return;
    }

    log_info("Crash context scopes", 0, g_scope_depth);
    for (int i = 0; i < g_scope_depth; ++i) {
        ScopeState &scope = g_scopes[i];
        if (!scope.active) {
            continue;
        }

        char label[32];
        snprintf(label, sizeof(label), "Crash scope %d", i);
        if (scope.stage[0]) {
            log_info(label, scope.stage, 0);
        }
        if (scope.context[0]) {
            log_info("Crash context", scope.context, 0);
        }
        if (scope.callback) {
            scope.callback(scope.userdata);
        }
    }
}

extern "C" void crash_context_report_error(const char *message, const char *detail)
{
    report_error_internal("Vespasian Runtime Error", message, detail, 0);
}

extern "C" void crash_context_report_error_dialog(const char *title, const char *message, const char *detail)
{
    report_error_internal(title, message, detail, 1);
}

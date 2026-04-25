#pragma once

typedef void (*crash_context_log_callback)(void *userdata);

/**
 * Severity definitions for context-aware runtime reports:
 * Info = expected compatibility or diagnostic note; no action required.
 * Warning = probably unintended, probably safe.
 * Error = definitely unintended, survivable, and does not damage game state, save data, or campaign data.
 * Fatal error = unrecoverable or likely to cause data loss, and the process must stop.
 *
 * Context scopes describe where a report happened. They are not errors by themselves, so log text must
 * use neutral "Context" labels unless the reported severity is actually Error or Fatal error.
 * Info reports should stay concise and omit context scopes. Warning, Error, and Fatal error reports should
 * include the current context block in the same log entry as the message/detail instead of emitting
 * separate context lines.
 *
 * Preferred names for new code are ErrorContextScope and error_context_*.
 * The CrashContextScope / crash_context_* names are kept as compatibility wrappers for the existing codebase.
 * User-facing logs emitted by this module should reserve error terminology for Error and Fatal error reports.
 */

#ifdef __cplusplus
class CrashContextScope {
public:
    CrashContextScope(
        const char *stage,
        const char *context = nullptr,
        crash_context_log_callback callback = nullptr,
        void *userdata = nullptr);
    ~CrashContextScope();

    void set_context(const char *context);
    void set_callback(crash_context_log_callback callback, void *userdata = nullptr);

private:
    int token_ = -1;
};

using ErrorContextScope = CrashContextScope;
#endif

#ifdef __cplusplus
extern "C" {
#endif

void crash_context_clear(void);
int crash_context_push_scope(
    const char *stage,
    const char *context,
    crash_context_log_callback callback,
    void *userdata);
void crash_context_pop_scope(int token);
void crash_context_update_scope_context(int token, const char *context);
void crash_context_update_scope_callback(int token, crash_context_log_callback callback, void *userdata);
void crash_context_set_stage(const char *stage, const char *context);
void crash_context_log_current(void);
void error_context_log_current(void);
void error_context_flush_report_counts(void);
void error_context_report_info(const char *message, const char *detail);
void error_context_report_warning(const char *message, const char *detail);
void error_context_report_error(const char *message, const char *detail);
void error_context_report_fatal_error_dialog(const char *title, const char *message, const char *detail);
void crash_context_report_error(const char *message, const char *detail);
void crash_context_report_error_dialog(const char *title, const char *message, const char *detail);

#ifdef __cplusplus
}
#endif

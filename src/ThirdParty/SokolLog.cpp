#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "../../../External/Sokol/sokol_log.h"

// Custom logger used by this project. Forward logs to OutputDebugStringA
// (visible in Visual Studio Output window) and to stderr.
// Named `slog_to_debug` to avoid clashing with the default `slog_func`
extern "C" void slog_to_debug(const char* tag,
                              uint32_t log_level,
                              uint32_t log_item_id,
                              const char* message_or_null,
                              uint32_t line_nr,
                              const char* filename_or_null,
                              void* user_data)
{
    (void)log_item_id;
    (void)user_data;

    const char* level_str = "INFO";
    switch (log_level) {
        case 0: level_str = "PANIC"; break;
        case 1: level_str = "ERROR"; break;
        case 2: level_str = "WARN";  break;
        case 3: level_str = "INFO";  break;
        default: level_str = "LOG";  break;
    }

    const char* filename = filename_or_null ? filename_or_null : "unknown";
    const char* msg = message_or_null ? message_or_null : "";

    char buf[1024];
    int n = _snprintf_s(buf, sizeof(buf), _TRUNCATE, "[%s] %s (%s:%u) %s\n", level_str, tag, filename, (unsigned)line_nr, msg);
    if (n < 0) buf[sizeof(buf)-1] = '\0';

    // Output to VS Output window
    OutputDebugStringA(buf);

    // Also print to stderr (console / terminal)
    fprintf(stderr, "%s", buf);
    fflush(stderr);
}
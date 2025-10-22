#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Log levels in order of severity
typedef enum {
    LOG_ERROR = 0,
    LOG_WARN  = 1,
    LOG_INFO  = 2,
    LOG_DEBUG = 3
} log_level_t;

// Initialize logger reading LOG_LEVEL env var (ERROR/WARN/INFO/DEBUG)
void log_init(void);

// Set/get log level at runtime
void log_set_level(log_level_t level);
log_level_t log_get_level(void);

// Base logging implementation
void log_log(log_level_t level, const char* module, const char* fmt, ...);

// Color helpers for external use (e.g., statistics).
// Return ANSI codes or empty string if color is disabled for stdout.
const char* log_color_reset(void);
const char* log_color_bold(void);
const char* log_color_blue(void);
const char* log_color_cyan(void);
const char* log_color_green(void);
const char* log_color_yellow(void);
int log_colors_enabled_stdout(void);

// Module tag per file (define LOG_MODULE before including to customize)
#ifndef LOG_MODULE
#define LOG_MODULE "APP"
#endif

// Convenience macros honoring current log level
#define LOGE(fmt, ...) do { \
    if (log_get_level() >= LOG_ERROR) log_log(LOG_ERROR, LOG_MODULE, fmt, ##__VA_ARGS__); \
} while (0)

#define LOGW(fmt, ...) do { \
    if (log_get_level() >= LOG_WARN) log_log(LOG_WARN, LOG_MODULE, fmt, ##__VA_ARGS__); \
} while (0)

#define LOGI(fmt, ...) do { \
    if (log_get_level() >= LOG_INFO) log_log(LOG_INFO, LOG_MODULE, fmt, ##__VA_ARGS__); \
} while (0)

#define LOGD(fmt, ...) do { \
    if (log_get_level() >= LOG_DEBUG) log_log(LOG_DEBUG, LOG_MODULE, fmt, ##__VA_ARGS__); \
} while (0)

#ifdef __cplusplus
}
#endif

#endif // LOG_H

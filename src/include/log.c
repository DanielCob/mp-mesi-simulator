#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   // isatty, fileno
#include <stdbool.h>
#include <pthread.h>

static log_level_t CURRENT_LEVEL = LOG_INFO;

// Modo de color
typedef enum {
    COLOR_AUTO = 0,
    COLOR_ALWAYS,
    COLOR_NEVER
} color_mode_t;

static color_mode_t COLOR_MODE = COLOR_AUTO;
static bool NO_COLOR_ENV = false; // Honor NO_COLOR standard
static pthread_mutex_t LOG_MUTEX = PTHREAD_MUTEX_INITIALIZER; // Serialize output

static log_level_t parse_level(const char* s) {
    if (!s) return LOG_INFO;
    if (strcasecmp(s, "ERROR") == 0) return LOG_ERROR;
    if (strcasecmp(s, "WARN") == 0 || strcasecmp(s, "WARNING") == 0) return LOG_WARN;
    if (strcasecmp(s, "INFO") == 0) return LOG_INFO;
    if (strcasecmp(s, "DEBUG") == 0) return LOG_DEBUG;
    return LOG_INFO;
}

static color_mode_t parse_color_mode(const char* s) {
    if (!s) return COLOR_AUTO;
    if (strcasecmp(s, "auto") == 0) return COLOR_AUTO;
    if (strcasecmp(s, "always") == 0) return COLOR_ALWAYS;
    if (strcasecmp(s, "never") == 0) return COLOR_NEVER;
    if (strcmp(s, "1") == 0 || strcasecmp(s, "true") == 0 || strcasecmp(s, "yes") == 0) return COLOR_ALWAYS;
    if (strcmp(s, "0") == 0 || strcasecmp(s, "false") == 0 || strcasecmp(s, "no") == 0) return COLOR_NEVER;
    return COLOR_AUTO;
}

static bool should_color(FILE* out) {
    if (NO_COLOR_ENV) return false;
    switch (COLOR_MODE) {
        case COLOR_ALWAYS: return true;
        case COLOR_NEVER:  return false;
        case COLOR_AUTO:
        default:
            return isatty(fileno(out));
    }
}

void log_init(void) {
    const char* env = getenv("LOG_LEVEL");
    CURRENT_LEVEL = parse_level(env);
    const char* cenv = getenv("LOG_COLOR");
    COLOR_MODE = parse_color_mode(cenv);
    NO_COLOR_ENV = getenv("NO_COLOR") != NULL; // If defined, disable colors
}

void log_set_level(log_level_t level) { CURRENT_LEVEL = level; }
log_level_t log_get_level(void) { return CURRENT_LEVEL; }

void log_log(log_level_t level, const char* module, const char* fmt, ...) {
    const char* lvl = (level == LOG_ERROR) ? "ERROR" :
                      (level == LOG_WARN)  ? "WARN"  :
                      (level == LOG_INFO)  ? "INFO"  : "DEBUG";

    FILE* out = (level <= LOG_WARN) ? stderr : stdout;

    const char* reset = "\x1b[0m";
    const char* level_color = NULL;
    const char* module_color = NULL;
    if (should_color(out)) {
        switch (level) {
            case LOG_ERROR: level_color = "\x1b[1;31m"; break; // bright red
            case LOG_WARN:  level_color = "\x1b[1;33m"; break; // yellow
            case LOG_INFO:  level_color = "\x1b[1;32m"; break; // green
            case LOG_DEBUG: level_color = "\x1b[1;36m"; break; // cyan
            default: level_color = NULL; break;
        }
        module_color = "\x1b[0;34m"; // blue for module tag
    }

    pthread_mutex_lock(&LOG_MUTEX);
    // Minimal format, no decorative ASCII
    if (level_color && module_color) {
        fprintf(out, "%s[%s]%s%s[%s]%s ", level_color, lvl, reset, module_color, module, reset);
    } else {
        fprintf(out, "[%s][%s] ", lvl, module);
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    // Ensure newline at end
    size_t len = strlen(fmt);
    if (len == 0 || fmt[len-1] != '\n') {
        fputc('\n', out);
    }

    fflush(out);
    pthread_mutex_unlock(&LOG_MUTEX);
}

// ============ Public color helpers ============
static const char* ansi_reset = "\x1b[0m";
static const char* ansi_bold = "\x1b[1m";
static const char* ansi_blue = "\x1b[34m";
static const char* ansi_cyan = "\x1b[36m";
static const char* ansi_green = "\x1b[32m";
static const char* ansi_yellow = "\x1b[33m";

int log_colors_enabled_stdout(void) {
    return should_color(stdout) ? 1 : 0;
}

const char* log_color_reset(void)  { return log_colors_enabled_stdout() ? ansi_reset  : ""; }
const char* log_color_bold(void)   { return log_colors_enabled_stdout() ? ansi_bold   : ""; }
const char* log_color_blue(void)   { return log_colors_enabled_stdout() ? ansi_blue   : ""; }
const char* log_color_cyan(void)   { return log_colors_enabled_stdout() ? ansi_cyan   : ""; }
const char* log_color_green(void)  { return log_colors_enabled_stdout() ? ansi_green  : ""; }
const char* log_color_yellow(void) { return log_colors_enabled_stdout() ? ansi_yellow : ""; }

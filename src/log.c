#include "log.h"
#include <stdlib.h>
#include <string.h>

static log_level_t CURRENT_LEVEL = LOG_INFO;

static log_level_t parse_level(const char* s) {
    if (!s) return LOG_INFO;
    if (strcasecmp(s, "ERROR") == 0) return LOG_ERROR;
    if (strcasecmp(s, "WARN") == 0 || strcasecmp(s, "WARNING") == 0) return LOG_WARN;
    if (strcasecmp(s, "INFO") == 0) return LOG_INFO;
    if (strcasecmp(s, "DEBUG") == 0) return LOG_DEBUG;
    return LOG_INFO;
}

void log_init(void) {
    const char* env = getenv("LOG_LEVEL");
    CURRENT_LEVEL = parse_level(env);
}

void log_set_level(log_level_t level) { CURRENT_LEVEL = level; }
log_level_t log_get_level(void) { return CURRENT_LEVEL; }

void log_log(log_level_t level, const char* module, const char* fmt, ...) {
    const char* lvl = (level == LOG_ERROR) ? "ERROR" :
                      (level == LOG_WARN)  ? "WARN"  :
                      (level == LOG_INFO)  ? "INFO"  : "DEBUG";

    FILE* out = (level <= LOG_WARN) ? stderr : stdout;

    // Formato minimalista, en español, sin ASCII decorativo
    fprintf(out, "[%s][%s] ", lvl, module);

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    // Garantizar salto de línea
    size_t len = strlen(fmt);
    if (len == 0 || fmt[len-1] != '\n') {
        fputc('\n', out);
    }
}

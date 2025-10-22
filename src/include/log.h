#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Niveles de log en orden de severidad
typedef enum {
    LOG_ERROR = 0,
    LOG_WARN  = 1,
    LOG_INFO  = 2,
    LOG_DEBUG = 3
} log_level_t;

// Inicializa el nivel leyendo la variable de entorno LOG_LEVEL (ERROR/WARN/INFO/DEBUG)
void log_init(void);

// Permite fijar nivel en tiempo de ejecución
void log_set_level(log_level_t level);
log_level_t log_get_level(void);

// Implementación base
void log_log(log_level_t level, const char* module, const char* fmt, ...);

// Helpers de color para consumo externo (p.ej., estadísticas).
// Devuelven códigos ANSI o cadena vacía si el color está deshabilitado para stdout.
const char* log_color_reset(void);
const char* log_color_bold(void);
const char* log_color_blue(void);
const char* log_color_cyan(void);
const char* log_color_green(void);
const char* log_color_yellow(void);
int log_colors_enabled_stdout(void);

// Módulo por archivo (definir antes de incluir si se desea custom)
#ifndef LOG_MODULE
#define LOG_MODULE "APP"
#endif

// Macros de conveniencia. Respetan el nivel actual de log.
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

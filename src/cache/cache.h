#ifndef CACHE_H
#define CACHE_H

#include "config.h"
#include "cache_stats.h"
#include <pthread.h>

// Forward declaration para evitar dependencia circular
struct Bus;
typedef struct Bus Bus;

typedef struct {
    unsigned long tag;
    MESI_State state;
    double data[BLOCK_SIZE];
    int valid;
    int lru_bit;
} CacheLine;

typedef struct {
    CacheLine lines[WAYS];
} CacheSet;

typedef struct {
    Bus* bus;
    CacheSet sets[SETS];
    pthread_mutex_t mutex;  // Mutex para proteger acceso concurrente
    CacheStats stats;       // Estadísticas de esta caché
    int pe_id;              // ID del PE dueño de esta caché
} Cache;

void cache_init(Cache* cache);
void cache_destroy(Cache* cache);

double cache_read(Cache* cache, int addr, int pe_id);
void cache_write(Cache* cache, int addr, double value, int pe_id);

// Política de reemplazo
CacheLine* cache_select_victim(Cache* cache, int set_index, int pe_id);

// Funciones auxiliares para el protocolo MESI
CacheLine* cache_get_line(Cache* cache, int addr);
MESI_State cache_get_state(Cache* cache, int addr);
void cache_set_state(Cache* cache, int addr, MESI_State new_state);

// Funciones de acceso a bloques completos (para handlers del bus)
void cache_get_block(Cache* cache, int addr, double block[BLOCK_SIZE]);
void cache_set_block(Cache* cache, int addr, const double block[BLOCK_SIZE]);

// Función para hacer writeback de todas las líneas modificadas (para HALT)
void cache_flush(Cache* cache, int pe_id);

#endif

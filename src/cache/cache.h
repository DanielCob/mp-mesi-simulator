#ifndef CACHE_H
#define CACHE_H

#include "config.h"
#include "cache_stats.h"
#include <pthread.h>

// FORWARD DECLARATIONS
struct Bus;
typedef struct Bus Bus;

// ESTRUCTURAS

/**
 * Línea de cache
 * Contiene un bloque de BLOCK_SIZE doubles con metadatos
 */
typedef struct {
    unsigned long tag;          // Tag de la dirección
    MESI_State state;           // Estado MESI (M, E, S, I)
    double data[BLOCK_SIZE];    // Datos del bloque
    int valid;                  // 1 = válida, 0 = inválida
    int lru_bit;                // Bit LRU para reemplazo
} CacheLine;

/**
 * Conjunto de cache
 * Contiene WAYS líneas
 */
typedef struct {
    CacheLine lines[WAYS];      // Líneas del conjunto
} CacheSet;

/**
 * Cache L1 privado con protocolo MESI
 * Thread-safe mediante mutex
 */
typedef struct {
    Bus* bus;                   // Referencia al bus compartido
    CacheSet sets[SETS];        // Array de conjuntos
    pthread_mutex_t mutex;      // Mutex para sincronización
    CacheStats stats;           // Estadísticas de accesos
    int pe_id;                  // ID del PE propietario
} Cache;

// FUNCIONES PÚBLICAS

// Inicialización y limpieza
void cache_init(Cache* cache);
void cache_destroy(Cache* cache);

// Operaciones de lectura/escritura
double cache_read(Cache* cache, int addr, int pe_id);
void cache_write(Cache* cache, int addr, double value, int pe_id);

// Política de reemplazo
CacheLine* cache_select_victim(Cache* cache, int set_index, int pe_id);

// Operaciones de coherencia MESI
CacheLine* cache_get_line(Cache* cache, int addr);
MESI_State cache_get_state(Cache* cache, int addr);
void cache_set_state(Cache* cache, int addr, MESI_State new_state);

// Operaciones de bloque
void cache_get_block(Cache* cache, int addr, double block[BLOCK_SIZE]);
void cache_set_block(Cache* cache, int addr, const double block[BLOCK_SIZE]);

// Flush
void cache_flush(Cache* cache, int pe_id);

#endif

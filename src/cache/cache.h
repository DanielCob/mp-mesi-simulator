#ifndef CACHE_H
#define CACHE_H

#include "include/config.h"
#include "mesi/mesi.h"
#include <pthread.h>

// Forward declaration para evitar dependencia circular
struct Bus;
typedef struct Bus Bus;

typedef struct {
    unsigned long tag;
    MESI_State state;
    double data[BLOCK_SIZE];
    int valid;
} CacheLine;

typedef struct {
    CacheLine lines[WAYS];
} CacheSet;

typedef struct {
    Bus* bus;
    CacheSet sets[SETS];
    pthread_mutex_t mutex;  // Mutex para proteger acceso concurrente
} Cache;

void cache_init(Cache* cache);
void cache_destroy(Cache* cache);  // Nueva función para limpiar recursos

// Funciones que requieren alineamiento (addr debe ser múltiplo de BLOCK_SIZE)
double cache_read(Cache* cache, int addr, int pe_id);   // Requiere IS_ALIGNED(addr)
void cache_write(Cache* cache, int addr, double value, int pe_id);  // Requiere IS_ALIGNED(addr)

// Política de reemplazo
CacheLine* cache_select_victim(Cache* cache, int set_index, int pe_id);

// Funciones auxiliares para el protocolo MESI
CacheLine* cache_get_line(Cache* cache, int addr);
MESI_State cache_get_state(Cache* cache, int addr);
void cache_set_state(Cache* cache, int addr, MESI_State new_state);
double cache_get_data(Cache* cache, int addr);
void cache_set_data(Cache* cache, int addr, double data);

#endif

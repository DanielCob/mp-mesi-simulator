#include "cache.h"
#include "bus.h"
#include <stdio.h>

// ESTRUCTURAS Y CALLBACKS PRIVADOS

/**
 * Contexto para callback de escritura en BUS_RDX
 * Permite escribir el valor atómicamente después de que el handler traiga el bloque
 */
typedef struct {
    CacheLine* victim;
    int offset;
    double value;
    int set_index;
    int victim_way;
    int pe_id;
} WriteCallbackContext;

/**
 * Callback ejecutado por el bus thread después del handler BUS_RDX
 * Escribe el valor en la cache y actualiza el estado a Modified
 */
static void write_callback(void* context) {
    WriteCallbackContext* ctx = (WriteCallbackContext*)context;
    
    ctx->victim->data[ctx->offset] = ctx->value;
    ctx->victim->state = M;
    
    printf("[CALLBACK PE%d] WRITE completado -> way %d, offset %d, valor=%.2f (estado M)\n", 
           ctx->pe_id, ctx->victim_way, ctx->offset, ctx->value);
}

// FUNCIONES DE INICIALIZACIÓN Y LIMPIEZA

void cache_init(Cache* cache) {
    cache->bus = NULL;
    cache->pe_id = -1;
    
    pthread_mutex_init(&cache->mutex, NULL);
    stats_init(&cache->stats);
    
    for (int i = 0; i < SETS; i++) {
        for (int j = 0; j < WAYS; j++) {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].state = I;
            cache->sets[i].lines[j].lru_bit = 0;
        }
    }
}

void cache_destroy(Cache* cache) {
    pthread_mutex_destroy(&cache->mutex);
}

// POLÍTICA LRU (Least Recently Used)

/**
 * Actualizar bits LRU al acceder a una línea
 * El way accedido se marca como recientemente usado (1)
 * Los demás ways se marcan como menos recientemente usados (0)
 */
static void cache_update_lru(Cache* cache, int set_index, int accessed_way) {
    CacheSet* set = &cache->sets[set_index];
    for (int i = 0; i < WAYS; i++) {
        set->lines[i].lru_bit = (i == accessed_way) ? 1 : 0;
    }
}

// OPERACIONES DE LECTURA Y ESCRITURA

double cache_read(Cache* cache, int addr, int pe_id) {
    int block_base = GET_BLOCK_BASE(addr);
    int offset = GET_BLOCK_OFFSET(addr);
    
    if (offset != 0) {
        printf("[PE%d] READ addr=%d → block_base=%d, offset=%d\n", 
               pe_id, addr, block_base, offset);
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    int set_index = block_base % SETS;
    unsigned long tag = block_base / SETS;
    CacheSet* set = &cache->sets[set_index];

    // Buscar HIT en el cache
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            if (state == M || state == E || state == S) {
                double result = set->lines[i].data[offset];
                cache_update_lru(cache, set_index, i);
                stats_record_read_hit(&cache->stats);
                printf("[PE%d] READ HIT en set %d (way %d, estado %c, offset %d) -> valor=%.2f\n", 
                       pe_id, set_index, i, "MESI"[state], offset, result);
                pthread_mutex_unlock(&cache->mutex);
                return result;
            }
            
            printf("[PE%d] READ tag match pero estado I en set %d (way %d)\n", 
                   pe_id, set_index, i);
            break;
        }
    }

    // MISS: traer línea del bus
    stats_record_read_miss(&cache->stats);
    stats_record_bus_traffic(&cache->stats, BLOCK_SIZE * sizeof(double), 0);
    printf("[PE%d] READ MISS en set %d, enviando BUS_RD\n", pe_id, set_index);

    CacheLine* victim = cache_select_victim(cache, set_index, pe_id);
    int victim_way = victim - set->lines;

    victim->valid = 1;
    victim->tag = tag;
    victim->state = I;

    pthread_mutex_unlock(&cache->mutex);
    bus_broadcast(cache->bus, BUS_RD, block_base, pe_id);
    pthread_mutex_lock(&cache->mutex);
    
    double result = victim->data[offset];
    cache_update_lru(cache, set_index, victim_way);
    printf("[PE%d] READ completado -> bloque traído en way %d, offset %d, valor=%.2f (estado %c)\n", 
           pe_id, victim_way, offset, result, "MESI"[victim->state]);
    pthread_mutex_unlock(&cache->mutex);
    return result;
}

void cache_write(Cache* cache, int addr, double value, int pe_id) {
    int block_base = GET_BLOCK_BASE(addr);
    int offset = GET_BLOCK_OFFSET(addr);
    
    if (offset != 0) {
        printf("[PE%d] WRITE addr=%d → block_base=%d, offset=%d\n", 
               pe_id, addr, block_base, offset);
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    int set_index = block_base % SETS;
    unsigned long tag = block_base / SETS;
    CacheSet* set = &cache->sets[set_index];

    // Buscar si la línea ya está en el cache
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            if (state == M) {
                // HIT en M: ya tenemos permisos exclusivos
                set->lines[i].data[offset] = value;
                cache_update_lru(cache, set_index, i);
                stats_record_write_hit(&cache->stats);
                printf("[PE%d] WRITE HIT en set %d (way %d, estado M, offset %d) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, offset, value);
                pthread_mutex_unlock(&cache->mutex);
                return;
            } 
            else if (state == E) {
                // HIT en E: escribir y cambiar a M
                set->lines[i].data[offset] = value;
                MESI_State old_state = set->lines[i].state;
                set->lines[i].state = M;
                stats_record_mesi_transition(&cache->stats, old_state, M);
                cache_update_lru(cache, set_index, i);
                stats_record_write_hit(&cache->stats);
                printf("[PE%d] WRITE HIT en set %d (way %d, estado E->M, offset %d) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, offset, value);
                pthread_mutex_unlock(&cache->mutex);
                return;
            } 
            else if (state == S) {
                // HIT en S: invalidar otras copias con BUS_UPGR
                stats_record_write_hit(&cache->stats);
                cache->stats.bus_upgrades++;
                printf("[PE%d] WRITE HIT en set %d (way %d, estado S->M, offset %d, enviando BUS_UPGR) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, offset, value);
                
                pthread_mutex_unlock(&cache->mutex);
                bus_broadcast(cache->bus, BUS_UPGR, block_base, pe_id);
                pthread_mutex_lock(&cache->mutex);
                
                set->lines[i].data[offset] = value;
                MESI_State old_state = set->lines[i].state;
                set->lines[i].state = M;
                stats_record_mesi_transition(&cache->stats, old_state, M);
                cache_update_lru(cache, set_index, i);
                pthread_mutex_unlock(&cache->mutex);
                return;
            }
            break;
        }
    }

    // MISS: traer línea con BUS_RDX y escribir con callback
    stats_record_write_miss(&cache->stats);
    stats_record_bus_traffic(&cache->stats, BLOCK_SIZE * sizeof(double), 0);
    printf("[PE%d] WRITE MISS en set %d, enviando BUS_RDX -> valor a escribir=%.2f\n", 
           pe_id, set_index, value);

    CacheLine* victim = cache_select_victim(cache, set_index, pe_id);
    int victim_way = victim - set->lines;

    victim->valid = 1;
    victim->tag = tag;
    victim->state = I;

    // Preparar contexto para el callback
    WriteCallbackContext ctx = {
        .victim = victim,
        .offset = offset,
        .value = value,
        .set_index = set_index,
        .victim_way = victim_way,
        .pe_id = pe_id
    };

    pthread_mutex_unlock(&cache->mutex);
    
    // El handler traerá el bloque, el callback escribirá el valor
    bus_broadcast_with_callback(cache->bus, BUS_RDX, block_base, pe_id, 
                                 write_callback, &ctx);

    pthread_mutex_lock(&cache->mutex);
    cache_update_lru(cache, set_index, victim_way);
    pthread_mutex_unlock(&cache->mutex);
}

// SELECCIÓN DE VÍCTIMA Y POLÍTICAS DE REEMPLAZO

CacheLine* cache_select_victim(Cache* cache, int set_index, int pe_id) {
    CacheSet* set = &cache->sets[set_index];
    CacheLine* victim = NULL;

    // Prioridad 1: Reutilizar línea inválida con tag correcto
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].state == I) {
            victim = &set->lines[i];
            printf("[PE%d] Víctima: way %d (línea en estado I, reutilizando)\n", pe_id, i);
            return victim;
        }
    }
    
    // Prioridad 2: Buscar línea inválida
    for (int i = 0; i < WAYS; i++) {
        if (!set->lines[i].valid) {
            victim = &set->lines[i];
            printf("[PE%d] Víctima: way %d (línea inválida)\n", pe_id, i);
            return victim;
        }
    }
    
    // Prioridad 3: Política LRU - línea con lru_bit = 0
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].lru_bit == 0) {
            victim = &set->lines[i];
            printf("[PE%d] Víctima: way %d (política LRU)\n", pe_id, i);
            break;
        }
    }
    
    // Fallback: usar way 0
    if (victim == NULL) {
        victim = &set->lines[0];
        printf("[PE%d] Víctima: way 0 (fallback)\n", pe_id);
    }
    
    // Writeback si la víctima está en estado M
    if (victim->state == M) {
        int victim_addr = (int)(victim->tag * SETS + set_index);
        cache->stats.bus_writebacks++;
        stats_record_bus_traffic(&cache->stats, 0, BLOCK_SIZE * sizeof(double));
        printf("[PE%d] Evicting línea M (addr=%d), haciendo writeback\n", 
               pe_id, victim_addr);
        bus_broadcast(cache->bus, BUS_WB, victim_addr, pe_id);
    }
    
    return victim;
}

// FUNCIONES AUXILIARES PARA HANDLERS DEL BUS

CacheLine* cache_get_line(Cache* cache, int addr) {
    int set_index = addr % SETS;
    unsigned long tag = addr / SETS;
    CacheSet* set = &cache->sets[set_index];

    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            return &set->lines[i];
        }
    }
    return NULL;
}

MESI_State cache_get_state(Cache* cache, int addr) {
    pthread_mutex_lock(&cache->mutex);
    CacheLine* line = cache_get_line(cache, addr);
    MESI_State state = line ? line->state : I;
    pthread_mutex_unlock(&cache->mutex);
    return state;
}

void cache_set_state(Cache* cache, int addr, MESI_State new_state) {
    pthread_mutex_lock(&cache->mutex);
    CacheLine* line = cache_get_line(cache, addr);
    if (line) {
        MESI_State old_state = line->state;
        line->state = new_state;
        
        if (old_state != new_state) {
            stats_record_mesi_transition(&cache->stats, old_state, new_state);
        }
        
        if (new_state == I && old_state != I) {
            stats_record_invalidation_received(&cache->stats);
        }
    }
    pthread_mutex_unlock(&cache->mutex);
}

void cache_get_block(Cache* cache, int addr, double block[BLOCK_SIZE]) {
    pthread_mutex_lock(&cache->mutex);
    CacheLine* line = cache_get_line(cache, addr);
    if (line) {
        for (int i = 0; i < BLOCK_SIZE; i++) {
            block[i] = line->data[i];
        }
    } else {
        for (int i = 0; i < BLOCK_SIZE; i++) {
            block[i] = 0.0;
        }
    }
    pthread_mutex_unlock(&cache->mutex);
}

void cache_set_block(Cache* cache, int addr, const double block[BLOCK_SIZE]) {
    pthread_mutex_lock(&cache->mutex);
    CacheLine* line = cache_get_line(cache, addr);
    if (line) {
        for (int i = 0; i < BLOCK_SIZE; i++) {
            line->data[i] = block[i];
        }
    }
    pthread_mutex_unlock(&cache->mutex);
}

// CACHE FLUSH (WRITEBACK AL HALT)

/**
 * Hacer writeback de todas las líneas modificadas
 * Se llama cuando un PE ejecuta HALT
 */
void cache_flush(Cache* cache, int pe_id) {
    printf("[PE%d] Flushing all modified cache lines...\n", pe_id);
    
    int modified_blocks[SETS * WAYS];
    int count = 0;
    
    pthread_mutex_lock(&cache->mutex);
    
    for (int set = 0; set < SETS; set++) {
        for (int way = 0; way < WAYS; way++) {
            CacheLine* line = &cache->sets[set].lines[way];
            if (line->valid && line->state == M) {
                modified_blocks[count++] = line->tag * SETS + set;
            }
        }
    }
    
    pthread_mutex_unlock(&cache->mutex);
    
    for (int i = 0; i < count; i++) {
        bus_broadcast(cache->bus, BUS_WB, modified_blocks[i], pe_id);
    }
    
    printf("[PE%d] Flush complete: %d line(s) written back\n", pe_id, count);
}

#include "cache.h"
#include "bus.h"
#include <stdio.h>

// Estructura para pasar contexto al callback de BUS_RDX (OPCIÓN B)
typedef struct {
    CacheLine* victim;
    int offset;
    double value;
    int set_index;
    int victim_way;
    int pe_id;
} WriteCallbackContext;

// Callback que se ejecuta después del handler BUS_RDX
static void write_callback(void* context) {
    WriteCallbackContext* ctx = (WriteCallbackContext*)context;
    
    // Escribir el valor en el offset correcto y marcar como M
    ctx->victim->data[ctx->offset] = ctx->value;
    ctx->victim->state = M;
    
    printf("[CALLBACK PE%d] WRITE completado -> way %d, offset %d, valor=%.2f (estado M)\n", 
           ctx->pe_id, ctx->victim_way, ctx->offset, ctx->value);
}

void cache_init(Cache* cache) {
    cache->bus = NULL;
    cache->pe_id = -1;  // Se asignará después
    
    // Inicializar mutex
    pthread_mutex_init(&cache->mutex, NULL);
    
    // Inicializar estadísticas
    stats_init(&cache->stats);
    
    for (int i = 0; i < SETS; i++)
        for (int j = 0; j < WAYS; j++) {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].state = I;
            cache->sets[i].lines[j].lru_bit = 0;
        }
}

void cache_destroy(Cache* cache) {
    // Destruir mutex
    pthread_mutex_destroy(&cache->mutex);
}

// Actualizar bit LRU cuando se accede a una línea
// El way accedido se marca como recientemente usado (1)
// Los demás ways del mismo set se marcan como menos recientemente usados (0)
static void cache_update_lru(Cache* cache, int set_index, int accessed_way) {
    CacheSet* set = &cache->sets[set_index];
    for (int i = 0; i < WAYS; i++) {
        if (i == accessed_way) {
            set->lines[i].lru_bit = 1;  // Marcar como recientemente usado
        } else {
            set->lines[i].lru_bit = 0;  // Marcar como menos recientemente usado
        }
    }
}

double cache_read(Cache* cache, int addr, int pe_id) {
    // Extraer dirección base del bloque y offset
    int block_base = GET_BLOCK_BASE(addr);
    int offset = GET_BLOCK_OFFSET(addr);
    
    if (offset != 0) {
        printf("[PE%d] READ addr=%d → block_base=%d, offset=%d\n", 
               pe_id, addr, block_base, offset);
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    // El cache trabaja con direcciones de bloque (base)
    int set_index = block_base % SETS;
    unsigned long tag = block_base / SETS;
    CacheSet* set = &cache->sets[set_index];

    // Buscar hit en el cache
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            // HIT: estados M, E o S pueden servir la lectura directamente
            if (state == M || state == E || state == S) {
                double result = set->lines[i].data[offset];  // ← Usa el offset aquí
                cache_update_lru(cache, set_index, i);  // ← Actualizar LRU
                stats_record_read_hit(&cache->stats);  // ← Registrar hit
                printf("[PE%d] READ HIT en set %d (way %d, estado %c, offset %d, LRU actualizado) -> valor=%.2f\n", 
                       pe_id, set_index, i, "MESI"[state], offset, result);
                pthread_mutex_unlock(&cache->mutex);
                return result;
            }
            
            // Línea con tag correcto pero invalidada (estado I)
            // La reutilizaremos después del broadcast
            printf("[PE%d] READ tag match pero estado I en set %d (way %d)\n", 
                   pe_id, set_index, i);
            break;
        }
    }

    // MISS: No hay hit válido, necesitamos traer la línea
    stats_record_read_miss(&cache->stats);  // ← Registrar miss
    stats_record_bus_traffic(&cache->stats, BLOCK_SIZE * sizeof(double), 0);  // ← Tráfico de lectura
    printf("[PE%d] READ MISS en set %d, enviando BUS_RD\n", pe_id, set_index);

    // Seleccionar línea víctima usando la política de reemplazo LRU
    CacheLine* victim = cache_select_victim(cache, set_index, pe_id);
    
    // Calcular el índice del way de la víctima para actualizar LRU
    int victim_way = victim - set->lines;

    // Preparar la línea para recibir datos
    victim->valid = 1;
    victim->tag = tag;
    victim->state = I;  // Temporalmente inválido antes del broadcast

    // IMPORTANTE: Desbloquear cache local antes de usar el bus
    // para evitar deadlock (el bus necesitará lockear otras cachés)
    pthread_mutex_unlock(&cache->mutex);
    
    // Enviar BUS_RD - el handler actualizará el estado y datos (trae bloque completo)
    bus_broadcast(cache->bus, BUS_RD, block_base, pe_id);
    
    // Re-lockear para leer el resultado
    pthread_mutex_lock(&cache->mutex);
    
    double result = victim->data[offset];  // ← Usa el offset aquí
    cache_update_lru(cache, set_index, victim_way);  // ← Actualizar LRU después de traer el bloque
    printf("[PE%d] READ completado -> bloque traído en way %d, offset %d, valor=%.2f (estado %c, LRU actualizado)\n", 
           pe_id, victim_way, offset, result, "MESI"[victim->state]);
    pthread_mutex_unlock(&cache->mutex);
    return result;
}

void cache_write(Cache* cache, int addr, double value, int pe_id) {
    // Extraer dirección base del bloque y offset
    int block_base = GET_BLOCK_BASE(addr);
    int offset = GET_BLOCK_OFFSET(addr);
    
    if (offset != 0) {
        printf("[PE%d] WRITE addr=%d → block_base=%d, offset=%d\n", 
               pe_id, addr, block_base, offset);
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    // El cache trabaja con direcciones de bloque (base)
    int set_index = block_base % SETS;
    unsigned long tag = block_base / SETS;
    CacheSet* set = &cache->sets[set_index];

    // Buscar si la línea ya está en el cache
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            if (state == M) {
                // HIT en M: escribir directamente, ya tenemos permisos exclusivos
                set->lines[i].data[offset] = value;  // ← Usa el offset aquí
                cache_update_lru(cache, set_index, i);  // ← Actualizar LRU
                stats_record_write_hit(&cache->stats);  // ← Registrar hit
                printf("[PE%d] WRITE HIT en set %d (way %d, estado M, offset %d, LRU actualizado) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, offset, value);
                pthread_mutex_unlock(&cache->mutex);
                return;
            } 
            else if (state == E) {
                // HIT en E: escribir y cambiar a M
                set->lines[i].data[offset] = value;  // ← Usa el offset aquí
                MESI_State old_state = set->lines[i].state;
                set->lines[i].state = M;
                stats_record_mesi_transition(&cache->stats, old_state, M);  // ← E→M
                cache_update_lru(cache, set_index, i);  // ← Actualizar LRU
                stats_record_write_hit(&cache->stats);  // ← Registrar hit
                printf("[PE%d] WRITE HIT en set %d (way %d, estado E->M, offset %d, LRU actualizado) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, offset, value);
                pthread_mutex_unlock(&cache->mutex);
                return;
            } 
            else if (state == S) {
                // HIT en S: necesitamos invalidar otras copias con BUS_UPGR
                stats_record_write_hit(&cache->stats);  // ← Registrar hit
                cache->stats.bus_upgrades++;  // ← Registrar upgrade
                printf("[PE%d] WRITE HIT en set %d (way %d, estado S->M, offset %d, enviando BUS_UPGR) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, offset, value);
                
                // Desbloquear antes de usar el bus
                pthread_mutex_unlock(&cache->mutex);
                bus_broadcast(cache->bus, BUS_UPGR, block_base, pe_id);
                
                // Re-lockear para actualizar
                pthread_mutex_lock(&cache->mutex);
                set->lines[i].data[offset] = value;  // ← Usa el offset aquí
                MESI_State old_state = set->lines[i].state;
                set->lines[i].state = M;
                stats_record_mesi_transition(&cache->stats, old_state, M);  // ← S→M
                cache_update_lru(cache, set_index, i);  // ← Actualizar LRU
                pthread_mutex_unlock(&cache->mutex);
                return;
            }
            // Si estado == I, continuar al MISS (línea invalidada)
            break;
        }
    }

    // MISS: No hay línea válida con ese tag
    stats_record_write_miss(&cache->stats);  // ← Registrar miss
    stats_record_bus_traffic(&cache->stats, BLOCK_SIZE * sizeof(double), 0);  // ← Tráfico
    printf("[PE%d] WRITE MISS en set %d, enviando BUS_RDX -> valor a escribir=%.2f\n", 
           pe_id, set_index, value);

    // Seleccionar línea víctima usando la política de reemplazo LRU
    CacheLine* victim = cache_select_victim(cache, set_index, pe_id);
    
    // Calcular el índice del way de la víctima para actualizar LRU
    int victim_way = victim - set->lines;

    // Preparar la línea para la escritura
    victim->valid = 1;
    victim->tag = tag;
    victim->state = I;  // Temporalmente inválido antes del broadcast

    // OPCIÓN B: Preparar contexto para el callback
    WriteCallbackContext ctx = {
        .victim = victim,
        .offset = offset,
        .value = value,
        .set_index = set_index,
        .victim_way = victim_way,
        .pe_id = pe_id
    };

    // Desbloquear antes de usar el bus
    pthread_mutex_unlock(&cache->mutex);
    
    // Enviar BUS_RDX con callback - el handler traerá el bloque y el callback escribirá
    bus_broadcast_with_callback(cache->bus, BUS_RDX, block_base, pe_id, 
                                 write_callback, &ctx);

    // Re-lockear para actualizar LRU
    pthread_mutex_lock(&cache->mutex);
    
    // El callback ya escribió el valor y puso estado M
    cache_update_lru(cache, set_index, victim_way);  // ← Actualizar LRU
    
    pthread_mutex_unlock(&cache->mutex);
}

CacheLine* cache_select_victim(Cache* cache, int set_index, int pe_id) {
    CacheSet* set = &cache->sets[set_index];
    CacheLine* victim = NULL;

    // Política 0: Si hay una línea inválida con el tag correcto, reutilizarla
    // Esto previene seleccionar una víctima diferente cuando ya tenemos el slot correcto
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].state == I) {
            victim = &set->lines[i];
            printf("[PE%d] Víctima: way %d (línea en estado I, reutilizando)\n", pe_id, i);
            return victim;
        }
    }
    
    // Política 1: Buscar primera línea inválida
    for (int i = 0; i < WAYS; i++) {
        if (!set->lines[i].valid) {
            victim = &set->lines[i];
            printf("[PE%d] Víctima: way %d (línea inválida)\n", pe_id, i);
            return victim;
        }
    }
    
    // Política 2 (LRU): Si todas las líneas son válidas, usar política LRU
    // Seleccionar la línea con lru_bit = 0 (menos recientemente usada)
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].lru_bit == 0) {
            victim = &set->lines[i];
            printf("[PE%d] Víctima: way %d (política LRU - bit=0, menos recientemente usado)\n", pe_id, i);
            break;
        }
    }
    
    // Fallback: Si por alguna razón no encontramos víctima (todos tienen bit=1), usar way 0
    if (victim == NULL) {
        victim = &set->lines[0];
        printf("[PE%d] Víctima: way 0 (fallback LRU)\n", pe_id);
    }
    
    // Si la víctima está en M, hacer writeback antes de reemplazar
    if (victim->state == M) {
        int victim_addr = (int)(victim->tag * SETS + set_index);
        cache->stats.bus_writebacks++;  // ← Registrar writeback
        stats_record_bus_traffic(&cache->stats, 0, BLOCK_SIZE * sizeof(double));  // ← Tráfico de escritura
        printf("[PE%d] Evicting línea M (addr=%d), haciendo writeback\n", 
               pe_id, victim_addr);
        bus_broadcast(cache->bus, BUS_WB, victim_addr, pe_id);
    }
    
    return victim;
}

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
        
        // Registrar transición MESI
        if (old_state != new_state) {
            stats_record_mesi_transition(&cache->stats, old_state, new_state);
        }
        
        // Registrar invalidación si transición termina en I
        if (new_state == I && old_state != I) {
            stats_record_invalidation_received(&cache->stats);
        }
    }
    pthread_mutex_unlock(&cache->mutex);
}

// Obtener bloque completo (4 doubles) - Para handlers del bus
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

// Establecer bloque completo (4 doubles) - Para handlers del bus
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

/**
 * Hacer writeback de todas las líneas modificadas en la caché
 * Se llama cuando un PE ejecuta HALT para asegurar que todos los datos
 * modificados se escriban a memoria principal a través del bus
 */
void cache_flush(Cache* cache, int pe_id) {
    printf("[PE%d] Flushing all modified cache lines...\n", pe_id);
    
    // Recolectar direcciones de bloques modificados (liberamos mutex antes del bus)
    int modified_blocks[SETS * WAYS];
    int count = 0;
    
    pthread_mutex_lock(&cache->mutex);
    
    for (int set = 0; set < SETS; set++) {
        for (int way = 0; way < WAYS; way++) {
            CacheLine* line = &cache->sets[set].lines[way];
            
            if (line->valid && line->state == M) {
                // Calcular dirección: tag * SETS + set
                modified_blocks[count++] = line->tag * SETS + set;
            }
        }
    }
    
    pthread_mutex_unlock(&cache->mutex);
    
    // Hacer writeback de cada bloque modificado a través del bus
    for (int i = 0; i < count; i++) {
        bus_broadcast(cache->bus, BUS_WB, modified_blocks[i], pe_id);
    }
    
    printf("[PE%d] Flush complete: %d line(s) written back\n", pe_id, count);
}

#define LOG_MODULE "CACHE"
#include "cache.h"
#include "bus.h"
#include <stdio.h>
#include "log.h"

// ESTRUCTURAS PRIVADAS

typedef struct {
    CacheLine* victim;
    int offset;
    double value;
    int set_index;
    int victim_way;
    int pe_id;
} WriteCallbackContext;

static void write_callback(void* context) {
    WriteCallbackContext* ctx = (WriteCallbackContext*)context;
    
    // Escribir el valor en el bloque
    ctx->victim->data[ctx->offset] = ctx->value;
    
    // Cambiar estado a Modified (I->M ya se registró en handler)
    ctx->victim->state = M;
    
    LOGD("PE%d write callback: way=%d offset=%d valor=%.2f estado=M", 
        ctx->pe_id, ctx->victim_way, ctx->offset, ctx->value);
}

// INICIALIZACIÓN Y LIMPIEZA

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

// POLÍTICA LRU

static void cache_update_lru(Cache* cache, int set_index, int accessed_way) {
    CacheSet* set = &cache->sets[set_index];
    for (int i = 0; i < WAYS; i++) {
        set->lines[i].lru_bit = (i == accessed_way) ? 1 : 0;
    }
}

// OPERACIONES DE LECTURA Y ESCRITURA

double cache_read(Cache* cache, int addr, int pe_id) {
    // Calcular dirección base del bloque y offset dentro del bloque
    int block_base = GET_BLOCK_BASE(addr);
    int offset = GET_BLOCK_OFFSET(addr);
    
    // Log solo si hay offset (dirección no alineada)
    if (offset != 0) {
        LOGD("PE%d read: addr=%d base=%d offset=%d", pe_id, addr, block_base, offset);
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    // Calcular set_index y tag para buscar en la caché
    int set_index = block_base % SETS;
    unsigned long tag = block_base / SETS;
    CacheSet* set = &cache->sets[set_index];

    // BÚSQUEDA DE HIT EN LA CACHÉ
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            // HIT: la línea está en estado válido (M, E o S)
            if (state == M || state == E || state == S) {
                double result = set->lines[i].data[offset];
                cache_update_lru(cache, set_index, i);
                stats_record_read_hit(&cache->stats);
             LOGD("PE%d read hit: set=%d way=%d estado=%c offset=%d valor=%.2f", 
                 pe_id, set_index, i, "MESI"[state], offset, result);
                pthread_mutex_unlock(&cache->mutex);
                return result;
            }
            
            // Tag match pero estado I (línea invalidada)
         LOGD("PE%d read tag-match pero estado=I: set=%d way=%d", pe_id, set_index, i);
            break;
        }
    }

    // MISS: traer línea del bus
    stats_record_read_miss(&cache->stats);
    stats_record_bus_traffic(&cache->stats, BLOCK_SIZE * sizeof(double), 0);
    LOGD("PE%d read miss: set=%d -> BUS_RD", pe_id, set_index);

    // Seleccionar víctima (puede hacer writeback si está en M)
    CacheLine* victim = cache_select_victim(cache, set_index, pe_id);
    int victim_way = victim - set->lines;

    // Preparar línea para recibir el bloque
    victim->valid = 1;
    victim->tag = tag;
    victim->state = I;  // Handler del bus cambiará a E o S

    // Enviar BUS_RD y esperar que el handler traiga el bloque
    pthread_mutex_unlock(&cache->mutex);
    bus_broadcast(cache->bus, BUS_RD, block_base, pe_id);
    pthread_mutex_lock(&cache->mutex);
    
    // Leer el valor del bloque traído
    double result = victim->data[offset];
    cache_update_lru(cache, set_index, victim_way);
    LOGD("PE%d read completado: way=%d offset=%d valor=%.2f estado=%c", 
        pe_id, victim_way, offset, result, "MESI"[victim->state]);
    pthread_mutex_unlock(&cache->mutex);
    return result;
}

void cache_write(Cache* cache, int addr, double value, int pe_id) {
    // Calcular dirección base del bloque y offset dentro del bloque
    int block_base = GET_BLOCK_BASE(addr);
    int offset = GET_BLOCK_OFFSET(addr);
    
    // Log solo si hay offset (dirección no alineada)
    if (offset != 0) {
        LOGD("PE%d write: addr=%d base=%d offset=%d", pe_id, addr, block_base, offset);
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    // Calcular set_index y tag para buscar en la caché
    int set_index = block_base % SETS;
    unsigned long tag = block_base / SETS;
    CacheSet* set = &cache->sets[set_index];

    // BÚSQUEDA DE HIT EN LA CACHÉ
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            // ===== CASO 1: HIT EN M =====
            // Ya tenemos permisos exclusivos de escritura
            if (state == M) {
                set->lines[i].data[offset] = value;
                cache_update_lru(cache, set_index, i);
                stats_record_write_hit(&cache->stats);
             LOGD("PE%d write hit: set=%d way=%d estado=M offset=%d valor=%.2f", 
                 pe_id, set_index, i, offset, value);
                pthread_mutex_unlock(&cache->mutex);
                return;
            } 
            // ===== CASO 2: HIT EN E =====
            // Tenemos el bloque exclusivo pero no modificado
            // Escribimos y cambiamos a M
            else if (state == E) {
                set->lines[i].data[offset] = value;
                MESI_State old_state = set->lines[i].state;
                set->lines[i].state = M;
                stats_record_mesi_transition(&cache->stats, old_state, M);
                cache_update_lru(cache, set_index, i);
                stats_record_write_hit(&cache->stats);
             LOGD("PE%d write hit: set=%d way=%d E->M offset=%d valor=%.2f", 
                 pe_id, set_index, i, offset, value);
                pthread_mutex_unlock(&cache->mutex);
                return;
            } 
            // ===== CASO 3: HIT EN S =====
            // Tenemos una copia compartida, necesitamos permisos exclusivos
            // Enviamos BUS_UPGR para invalidar otras copias
            else if (state == S) {
                stats_record_write_hit(&cache->stats);
                cache->stats.bus_upgrades++;
                stats_record_invalidation_sent(&cache->stats);  // Enviamos invalidaciones
             LOGD("PE%d write hit: set=%d way=%d S->M offset=%d BUS_UPGR valor=%.2f", 
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

    // MISS: TRAER LÍNEA CON BUS_RDX Y ESCRIBIR CON CALLBACK
    stats_record_write_miss(&cache->stats);
    stats_record_bus_traffic(&cache->stats, BLOCK_SIZE * sizeof(double), 0);
    stats_record_invalidation_sent(&cache->stats);  // BUS_RDX puede causar invalidaciones
    LOGD("PE%d write miss: set=%d -> BUS_RDX valor=%.2f", pe_id, set_index, value);

    // Seleccionar víctima (puede hacer writeback si está en M)
    CacheLine* victim = cache_select_victim(cache, set_index, pe_id);
    int victim_way = victim - set->lines;

    // Preparar línea para recibir el bloque
    victim->valid = 1;
    victim->tag = tag;
    victim->state = I;  // El callback cambiará a M después de escribir

    // Preparar contexto para el callback
    // El callback escribirá el valor DESPUÉS de que el handler traiga el bloque
    WriteCallbackContext ctx = {
        .victim = victim,
        .offset = offset,
        .value = value,
        .set_index = set_index,
        .victim_way = victim_way,
        .pe_id = pe_id
    };

    pthread_mutex_unlock(&cache->mutex);
    
    // Enviar BUS_RDX con callback
    // 1. El handler trae el bloque e invalida otras copias
    // 2. El callback escribe el valor y cambia estado a M
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

    // ===== PRIORIDAD 1: Reutilizar línea inválida con tag correcto =====
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].state == I) {
            victim = &set->lines[i];
            LOGD("PE%d víctima: way=%d estado=I reutilizar", pe_id, i);
            return victim;
        }
    }
    
    // ===== PRIORIDAD 2: Buscar línea inválida =====
    for (int i = 0; i < WAYS; i++) {
        if (!set->lines[i].valid) {
            victim = &set->lines[i];
            LOGD("PE%d víctima: way=%d inválida", pe_id, i);
            return victim;
        }
    }
    
    // ===== PRIORIDAD 3: Política LRU =====
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].lru_bit == 0) {
            victim = &set->lines[i];
            LOGD("PE%d víctima: way=%d por LRU", pe_id, i);
            break;
        }
    }
    
    // ===== FALLBACK: Usar way 0 =====
    if (victim == NULL) {
        victim = &set->lines[0];
    LOGD("PE%d víctima: way=0 (fallback)", pe_id);
    }
    
    // ===== WRITEBACK SI LA VÍCTIMA ESTÁ EN ESTADO M =====
    if (victim->state == M) {
        int victim_addr = (int)(victim->tag * SETS + set_index);
        cache->stats.bus_writebacks++;
        stats_record_bus_traffic(&cache->stats, 0, BLOCK_SIZE * sizeof(double));
    LOGD("PE%d evicción: línea M addr=%d -> BUS_WB", pe_id, victim_addr);
        bus_broadcast(cache->bus, BUS_WB, victim_addr, pe_id);
    }
    
    return victim;
}

// FUNCIONES AUXILIARES PARA HANDLERS DEL BUS

CacheLine* cache_get_line(Cache* cache, int addr) {
    // Calcular set_index y tag de la dirección
    int set_index = addr % SETS;
    unsigned long tag = addr / SETS;
    CacheSet* set = &cache->sets[set_index];

    // Buscar la línea con ese tag en el conjunto
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            return &set->lines[i];
        }
    }
    
    // No se encontró la línea en caché
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
        
        // Registrar transición en estadísticas
        if (old_state != new_state) {
            stats_record_mesi_transition(&cache->stats, old_state, new_state);
        }
        
        // Registrar invalidación si se cambió de un estado válido a I
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
        // Copiar el bloque completo desde la línea de caché
        for (int i = 0; i < BLOCK_SIZE; i++) {
            block[i] = line->data[i];
        }
    } else {
        // La línea no está en caché, retornar ceros
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
        // Copiar el bloque completo hacia la línea de caché
        for (int i = 0; i < BLOCK_SIZE; i++) {
            line->data[i] = block[i];
        }
    }
    
    pthread_mutex_unlock(&cache->mutex);
}

// Hace writeback de todas las líneas modificadas
void cache_flush(Cache* cache, int pe_id) {
    LOGI("PE%d flush: iniciando writeback de líneas modificadas", pe_id);
    
    // Array para guardar direcciones de bloques modificados
    int modified_blocks[SETS * WAYS];
    int count = 0;
    
    pthread_mutex_lock(&cache->mutex);
    
    // Escanear toda la caché buscando líneas en estado M
    for (int set = 0; set < SETS; set++) {
        for (int way = 0; way < WAYS; way++) {
            CacheLine* line = &cache->sets[set].lines[way];
            if (line->valid && line->state == M) {
                // Calcular dirección del bloque: (tag * SETS) + set_index
                modified_blocks[count++] = line->tag * SETS + set;
            }
        }
    }
    
    pthread_mutex_unlock(&cache->mutex);
    
    // Hacer writeback de todos los bloques modificados
    for (int i = 0; i < count; i++) {
        bus_broadcast(cache->bus, BUS_WB, modified_blocks[i], pe_id);
    }
    
    LOGI("PE%d flush: %d líneas enviadas a memoria", pe_id, count);
}

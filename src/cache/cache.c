#define LOG_MODULE "CACHE"
#include "cache.h"
#include "bus.h"
#include <stdio.h>
#include "log.h"

// PRIVATE STRUCTURES

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
    
    // Write the value into the block
    ctx->victim->data[ctx->offset] = ctx->value;
    
    // Change state to Modified (I->M already recorded by handler)
    ctx->victim->state = M;
    
    LOGD("PE%d write callback: way=%d offset=%d value=%.2f state=M", 
        ctx->pe_id, ctx->victim_way, ctx->offset, ctx->value);
}

// INIT AND CLEANUP

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

// LRU POLICY

static void cache_update_lru(Cache* cache, int set_index, int accessed_way) {
    CacheSet* set = &cache->sets[set_index];
    for (int i = 0; i < WAYS; i++) {
        set->lines[i].lru_bit = (i == accessed_way) ? 1 : 0;
    }
}

// READ AND WRITE OPERATIONS

double cache_read(Cache* cache, int addr, int pe_id) {
    // Compute block base address and offset within block
    int block_base = GET_BLOCK_BASE(addr);
    int offset = GET_BLOCK_OFFSET(addr);
    
    // Log only when offset != 0 (unaligned address)
    if (offset != 0) {
        LOGD("PE%d read: addr=%d base=%d offset=%d", pe_id, addr, block_base, offset);
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    // Compute set_index and tag to search the cache
    int set_index = block_base % SETS;
    unsigned long tag = block_base / SETS;
    CacheSet* set = &cache->sets[set_index];

    // SEARCH FOR CACHE HIT
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            // HIT: line in valid state (M, E, or S)
            if (state == M || state == E || state == S) {
                double result = set->lines[i].data[offset];
                cache_update_lru(cache, set_index, i);
                stats_record_read_hit(&cache->stats);
             LOGD("PE%d read hit: set=%d way=%d state=%c offset=%d value=%.2f", 
                 pe_id, set_index, i, "MESI"[state], offset, result);
                pthread_mutex_unlock(&cache->mutex);
                return result;
            }
            
                // Tag match but state I (invalidated line)
            LOGD("PE%d read tag-match but state=I: set=%d way=%d", pe_id, set_index, i);
            break;
        }
    }

    // MISS: fetch line from bus
    stats_record_read_miss(&cache->stats);
    stats_record_bus_traffic(&cache->stats, BLOCK_SIZE * sizeof(double), 0);
    LOGD("PE%d read miss: set=%d -> BUS_RD", pe_id, set_index);

    // Select victim (might write back if in M)
    CacheLine* victim = cache_select_victim(cache, set_index, pe_id);
    int victim_way = victim - set->lines;

    // Prepare line to receive block
    victim->valid = 1;
    victim->tag = tag;
    victim->state = I;  // Handler del bus cambiará a E o S

    // Send BUS_RD and wait for the handler to bring the block
    pthread_mutex_unlock(&cache->mutex);
    bus_broadcast(cache->bus, BUS_RD, block_base, pe_id);
    pthread_mutex_lock(&cache->mutex);
    
    // Read the value from the fetched block
    double result = victim->data[offset];
    cache_update_lru(cache, set_index, victim_way);
    LOGD("PE%d read complete: way=%d offset=%d value=%.2f state=%c", 
        pe_id, victim_way, offset, result, "MESI"[victim->state]);
    pthread_mutex_unlock(&cache->mutex);
    return result;
}

void cache_write(Cache* cache, int addr, double value, int pe_id) {
    // Compute block base address and offset within block
    int block_base = GET_BLOCK_BASE(addr);
    int offset = GET_BLOCK_OFFSET(addr);
    
    // Log only when offset != 0 (unaligned address)
    if (offset != 0) {
        LOGD("PE%d write: addr=%d base=%d offset=%d", pe_id, addr, block_base, offset);
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    // Compute set_index and tag to search the cache
    int set_index = block_base % SETS;
    unsigned long tag = block_base / SETS;
    CacheSet* set = &cache->sets[set_index];

    // SEARCH FOR CACHE HIT
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            // ===== CASE 1: HIT IN M =====
            // Already have exclusive write permission
            if (state == M) {
                set->lines[i].data[offset] = value;
                cache_update_lru(cache, set_index, i);
                stats_record_write_hit(&cache->stats);
             LOGD("PE%d write hit: set=%d way=%d state=M offset=%d value=%.2f", 
                 pe_id, set_index, i, offset, value);
                pthread_mutex_unlock(&cache->mutex);
                return;
            } 
            // ===== CASE 2: HIT IN E =====
            // We have the block exclusive but not modified
            // Write and change to M
            else if (state == E) {
                set->lines[i].data[offset] = value;
                MESI_State old_state = set->lines[i].state;
                set->lines[i].state = M;
                stats_record_mesi_transition(&cache->stats, old_state, M);
                cache_update_lru(cache, set_index, i);
                stats_record_write_hit(&cache->stats);
             LOGD("PE%d write hit: set=%d way=%d E->M offset=%d value=%.2f", 
                 pe_id, set_index, i, offset, value);
                pthread_mutex_unlock(&cache->mutex);
                return;
            } 
            // ===== CASE 3: HIT IN S =====
            // We have a shared copy; we need exclusive permissions
            // Send BUS_UPGR to invalidate other copies
            else if (state == S) {
                stats_record_write_hit(&cache->stats);
                cache->stats.bus_upgrades++;
                     stats_record_invalidation_sent(&cache->stats);  // We send invalidations
                 LOGD("PE%d write hit: set=%d way=%d S->M offset=%d BUS_UPGR value=%.2f", 
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

    // MISS: FETCH LINE WITH BUS_RDX AND WRITE WITH CALLBACK
    stats_record_write_miss(&cache->stats);
    stats_record_bus_traffic(&cache->stats, BLOCK_SIZE * sizeof(double), 0);
    stats_record_invalidation_sent(&cache->stats);  // BUS_RDX may cause invalidations
    LOGD("PE%d write miss: set=%d -> BUS_RDX value=%.2f", pe_id, set_index, value);

    // Select victim (may write back if in M)
    CacheLine* victim = cache_select_victim(cache, set_index, pe_id);
    int victim_way = victim - set->lines;

    // Prepare line to receive the block
    victim->valid = 1;
    victim->tag = tag;
    victim->state = I;  // Callback will change to M after writing

    // Prepare context for callback
    // Callback writes the value AFTER handler brings the block
    WriteCallbackContext ctx = {
        .victim = victim,
        .offset = offset,
        .value = value,
        .set_index = set_index,
        .victim_way = victim_way,
        .pe_id = pe_id
    };

    pthread_mutex_unlock(&cache->mutex);
    
    // Send BUS_RDX with callback
    // 1. Handler brings the block and invalidates other copies
    // 2. Callback writes the value and changes state to M
    bus_broadcast_with_callback(cache->bus, BUS_RDX, block_base, pe_id, 
                                 write_callback, &ctx);

    pthread_mutex_lock(&cache->mutex);
    cache_update_lru(cache, set_index, victim_way);
    pthread_mutex_unlock(&cache->mutex);
}

// VICTIM SELECTION AND REPLACEMENT POLICY

CacheLine* cache_select_victim(Cache* cache, int set_index, int pe_id) {
    CacheSet* set = &cache->sets[set_index];
    CacheLine* victim = NULL;

    // ===== PRIORITY 1: Reuse invalid line with correct tag =====
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].state == I) {
            victim = &set->lines[i];
            LOGD("PE%d victim: way=%d state=I reuse", pe_id, i);
            return victim;
        }
    }
    
    // ===== PRIORITY 2: Look for invalid line =====
    for (int i = 0; i < WAYS; i++) {
        if (!set->lines[i].valid) {
            victim = &set->lines[i];
            LOGD("PE%d victim: way=%d invalid", pe_id, i);
            return victim;
        }
    }
    
    // ===== PRIORITY 3: LRU policy =====
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].lru_bit == 0) {
            victim = &set->lines[i];
            LOGD("PE%d victim: way=%d by LRU", pe_id, i);
            break;
        }
    }
    
    // ===== FALLBACK: Use way 0 =====
    if (victim == NULL) {
        victim = &set->lines[0];
    LOGD("PE%d victim: way=0 (fallback)", pe_id);
    }
    
    // ===== WRITEBACK IF THE VICTIM IS IN STATE M =====
    if (victim->state == M) {
        int victim_addr = (int)(victim->tag * SETS + set_index);
        cache->stats.bus_writebacks++;
        stats_record_bus_traffic(&cache->stats, 0, BLOCK_SIZE * sizeof(double));
    LOGD("PE%d eviction: line M addr=%d -> BUS_WB", pe_id, victim_addr);
        bus_broadcast(cache->bus, BUS_WB, victim_addr, pe_id);
    }
    
    return victim;
}

// HELPER FUNCTIONS FOR BUS HANDLERS

CacheLine* cache_get_line(Cache* cache, int addr) {
    // Compute set_index and tag from address
    int set_index = addr % SETS;
    unsigned long tag = addr / SETS;
    CacheSet* set = &cache->sets[set_index];

    // Search the line with that tag in the set
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            return &set->lines[i];
        }
    }
    
    // Line not found in cache
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
    // Copy the full block from the cache line
        for (int i = 0; i < BLOCK_SIZE; i++) {
            block[i] = line->data[i];
        }
    } else {
    // Line not in cache, return zeros
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
    // Copy the full block into the cache line
        for (int i = 0; i < BLOCK_SIZE; i++) {
            line->data[i] = block[i];
        }
    }
    
    pthread_mutex_unlock(&cache->mutex);
}

// Write back all modified lines
void cache_flush(Cache* cache, int pe_id) {
    LOGI("PE%d flush: starting writeback of modified lines", pe_id);
    
    // Array to store addresses of modified blocks
    int modified_blocks[SETS * WAYS];
    int count = 0;
    
    pthread_mutex_lock(&cache->mutex);
    
    // Scan entire cache for lines in state M
    for (int set = 0; set < SETS; set++) {
        for (int way = 0; way < WAYS; way++) {
            CacheLine* line = &cache->sets[set].lines[way];
            if (line->valid && line->state == M) {
                // Compute block address: (tag * SETS) + set_index
                modified_blocks[count++] = line->tag * SETS + set;
            }
        }
    }
    
    pthread_mutex_unlock(&cache->mutex);
    
    // Write back all modified blocks
    for (int i = 0; i < count; i++) {
        bus_broadcast(cache->bus, BUS_WB, modified_blocks[i], pe_id);
    }
    
    LOGI("PE%d flush: %d lines written to memory", pe_id, count);
}

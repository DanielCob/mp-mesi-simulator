#ifndef CACHE_H
#define CACHE_H

#include "config.h"
#include "cache_stats.h"
#include <pthread.h>

// FORWARD DECLARATIONS
struct Bus;
typedef struct Bus Bus;

// STRUCTURES

/**
 * Cache line
 * Contains one BLOCK_SIZE block of doubles plus metadata
 */
typedef struct {
    unsigned long tag;          // Address tag
    MESI_State state;           // MESI state (M, E, S, I)
    double data[BLOCK_SIZE];    // Block data
    int valid;                  // 1 = valid, 0 = invalid
    int lru_bit;                // LRU bit for replacement
} CacheLine;

/**
 * Cache set
 * Contains WAYS lines
 */
typedef struct {
    CacheLine lines[WAYS];      // Set lines
} CacheSet;

/**
 * Private L1 cache with MESI protocol
 * Thread-safe via mutex
 */
typedef struct {
    Bus* bus;                   // Reference to shared bus
    CacheSet sets[SETS];        // Array of sets
    pthread_mutex_t mutex;      // Synchronization mutex
    CacheStats stats;           // Access statistics
    int pe_id;                  // Owning PE id
} Cache;

// PUBLIC API

// Init and cleanup
void cache_init(Cache* cache);
void cache_destroy(Cache* cache);

// Read/write operations
double cache_read(Cache* cache, int addr, int pe_id);
void cache_write(Cache* cache, int addr, double value, int pe_id);

// Replacement policy
CacheLine* cache_select_victim(Cache* cache, int set_index, int pe_id);

// MESI coherence operations
CacheLine* cache_get_line(Cache* cache, int addr);
MESI_State cache_get_state(Cache* cache, int addr);
void cache_set_state(Cache* cache, int addr, MESI_State new_state);

// Block operations
void cache_get_block(Cache* cache, int addr, double block[BLOCK_SIZE]);
void cache_set_block(Cache* cache, int addr, const double block[BLOCK_SIZE]);

// Flush
void cache_flush(Cache* cache, int pe_id);

#endif

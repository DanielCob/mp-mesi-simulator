#ifndef MEMORY_STATS_H
#define MEMORY_STATS_H

#include <stdint.h>

/**
 * @brief Statistics for main memory accesses
 */
typedef struct {
    uint64_t reads;                    // Reads from memory
    uint64_t writes;                   // Writes to memory
    uint64_t total_accesses;           // Total accesses
    uint64_t bytes_read;               // Bytes read
    uint64_t bytes_written;            // Bytes written
    
    // Accesses per PE (to see which PE uses memory more)
    uint64_t reads_per_pe[4];
    uint64_t writes_per_pe[4];
} MemoryStats;

/**
 * @brief Initialize memory statistics
 */
void memory_stats_init(MemoryStats* stats);

/**
 * @brief Record a memory read
 */
void memory_stats_record_read(MemoryStats* stats, int pe_id, int bytes);

/**
 * @brief Record a memory write
 */
void memory_stats_record_write(MemoryStats* stats, int pe_id, int bytes);

/**
 * @brief Print memory statistics
 */
void memory_stats_print(const MemoryStats* stats);

#endif // MEMORY_STATS_H

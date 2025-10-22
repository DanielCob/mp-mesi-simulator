#ifndef STATS_H
#define STATS_H

#include <stdint.h>
#include "config.h"

/**
 * @brief Structure to store MESI state transitions
 */
typedef struct {
    // Transitions from Invalid
    uint64_t I_to_E;  // Invalid → Exclusive (read miss, not shared)
    uint64_t I_to_S;  // Invalid → Shared (read miss, shared)
    uint64_t I_to_M;  // Invalid → Modified (write miss)
    
    // Transitions from Exclusive
    uint64_t E_to_M;  // Exclusive → Modified (write hit)
    uint64_t E_to_S;  // Exclusive → Shared (another cache reads)
    uint64_t E_to_I;  // Exclusive → Invalid (invalidation)
    
    // Transitions from Shared
    uint64_t S_to_M;  // Shared → Modified (write hit, upgrade)
    uint64_t S_to_I;  // Shared → Invalid (invalidation)
    
    // Transitions from Modified
    uint64_t M_to_S;  // Modified → Shared (another cache reads, writeback)
    uint64_t M_to_I;  // Modified → Invalid (invalidation + writeback)
} MESITransitions;

/**
 * @brief Per-PE cache statistics
 */
typedef struct {
    // Cache hits and misses
    uint64_t read_hits;
    uint64_t read_misses;
    uint64_t write_hits;
    uint64_t write_misses;
    
    // Coherence invalidations
    uint64_t invalidations_requested; // Requests that may cause invalidations (e.g., BusRdX/Upgr issued)
    uint64_t invalidations_sent;      // Actual broadcast invalidations sent by this PE (bus confirmed)
    uint64_t invalidations_received;  // Broadcast invalidations received by this PE
    
    // Read/write operations
    uint64_t total_reads;
    uint64_t total_writes;
    
    // Bus traffic (in blocks)
    uint64_t bus_reads;       // BusRd issued
    uint64_t bus_read_x;      // BusRdX issued
    uint64_t bus_upgrades;    // BusUpgr issued
    uint64_t bus_writebacks;  // Writebacks to memory
    
    // MESI transitions
    MESITransitions transitions;
    
    // Bytes transferred
    uint64_t bytes_read_from_bus;
    uint64_t bytes_written_to_bus;
    
} CacheStats;

/**
 * @brief Initialize cache statistics
 *
 * @param stats Pointer to stats structure
 */
void stats_init(CacheStats* stats);

/**
 * @brief Record a read hit
 *
 * @param stats Pointer to stats
 */
void stats_record_read_hit(CacheStats* stats);

/**
 * @brief Record a read miss
 *
 * @param stats Pointer to stats
 */
void stats_record_read_miss(CacheStats* stats);

/**
 * @brief Record a write hit
 *
 * @param stats Pointer to stats
 */
void stats_record_write_hit(CacheStats* stats);

/**
 * @brief Record a write miss
 *
 * @param stats Pointer to stats
 */
void stats_record_write_miss(CacheStats* stats);

/**
 * @brief Record a received invalidation
 *
 * @param stats Pointer to stats
 */
void stats_record_invalidation_received(CacheStats* stats);

/**
 * @brief Record a sent invalidation
 *
 * @param stats Pointer to stats
 */
void stats_record_invalidation_sent(CacheStats* stats);

// Record an invalidation request (attempt) from this PE (BusRdX/Upgr issued)
void stats_record_invalidation_requested(CacheStats* stats);

/**
 * @brief Record bus traffic (bytes)
 *
 * @param stats Pointer to stats
 * @param bytes_read Bytes read from bus
 * @param bytes_written Bytes written to bus
 */
void stats_record_bus_traffic(CacheStats* stats, uint64_t bytes_read, uint64_t bytes_written);

/**
 * @brief Record a MESI state transition
 *
 * @param stats Pointer to stats
 * @param from Source state (I/E/S/M)
 * @param to Destination state (I/E/S/M)
 */
void stats_record_mesi_transition(CacheStats* stats, MESI_State from, MESI_State to);

/**
 * @brief Print statistics for one PE
 *
 * @param stats Pointer to stats
 * @param pe_id PE id
 */
void stats_print(const CacheStats* stats, int pe_id);

/**
 * @brief Print a comparative summary for all PEs
 *
 * @param stats_array Array of stats (one per PE)
 * @param num_pes Number of PEs
 */
void stats_print_summary(const CacheStats* stats_array, int num_pes);

#endif // STATS_H

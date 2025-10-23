#ifndef BUS_STATS_H
#define BUS_STATS_H

#include <stdint.h>

/**
 * @brief Statistics for the interconnect bus
 */
typedef struct {
    // Signals by type
    uint64_t bus_rd_count;         // Reads (BUS_RD)
    uint64_t bus_rdx_count;        // Exclusive reads for write (BUS_RDX)
    uint64_t bus_upgr_count;       // Upgrades (BUS_UPGR)
    uint64_t bus_wb_count;         // Writebacks (BUS_WB)
    
    // Generated invalidations
    uint64_t invalidations_sent;   // Total broadcast invalidations
    
    // Totals
    uint64_t total_transactions;   // Total transactions
    uint64_t bytes_transferred;    // Total bytes transferred on the bus
    
    // Traffic breakdown
    uint64_t bytes_data;           // Data bytes (cache blocks)
    uint64_t bytes_control;        // Control bytes (total)
    uint64_t bytes_control_base;   // Base control bytes per transaction
    uint64_t bytes_control_invs;   // Additional control bytes due to invalidations
    
    // Per-PE counts (who uses the bus more)
    uint64_t transactions_per_pe[4];
} BusStats;

/**
 * @brief Initialize bus statistics
 */
void bus_stats_init(BusStats* stats);

/**
 * @brief Record a BUS_RD transaction
 */
void bus_stats_record_bus_rd(BusStats* stats, int pe_id);

/**
 * @brief Record a BUS_RDX transaction
 */
void bus_stats_record_bus_rdx(BusStats* stats, int pe_id);

/**
 * @brief Record a BUS_UPGR transaction
 */
void bus_stats_record_bus_upgr(BusStats* stats, int pe_id);

/**
 * @brief Record a BUS_WB transaction
 */
void bus_stats_record_bus_wb(BusStats* stats, int pe_id);

/**
 * @brief Record broadcast invalidations sent
 */
void bus_stats_record_invalidations(BusStats* stats, int count);

/**
 * @brief Record data bytes transferred (cache blocks)
 */
void bus_stats_record_data_transfer(BusStats* stats, int bytes);

/**
 * @brief Record control bytes transferred (signals, invalidations)
 */
void bus_stats_record_control_transfer(BusStats* stats, int bytes);
// Desgloses específicos de control
// Control breakdowns
void bus_stats_record_control_base(BusStats* stats, int bytes);
void bus_stats_record_control_invalidations(BusStats* stats, int bytes);

/**
 * @brief Print bus statistics
 */
void bus_stats_print(const BusStats* stats);

#endif // BUS_STATS_H

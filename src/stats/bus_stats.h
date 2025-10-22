#ifndef BUS_STATS_H
#define BUS_STATS_H

#include <stdint.h>

/**
 * @brief Estructura para estadísticas del bus de interconexión
 */
typedef struct {
    // Señales por tipo
    uint64_t bus_rd_count;         // Lecturas (BUS_RD)
    uint64_t bus_rdx_count;        // Escrituras exclusivas (BUS_RDX)
    uint64_t bus_upgr_count;       // Upgrades (BUS_UPGR)
    uint64_t bus_wb_count;         // Writebacks (BUS_WB)
    
    // Invalidaciones generadas
    uint64_t invalidations_sent;   // Total de invalidaciones broadcast
    
    // Tráfico total
    uint64_t total_transactions;   // Total de transacciones
    uint64_t bytes_transferred;    // Bytes transferidos por el bus (total)
    
    // Desglose de tráfico
    uint64_t bytes_data;           // Bytes de datos (bloques de caché)
    uint64_t bytes_control;        // Bytes de señales de control (invalidaciones, etc.)
    
    // Conteo por PE (quién usa más el bus)
    uint64_t transactions_per_pe[4];
} BusStats;

/**
 * @brief Inicializa las estadísticas del bus
 */
void bus_stats_init(BusStats* stats);

/**
 * @brief Registra una transacción BUS_RD
 */
void bus_stats_record_bus_rd(BusStats* stats, int pe_id);

/**
 * @brief Registra una transacción BUS_RDX
 */
void bus_stats_record_bus_rdx(BusStats* stats, int pe_id);

/**
 * @brief Registra una transacción BUS_UPGR
 */
void bus_stats_record_bus_upgr(BusStats* stats, int pe_id);

/**
 * @brief Registra una transacción BUS_WB
 */
void bus_stats_record_bus_wb(BusStats* stats, int pe_id);

/**
 * @brief Registra invalidaciones enviadas
 */
void bus_stats_record_invalidations(BusStats* stats, int count);

/**
 * @brief Registra bytes de datos transferidos (bloques de caché)
 */
void bus_stats_record_data_transfer(BusStats* stats, int bytes);

/**
 * @brief Registra bytes de control transferidos (señales, invalidaciones)
 */
void bus_stats_record_control_transfer(BusStats* stats, int bytes);

/**
 * @brief Imprime las estadísticas del bus
 */
void bus_stats_print(const BusStats* stats);

#endif // BUS_STATS_H

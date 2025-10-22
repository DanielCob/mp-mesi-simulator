#ifndef STATS_H
#define STATS_H

#include <stdint.h>

/**
 * @brief Estructura para almacenar transiciones de estados MESI
 */
typedef struct {
    // Transiciones desde Invalid
    uint64_t I_to_E;  // Invalid → Exclusive (read miss, no compartido)
    uint64_t I_to_S;  // Invalid → Shared (read miss, compartido)
    uint64_t I_to_M;  // Invalid → Modified (write miss)
    
    // Transiciones desde Exclusive
    uint64_t E_to_M;  // Exclusive → Modified (write hit)
    uint64_t E_to_S;  // Exclusive → Shared (otra caché lee)
    uint64_t E_to_I;  // Exclusive → Invalid (invalidación)
    
    // Transiciones desde Shared
    uint64_t S_to_M;  // Shared → Modified (write hit, upgrade)
    uint64_t S_to_I;  // Shared → Invalid (invalidación)
    
    // Transiciones desde Modified
    uint64_t M_to_S;  // Modified → Shared (otra caché lee, writeback)
    uint64_t M_to_I;  // Modified → Invalid (invalidación + writeback)
} MESITransitions;

/**
 * @brief Estructura para estadísticas de caché por PE
 */
typedef struct {
    // Cache hits y misses
    uint64_t read_hits;
    uint64_t read_misses;
    uint64_t write_hits;
    uint64_t write_misses;
    
    // Coherence invalidations
    uint64_t invalidations_requested; // Requests that may cause invalidations (e.g., BusRdX/Upgr issued)
    uint64_t invalidations_sent;      // Actual broadcast invalidations sent by this PE (bus confirmed)
    uint64_t invalidations_received;  // Broadcast invalidations received by this PE
    
    // Operaciones de lectura/escritura
    uint64_t total_reads;
    uint64_t total_writes;
    
    // Tráfico del bus (en bloques)
    uint64_t bus_reads;       // BusRd enviados
    uint64_t bus_read_x;      // BusRdX enviados
    uint64_t bus_upgrades;    // BusUpgr enviados
    uint64_t bus_writebacks;  // Writebacks a memoria
    
    // Transiciones MESI
    MESITransitions transitions;
    
    // Bytes transferidos
    uint64_t bytes_read_from_bus;
    uint64_t bytes_written_to_bus;
    
} CacheStats;

/**
 * @brief Inicializa las estadísticas de caché
 * 
 * @param stats Puntero a la estructura de estadísticas
 */
void stats_init(CacheStats* stats);

/**
 * @brief Registra un read hit
 * 
 * @param stats Puntero a las estadísticas
 */
void stats_record_read_hit(CacheStats* stats);

/**
 * @brief Registra un read miss
 * 
 * @param stats Puntero a las estadísticas
 */
void stats_record_read_miss(CacheStats* stats);

/**
 * @brief Registra un write hit
 * 
 * @param stats Puntero a las estadísticas
 */
void stats_record_write_hit(CacheStats* stats);

/**
 * @brief Registra un write miss
 * 
 * @param stats Puntero a las estadísticas
 */
void stats_record_write_miss(CacheStats* stats);

/**
 * @brief Registra una invalidación recibida
 * 
 * @param stats Puntero a las estadísticas
 */
void stats_record_invalidation_received(CacheStats* stats);

/**
 * @brief Registra una invalidación enviada
 * 
 * @param stats Puntero a las estadísticas
 */
void stats_record_invalidation_sent(CacheStats* stats);

// Record an invalidation request (attempt) from this PE (BusRdX/Upgr issued)
void stats_record_invalidation_requested(CacheStats* stats);

/**
 * @brief Registra tráfico del bus (en bytes)
 * 
 * @param stats Puntero a las estadísticas
 * @param bytes_read Bytes leídos del bus
 * @param bytes_written Bytes escritos al bus
 */
void stats_record_bus_traffic(CacheStats* stats, uint64_t bytes_read, uint64_t bytes_written);

/**
 * @brief Registra una transición de estado MESI
 * 
 * @param stats Puntero a las estadísticas
 * @param from Estado origen (0=I, 1=E, 2=S, 3=M)
 * @param to Estado destino (0=I, 1=E, 2=S, 3=M)
 */
void stats_record_mesi_transition(CacheStats* stats, int from, int to);

/**
 * @brief Imprime las estadísticas de un PE
 * 
 * @param stats Puntero a las estadísticas
 * @param pe_id ID del PE
 */
void stats_print(const CacheStats* stats, int pe_id);

/**
 * @brief Imprime un resumen comparativo de todos los PEs
 * 
 * @param stats_array Array de estadísticas de todos los PEs
 * @param num_pes Número de PEs
 */
void stats_print_summary(const CacheStats* stats_array, int num_pes);

#endif // STATS_H

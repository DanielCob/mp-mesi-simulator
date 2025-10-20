#ifndef MEMORY_STATS_H
#define MEMORY_STATS_H

#include <stdint.h>

/**
 * @brief Estructura para estadísticas de accesos a memoria principal
 */
typedef struct {
    uint64_t reads;                    // Lecturas desde memoria
    uint64_t writes;                   // Escrituras a memoria
    uint64_t total_accesses;           // Total de accesos
    uint64_t bytes_read;               // Bytes leídos
    uint64_t bytes_written;            // Bytes escritos
    
    // Accesos por PE (para ver qué PE accede más a memoria)
    uint64_t reads_per_pe[4];
    uint64_t writes_per_pe[4];
} MemoryStats;

/**
 * @brief Inicializa las estadísticas de memoria
 */
void memory_stats_init(MemoryStats* stats);

/**
 * @brief Registra una lectura de memoria
 */
void memory_stats_record_read(MemoryStats* stats, int pe_id, int bytes);

/**
 * @brief Registra una escritura a memoria
 */
void memory_stats_record_write(MemoryStats* stats, int pe_id, int bytes);

/**
 * @brief Imprime las estadísticas de memoria
 */
void memory_stats_print(const MemoryStats* stats);

#endif // MEMORY_STATS_H

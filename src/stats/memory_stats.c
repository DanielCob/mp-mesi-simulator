#include "memory_stats.h"
#include <stdio.h>
#include <string.h>

void memory_stats_init(MemoryStats* stats) {
    memset(stats, 0, sizeof(MemoryStats));
}

void memory_stats_record_read(MemoryStats* stats, int pe_id, int bytes) {
    stats->reads++;
    stats->total_accesses++;
    stats->bytes_read += bytes;
    
    if (pe_id >= 0 && pe_id < 4) {
        stats->reads_per_pe[pe_id]++;
    }
}

void memory_stats_record_write(MemoryStats* stats, int pe_id, int bytes) {
    stats->writes++;
    stats->total_accesses++;
    stats->bytes_written += bytes;
    
    if (pe_id >= 0 && pe_id < 4) {
        stats->writes_per_pe[pe_id]++;
    }
}

void memory_stats_print(const MemoryStats* stats) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                 ESTADÍSTICAS DE MEMORIA PRINCIPAL              ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    
    printf("║  📝 ACCESOS A MEMORIA                                         ║\n");
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║    - Lecturas:     %10lu                                ║\n", stats->reads);
    printf("║    - Escrituras:   %10lu                                ║\n", stats->writes);
    printf("║    - Total:        %10lu                                ║\n", stats->total_accesses);
    printf("║                                                                ║\n");
    
    double read_kb = stats->bytes_read / 1024.0;
    double write_kb = stats->bytes_written / 1024.0;
    double total_mb = (stats->bytes_read + stats->bytes_written) / (1024.0 * 1024.0);
    
    printf("║  📊 TRÁFICO                                                   ║\n");
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║    - Bytes leídos:    %10lu (%.2f KB)                  ║\n", 
           stats->bytes_read, read_kb);
    printf("║    - Bytes escritos:  %10lu (%.2f KB)                  ║\n", 
           stats->bytes_written, write_kb);
    printf("║    - Total:           %.6f MB                            ║\n", total_mb);
    printf("║                                                                ║\n");
    
    printf("║  🔢 ACCESOS POR PE                                            ║\n");
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║    PE  │  Lecturas  │  Escrituras │   Total                   ║\n");
    printf("╟────────┼────────────┼─────────────┼───────────────────────────╢\n");
    
    for (int i = 0; i < 4; i++) {
        uint64_t total_pe = stats->reads_per_pe[i] + stats->writes_per_pe[i];
        printf("║    %d   │  %8lu  │   %8lu  │  %8lu                 ║\n",
               i, stats->reads_per_pe[i], stats->writes_per_pe[i], total_pe);
    }
    
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

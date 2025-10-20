#include "cache_stats.h"
#include <stdio.h>
#include <string.h>

void stats_init(CacheStats* stats) {
    memset(stats, 0, sizeof(CacheStats));
}

void stats_record_read_hit(CacheStats* stats) {
    stats->read_hits++;
    stats->total_reads++;
}

void stats_record_read_miss(CacheStats* stats) {
    stats->read_misses++;
    stats->total_reads++;
    stats->bus_reads++;
}

void stats_record_write_hit(CacheStats* stats) {
    stats->write_hits++;
    stats->total_writes++;
}

void stats_record_write_miss(CacheStats* stats) {
    stats->write_misses++;
    stats->total_writes++;
    stats->bus_read_x++;
}

void stats_record_invalidation_received(CacheStats* stats) {
    stats->invalidations_received++;
}

void stats_record_invalidation_sent(CacheStats* stats) {
    stats->invalidations_sent++;
}

void stats_record_bus_traffic(CacheStats* stats, uint64_t bytes_read, uint64_t bytes_written) {
    stats->bytes_read_from_bus += bytes_read;
    stats->bytes_written_to_bus += bytes_written;
}

void stats_record_mesi_transition(CacheStats* stats, int from, int to) {
    // from: 0=I, 1=E, 2=S, 3=M
    // to:   0=I, 1=E, 2=S, 3=M
    
    if (from == 0 && to == 1) stats->transitions.I_to_E++;
    else if (from == 0 && to == 2) stats->transitions.I_to_S++;
    else if (from == 0 && to == 3) stats->transitions.I_to_M++;
    
    else if (from == 1 && to == 3) stats->transitions.E_to_M++;
    else if (from == 1 && to == 2) stats->transitions.E_to_S++;
    else if (from == 1 && to == 0) stats->transitions.E_to_I++;
    
    else if (from == 2 && to == 3) stats->transitions.S_to_M++;
    else if (from == 2 && to == 0) stats->transitions.S_to_I++;
    
    else if (from == 3 && to == 2) stats->transitions.M_to_S++;
    else if (from == 3 && to == 0) stats->transitions.M_to_I++;
}

void stats_print(const CacheStats* stats, int pe_id) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║            ESTADÍSTICAS DE PE%d                                ║\n", pe_id);
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    
    // Cache Performance
    printf("║  📊 RENDIMIENTO DE CACHÉ                                      ║\n");
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    
    uint64_t total_accesses = stats->total_reads + stats->total_writes;
    uint64_t total_hits = stats->read_hits + stats->write_hits;
    uint64_t total_misses = stats->read_misses + stats->write_misses;
    
    double hit_rate = total_accesses > 0 ? (100.0 * total_hits / total_accesses) : 0.0;
    double miss_rate = total_accesses > 0 ? (100.0 * total_misses / total_accesses) : 0.0;
    
    printf("║  Lecturas:                                                     ║\n");
    printf("║    - Hits:   %10lu    Misses: %10lu              ║\n", 
           stats->read_hits, stats->read_misses);
    printf("║    - Total:  %10lu                                     ║\n", 
           stats->total_reads);
    printf("║                                                                ║\n");
    printf("║  Escrituras:                                                   ║\n");
    printf("║    - Hits:   %10lu    Misses: %10lu              ║\n", 
           stats->write_hits, stats->write_misses);
    printf("║    - Total:  %10lu                                     ║\n", 
           stats->total_writes);
    printf("║                                                                ║\n");
    printf("║  Total:                                                        ║\n");
    printf("║    - Hits:   %10lu    (%.2f%%)                         ║\n", 
           total_hits, hit_rate);
    printf("║    - Misses: %10lu    (%.2f%%)                         ║\n", 
           total_misses, miss_rate);
    printf("║    - Accesos: %10lu                                    ║\n", 
           total_accesses);
    
    // Coherence
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║  🔄 COHERENCIA (Invalidaciones)                               ║\n");
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║    - Recibidas: %10lu                                  ║\n", 
           stats->invalidations_received);
    printf("║    - Enviadas:  %10lu                                  ║\n", 
           stats->invalidations_sent);
    
    // Bus Traffic
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║  🚌 TRÁFICO DEL BUS                                           ║\n");
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║    - BusRd (lecturas):      %10lu                      ║\n", 
           stats->bus_reads);
    printf("║    - BusRdX (escrituras):   %10lu                      ║\n", 
           stats->bus_read_x);
    printf("║    - BusUpgr (upgrades):    %10lu                      ║\n", 
           stats->bus_upgrades);
    printf("║    - Writebacks:            %10lu                      ║\n", 
           stats->bus_writebacks);
    printf("║                                                                ║\n");
    
    double total_mb = (stats->bytes_read_from_bus + stats->bytes_written_to_bus) / (1024.0 * 1024.0);
    printf("║    - Bytes leídos:   %10lu (%.2f KB)                ║\n", 
           stats->bytes_read_from_bus, stats->bytes_read_from_bus / 1024.0);
    printf("║    - Bytes escritos: %10lu (%.2f KB)                ║\n", 
           stats->bytes_written_to_bus, stats->bytes_written_to_bus / 1024.0);
    printf("║    - Tráfico total:  %.6f MB                           ║\n", total_mb);
    
    // MESI Transitions
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║  🔀 TRANSICIONES DE ESTADOS MESI                              ║\n");
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    
    printf("║  Desde INVALID:                                                ║\n");
    printf("║    I → E (Exclusive):  %10lu                            ║\n", 
           stats->transitions.I_to_E);
    printf("║    I → S (Shared):     %10lu                            ║\n", 
           stats->transitions.I_to_S);
    printf("║    I → M (Modified):   %10lu                            ║\n", 
           stats->transitions.I_to_M);
    
    printf("║                                                                ║\n");
    printf("║  Desde EXCLUSIVE:                                              ║\n");
    printf("║    E → M (Modified):   %10lu                            ║\n", 
           stats->transitions.E_to_M);
    printf("║    E → S (Shared):     %10lu                            ║\n", 
           stats->transitions.E_to_S);
    printf("║    E → I (Invalid):    %10lu                            ║\n", 
           stats->transitions.E_to_I);
    
    printf("║                                                                ║\n");
    printf("║  Desde SHARED:                                                 ║\n");
    printf("║    S → M (Modified):   %10lu                            ║\n", 
           stats->transitions.S_to_M);
    printf("║    S → I (Invalid):    %10lu                            ║\n", 
           stats->transitions.S_to_I);
    
    printf("║                                                                ║\n");
    printf("║  Desde MODIFIED:                                               ║\n");
    printf("║    M → S (Shared):     %10lu                            ║\n", 
           stats->transitions.M_to_S);
    printf("║    M → I (Invalid):    %10lu                            ║\n", 
           stats->transitions.M_to_I);
    
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void stats_print_summary(const CacheStats* stats_array, int num_pes) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    RESUMEN COMPARATIVO DE ESTADÍSTICAS                        ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════════════╣\n");
    
    // Tabla de hit rates
    printf("║  HIT RATES POR PE                                                              ║\n");
    printf("╟────────────────────────────────────────────────────────────────────────────────╢\n");
    printf("║  PE  │  Accesos  │   Hits    │  Misses   │  Hit Rate  │  Miss Rate            ║\n");
    printf("╟──────┼───────────┼───────────┼───────────┼────────────┼────────────────────────╢\n");
    
    uint64_t total_accesses_all = 0;
    uint64_t total_hits_all = 0;
    uint64_t total_misses_all = 0;
    
    for (int i = 0; i < num_pes; i++) {
        uint64_t accesses = stats_array[i].total_reads + stats_array[i].total_writes;
        uint64_t hits = stats_array[i].read_hits + stats_array[i].write_hits;
        uint64_t misses = stats_array[i].read_misses + stats_array[i].write_misses;
        
        double hit_rate = accesses > 0 ? (100.0 * hits / accesses) : 0.0;
        double miss_rate = accesses > 0 ? (100.0 * misses / accesses) : 0.0;
        
        printf("║  %d   │ %9lu │ %9lu │ %9lu │  %6.2f%%  │  %6.2f%%              ║\n",
               i, accesses, hits, misses, hit_rate, miss_rate);
        
        total_accesses_all += accesses;
        total_hits_all += hits;
        total_misses_all += misses;
    }
    
    double avg_hit_rate = total_accesses_all > 0 ? (100.0 * total_hits_all / total_accesses_all) : 0.0;
    double avg_miss_rate = total_accesses_all > 0 ? (100.0 * total_misses_all / total_accesses_all) : 0.0;
    
    printf("╟──────┼───────────┼───────────┼───────────┼────────────┼────────────────────────╢\n");
    printf("║ TOTAL│ %9lu │ %9lu │ %9lu │  %6.2f%%  │  %6.2f%%              ║\n",
           total_accesses_all, total_hits_all, total_misses_all, avg_hit_rate, avg_miss_rate);
    
    // Invalidaciones
    printf("╟────────────────────────────────────────────────────────────────────────────────╢\n");
    printf("║  INVALIDACIONES POR PE                                                         ║\n");
    printf("╟────────────────────────────────────────────────────────────────────────────────╢\n");
    printf("║  PE  │  Recibidas  │  Enviadas   │  Total                                     ║\n");
    printf("╟──────┼─────────────┼─────────────┼────────────────────────────────────────────╢\n");
    
    uint64_t total_inv_received = 0;
    uint64_t total_inv_sent = 0;
    
    for (int i = 0; i < num_pes; i++) {
        uint64_t received = stats_array[i].invalidations_received;
        uint64_t sent = stats_array[i].invalidations_sent;
        
        printf("║  %d   │  %10lu │  %10lu │  %10lu                        ║\n",
               i, received, sent, received + sent);
        
        total_inv_received += received;
        total_inv_sent += sent;
    }
    
    printf("╟──────┼─────────────┼─────────────┼────────────────────────────────────────────╢\n");
    printf("║ TOTAL│  %10lu │  %10lu │  %10lu                        ║\n",
           total_inv_received, total_inv_sent, total_inv_received + total_inv_sent);
    
    // Tráfico del bus
    printf("╟────────────────────────────────────────────────────────────────────────────────╢\n");
    printf("║  TRÁFICO DEL BUS POR PE                                                        ║\n");
    printf("╟────────────────────────────────────────────────────────────────────────────────╢\n");
    printf("║  PE  │ BusRd │ BusRdX │ BusUpgr │ WB │ Tráfico (KB)                          ║\n");
    printf("╟──────┼───────┼────────┼─────────┼────┼───────────────────────────────────────╢\n");
    
    double total_traffic_kb = 0.0;
    
    for (int i = 0; i < num_pes; i++) {
        uint64_t bus_rd = stats_array[i].bus_reads;
        uint64_t bus_rdx = stats_array[i].bus_read_x;
        uint64_t bus_upgr = stats_array[i].bus_upgrades;
        uint64_t wb = stats_array[i].bus_writebacks;
        
        double traffic_kb = (stats_array[i].bytes_read_from_bus + 
                            stats_array[i].bytes_written_to_bus) / 1024.0;
        
        printf("║  %d   │ %5lu │ %6lu │ %7lu │ %2lu │ %10.2f                     ║\n",
               i, bus_rd, bus_rdx, bus_upgr, wb, traffic_kb);
        
        total_traffic_kb += traffic_kb;
    }
    
    printf("╟──────┴───────┴────────┴─────────┴────┴───────────────────────────────────────╢\n");
    printf("║  Tráfico total del bus: %.2f KB (%.6f MB)                           ║\n",
           total_traffic_kb, total_traffic_kb / 1024.0);
    
    printf("╚════════════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

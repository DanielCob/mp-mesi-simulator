#include "bus_stats.h"
#include <stdio.h>
#include <string.h>

void bus_stats_init(BusStats* stats) {
    memset(stats, 0, sizeof(BusStats));
}

void bus_stats_record_bus_rd(BusStats* stats, int pe_id) {
    stats->bus_rd_count++;
    stats->total_transactions++;
    if (pe_id >= 0 && pe_id < 4) {
        stats->transactions_per_pe[pe_id]++;
    }
}

void bus_stats_record_bus_rdx(BusStats* stats, int pe_id) {
    stats->bus_rdx_count++;
    stats->total_transactions++;
    if (pe_id >= 0 && pe_id < 4) {
        stats->transactions_per_pe[pe_id]++;
    }
}

void bus_stats_record_bus_upgr(BusStats* stats, int pe_id) {
    stats->bus_upgr_count++;
    stats->total_transactions++;
    if (pe_id >= 0 && pe_id < 4) {
        stats->transactions_per_pe[pe_id]++;
    }
}

void bus_stats_record_bus_wb(BusStats* stats, int pe_id) {
    stats->bus_wb_count++;
    stats->total_transactions++;
    if (pe_id >= 0 && pe_id < 4) {
        stats->transactions_per_pe[pe_id]++;
    }
}

void bus_stats_record_invalidations(BusStats* stats, int count) {
    stats->invalidations_sent += count;
}

void bus_stats_record_data_transfer(BusStats* stats, int bytes) {
    stats->bytes_data += bytes;
    stats->bytes_transferred += bytes;
}

void bus_stats_record_control_transfer(BusStats* stats, int bytes) {
    stats->bytes_control += bytes;
    stats->bytes_transferred += bytes;
}

void bus_stats_print(const BusStats* stats) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    ESTADÍSTICAS DEL BUS                        ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    
    printf("║  🚌 TRANSACCIONES DEL BUS                                     ║\n");
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║    - BUS_RD (lecturas):           %10lu              ║\n", stats->bus_rd_count);
    printf("║    - BUS_RDX (escrituras excl.):  %10lu              ║\n", stats->bus_rdx_count);
    printf("║    - BUS_UPGR (upgrades):         %10lu              ║\n", stats->bus_upgr_count);
    printf("║    - BUS_WB (writebacks):         %10lu              ║\n", stats->bus_wb_count);
    printf("║    - Total transacciones:         %10lu              ║\n", stats->total_transactions);
    printf("║                                                                ║\n");
    
    printf("║  ⚡ COHERENCIA                                                ║\n");
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║    - Invalidaciones broadcast:    %10lu              ║\n", stats->invalidations_sent);
    printf("║                                                                ║\n");
    
    double traffic_kb = stats->bytes_transferred / 1024.0;
    double traffic_mb = traffic_kb / 1024.0;
    double data_kb = stats->bytes_data / 1024.0;
    double control_kb = stats->bytes_control / 1024.0;
    
    printf("║  📊 TRÁFICO                                                   ║\n");
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║    - Bytes de datos:      %10lu (%.2f KB)           ║\n", 
           stats->bytes_data, data_kb);
    printf("║    - Bytes de control:    %10lu (%.2f KB)           ║\n", 
           stats->bytes_control, control_kb);
    printf("║    - Total transferido:   %10lu (%.2f KB)           ║\n", 
           stats->bytes_transferred, traffic_kb);
    printf("║    - Tráfico total (MB):  %10.6f                        ║\n", traffic_mb);
    printf("║                                                                ║\n");
    
    printf("║  🔢 USO DEL BUS POR PE                                        ║\n");
    printf("╟────────────────────────────────────────────────────────────────╢\n");
    printf("║    PE  │  Transacciones │  Porcentaje                         ║\n");
    printf("╟────────┼────────────────┼─────────────────────────────────────╢\n");
    
    for (int i = 0; i < 4; i++) {
        double percentage = stats->total_transactions > 0 ?
            (100.0 * stats->transactions_per_pe[i] / stats->total_transactions) : 0.0;
        printf("║    %d   │   %10lu   │  %6.2f%%                          ║\n",
               i, stats->transactions_per_pe[i], percentage);
    }
    
    printf("╟────────┴────────────────┴─────────────────────────────────────╢\n");
    printf("║  TOTAL │   %10lu   │  100.00%%                          ║\n",
           stats->total_transactions);
    
    // Estadísticas de eficiencia
    if (stats->total_transactions > 0) {
        double avg_bytes_per_transaction = (double)stats->bytes_transferred / stats->total_transactions;
        printf("║                                                                ║\n");
        printf("║  📈 EFICIENCIA                                                ║\n");
        printf("╟────────────────────────────────────────────────────────────────╢\n");
        printf("║    - Bytes/transacción promedio:  %.2f bytes              ║\n", 
               avg_bytes_per_transaction);
        
        double read_ratio = stats->total_transactions > 0 ?
            (100.0 * stats->bus_rd_count / stats->total_transactions) : 0.0;
        double write_ratio = stats->total_transactions > 0 ?
            (100.0 * (stats->bus_rdx_count + stats->bus_wb_count) / stats->total_transactions) : 0.0;
        
        printf("║    - Ratio lecturas:               %.2f%%                  ║\n", read_ratio);
        printf("║    - Ratio escrituras:             %.2f%%                  ║\n", write_ratio);
    }
    
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

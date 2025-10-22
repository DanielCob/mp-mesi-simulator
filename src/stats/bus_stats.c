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
    printf("\n[Estadísticas del bus]\n");
    printf("Transacciones: BUS_RD=%lu BUS_RDX=%lu BUS_UPGR=%lu BUS_WB=%lu Total=%lu\n",
           stats->bus_rd_count, stats->bus_rdx_count, stats->bus_upgr_count,
           stats->bus_wb_count, stats->total_transactions);
    printf("Coherencia: invalidaciones_broadcast=%lu\n", stats->invalidations_sent);

    double traffic_kb = stats->bytes_transferred / 1024.0;
    double traffic_mb = traffic_kb / 1024.0;
    double data_kb = stats->bytes_data / 1024.0;
    double control_kb = stats->bytes_control / 1024.0;

    printf("Tráfico: datos=%lu (%.2f KB) control=%lu (%.2f KB) total=%lu (%.2f KB, %.6f MB)\n",
           stats->bytes_data, data_kb,
           stats->bytes_control, control_kb,
           stats->bytes_transferred, traffic_kb, traffic_mb);

    printf("Uso por PE (transacciones y %%):\n");
    for (int i = 0; i < 4; i++) {
        double percentage = stats->total_transactions > 0 ?
            (100.0 * stats->transactions_per_pe[i] / stats->total_transactions) : 0.0;
        printf("  PE%d: %lu (%.2f%%)\n", i, stats->transactions_per_pe[i], percentage);
    }

    if (stats->total_transactions > 0) {
        double avg_bytes_per_transaction = (double)stats->bytes_transferred / stats->total_transactions;
        double read_ratio = (100.0 * stats->bus_rd_count) / stats->total_transactions;
        double write_ratio = (100.0 * (stats->bus_rdx_count + stats->bus_wb_count)) / stats->total_transactions;
        printf("Eficiencia: bytes/transacción=%.2f lecturas=%.2f%% escrituras=%.2f%%\n",
               avg_bytes_per_transaction, read_ratio, write_ratio);
    }
}

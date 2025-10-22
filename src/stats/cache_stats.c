#include "cache_stats.h"
#include <stdio.h>
#include <string.h>
#include "log.h"

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
       const char* B = log_color_bold();
       const char* BLUE = log_color_blue();
       const char* RESET = log_color_reset();

       printf("\n%s[PE%d statistics]%s\n", BLUE, pe_id, RESET);
       // Cache performance
    
    uint64_t total_accesses = stats->total_reads + stats->total_writes;
    uint64_t total_hits = stats->read_hits + stats->write_hits;
    uint64_t total_misses = stats->read_misses + stats->write_misses;
    
    double hit_rate = total_accesses > 0 ? (100.0 * total_hits / total_accesses) : 0.0;
    double miss_rate = total_accesses > 0 ? (100.0 * total_misses / total_accesses) : 0.0;
    
    printf("%sReads%s: hits=%lu misses=%lu total=%lu\n", 
           B, RESET, stats->read_hits, stats->read_misses, stats->total_reads);
    printf("%sWrites%s: hits=%lu misses=%lu total=%lu\n", 
           B, RESET, stats->write_hits, stats->write_misses, stats->total_writes);
    printf("%sTotals%s: hits=%lu (%.2f%%) misses=%lu (%.2f%%) accesses=%lu\n", 
           B, RESET, total_hits, hit_rate, total_misses, miss_rate, total_accesses);
    
    // Coherence
    printf("%sInvalidations%s: received=%lu sent=%lu\n", 
           B, RESET, stats->invalidations_received, stats->invalidations_sent);
    
    // Bus Traffic
    printf("%sBus%s: BusRd=%lu BusRdX=%lu BusUpgr=%lu WB=%lu\n", 
           B, RESET, stats->bus_reads, stats->bus_read_x, stats->bus_upgrades, stats->bus_writebacks);
    
    double total_mb = (stats->bytes_read_from_bus + stats->bytes_written_to_bus) / (1024.0 * 1024.0);
    printf("%sTraffic%s: read=%lu (%.2f KB) written=%lu (%.2f KB) total=%.6f MB\n", 
           B, RESET,
           stats->bytes_read_from_bus, stats->bytes_read_from_bus / 1024.0,
           stats->bytes_written_to_bus, stats->bytes_written_to_bus / 1024.0, total_mb);
    
    // MESI Transitions
    printf("%sMESI transitions%s:\n", B, RESET);
    printf("  I->E=%lu I->S=%lu I->M=%lu\n", 
           stats->transitions.I_to_E, stats->transitions.I_to_S, stats->transitions.I_to_M);
    printf("  E->M=%lu E->S=%lu E->I=%lu\n", 
           stats->transitions.E_to_M, stats->transitions.E_to_S, stats->transitions.E_to_I);
    printf("  S->M=%lu S->I=%lu\n", 
           stats->transitions.S_to_M, stats->transitions.S_to_I);
    printf("  M->S=%lu M->I=%lu\n", 
           stats->transitions.M_to_S, stats->transitions.M_to_I);
}

void stats_print_summary(const CacheStats* stats_array, int num_pes) {
       const char* B = log_color_bold();
       const char* BLUE = log_color_blue();
       const char* RESET = log_color_reset();

       printf("\n%s[Statistics summary]%s\n", BLUE, RESET);
       printf("%sHit rates per PE%s:\n", B, RESET);
    
    uint64_t total_accesses_all = 0;
    uint64_t total_hits_all = 0;
    uint64_t total_misses_all = 0;
    
    for (int i = 0; i < num_pes; i++) {
        uint64_t accesses = stats_array[i].total_reads + stats_array[i].total_writes;
        uint64_t hits = stats_array[i].read_hits + stats_array[i].write_hits;
        uint64_t misses = stats_array[i].read_misses + stats_array[i].write_misses;
        
        double hit_rate = accesses > 0 ? (100.0 * hits / accesses) : 0.0;
        double miss_rate = accesses > 0 ? (100.0 * misses / accesses) : 0.0;
        
        printf("  PE%d: accesses=%lu hits=%lu misses=%lu hit=%.2f%% miss=%.2f%%\n",
               i, accesses, hits, misses, hit_rate, miss_rate);
        
        total_accesses_all += accesses;
        total_hits_all += hits;
        total_misses_all += misses;
    }
    
    double avg_hit_rate = total_accesses_all > 0 ? (100.0 * total_hits_all / total_accesses_all) : 0.0;
    double avg_miss_rate = total_accesses_all > 0 ? (100.0 * total_misses_all / total_accesses_all) : 0.0;
    
    printf("Total: accesses=%lu hits=%lu misses=%lu hit=%.2f%% miss=%.2f%%\n",
           total_accesses_all, total_hits_all, total_misses_all, avg_hit_rate, avg_miss_rate);
    
    // Invalidaciones
       printf("%sInvalidations per PE%s:\n", B, RESET);
    
    uint64_t total_inv_received = 0;
    uint64_t total_inv_sent = 0;
    
    for (int i = 0; i < num_pes; i++) {
        uint64_t received = stats_array[i].invalidations_received;
        uint64_t sent = stats_array[i].invalidations_sent;
        
        printf("  PE%d: received=%lu sent=%lu total=%lu\n",
               i, received, sent, received + sent);
        
        total_inv_received += received;
        total_inv_sent += sent;
    }
    
    printf("Total: received=%lu sent=%lu total=%lu\n",
           total_inv_received, total_inv_sent, total_inv_received + total_inv_sent);
    
    // Tráfico del bus
       printf("%sBus traffic per PE%s (KB):\n", B, RESET);
    
    double total_traffic_kb = 0.0;
    
    for (int i = 0; i < num_pes; i++) {
        uint64_t bus_rd = stats_array[i].bus_reads;
        uint64_t bus_rdx = stats_array[i].bus_read_x;
        uint64_t bus_upgr = stats_array[i].bus_upgrades;
        uint64_t wb = stats_array[i].bus_writebacks;
        
        double traffic_kb = (stats_array[i].bytes_read_from_bus + 
                            stats_array[i].bytes_written_to_bus) / 1024.0;
        
       printf("  PE%d: BusRd=%lu BusRdX=%lu BusUpgr=%lu WB=%lu Traffic=%.2f\n",
               i, bus_rd, bus_rdx, bus_upgr, wb, traffic_kb);
        
        total_traffic_kb += traffic_kb;
    }
    
    printf("Total bus traffic: %.2f KB (%.6f MB)\n",
           total_traffic_kb, total_traffic_kb / 1024.0);
}

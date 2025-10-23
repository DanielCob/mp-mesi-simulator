#include "memory_stats.h"
#include <stdio.h>
#include <string.h>
#include "log.h"

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
    const char* B = log_color_bold();
    const char* BLUE = log_color_blue();
    const char* RESET = log_color_reset();

    printf("\n%s[Memory statistics]%s\n", BLUE, RESET);
    printf("%sAccesses%s: reads=%lu writes=%lu total=%lu\n",
        B, RESET, stats->reads, stats->writes, stats->total_accesses);

    double read_kb = stats->bytes_read / 1024.0;
    double write_kb = stats->bytes_written / 1024.0;
    double total_mb = (stats->bytes_read + stats->bytes_written) / (1024.0 * 1024.0);

    printf("%sTraffic%s: read=%lu (%.2f KB) written=%lu (%.2f KB) total=%.6f MB\n",
        B, RESET, stats->bytes_read, read_kb, stats->bytes_written, write_kb, total_mb);

    printf("%sAccesses per PE%s:\n", B, RESET);
    for (int i = 0; i < 4; i++) {
        uint64_t total_pe = stats->reads_per_pe[i] + stats->writes_per_pe[i];
        printf("  PE%d: reads=%lu writes=%lu total=%lu\n",
               i, stats->reads_per_pe[i], stats->writes_per_pe[i], total_pe);
    }
}

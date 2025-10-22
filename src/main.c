#include <stdio.h>
#include "config.h"
#include "cache_stats.h"
#include "memory_stats.h"
#include "bus_stats.h"
#include "dotprod.h"
#include "pe.h"
#include "bus.h"
#include "memory.h"
#include "log.h"

int main() {
    log_init();
    LOGI("Starting MESI simulator - Parallel dot product");
    
    // Inicializar memoria y crear su thread
    Memory mem;
    mem_init(&mem);
    
    // ===== INITIALIZE DOT PRODUCT DATA =====
    dotprod_init_data(&mem);
    
    pthread_t mem_thread;
    pthread_create(&mem_thread, NULL, mem_thread_func, &mem);

    Bus bus;
    Cache caches[NUM_PES];
    PE pes[NUM_PES];
    pthread_t pe_threads[NUM_PES];
    pthread_t bus_thread;

    // Inicializar cachés
    for (int i = 0; i < NUM_PES; i++) {
        cache_init(&caches[i]);
        caches[i].bus = &bus;
    caches[i].pe_id = i;  // Assign PE ID
    }

    // Initialize bus with memory reference
    Cache* cache_ptrs[NUM_PES];
    for (int i = 0; i < NUM_PES; i++)
        cache_ptrs[i] = &caches[i];
    bus_init(&bus, cache_ptrs, &mem);

    // Create bus thread
    pthread_create(&bus_thread, NULL, bus_thread_func, &bus);

    // Initialize and create PE threads
    for (int i = 0; i < NUM_PES; i++) {
        pes[i].id = i;
        pes[i].cache = &caches[i];
    reg_init(&pes[i].rf);  // Initialize register file
        pthread_create(&pe_threads[i], NULL, pe_run, &pes[i]);
    }

    // Join PE threads
    for (int i = 0; i < NUM_PES; i++)
        pthread_join(pe_threads[i], NULL);

    LOGI("All PEs finished execution");

    // ===== SHOW DOT PRODUCT RESULTS =====
    // PEs already did writeback on HALT via the bus, so all modified data is in main memory
    dotprod_print_results(&mem);

    // Stop bus and join its thread
    bus_destroy(&bus);
    pthread_join(bus_thread, NULL);

    // Stop memory and join its thread
    mem_destroy(&mem);
    pthread_join(mem_thread, NULL);

    // Print per-PE statistics
    LOGI("Printing simulator statistics");
    
    for (int i = 0; i < NUM_PES; i++) {
        stats_print(&caches[i].stats, i);
    }
    
    // Print comparative summary
    CacheStats stats_array[NUM_PES];
    for (int i = 0; i < NUM_PES; i++) {
        stats_array[i] = caches[i].stats;
    }
    stats_print_summary(stats_array, NUM_PES);

    // Print memory statistics
    memory_stats_print(&mem.stats);

    // Print bus statistics
    bus_stats_print(&bus.stats);

    // Cleanup resources
    for (int i = 0; i < NUM_PES; i++) {
        cache_destroy(&caches[i]);
    }

    return 0;
}

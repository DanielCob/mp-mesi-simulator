#include <stdio.h>
#include "config.h"
#include "cache_stats.h"
#include "memory_stats.h"
#include "bus_stats.h"
#include "dotprod.h"
#include "pe.h"
#include "bus.h"
#include "memory.h"

int main() {
    printf("================================================================================\n");
    printf("           MESI MULTIPROCESSOR SIMULATOR - DOT PRODUCT PARALLEL                \n");
    printf("================================================================================\n\n");
    
    // Inicializar memoria y crear su thread
    Memory mem;
    mem_init(&mem);
    
    // ===== INICIALIZAR DATOS DEL PRODUCTO PUNTO =====
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
        caches[i].pe_id = i;  // Asignar ID del PE
    }

    // Inicializar bus con referencia a memoria
    Cache* cache_ptrs[NUM_PES];
    for (int i = 0; i < NUM_PES; i++)
        cache_ptrs[i] = &caches[i];
    bus_init(&bus, cache_ptrs, &mem);

    // Crear thread del bus
    pthread_create(&bus_thread, NULL, bus_thread_func, &bus);

    // Inicializar y crear threads de PEs
    for (int i = 0; i < NUM_PES; i++) {
        pes[i].id = i;
        pes[i].cache = &caches[i];
        reg_init(&pes[i].rf);  // Inicializar banco de registros
        pthread_create(&pe_threads[i], NULL, pe_run, &pes[i]);
    }

    // Esperar hilos de PEs
    for (int i = 0; i < NUM_PES; i++)
        pthread_join(pe_threads[i], NULL);

    printf("\n[Main] All PEs have finished execution\n");

    // ===== MOSTRAR RESULTADOS DEL PRODUCTO PUNTO =====
    // Los PEs ya hicieron writeback en HALT a través del bus
    // por lo que todos los datos modificados están en memoria principal
    dotprod_print_results(&mem);

    // Terminar el bus y esperar su thread
    bus_destroy(&bus);
    pthread_join(bus_thread, NULL);

    // Terminar memoria y esperar su thread
    mem_destroy(&mem);
    pthread_join(mem_thread, NULL);

    // Imprimir estadísticas de cada PE
    printf("\n");
    printf("================================================================================\n");
    printf("                         ESTADÍSTICAS DEL SIMULADOR                             \n");
    printf("================================================================================\n");
    
    for (int i = 0; i < NUM_PES; i++) {
        stats_print(&caches[i].stats, i);
    }
    
    // Imprimir resumen comparativo
    CacheStats stats_array[NUM_PES];
    for (int i = 0; i < NUM_PES; i++) {
        stats_array[i] = caches[i].stats;
    }
    stats_print_summary(stats_array, NUM_PES);

    // Imprimir estadísticas de memoria
    printf("\n");
    memory_stats_print(&mem.stats);

    // Imprimir estadísticas del bus
    printf("\n");
    bus_stats_print(&bus.stats);

    // Limpiar recursos
    for (int i = 0; i < NUM_PES; i++) {
        cache_destroy(&caches[i]);
    }

    return 0;
}

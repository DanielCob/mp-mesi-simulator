#include "include/config.h"
#include "pe/pe.h"
#include "bus/bus.h"
#include "memory/memory.h"

int main() {
    mem_init();

    Bus bus;
    Cache caches[NUM_PES];
    PE pes[NUM_PES];
    pthread_t pe_threads[NUM_PES];
    pthread_t bus_thread;

    // Inicializar cachés
    for (int i = 0; i < NUM_PES; i++) {
        cache_init(&caches[i]);
        caches[i].bus = &bus;
    }

    // Inicializar bus
    Cache* cache_ptrs[NUM_PES];
    for (int i = 0; i < NUM_PES; i++)
        cache_ptrs[i] = &caches[i];
    bus_init(&bus, cache_ptrs);

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

    // Terminar el bus y esperar su thread
    bus_destroy(&bus);
    pthread_join(bus_thread, NULL);

    // Limpiar recursos
    for (int i = 0; i < NUM_PES; i++) {
        cache_destroy(&caches[i]);
    }
    
    mem_destroy();

    return 0;
}

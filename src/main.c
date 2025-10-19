#include "include/config.h"
#include "pe/pe.h"
#include "bus/bus.h"
#include "memory/memory.h"

int main() {
    // Inicializar memoria y crear su thread
    Memory mem;
    mem_init(&mem);
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

    // Terminar el bus y esperar su thread
    bus_destroy(&bus);
    pthread_join(bus_thread, NULL);

    // Terminar memoria y esperar su thread
    mem_destroy(&mem);
    pthread_join(mem_thread, NULL);

    // Limpiar recursos
    for (int i = 0; i < NUM_PES; i++) {
        cache_destroy(&caches[i]);
    }

    return 0;
}

#include "handlers.h"
#include "../memory/memory.h"
#include "../cache/cache.h"
#include <stdio.h>

void bus_register_handlers(Bus* bus) {
    bus->handlers[BUS_RD]   = handle_busrd;
    bus->handlers[BUS_RDX]  = handle_busrdx;
    bus->handlers[BUS_UPGR] = handle_busupgr;
    bus->handlers[BUS_WB]   = handle_buswb;
}

void handle_busrd(Bus* bus, int addr, int src_pe) {
    // BUS_RD: Otro procesador quiere leer la línea
    double shared_data = 0.0;
    int data_found = 0;
    Cache* requestor = bus->caches[src_pe];
    
    for (int i = 0; i < NUM_PES; i++) {
        if (i != src_pe) {
            Cache* cache = bus->caches[i];
            MESI_State state = cache_get_state(cache, addr);
            
            if (state == M) {
                // Proveer dato y escribir a memoria
                shared_data = cache_get_data(cache, addr);
                printf("  [Cache PE%d] Tiene dato en M (%.2f), haciendo WRITEBACK y pasando a S\n", 
                       i, shared_data);
                mem_write(addr, shared_data);
                cache_set_state(cache, addr, S);
                data_found = 1;
                break;  // El dato en M es el más actualizado
            } else if (state == E) {
                // Proveer dato, no necesita writeback
                shared_data = cache_get_data(cache, addr);
                printf("  [Cache PE%d] Tiene dato en E (%.2f), pasando a S\n", i, shared_data);
                cache_set_state(cache, addr, S);
                data_found = 1;
                break;  // El dato en E es válido
            } else if (state == S && !data_found) {
                // Tomar el dato de cualquier cache que lo tenga en S
                shared_data = cache_get_data(cache, addr);
                printf("  [Cache PE%d] Tiene dato en S (%.2f), compartiendo\n", i, shared_data);
                data_found = 1;
                break;  // Podemos salir, todas las otras copias válidas deben estar en S
            }
        }
    }

    // Si ningún cache tiene el dato, leer de memoria
    if (!data_found) {
        printf("[Bus] Read miss, leyendo de memoria principal addr=%d\n", addr);
        shared_data = mem_read(addr);
        printf("  [Memoria] Devolviendo valor %.2f\n", shared_data);
        // El primer lector obtiene estado E
        cache_set_data(requestor, addr, shared_data);
        cache_set_state(requestor, addr, E);
    } else {
        // Si obtuvimos el dato de otro cache, estado S
        printf("  [Bus] Suministrando valor %.2f al PE%d (estado S)\n", shared_data, src_pe);
        cache_set_data(requestor, addr, shared_data);
        cache_set_state(requestor, addr, S);
    }
}

void handle_busrdx(Bus* bus, int addr, int src_pe) {
    // BUS_RDX: Otro procesador quiere escribir la línea
    double exclusive_data = 0.0;
    int data_found = 0;
    Cache* requestor = bus->caches[src_pe];
    
    for (int i = 0; i < NUM_PES; i++) {
        if (i != src_pe) {
            Cache* cache = bus->caches[i];
            MESI_State state = cache_get_state(cache, addr);
            
            if (state == M) {
                // Proveer dato y escribir a memoria
                exclusive_data = cache_get_data(cache, addr);
                printf("  [Cache PE%d] Tiene dato en M (%.2f), haciendo WRITEBACK e invalidando\n", 
                       i, exclusive_data);
                mem_write(addr, exclusive_data);
                cache_set_state(cache, addr, I);
                data_found = 1;
                break;  // El dato en M es el más actualizado
            } else if (state == E) {
                exclusive_data = cache_get_data(cache, addr);
                printf("  [Cache PE%d] Tiene dato en E (%.2f), invalidando\n", i, exclusive_data);
                cache_set_state(cache, addr, I);
                data_found = 1;
                break;  // El dato en E es válido
            } else if (state == S) {
                // Se debe invalidar todas las copias en S, y guardar el dato para el solicitante
                if (!data_found)
                {
                    exclusive_data = cache_get_data(cache, addr);
                    printf("  [Cache PE%d] Tiene dato en S (%.2f), invalidando\n", i, exclusive_data);
                    data_found = 1;
                }
                else {
                    printf("  [Cache PE%d] Tiene dato en S, invalidando\n", i);
                }
                
                cache_set_state(cache, addr, I);
            }
        }
    }

    // Si no encontramos el dato en otro cache, leer de memoria
    if (!data_found) {
        printf("[Bus] RDX miss, leyendo de memoria principal addr=%d\n", addr);
        exclusive_data = mem_read(addr);
        printf("  [Memoria] Devolviendo valor %.2f\n", exclusive_data);
    }

    // Actualizar el cache del solicitante
    printf("  [Bus] Suministrando valor %.2f al PE%d (estado M)\n", exclusive_data, src_pe);
    cache_set_data(requestor, addr, exclusive_data);
    cache_set_state(requestor, addr, M);
}

void handle_busupgr(Bus* bus, int addr, int src_pe) {
    // BUS_UPGR: Un procesador quiere actualizar de S a M
    Cache* requestor = bus->caches[src_pe];
    
    for (int i = 0; i < NUM_PES; i++) {
        if (i != src_pe) {
            Cache* cache = bus->caches[i];
            MESI_State state = cache_get_state(cache, addr);
            
            if (state == S) {
                printf("  [Cache PE%d] Invalidando línea en S\n", i);
                cache_set_state(cache, addr, I);
            }
        }
    }

    // El dato ya está en el cache, solo actualizamos estado
    cache_set_state(requestor, addr, M);
}

void handle_buswb(Bus* bus, int addr, int src_pe) {
    // BUS_WB: Un procesador está escribiendo de vuelta a memoria
    Cache* writer = bus->caches[src_pe];
    
    if (cache_get_state(writer, addr) == M) {
        double data = cache_get_data(writer, addr);
        printf("[Bus] Writeback a memoria principal addr=%d valor=%.2f\n", addr, data);
        mem_write(addr, data);
    }
    
    cache_set_state(writer, addr, I);
}

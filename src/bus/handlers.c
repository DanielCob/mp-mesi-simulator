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
    // THREAD-SAFETY: Este handler es llamado por el bus thread (serializado)
    // Las funciones cache_get/set_* ya tienen mutex interno
    int data_found = 0;
    Cache* requestor = bus->caches[src_pe];
    
    for (int i = 0; i < NUM_PES; i++) {
        if (i != src_pe) {
            Cache* cache = bus->caches[i];
            // cache_get_state tiene mutex interno
            MESI_State state = cache_get_state(cache, addr);
            
            if (state == M) {
                // Proveer bloque completo y escribir a memoria
                double block[BLOCK_SIZE];
                cache_get_block(cache, addr, block);
                printf("  [Cache PE%d] Tiene bloque en M, haciendo WRITEBACK y pasando a S\n", i);
                mem_write_block(bus->memory, addr, block);  // ← Escribe bloque completo
                // Proveer el bloque al solicitante
                cache_set_block(requestor, addr, block);
                cache_set_state(cache, addr, S);
                cache_set_state(requestor, addr, S);
                data_found = 1;
                return;  // Ya completamos la operación
            } else if (state == E) {
                // Proveer bloque, no necesita writeback
                double block[BLOCK_SIZE];
                cache_get_block(cache, addr, block);
                printf("  [Cache PE%d] Tiene bloque en E, pasando a S\n", i);
                cache_set_block(requestor, addr, block);
                cache_set_state(cache, addr, S);
                cache_set_state(requestor, addr, S);
                data_found = 1;
                return;  // Ya completamos la operación
            } else if (state == S && !data_found) {
                // Tomar el bloque de cualquier cache que lo tenga en S
                double block[BLOCK_SIZE];
                cache_get_block(cache, addr, block);
                printf("  [Cache PE%d] Tiene bloque en S, compartiendo\n", i);
                cache_set_block(requestor, addr, block);
                cache_set_state(requestor, addr, S);
                data_found = 1;
                return;  // Ya completamos la operación
            }
        }
    }

    // Si ningún cache tiene el dato, leer de memoria
    if (!data_found) {
        printf("[Bus] Read miss, leyendo BLOQUE desde memoria addr=%d\n", addr);
        double block[BLOCK_SIZE];
        mem_read_block(bus->memory, addr, block);  // ← Lee bloque completo
        printf("  [Memoria] Devolviendo bloque [%.2f, %.2f, %.2f, %.2f]\n", 
               block[0], block[1], block[2], block[3]);
        // El primer lector obtiene estado E
        cache_set_block(requestor, addr, block);  // ← Establece bloque completo
        cache_set_state(requestor, addr, E);
    }
    // Nota: Si data_found es true, ya hicimos return en el loop
}

void handle_busrdx(Bus* bus, int addr, int src_pe) {
    // BUS_RDX: Otro procesador quiere escribir (pide permisos exclusivos)
    // THREAD-SAFETY: Este handler es llamado por el bus thread (serializado)
    // Las funciones cache_get/set_* ya tienen mutex interno
    int data_found = 0;
    Cache* requestor = bus->caches[src_pe];
    
    for (int i = 0; i < NUM_PES; i++) {
        if (i != src_pe) {
            Cache* cache = bus->caches[i];
            // cache_get_state tiene mutex interno
            MESI_State state = cache_get_state(cache, addr);
            
            if (state == M) {
                // Proveer bloque completo y escribir a memoria
                double block[BLOCK_SIZE];
                cache_get_block(cache, addr, block);
                printf("  [Cache PE%d] Tiene bloque en M, haciendo WRITEBACK e INVALIDANDO\n", i);
                mem_write_block(bus->memory, addr, block);  // ← Escribe bloque completo
                // Proveer bloque al solicitante
                cache_set_block(requestor, addr, block);
                cache_set_state(cache, addr, I);
                cache_set_state(requestor, addr, M);
                data_found = 1;
                return;
            } else if (state == E || state == S) {
                if (!data_found) {
                    // Proveer bloque solo si aún no se encontró
                    double block[BLOCK_SIZE];
                    cache_get_block(cache, addr, block);
                    printf("  [Cache PE%d] Tiene bloque en %c, proveyendo e INVALIDANDO\n", i, state == E ? 'E' : 'S');
                    cache_set_block(requestor, addr, block);
                    cache_set_state(requestor, addr, M);
                    data_found = 1;
                }
                // Invalidar en todos los casos (E o S)
                cache_set_state(cache, addr, I);
            }
        }
    }
}

void handle_busupgr(Bus* bus, int addr, int src_pe) {
    // BUS_UPGR: Un procesador quiere actualizar de S a M
    // THREAD-SAFETY: Serializado por el bus thread
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
    // THREAD-SAFETY: Serializado por el bus thread
    Cache* writer = bus->caches[src_pe];
    
    if (cache_get_state(writer, addr) == M) {
        double block[BLOCK_SIZE];
        cache_get_block(writer, addr, block);  // ← Obtiene bloque completo
        printf("[Bus] Writeback BLOQUE a memoria addr=%d [%.2f, %.2f, %.2f, %.2f]\n", 
               addr, block[0], block[1], block[2], block[3]);
        mem_write_block(bus->memory, addr, block);  // ← Escribe bloque completo
    }
    
    cache_set_state(writer, addr, I);
}

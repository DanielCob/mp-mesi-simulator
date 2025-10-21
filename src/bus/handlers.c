#include "handlers.h"
#include "memory.h"
#include "cache.h"
#include <stdio.h>

// REGISTRO DE HANDLERS

void bus_register_handlers(Bus* bus) {
    bus->handlers[BUS_RD]   = handle_busrd;
    bus->handlers[BUS_RDX]  = handle_busrdx;
    bus->handlers[BUS_UPGR] = handle_busupgr;
    bus->handlers[BUS_WB]   = handle_buswb;
}

// HANDLER: BUS_RD (Lectura compartida)

void handle_busrd(Bus* bus, int addr, int src_pe) {
    int data_found = 0;
    Cache* requestor = bus->caches[src_pe];
    
    for (int i = 0; i < NUM_PES; i++) {
        if (i != src_pe) {
            Cache* cache = bus->caches[i];
            MESI_State state = cache_get_state(cache, addr);
            
            if (state == M) {
                double block[BLOCK_SIZE];
                cache_get_block(cache, addr, block);
                printf("  [Cache PE%d] Tiene bloque en M, haciendo WRITEBACK y pasando a S\n", i);
                mem_write_block(bus->memory, addr, block, src_pe);
                cache_set_block(requestor, addr, block);
                cache_set_state(cache, addr, S);
                cache_set_state(requestor, addr, S);
                data_found = 1;
                return;
            } else if (state == E) {
                double block[BLOCK_SIZE];
                cache_get_block(cache, addr, block);
                printf("  [Cache PE%d] Tiene bloque en E, pasando a S\n", i);
                cache_set_block(requestor, addr, block);
                cache_set_state(cache, addr, S);
                cache_set_state(requestor, addr, S);
                data_found = 1;
                return;
            } else if (state == S && !data_found) {
                double block[BLOCK_SIZE];
                cache_get_block(cache, addr, block);
                printf("  [Cache PE%d] Tiene bloque en S, compartiendo\n", i);
                cache_set_block(requestor, addr, block);
                cache_set_state(requestor, addr, S);
                data_found = 1;
                return;
            }
        }
    }

    // Ningún cache tiene el dato, leer de memoria
    if (!data_found) {
        printf("[Bus] Read miss, leyendo BLOQUE desde memoria addr=%d\n", addr);
        double block[BLOCK_SIZE];
        mem_read_block(bus->memory, addr, block, src_pe);
        printf("  [Memoria] Devolviendo bloque [%.2f, %.2f, %.2f, %.2f]\n", 
               block[0], block[1], block[2], block[3]);
        cache_set_block(requestor, addr, block);
        cache_set_state(requestor, addr, E);
    }
}

// HANDLER: BUS_RDX (Lectura exclusiva para escritura)

void handle_busrdx(Bus* bus, int addr, int src_pe) {
    int data_found = 0;
    int invalidations_count = 0;
    Cache* requestor = bus->caches[src_pe];
    
    for (int i = 0; i < NUM_PES; i++) {
        if (i != src_pe) {
            Cache* cache = bus->caches[i];
            MESI_State state = cache_get_state(cache, addr);
            
            if (state == M) {
                double block[BLOCK_SIZE];
                cache_get_block(cache, addr, block);
                printf("  [Cache PE%d] Tiene bloque en M, haciendo WRITEBACK e INVALIDANDO\n", i);
                mem_write_block(bus->memory, addr, block, src_pe);
                cache_set_block(requestor, addr, block);
                cache_set_state(cache, addr, I);
                cache_set_state(requestor, addr, M);
                data_found = 1;
                invalidations_count++;
                return;
            } else if (state == E || state == S) {
                if (!data_found) {
                    double block[BLOCK_SIZE];
                    cache_get_block(cache, addr, block);
                    printf("  [Cache PE%d] Tiene bloque en %c, proveyendo e INVALIDANDO\n", i, state == E ? 'E' : 'S');
                    cache_set_block(requestor, addr, block);
                    cache_set_state(requestor, addr, M);
                    data_found = 1;
                }
                cache_set_state(cache, addr, I);
                invalidations_count++;
            }
        }
    }
    
    if (invalidations_count > 0) {
        bus_stats_record_invalidations(&bus->stats, invalidations_count);
    }
    
    // Ningún cache tiene el dato, leer de memoria
    if (!data_found) {
        printf("[Bus] Write miss, leyendo BLOQUE desde memoria addr=%d\n", addr);
        double block[BLOCK_SIZE];
        mem_read_block(bus->memory, addr, block, src_pe);
        printf("  [Memoria] Devolviendo bloque [%.2f, %.2f, %.2f, %.2f]\n", 
               block[0], block[1], block[2], block[3]);
        cache_set_block(requestor, addr, block);
        cache_set_state(requestor, addr, M);
    }
}

// HANDLER: BUS_UPGR (Upgrade de Shared a Modified)

void handle_busupgr(Bus* bus, int addr, int src_pe) {
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

    cache_set_state(requestor, addr, M);
}

// HANDLER: BUS_WB (Writeback a memoria)

void handle_buswb(Bus* bus, int addr, int src_pe) {
    Cache* writer = bus->caches[src_pe];
    
    if (cache_get_state(writer, addr) == M) {
        double block[BLOCK_SIZE];
        cache_get_block(writer, addr, block);
        printf("[Bus] Writeback BLOQUE a memoria addr=%d [%.2f, %.2f, %.2f, %.2f]\n", 
               addr, block[0], block[1], block[2], block[3]);
        mem_write_block(bus->memory, addr, block, src_pe);
    }
    
    cache_set_state(writer, addr, I);
}

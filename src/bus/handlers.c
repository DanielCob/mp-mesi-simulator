#define LOG_MODULE "BUS"
#include "handlers.h"
#include "memory.h"
#include "cache.h"
#include <stdio.h>
#include "log.h"

// REGISTRO DE HANDLERS

void bus_register_handlers(Bus* bus) {
    bus->handlers[BUS_RD]   = handle_busrd;
    bus->handlers[BUS_RDX]  = handle_busrdx;
    bus->handlers[BUS_UPGR] = handle_busupgr;
    bus->handlers[BUS_WB]   = handle_buswb;
}

// HANDLER: BUS_RD (Shared read)

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
                LOGD("Cache PE%d: M -> writeback and move to S", i);
                mem_write_block(bus->memory, addr, block, src_pe);
                cache_set_block(requestor, addr, block);
                cache_set_state(cache, addr, S);      // M->S (record transition)
                cache_set_state(requestor, addr, S);  // I->S (record transition)
                data_found = 1;
                return;
            } else if (state == E) {
                double block[BLOCK_SIZE];
                cache_get_block(cache, addr, block);
                LOGD("Cache PE%d: E -> move to S", i);
                cache_set_block(requestor, addr, block);
                cache_set_state(cache, addr, S);      // E->S (record transition)
                cache_set_state(requestor, addr, S);  // I->S (record transition)
                data_found = 1;
                return;
            } else if (state == S && !data_found) {
                double block[BLOCK_SIZE];
                cache_get_block(cache, addr, block);
                LOGD("Cache PE%d: S -> sharing", i);
                cache_set_block(requestor, addr, block);
                cache_set_state(requestor, addr, S);  // I->S (record transition)
                data_found = 1;
                return;
            }
        }
    }

    // No cache has the data, read from memory
    if (!data_found) {
           LOGD("Read miss: reading block from memory addr=0x%X", addr);
        double block[BLOCK_SIZE];
        mem_read_block(bus->memory, addr, block, src_pe);
        LOGD("Memory returns block [%.2f, %.2f, %.2f, %.2f]", 
             block[0], block[1], block[2], block[3]);
        cache_set_block(requestor, addr, block);
        cache_set_state(requestor, addr, E);  // I->E (record transition)
    }
}

// HANDLER: BUS_RDX (Exclusive read for write)

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
                LOGD("Cache PE%d: M -> writeback and invalidate", i);
                mem_write_block(bus->memory, addr, block, src_pe);
                cache_set_block(requestor, addr, block);
                cache_set_state(cache, addr, I);      // M->I (record transition)
                cache_set_state(requestor, addr, M);  // I->M (record transition)
                data_found = 1;
                invalidations_count++;
                // Record invalidations before early return
                if (invalidations_count > 0) {
                    bus_stats_record_invalidations(&bus->stats, invalidations_count);
                    // Count actual broadcast invalidations as sent by the requestor
                    for (int k = 0; k < invalidations_count; k++) {
                        stats_record_invalidation_sent(&requestor->stats);
                    }
                    // Account additional control per invalidated cache
                    bus_stats_record_control_invalidations(&bus->stats, invalidations_count * INVALIDATION_CONTROL_SIGNAL_SIZE);
                }
                return;
            } else if (state == E || state == S) {
                if (!data_found) {
                    double block[BLOCK_SIZE];
                    cache_get_block(cache, addr, block);
                    LOGD("Cache PE%d: %c -> provide and invalidate", i, state == E ? 'E' : 'S');
                    cache_set_block(requestor, addr, block);
                    cache_set_state(requestor, addr, M);  // I->M (record transition)
                    data_found = 1;
                }
                cache_set_state(cache, addr, I);  // E->I or S->I (record transition)
                invalidations_count++;
            }
        }
    }
    
    if (invalidations_count > 0) {
        bus_stats_record_invalidations(&bus->stats, invalidations_count);
        // Count actual broadcast invalidations as sent by the requestor
        for (int k = 0; k < invalidations_count; k++) {
            stats_record_invalidation_sent(&requestor->stats);
        }
    // Account additional control per invalidated cache
    bus_stats_record_control_invalidations(&bus->stats, invalidations_count * INVALIDATION_CONTROL_SIGNAL_SIZE);
    }
    
    // No cache has the data, read from memory
    if (!data_found) {
           LOGD("Write miss: reading block from memory addr=0x%X", addr);
        double block[BLOCK_SIZE];
        mem_read_block(bus->memory, addr, block, src_pe);
        LOGD("Memory returns block [%.2f, %.2f, %.2f, %.2f]", 
             block[0], block[1], block[2], block[3]);
        cache_set_block(requestor, addr, block);
        cache_set_state(requestor, addr, M);  // I->M (record transition)
    }
}

// HANDLER: BUS_UPGR (Upgrade from Shared to Modified)

void handle_busupgr(Bus* bus, int addr, int src_pe) {
    Cache* requestor = bus->caches[src_pe];
    int invalidations_count = 0;
    
    for (int i = 0; i < NUM_PES; i++) {
        if (i != src_pe) {
            Cache* cache = bus->caches[i];
            MESI_State state = cache_get_state(cache, addr);
            
            if (state == S) {
                LOGD("Cache PE%d: invalidate line in S", i);
                cache_set_state(cache, addr, I);  // S->I (record transition)
                invalidations_count++;
            }
        }
    }

    if (invalidations_count > 0) {
        bus_stats_record_invalidations(&bus->stats, invalidations_count);
        for (int k = 0; k < invalidations_count; k++) {
            stats_record_invalidation_sent(&requestor->stats);
        }
    // Account additional control per invalidated cache
    bus_stats_record_control_invalidations(&bus->stats, invalidations_count * INVALIDATION_CONTROL_SIGNAL_SIZE);
    }

    cache_set_state(requestor, addr, M);  // S->M (registra transición)
}

// HANDLER: BUS_WB (Writeback to memory)

void handle_buswb(Bus* bus, int addr, int src_pe) {
    Cache* writer = bus->caches[src_pe];
    
    if (cache_get_state(writer, addr) == M) {
        double block[BLOCK_SIZE];
        cache_get_block(writer, addr, block);
       LOGD("Write block to memory addr=%d [%.2f, %.2f, %.2f, %.2f]", 
           addr, block[0], block[1], block[2], block[3]);
       LOGD("Write block to memory addr=0x%X [%.2f, %.2f, %.2f, %.2f]", 
           addr, block[0], block[1], block[2], block[3]);
        mem_write_block(bus->memory, addr, block, src_pe);
    }
    
    cache_set_state(writer, addr, I);
}

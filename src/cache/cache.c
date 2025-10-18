#include "cache.h"
#include "bus/bus.h"
#include <stdio.h>

void cache_init(Cache* cache) {
    cache->bus = NULL;
    for (int i = 0; i < SETS; i++)
        for (int j = 0; j < WAYS; j++) {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].state = I;
        }
}

double cache_read(Cache* cache, int addr, int pe_id) {
    int set_index = addr % SETS;
    unsigned long tag = addr / SETS;
    CacheSet* set = &cache->sets[set_index];

    // Buscar hit en el cache
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            // HIT: estados M, E o S pueden servir la lectura directamente
            if (state == M || state == E || state == S) {
                printf("[PE%d] READ HIT en set %d (way %d, estado %c) -> valor=%.2f\n", 
                       pe_id, set_index, i, "MESI"[state], set->lines[i].data[0]);
                return set->lines[i].data[0];
            }
            
            // Línea con tag correcto pero invalidada (estado I)
            // La reutilizaremos después del broadcast
            printf("[PE%d] READ tag match pero estado I en set %d (way %d)\n", 
                   pe_id, set_index, i);
            break;
        }
    }

    // MISS: No hay hit válido, necesitamos traer la línea
    printf("[PE%d] READ MISS en set %d, enviando BUS_RD\n", pe_id, set_index);

    // Seleccionar línea víctima usando la política de reemplazo
    CacheLine* victim = cache_select_victim(cache, set_index, pe_id);

    // Preparar la línea para recibir datos
    victim->valid = 1;
    victim->tag = tag;
    victim->state = I;  // Temporalmente inválido antes del broadcast

    // Enviar BUS_RD - el handler actualizará el estado y datos
    bus_broadcast(cache->bus, BUS_RD, addr, pe_id);
/* 
    // El handler debería haber actualizado la línea a E o S
    // Si por alguna razón no lo hizo, leer de memoria como fallback
    if (victim->state == I) {
        printf("[PE%d] WARNING: handler no actualizó estado, leyendo de memoria\n", pe_id);
        victim->data[0] = mem_read(addr);
        victim->state = E;
    } */

    printf("[PE%d] READ completado -> valor=%.2f (estado %c)\n", 
           pe_id, victim->data[0], "MESI"[victim->state]);
    return victim->data[0];
}

void cache_write(Cache* cache, int addr, double value, int pe_id) {
    int set_index = addr % SETS;
    unsigned long tag = addr / SETS;
    CacheSet* set = &cache->sets[set_index];

    // Buscar si la línea ya está en el cache
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            if (state == M) {
                // HIT en M: escribir directamente, ya tenemos permisos exclusivos
                printf("[PE%d] WRITE HIT en set %d (way %d, estado M) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, value);
                set->lines[i].data[0] = value;
                return;
            } 
            else if (state == E) {
                // HIT en E: escribir y cambiar a M
                printf("[PE%d] WRITE HIT en set %d (way %d, estado E->M) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, value);
                set->lines[i].data[0] = value;
                set->lines[i].state = M;
                return;
            } 
            else if (state == S) {
                // HIT en S: necesitamos invalidar otras copias con BUS_UPGR
                printf("[PE%d] WRITE HIT en set %d (way %d, estado S->M, enviando BUS_UPGR) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, value);
                bus_broadcast(cache->bus, BUS_UPGR, addr, pe_id);
                set->lines[i].data[0] = value;
                set->lines[i].state = M;
                return;
            }
            // Si estado == I, continuar al MISS (línea invalidada)
            break;
        }
    }

    // MISS: No hay línea válida con ese tag
    printf("[PE%d] WRITE MISS en set %d, enviando BUS_RDX -> valor a escribir=%.2f\n", 
           pe_id, set_index, value);

    // Seleccionar línea víctima usando la política de reemplazo
    CacheLine* victim = cache_select_victim(cache, set_index, pe_id);

    // Preparar la línea para la escritura
    victim->valid = 1;
    victim->tag = tag;
    victim->state = I;  // Temporalmente inválido antes del broadcast

    // Enviar BUS_RDX - el handler invalidará otras copias y traerá el dato
    bus_broadcast(cache->bus, BUS_RDX, addr, pe_id);

    // Escribir el nuevo valor y marcar como M
    victim->data[0] = value;
    victim->state = M;
    printf("[PE%d] WRITE completado -> valor=%.2f (estado M)\n", pe_id, value);
}

CacheLine* cache_select_victim(Cache* cache, int set_index, int pe_id) {
    CacheSet* set = &cache->sets[set_index];
    CacheLine* victim = NULL;

    // Política 0 (nueva): Si hay una línea inválida con el tag correcto, reutilizarla
    // Esto previene seleccionar una víctima diferente cuando ya tenemos el slot correcto
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].state == I) {
            victim = &set->lines[i];
            printf("[PE%d] Víctima: way %d (línea en estado I, reutilizando)\n", pe_id, i);
            return victim;
        }
    }
    
    // Política 1: Buscar primera línea inválida
    for (int i = 0; i < WAYS; i++) {
        if (!set->lines[i].valid) {
            victim = &set->lines[i];
            printf("[PE%d] Víctima: way %d (línea inválida)\n", pe_id, i);
            return victim;
        }
    }
    
    // Política 2: Si no hay líneas inválidas, usar way 0 (FIFO simple)
    victim = &set->lines[0];
    printf("[PE%d] Víctima: way 0 (política FIFO)\n", pe_id);
    
    // Si la víctima está en M, hacer writeback antes de reemplazar
    if (victim->state == M) {
        int victim_addr = (int)(victim->tag * SETS + set_index);
        printf("[PE%d] Evicting línea M (addr=%d), haciendo writeback\n", 
               pe_id, victim_addr);
        bus_broadcast(cache->bus, BUS_WB, victim_addr, pe_id);
    }
    
    return victim;
}

CacheLine* cache_get_line(Cache* cache, int addr) {
    int set_index = addr % SETS;
    unsigned long tag = addr / SETS;
    CacheSet* set = &cache->sets[set_index];

    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            return &set->lines[i];
        }
    }
    return NULL;
}

MESI_State cache_get_state(Cache* cache, int addr) {
    CacheLine* line = cache_get_line(cache, addr);
    return line ? line->state : I;
}

void cache_set_state(Cache* cache, int addr, MESI_State new_state) {
    CacheLine* line = cache_get_line(cache, addr);
    if (line) {
        line->state = new_state;
    }
}

double cache_get_data(Cache* cache, int addr) {
    CacheLine* line = cache_get_line(cache, addr);
    return line ? line->data[0] : 0.0;
}

void cache_set_data(Cache* cache, int addr, double data) {
    CacheLine* line = cache_get_line(cache, addr);
    if (line) {
        line->data[0] = data;
    }
}

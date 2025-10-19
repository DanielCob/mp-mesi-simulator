#include "cache.h"
#include "bus/bus.h"
#include <stdio.h>

void cache_init(Cache* cache) {
    cache->bus = NULL;
    
    // Inicializar mutex
    pthread_mutex_init(&cache->mutex, NULL);
    
    for (int i = 0; i < SETS; i++)
        for (int j = 0; j < WAYS; j++) {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].state = I;
        }
}

void cache_destroy(Cache* cache) {
    // Destruir mutex
    pthread_mutex_destroy(&cache->mutex);
}

double cache_read(Cache* cache, int addr, int pe_id) {
    // Extraer dirección base del bloque y offset
    int block_base = GET_BLOCK_BASE(addr);
    int offset = GET_BLOCK_OFFSET(addr);
    
    if (offset != 0) {
        printf("[PE%d] READ addr=%d → block_base=%d, offset=%d\n", 
               pe_id, addr, block_base, offset);
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    // El cache trabaja con direcciones de bloque (base)
    int set_index = block_base % SETS;
    unsigned long tag = block_base / SETS;
    CacheSet* set = &cache->sets[set_index];

    // Buscar hit en el cache
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            // HIT: estados M, E o S pueden servir la lectura directamente
            if (state == M || state == E || state == S) {
                double result = set->lines[i].data[offset];  // ← Usa el offset aquí
                printf("[PE%d] READ HIT en set %d (way %d, estado %c, offset %d) -> valor=%.2f\n", 
                       pe_id, set_index, i, "MESI"[state], offset, result);
                pthread_mutex_unlock(&cache->mutex);
                return result;
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

    // IMPORTANTE: Desbloquear cache local antes de usar el bus
    // para evitar deadlock (el bus necesitará lockear otras cachés)
    pthread_mutex_unlock(&cache->mutex);
    
    // Enviar BUS_RD - el handler actualizará el estado y datos (trae bloque completo)
    bus_broadcast(cache->bus, BUS_RD, block_base, pe_id);
    
    // Re-lockear para leer el resultado
    pthread_mutex_lock(&cache->mutex);
    
    double result = victim->data[offset];  // ← Usa el offset aquí
    printf("[PE%d] READ completado -> bloque traído, offset %d, valor=%.2f (estado %c)\n", 
           pe_id, offset, result, "MESI"[victim->state]);
    pthread_mutex_unlock(&cache->mutex);
    return result;
}

void cache_write(Cache* cache, int addr, double value, int pe_id) {
    // Extraer dirección base del bloque y offset
    int block_base = GET_BLOCK_BASE(addr);
    int offset = GET_BLOCK_OFFSET(addr);
    
    if (offset != 0) {
        printf("[PE%d] WRITE addr=%d → block_base=%d, offset=%d\n", 
               pe_id, addr, block_base, offset);
    }
    
    pthread_mutex_lock(&cache->mutex);
    
    // El cache trabaja con direcciones de bloque (base)
    int set_index = block_base % SETS;
    unsigned long tag = block_base / SETS;
    CacheSet* set = &cache->sets[set_index];

    // Buscar si la línea ya está en el cache
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            MESI_State state = set->lines[i].state;
            
            if (state == M) {
                // HIT en M: escribir directamente, ya tenemos permisos exclusivos
                printf("[PE%d] WRITE HIT en set %d (way %d, estado M, offset %d) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, offset, value);
                set->lines[i].data[offset] = value;  // ← Usa el offset aquí
                pthread_mutex_unlock(&cache->mutex);
                return;
            } 
            else if (state == E) {
                // HIT en E: escribir y cambiar a M
                printf("[PE%d] WRITE HIT en set %d (way %d, estado E->M, offset %d) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, offset, value);
                set->lines[i].data[offset] = value;  // ← Usa el offset aquí
                set->lines[i].state = M;
                pthread_mutex_unlock(&cache->mutex);
                return;
            } 
            else if (state == S) {
                // HIT en S: necesitamos invalidar otras copias con BUS_UPGR
                printf("[PE%d] WRITE HIT en set %d (way %d, estado S->M, offset %d, enviando BUS_UPGR) -> escribiendo %.2f\n", 
                       pe_id, set_index, i, offset, value);
                
                // Desbloquear antes de usar el bus
                pthread_mutex_unlock(&cache->mutex);
                bus_broadcast(cache->bus, BUS_UPGR, block_base, pe_id);
                
                // Re-lockear para actualizar
                pthread_mutex_lock(&cache->mutex);
                set->lines[i].data[offset] = value;  // ← Usa el offset aquí
                set->lines[i].state = M;
                pthread_mutex_unlock(&cache->mutex);
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

    // Desbloquear antes de usar el bus
    pthread_mutex_unlock(&cache->mutex);
    
    // Enviar BUS_RDX - el handler invalidará otras copias y traerá el bloque
    bus_broadcast(cache->bus, BUS_RDX, block_base, pe_id);

    // Re-lockear para escribir
    pthread_mutex_lock(&cache->mutex);
    
    // Escribir el nuevo valor en el offset correcto y marcar como M
    victim->data[offset] = value;  // ← Usa el offset aquí
    victim->state = M;
    printf("[PE%d] WRITE completado -> bloque traído, offset %d, valor=%.2f (estado M)\n", 
           pe_id, offset, value);
    
    pthread_mutex_unlock(&cache->mutex);
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
    pthread_mutex_lock(&cache->mutex);
    CacheLine* line = cache_get_line(cache, addr);
    MESI_State state = line ? line->state : I;
    pthread_mutex_unlock(&cache->mutex);
    return state;
}

void cache_set_state(Cache* cache, int addr, MESI_State new_state) {
    pthread_mutex_lock(&cache->mutex);
    CacheLine* line = cache_get_line(cache, addr);
    if (line) {
        line->state = new_state;
    }
    pthread_mutex_unlock(&cache->mutex);
}

// Obtener bloque completo (4 doubles) - Para handlers del bus
void cache_get_block(Cache* cache, int addr, double block[BLOCK_SIZE]) {
    pthread_mutex_lock(&cache->mutex);
    CacheLine* line = cache_get_line(cache, addr);
    if (line) {
        for (int i = 0; i < BLOCK_SIZE; i++) {
            block[i] = line->data[i];
        }
    } else {
        for (int i = 0; i < BLOCK_SIZE; i++) {
            block[i] = 0.0;
        }
    }
    pthread_mutex_unlock(&cache->mutex);
}

// Establecer bloque completo (4 doubles) - Para handlers del bus
void cache_set_block(Cache* cache, int addr, const double block[BLOCK_SIZE]) {
    pthread_mutex_lock(&cache->mutex);
    CacheLine* line = cache_get_line(cache, addr);
    if (line) {
        for (int i = 0; i < BLOCK_SIZE; i++) {
            line->data[i] = block[i];
        }
    }
    pthread_mutex_unlock(&cache->mutex);
}

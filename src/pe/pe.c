#include "pe.h"
#include <stdio.h>
#include <unistd.h>

void* pe_run(void* arg) {
    PE* pe = (PE*)arg;
    printf("[PE%d] Starting thread...\n", pe->id);
    
    // Escenario para demostrar diferentes transiciones MESI
    
    if (pe->id == 0) {
        // PE0: Escribe addr=0 (MISS -> M)
        printf("\n=== PE0: Escribir addr=0 (estado I->M) ===\n");
        cache_write(pe->cache, 0, 100.0, pe->id);
        sleep(1);
        
        // PE0: Lee addr=0 (HIT en M)
        printf("\n=== PE0: Leer addr=0 (HIT en M) ===\n");
        double val = cache_read(pe->cache, 0, pe->id);
        printf("[PE0] Valor leído: %.2f\n", val);
        sleep(1);
        
        // PE0: Escribe de nuevo addr=0 (HIT en M)
        printf("\n=== PE0: Escribir addr=0 nuevamente (HIT en M) ===\n");
        cache_write(pe->cache, 0, 200.0, pe->id);
        sleep(2);
        
        // PE0: Lee addr=100 que PE1 escribió (estado M->S en PE1, E en PE0)
        printf("\n=== PE0: Leer addr=100 (que PE1 modificó) ===\n");
        val = cache_read(pe->cache, 100, pe->id);
        printf("[PE0] Valor leído de PE1: %.2f\n", val);
    }
    else if (pe->id == 1) {
        sleep(2);
        
        // PE1: Lee addr=0 que PE0 modificó (M->S en PE0, S en PE1)
        printf("\n=== PE1: Leer addr=0 (que PE0 modificó, M->S) ===\n");
        double val = cache_read(pe->cache, 0, pe->id);
        printf("[PE1] Valor leído de PE0: %.2f\n", val);
        sleep(1);
        
        // PE1: Escribe addr=100 (MISS -> M)
        printf("\n=== PE1: Escribir addr=100 (estado I->M) ===\n");
        cache_write(pe->cache, 100, 500.0, pe->id);
        sleep(1);
        
        // PE1: Lee addr=0 otra vez (HIT en S)
        printf("\n=== PE1: Leer addr=0 otra vez (HIT en S) ===\n");
        val = cache_read(pe->cache, 0, pe->id);
        printf("[PE1] Valor leído: %.2f\n", val);
    }
    else if (pe->id == 2) {
        sleep(4);
        
        // PE2: Lee addr=0 (que PE0 y PE1 tienen en S) -> todos en S
        printf("\n=== PE2: Leer addr=0 (compartido con PE0 y PE1) ===\n");
        double val = cache_read(pe->cache, 0, pe->id);
        printf("[PE2] Valor leído (compartido): %.2f\n", val);
        sleep(1);
        
        // PE2: Escribe addr=0 (S->M, invalida PE0 y PE1 con BUS_UPGR)
        printf("\n=== PE2: Escribir addr=0 (S->M, BUS_UPGR invalida otros) ===\n");
        cache_write(pe->cache, 0, 999.0, pe->id);
    }
    else if (pe->id == 3) {
        sleep(1);
        
        // PE3: Lee addr=200 (nuevo, E)
        printf("\n=== PE3: Leer addr=200 (primera lectura, I->E) ===\n");
        double val = cache_read(pe->cache, 200, pe->id);
        printf("[PE3] Valor leído: %.2f\n", val);
        sleep(1);
        
        // PE3: Escribe addr=200 (E->M, sin broadcast)
        printf("\n=== PE3: Escribir addr=200 (E->M, sin broadcast) ===\n");
        cache_write(pe->cache, 200, 777.0, pe->id);
    }
    
    sleep(1);
    printf("[PE%d] Finished.\n", pe->id);
    return NULL;
}

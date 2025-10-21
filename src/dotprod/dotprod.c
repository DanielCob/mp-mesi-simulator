#include "dotprod.h"
#include "config.h"
#include <stdio.h>
#include <pthread.h>

void dotprod_init_data(Memory* mem) {
    pthread_mutex_lock(&mem->mutex);
    
    printf("\n[DotProd] Initializing test data...\n");
    
    // Vector A [0-15]: valores 1.0 a 16.0
    printf("[DotProd] Loading Vector A at addresses 0-15:\n  A = [");
    for (int i = 0; i < 16; i++) {
        mem->data[i] = (double)(i + 1);
        printf("%.0f", mem->data[i]);
        if (i < 15) printf(", ");
    }
    printf("]\n");
    
    // Vector B [100-115]: todos valores 1.0
    printf("[DotProd] Loading Vector B at addresses 100-115:\n  B = [");
    for (int i = 0; i < 16; i++) {
        mem->data[100 + i] = 1.0;
        printf("%.0f", mem->data[100 + i]);
        if (i < 15) printf(", ");
    }
    printf("]\n");
    
    // Inicializar área de resultados en 0
    // IMPORTANTE: Usar bloques separados para evitar race conditions en writeback
    // PE0→200, PE1→204, PE2→208, PE3→212, Final→216
    printf("[DotProd] Initializing result areas (separate cache blocks)\n");
    mem->data[200] = 0.0;  // PE0 partial (block 200-203)
    mem->data[204] = 0.0;  // PE1 partial (block 204-207)
    mem->data[208] = 0.0;  // PE2 partial (block 208-211)
    mem->data[212] = 0.0;  // PE3 partial (block 212-215)
    mem->data[216] = 0.0;  // Final result (block 216-219)
    
    // Flags de sincronización para barrier (uno por PE en bloques separados)
    // Cada PE escribe 1.0 cuando termina su cálculo individual
    // PE3 espera a que todos los flags sean 1.0 antes de hacer la suma final
    printf("[DotProd] Initializing synchronization flags in separate blocks\n");
    mem->data[220] = 0.0;  // Flag PE0 (block 220-223)
    mem->data[224] = 0.0;  // Flag PE1 (block 224-227) - bloque separado
    mem->data[228] = 0.0;  // Flag PE2 (block 228-231) - bloque separado
    mem->data[232] = -3.0; // Constante -3.0 para comparación (block 232-235)
    
    // Cálculo esperado
    double expected = 0.0;
    for (int i = 1; i <= 16; i++) {
        expected += i * 1.0;
    }
    printf("[DotProd] Expected result: %.2f\n", expected);
    printf("[DotProd] Data initialization complete\n\n");
    
    pthread_mutex_unlock(&mem->mutex);
}

double dotprod_get_result(Memory* mem) {
    pthread_mutex_lock(&mem->mutex);
    double result = mem->data[216];  // Nueva dirección para resultado final
    pthread_mutex_unlock(&mem->mutex);
    return result;
}

void dotprod_print_results(Memory* mem) {
    // Ya no necesitamos hacer flush manual, porque HALT ya hizo writeback
    // de todas las líneas modificadas a través del bus
    
    pthread_mutex_lock(&mem->mutex);
    
    printf("\n");
    printf("================================================================================\n");
    printf("                      DOT PRODUCT COMPUTATION RESULTS                           \n");
    printf("================================================================================\n");
    
    // Imprimir vectores de entrada
    printf("\nInput Vectors:\n");
    printf("  Vector A: [");
    for (int i = 0; i < 16; i++) {
        printf("%.0f", mem->data[i]);
        if (i < 15) printf(", ");
    }
    printf("]\n");
    
    printf("  Vector B: [");
    for (int i = 0; i < 16; i++) {
        printf("%.0f", mem->data[100 + i]);
        if (i < 15) printf(", ");
    }
    printf("]\n");
    
    // Imprimir resultados parciales
    printf("\nPartial Products (per PE):\n");
    printf("  PE0 (elements 0-3):   %.2f (addr 200)\n", mem->data[200]);
    printf("  PE1 (elements 4-7):   %.2f (addr 204)\n", mem->data[204]);
    printf("  PE2 (elements 8-11):  %.2f (addr 208)\n", mem->data[208]);
    printf("  PE3 (elements 12-15): %.2f (addr 212)\n", mem->data[212]);
    
    // Resultado final
    double final_result = mem->data[216];
    printf("\nFinal Dot Product: %.2f (addr 216)\n", final_result);
    
    // Verificación
    double expected = 0.0;
    for (int i = 1; i <= 16; i++) {
        expected += i * 1.0;
    }
    
    double error = final_result - expected;
    printf("\nVerification:\n");
    printf("  Expected: %.2f\n", expected);
    printf("  Computed: %.2f\n", final_result);
    printf("  Error:    %.10f\n", error);
    
    if (error < 0.0001 && error > -0.0001) {
        printf("  Status:   ✓ CORRECT\n");
    } else {
        printf("  Status:   ✗ INCORRECT\n");
    }
    
    printf("================================================================================\n\n");
    
    pthread_mutex_unlock(&mem->mutex);
}

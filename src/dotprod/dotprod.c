#include "dotprod.h"
#include "config.h"
#include <stdio.h>
#include <pthread.h>

void dotprod_init_data(Memory* mem) {
    pthread_mutex_lock(&mem->mutex);
    
    printf("\n[DotProd] Initializing test data...\n");
    printf("[DotProd] Configuration: VECTOR_SIZE=%d, NUM_PES=%d, SEGMENT_SIZE=%d\n", 
           VECTOR_SIZE, NUM_PES, SEGMENT_SIZE);
    
    // Vector A: valores 1.0 a VECTOR_SIZE
    printf("[DotProd] Loading Vector A at addresses %d-%d:\n  A = [", 
           VECTOR_A_BASE, VECTOR_A_BASE + VECTOR_SIZE - 1);
    for (int i = 0; i < VECTOR_SIZE; i++) {
        mem->data[VECTOR_A_BASE + i] = (double)(i + 1);
        printf("%.0f", mem->data[VECTOR_A_BASE + i]);
        if (i < VECTOR_SIZE - 1) printf(", ");
    }
    printf("]\n");
    
    // Vector B: todos valores 1.0
    printf("[DotProd] Loading Vector B at addresses %d-%d:\n  B = [",
           VECTOR_B_BASE, VECTOR_B_BASE + VECTOR_SIZE - 1);
    for (int i = 0; i < VECTOR_SIZE; i++) {
        mem->data[VECTOR_B_BASE + i] = 1.0;
        printf("%.0f", mem->data[VECTOR_B_BASE + i]);
        if (i < VECTOR_SIZE - 1) printf(", ");
    }
    printf("]\n");
    
    // Inicializar área de resultados parciales (bloques separados)
    printf("[DotProd] Initializing result areas (separate cache blocks)\n");
    for (int pe = 0; pe < NUM_PES; pe++) {
        mem->data[RESULTS_BASE + pe * BLOCK_SIZE] = 0.0;
        printf("  PE%d partial result → addr %d (block %d-%d)\n", 
               pe, RESULTS_BASE + pe * BLOCK_SIZE,
               RESULTS_BASE + pe * BLOCK_SIZE,
               RESULTS_BASE + pe * BLOCK_SIZE + BLOCK_SIZE - 1);
    }
    mem->data[FINAL_RESULT_ADDR] = 0.0;  // Final result
    printf("  Final result → addr %d\n", FINAL_RESULT_ADDR);
    
    // Flags de sincronización para barrier (bloques separados)
    printf("[DotProd] Initializing synchronization flags in separate blocks\n");
    for (int pe = 0; pe < NUM_PES - 1; pe++) {  // Solo PE0-PE2 necesitan flags
        mem->data[FLAGS_BASE + pe * BLOCK_SIZE] = 0.0;
        printf("  PE%d flag → addr %d\n", pe, FLAGS_BASE + pe * BLOCK_SIZE);
    }
    mem->data[CONSTANTS_BASE] = -(double)(NUM_PES - 1);  // Constante para barrier
    printf("  Barrier constant (-%.0f) → addr %d\n", (double)(NUM_PES - 1), CONSTANTS_BASE);
    
    // Cálculo esperado
    double expected = 0.0;
    for (int i = 1; i <= VECTOR_SIZE; i++) {
        expected += i * 1.0;
    }
    printf("[DotProd] Expected result: %.2f\n", expected);
    printf("[DotProd] Data initialization complete\n\n");
    
    pthread_mutex_unlock(&mem->mutex);
}

double dotprod_get_result(Memory* mem) {
    pthread_mutex_lock(&mem->mutex);
    double result = mem->data[FINAL_RESULT_ADDR];
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
    for (int i = 0; i < VECTOR_SIZE; i++) {
        printf("%.0f", mem->data[VECTOR_A_BASE + i]);
        if (i < VECTOR_SIZE - 1) printf(", ");
    }
    printf("]\n");
    
    printf("  Vector B: [");
    for (int i = 0; i < VECTOR_SIZE; i++) {
        printf("%.0f", mem->data[VECTOR_B_BASE + i]);
        if (i < VECTOR_SIZE - 1) printf(", ");
    }
    printf("]\n");
    
    // Imprimir resultados parciales
    printf("\nPartial Products (per PE):\n");
    for (int pe = 0; pe < NUM_PES; pe++) {
        int start_elem = pe * SEGMENT_SIZE;
        int end_elem = start_elem + SEGMENT_SIZE - 1;
        int addr = RESULTS_BASE + pe * BLOCK_SIZE;
        printf("  PE%d (elements %d-%d):   %.2f (addr %d)\n", 
               pe, start_elem, end_elem, mem->data[addr], addr);
    }
    
    // Resultado final
    double final_result = mem->data[FINAL_RESULT_ADDR];
    printf("\nFinal Dot Product: %.2f (addr %d)\n", final_result, FINAL_RESULT_ADDR);
    
    // Verificación
    double expected = 0.0;
    for (int i = 1; i <= VECTOR_SIZE; i++) {
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

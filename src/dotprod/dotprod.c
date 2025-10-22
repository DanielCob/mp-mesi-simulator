#include "dotprod.h"
#include "config.h"
#include <stdio.h>
#include <pthread.h>

void dotprod_init_data(Memory* mem) {
    pthread_mutex_lock(&mem->mutex);
    
    printf("\n[DotProd] Initializing test data...\n");
    
    // ========================================================================
    // SHARED_CONFIG: Configuración compartida que los PEs leen al arranque
    // ========================================================================
    printf("[DotProd] Initializing SHARED_CONFIG area (addresses %d-%d)\n", 
           SHARED_CONFIG_ADDR, CFG_PE_START_ADDR + NUM_PES * CFG_PARAMS_PER_PE - 1);
    
    // Configuración global del sistema
    mem->data[CFG_VECTOR_A_ADDR] = (double)VECTOR_A_ADDR;
    mem->data[CFG_VECTOR_B_ADDR] = (double)VECTOR_B_ADDR;
    mem->data[CFG_RESULTS_ADDR] = (double)RESULTS_ADDR;
    mem->data[CFG_FLAGS_ADDR] = (double)FLAGS_ADDR;
    mem->data[CFG_FINAL_RESULT_ADDR] = (double)FINAL_RESULT_ADDR;
    mem->data[CFG_NUM_PES_ADDR] = (double)NUM_PES;
    mem->data[CFG_BARRIER_CHECK_ADDR] = -(double)(NUM_PES - 1);
    
    printf("  Global config:\n");
    printf("    VECTOR_A_ADDR=%d, VECTOR_B_ADDR=%d\n", VECTOR_A_ADDR, VECTOR_B_ADDR);
    printf("    RESULTS_ADDR=%d, FLAGS_ADDR=%d, FINAL_RESULT=%d\n", 
           RESULTS_ADDR, FLAGS_ADDR, FINAL_RESULT_ADDR);
    printf("    NUM_PES=%d, BARRIER_CHECK=%.0f\n", NUM_PES, mem->data[CFG_BARRIER_CHECK_ADDR]);
    
    // Configuración específica por PE (start_index, segment_size)
    printf("  Per-PE config:\n");
    for (int pe = 0; pe < NUM_PES; pe++) {
        int start_idx = pe * SEGMENT_SIZE_WORKER;
        int segment_size = (pe == NUM_PES - 1) ? SEGMENT_SIZE_MASTER : SEGMENT_SIZE_WORKER;
        
        mem->data[CFG_PE(pe, PE_START_INDEX)] = (double)start_idx;
        mem->data[CFG_PE(pe, PE_SEGMENT_SIZE)] = (double)segment_size;
        
        printf("    PE%d: start=%d, size=%d (addr %d-%d)\n",
               pe, start_idx, segment_size, 
               CFG_PE(pe, 0), CFG_PE(pe, 1));
    }
    
    // ========================================================================
    // Inicializar área de resultados parciales (1 bloque: 4 valores)
    // ========================================================================
    printf("[DotProd] Initializing results area (1 cache block at addr %d)\n", RESULTS_ADDR);
    for (int pe = 0; pe < NUM_PES; pe++) {
        mem->data[RESULTS_ADDR + pe] = 0.0;
        printf("  PE%d result → addr %d\n", pe, RESULTS_ADDR + pe);
    }
    
    // ========================================================================
    // Inicializar flags de sincronización (1 bloque: primeros 3 valores)
    // ========================================================================
    printf("[DotProd] Initializing sync flags (1 cache block at addr %d)\n", FLAGS_ADDR);
    for (int pe = 0; pe < NUM_PES - 1; pe++) {  // Solo PE0-PE2 necesitan flags
        mem->data[FLAGS_ADDR + pe] = 0.0;
        printf("  PE%d flag → addr %d\n", pe, FLAGS_ADDR + pe);
    }
    
    // Resultado final
    mem->data[FINAL_RESULT_ADDR] = 0.0;
    printf("[DotProd] Final result → addr %d\n", FINAL_RESULT_ADDR);
    
    // ========================================================================
    // Vectores de datos (al final de memoria)
    // ========================================================================
    // Vector A: valores 1.0 a VECTOR_SIZE
    printf("[DotProd] Loading Vector A at addresses %d-%d:\n  A = [", 
           VECTOR_A_ADDR, VECTOR_A_ADDR + VECTOR_SIZE - 1);
    for (int i = 0; i < VECTOR_SIZE; i++) {
        mem->data[VECTOR_A_ADDR + i] = (double)(i + 1);
        printf("%.0f", mem->data[VECTOR_A_ADDR + i]);
        if (i < VECTOR_SIZE - 1) printf(", ");
    }
    printf("]\n");
    
    // Vector B: todos valores 1.0
    printf("[DotProd] Loading Vector B at addresses %d-%d:\n  B = [",
           VECTOR_B_ADDR, VECTOR_B_ADDR + VECTOR_SIZE - 1);
    for (int i = 0; i < VECTOR_SIZE; i++) {
        mem->data[VECTOR_B_ADDR + i] = 1.0;
        printf("%.0f", mem->data[VECTOR_B_ADDR + i]);
        if (i < VECTOR_SIZE - 1) printf(", ");
    }
    printf("]\n");
    
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
        printf("%.0f", mem->data[VECTOR_A_ADDR + i]);
        if (i < VECTOR_SIZE - 1) printf(", ");
    }
    printf("]\n");
    
    printf("  Vector B: [");
    for (int i = 0; i < VECTOR_SIZE; i++) {
        printf("%.0f", mem->data[VECTOR_B_ADDR + i]);
        if (i < VECTOR_SIZE - 1) printf(", ");
    }
    printf("]\n");
    
    // Imprimir resultados parciales
    printf("\nPartial Products (per PE):\n");
    for (int pe = 0; pe < NUM_PES; pe++) {
        int start_elem = pe * SEGMENT_SIZE_WORKER;
        int end_elem = start_elem + SEGMENT_SIZE_WORKER - 1;
        int addr = RESULTS_ADDR + pe;  // Compactos: direcciones consecutivas
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

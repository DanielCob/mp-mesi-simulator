#include "dotprod.h"
#include "vector_loader.h"
#include "config.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

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
    printf("[DotProd] Vector Memory Layout:\n");
    printf("  Aligned start position: %d\n", VECTORS_START_ALIGNED);
    printf("  BLOCK_SIZE: %d doubles (%zu bytes)\n", BLOCK_SIZE, BLOCK_SIZE * sizeof(double));
    printf("  Misalignment offset: %d doubles\n", MISALIGNMENT_OFFSET);
    
    printf("  Vector A: addr %d (aligned: %s, offset=%d)\n", 
           VECTOR_A_ADDR, IS_ALIGNED(VECTOR_A_ADDR) ? "YES" : "NO", 
           GET_BLOCK_OFFSET(VECTOR_A_ADDR));
    printf("  Vector B: addr %d (aligned: %s, offset=%d)\n", 
           VECTOR_B_ADDR, IS_ALIGNED(VECTOR_B_ADDR) ? "YES" : "NO",
           GET_BLOCK_OFFSET(VECTOR_B_ADDR));
    
    // Calcular cuántos bloques de caché ocuparán los vectores
    int blocks_a = (VECTOR_SIZE + GET_BLOCK_OFFSET(VECTOR_A_ADDR) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int blocks_b = (VECTOR_SIZE + GET_BLOCK_OFFSET(VECTOR_B_ADDR) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int blocks_aligned = (VECTOR_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    printf("  Cache blocks needed:\n");
    printf("    Vector A: %d blocks (aligned would use %d)\n", blocks_a, blocks_aligned);
    printf("    Vector B: %d blocks (aligned would use %d)\n", blocks_b, blocks_aligned);
    if (blocks_a > blocks_aligned || blocks_b > blocks_aligned) {
        printf("    ⚠ Misalignment causes extra cache block usage!\n");
    }
    
    // ========================================================================
    // Cargar vectores desde archivos CSV
    // ========================================================================
    printf("\n[DotProd] Loading vectors from CSV files...\n");
    
    // Buffers temporales para cargar los vectores
    double* vec_a_buffer = (double*)malloc(VECTOR_SIZE * sizeof(double));
    double* vec_b_buffer = (double*)malloc(VECTOR_SIZE * sizeof(double));
    
    if (!vec_a_buffer || !vec_b_buffer) {
        fprintf(stderr, "[DotProd] ERROR: No se pudo asignar memoria para buffers\n");
        pthread_mutex_unlock(&mem->mutex);
        return;
    }
    
    // Cargar Vector A
    VectorLoadResult result_a = load_vector_from_csv(VECTOR_A_FILE, vec_a_buffer, VECTOR_SIZE);
    if (!result_a.success) {
        fprintf(stderr, "[DotProd] ERROR cargando Vector A: %s\n", result_a.error_message);
        fprintf(stderr, "[DotProd] El programa no puede continuar sin vectores válidos\n");
        free(vec_a_buffer);
        free(vec_b_buffer);
        pthread_mutex_unlock(&mem->mutex);
        return;
    }
    
    printf("[DotProd] ✓ Vector A cargado desde '%s' (%d valores)\n", 
           VECTOR_A_FILE, result_a.values_read);
    
    // Si se leyeron menos valores de los necesarios, rellenar con ceros
    if (result_a.values_read < VECTOR_SIZE) {
        printf("[DotProd] ⚠ Solo se leyeron %d/%d valores, rellenando con 0.0\n",
               result_a.values_read, VECTOR_SIZE);
        for (int i = result_a.values_read; i < VECTOR_SIZE; i++) {
            vec_a_buffer[i] = 0.0;
        }
    }
    
    // Cargar Vector B
    VectorLoadResult result_b = load_vector_from_csv(VECTOR_B_FILE, vec_b_buffer, VECTOR_SIZE);
    if (!result_b.success) {
        fprintf(stderr, "[DotProd] ERROR cargando Vector B: %s\n", result_b.error_message);
        fprintf(stderr, "[DotProd] El programa no puede continuar sin vectores válidos\n");
        free(vec_a_buffer);
        free(vec_b_buffer);
        pthread_mutex_unlock(&mem->mutex);
        return;
    }
    
    printf("[DotProd] ✓ Vector B cargado desde '%s' (%d valores)\n", 
           VECTOR_B_FILE, result_b.values_read);
    
    // Si se leyeron menos valores de los necesarios, rellenar con ceros
    if (result_b.values_read < VECTOR_SIZE) {
        printf("[DotProd] ⚠ Solo se leyeron %d/%d valores, rellenando con 0.0\n",
               result_b.values_read, VECTOR_SIZE);
        for (int i = result_b.values_read; i < VECTOR_SIZE; i++) {
            vec_b_buffer[i] = 0.0;
        }
    }
    
    // Copiar vectores a memoria y mostrarlos
    print_vector("Vector A", vec_a_buffer, VECTOR_SIZE, VECTOR_A_ADDR);
    for (int i = 0; i < VECTOR_SIZE; i++) {
        mem->data[VECTOR_A_ADDR + i] = vec_a_buffer[i];
    }
    
    print_vector("Vector B", vec_b_buffer, VECTOR_SIZE, VECTOR_B_ADDR);
    for (int i = 0; i < VECTOR_SIZE; i++) {
        mem->data[VECTOR_B_ADDR + i] = vec_b_buffer[i];
    }
    
    // Liberar buffers temporales
    free(vec_a_buffer);
    free(vec_b_buffer);
    
    // Cálculo esperado (producto punto real de los vectores cargados)
    double expected = 0.0;
    for (int i = 0; i < VECTOR_SIZE; i++) {
        expected += mem->data[VECTOR_A_ADDR + i] * mem->data[VECTOR_B_ADDR + i];
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
    
    // Verificación (calcular producto punto esperado de los vectores reales)
    double expected = 0.0;
    for (int i = 0; i < VECTOR_SIZE; i++) {
        expected += mem->data[VECTOR_A_ADDR + i] * mem->data[VECTOR_B_ADDR + i];
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

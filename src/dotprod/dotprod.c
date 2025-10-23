#define LOG_MODULE "DOTPROD"
#include "dotprod.h"
#include "vector_loader.h"
#include "config.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "log.h"

void dotprod_init_data(Memory* mem) {
    pthread_mutex_lock(&mem->mutex);
    
        printf("\n[DotProd] Initializing sample data\n");
    
    // ========================================================================
    // SHARED_CONFIG: Shared configuration read by PEs at startup
    // ========================================================================
        printf("[DotProd] Initializing SHARED_CONFIG area (addresses 0x%X-0x%X)\n", 
        SHARED_CONFIG_ADDR, CFG_PE_START_ADDR + NUM_PES * CFG_PARAMS_PER_PE - 1);
    
    // Global system configuration
    mem->data[CFG_VECTOR_A_ADDR] = (double)VECTOR_A_ADDR;
    mem->data[CFG_VECTOR_B_ADDR] = (double)VECTOR_B_ADDR;
    mem->data[CFG_RESULTS_ADDR] = (double)RESULTS_ADDR;
    mem->data[CFG_FLAGS_ADDR] = (double)FLAGS_ADDR;
    mem->data[CFG_FINAL_RESULT_ADDR] = (double)FINAL_RESULT_ADDR;
    mem->data[CFG_NUM_PES_ADDR] = (double)NUM_PES;
    mem->data[CFG_BARRIER_CHECK_ADDR] = -(double)(NUM_PES - 1);
    
        printf("  Global configuration:\n");
    printf("    VECTOR_A_ADDR=0x%X, VECTOR_B_ADDR=0x%X\n", VECTOR_A_ADDR, VECTOR_B_ADDR);
    printf("    RESULTS_ADDR=0x%X, FLAGS_ADDR=0x%X, FINAL_RESULT=0x%X\n", 
           RESULTS_ADDR, FLAGS_ADDR, FINAL_RESULT_ADDR);
    printf("    NUM_PES=%d, BARRIER_CHECK=%.0f\n", NUM_PES, mem->data[CFG_BARRIER_CHECK_ADDR]);
    
    // Per-PE configuration (start_index, segment_size)
        printf("  Per-PE configuration:\n");
    for (int pe = 0; pe < NUM_PES; pe++) {
        int start_idx = pe * SEGMENT_SIZE_WORKER;
        int segment_size = (pe == NUM_PES - 1) ? SEGMENT_SIZE_MASTER : SEGMENT_SIZE_WORKER;
        
        mem->data[CFG_PE(pe, PE_START_INDEX)] = (double)start_idx;
        mem->data[CFG_PE(pe, PE_SEGMENT_SIZE)] = (double)segment_size;
        
    printf("    PE%d: start=%d, size=%d (addr 0x%X-0x%X)\n",
               pe, start_idx, segment_size, 
               CFG_PE(pe, 0), CFG_PE(pe, 1));
    }
    
    // ========================================================================
    // Initialize partial results area (1 block: 4 values)
    // ========================================================================
        printf("[DotProd] Initializing results area (1 cache block at addr 0x%X)\n", RESULTS_ADDR);
    for (int pe = 0; pe < NUM_PES; pe++) {
        mem->data[RESULTS_ADDR + pe] = 0.0;
            printf("  PE%d result -> addr 0x%X\n", pe, RESULTS_ADDR + pe);
    }
    
    // ========================================================================
    // Initialize synchronization flags (1 block: first 3 values)
    // ========================================================================
        printf("[DotProd] Initializing synchronization flags (1 cache block at addr 0x%X)\n", FLAGS_ADDR);
    for (int pe = 0; pe < NUM_PES - 1; pe++) {  // Solo PE0-PE2 necesitan flags
        mem->data[FLAGS_ADDR + pe] = 0.0;
            printf("  PE%d flag -> addr 0x%X\n", pe, FLAGS_ADDR + pe);
    }
    
    // Final result
    mem->data[FINAL_RESULT_ADDR] = 0.0;
    printf("[DotProd] Final result -> addr 0x%X\n", FINAL_RESULT_ADDR);
    
    // ========================================================================
    // Data vectors (at the end of memory)
    // ========================================================================
    printf("[DotProd] Vector memory layout:\n");
    printf("  Start aligned: 0x%X\n", VECTORS_START_ALIGNED);
    printf("  BLOCK_SIZE: %d doubles (%zu bytes)\n", BLOCK_SIZE, BLOCK_SIZE * sizeof(double));
    printf("  Misalignment: %d doubles\n", MISALIGNMENT_OFFSET);
    
    printf("  Vector A: addr 0x%X (aligned: %s, offset=%d)\n", 
        VECTOR_A_ADDR, IS_ALIGNED(VECTOR_A_ADDR) ? "yes" : "no", 
           GET_BLOCK_OFFSET(VECTOR_A_ADDR));
    printf("  Vector B: addr 0x%X (aligned: %s, offset=%d)\n", 
        VECTOR_B_ADDR, IS_ALIGNED(VECTOR_B_ADDR) ? "yes" : "no",
           GET_BLOCK_OFFSET(VECTOR_B_ADDR));
    
    // Calculate how many cache blocks are needed
    int blocks_a = (VECTOR_SIZE + GET_BLOCK_OFFSET(VECTOR_A_ADDR) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int blocks_b = (VECTOR_SIZE + GET_BLOCK_OFFSET(VECTOR_B_ADDR) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int blocks_aligned = (VECTOR_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    printf("  Required cache blocks:\n");
    printf("    Vector A: %d blocks (aligned would use %d)\n", blocks_a, blocks_aligned);
    printf("    Vector B: %d blocks (aligned would use %d)\n", blocks_b, blocks_aligned);
    if (blocks_a > blocks_aligned || blocks_b > blocks_aligned) {
        printf("    Warning: misalignment causes extra cache blocks\n");
    }
    
    // ========================================================================
    // Cargar vectores desde archivos CSV
    // ========================================================================
        printf("\n[DotProd] Loading vectors from CSV files\n");
    
    // Temporary buffers to load vectors
    double* vec_a_buffer = (double*)malloc(VECTOR_SIZE * sizeof(double));
    double* vec_b_buffer = (double*)malloc(VECTOR_SIZE * sizeof(double));
    
    if (!vec_a_buffer || !vec_b_buffer) {
        LOGE("Could not allocate memory for vector buffers");
        pthread_mutex_unlock(&mem->mutex);
        return;
    }
    
    // Cargar Vector A
    VectorLoadResult result_a = load_vector_from_csv(VECTOR_A_FILE, vec_a_buffer, VECTOR_SIZE);
    if (!result_a.success) {
        LOGE("Error loading vector A: %s", result_a.error_message);
        LOGE("Program cannot continue without valid vectors");
        free(vec_a_buffer);
        free(vec_b_buffer);
        pthread_mutex_unlock(&mem->mutex);
        return;
    }
    
    printf("[DotProd] Vector A loaded from '%s' (%d values)\n", 
        VECTOR_A_FILE, result_a.values_read);
    
    // If fewer values were read than needed, fill with zeros
    if (result_a.values_read < VECTOR_SIZE) {
        printf("[DotProd] Only %d/%d values read, filling with 0.0\n",
               result_a.values_read, VECTOR_SIZE);
        for (int i = result_a.values_read; i < VECTOR_SIZE; i++) {
            vec_a_buffer[i] = 0.0;
        }
    }
    
    // Cargar Vector B
    VectorLoadResult result_b = load_vector_from_csv(VECTOR_B_FILE, vec_b_buffer, VECTOR_SIZE);
    if (!result_b.success) {
        LOGE("Error loading vector B: %s", result_b.error_message);
        LOGE("Program cannot continue without valid vectors");
        free(vec_a_buffer);
        free(vec_b_buffer);
        pthread_mutex_unlock(&mem->mutex);
        return;
    }
    
    printf("[DotProd] Vector B loaded from '%s' (%d values)\n", 
        VECTOR_B_FILE, result_b.values_read);
    
    // If fewer values were read than needed, fill with zeros
    if (result_b.values_read < VECTOR_SIZE) {
        printf("[DotProd] Only %d/%d values read, filling with 0.0\n",
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
    
    // Free temporary buffers
    free(vec_a_buffer);
    free(vec_b_buffer);
    
    // Expected calculation (true dot product of loaded vectors)
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
    // No need for a manual flush; HALT already wrote back all modified lines via the bus
    
    pthread_mutex_lock(&mem->mutex);
    
    printf("\n[Dot product results]\n");
    
    // Print input vectors
    printf("\nInput vectors:\n");
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
    
    // Print partial products
    printf("\nPartial products (per PE):\n");
    for (int pe = 0; pe < NUM_PES; pe++) {
        int start_elem = pe * SEGMENT_SIZE_WORKER;
        int end_elem = start_elem + SEGMENT_SIZE_WORKER - 1;
    int addr = RESULTS_ADDR + pe;  // Compact: consecutive addresses
    printf("  PE%d (elements %d-%d):   %.2f (addr 0x%X)\n", 
               pe, start_elem, end_elem, mem->data[addr], addr);
    }
    
    // Final result
    double final_result = mem->data[FINAL_RESULT_ADDR];
    printf("\nFinal dot product: %.2f (addr 0x%X)\n", final_result, FINAL_RESULT_ADDR);
    
    // Verification (compute expected dot product from the loaded vectors)
    double expected = 0.0;
    for (int i = 0; i < VECTOR_SIZE; i++) {
        expected += mem->data[VECTOR_A_ADDR + i] * mem->data[VECTOR_B_ADDR + i];
    }
    
    double error = final_result - expected;
    printf("\nVerification:\n");
    printf("  Expected: %.2f\n", expected);
    printf("  Calculated: %.2f\n", final_result);
    printf("  Error: %.10f\n", error);
    
    if (error < 0.0001 && error > -0.0001) {
        printf("  Status: CORRECT\n");
    } else {
        printf("  Status: INCORRECT\n");
    }
    
    printf("\n");
    
    pthread_mutex_unlock(&mem->mutex);
}

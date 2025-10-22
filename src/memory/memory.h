#ifndef MEMORY_H
#define MEMORY_H

#include "config.h"
#include "memory_stats.h"
#include <pthread.h>
#include <stdbool.h>

// MEMORY OPERATION TYPES

typedef enum { 
    MEM_OP_READ_BLOCK,   // Read a full block (BLOCK_SIZE doubles)
    MEM_OP_WRITE_BLOCK   // Write a full block (BLOCK_SIZE doubles)
} MemOp;

// ESTRUCTURAS

/**
 * Memory request
 * Encapsulates a block read/write operation
 */
typedef struct {
    MemOp op;
    int addr;                     // Block base address (must be aligned)
    double block[BLOCK_SIZE];     // Data buffer for the block
    int pe_id;                    // Requesting PE id
    bool processed;               // Processing flag
    pthread_cond_t done;          // Synchronization condition
} MemRequest;

/**
 * Shared main memory
 * Thread-safe with a dedicated thread to process requests
 */
typedef struct {
    double data[MEM_SIZE];            // Data array
    pthread_mutex_t mutex;            // Synchronization mutex
    pthread_cond_t request_ready;     // Pending request signal
    MemRequest current_request;       // Current request
    bool has_request;                 // Pending request flag
    bool running;                     // Memory thread running flag
    MemoryStats stats;                // Access statistics
} Memory;

// PUBLIC API

// Init and cleanup
void mem_init(Memory* mem);
void mem_destroy(Memory* mem);

// Block operations (used by the bus)
void mem_read_block(Memory* mem, int addr, double block[BLOCK_SIZE], int pe_id);
void mem_write_block(Memory* mem, int addr, const double block[BLOCK_SIZE], int pe_id);

// Memory thread
void* mem_thread_func(void* arg);

#endif

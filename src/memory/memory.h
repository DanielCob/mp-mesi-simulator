#ifndef MEMORY_H
#define MEMORY_H

#include "include/config.h"
#include <pthread.h>
#include <stdbool.h>

// Tipos de operaciones de memoria
typedef enum { 
    MEM_OP_READ_BLOCK,  // Lee un bloque completo (BLOCK_SIZE doubles)
    MEM_OP_WRITE_BLOCK  // Escribe un bloque completo (BLOCK_SIZE doubles)
} MemOp;

// Estructura de solicitud de memoria
typedef struct {
    MemOp op;
    int addr;                    // Dirección base del bloque
    double block[BLOCK_SIZE];    // Para operaciones de bloque
    bool processed;
    pthread_cond_t done;
} MemRequest;

// Estructura de la memoria
typedef struct {
    double data[MEM_SIZE];
    pthread_mutex_t mutex;
    pthread_cond_t request_ready;
    MemRequest current_request;
    bool has_request;
    bool running;
} Memory;

// Funciones públicas
void mem_init(Memory* mem);
void mem_destroy(Memory* mem);

// Funciones de bloque completo (para uso del bus)
void mem_read_block(Memory* mem, int addr, double block[BLOCK_SIZE]);
void mem_write_block(Memory* mem, int addr, const double block[BLOCK_SIZE]);

void* mem_thread_func(void* arg);  // Función del thread de memoria

#endif

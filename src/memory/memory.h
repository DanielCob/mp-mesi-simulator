#ifndef MEMORY_H
#define MEMORY_H

#include "include/config.h"
#include <pthread.h>
#include <stdbool.h>

// Tipos de operaciones de memoria
typedef enum { MEM_OP_READ, MEM_OP_WRITE } MemOp;

// Estructura de solicitud de memoria
typedef struct {
    MemOp op;
    int addr;
    double value;           // Para WRITE
    double result;          // Para READ
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

// Funciones que requieren alineamiento (addr debe ser múltiplo de BLOCK_SIZE)
double mem_read(Memory* mem, int addr);   // Requiere IS_ALIGNED(addr)
void mem_write(Memory* mem, int addr, double value);  // Requiere IS_ALIGNED(addr)

void* mem_thread_func(void* arg);  // Función del thread de memoria

#endif

#ifndef MEMORY_H
#define MEMORY_H

#include "config.h"
#include "memory_stats.h"
#include <pthread.h>
#include <stdbool.h>

// TIPOS DE OPERACIONES DE MEMORIA

typedef enum { 
    MEM_OP_READ_BLOCK,   // Lee un bloque completo (BLOCK_SIZE doubles)
    MEM_OP_WRITE_BLOCK   // Escribe un bloque completo (BLOCK_SIZE doubles)
} MemOp;

// ESTRUCTURAS

/**
 * Solicitud de memoria
 * Encapsula una operación de lectura/escritura de bloque
 */
typedef struct {
    MemOp op;
    int addr;                     // Dirección base del bloque (debe estar alineada)
    double block[BLOCK_SIZE];     // Buffer de datos del bloque
    int pe_id;                    // ID del PE que hace la solicitud
    bool processed;               // Flag de procesamiento
    pthread_cond_t done;          // Condición para sincronización
} MemRequest;

/**
 * Memoria principal compartida
 * Thread-safe con un thread dedicado para procesar solicitudes
 */
typedef struct {
    double data[MEM_SIZE];            // Arreglo de datos
    pthread_mutex_t mutex;            // Mutex para sincronización
    pthread_cond_t request_ready;     // Señal de solicitud pendiente
    MemRequest current_request;       // Solicitud actual
    bool has_request;                 // Flag de solicitud pendiente
    bool running;                     // Flag de ejecución del thread
    MemoryStats stats;                // Estadísticas de accesos
} Memory;

// FUNCIONES PÚBLICAS

// Inicialización y limpieza
void mem_init(Memory* mem);
void mem_destroy(Memory* mem);

// Operaciones de bloque (usadas por el bus)
void mem_read_block(Memory* mem, int addr, double block[BLOCK_SIZE], int pe_id);
void mem_write_block(Memory* mem, int addr, const double block[BLOCK_SIZE], int pe_id);

// Thread de memoria
void* mem_thread_func(void* arg);

#endif

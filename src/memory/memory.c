#include "memory.h"
#include <stdio.h>

void mem_init(Memory* mem) {
    // Inicializar datos
    for (int i = 0; i < MEM_SIZE; i++) {
        mem->data[i] = 0.0;
    }
    
    // Inicializar sincronización
    pthread_mutex_init(&mem->mutex, NULL);
    pthread_cond_init(&mem->request_ready, NULL);
    pthread_cond_init(&mem->current_request.done, NULL);
    
    mem->has_request = false;
    mem->running = true;
    mem->current_request.processed = false;
    
    printf("[MEMORY] Initialized.\n");
}

void mem_destroy(Memory* mem) {
    mem->running = false;
    pthread_cond_broadcast(&mem->request_ready);  // Despertar thread de memoria
    pthread_mutex_destroy(&mem->mutex);
    pthread_cond_destroy(&mem->request_ready);
    pthread_cond_destroy(&mem->current_request.done);
}

void mem_read_block(Memory* mem, int addr, double block[BLOCK_SIZE]) {
    // Verificar alineamiento
    if (!IS_ALIGNED(addr)) {
        fprintf(stderr, "[MEMORY ERROR] Read block address %d is not aligned\n", addr);
        addr = ALIGN_DOWN(addr);
    }
    
    pthread_mutex_lock(&mem->mutex);
    
    // Esperar si hay otra solicitud en proceso
    while (mem->has_request) {
        pthread_cond_wait(&mem->current_request.done, &mem->mutex);
    }
    
    // Preparar solicitud de lectura de bloque
    mem->current_request.op = MEM_OP_READ_BLOCK;
    mem->current_request.addr = addr;
    mem->current_request.processed = false;
    mem->has_request = true;
    
    // Señalizar al thread de memoria
    pthread_cond_signal(&mem->request_ready);
    
    // Esperar a que la memoria procese la solicitud
    while (!mem->current_request.processed) {
        pthread_cond_wait(&mem->current_request.done, &mem->mutex);
    }
    
    // Copiar resultado
    for (int i = 0; i < BLOCK_SIZE; i++) {
        block[i] = mem->current_request.block[i];
    }
    
    pthread_mutex_unlock(&mem->mutex);
}

void mem_write_block(Memory* mem, int addr, const double block[BLOCK_SIZE]) {
    // Verificar alineamiento
    if (!IS_ALIGNED(addr)) {
        fprintf(stderr, "[MEMORY ERROR] Write block address %d is not aligned\n", addr);
        addr = ALIGN_DOWN(addr);
    }
    
    pthread_mutex_lock(&mem->mutex);
    
    // Esperar si hay otra solicitud en proceso
    while (mem->has_request) {
        pthread_cond_wait(&mem->current_request.done, &mem->mutex);
    }
    
    // Preparar solicitud de escritura de bloque
    mem->current_request.op = MEM_OP_WRITE_BLOCK;
    mem->current_request.addr = addr;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        mem->current_request.block[i] = block[i];
    }
    mem->current_request.processed = false;
    mem->has_request = true;
    
    // Señalizar al thread de memoria
    pthread_cond_signal(&mem->request_ready);
    
    // Esperar a que la memoria procese la solicitud
    while (!mem->current_request.processed) {
        pthread_cond_wait(&mem->current_request.done, &mem->mutex);
    }
    
    pthread_mutex_unlock(&mem->mutex);
}

void* mem_thread_func(void* arg) {
    Memory* mem = (Memory*)arg;
    printf("[MEMORY] Thread iniciado.\n");
    
    while (mem->running) {
        pthread_mutex_lock(&mem->mutex);
        
        // Esperar por solicitudes
        while (!mem->has_request && mem->running) {
            pthread_cond_wait(&mem->request_ready, &mem->mutex);
        }
        
        if (!mem->running) {
            pthread_mutex_unlock(&mem->mutex);
            break;
        }
        
        // Procesar la solicitud
        MemRequest* req = &mem->current_request;
        
        pthread_mutex_unlock(&mem->mutex);
        
        // Ejecutar operación (fuera del lock)
        if (req->op == MEM_OP_READ_BLOCK) {
            printf("[MEMORY] READ_BLOCK addr=%d (reading %d doubles)\n", req->addr, BLOCK_SIZE);
            for (int i = 0; i < BLOCK_SIZE; i++) {
                req->block[i] = mem->data[req->addr + i];
            }
        } else if (req->op == MEM_OP_WRITE_BLOCK) {
            printf("[MEMORY] WRITE_BLOCK addr=%d (writing %d doubles)\n", req->addr, BLOCK_SIZE);
            for (int i = 0; i < BLOCK_SIZE; i++) {
                mem->data[req->addr + i] = req->block[i];
            }
        }
        
        pthread_mutex_lock(&mem->mutex);
        
        // Marcar como procesada y señalizar
        req->processed = true;
        mem->has_request = false;
        pthread_cond_broadcast(&mem->current_request.done);
        
        pthread_mutex_unlock(&mem->mutex);
    }
    
    printf("[MEMORY] Thread terminado.\n");
    return NULL;
}

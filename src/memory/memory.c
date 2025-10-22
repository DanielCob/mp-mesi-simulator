#define LOG_MODULE "MEMORY"
#include "memory.h"
#include <stdio.h>
#include "log.h"

// INICIALIZACIÓN Y LIMPIEZA

void mem_init(Memory* mem) {
    // Inicializar datos a cero
    for (int i = 0; i < MEM_SIZE; i++) {
        mem->data[i] = 0.0;
    }
    
    memory_stats_init(&mem->stats);
    
    pthread_mutex_init(&mem->mutex, NULL);
    pthread_cond_init(&mem->request_ready, NULL);
    pthread_cond_init(&mem->current_request.done, NULL);
    
    mem->has_request = false;
    mem->running = true;
    mem->current_request.processed = false;
    
    LOGI("Initialized");
}

void mem_destroy(Memory* mem) {
    mem->running = false;
    pthread_cond_broadcast(&mem->request_ready);
    pthread_mutex_destroy(&mem->mutex);
    pthread_cond_destroy(&mem->request_ready);
    pthread_cond_destroy(&mem->current_request.done);
}

// OPERACIONES DE LECTURA Y ESCRITURA DE BLOQUES

void mem_read_block(Memory* mem, int addr, double block[BLOCK_SIZE], int pe_id) {
    if (!IS_ALIGNED(addr)) {
    LOGW("block read: unaligned address %d (adjusting)", addr);
        addr = ALIGN_DOWN(addr);
    }
    
    pthread_mutex_lock(&mem->mutex);
    
    // Esperar si hay otra solicitud en proceso
    while (mem->has_request) {
        pthread_cond_wait(&mem->current_request.done, &mem->mutex);
    }
    
    // Preparar solicitud de lectura
    mem->current_request.op = MEM_OP_READ_BLOCK;
    mem->current_request.addr = addr;
    mem->current_request.pe_id = pe_id;
    mem->current_request.processed = false;
    mem->has_request = true;
    
    pthread_cond_signal(&mem->request_ready);
    
    // Esperar procesamiento
    while (!mem->current_request.processed) {
        pthread_cond_wait(&mem->current_request.done, &mem->mutex);
    }
    
    // Copiar resultado
    for (int i = 0; i < BLOCK_SIZE; i++) {
        block[i] = mem->current_request.block[i];
    }
    
    pthread_mutex_unlock(&mem->mutex);
}

void mem_write_block(Memory* mem, int addr, const double block[BLOCK_SIZE], int pe_id) {
    if (!IS_ALIGNED(addr)) {
    LOGW("block write: unaligned address %d (adjusting)", addr);
        addr = ALIGN_DOWN(addr);
    }
    
    pthread_mutex_lock(&mem->mutex);
    
    // Esperar si hay otra solicitud en proceso
    while (mem->has_request) {
        pthread_cond_wait(&mem->current_request.done, &mem->mutex);
    }
    
    // Preparar solicitud de escritura
    mem->current_request.op = MEM_OP_WRITE_BLOCK;
    mem->current_request.addr = addr;
    mem->current_request.pe_id = pe_id;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        mem->current_request.block[i] = block[i];
    }
    mem->current_request.processed = false;
    mem->has_request = true;
    
    pthread_cond_signal(&mem->request_ready);
    
    // Esperar procesamiento
    while (!mem->current_request.processed) {
        pthread_cond_wait(&mem->current_request.done, &mem->mutex);
    }
    
    pthread_mutex_unlock(&mem->mutex);
}

// THREAD DE MEMORIA

void* mem_thread_func(void* arg) {
    Memory* mem = (Memory*)arg;
    LOGD("Thread started");
    
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
        
        MemRequest* req = &mem->current_request;
        pthread_mutex_unlock(&mem->mutex);
        
        // Procesar solicitud (fuera del lock para permitir otras operaciones)
        if (req->op == MEM_OP_READ_BLOCK) {
          LOGD("READ_BLOCK addr=%d (%d doubles) from PE%d", 
              req->addr, BLOCK_SIZE, req->pe_id);
            for (int i = 0; i < BLOCK_SIZE; i++) {
                req->block[i] = mem->data[req->addr + i];
            }
            memory_stats_record_read(&mem->stats, req->pe_id, BLOCK_SIZE * sizeof(double));
        } 
        else if (req->op == MEM_OP_WRITE_BLOCK) {
          LOGD("WRITE_BLOCK addr=%d (%d doubles) from PE%d", 
              req->addr, BLOCK_SIZE, req->pe_id);
            for (int i = 0; i < BLOCK_SIZE; i++) {
                mem->data[req->addr + i] = req->block[i];
            }
            memory_stats_record_write(&mem->stats, req->pe_id, BLOCK_SIZE * sizeof(double));
        }
        
        pthread_mutex_lock(&mem->mutex);
        
        // Marcar como procesada y señalizar
        req->processed = true;
        mem->has_request = false;
        pthread_cond_broadcast(&mem->current_request.done);
        
        pthread_mutex_unlock(&mem->mutex);
    }
    
    LOGD("Thread finished");
    return NULL;
}

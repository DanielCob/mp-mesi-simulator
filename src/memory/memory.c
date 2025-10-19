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

double mem_read(Memory* mem, int addr) {
    pthread_mutex_lock(&mem->mutex);
    
    // Esperar si hay otra solicitud en proceso
    while (mem->has_request) {
        pthread_cond_wait(&mem->current_request.done, &mem->mutex);
    }
    
    // Preparar solicitud de lectura
    mem->current_request.op = MEM_OP_READ;
    mem->current_request.addr = addr;
    mem->current_request.processed = false;
    mem->has_request = true;
    
    // Señalizar al thread de memoria
    pthread_cond_signal(&mem->request_ready);
    
    // Esperar a que la memoria procese la solicitud
    while (!mem->current_request.processed) {
        pthread_cond_wait(&mem->current_request.done, &mem->mutex);
    }
    
    // Obtener resultado
    double result = mem->current_request.result;
    
    pthread_mutex_unlock(&mem->mutex);
    return result;
}

void mem_write(Memory* mem, int addr, double value) {
    pthread_mutex_lock(&mem->mutex);
    
    // Esperar si hay otra solicitud en proceso
    while (mem->has_request) {
        pthread_cond_wait(&mem->current_request.done, &mem->mutex);
    }
    
    // Preparar solicitud de escritura
    mem->current_request.op = MEM_OP_WRITE;
    mem->current_request.addr = addr;
    mem->current_request.value = value;
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
        if (req->op == MEM_OP_READ) {
            printf("[MEMORY] READ addr=%d\n", req->addr);
            req->result = mem->data[req->addr];
        } else if (req->op == MEM_OP_WRITE) {
            printf("[MEMORY] WRITE addr=%d, value=%.2f\n", req->addr, req->value);
            mem->data[req->addr] = req->value;
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

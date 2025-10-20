#include "bus.h"
#include "handlers.h"
#include <stdio.h>
#include <pthread.h>

void bus_init(Bus* bus, Cache* caches[], Memory* memory) {
    for (int i = 0; i < NUM_PES; i++)
        bus->caches[i] = caches[i];
    
    bus->memory = memory;  // Guardar referencia a la memoria

    // Inicializar estadísticas
    bus_stats_init(&bus->stats);

    // Inicializar mutex y variables de condición
    pthread_mutex_init(&bus->mutex, NULL);
    pthread_cond_init(&bus->request_ready, NULL);
    pthread_cond_init(&bus->current_request.done, NULL);
    
    bus->has_request = false;
    bus->running = true;
    bus->current_request.processed = false;

    // Registrar los handlers
    bus_register_handlers(bus);

    printf("[BUS] Initialized.\n");
}

void bus_destroy(Bus* bus) {
    bus->running = false;
    pthread_cond_broadcast(&bus->request_ready);  // Despertar thread del bus
    pthread_mutex_destroy(&bus->mutex);
    pthread_cond_destroy(&bus->request_ready);
    pthread_cond_destroy(&bus->current_request.done);
}

void bus_broadcast(Bus* bus, BusMsg msg, int addr, int src_pe) {
    pthread_mutex_lock(&bus->mutex);
    
    // Esperar si hay otra solicitud en proceso
    while (bus->has_request) {
        pthread_cond_wait(&bus->current_request.done, &bus->mutex);
    }
    
    // Preparar la solicitud
    bus->current_request.msg = msg;
    bus->current_request.addr = addr;
    bus->current_request.src_pe = src_pe;
    bus->current_request.processed = false;
    bus->has_request = true;
    
    // Señalizar al thread del bus
    pthread_cond_signal(&bus->request_ready);
    
    // Esperar a que el bus procese la solicitud
    while (!bus->current_request.processed) {
        pthread_cond_wait(&bus->current_request.done, &bus->mutex);
    }
    
    pthread_mutex_unlock(&bus->mutex);
}

void* bus_thread_func(void* arg) {
    Bus* bus = (Bus*)arg;
    printf("[BUS] Thread iniciado.\n");
    
    while (bus->running) {
        pthread_mutex_lock(&bus->mutex);
        
        // Esperar por solicitudes
        while (!bus->has_request && bus->running) {
            pthread_cond_wait(&bus->request_ready, &bus->mutex);
        }
        
        if (!bus->running) {
            pthread_mutex_unlock(&bus->mutex);
            break;
        }
        
        // Procesar la solicitud
        BusRequest* req = &bus->current_request;
        printf("[BUS] Señal %d recibida de PE%d (addr=%d)\n", 
               req->msg, req->src_pe, req->addr);
        
        // Registrar estadísticas según el tipo de mensaje
        switch (req->msg) {
            case BUS_RD:
                bus_stats_record_bus_rd(&bus->stats, req->src_pe);
                bus_stats_record_transfer(&bus->stats, BLOCK_SIZE * sizeof(double));
                break;
            case BUS_RDX:
                bus_stats_record_bus_rdx(&bus->stats, req->src_pe);
                bus_stats_record_transfer(&bus->stats, BLOCK_SIZE * sizeof(double));
                break;
            case BUS_UPGR:
                bus_stats_record_bus_upgr(&bus->stats, req->src_pe);
                break;
            case BUS_WB:
                bus_stats_record_bus_wb(&bus->stats, req->src_pe);
                bus_stats_record_transfer(&bus->stats, BLOCK_SIZE * sizeof(double));
                break;
        }
        
        pthread_mutex_unlock(&bus->mutex);
        
        // Llamar al handler (fuera del lock para evitar deadlock)
        if (bus->handlers[req->msg]) {
            bus->handlers[req->msg](bus, req->addr, req->src_pe);
        } else {
            printf("[BUS] No hay handler definido para la señal %d\n", req->msg);
        }
        
        pthread_mutex_lock(&bus->mutex);
        
        // Marcar como procesada, limpiar y señalizar
        req->processed = true;
        bus->has_request = false;  // Liberar para próxima solicitud
        pthread_cond_broadcast(&bus->current_request.done);
        
        pthread_mutex_unlock(&bus->mutex);
    }
    
    printf("[BUS] Thread terminado.\n");
    return NULL;
}

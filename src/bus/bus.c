#include "bus.h"
#include "handlers.h"
#include <stdio.h>
#include <pthread.h>

void bus_init(Bus* bus, Cache* caches[], Memory* memory) {
    for (int i = 0; i < NUM_PES; i++)
        bus->caches[i] = caches[i];
    
    bus->memory = memory;

    // Inicializar estadísticas
    bus_stats_init(&bus->stats);

    // Inicializar mutex y variables de condición
    pthread_mutex_init(&bus->mutex, NULL);
    pthread_cond_init(&bus->request_ready, NULL);
    
    // Inicializar solicitudes por PE
    for (int i = 0; i < NUM_PES; i++) {
        bus->requests[i].has_request = false;
        bus->requests[i].processed = false;
        pthread_cond_init(&bus->requests[i].done, NULL);
    }
    
    bus->next_pe = 0;  // Comenzar con PE0
    bus->running = true;

    // Registrar los handlers
    bus_register_handlers(bus);

    printf("[BUS] Initialized with Round-Robin scheduling.\n");
}

void bus_destroy(Bus* bus) {
    bus->running = false;
    pthread_cond_broadcast(&bus->request_ready);  // Despertar thread del bus
    pthread_mutex_destroy(&bus->mutex);
    pthread_cond_destroy(&bus->request_ready);
    
    // Destruir condition variables de cada PE
    for (int i = 0; i < NUM_PES; i++) {
        pthread_cond_destroy(&bus->requests[i].done);
    }
}

void bus_broadcast(Bus* bus, BusMsg msg, int addr, int src_pe) {
    bus_broadcast_with_callback(bus, msg, addr, src_pe, NULL, NULL);
}

void bus_broadcast_with_callback(Bus* bus, BusMsg msg, int addr, int src_pe,
                                   BusCallback callback, void* callback_context) {
    pthread_mutex_lock(&bus->mutex);
    
    // Esperar si este PE ya tiene una solicitud pendiente
    while (bus->requests[src_pe].has_request) {
        pthread_cond_wait(&bus->requests[src_pe].done, &bus->mutex);
    }
    
    // Registrar la solicitud
    bus->requests[src_pe].msg = msg;
    bus->requests[src_pe].addr = addr;
    bus->requests[src_pe].src_pe = src_pe;
    bus->requests[src_pe].has_request = true;
    bus->requests[src_pe].processed = false;
    bus->requests[src_pe].callback = callback;
    bus->requests[src_pe].callback_context = callback_context;
    
    // Señalar que hay una nueva solicitud
    pthread_cond_signal(&bus->request_ready);
    
    // Esperar a que se procese esta solicitud
    while (!bus->requests[src_pe].processed) {
        pthread_cond_wait(&bus->requests[src_pe].done, &bus->mutex);
    }
    
    // Limpiar la solicitud
    bus->requests[src_pe].has_request = false;
    
    pthread_mutex_unlock(&bus->mutex);
}

void* bus_thread_func(void* arg) {
    Bus* bus = (Bus*)arg;
    printf("[BUS] Thread iniciado con Round-Robin scheduling.\n");
    
    while (bus->running) {
        pthread_mutex_lock(&bus->mutex);
        
        // Esperar hasta que haya al menos una solicitud pendiente
        bool has_pending = false;
        while (bus->running) {
            // Revisar si hay solicitudes pendientes en algún PE
            has_pending = false;
            for (int i = 0; i < NUM_PES; i++) {
                if (bus->requests[i].has_request && !bus->requests[i].processed) {
                    has_pending = true;
                    break;
                }
            }
            
            if (has_pending) break;
            pthread_cond_wait(&bus->request_ready, &bus->mutex);
        }
        
        if (!bus->running) {
            pthread_mutex_unlock(&bus->mutex);
            break;
        }
        
        // Round-robin: buscar el siguiente PE con solicitud pendiente
        PERequest* req = NULL;
        int selected_pe = -1;
        
        for (int i = 0; i < NUM_PES; i++) {
            int pe_idx = (bus->next_pe + i) % NUM_PES;
            if (bus->requests[pe_idx].has_request && !bus->requests[pe_idx].processed) {
                req = &bus->requests[pe_idx];
                selected_pe = pe_idx;
                bus->next_pe = (pe_idx + 1) % NUM_PES;  // Siguiente PE en round-robin
                break;
            }
        }
        
        printf("[BUS] [RR] PE%d: Señal %d (addr=%d)\n", 
               selected_pe, req->msg, req->addr);
        
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
        
        // Llamar al handler (fuera del lock para evitar deadlock)
        if (bus->handlers[req->msg]) {
            bus->handlers[req->msg](bus, req->addr, req->src_pe);
        } else {
            printf("[BUS] No hay handler definido para la señal %d\n", req->msg);
        }
        
        // Ejecutar callback si fue proporcionado (OPCIÓN B)
        // El callback se ejecuta después del handler, antes de señalizar al PE
        if (req->callback) {
            printf("[BUS] Ejecutando callback para PE%d\n", selected_pe);
            req->callback(req->callback_context);
        }
        
        // Marcar como procesada y señalizar al PE
        req->processed = true;
        pthread_cond_broadcast(&bus->requests[selected_pe].done);
        
        pthread_mutex_unlock(&bus->mutex);
    }
    
    printf("[BUS] Thread terminado.\n");
    return NULL;
}

#define LOG_MODULE "BUS"
#include "bus.h"
#include "handlers.h"
#include <stdio.h>
#include <pthread.h>
#include "log.h"

// INICIALIZACIÓN Y LIMPIEZA

void bus_init(Bus* bus, Cache* caches[], Memory* memory) {
    for (int i = 0; i < NUM_PES; i++) {
        bus->caches[i] = caches[i];
    }
    
    bus->memory = memory;
    bus_stats_init(&bus->stats);

    pthread_mutex_init(&bus->mutex, NULL);
    pthread_cond_init(&bus->request_ready, NULL);
    
    for (int i = 0; i < NUM_PES; i++) {
        bus->requests[i].has_request = false;
        bus->requests[i].processed = false;
        pthread_cond_init(&bus->requests[i].done, NULL);
    }
    
    bus->next_pe = 0;
    bus->running = true;

    bus_register_handlers(bus);

    LOGI("Inicializado (planificación round-robin)");
}

void bus_destroy(Bus* bus) {
    bus->running = false;
    pthread_cond_broadcast(&bus->request_ready);
    pthread_mutex_destroy(&bus->mutex);
    pthread_cond_destroy(&bus->request_ready);
    
    for (int i = 0; i < NUM_PES; i++) {
        pthread_cond_destroy(&bus->requests[i].done);
    }
}

// FUNCIONES DE BROADCAST

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
    
    pthread_cond_signal(&bus->request_ready);
    
    // Esperar a que se procese esta solicitud (bloqueante)
    while (!bus->requests[src_pe].processed) {
        pthread_cond_wait(&bus->requests[src_pe].done, &bus->mutex);
    }
    
    bus->requests[src_pe].has_request = false;
    pthread_mutex_unlock(&bus->mutex);
}

// THREAD DEL BUS (Round-Robin)

void* bus_thread_func(void* arg) {
    Bus* bus = (Bus*)arg;
    LOGD("Thread iniciado (round-robin)");
    
    while (bus->running) {
        pthread_mutex_lock(&bus->mutex);
        
        // Esperar hasta que haya solicitudes pendientes
        bool has_pending = false;
        while (bus->running) {
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
                bus->next_pe = (pe_idx + 1) % NUM_PES;
                break;
            }
        }
        
    LOGD("RR: PE%d señal=%d addr=%d", selected_pe, req->msg, req->addr);
        
        // Registrar estadísticas
        switch (req->msg) {
            case BUS_RD:
                bus_stats_record_bus_rd(&bus->stats, req->src_pe);
                // BUS_RD transfiere un bloque completo de datos
                bus_stats_record_data_transfer(&bus->stats, BLOCK_SIZE * sizeof(double));
                break;
            case BUS_RDX:
                bus_stats_record_bus_rdx(&bus->stats, req->src_pe);
                // BUS_RDX transfiere un bloque completo de datos
                bus_stats_record_data_transfer(&bus->stats, BLOCK_SIZE * sizeof(double));
                break;
            case BUS_UPGR:
                bus_stats_record_bus_upgr(&bus->stats, req->src_pe);
                // BUS_UPGR solo transfiere señal de control (dirección + comando)
                bus_stats_record_control_transfer(&bus->stats, BUS_CONTROL_SIGNAL_SIZE);
                break;
            case BUS_WB:
                bus_stats_record_bus_wb(&bus->stats, req->src_pe);
                // BUS_WB transfiere un bloque completo de datos
                bus_stats_record_data_transfer(&bus->stats, BLOCK_SIZE * sizeof(double));
                break;
        }
        
        // Ejecutar handler
        if (bus->handlers[req->msg]) {
            bus->handlers[req->msg](bus, req->addr, req->src_pe);
        } else {
            LOGW("Sin handler para señal=%d", req->msg);
        }
        
        // Ejecutar callback si fue proporcionado (después del handler)
        if (req->callback) {
            LOGD("Ejecutando callback para PE%d", selected_pe);
            req->callback(req->callback_context);
        }
        
        // Marcar como procesada y señalizar al PE
        req->processed = true;
        pthread_cond_broadcast(&bus->requests[selected_pe].done);
        
        pthread_mutex_unlock(&bus->mutex);
    }
    
    LOGD("Thread terminado");
    return NULL;
}

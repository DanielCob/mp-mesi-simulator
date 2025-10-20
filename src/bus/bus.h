#ifndef BUS_H
#define BUS_H

#include "config.h"
#include "bus_stats.h"
#include "memory.h"
#include "cache.h"
#include <pthread.h>
#include <stdbool.h>

// Tipos de mensajes del bus
typedef enum { BUS_RD, BUS_RDX, BUS_UPGR, BUS_WB } BusMsg;

struct Bus; // Declaración adelantada

// Tipo de puntero a función handler
typedef void (*BusHandler)(struct Bus* bus, int addr, int src_pe);

// Estructura de solicitud del bus (una por PE)
typedef struct {
    BusMsg msg;
    int addr;
    int src_pe;
    volatile bool has_request;   // Si hay solicitud pendiente
    volatile bool processed;     // Si ya fue procesada
    pthread_cond_t done;         // Condition variable para este PE
} PERequest;

// Estructura principal del bus
typedef struct Bus {
    Cache* caches[NUM_PES];
    Memory* memory;              // Referencia a la memoria
    BusHandler handlers[4];      // Dispatch table
    pthread_mutex_t mutex;       // Protección del bus
    pthread_cond_t request_ready;  // Señal de nueva solicitud
    PERequest requests[NUM_PES]; // Una solicitud por PE
    int next_pe;                 // Siguiente PE a atender (round-robin)
    bool running;                // El bus está corriendo
    BusStats stats;              // Estadísticas del bus
} Bus;

// Funciones públicas
void bus_init(Bus* bus, Cache* caches[], Memory* memory);
void bus_destroy(Bus* bus);
void bus_broadcast(Bus* bus, BusMsg msg, int addr, int src_pe);
void* bus_thread_func(void* arg);  // Función del thread del bus

#endif

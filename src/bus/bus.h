#ifndef BUS_H
#define BUS_H

#include "config.h"
#include "bus_stats.h"
#include "memory.h"
#include "cache.h"
#include <pthread.h>
#include <stdbool.h>

// Bus message types
typedef enum { BUS_RD, BUS_RDX, BUS_UPGR, BUS_WB } BusMsg;

struct Bus; // Forward declaration

// Handler function pointer type
typedef void (*BusHandler)(struct Bus* bus, int addr, int src_pe);

// Callback type a PE can pass to execute after the handler
// Allows the PE to perform additional operations (e.g., a write) atomically
typedef void (*BusCallback)(void* context);

// Bus request structure (one per PE)
typedef struct {
    BusMsg msg;
    int addr;
    int src_pe;
    volatile bool has_request;   // Whether a pending request exists
    volatile bool processed;     // Whether it has been processed
    pthread_cond_t done;         // Condition variable for this PE
    BusCallback callback;        // Optional callback to run after handler
    void* callback_context;      // Context passed to the callback
} PERequest;

// Bus main structure
typedef struct Bus {
    Cache* caches[NUM_PES];
    Memory* memory;              // Memory reference
    BusHandler handlers[4];      // Dispatch table
    pthread_mutex_t mutex;       // Bus protection
    pthread_cond_t request_ready; // New request signal
    PERequest requests[NUM_PES]; // One request per PE
    int next_pe;                 // Next PE to serve (round-robin)
    bool running;                // Bus is running
    BusStats stats;              // Bus statistics
} Bus;

// Public API
void bus_init(Bus* bus, Cache* caches[], Memory* memory);
void bus_destroy(Bus* bus);
void bus_broadcast(Bus* bus, BusMsg msg, int addr, int src_pe);
void bus_broadcast_with_callback(Bus* bus, BusMsg msg, int addr, int src_pe, 
                                  BusCallback callback, void* callback_context);
void* bus_thread_func(void* arg);  // Bus thread function

#endif

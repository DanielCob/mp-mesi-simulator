#ifndef PE_H
#define PE_H

#include "cache/cache.h"
#include "pe/registers.h"
#include <pthread.h>

typedef struct {
    int id;
    RegisterFile rf;  // Banco de registros
    Cache* cache;
} PE;

void* pe_run(void* arg);

#endif

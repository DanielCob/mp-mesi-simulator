#define LOG_MODULE "PE"
#include "pe.h"
#include "registers.h"
#include "isa.h"
#include "loader.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "log.h"

void* pe_run(void* arg) {
    PE* pe = (PE*)arg;
    LOGD("PE%d: starting thread", pe->id);
    
    // ===== CARGAR PROGRAMA DESDE ARCHIVO =====
    // Cada PE ejecuta su parte del producto punto paralelo
    // PE0-PE2: Calculan productos parciales
    // PE3: Calcula su producto parcial + reducción final
    
    const char* program_files[] = {
        "asm/dotprod_pe0.asm",   // PE0: elementos [0-3]
        "asm/dotprod_pe1.asm",   // PE1: elementos [4-7]
        "asm/dotprod_pe2.asm",   // PE2: elementos [8-11]
        "asm/dotprod_pe3.asm"    // PE3: elementos [12-15] + reducción
    };
    
    const char* filename = program_files[pe->id];
    
    LOGI("PE%d: loading program", pe->id);
    LOGD("PE%d: file=%s", pe->id, filename);
    
    Program* prog = load_program(filename);
    
    if (!prog) {
    LOGE("PE%d: could not load program %s", pe->id, filename);
        return NULL;
    }
    
    LOGI("PE%d: program loaded, instructions=%d", pe->id, prog->size);
    
    LOGI("PE%d: starting execution", pe->id);
    
    // Ejecutar programa
    pe->rf.pc = 0;
    int running = 1;
    int max_iterations = 1000;  // Aumentado para permitir programas con loops
    int iterations = 0;
    
    while (running && iterations < max_iterations) {
        if (pe->rf.pc >= (uint64_t)prog->size) {
         LOGE("PE%d: PC out of range (%lu >= %d)", pe->id, pe->rf.pc, prog->size);
            break;
        }
        
        running = execute_instruction(&prog->code[pe->rf.pc], 
                                      &pe->rf, 
                                      pe->cache, 
                                      pe->id);
        iterations++;
    }
    
    if (iterations >= max_iterations) {
    LOGW("PE%d: maximum number of iterations reached (%d)", pe->id, max_iterations);
    }
    
    LOGI("PE%d: execution finished", pe->id);
    LOGI("PE%d: iterations executed=%d", pe->id, iterations);
    
    // Imprimir estado final de registros
    reg_print(&pe->rf, pe->id);
    
    // Liberar memoria del programa
    free_program(prog);
    
    LOGD("PE%d: done", pe->id);
    return NULL;
}

#define LOG_MODULE "PE"
#include "pe.h"
#include "registers.h"
#include "isa.h"
#include "loader.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "log.h"
#include "debug/debug.h"

void* pe_run(void* arg) {
    PE* pe = (PE*)arg;
    LOGD("PE%d: starting thread", pe->id);
    
    // ===== LOAD PROGRAM FROM FILE =====
    // Each PE executes its portion of the parallel dot product
    // PE0-PE2: compute partial products
    // PE3: compute partial product + final reduction
    
    const char* program_files[] = {
    ASM_DOTPROD_PE0_PATH,   // PE0: elements [0-3]
    ASM_DOTPROD_PE1_PATH,   // PE1: elements [4-7]
    ASM_DOTPROD_PE2_PATH,   // PE2: elements [8-11]
    ASM_DOTPROD_PE3_PATH    // PE3: elements [12-15] + reduction
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
    
    // Run program
    pe->rf.pc = 0;
    int running = 1;
    int iterations = 0;

    // Allow overriding the max iterations via environment variable.
    // SIM_MAX_ITERS: if set to 0 or negative, run without an iteration cap.
    int max_iterations = 100000;  // default higher to allow longer loops
    const char* env_max = getenv("SIM_MAX_ITERS");
    if (env_max) {
        int val = atoi(env_max);
        max_iterations = val;
    }
    
    while (running && (max_iterations <= 0 || iterations < max_iterations)) {
        if (pe->rf.pc >= (uint64_t)prog->size) {
         LOGE("PE%d: PC out of range (%lu >= %d)", pe->id, pe->rf.pc, prog->size);
            break;
        }

        // Debugger hook: pause/step before executing
        dbg_before_instruction(pe->id, pe->rf.pc, &prog->code[pe->rf.pc]);
        
        running = execute_instruction(&prog->code[pe->rf.pc], 
                                      &pe->rf, 
                                      pe->cache, 
                                      pe->id);
        iterations++;
    }
    
    if (max_iterations > 0 && iterations >= max_iterations) {
        LOGW("PE%d: maximum number of iterations reached (%d)", pe->id, max_iterations);
    }
    
    LOGI("PE%d: execution finished", pe->id);
    LOGI("PE%d: iterations executed=%d", pe->id, iterations);
    
    // Print final register state
    reg_print(&pe->rf, pe->id);
    
    // Free program memory
    free_program(prog);
    
    LOGD("PE%d: done", pe->id);
    return NULL;
}

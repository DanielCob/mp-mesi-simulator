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
    LOGD("PE%d: iniciando hilo", pe->id);
    
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
    
    LOGI("PE%d: cargando programa", pe->id);
    LOGD("PE%d: archivo=%s", pe->id, filename);
    
    Program* prog = load_program(filename);
    
    if (!prog) {
    LOGE("PE%d: no se pudo cargar el programa %s", pe->id, filename);
        return NULL;
    }
    
    LOGI("PE%d: programa cargado, instrucciones=%d", pe->id, prog->size);
    
    // Imprimir el programa cargado
    LOGD("PE%d: contenido del programa (primeras 10)", pe->id);
    for (int i = 0; i < prog->size && i < 10; i++) {  // Mostrar máximo 10 instrucciones
        Instruction* inst = &prog->code[i];
        printf("[PE%d]   [%2d] %s ", pe->id, i, 
               inst->op == OP_MOV ? "MOV" :
               inst->op == OP_LOAD ? "LOAD" :
               inst->op == OP_STORE ? "STORE" :
               inst->op == OP_FADD ? "FADD" :
               inst->op == OP_FMUL ? "FMUL" :
               inst->op == OP_INC ? "INC" :
               inst->op == OP_DEC ? "DEC" :
               inst->op == OP_JNZ ? "JNZ" : "HALT");
        
        switch (inst->op) {
            case OP_MOV:
                printf("R%d, %.2f", inst->rd, inst->imm);
                break;
            case OP_LOAD:
            case OP_STORE:
                if (inst->addr_mode == ADDR_DIRECT) {
                    printf("R%d, [%d]", inst->rd, inst->addr);
                } else {
                    printf("R%d, [R%d]", inst->rd, inst->addr_reg);
                }
                break;
            case OP_FADD:
            case OP_FMUL:
                printf("R%d, R%d, R%d", inst->rd, inst->ra, inst->rb);
                break;
            case OP_INC:
            case OP_DEC:
                printf("R%d", inst->rd);
                break;
            case OP_JNZ:
                printf("%d", inst->label);
                break;
            case OP_HALT:
                break;
        }
        printf("\n");
    }
    if (prog->size > 10) {
    LOGD("PE%d: ... (%d instrucciones más)", pe->id, prog->size - 10);
    }
    
    // NOTA: La memoria ya fue inicializada por dotprod_init_data() en main.c
    // No se necesita inicialización adicional aquí.
    
    LOGI("PE%d: iniciando ejecución", pe->id);
    
    // Ejecutar programa
    pe->rf.pc = 0;
    int running = 1;
    int max_iterations = 1000;  // Aumentado para permitir programas con loops
    int iterations = 0;
    
    while (running && iterations < max_iterations) {
        if (pe->rf.pc >= (uint64_t)prog->size) {
         LOGE("PE%d: PC fuera de rango (%lu >= %d)", pe->id, pe->rf.pc, prog->size);
            break;
        }
        
        running = execute_instruction(&prog->code[pe->rf.pc], 
                                      &pe->rf, 
                                      pe->cache, 
                                      pe->id);
        iterations++;
    }
    
    if (iterations >= max_iterations) {
    LOGW("PE%d: máximo de iteraciones alcanzado (%d)", pe->id, max_iterations);
    }
    
    LOGI("PE%d: ejecución terminada", pe->id);
    LOGI("PE%d: iteraciones ejecutadas=%d", pe->id, iterations);
    
    // Imprimir estado final de registros
    reg_print(&pe->rf, pe->id);
    
    // Liberar memoria del programa
    free_program(prog);
    
    LOGD("PE%d: terminado", pe->id);
    return NULL;
}

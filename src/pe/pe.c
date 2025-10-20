#include "pe.h"
#include "registers.h"
#include "isa.h"
#include "loader.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void* pe_run(void* arg) {
    PE* pe = (PE*)arg;
    printf("[PE%d] Starting thread...\n", pe->id);
    
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
    
    printf("\n[PE%d] ========== CARGANDO PROGRAMA ==========\n", pe->id);
    printf("[PE%d] Archivo: %s\n", pe->id, filename);
    
    Program* prog = load_program(filename);
    
    if (!prog) {
        fprintf(stderr, "[PE%d] ERROR: No se pudo cargar el programa %s\n", pe->id, filename);
        return NULL;
    }
    
    printf("[PE%d] Programa cargado: %d instrucciones\n", pe->id, prog->size);
    
    // Imprimir el programa cargado
    printf("\n[PE%d] Contenido del programa:\n", pe->id);
    for (int i = 0; i < prog->size && i < 10; i++) {  // Mostrar máximo 10 instrucciones
        Instruction* inst = &prog->code[i];
        printf("[PE%d]   [%2d] %s ", pe->id, i, 
               inst->op == OP_LOAD ? "LOAD" :
               inst->op == OP_STORE ? "STORE" :
               inst->op == OP_FADD ? "FADD" :
               inst->op == OP_FMUL ? "FMUL" :
               inst->op == OP_INC ? "INC" :
               inst->op == OP_DEC ? "DEC" :
               inst->op == OP_JNZ ? "JNZ" : "HALT");
        
        switch (inst->op) {
            case OP_LOAD:
            case OP_STORE:
                printf("R%d, [%d]", inst->rd, inst->addr);
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
        printf("[PE%d]   ... (%d instrucciones más)\n", pe->id, prog->size - 10);
    }
    
    // NOTA: La memoria ya fue inicializada por dotprod_init_data() en main.c
    // No se necesita inicialización adicional aquí.
    
    printf("\n[PE%d] ========== INICIANDO EJECUCIÓN ==========\n", pe->id);
    
    // Ejecutar programa
    pe->rf.pc = 0;
    int running = 1;
    int max_iterations = 100;
    int iterations = 0;
    
    while (running && iterations < max_iterations) {
        if (pe->rf.pc >= (uint64_t)prog->size) {
            printf("[PE%d] ERROR: PC fuera de rango (%lu >= %d)\n", 
                   pe->id, pe->rf.pc, prog->size);
            break;
        }
        
        running = execute_instruction(&prog->code[pe->rf.pc], 
                                      &pe->rf, 
                                      pe->cache, 
                                      pe->id);
        iterations++;
    }
    
    if (iterations >= max_iterations) {
        printf("[PE%d] ADVERTENCIA: Máximo de iteraciones alcanzado (%d)\n", 
               pe->id, max_iterations);
    }
    
    printf("\n[PE%d] ========== EJECUCIÓN TERMINADA ==========\n", pe->id);
    printf("[PE%d] Iteraciones ejecutadas: %d\n", pe->id, iterations);
    
    // Imprimir estado final de registros
    reg_print(&pe->rf, pe->id);
    
    // Liberar memoria del programa
    free_program(prog);
    
    printf("[PE%d] Finished.\n", pe->id);
    return NULL;
}

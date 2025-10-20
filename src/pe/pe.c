#include "pe.h"
#include "pe/registers.h"
#include "pe/isa.h"
#include "pe/loader.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void* pe_run(void* arg) {
    PE* pe = (PE*)arg;
    printf("[PE%d] Starting thread...\n", pe->id);
    
    // ===== CARGAR PROGRAMA DESDE ARCHIVO =====
    // Cada PE puede ejecutar un programa diferente o el mismo
    // Formato: test_suma.asm, test_loop.asm, etc.
    
    // Por defecto, todos los PEs ejecutan el mismo programa
    // Puedes cambiar esto para que cada PE ejecute un programa diferente
    const char* program_files[] = {
        "asm/test_suma.asm",      // PE0
        "asm/test_producto.asm",  // PE1
        "asm/test_loop.asm",      // PE2
        "asm/test_jnz.asm"        // PE3
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
                printf("R%d, %d", inst->rd, inst->label);
                break;
            case OP_HALT:
                break;
        }
        printf("\n");
    }
    if (prog->size > 10) {
        printf("[PE%d]   ... (%d instrucciones más)\n", pe->id, prog->size - 10);
    }
    
    // Inicializar memoria con valores de prueba para cada PE
    // Cada PE tiene su región de memoria: PE0 usa 100-199, PE1 usa 200-299, etc.
    int base_addr = 100 + pe->id * 100;  // PE0→100, PE1→200, PE2→300, PE3→400
    
    // Valores específicos por PE según sus programas:
    // PE0 (test_suma.asm): 6.0 + 3.5 = 9.5
    // PE1 (test_producto.asm): 6.0 * 3.5 + 6.0 = 27.0
    // PE2 (test_loop.asm): 5.0 + 5.0 = 10.0
    // PE3 (test_jnz.asm): loop desde 8 hasta 0
    double val1, val2;
    if (pe->id == 2) {
        // PE2 necesita 5.0 + 5.0
        val1 = 5.0;
        val2 = 5.0;
    } else {
        // PE0, PE1, PE3 usan 6.0 y 3.5
        val1 = 6.0;
        val2 = 3.5;
    }
    
    printf("\n[PE%d] Inicializando memoria de prueba:\n", pe->id);
    printf("[PE%d]   memoria[%d] = %.2f\n", pe->id, base_addr, val1);
    printf("[PE%d]   memoria[%d] = %.2f\n", pe->id, base_addr + 4, val2);
    
    cache_write(pe->cache, base_addr, val1, pe->id);
    cache_write(pe->cache, base_addr + 4, val2, pe->id);
    
    // Escalonar ejecución para reducir contención
    sleep(pe->id);
    
    printf("\n[PE%d] ========== INICIANDO EJECUCIÓN ==========\n", pe->id);
    
    // Ejecutar programa
    pe->rf.pc = 0;
    int running = 1;
    int max_iterations = 1000;  // Prevenir loops infinitos
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
    
    sleep(1);
    printf("[PE%d] Finished.\n", pe->id);
    return NULL;
}

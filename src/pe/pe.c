#include "pe.h"
#include "pe/registers.h"
#include "pe/isa.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void* pe_run(void* arg) {
    PE* pe = (PE*)arg;
    printf("[PE%d] Starting thread...\n", pe->id);
    
    // ===== TEST DE ISA =====
    printf("\n=== PE%d: Probando ISA ===\n", pe->id);
    
    // Crear programa de prueba simple
    // Programa: Suma dos números de memoria y guarda resultado
    // addr[0] = 5.0
    // addr[1] = 3.0
    // R0 = memoria[0]
    // R1 = memoria[1]
    // R2 = R0 + R1
    // memoria[2] = R2
    // R3 = R2 * R0
    // memoria[3] = R3
    // HALT
    
    Instruction program[] = {
        {OP_LOAD,  0, 0, 0, pe->id * 10 + 0, 0},      // [0] LOAD R0, [pe->id*10+0]
        {OP_LOAD,  1, 0, 0, pe->id * 10 + 1, 0},      // [1] LOAD R1, [pe->id*10+1]
        {OP_FADD,  2, 0, 1, 0, 0},                     // [2] FADD R2, R0, R1
        {OP_STORE, 2, 0, 0, pe->id * 10 + 2, 0},      // [3] STORE R2, [pe->id*10+2]
        {OP_FMUL,  3, 2, 0, 0, 0},                     // [4] FMUL R3, R2, R0
        {OP_STORE, 3, 0, 0, pe->id * 10 + 3, 0},      // [5] STORE R3, [pe->id*10+3]
        {OP_INC,   0, 0, 0, 0, 0},                     // [6] INC R0
        {OP_DEC,   1, 0, 0, 0, 0},                     // [7] DEC R1
        {OP_HALT,  0, 0, 0, 0, 0}                      // [8] HALT
    };
    
    size_t program_size = sizeof(program) / sizeof(Instruction);
    
    // Inicializar memoria con valores de prueba
    double val1 = 5.0 + pe->id;
    double val2 = 3.0 + pe->id * 0.5;
    
    printf("[PE%d] Inicializando memoria: addr[%d]=%.2f, addr[%d]=%.2f\n", 
           pe->id, pe->id * 10 + 0, val1, pe->id * 10 + 1, val2);
    
    cache_write(pe->cache, pe->id * 10 + 0, val1, pe->id);
    cache_write(pe->cache, pe->id * 10 + 1, val2, pe->id);
    
    sleep(pe->id);  // Escalonar ejecución para ver mejor
    
    printf("\n[PE%d] ========== INICIANDO EJECUCIÓN ==========\n", pe->id);
    
    // Ejecutar programa
    pe->rf.pc = 0;
    int running = 1;
    int max_iterations = 100;  // Prevenir loops infinitos
    int iterations = 0;
    
    while (running && iterations < max_iterations) {
        if (pe->rf.pc >= program_size) {
            printf("[PE%d] ERROR: PC fuera de rango (%lu >= %zu)\n", pe->id, pe->rf.pc, program_size);
            break;
        }
        
        running = execute_instruction(&program[pe->rf.pc], &pe->rf, pe->cache, pe->id);
        iterations++;
    }
    
    if (iterations >= max_iterations) {
        printf("[PE%d] ERROR: Máximo de iteraciones alcanzado\n", pe->id);
    }
    
    printf("\n[PE%d] ========== EJECUCIÓN TERMINADA ==========\n", pe->id);
    
    // Imprimir estado final
    reg_print(&pe->rf, pe->id);
    
    sleep(1);
    printf("[PE%d] Finished.\n", pe->id);
    return NULL;
}

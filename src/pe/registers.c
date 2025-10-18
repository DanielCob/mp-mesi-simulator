#include "registers.h"
#include <stdio.h>
#include <string.h>

void reg_init(RegisterFile* rf) {
    // Inicializar todos los registros en 0.0
    for (int i = 0; i < NUM_REGISTERS; i++) {
        rf->regs[i] = 0.0;
    }
    
    // Inicializar program counter en 0
    rf->pc = 0;
}

double reg_read(RegisterFile* rf, int reg_id) {
    // Validación de rango
    if (reg_id < 0 || reg_id >= NUM_REGISTERS) {
        printf("[RegisterFile] ERROR: reg_id=%d fuera de rango [0-%d]\n", 
               reg_id, NUM_REGISTERS - 1);
        return 0.0;
    }
    
    return rf->regs[reg_id];
}

void reg_write(RegisterFile* rf, int reg_id, double value) {
    // Validación de rango
    if (reg_id < 0 || reg_id >= NUM_REGISTERS) {
        printf("[RegisterFile] ERROR: reg_id=%d fuera de rango [0-%d]\n", 
               reg_id, NUM_REGISTERS - 1);
        return;
    }
    
    rf->regs[reg_id] = value;
}

void reg_print(RegisterFile* rf, int pe_id) {
    printf("\n========== PE%d Register File ==========\n", pe_id);
    printf("PC: %lu\n", rf->pc);
    printf("Registers:\n");
    
    for (int i = 0; i < NUM_REGISTERS; i++) {
        printf("  R%d: %.6f", i, rf->regs[i]);
        
        // Imprimir 4 registros por línea para mejor formato
        if ((i + 1) % 4 == 0) {
            printf("\n");
        } else {
            printf("\t");
        }
    }
    
    if (NUM_REGISTERS % 4 != 0) {
        printf("\n");
    }
    
    printf("========================================\n\n");
}

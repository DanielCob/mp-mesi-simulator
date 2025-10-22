#define LOG_MODULE "REGS"
#include "registers.h"
#include <stdio.h>
#include <string.h>
#include "log.h"

void reg_init(RegisterFile* rf) {
    // Initialize all registers to 0.0
    for (int i = 0; i < NUM_REGISTERS; i++) {
        rf->regs[i] = 0.0;
    }
    
    // Initialize program counter to 0
    rf->pc = 0;
    
    // Initialize zero flag to 1 (since all registers are 0)
    rf->zero_flag = 1;
}

double reg_read(RegisterFile* rf, int reg_id) {
    // Range validation
    if (reg_id < 0 || reg_id >= NUM_REGISTERS) {
        LOGE("reg_id=%d out of range [0-%d]", reg_id, NUM_REGISTERS - 1);
        return 0.0;
    }
    
    return rf->regs[reg_id];
}

void reg_write(RegisterFile* rf, int reg_id, double value) {
    // Range validation
    if (reg_id < 0 || reg_id >= NUM_REGISTERS) {
        LOGE("reg_id=%d out of range [0-%d]", reg_id, NUM_REGISTERS - 1);
        return;
    }
    
    rf->regs[reg_id] = value;
}

void reg_update_zero_flag(RegisterFile* rf, double value) {
    // Update zero flag: 1 if value is 0.0, else 0
    rf->zero_flag = (value == 0.0) ? 1 : 0;
}

void reg_print(RegisterFile* rf, int pe_id) {
    printf("\n[PE%d registers]\n", pe_id);
    printf("PC: %lu\n", rf->pc);
    printf("Zero Flag: %d\n", rf->zero_flag);
    printf("Registers:\n");
    
    for (int i = 0; i < NUM_REGISTERS; i++) {
        printf("  R%d: %.6f", i, rf->regs[i]);
        
        // Print 4 registers per line for better formatting
        if ((i + 1) % 4 == 0) {
            printf("\n");
        } else {
            printf("\t");
        }
    }
    
    if (NUM_REGISTERS % 4 != 0) {
        printf("\n");
    }
    
    printf("\n");
}

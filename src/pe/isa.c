#include "isa.h"
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>  // Para sched_yield()

const char* opcode_to_str(OpCode op) {
    switch (op) {
        case OP_MOV:   return "MOV";
        case OP_LOAD:  return "LOAD";
        case OP_STORE: return "STORE";
        case OP_FADD:  return "FADD";
        case OP_FMUL:  return "FMUL";
        case OP_INC:   return "INC";
        case OP_DEC:   return "DEC";
        case OP_JNZ:   return "JNZ";
        case OP_HALT:  return "HALT";
        default:       return "UNKNOWN";
    }
}

void print_instruction(Instruction* inst, uint64_t pc) {
    printf("[PC=%lu] ", pc);
    
    switch (inst->op) {
        case OP_MOV:
            printf("MOV R%d, %.2f", inst->rd, inst->imm);
            break;
        case OP_LOAD:
            if (inst->addr_mode == ADDR_DIRECT) {
                printf("LOAD R%d, [%d]", inst->rd, inst->addr);
            } else {
                printf("LOAD R%d, [R%d]", inst->rd, inst->addr_reg);
            }
            break;
        case OP_STORE:
            if (inst->addr_mode == ADDR_DIRECT) {
                printf("STORE R%d, [%d]", inst->rd, inst->addr);
            } else {
                printf("STORE R%d, [R%d]", inst->rd, inst->addr_reg);
            }
            break;
        case OP_FADD:
            printf("FADD R%d, R%d, R%d", inst->rd, inst->ra, inst->rb);
            break;
        case OP_FMUL:
            printf("FMUL R%d, R%d, R%d", inst->rd, inst->ra, inst->rb);
            break;
        case OP_INC:
            printf("INC R%d", inst->rd);
            break;
        case OP_DEC:
            printf("DEC R%d", inst->rd);
            break;
        case OP_JNZ:
            printf("JNZ %d", inst->label);
            break;
        case OP_HALT:
            printf("HALT");
            break;
        default:
            printf("UNKNOWN");
            break;
    }
    printf("\n");
}

int execute_instruction(Instruction* inst, RegisterFile* rf, Cache* cache, int pe_id) {
    double val_a, val_b, result;
    int effective_addr;
    
    // Imprimir instrucción que se va a ejecutar
    printf("[PE%d] Ejecutando: ", pe_id);
    print_instruction(inst, rf->pc);
    
    switch (inst->op) {
        case OP_MOV:
            // MOV Rd, imm - Cargar valor inmediato al registro
            printf("  [PE%d] MOV: %.6f -> R%d\n", pe_id, inst->imm, inst->rd);
            reg_write(rf, inst->rd, inst->imm);
            rf->pc++;
            break;
            
        case OP_LOAD:
            // LOAD Rd, [addr] o LOAD Rd, [Rx] - Leer de memoria al registro
            // Calcular dirección efectiva según el modo de direccionamiento
            if (inst->addr_mode == ADDR_DIRECT) {
                effective_addr = inst->addr;
                printf("  [PE%d] LOAD: Leyendo memoria[%d] -> R%d\n", pe_id, effective_addr, inst->rd);
            } else {
                effective_addr = (int)reg_read(rf, inst->addr_reg);
                printf("  [PE%d] LOAD: Leyendo memoria[R%d=%d] -> R%d\n", 
                       pe_id, inst->addr_reg, effective_addr, inst->rd);
            }
            
            result = cache_read(cache, effective_addr, pe_id);
            reg_write(rf, inst->rd, result);
            printf("  [PE%d] R%d = %.6f\n", pe_id, inst->rd, result);
            rf->pc++;
            break;
            
        case OP_STORE:
            // STORE Rs, [addr] o STORE Rs, [Rx] - Escribir registro a memoria
            val_a = reg_read(rf, inst->rd);
            
            // Calcular dirección efectiva según el modo de direccionamiento
            if (inst->addr_mode == ADDR_DIRECT) {
                effective_addr = inst->addr;
                printf("  [PE%d] STORE: R%d (%.6f) -> memoria[%d]\n", 
                       pe_id, inst->rd, val_a, effective_addr);
            } else {
                effective_addr = (int)reg_read(rf, inst->addr_reg);
                printf("  [PE%d] STORE: R%d (%.6f) -> memoria[R%d=%d]\n", 
                       pe_id, inst->rd, val_a, inst->addr_reg, effective_addr);
            }
            
            cache_write(cache, effective_addr, val_a, pe_id);
            rf->pc++;
            break;
            
        case OP_FADD:
            // FADD Rd, Ra, Rb - Suma flotante
            val_a = reg_read(rf, inst->ra);
            val_b = reg_read(rf, inst->rb);
            result = val_a + val_b;
            printf("  [PE%d] FADD: R%d (%.6f) + R%d (%.6f) = %.6f -> R%d\n", 
                   pe_id, inst->ra, val_a, inst->rb, val_b, result, inst->rd);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_FMUL:
            // FMUL Rd, Ra, Rb - Multiplicación flotante
            val_a = reg_read(rf, inst->ra);
            val_b = reg_read(rf, inst->rb);
            result = val_a * val_b;
            printf("  [PE%d] FMUL: R%d (%.6f) * R%d (%.6f) = %.6f -> R%d\n", 
                   pe_id, inst->ra, val_a, inst->rb, val_b, result, inst->rd);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_INC:
            // INC Rd - Incrementar registro
            val_a = reg_read(rf, inst->rd);
            result = val_a + 1.0;
            printf("  [PE%d] INC: R%d (%.6f) + 1 = %.6f\n", pe_id, inst->rd, val_a, result);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_DEC:
            // DEC Rd - Decrementar registro
            val_a = reg_read(rf, inst->rd);
            result = val_a - 1.0;
            printf("  [PE%d] DEC: R%d (%.6f) - 1 = %.6f\n", pe_id, inst->rd, val_a, result);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_JNZ:
            // JNZ label - Saltar si zero_flag == 0 (última operación != 0)
            if (rf->zero_flag == 0) {
                printf("  [PE%d] JNZ: zero_flag=0 (última op != 0), saltando a PC=%d\n", 
                       pe_id, inst->label);
                rf->pc = inst->label;
            } else {
                printf("  [PE%d] JNZ: zero_flag=1 (última op == 0), no salta\n", pe_id);
                rf->pc++;
            }
            break;
            
        case OP_HALT:
            // HALT - Terminar ejecución
            // Hacer writeback de todas las líneas modificadas a través del bus
            printf("  [PE%d] HALT: Haciendo writeback de líneas modificadas...\n", pe_id);
            cache_flush(cache, pe_id);
            printf("  [PE%d] HALT: Terminando ejecución\n", pe_id);
            return 0;  // Señal para detener ejecución
            
        default:
            printf("  [PE%d] ERROR: OpCode desconocido %d\n", pe_id, inst->op);
            return 0;
    }
    
    // Ceder el procesador para simular el tiempo de ejecución de la instrucción
    // y permitir que otros PEs ejecuten (scheduling justo)
    sched_yield();
    
    return 1;  // Continuar ejecución
}

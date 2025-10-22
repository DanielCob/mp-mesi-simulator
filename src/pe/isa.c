#define LOG_MODULE "ISA"
#include "isa.h"
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>  // Para sched_yield()
#include "log.h"

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
    // Traza concisa de la instrucción (nivel DEBUG)
    switch (inst->op) {
        case OP_MOV:
            LOGD("PC=%lu MOV R%d, %.2f", pc, inst->rd, inst->imm);
            break;
        case OP_LOAD:
            if (inst->addr_mode == ADDR_DIRECT) {
                LOGD("PC=%lu LOAD R%d, [%d]", pc, inst->rd, inst->addr);
            } else {
                LOGD("PC=%lu LOAD R%d, [R%d]", pc, inst->rd, inst->addr_reg);
            }
            break;
        case OP_STORE:
            if (inst->addr_mode == ADDR_DIRECT) {
                LOGD("PC=%lu STORE R%d, [%d]", pc, inst->rd, inst->addr);
            } else {
                LOGD("PC=%lu STORE R%d, [R%d]", pc, inst->rd, inst->addr_reg);
            }
            break;
        case OP_FADD:
            LOGD("PC=%lu FADD R%d, R%d, R%d", pc, inst->rd, inst->ra, inst->rb);
            break;
        case OP_FMUL:
            LOGD("PC=%lu FMUL R%d, R%d, R%d", pc, inst->rd, inst->ra, inst->rb);
            break;
        case OP_INC:
            LOGD("PC=%lu INC R%d", pc, inst->rd);
            break;
        case OP_DEC:
            LOGD("PC=%lu DEC R%d", pc, inst->rd);
            break;
        case OP_JNZ:
            LOGD("PC=%lu JNZ %d", pc, inst->label);
            break;
        case OP_HALT:
            LOGD("PC=%lu HALT", pc);
            break;
        default:
            LOGD("PC=%lu UNKNOWN", pc);
            break;
    }
}

int execute_instruction(Instruction* inst, RegisterFile* rf, Cache* cache, int pe_id) {
    double val_a, val_b, result;
    int effective_addr;
    
    // Traza de instrucción (DEBUG)
    LOGD("PE%d: ejecutando instrucción", pe_id);
    print_instruction(inst, rf->pc);
    
    switch (inst->op) {
        case OP_MOV:
            // MOV Rd, imm - Cargar valor inmediato al registro
            LOGD("PE%d: MOV %.6f -> R%d", pe_id, inst->imm, inst->rd);
            reg_write(rf, inst->rd, inst->imm);
            rf->pc++;
            break;
            
        case OP_LOAD:
            // LOAD Rd, [addr] o LOAD Rd, [Rx] - Leer de memoria al registro
            // Calcular dirección efectiva según el modo de direccionamiento
            if (inst->addr_mode == ADDR_DIRECT) {
                effective_addr = inst->addr;
                LOGD("PE%d: LOAD memoria[%d] -> R%d", pe_id, effective_addr, inst->rd);
            } else {
                effective_addr = (int)reg_read(rf, inst->addr_reg);
                LOGD("PE%d: LOAD memoria[R%d=%d] -> R%d", pe_id, inst->addr_reg, effective_addr, inst->rd);
            }
            
            result = cache_read(cache, effective_addr, pe_id);
            reg_write(rf, inst->rd, result);
            LOGD("PE%d: R%d = %.6f", pe_id, inst->rd, result);
            rf->pc++;
            break;
            
        case OP_STORE:
            // STORE Rs, [addr] o STORE Rs, [Rx] - Escribir registro a memoria
            val_a = reg_read(rf, inst->rd);
            
            // Calcular dirección efectiva según el modo de direccionamiento
            if (inst->addr_mode == ADDR_DIRECT) {
                effective_addr = inst->addr;
          LOGD("PE%d: STORE R%d (%.6f) -> memoria[%d]", pe_id, inst->rd, val_a, effective_addr);
            } else {
                effective_addr = (int)reg_read(rf, inst->addr_reg);
          LOGD("PE%d: STORE R%d (%.6f) -> memoria[R%d=%d]", pe_id, inst->rd, val_a, inst->addr_reg, effective_addr);
            }
            
            cache_write(cache, effective_addr, val_a, pe_id);
            rf->pc++;
            break;
            
        case OP_FADD:
            // FADD Rd, Ra, Rb - Suma flotante
            val_a = reg_read(rf, inst->ra);
            val_b = reg_read(rf, inst->rb);
            result = val_a + val_b;
         LOGD("PE%d: FADD R%d (%.6f) + R%d (%.6f) = %.6f -> R%d", pe_id, inst->ra, val_a, inst->rb, val_b, result, inst->rd);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_FMUL:
            // FMUL Rd, Ra, Rb - Multiplicación flotante
            val_a = reg_read(rf, inst->ra);
            val_b = reg_read(rf, inst->rb);
            result = val_a * val_b;
         LOGD("PE%d: FMUL R%d (%.6f) * R%d (%.6f) = %.6f -> R%d", pe_id, inst->ra, val_a, inst->rb, val_b, result, inst->rd);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_INC:
            // INC Rd - Incrementar registro
            val_a = reg_read(rf, inst->rd);
            result = val_a + 1.0;
            LOGD("PE%d: INC R%d (%.6f) + 1 = %.6f", pe_id, inst->rd, val_a, result);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_DEC:
            // DEC Rd - Decrementar registro
            val_a = reg_read(rf, inst->rd);
            result = val_a - 1.0;
            LOGD("PE%d: DEC R%d (%.6f) - 1 = %.6f", pe_id, inst->rd, val_a, result);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_JNZ:
            // JNZ label - Saltar si zero_flag == 0 (última operación != 0)
            if (rf->zero_flag == 0) {
                LOGD("PE%d: JNZ zero_flag=0, salto a PC=%d", pe_id, inst->label);
                rf->pc = inst->label;
            } else {
                LOGD("PE%d: JNZ zero_flag=1, no salta", pe_id);
                rf->pc++;
            }
            break;
            
        case OP_HALT:
            // HALT - Terminar ejecución
            // Hacer writeback de todas las líneas modificadas a través del bus
            LOGD("PE%d: HALT writeback de líneas modificadas", pe_id);
            cache_flush(cache, pe_id);
            LOGD("PE%d: HALT fin de ejecución", pe_id);
            return 0;  // Señal para detener ejecución
            
        default:
            LOGE("PE%d: opcode desconocido %d", pe_id, inst->op);
            return 0;
    }
    
    // Ceder el procesador para simular el tiempo de ejecución de la instrucción
    // y permitir que otros PEs ejecuten (scheduling justo)
    sched_yield();
    
    return 1;  // Continuar ejecución
}

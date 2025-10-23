#define LOG_MODULE "ISA"
#include "isa.h"
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>  // for sched_yield()
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
    // Instruction trace (DEBUG level)
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
    
    // Optional instruction trace; keep concise to avoid noise
    LOGD("PE%d: executing instruction", pe_id);
    print_instruction(inst, rf->pc);
    
    switch (inst->op) {
        case OP_MOV:
            // MOV Rd, imm - load immediate into register
            LOGD("PE%d: MOV %.6f -> R%d", pe_id, inst->imm, inst->rd);
            reg_write(rf, inst->rd, inst->imm);
            rf->pc++;
            break;
            
        case OP_LOAD:
            // LOAD Rd, [addr] or LOAD Rd, [Rx] - read from memory to register
            // Compute effective address by addressing mode
            if (inst->addr_mode == ADDR_DIRECT) {
                effective_addr = inst->addr;
                LOGD("PE%d: LOAD memory[0x%X] -> R%d", pe_id, effective_addr, inst->rd);
            } else {
                effective_addr = (int)reg_read(rf, inst->addr_reg);
                LOGD("PE%d: LOAD memory[R%d=0x%X] -> R%d", pe_id, inst->addr_reg, effective_addr, inst->rd);
            }
            
            result = cache_read(cache, effective_addr, pe_id);
            reg_write(rf, inst->rd, result);
            LOGD("PE%d: R%d = %.6f", pe_id, inst->rd, result);
            rf->pc++;
            break;
            
        case OP_STORE:
            // STORE Rs, [addr] or STORE Rs, [Rx] - write register to memory
            val_a = reg_read(rf, inst->rd);
            
            // Compute effective address by addressing mode
            if (inst->addr_mode == ADDR_DIRECT) {
                effective_addr = inst->addr;
                LOGD("PE%d: STORE R%d (%.6f) -> memory[0x%X]", pe_id, inst->rd, val_a, effective_addr);
            } else {
                effective_addr = (int)reg_read(rf, inst->addr_reg);
                LOGD("PE%d: STORE R%d (%.6f) -> memory[R%d=0x%X]", pe_id, inst->rd, val_a, inst->addr_reg, effective_addr);
            }
            
            cache_write(cache, effective_addr, val_a, pe_id);
            rf->pc++;
            break;
            
        case OP_FADD:
            // FADD Rd, Ra, Rb - floating addition
            val_a = reg_read(rf, inst->ra);
            val_b = reg_read(rf, inst->rb);
            result = val_a + val_b;
         LOGD("PE%d: FADD R%d (%.6f) + R%d (%.6f) = %.6f -> R%d", pe_id, inst->ra, val_a, inst->rb, val_b, result, inst->rd);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_FMUL:
            // FMUL Rd, Ra, Rb - floating multiply
            val_a = reg_read(rf, inst->ra);
            val_b = reg_read(rf, inst->rb);
            result = val_a * val_b;
         LOGD("PE%d: FMUL R%d (%.6f) * R%d (%.6f) = %.6f -> R%d", pe_id, inst->ra, val_a, inst->rb, val_b, result, inst->rd);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_INC:
            // INC Rd - increment register
            val_a = reg_read(rf, inst->rd);
            result = val_a + 1.0;
            LOGD("PE%d: INC R%d (%.6f) + 1 = %.6f", pe_id, inst->rd, val_a, result);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_DEC:
            // DEC Rd - decrement register
            val_a = reg_read(rf, inst->rd);
            result = val_a - 1.0;
            LOGD("PE%d: DEC R%d (%.6f) - 1 = %.6f", pe_id, inst->rd, val_a, result);
            reg_write(rf, inst->rd, result);
            reg_update_zero_flag(rf, result);
            rf->pc++;
            break;
            
        case OP_JNZ:
            // JNZ label - jump if zero_flag == 0 (last result != 0)
            if (rf->zero_flag == 0) {
                LOGD("PE%d: JNZ zero_flag=0, jumping to PC=%d", pe_id, inst->label);
                rf->pc = inst->label;
            } else {
                LOGD("PE%d: JNZ zero_flag=1, no jump", pe_id);
                rf->pc++;
            }
            break;
            
        case OP_HALT:
            // HALT - end execution; write back modified lines via bus
            LOGD("PE%d: HALT writeback of modified lines", pe_id);
            cache_flush(cache, pe_id);
            LOGD("PE%d: HALT end of execution", pe_id);
            return 0;  // Stop execution
            
        default:
            LOGE("PE%d: unknown opcode %d", pe_id, inst->op);
            return 0;
    }
    
    // Yield CPU to simulate instruction time and allow fair scheduling
    sched_yield();
    
    return 1;  // Continue execution
}

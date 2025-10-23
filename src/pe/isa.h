#ifndef ISA_H
#define ISA_H

#include "registers.h"
#include "cache.h"

/**
 * @brief ISA operation codes
 */
typedef enum {
    OP_MOV,    // MOV Rd, imm        - Load immediate value into register
    OP_LOAD,   // LOAD Rd, [addr]    - Load from memory to register (direct)
               // LOAD Rd, [Rx]      - Load from memory to register (indirect)
    OP_STORE,  // STORE Rs, [addr]   - Store register to memory (direct)
               // STORE Rs, [Rx]     - Store register to memory (indirect)
    OP_FADD,   // FADD Rd, Ra, Rb    - Floating add: Rd = Ra + Rb
    OP_FMUL,   // FMUL Rd, Ra, Rb    - Floating multiply: Rd = Ra * Rb
    OP_INC,    // INC Rd             - Increment: Rd = Rd + 1
    OP_DEC,    // DEC Rd             - Decrement: Rd = Rd - 1
    OP_JNZ,    // JNZ label          - Jump if zero_flag == 0
    OP_HALT    // HALT               - Terminate execution
} OpCode;

/**
 * @brief Addressing modes for LOAD/STORE
 */
typedef enum {
    ADDR_DIRECT,   // [addr]    - Immediate address (numeric value)
    ADDR_REGISTER  // [Rx]      - Address in register (indirect)
} AddressingMode;

/**
 * @brief Instruction structure
 *
 * Represents an ISA instruction with all possible operands
 */
typedef struct {
    OpCode op;              // Operation code
    int rd;                 // Destination register
    int ra;                 // Source register A
    int rb;                 // Source register B
    double imm;             // Immediate value (for MOV)
    int addr;               // Immediate address (for LOAD/STORE direct)
    int addr_reg;           // Register with address (for LOAD/STORE indirect)
    AddressingMode addr_mode; // Addressing mode (direct or indirect)
    int label;              // Jump target (for JNZ)
} Instruction;

/**
 * @brief Executable program
 *
 * Contains an array of instructions and its size
 */
typedef struct {
    Instruction* code;  // Instruction array
    int size;           // Number of instructions
} Program;

/**
 * @brief Execute a single instruction
 *
 * @param inst Pointer to instruction to execute
 * @param rf Pointer to register file
 * @param cache Pointer to cache
 * @param pe_id Processor ID (for debug messages)
 * @return int 1 to continue, 0 if HALT
 */
int execute_instruction(Instruction* inst, RegisterFile* rf, Cache* cache, int pe_id);

/**
 * @brief Convert an OpCode to string (for debugging)
 *
 * @param op Operation code
 * @return const char* Instruction name
 */
const char* opcode_to_str(OpCode op);

/**
 * @brief Print an instruction (for debugging)
 *
 * @param inst Pointer to instruction
 * @param pc Current program counter value
 */
void print_instruction(Instruction* inst, uint64_t pc);

#endif // ISA_H

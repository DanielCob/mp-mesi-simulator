#ifndef ISA_H
#define ISA_H

#include "pe/registers.h"
#include "cache/cache.h"

/**
 * @brief Códigos de operación de la ISA
 */
typedef enum {
    OP_LOAD,   // LOAD Rd, [addr]   - Lee de memoria a registro
    OP_STORE,  // STORE Rs, [addr]  - Escribe registro a memoria
    OP_FADD,   // FADD Rd, Ra, Rb   - Suma flotante: Rd = Ra + Rb
    OP_FMUL,   // FMUL Rd, Ra, Rb   - Multiplicación: Rd = Ra * Rb
    OP_INC,    // INC Rd             - Incremento: Rd = Rd + 1
    OP_DEC,    // DEC Rd             - Decremento: Rd = Rd - 1
    OP_JNZ,    // JNZ Rd, label      - Salto si Rd != 0
    OP_HALT    // HALT               - Termina ejecución
} OpCode;

/**
 * @brief Estructura de una instrucción
 * 
 * Representa una instrucción del ISA con todos sus operandos posibles
 */
typedef struct {
    OpCode op;      // Código de operación
    int rd;         // Registro destino
    int ra;         // Registro fuente A
    int rb;         // Registro fuente B
    int addr;       // Dirección de memoria (para LOAD/STORE)
    int label;      // Etiqueta de salto (para JNZ)
} Instruction;

/**
 * @brief Programa ejecutable
 * 
 * Contiene un arreglo de instrucciones y su tamaño
 */
typedef struct {
    Instruction* code;  // Arreglo de instrucciones
    int size;           // Número de instrucciones
} Program;

/**
 * @brief Ejecuta una instrucción
 * 
 * @param inst Puntero a la instrucción a ejecutar
 * @param rf Puntero al banco de registros
 * @param cache Puntero al cache
 * @param pe_id ID del procesador (para mensajes de debug)
 * @return int 1 si debe continuar, 0 si es HALT
 */
int execute_instruction(Instruction* inst, RegisterFile* rf, Cache* cache, int pe_id);

/**
 * @brief Convierte un OpCode a string (para debugging)
 * 
 * @param op Código de operación
 * @return const char* Nombre de la instrucción
 */
const char* opcode_to_str(OpCode op);

/**
 * @brief Imprime una instrucción (para debugging)
 * 
 * @param inst Puntero a la instrucción
 * @param pc Valor del program counter actual
 */
void print_instruction(Instruction* inst, uint64_t pc);

#endif // ISA_H

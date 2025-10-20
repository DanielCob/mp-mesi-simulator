#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>

#define NUM_REGISTERS 8

/**
 * @brief Banco de registros de propósito general
 * 
 * Contiene 8 registros de 64 bits (double precision floating point),
 * un program counter para seguimiento de instrucciones,
 * y una bandera de cero que se actualiza automáticamente
 */
typedef struct {
    double regs[NUM_REGISTERS];  // REG0 - REG7 (64 bits cada uno)
    uint64_t pc;                 // Program Counter
    int zero_flag;               // Bandera de cero: 1 si última operación = 0, 0 en caso contrario
} RegisterFile;

/**
 * @brief Inicializa el banco de registros
 * 
 * Pone todos los registros en 0.0 y el PC en 0
 * 
 * @param rf Puntero al banco de registros
 */
void reg_init(RegisterFile* rf);

/**
 * @brief Lee el valor de un registro
 * 
 * @param rf Puntero al banco de registros
 * @param reg_id ID del registro (0-7)
 * @return double Valor del registro
 */
double reg_read(RegisterFile* rf, int reg_id);

/**
 * @brief Escribe un valor en un registro
 * 
 * @param rf Puntero al banco de registros
 * @param reg_id ID del registro (0-7)
 * @param value Valor a escribir
 */
void reg_write(RegisterFile* rf, int reg_id, double value);

/**
 * @brief Actualiza la bandera de cero basándose en un valor
 * 
 * @param rf Puntero al banco de registros
 * @param value Valor para verificar
 */
void reg_update_zero_flag(RegisterFile* rf, double value);

/**
 * @brief Imprime el estado de todos los registros (para debugging)
 * 
 * @param rf Puntero al banco de registros
 * @param pe_id ID del procesador (para mensajes)
 */
void reg_print(RegisterFile* rf, int pe_id);

#endif // REGISTERS_H

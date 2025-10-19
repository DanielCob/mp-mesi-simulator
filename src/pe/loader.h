#ifndef LOADER_H
#define LOADER_H

#include "isa.h"

/**
 * @file loader.h
 * @brief Cargador de programas assembly desde archivos de texto
 * 
 * Formato de entrada esperado:
 * - Una instrucción por línea
 * - Formato: OPCODE [operandos]
 * 
 * Ejemplos:
 *   LOAD R0 100      # Carga desde dirección 100 a R0
 *   LOAD R1 104      # Carga desde dirección 104 a R1
 *   FADD R2 R0 R1    # R2 = R0 + R1
 *   FMUL R3 R2 R0    # R3 = R2 * R0
 *   STORE R2 200     # Guarda R2 en dirección 200
 *   INC R0           # Incrementa R0
 *   DEC R1           # Decrementa R1
 *   JNZ R0 5         # Salta a línea 5 si R0 != 0
 *   HALT             # Termina ejecución
 * 
 * Notas:
 * - Líneas vacías y comentarios (que empiezan con #) son ignorados
 * - Los registros se especifican como R0-R7
 * - Las direcciones de memoria y labels son números decimales
 */

/**
 * @brief Carga un programa assembly desde un archivo de texto
 * 
 * @param filename Ruta al archivo que contiene el código assembly
 * @return Program estructura con el código cargado, o NULL en caso de error
 * 
 * La memoria del Program debe ser liberada con free_program()
 */
Program* load_program(const char* filename);

/**
 * @brief Libera la memoria de un programa cargado
 * 
 * @param prog Puntero al programa a liberar
 */
void free_program(Program* prog);

/**
 * @brief Imprime un programa cargado (útil para debugging)
 * 
 * @param prog Programa a imprimir
 */
void print_program(const Program* prog);

#endif // LOADER_H

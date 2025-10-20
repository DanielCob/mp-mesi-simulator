#ifndef DOTPROD_H
#define DOTPROD_H

#include "memory.h"

/**
 * @brief Inicializa los vectores A y B en memoria para el producto punto
 * 
 * Vector A: direcciones 0-15
 * Vector B: direcciones 100-115
 * Resultados parciales: direcciones 200-203
 * Resultado final: dirección 204
 * 
 * @param mem Puntero a la estructura Memory
 */
void dotprod_init_data(Memory* mem);

/**
 * @brief Verifica el resultado del producto punto
 * 
 * @param mem Puntero a la estructura Memory
 * @return El valor del producto punto calculado en addr 204
 */
double dotprod_get_result(Memory* mem);

/**
 * @brief Imprime los vectores y resultados del producto punto
 * Incluye flush automático de cachés para asegurar coherencia
 * 
 * @param mem Puntero a la estructura Memory
 * @param caches Array de cachés de los PEs
 */
void dotprod_print_results(Memory* mem);

#endif // DOTPROD_H

#ifndef DOTPROD_H
#define DOTPROD_H

#include "memory.h"

/**
 * @brief Initialize vectors A and B in memory for the dot product
 * 
 * Vector A: addresses 0-15
 * Vector B: addresses 100-115
 * Partial results: addresses 200-203
 * Final result: address 204
 * 
 * @param mem Pointer to Memory structure
 */
void dotprod_init_data(Memory* mem);

/**
 * @brief Get the calculated dot product result
 * 
 * @param mem Pointer to Memory structure
 * @return The dot product value stored at addr 204
 */
double dotprod_get_result(Memory* mem);

/**
 * @brief Print vectors and dot product results
 * Includes automatic cache writeback to ensure coherence
 * 
 * @param mem Pointer to Memory structure
 */
void dotprod_print_results(Memory* mem);

#endif // DOTPROD_H

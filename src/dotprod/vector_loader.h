#ifndef VECTOR_LOADER_H
#define VECTOR_LOADER_H

#include <stdbool.h>

// Result of loading a vector
typedef struct {
    bool success;
    int values_read;
    char error_message[256];
} VectorLoadResult;

/**
 * Load a vector from a CSV file
 * 
 * @param filename   Path to the CSV file
 * @param buffer     Buffer to store the values
 * @param max_size   Maximum buffer size
 * @return           Operation result
 * 
 * Supported formats:
 *   - One line with comma-separated values: 1.0, 2.0, 3.0
 *   - Multiple lines, one value per line
 *   - A mix of both
 */
VectorLoadResult load_vector_from_csv(const char* filename, double* buffer, int max_size);

/**
 * Print the contents of a vector in a readable format
 * 
 * @param name       Vector name (for the message)
 * @param buffer     Buffer with values
 * @param size       Number of values to print
 * @param start_addr Initial memory address
 */
void print_vector(const char* name, const double* buffer, int size, int start_addr);

#endif // VECTOR_LOADER_H

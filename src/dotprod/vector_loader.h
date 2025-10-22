#ifndef VECTOR_LOADER_H
#define VECTOR_LOADER_H

#include <stdbool.h>

// Resultado de la carga de un vector
typedef struct {
    bool success;
    int values_read;
    char error_message[256];
} VectorLoadResult;

/**
 * Carga un vector desde un archivo CSV
 * 
 * @param filename   Ruta al archivo CSV
 * @param buffer     Buffer donde almacenar los valores
 * @param max_size   Tamaño máximo del buffer
 * @return           Resultado de la operación
 * 
 * Formatos soportados:
 *   - Una línea con valores separados por comas: 1.0, 2.0, 3.0
 *   - Múltiples líneas, un valor por línea
 *   - Mezcla de ambos
 */
VectorLoadResult load_vector_from_csv(const char* filename, double* buffer, int max_size);

/**
 * Imprime el contenido de un vector en formato legible
 * 
 * @param name       Nombre del vector (para el mensaje)
 * @param buffer     Buffer con los valores
 * @param size       Cantidad de valores a imprimir
 * @param start_addr Dirección de memoria inicial
 */
void print_vector(const char* name, const double* buffer, int size, int start_addr);

#endif // VECTOR_LOADER_H

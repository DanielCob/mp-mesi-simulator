#include "vector_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Tamaño máximo de línea que se puede leer
#define MAX_LINE_LENGTH 4096

/**
 * Elimina espacios en blanco al inicio y final de una cadena
 */
static char* trim(char* str) {
    char* end;
    
    // Eliminar espacios al inicio
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0) return str;
    
    // Eliminar espacios al final
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    end[1] = '\0';
    return str;
}

/**
 * Parsea una línea del CSV y extrae los valores
 */
static int parse_csv_line(const char* line, double* buffer, int buffer_size, int current_count) {
    char line_copy[MAX_LINE_LENGTH];
    strncpy(line_copy, line, MAX_LINE_LENGTH - 1);
    line_copy[MAX_LINE_LENGTH - 1] = '\0';
    
    int count = current_count;
    char* token = strtok(line_copy, ",");
    
    while (token != NULL && count < buffer_size) {
        token = trim(token);
        
        // Ignorar tokens vacíos
        if (strlen(token) > 0) {
            char* endptr;
            double value = strtod(token, &endptr);
            
            // Verificar si la conversión fue exitosa
            if (endptr != token) {
                buffer[count++] = value;
            }
        }
        
        token = strtok(NULL, ",");
    }
    
    return count;
}

VectorLoadResult load_vector_from_csv(const char* filename, double* buffer, int max_size) {
    VectorLoadResult result = {
        .success = false,
        .values_read = 0,
        .error_message = ""
    };
    
    FILE* file = fopen(filename, "r");
    if (!file) {
        snprintf(result.error_message, sizeof(result.error_message),
                "No se pudo abrir el archivo: %s", filename);
        return result;
    }
    
    char line[MAX_LINE_LENGTH];
    int count = 0;
    int line_number = 0;
    
    while (fgets(line, sizeof(line), file) && count < max_size) {
        line_number++;
        
        // Ignorar líneas vacías y comentarios
        char* trimmed = trim(line);
        if (strlen(trimmed) == 0 || trimmed[0] == '#') {
            continue;
        }
        
        // Parsear la línea
        int old_count = count;
        count = parse_csv_line(trimmed, buffer, max_size, count);
        
        // Si no se leyó ningún valor, reportar advertencia
        if (count == old_count) {
            fprintf(stderr, "[VectorLoader] Warning: línea %d no contiene valores válidos\n", 
                    line_number);
        }
    }
    
    fclose(file);
    
    if (count == 0) {
        snprintf(result.error_message, sizeof(result.error_message),
                "No se encontraron valores válidos en: %s", filename);
        return result;
    }
    
    result.success = true;
    result.values_read = count;
    return result;
}

void print_vector(const char* name, const double* buffer, int size, int start_addr) {
    printf("[DotProd] Loading %s at addresses %d-%d:\n  %s = [", 
           name, start_addr, start_addr + size - 1, name);
    
    for (int i = 0; i < size; i++) {
        printf("%.2f", buffer[i]);
        if (i < size - 1) printf(", ");
    }
    printf("]\n");
}

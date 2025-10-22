#define LOG_MODULE "DOTPROD"
#include "vector_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "log.h"

// Maximum line length to read
#define MAX_LINE_LENGTH 4096

/**
 * Trim whitespace at the beginning and end of a string
 */
static char* trim(char* str) {
    char* end;
    
    // Trim start
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0) return str;
    
    // Trim end
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    end[1] = '\0';
    return str;
}

/**
 * Parse a CSV line and extract values
 */
static int parse_csv_line(const char* line, double* buffer, int buffer_size, int current_count) {
    char line_copy[MAX_LINE_LENGTH];
    strncpy(line_copy, line, MAX_LINE_LENGTH - 1);
    line_copy[MAX_LINE_LENGTH - 1] = '\0';
    
    int count = current_count;
    char* token = strtok(line_copy, ",");
    
    while (token != NULL && count < buffer_size) {
        token = trim(token);
        
        // Ignore empty tokens
        if (strlen(token) > 0) {
            char* endptr;
            double value = strtod(token, &endptr);
            
            // Verify conversion succeeded
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
                "Failed to open file: %s", filename);
        return result;
    }
    
    char line[MAX_LINE_LENGTH];
    int count = 0;
    int line_number = 0;
    
    while (fgets(line, sizeof(line), file) && count < max_size) {
        line_number++;
        
        // Ignore empty lines and comments
        char* trimmed = trim(line);
        if (strlen(trimmed) == 0 || trimmed[0] == '#') {
            continue;
        }
        
        // Parse the line
        int old_count = count;
        count = parse_csv_line(trimmed, buffer, max_size, count);
        
        // If nothing was read, report a warning
        if (count == old_count) {
            LOGW("Line %d has no valid values", line_number);
        }
    }
    
    fclose(file);
    
    if (count == 0) {
        snprintf(result.error_message, sizeof(result.error_message),
                "No valid values found in: %s", filename);
        return result;
    }
    
    result.success = true;
    result.values_read = count;
    return result;
}

void print_vector(const char* name, const double* buffer, int size, int start_addr) {
    printf("[DotProd] Loading %s into addresses 0x%X-0x%X\n  %s = [", 
           name, start_addr, start_addr + size - 1, name);
    
    for (int i = 0; i < size; i++) {
        printf("%.2f", buffer[i]);
        if (i < size - 1) printf(", ");
    }
    printf("]\n");
}

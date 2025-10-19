#include "loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 256
#define INITIAL_CAPACITY 64

/**
 * @brief Convierte OpCode a string (versión local)
 */
static const char* opcode_to_string_local(OpCode op) {
    switch (op) {
        case OP_LOAD:  return "LOAD";
        case OP_STORE: return "STORE";
        case OP_FADD:  return "FADD";
        case OP_FMUL:  return "FMUL";
        case OP_INC:   return "INC";
        case OP_DEC:   return "DEC";
        case OP_JNZ:   return "JNZ";
        case OP_HALT:  return "HALT";
        default:       return "UNKNOWN";
    }
}

/**
 * @brief Elimina espacios en blanco al inicio y final de una cadena
 */
static char* trim(char* str) {
    // Trim inicial
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // Trim final
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    *(end + 1) = '\0';
    return str;
}

/**
 * @brief Convierte un string de opcode a OpCode enum
 */
static OpCode parse_opcode(const char* str) {
    if (strcmp(str, "LOAD") == 0)  return OP_LOAD;
    if (strcmp(str, "STORE") == 0) return OP_STORE;
    if (strcmp(str, "FADD") == 0)  return OP_FADD;
    if (strcmp(str, "FMUL") == 0)  return OP_FMUL;
    if (strcmp(str, "INC") == 0)   return OP_INC;
    if (strcmp(str, "DEC") == 0)   return OP_DEC;
    if (strcmp(str, "JNZ") == 0)   return OP_JNZ;
    if (strcmp(str, "HALT") == 0)  return OP_HALT;
    return OP_HALT; // Default para opcodes no reconocidos
}

/**
 * @brief Parsea un número de registro (R0-R7) a su índice
 */
static int parse_register(const char* str) {
    if (str[0] != 'R' && str[0] != 'r') return -1;
    
    int reg = atoi(str + 1);
    if (reg < 0 || reg > 7) return -1;
    
    return reg;
}

/**
 * @brief Parsea una línea de assembly en una instrucción
 * @return 1 si se parseó exitosamente, 0 si es línea vacía/comentario, -1 si error
 */
static int parse_line(const char* line, Instruction* inst) {
    char line_copy[MAX_LINE_LENGTH];
    strncpy(line_copy, line, MAX_LINE_LENGTH - 1);
    line_copy[MAX_LINE_LENGTH - 1] = '\0';
    
    // Eliminar comentarios
    char* comment = strchr(line_copy, '#');
    if (comment) *comment = '\0';
    
    // Trim
    char* trimmed = trim(line_copy);
    if (strlen(trimmed) == 0) return 0; // Línea vacía
    
    // Parsear opcode
    char opcode_str[16];
    if (sscanf(trimmed, "%15s", opcode_str) != 1) return 0;
    
    OpCode op = parse_opcode(opcode_str);
    
    // Inicializar instrucción con valores por defecto
    inst->op = op;
    inst->rd = 0;
    inst->ra = 0;
    inst->rb = 0;
    inst->addr = 0;
    inst->label = 0;
    
    // Parsear operandos según el tipo de instrucción
    char* operands = trimmed + strlen(opcode_str);
    operands = trim(operands);
    
    char reg1[8], reg2[8], reg3[8];
    int addr, label;
    
    switch (op) {
        case OP_LOAD:
            // LOAD Rd, addr
            if (sscanf(operands, "%7s %d", reg1, &addr) == 2) {
                inst->rd = parse_register(reg1);
                inst->addr = addr;
                if (inst->rd < 0) {
                    fprintf(stderr, "[Loader] ERROR: Registro inválido en LOAD: %s\n", reg1);
                    return -1;
                }
            } else {
                fprintf(stderr, "[Loader] ERROR: Formato inválido para LOAD: %s\n", operands);
                return -1;
            }
            break;
            
        case OP_STORE:
            // STORE Rs, addr
            if (sscanf(operands, "%7s %d", reg1, &addr) == 2) {
                inst->rd = parse_register(reg1);  // rd se usa como source en STORE
                inst->addr = addr;
                if (inst->rd < 0) {
                    fprintf(stderr, "[Loader] ERROR: Registro inválido en STORE: %s\n", reg1);
                    return -1;
                }
            } else {
                fprintf(stderr, "[Loader] ERROR: Formato inválido para STORE: %s\n", operands);
                return -1;
            }
            break;
            
        case OP_FADD:
        case OP_FMUL:
            // FADD/FMUL Rd, Ra, Rb
            if (sscanf(operands, "%7s %7s %7s", reg1, reg2, reg3) == 3) {
                inst->rd = parse_register(reg1);
                inst->ra = parse_register(reg2);
                inst->rb = parse_register(reg3);
                if (inst->rd < 0 || inst->ra < 0 || inst->rb < 0) {
                    fprintf(stderr, "[Loader] ERROR: Registro inválido en %s: %s %s %s\n",
                            opcode_str, reg1, reg2, reg3);
                    return -1;
                }
            } else {
                fprintf(stderr, "[Loader] ERROR: Formato inválido para %s: %s\n", 
                        opcode_str, operands);
                return -1;
            }
            break;
            
        case OP_INC:
        case OP_DEC:
            // INC/DEC Rd
            if (sscanf(operands, "%7s", reg1) == 1) {
                inst->rd = parse_register(reg1);
                if (inst->rd < 0) {
                    fprintf(stderr, "[Loader] ERROR: Registro inválido en %s: %s\n",
                            opcode_str, reg1);
                    return -1;
                }
            } else {
                fprintf(stderr, "[Loader] ERROR: Formato inválido para %s: %s\n",
                        opcode_str, operands);
                return -1;
            }
            break;
            
        case OP_JNZ:
            // JNZ Rd, label
            if (sscanf(operands, "%7s %d", reg1, &label) == 2) {
                inst->rd = parse_register(reg1);
                inst->label = label;
                if (inst->rd < 0) {
                    fprintf(stderr, "[Loader] ERROR: Registro inválido en JNZ: %s\n", reg1);
                    return -1;
                }
            } else {
                fprintf(stderr, "[Loader] ERROR: Formato inválido para JNZ: %s\n", operands);
                return -1;
            }
            break;
            
        case OP_HALT:
            // HALT no tiene operandos
            break;
            
        default:
            fprintf(stderr, "[Loader] ERROR: Opcode no reconocido: %s\n", opcode_str);
            return -1;
    }
    
    return 1; // Éxito
}

Program* load_program(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "[Loader] ERROR: No se pudo abrir el archivo: %s\n", filename);
        return NULL;
    }
    
    // Crear programa con capacidad inicial
    Program* prog = (Program*)malloc(sizeof(Program));
    if (!prog) {
        fprintf(stderr, "[Loader] ERROR: No se pudo asignar memoria para el programa\n");
        fclose(file);
        return NULL;
    }
    
    int capacity = INITIAL_CAPACITY;
    prog->code = (Instruction*)malloc(capacity * sizeof(Instruction));
    if (!prog->code) {
        fprintf(stderr, "[Loader] ERROR: No se pudo asignar memoria para las instrucciones\n");
        free(prog);
        fclose(file);
        return NULL;
    }
    
    prog->size = 0;
    
    // Leer línea por línea
    char line[MAX_LINE_LENGTH];
    int line_num = 0;
    
    printf("[Loader] Cargando programa desde: %s\n", filename);
    
    while (fgets(line, MAX_LINE_LENGTH, file)) {
        line_num++;
        
        Instruction inst;
        int result = parse_line(line, &inst);
        
        if (result < 0) {
            fprintf(stderr, "[Loader] ERROR en línea %d: %s", line_num, line);
            free(prog->code);
            free(prog);
            fclose(file);
            return NULL;
        }
        
        if (result == 0) continue; // Línea vacía o comentario
        
        // Expandir array si es necesario
        if (prog->size >= capacity) {
            capacity *= 2;
            Instruction* new_code = (Instruction*)realloc(prog->code, 
                                                          capacity * sizeof(Instruction));
            if (!new_code) {
                fprintf(stderr, "[Loader] ERROR: No se pudo expandir memoria para instrucciones\n");
                free(prog->code);
                free(prog);
                fclose(file);
                return NULL;
            }
            prog->code = new_code;
        }
        
        prog->code[prog->size++] = inst;
    }
    
    fclose(file);
    
    printf("[Loader] Programa cargado exitosamente: %d instrucciones\n", prog->size);
    
    return prog;
}

void free_program(Program* prog) {
    if (prog) {
        if (prog->code) {
            free(prog->code);
        }
        free(prog);
    }
}

void print_program(const Program* prog) {
    if (!prog) {
        printf("[Loader] Programa NULL\n");
        return;
    }
    
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║           PROGRAMA CARGADO (%d instrucciones)      \n", prog->size);
    printf("╠════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < prog->size; i++) {
        Instruction* inst = &prog->code[i];
        const char* op_name = opcode_to_string_local(inst->op);
        
        printf("║ [%3d] %-8s ", i, op_name);
        
        switch (inst->op) {
            case OP_LOAD:
                printf("R%d, [%d]", inst->rd, inst->addr);
                break;
            case OP_STORE:
                printf("R%d, [%d]", inst->rd, inst->addr);
                break;
            case OP_FADD:
            case OP_FMUL:
                printf("R%d, R%d, R%d", inst->rd, inst->ra, inst->rb);
                break;
            case OP_INC:
            case OP_DEC:
                printf("R%d", inst->rd);
                break;
            case OP_JNZ:
                printf("R%d, %d", inst->rd, inst->label);
                break;
            case OP_HALT:
                break;
        }
        
        printf("\n");
    }
    
    printf("╚════════════════════════════════════════════════════╝\n\n");
}

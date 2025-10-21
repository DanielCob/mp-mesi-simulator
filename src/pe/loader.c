#include "loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 256
#define INITIAL_CAPACITY 64
#define MAX_LABELS 128
#define MAX_LABEL_NAME 64

/**
 * @brief Estructura para almacenar labels
 */
typedef struct {
    char name[MAX_LABEL_NAME];
    int line_number;  // Número de instrucción (no de línea de archivo)
} Label;

/**
 * @brief Tabla de labels
 */
typedef struct {
    Label labels[MAX_LABELS];
    int count;
} LabelTable;

/**
 * @brief Inicializa la tabla de labels
 */
static void init_label_table(LabelTable* table) {
    table->count = 0;
}

/**
 * @brief Añade un label a la tabla
 */
static int add_label(LabelTable* table, const char* name, int line_number) {
    if (table->count >= MAX_LABELS) {
        fprintf(stderr, "[Loader] ERROR: Demasiados labels (máximo %d)\n", MAX_LABELS);
        return -1;
    }
    
    // Verificar si el label ya existe
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->labels[i].name, name) == 0) {
            fprintf(stderr, "[Loader] ERROR: Label duplicado: %s\n", name);
            return -1;
        }
    }
    
    strncpy(table->labels[table->count].name, name, MAX_LABEL_NAME - 1);
    table->labels[table->count].name[MAX_LABEL_NAME - 1] = '\0';
    table->labels[table->count].line_number = line_number;
    table->count++;
    
    return 0;
}

/**
 * @brief Busca un label en la tabla
 * @return Número de línea del label, o -1 si no se encuentra
 */
static int find_label(const LabelTable* table, const char* name) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->labels[i].name, name) == 0) {
            return table->labels[i].line_number;
        }
    }
    return -1;
}

/**
 * @brief Convierte OpCode a string (versión local)
 */
static const char* opcode_to_string_local(OpCode op) {
    switch (op) {
        case OP_MOV:   return "MOV";
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
    if (strcmp(str, "MOV") == 0)   return OP_MOV;
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
 * @brief Verifica si una línea contiene un label (formato "NOMBRE:")
 * @param rest Si no es NULL, almacena el resto de la línea después del ':'
 * @return Puntero al nombre del label (sin el :) o NULL si no es un label
 */
static char* extract_label(char* line, char* label_name, char* rest) {
    char* trimmed = trim(line);
    
    // Buscar el carácter ':'
    char* colon = strchr(trimmed, ':');
    if (!colon) return NULL;
    
    // Extraer el nombre del label (todo antes del ':')
    size_t len = colon - trimmed;
    if (len == 0 || len >= MAX_LABEL_NAME) return NULL;
    
    strncpy(label_name, trimmed, len);
    label_name[len] = '\0';
    
    // Trim el nombre del label
    char* label_trimmed = trim(label_name);
    
    // Verificar que el nombre del label sea válido (solo letras, números y _)
    for (size_t i = 0; i < strlen(label_trimmed); i++) {
        if (!isalnum(label_trimmed[i]) && label_trimmed[i] != '_') {
            return NULL;
        }
    }
    
    // Copiar de vuelta
    if (label_trimmed != label_name) {
        strcpy(label_name, label_trimmed);
    }
    
    // Si se solicita, extraer el resto de la línea después del ':'
    if (rest) {
        char* after_colon = colon + 1;
        strcpy(rest, trim(after_colon));
    }
    
    return label_name;
}

/**
 * @brief Parsea una línea de assembly en una instrucción
 * @param label_table Tabla de labels para resolver referencias
 * @return 1 si se parseó exitosamente, 0 si es línea vacía/comentario/label, -1 si error
 */
static int parse_line(const char* line, Instruction* inst, const LabelTable* label_table) {
    char line_copy[MAX_LINE_LENGTH];
    strncpy(line_copy, line, MAX_LINE_LENGTH - 1);
    line_copy[MAX_LINE_LENGTH - 1] = '\0';
    
    // Eliminar comentarios
    char* comment = strchr(line_copy, '#');
    if (comment) *comment = '\0';
    
    // Verificar si es un label (y omitirlo en la segunda pasada)
    char label_name[MAX_LABEL_NAME];
    char rest[MAX_LINE_LENGTH];
    if (extract_label(line_copy, label_name, rest)) {
        // Es un label, verificar si hay instrucción después del ':'
        if (strlen(rest) == 0) {
            return 0; // Solo label, sin instrucción
        }
        // Hay instrucción después del label, parsear el resto
        strcpy(line_copy, rest);
    }
    
    // Trim
    char* trimmed = trim(line_copy);
    if (strlen(trimmed) == 0) return 0; // Línea vacía
    
    // Parsear opcode
    char opcode_str[16];
    if (sscanf(trimmed, "%15s", opcode_str) != 1) return 0;
    
    OpCode op = parse_opcode(opcode_str);
    
    // Verificar si el opcode es válido
    if (op == OP_HALT && strcmp(opcode_str, "HALT") != 0) {
        // parse_opcode devolvió HALT como default, pero no es un HALT real
        return 0; // Ignorar línea
    }
    
    // Inicializar instrucción con valores por defecto
    inst->op = op;
    inst->rd = 0;
    inst->ra = 0;
    inst->rb = 0;
    inst->imm = 0.0;
    inst->addr = 0;
    inst->addr_reg = 0;
    inst->addr_mode = ADDR_DIRECT;
    inst->label = 0;
    
    // Parsear operandos según el tipo de instrucción
    char* operands = trimmed + strlen(opcode_str);
    operands = trim(operands);
    
    char reg1[8], reg2[8], reg3[8];
    char addr_str[32];
    double imm_value;
    
    switch (op) {
        case OP_MOV:
            // MOV Rd, imm
            if (sscanf(operands, "%7[^,], %lf", reg1, &imm_value) == 2) {
                inst->rd = parse_register(reg1);
                if (inst->rd < 0) {
                    fprintf(stderr, "[Loader] ERROR: Registro inválido en MOV: %s\n", reg1);
                    return -1;
                }
                inst->imm = imm_value;
            } else {
                fprintf(stderr, "[Loader] ERROR: Formato inválido para MOV: %s\n", operands);
                return -1;
            }
            break;
            
        case OP_LOAD:
            // LOAD Rd, [addr] o LOAD Rd, [Rx]
            if (sscanf(operands, "%7[^,], [%31[^]]]", reg1, addr_str) == 2) {
                inst->rd = parse_register(reg1);
                if (inst->rd < 0) {
                    fprintf(stderr, "[Loader] ERROR: Registro inválido en LOAD: %s\n", reg1);
                    return -1;
                }
                
                // Determinar si es directo [número] o indirecto [Rx]
                char* trimmed_addr = trim(addr_str);
                if (trimmed_addr[0] == 'R' || trimmed_addr[0] == 'r') {
                    // Es direccionamiento indirecto [Rx]
                    inst->addr_reg = parse_register(trimmed_addr);
                    if (inst->addr_reg < 0) {
                        fprintf(stderr, "[Loader] ERROR: Registro de dirección inválido en LOAD: [%s]\n", 
                                trimmed_addr);
                        return -1;
                    }
                    inst->addr_mode = ADDR_REGISTER;
                } else {
                    // Es direccionamiento directo [número]
                    inst->addr = atoi(trimmed_addr);
                    inst->addr_mode = ADDR_DIRECT;
                }
            } else {
                fprintf(stderr, "[Loader] ERROR: Formato inválido para LOAD (use [addr] o [Rx]): %s\n", operands);
                return -1;
            }
            break;
            
        case OP_STORE:
            // STORE Rs, [addr] o STORE Rs, [Rx]
            if (sscanf(operands, "%7[^,], [%31[^]]]", reg1, addr_str) == 2) {
                inst->rd = parse_register(reg1);  // rd se usa como source en STORE
                if (inst->rd < 0) {
                    fprintf(stderr, "[Loader] ERROR: Registro inválido en STORE: %s\n", reg1);
                    return -1;
                }
                
                // Determinar si es directo [número] o indirecto [Rx]
                char* trimmed_addr = trim(addr_str);
                if (trimmed_addr[0] == 'R' || trimmed_addr[0] == 'r') {
                    // Es direccionamiento indirecto [Rx]
                    inst->addr_reg = parse_register(trimmed_addr);
                    if (inst->addr_reg < 0) {
                        fprintf(stderr, "[Loader] ERROR: Registro de dirección inválido en STORE: [%s]\n", 
                                trimmed_addr);
                        return -1;
                    }
                    inst->addr_mode = ADDR_REGISTER;
                } else {
                    // Es direccionamiento directo [número]
                    inst->addr = atoi(trimmed_addr);
                    inst->addr_mode = ADDR_DIRECT;
                }
            } else {
                fprintf(stderr, "[Loader] ERROR: Formato inválido para STORE (use [addr] o [Rx]): %s\n", operands);
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
            // JNZ label (usa zero_flag implícitamente)
            {
                char label_str[MAX_LABEL_NAME];
                if (sscanf(operands, "%63s", label_str) == 1) {
                    // No necesita registro, usa zero_flag
                    inst->rd = 0;  // No usado, pero mantener por compatibilidad
                    
                    // Intentar parsear como número
                    char* endptr;
                    long label_num = strtol(label_str, &endptr, 10);
                    
                    if (*endptr == '\0') {
                        // Es un número directo
                        inst->label = (int)label_num;
                    } else {
                        // Es un nombre de label, buscar en la tabla
                        int label_line = find_label(label_table, label_str);
                        if (label_line < 0) {
                            fprintf(stderr, "[Loader] ERROR: Label no encontrado: %s\n", label_str);
                            return -1;
                        }
                        inst->label = label_line;
                    }
                } else {
                    fprintf(stderr, "[Loader] ERROR: Formato inválido para JNZ: %s\n", operands);
                    return -1;
                }
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
    
    printf("[Loader] Cargando programa desde: %s\n", filename);
    
    // ===== PRIMERA PASADA: Recolectar labels =====
    LabelTable label_table;
    init_label_table(&label_table);
    
    char line[MAX_LINE_LENGTH];
    int line_num = 0;
    int instruction_count = 0;
    
    while (fgets(line, MAX_LINE_LENGTH, file)) {
        line_num++;
        
        // Eliminar comentarios
        char line_copy[MAX_LINE_LENGTH];
        strncpy(line_copy, line, MAX_LINE_LENGTH - 1);
        line_copy[MAX_LINE_LENGTH - 1] = '\0';
        
        char* comment = strchr(line_copy, '#');
        if (comment) *comment = '\0';
        
        // Verificar si es un label
        char label_name[MAX_LABEL_NAME];
        char rest[MAX_LINE_LENGTH];
        if (extract_label(line_copy, label_name, rest)) {
            // Es un label, añadirlo a la tabla
            if (add_label(&label_table, label_name, instruction_count) < 0) {
                fclose(file);
                return NULL;
            }
            printf("[Loader]   Label '%s' -> línea %d\n", label_name, instruction_count);
            
            // Verificar si hay una instrucción en la misma línea después del label
            if (strlen(rest) > 0) {
                char opcode[16];
                if (sscanf(rest, "%15s", opcode) == 1) {
                    OpCode op = parse_opcode(opcode);
                    if (op != OP_HALT || strcmp(opcode, "HALT") == 0) {
                        instruction_count++;
                    }
                }
            }
            continue;
        }
        
        // Si no es un label, verificar si es una instrucción válida
        char* trimmed = trim(line_copy);
        if (strlen(trimmed) > 0) {
            // Es una instrucción (no vacía), incrementar contador
            char opcode[16];
            if (sscanf(trimmed, "%15s", opcode) == 1) {
                // Verificar que sea un opcode válido
                OpCode op = parse_opcode(opcode);
                if (op != OP_HALT || strcmp(opcode, "HALT") == 0) {
                    instruction_count++;
                }
            }
        }
    }
    
    printf("[Loader]   Encontrados %d labels\n", label_table.count);
    
    // Volver al inicio del archivo para la segunda pasada
    rewind(file);
    
    // ===== SEGUNDA PASADA: Parsear instrucciones =====
    
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
    line_num = 0;
    
    while (fgets(line, MAX_LINE_LENGTH, file)) {
        line_num++;
        
        Instruction inst;
        int result = parse_line(line, &inst, &label_table);
        
        if (result < 0) {
            fprintf(stderr, "[Loader] ERROR en línea %d: %s", line_num, line);
            free(prog->code);
            free(prog);
            fclose(file);
            return NULL;
        }
        
        if (result == 0) continue; // Línea vacía, comentario o label
        
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
            case OP_MOV:
                printf("R%d, %.2f", inst->rd, inst->imm);
                break;
            case OP_LOAD:
                if (inst->addr_mode == ADDR_DIRECT) {
                    printf("R%d, [%d]", inst->rd, inst->addr);
                } else {
                    printf("R%d, [R%d]", inst->rd, inst->addr_reg);
                }
                break;
            case OP_STORE:
                if (inst->addr_mode == ADDR_DIRECT) {
                    printf("R%d, [%d]", inst->rd, inst->addr);
                } else {
                    printf("R%d, [R%d]", inst->rd, inst->addr_reg);
                }
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
                printf("%d (usa zero_flag)", inst->label);
                break;
            case OP_HALT:
                break;
        }
        
        printf("\n");
    }
    
    printf("╚════════════════════════════════════════════════════╝\n\n");
}

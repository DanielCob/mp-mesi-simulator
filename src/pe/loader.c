#define LOG_MODULE "LOADER"
#include "loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "log.h"

#define MAX_LINE_LENGTH 256
#define INITIAL_CAPACITY 64
#define MAX_LABELS 128
#define MAX_LABEL_NAME 64

/**
 * @brief Structure to store labels
 */
typedef struct {
    char name[MAX_LABEL_NAME];
    int line_number;  // Instruction index (not file line number)
} Label;

/**
 * @brief Label table
 */
typedef struct {
    Label labels[MAX_LABELS];
    int count;
} LabelTable;

/**
 * @brief Initialize label table
 */
static void init_label_table(LabelTable* table) {
    table->count = 0;
}

/**
 * @brief Add a label to the table
 */
static int add_label(LabelTable* table, const char* name, int line_number) {
    if (table->count >= MAX_LABELS) {
        LOGE("Too many labels (max %d)", MAX_LABELS);
        return -1;
    }
    
    // Check if label already exists
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->labels[i].name, name) == 0) {
            LOGE("Duplicate label: %s", name);
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
 * @brief Find a label in the table
 * @return Label instruction index, or -1 if not found
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
 * @brief Convert OpCode to string (local)
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
 * @brief Trim whitespace at both ends of string
 */
static char* trim(char* str) {
    // Trim start
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // Trim end
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    *(end + 1) = '\0';
    return str;
}

/**
 * @brief Convert opcode string to OpCode enum
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
    return OP_HALT; // Default for unrecognized opcodes
}

/**
 * @brief Parse a register number (R0-R7) into its index
 */
static int parse_register(const char* str) {
    if (str[0] != 'R' && str[0] != 'r') return -1;
    
    int reg = atoi(str + 1);
    if (reg < 0 || reg > 7) return -1;
    
    return reg;
}

/**
 * @brief Check if a line contains a label (format "NAME:")
 * @param rest If not NULL, stores the rest of the line after ':'
 * @return Pointer to the label name (without ':'), or NULL if not a label
 */
static char* extract_label(char* line, char* label_name, char* rest) {
    char* trimmed = trim(line);
    
    // Look for ':'
    char* colon = strchr(trimmed, ':');
    if (!colon) return NULL;
    
    // Extract label name (everything before ':')
    size_t len = colon - trimmed;
    if (len == 0 || len >= MAX_LABEL_NAME) return NULL;
    
    strncpy(label_name, trimmed, len);
    label_name[len] = '\0';
    
    // Trim the label name
    char* label_trimmed = trim(label_name);
    
    // Validate name (alphanumeric and _ only)
    for (size_t i = 0; i < strlen(label_trimmed); i++) {
        if (!isalnum(label_trimmed[i]) && label_trimmed[i] != '_') {
            return NULL;
        }
    }
    
    // Copy back
    if (label_trimmed != label_name) {
        strcpy(label_name, label_trimmed);
    }
    
    // If requested, extract rest of line after ':'
    if (rest) {
        char* after_colon = colon + 1;
        strcpy(rest, trim(after_colon));
    }
    
    return label_name;
}

/**
 * @brief Parse an assembly line into an instruction
 * @param label_table Label table to resolve references
 * @return 1 on success, 0 if empty/comment/label, -1 on error
 */
static int parse_line(const char* line, Instruction* inst, const LabelTable* label_table) {
    char line_copy[MAX_LINE_LENGTH];
    strncpy(line_copy, line, MAX_LINE_LENGTH - 1);
    line_copy[MAX_LINE_LENGTH - 1] = '\0';
    
    // Remove comments
    char* comment = strchr(line_copy, '#');
    if (comment) *comment = '\0';
    
    // Check if it's a label (and skip it in the second pass)
    char label_name[MAX_LABEL_NAME];
    char rest[MAX_LINE_LENGTH];
    if (extract_label(line_copy, label_name, rest)) {
        // It's a label; check if there's an instruction after ':'
        if (strlen(rest) == 0) {
            return 0; // Only label, no instruction
        }
        // Instruction after label, parse the rest
        strcpy(line_copy, rest);
    }
    
    // Trim
    char* trimmed = trim(line_copy);
    if (strlen(trimmed) == 0) return 0; // Empty line
    
    // Parse opcode
    char opcode_str[16];
    if (sscanf(trimmed, "%15s", opcode_str) != 1) return 0;
    
    OpCode op = parse_opcode(opcode_str);
    
    // Check opcode validity
    if (op == OP_HALT && strcmp(opcode_str, "HALT") != 0) {
        // parse_opcode returned HALT by default, but it's not a real HALT
        return 0; // Ignore line
    }
    
    // Initialize instruction with defaults
    inst->op = op;
    inst->rd = 0;
    inst->ra = 0;
    inst->rb = 0;
    inst->imm = 0.0;
    inst->addr = 0;
    inst->addr_reg = 0;
    inst->addr_mode = ADDR_DIRECT;
    inst->label = 0;
    
    // Parse operands according to instruction type
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
                    LOGE("Invalid register in MOV: %s", reg1);
                    return -1;
                }
                inst->imm = imm_value;
            } else {
                LOGE("Invalid format for MOV: %s", operands);
                return -1;
            }
            break;
            
        case OP_LOAD:
            // LOAD Rd, [addr] o LOAD Rd, [Rx]
            if (sscanf(operands, "%7[^,], [%31[^]]]", reg1, addr_str) == 2) {
                inst->rd = parse_register(reg1);
                if (inst->rd < 0) {
                    LOGE("Invalid register in LOAD: %s", reg1);
                    return -1;
                }
                
                // Determine if direct [number] or indirect [Rx]
                char* trimmed_addr = trim(addr_str);
                if (trimmed_addr[0] == 'R' || trimmed_addr[0] == 'r') {
                    // Indirect addressing [Rx]
                    inst->addr_reg = parse_register(trimmed_addr);
                    if (inst->addr_reg < 0) {
                        LOGE("Invalid address register in LOAD: [%s]", trimmed_addr);
                        return -1;
                    }
                    inst->addr_mode = ADDR_REGISTER;
                } else {
                    // Direct addressing [number]
                    inst->addr = atoi(trimmed_addr);
                    inst->addr_mode = ADDR_DIRECT;
                }
            } else {
                LOGE("Invalid format for LOAD (use [addr] or [Rx]): %s", operands);
                return -1;
            }
            break;
            
        case OP_STORE:
            // STORE Rs, [addr] o STORE Rs, [Rx]
            if (sscanf(operands, "%7[^,], [%31[^]]]", reg1, addr_str) == 2) {
                inst->rd = parse_register(reg1);  // rd se usa como source en STORE
                if (inst->rd < 0) {
                    LOGE("Invalid register in STORE: %s", reg1);
                    return -1;
                }
                
                // Determine if direct [number] or indirect [Rx]
                char* trimmed_addr = trim(addr_str);
                if (trimmed_addr[0] == 'R' || trimmed_addr[0] == 'r') {
                    // Indirect addressing [Rx]
                    inst->addr_reg = parse_register(trimmed_addr);
                    if (inst->addr_reg < 0) {
                        LOGE("Invalid address register in STORE: [%s]", trimmed_addr);
                        return -1;
                    }
                    inst->addr_mode = ADDR_REGISTER;
                } else {
                    // Direct addressing [number]
                    inst->addr = atoi(trimmed_addr);
                    inst->addr_mode = ADDR_DIRECT;
                }
            } else {
                LOGE("Invalid format for STORE (use [addr] or [Rx]): %s", operands);
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
                    LOGE("Invalid register in %s: %s %s %s", opcode_str, reg1, reg2, reg3);
                    return -1;
                }
            } else {
                LOGE("Invalid format for %s: %s", opcode_str, operands);
                return -1;
            }
            break;
            
        case OP_INC:
        case OP_DEC:
            // INC/DEC Rd
            if (sscanf(operands, "%7s", reg1) == 1) {
                inst->rd = parse_register(reg1);
                if (inst->rd < 0) {
                    LOGE("Invalid register in %s: %s", opcode_str, reg1);
                    return -1;
                }
            } else {
                LOGE("Invalid format for %s: %s", opcode_str, operands);
                return -1;
            }
            break;
            
        case OP_JNZ:
            // JNZ label (uses zero_flag implicitly)
            {
                char label_str[MAX_LABEL_NAME];
                if (sscanf(operands, "%63s", label_str) == 1) {
                    // No register needed, uses zero_flag
                    inst->rd = 0;  // No usado, pero mantener por compatibilidad
                    
                    // Intentar parsear como número
                    char* endptr;
                    long label_num = strtol(label_str, &endptr, 10);
                    
                    if (*endptr == '\0') {
                        // Direct numeric target
                        inst->label = (int)label_num;
                    } else {
                        // It's a label name; look it up in the table
                        int label_line = find_label(label_table, label_str);
                        if (label_line < 0) {
                            LOGE("Label not found: %s", label_str);
                            return -1;
                        }
                        inst->label = label_line;
                    }
                } else {
                    LOGE("Invalid format for JNZ: %s", operands);
                    return -1;
                }
            }
            break;
            
        case OP_HALT:
            // HALT no tiene operandos
            break;
            
        default:
            LOGE("Unrecognized opcode: %s", opcode_str);
            return -1;
    }
    
    return 1; // Success
}

Program* load_program(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        LOGE("Failed to open file: %s", filename);
        return NULL;
    }
    
    LOGI("Loading program: %s", filename);
    
    // ===== FIRST PASS: Collect labels =====
    LabelTable label_table;
    init_label_table(&label_table);
    
    char line[MAX_LINE_LENGTH];
    int line_num = 0;
    int instruction_count = 0;
    
    while (fgets(line, MAX_LINE_LENGTH, file)) {
        line_num++;
        
        // Remove comments
        char line_copy[MAX_LINE_LENGTH];
        strncpy(line_copy, line, MAX_LINE_LENGTH - 1);
        line_copy[MAX_LINE_LENGTH - 1] = '\0';
        
        char* comment = strchr(line_copy, '#');
        if (comment) *comment = '\0';
        
        // Check if it's a label
        char label_name[MAX_LABEL_NAME];
        char rest[MAX_LINE_LENGTH];
        if (extract_label(line_copy, label_name, rest)) {
            // It's a label, add to table
            if (add_label(&label_table, label_name, instruction_count) < 0) {
                fclose(file);
                return NULL;
            }
            LOGD("  Label '%s' -> line %d", label_name, instruction_count);
            
            // Check for instruction on same line after label
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
        
        // Not a label, check if it's a valid instruction
        char* trimmed = trim(line_copy);
        if (strlen(trimmed) > 0) {
            // Non-empty instruction, increment counter
            char opcode[16];
            if (sscanf(trimmed, "%15s", opcode) == 1) {
                // Validate opcode
                OpCode op = parse_opcode(opcode);
                if (op != OP_HALT || strcmp(opcode, "HALT") == 0) {
                    instruction_count++;
                }
            }
        }
    }
    
    LOGD("  Found %d labels", label_table.count);
    
    // Volver al inicio del archivo para la segunda pasada
    rewind(file);
    
    // ===== SECOND PASS: Parse instructions =====
    
    // Create program with initial capacity
    Program* prog = (Program*)malloc(sizeof(Program));
    if (!prog) {
        LOGE("Could not allocate memory for program");
        fclose(file);
        return NULL;
    }
    
    int capacity = INITIAL_CAPACITY;
    prog->code = (Instruction*)malloc(capacity * sizeof(Instruction));
    if (!prog->code) {
        LOGE("Could not allocate memory for instructions");
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
            LOGE("Error at line %d: %s", line_num, line);
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
                LOGE("Could not expand memory for instructions");
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
    
    LOGI("Program loaded: %d instructions", prog->size);
    
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
        LOGW("Program is NULL");
        return;
    }
    
    printf("\n[Program loaded: %d instructions]\n", prog->size);
    
    for (int i = 0; i < prog->size; i++) {
        Instruction* inst = &prog->code[i];
        const char* op_name = opcode_to_string_local(inst->op);
        
        printf("[%3d] %-8s ", i, op_name);
        
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
                printf("%d (uses zero_flag)", inst->label);
                break;
            case OP_HALT:
                break;
        }
        
        printf("\n");
    }
    
    printf("\n");
}

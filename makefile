# ============================
# CONFIGURACIÓN GENERAL
# ============================
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
TARGET = mp_mesi
PYTHON = python3

# Directorios de código
SRC_DIR = src
OBJ_DIR = obj
ASM_DIR = asm
SCRIPTS_DIR = scripts

# Incluir subcarpetas
INCLUDES = -I$(SRC_DIR) \
           -I$(SRC_DIR)/include \
           -I$(SRC_DIR)/bus \
           -I$(SRC_DIR)/cache \
           -I$(SRC_DIR)/memory \
           -I$(SRC_DIR)/pe \
           -I$(SRC_DIR)/stats \
           -I$(SRC_DIR)/dotprod

# Buscar todos los archivos .c en src/ y subcarpetas
SRC = $(shell find $(SRC_DIR) -name "*.c")
OBJ = $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Script de generación y archivo de configuración
GEN_SCRIPT = $(SCRIPTS_DIR)/generate_asm.py
CONFIG_H = $(SRC_DIR)/include/config.h

# Archivos assembly generados
ASM_FILES = $(ASM_DIR)/dotprod_pe0.asm \
            $(ASM_DIR)/dotprod_pe1.asm \
            $(ASM_DIR)/dotprod_pe2.asm \
            $(ASM_DIR)/dotprod_pe3.asm

# ============================
# COLORES
# ============================
GREEN  = \033[0;32m
YELLOW = \033[1;33m
RED    = \033[0;31m
RESET  = \033[0m

# ============================
# REGLAS PRINCIPALES
# ============================
all: $(TARGET)

# Generar archivos assembly si no existen o si config.h cambió
$(ASM_FILES): $(CONFIG_H) $(GEN_SCRIPT)
	@echo "$(YELLOW) Generando archivos assembly...$(RESET)"
	@$(PYTHON) $(GEN_SCRIPT)
	@echo "$(GREEN) Archivos assembly generados$(RESET)"

# Crear ejecutable
$(TARGET): $(ASM_FILES) $(OBJ)
	@echo "$(YELLOW) Enlazando...$(RESET)"
	@$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)
	@echo "$(GREEN) Compilación completa: $(TARGET)$(RESET)"

# Compilar cada archivo .c a .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "$(YELLOW) Compilando $< ...$(RESET)"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ============================
# REGLAS DE EJECUCIÓN
# ============================
run: $(TARGET)
	@echo "$(GREEN) Ejecutando...$(RESET)"
	@./$(TARGET)

debug: $(TARGET)
	@echo "$(GREEN) Iniciando gdb...$(RESET)"
	@gdb ./$(TARGET)

# ============================
# LIMPIEZA
# ============================
clean:
	@echo "$(RED) Limpiando archivos compilados...$(RESET)"
	@rm -rf $(OBJ_DIR) $(TARGET)

cleanall: clean
	@echo "$(RED) Limpiando archivos assembly generados...$(RESET)"
	@rm -f $(ASM_FILES)

# ============================
# EXTRA
# ============================
.PHONY: all clean cleanall run debug

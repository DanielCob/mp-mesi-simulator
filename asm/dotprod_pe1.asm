# ============================================================================
# Producto Punto Paralelo - PE1 (GENERADO AUTOMÁTICAMENTE CON LOOP)
# ============================================================================
# Segmento: elementos [4 to 7]
# Configuración: VECTOR_SIZE=16, SEGMENT_SIZE=4
# ============================================================================

# ============================================================================
# Inicialización
# ============================================================================
MOV R0, 0.0         # R0 = acumulador (resultado parcial)
MOV R1, 4.0         # R1 = índice de elemento actual
MOV R2, 100.0       # R2 = base de vector B
MOV R3, 4.0        # R3 = contador del loop (SEGMENT_SIZE)

# ============================================================================
# LOOP: Procesar SEGMENT_SIZE elementos
# ============================================================================
LOOP_START:
# Cargar A[i] usando addressing indirecto
LOAD R4, [R1]       # R4 = A[i]

# Calcular dirección de B[i] = VECTOR_B_BASE + i
FADD R5, R2, R1     # R5 = VECTOR_B_BASE + i
LOAD R6, [R5]       # R6 = B[i]

# Multiplicar A[i] * B[i]
FMUL R7, R4, R6     # R7 = A[i] * B[i]

# Acumular resultado
FADD R0, R0, R7     # acum += A[i] * B[i]

# Incrementar índice
INC R1              # i++

# Decrementar contador y verificar si continuar
DEC R3              # contador--
JNZ LOOP_START      # Si R3 != 0, repetir loop

# ============================================================================
# Guardar resultado parcial
# ============================================================================
MOV R5, 204.0       # RESULTS_BASE + 1*BLOCK_SIZE
STORE R0, [R5]      # Guardar resultado parcial de PE1

# ============================================================================
# Señalizar finalización (barrier)
# ============================================================================
MOV R6, 1.0         # Flag value
MOV R7, 224.0       # FLAGS_BASE + 1*BLOCK_SIZE
STORE R6, [R7]      # Flag PE1 = 1.0

HALT

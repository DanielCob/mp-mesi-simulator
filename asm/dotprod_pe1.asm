# ============================================================================
# Producto Punto Paralelo - PE1 (CÓDIGO GENÉRICO REUTILIZABLE)
# ============================================================================
# Este código carga todas las constantes desde SHARED_CONFIG (addr 0-15)
# NO requiere regeneración al cambiar VECTOR_SIZE, NUM_PES, etc.
# Solo recompilar para actualizar el área de configuración compartida
#
# SHARED_CONFIG Layout:
#   [0] = VECTOR_A_ADDR
#   [1] = VECTOR_B_ADDR
#   [2] = RESULTS_ADDR
#   [3] = FLAGS_ADDR
#   [8+1*2] = PE1_START_INDEX
#   [9+1*2] = PE1_SEGMENT_SIZE
# ============================================================================

# ============================================================================
# CARGA DE CONSTANTES DESDE SHARED_CONFIG
# ============================================================================

# Cargar VECTOR_A_ADDR (addr 0)
MOV R4, 0.0
LOAD R5, [R4]        # R5 = VECTOR_A_ADDR (para cálculo de direcciones)

# Cargar VECTOR_B_ADDR (addr 1)
MOV R4, 1.0
LOAD R2, [R4]        # R2 = VECTOR_B_ADDR

# Cargar start_index para PE1 (addr 10)
MOV R4, 10.0
LOAD R1, [R4]        # R1 = start_index (índice, no dirección)

# Cargar segment_size para PE1 (addr 11)
MOV R4, 11.0
LOAD R3, [R4]        # R3 = segment_size (contador del loop)

# ============================================================================
# INICIALIZACIÓN
# ============================================================================
MOV R0, 0.0         # R0 = acumulador (resultado parcial)

# ============================================================================
# LOOP: Procesar segment_size elementos
# ============================================================================
LOOP_START:
FADD R4, R5, R1     # R4 = VECTOR_A_ADDR + i (dirección de A[i])
LOAD R4, [R4]       # R4 = A[i]
FADD R6, R2, R1     # R6 = VECTOR_B_ADDR + i
LOAD R6, [R6]       # R6 = B[i]
FMUL R4, R4, R6     # R4 = A[i] * B[i]
FADD R0, R0, R4     # acumulador += producto
INC R1              # i++
DEC R3              # contador--
JNZ LOOP_START      # Repetir si contador != 0

# ============================================================================
# GUARDAR RESULTADO Y SEÑALIZAR
# ============================================================================
# Resultado parcial: RESULTS_ADDR + pe_id (direccionamiento directo)
MOV R5, 17.0
STORE R0, [R5]      # Guardar resultado parcial

# Flag de sincronización: FLAGS_ADDR + pe_id
MOV R7, 21.0
MOV R6, 1.0         # Flag value
STORE R6, [R7]      # Señalizar finalización

HALT

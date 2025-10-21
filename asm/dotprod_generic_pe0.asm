# ============================================================================
# Producto Punto Paralelo - PE0 (Genérico con MOV)
# ============================================================================
# Segmento: elementos [0 to SEGMENT_SIZE-1]
# Este archivo es PARAMETRIZABLE modificando SEGMENT_SIZE en config.h
# ============================================================================
# Para VECTOR_SIZE=16, NUM_PES=4 → SEGMENT_SIZE=4
# PE0 procesa: A[0-3] · B[0-3]
# ============================================================================

# Constantes parametrizables
# Base addresses (definidos en config.h):
#   VECTOR_A_BASE = 0
#   VECTOR_B_BASE = 100  
#   RESULTS_BASE = 200 (PE0 usa offset 0)
#   FLAGS_BASE = 220 (PE0 usa offset 0)

# ============================================================================
# Inicialización
# ============================================================================
MOV R0, 0.0         # R0 = acumulador (resultado parcial)
MOV R1, 0.0         # R1 = índice base para este PE (0 para PE0)
MOV R2, 100.0       # R2 = base de vector B

# ============================================================================
# Procesamiento de segmento (4 elementos para SEGMENT_SIZE=4)
# ============================================================================

# Elemento 0
LOAD R4, [R1]       # R4 = A[0]
FADD R3, R2, R1     # R3 = 100 + 0 = 100
LOAD R6, [R3]       # R6 = B[100]
FMUL R7, R4, R6     # R7 = A[0] * B[0]
FADD R0, R0, R7     # acum += A[0] * B[0]

# Elemento 1
INC R1              # R1 = 1
LOAD R4, [R1]       # R4 = A[1]
FADD R3, R2, R1     # R3 = 100 + 1 = 101
LOAD R6, [R3]       # R6 = B[101]
FMUL R7, R4, R6     # R7 = A[1] * B[1]
FADD R0, R0, R7     # acum += A[1] * B[1]

# Elemento 2
INC R1              # R1 = 2
LOAD R4, [R1]       # R4 = A[2]
FADD R3, R2, R1     # R3 = 100 + 2 = 102
LOAD R6, [R3]       # R6 = B[102]
FMUL R7, R4, R6     # R7 = A[2] * B[2]
FADD R0, R0, R7     # acum += A[2] * B[2]

# Elemento 3
INC R1              # R1 = 3
LOAD R4, [R1]       # R4 = A[3]
FADD R3, R2, R1     # R3 = 100 + 3 = 103
LOAD R6, [R3]       # R6 = B[103]
FMUL R7, R4, R6     # R7 = A[3] * B[3]
FADD R0, R0, R7     # acum += A[3] * B[3]

# ============================================================================
# Guardar resultado parcial
# ============================================================================
MOV R5, 200.0       # RESULTS_BASE + 0*BLOCK_SIZE
STORE R0, [R5]      # Guardar resultado parcial de PE0

# ============================================================================
# Señalizar finalización (barrier)
# ============================================================================
MOV R6, 1.0         # Flag value
MOV R7, 220.0       # FLAGS_BASE + 0*BLOCK_SIZE
STORE R6, [R7]      # Flag PE0 = 1.0

HALT

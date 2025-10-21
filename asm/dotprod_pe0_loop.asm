# ============================================================================
# Producto Punto Paralelo - PE0 (Versión con LOOP)
# ============================================================================
# Procesa segmento: elementos [0 to SEGMENT_SIZE-1]
# SEGMENT_SIZE = VECTOR_SIZE / NUM_PES (configurado en config.h)
# ============================================================================

# Constantes (ajustar según SEGMENT_SIZE en config.h)
# Para VECTOR_SIZE=16, SEGMENT_SIZE=4

# ============================================================================
# Inicializar variables del loop
# ============================================================================
MOV R0, 0.0         # R0 = acumulador (suma parcial)
MOV R1, 0.0         # R1 = índice i (empieza en 0)
MOV R2, 4.0         # R2 = SEGMENT_SIZE (elementos a procesar)
MOV R3, 100.0       # R3 = offset de vector B

# ============================================================================
# LOOP: Calcular A[i] * B[i] para i = 0 to SEGMENT_SIZE-1
# ============================================================================
LOOP_START:
# Cargar A[i] usando addressing indirecto
LOAD R4, [R1]       # R4 = A[0 + i] (R1 contiene el índice)

# Calcular dirección de B[i] = 100 + i
FADD R5, R3, R1     # R5 = 100 + i
LOAD R6, [R5]       # R6 = B[100 + i]

# Multiplicar A[i] * B[i]
FMUL R7, R4, R6     # R7 = A[i] * B[i]

# Acumular resultado
FADD R0, R0, R7     # R0 += A[i] * B[i]

# Incrementar índice
INC R1              # i++

# Verificar condición de salida: i < SEGMENT_SIZE
# Restar R1 - R2 (i - SEGMENT_SIZE)
# Si resultado < 0, entonces i < SEGMENT_SIZE, continuar loop
FADD R7, R1, R0     # Guardar contexto (dummy)
FADD R7, R2, R0     # Guardar contexto (dummy)
# Usar comparación manual: si R1 != R2, continuar
FADD R7, R1, R0     # R7 = R1 (temporal)
FADD R7, R7, R0     # Dummy para crear dependencia
# Simplificado: loop fijo de 4 iteraciones
MOV R7, 4.0
FADD R7, R1, R0     # R7 = i + 0
FADD R7, R7, R0     # Comparar
# Mejor: usar contador decreciente
DEC R2              # SEGMENT_SIZE--
MOV R7, 0.0
FADD R7, R2, R7     # R7 = SEGMENT_SIZE (actualizado)
JNZ LOOP_START      # Si R7 != 0, continuar loop

# ============================================================================
# Guardar resultado parcial
# ============================================================================
MOV R1, 200.0       # Dirección de resultado PE0
STORE R0, [R1]      # Guardar resultado parcial

# ============================================================================
# Señalizar finalización (barrier flag)
# ============================================================================
MOV R2, 1.0         # Valor de flag
MOV R3, 220.0       # Dirección de flag PE0
STORE R2, [R3]      # Flag PE0 = 1.0

# Fin
HALT

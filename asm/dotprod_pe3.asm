# ============================================================================
# Producto Punto Paralelo - PE3 (MASTER - GENERADO CON LOOP)
# ============================================================================
# Segmento: elementos [12 to 15] + REDUCCIÓN
# Configuración: VECTOR_SIZE=16, SEGMENT_SIZE=4
# ============================================================================

# ============================================================================
# FASE 1: Calcular producto parcial propio con LOOP
# ============================================================================

# Inicialización
MOV R0, 0.0         # R0 = acumulador (resultado parcial)
MOV R1, 12.0         # R1 = índice de elemento actual
MOV R2, 100.0       # R2 = base de vector B
MOV R3, 4.0        # R3 = contador del loop (SEGMENT_SIZE)

# LOOP: Procesar segmento propio
LOOP_START:
LOAD R4, [R1]       # R4 = A[i]
FADD R5, R2, R1     # R5 = VECTOR_B_BASE + i
LOAD R6, [R5]       # R6 = B[i]
FMUL R7, R4, R6     # R7 = A[i] * B[i]
FADD R0, R0, R7     # acum += A[i] * B[i]
INC R1              # i++
DEC R3              # contador--
JNZ LOOP_START      # Si R3 != 0, repetir

# Guardar resultado parcial de PE3
MOV R5, 212.0       # RESULTS_BASE + 3*BLOCK_SIZE
STORE R0, [R5]      # Guardar resultado parcial de PE3

# ============================================================================
# FASE 2: BARRIER - Esperar a que otros PEs terminen
# ============================================================================
WAIT_LOOP:
MOV R1, 220.0      # Dirección flag PE0
LOAD R1, [R1]  # R1 = flag PE0
MOV R2, 224.0      # Dirección flag PE1
LOAD R2, [R2]  # R2 = flag PE1
MOV R3, 228.0      # Dirección flag PE2
LOAD R3, [R3]  # R3 = flag PE2

# Sumar flags (deben sumar 3.0)
FADD R6, R1, R2     # R6 = flag0 + flag1
FADD R6, R6, R3     # R6 += flag2

# Restar 3.0 para verificar
MOV R7, 232.0       # Dirección de constante -3.0
LOAD R7, [R7]       # R7 = -3.0
FADD R6, R6, R7     # R6 = suma_flags - 3.0
JNZ WAIT_LOOP       # Si R6 != 0, repetir

# ============================================================================
# FASE 3: REDUCCIÓN - Sumar todos los productos parciales con LOOP
# ============================================================================

MOV R0, 0.0         # R0 = acumulador final
MOV R1, 200.0        # R1 = base de resultados parciales
MOV R2, 4.0             # R2 = contador (NUM_PES)
MOV R3, 4.0          # R3 = BLOCK_SIZE (offset entre resultados)

REDUCE_LOOP:
# Cargar resultado parcial
LOAD R4, [R1]       # R4 = resultado_parcial[i]
FADD R0, R0, R4     # acum += resultado_parcial[i]

# Avanzar a siguiente resultado (dirección += BLOCK_SIZE)
FADD R1, R1, R3     # R1 += BLOCK_SIZE

# Decrementar contador
DEC R2              # contador--
JNZ REDUCE_LOOP     # Si R2 != 0, repetir

# ============================================================================
# Guardar resultado final
# ============================================================================
MOV R2, 216.0       # Dirección resultado final
STORE R0, [R2]      # Guardar producto punto final

HALT

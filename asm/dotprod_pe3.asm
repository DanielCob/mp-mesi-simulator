# ============================================================================
# Producto Punto Paralelo - PE3
# ============================================================================
# Segmento: A[12-15] · B[12-15] + SUMA FINAL
# Vector A: direcciones 0-15 (16 elementos)
# Vector B: direcciones 100-115 (16 elementos)
# Resultado parcial: dirección 212 (bloque separado)
# Resultado final: dirección 216 (bloque separado)
# ============================================================================

# ============================================================================
# FASE 1: Calcular producto parcial A[12-15] · B[12-15]
# ============================================================================

# Inicializar acumulador
# R2 ya está inicializado en 0.0 al inicio del programa

# Calcular A[12] * B[12]
LOAD R4, [12]     # R4 = A[12]
LOAD R6, [112]    # R6 = B[12]
FMUL R7, R4, R6   # R7 = A[12] * B[12]
FADD R2, R2, R7   # R2 += A[12] * B[12]

# Calcular A[13] * B[13]
LOAD R4, [13]     # R4 = A[13]
LOAD R6, [113]    # R6 = B[13]
FMUL R7, R4, R6   # R7 = A[13] * B[13]
FADD R2, R2, R7   # R2 += A[13] * B[13]

# Calcular A[14] * B[14]
LOAD R4, [14]     # R4 = A[14]
LOAD R6, [114]    # R6 = B[14]
FMUL R7, R4, R6   # R7 = A[14] * B[14]
FADD R2, R2, R7   # R2 += A[14] * B[14]

# Calcular A[15] * B[15]
LOAD R4, [15]     # R4 = A[15]
LOAD R6, [115]    # R6 = B[15]
FMUL R7, R4, R6   # R7 = A[15] * B[15]
FADD R2, R2, R7   # R2 += A[15] * B[15]

# Guardar resultado parcial de PE3 en dirección 212 (bloque separado)
STORE R2, [212]

# ============================================================================
# BARRIER: Esperar a que PE0, PE1, PE2 terminen (busy-waiting con loop)
# ============================================================================
WAIT_LOOP:
LOAD R3, [220]    # R3 = flag PE0
LOAD R4, [224]    # R4 = flag PE1 (bloque separado)
LOAD R5, [228]    # R5 = flag PE2 (bloque separado)
# Sumar los 3 flags
FADD R6, R3, R4   # R6 = flag0 + flag1
FADD R6, R6, R5   # R6 = flag0 + flag1 + flag2 (debe ser 3.0 cuando todos terminaron)
# Restar 3.0: R6 + (-3.0)
LOAD R7, [232]    # R7 = -3.0 (constante en memoria, bloque separado)
FADD R3, R6, R7   # R3 = suma_flags - 3.0 (reusar R3)
# Si R3 == 0, entonces zero_flag=1 y JNZ no salta (todos terminaron)
# Si R3 != 0, entonces zero_flag=0 y JNZ salta al inicio del loop
JNZ WAIT_LOOP     # Si zero_flag==0 (R3!=0), saltar a WAIT_LOOP
# Si llegamos aquí, todos los PEs terminaron (suma_flags == 3.0)

# ============================================================================
# FASE 2: Reducción - Sumar todos los productos parciales
# ============================================================================
# PE3 es responsable de la suma final de los 4 productos parciales
# Resultados parciales en: 200 (PE0), 204 (PE1), 208 (PE2), 212 (PE3)

# Inicializar acumulador para suma final
# R0 ya está inicializado en 0.0 al inicio del programa

# Cargar y sumar producto parcial de PE0
LOAD R1, [200]    # R1 = resultado de PE0
FADD R0, R0, R1   # R0 += PE0

# Cargar y sumar producto parcial de PE1
LOAD R1, [204]    # R1 = resultado de PE1
FADD R0, R0, R1   # R0 += PE1

# Cargar y sumar producto parcial de PE2
LOAD R1, [208]    # R1 = resultado de PE2
FADD R0, R0, R1   # R0 += PE2

# Cargar y sumar producto parcial de PE3
LOAD R1, [212]    # R1 = resultado de PE3
FADD R0, R0, R1   # R0 += PE3

# ============================================================================
# Guardar resultado final del producto punto
# ============================================================================
# Guardar producto punto final en dirección 216 (bloque separado)
STORE R0, [216]

# Fin del programa
HALT

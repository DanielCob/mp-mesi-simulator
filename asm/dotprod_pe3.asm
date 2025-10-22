# ============================================================================
# Producto Punto Paralelo - PE3 (MASTER - CÓDIGO GENÉRICO REUTILIZABLE)
# ============================================================================
# Este código carga constantes desde SHARED_CONFIG (addr 0-15)
# OPTIMIZACIÓN: Usa MOV immediate para BARRIER_CHECK y NUM_PES
#   (evita cache thrashing durante barrier/reducción)
# ============================================================================

# ============================================================================
# FASE 1: Calcular producto parcial propio
# ============================================================================

# Cargar VECTOR_A_ADDR (addr 0)
MOV R4, 0.0
LOAD R5, [R4]        # R5 = VECTOR_A_ADDR (para cálculo de direcciones)

# Cargar VECTOR_B_ADDR (addr 1)
MOV R4, 1.0
LOAD R2, [R4]        # R2 = VECTOR_B_ADDR

# Cargar start_index para PE3 (addr 14)
MOV R4, 14.0
LOAD R1, [R4]        # R1 = start_index (índice, no dirección)

# Cargar segment_size para PE3 (addr 15)
MOV R4, 15.0
LOAD R3, [R4]        # R3 = segment_size (contador)

# Inicialización
MOV R0, 0.0         # R0 = acumulador

# LOOP: Procesar segmento propio
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

# Guardar resultado parcial (direccionamiento directo)
MOV R5, 19.0
STORE R0, [R5]      # Guardar en RESULTS_ADDR + 3

# ============================================================================
# FASE 2: BARRIER - Esperar a que otros PEs terminen
# ============================================================================

WAIT_LOOP:
# Cargar flags directamente (1 solo bloque de caché)
MOV R1, 20.0
LOAD R2, [R1]        # R2 = flag[PE0]

MOV R1, 21.0
LOAD R4, [R1]        # R4 = flag[PE1]

MOV R1, 22.0
LOAD R5, [R1]        # R5 = flag[PE2]

# Sumar flags
FADD R6, R2, R4      # R6 = flag0 + flag1
FADD R6, R6, R5      # R6 += flag2

# OPTIMIZACIÓN: MOV immediate para BARRIER_CHECK (evita cache miss)
MOV R7, -3  # R7 = -(NUM_PES-1) = -3
FADD R6, R6, R7      # R6 = suma_flags - (NUM_PES-1)
JNZ WAIT_LOOP        # Si != 0, repetir

# ============================================================================
# FASE 3: REDUCCIÓN - Sumar resultados parciales
# ============================================================================

# OPTIMIZACIÓN: MOV immediate para NUM_PES (evita cache miss)
MOV R2, 4.0  # R2 = NUM_PES = 4.0 (contador)

MOV R1, 16.0  # R1 = dirección actual (empieza en RESULTS_ADDR)
MOV R0, 0.0          # R0 = acumulador final

REDUCE_LOOP:
LOAD R4, [R1]        # R4 = resultado_parcial[i]
FADD R0, R0, R4      # acumulador += resultado_parcial[i]
INC R1               # R1++ (siguiente resultado: compacto, +1 dirección)
DEC R2               # contador--
JNZ REDUCE_LOOP      # Repetir si contador != 0

# ============================================================================
# Guardar resultado final
# ============================================================================
MOV R2, 24.0
STORE R0, [R2]       # Guardar producto punto final

HALT

# cálculo de producto punto parcial

MOV R4, 0.0
LOAD R5, [R4]        # R5 = VECTOR_A_ADDR

MOV R4, 1.0
LOAD R2, [R4]        # R2 = VECTOR_B_ADDR

MOV R4, 14.0
LOAD R1, [R4]        # R1 = start_index

MOV R4, 15.0
LOAD R3, [R4]        # R3 = segment_size (contador)

MOV R0, 0.0         # R0 = acumulador

LOOP_START:
FADD R4, R5, R1     # R4 = VECTOR_A_ADDR + i (dirección de A[i])
LOAD R4, [R4]       # R4 = A[i]
FADD R6, R2, R1     # R6 = VECTOR_B_ADDR + i
LOAD R6, [R6]       # R6 = B[i]
FMUL R4, R4, R6     # R4 = A[i] * B[i]
FADD R0, R0, R4     # acumulador += producto
INC R1              # i++
DEC R3              # contador--
JNZ LOOP_START      # repetir si contador != 0

MOV R5, 19.0
STORE R0, [R5]      # guardar en RESULTS_ADDR + 3

# barrera de sincronización

MOV R1, 6.0
LOAD R7, [R1]  # R7 = barrier_check = - (NUM_PES-1)

WAIT_LOOP:
MOV R1, 20.0
LOAD R2, [R1]        # R2 = flag[PE0]

MOV R1, 21.0
LOAD R4, [R1]        # R4 = flag[PE1]

MOV R1, 22.0
LOAD R5, [R1]        # R5 = flag[PE2]

FADD R6, R2, R4      # R6 = flag0 + flag1
FADD R6, R6, R5      # R6 += flag2
FADD R6, R6, R7      # R6 = suma_flags - (NUM_PES-1)

JNZ WAIT_LOOP        # si != 0, repetir

# reducción de resultados parciales

MOV R1, 5.0
LOAD R2, [R1]  # R2 = NUM_PES

MOV R1, 16.0  # R1 = dirección actual (empieza en RESULTS_ADDR)
MOV R0, 0.0          # R0 = acumulador final

REDUCE_LOOP:
LOAD R4, [R1]        # R4 = resultado_parcial[i]
FADD R0, R0, R4      # acumulador += resultado_parcial[i]
INC R1               # R1++ (siguiente resultado: compacto, +1 dirección)
DEC R2               # contador--
JNZ REDUCE_LOOP      # repetir si contador != 0

MOV R2, 24.0
STORE R0, [R2]       # guardar producto punto final

HALT

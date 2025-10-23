# Cargar VECTOR_A_ADDR (addr 0)
MOV R4, 0.0
LOAD R5, [R4]        # R5 = VECTOR_A_ADDR (para cálculo de direcciones)

MOV R4, 1.0
LOAD R2, [R4]        # R2 = VECTOR_B_ADDR

MOV R4, 12.0
LOAD R1, [R4]        # R1 = start_index (índice, no dirección)

MOV R4, 13.0
LOAD R3, [R4]        # R3 = segment_size (contador del loop)

MOV R0, 0.0         # R0 = acumulador (resultado parcial)

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

MOV R5, 18.0
STORE R0, [R5]      # Guardar resultado parcial

MOV R7, 22.0
MOV R6, 1.0         # Flag value
STORE R6, [R7]      # Señalizar finalización

HALT

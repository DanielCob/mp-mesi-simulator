# Programa de prueba de todas las intrucciones del ISA

LOAD R1 400     # 6
LOAD R2 404     # 3,5
FMUL R3 R1 R2
FADD R0 R3 R1   # R0 = 27
LOOP:
    DEC R0      # R0 - 1
    JNZ LOOP
HALT

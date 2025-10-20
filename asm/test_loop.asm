# Programa de prueba: Loop simple con JNZ
# Loop que decrementa R0 desde 8 hasta 0, incrementando R1 en cada iteración
# Resultado esperado: R0=0, R1=8

LOAD R0 300      # Cargar 0 desde memoria[300]
INC R0           # R0 = 1
INC R0           # R0 = 2  
INC R0           # R0 = 3
INC R0           # R0 = 4
INC R0           # R0 = 5
INC R0           # R0 = 6
INC R0           # R0 = 7
INC R0           # R0 = 8
LOOP:
    INC R1       # R1++
    DEC R0       # R0--
    JNZ LOOP     # Si R0 != 0, volver a LOOP
HALT             # Terminar

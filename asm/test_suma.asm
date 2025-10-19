# Programa de prueba simple: Suma de dos números
# Calcula: resultado = a + b

LOAD R0 100      # Cargar primer número desde memoria[100]
LOAD R1 104      # Cargar segundo número desde memoria[104]
FADD R2 R0 R1    # R2 = R0 + R1
STORE R2 200     # Guardar resultado en memoria[200]
HALT             # Terminar

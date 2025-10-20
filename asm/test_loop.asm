# Programa de prueba: Suma simple
# Calcula: 5 + 5 = 10

LOAD R0 300      # Cargar primer número desde memoria[300]
LOAD R1 304      # Cargar segundo número desde memoria[304]
FADD R0 R0 R1    # R0 = R0 + R1 (5 + 5 = 10)
STORE R0 308     # Guardar resultado en memoria[308]
HALT             # Terminar

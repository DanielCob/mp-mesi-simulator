# Programa de prueba: Loop simple con JNZ
# Incrementa 3 veces y guarda el resultado

LOAD R0 0        # Cargar 0 desde memoria[0]
INC R0           # R0 = 1
INC R0           # R0 = 2  
INC R0           # R0 = 3
STORE R0 100     # Guardar resultado (3)
HALT             # Terminar

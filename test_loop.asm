# Programa de prueba: Loop contador
# Cuenta de 0 a 5 usando INC y JNZ

LOAD R0 0        # R0 = contador (inicializar a 0)
INC R0           # R0++
INC R0           # R0++
INC R0           # R0++
INC R0           # R0++
INC R0           # R0++
STORE R0 100     # Guardar resultado en memoria[100]
HALT             # Terminar

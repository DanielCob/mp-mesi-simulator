# Programa de prueba: Producto de dos números
# Calcula: resultado = a * b + a

LOAD R0 200      # Cargar primer número
LOAD R1 204      # Cargar segundo número
FMUL R2 R0 R1    # R2 = R0 * R1
FADD R3 R2 R0    # R3 = R2 + R0
STORE R3 200     # Guardar resultado
HALT             # Terminar

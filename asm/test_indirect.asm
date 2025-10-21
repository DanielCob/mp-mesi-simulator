# ============================================================================
# Test de direccionamiento indirecto
# ============================================================================
# Este programa prueba el nuevo formato de direccionamiento indirecto
# con registros: LOAD Rd, [Rx] y STORE Rs, [Rx]
# ============================================================================

# Inicializar R0 con dirección base 100
LOAD R0, [100]        # R0 = valor en dirección 100 (debería ser 1.0)

# Usar R0 como dirección para cargar indirectamente
# Si R0 = 1.0, entonces [R0] = memoria[1]
LOAD R1, [R0]         # R1 = memoria[R0] = memoria[1]

# Hacer una operación
FADD R2, R0, R1       # R2 = R0 + R1

# Preparar dirección de destino en R3
LOAD R3, [200]        # R3 = 0.0 (direccion 200 está vacía)
INC R3                # R3 = 1.0
INC R3                # R3 = 2.0
INC R3                # R3 = 3.0  (usaremos esto como índice)

# Guardar resultado usando direccionamiento indirecto
STORE R2, [R3]        # memoria[R3] = memoria[3] = R2

# Verificar: cargar de vuelta
LOAD R4, [3]          # R4 = memoria[3] (debería ser igual a R2)
LOAD R5, [R3]         # R5 = memoria[R3] = memoria[3] (lo mismo)

# Fin del programa
HALT

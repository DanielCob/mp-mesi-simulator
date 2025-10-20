# Actualización del Script verify.sh

## Cambios Realizados

Se actualizó el script `tests/verify.sh` para reflejar correctamente los nuevos programas ASM y manejar el output mezclado de threads concurrentes.

## Problema Identificado

1. **Output mezclado de threads**: Los mensajes de diferentes threads se intercalan en la salida, causando que las líneas de registros se dividan
2. **Valores esperados obsoletos**: Los tests esperaban resultados de programas ASM antiguos
3. **Parsing frágil**: El script usaba `awk` con índices de columna fijos que fallaban cuando el output se mezclaba

## Solución Implementada

### 1. Parsing Robusto con `grep -o`

**ANTES** (parsing frágil):
```bash
PE0_R2=$(grep -A 3 "PE0 Register File" /tmp/mesi_output.txt | grep "R0:" | awk '{print $6}')
```

**DESPUÉS** (parsing robusto):
```bash
PE0_R2=$(grep -A 6 "PE0 Register File" /tmp/mesi_output.txt | grep -o "R2: [0-9.]*" | head -1 | awk '{print $2}')
```

**Ventajas**:
- `grep -o` extrae solo el patrón que coincide (ej: "R2: 9.500000")
- Funciona incluso si la línea está interrumpida por mensajes de otros threads
- `head -1` asegura que solo se tome el primer valor
- No depende de la posición de la columna

### 2. Valores Esperados Actualizados

#### Test 7 - PE0 (test_suma.asm)
- **Programa**: LOAD R0, R1 → FADD R2 → STORE
- **Resultado esperado**: R2 = 9.5 (6.0 + 3.5) ✓
- **Sin cambios**: Ya era correcto

#### Test 8 - PE1 (test_producto.asm)
- **Programa**: LOAD R0, R1 → FMUL R2 → FADD R3 → STORE
- **Resultado esperado**: R3 = 27.0 (6.0 * 3.5 + 6.0) ✓
- **Sin cambios**: Ya era correcto

#### Test 9 - PE2 (test_loop.asm)
- **Programa**: LOAD R0 (6.0) → 8x INC → LOOP (DEC R0, INC R1, JNZ)
- **Resultado esperado**: R0 = 0, R1 = 14 (6 + 8 = 14 iteraciones)
- **ANTES**: R0 = 10
- **AHORA**: R0 = 0, R1 = 14 ✓

```bash
# ANTES
if [ "$PE2_R0" == "10.000000" ]; then

# DESPUÉS
if [ "$PE2_R0" == "0.000000" ] && [ "$PE2_R1" == "14.000000" ]; then
```

#### Test 10 - PE3 (test_isa.asm)
- **Programa**: LOAD R0, R1 → FMUL R2 → FADD R3 (27) → LOOP (DEC R3 hasta 0)
- **Resultado esperado**: R3 = 0
- **ANTES**: R0 = 0, R1 = 8
- **AHORA**: R3 = 0 ✓

```bash
# ANTES
if [ "$PE3_R0" == "0.000000" ] && [ "$PE3_R1" == "8.000000" ]; then

# DESPUÉS
if [ "$PE3_R3" == "0.000000" ]; then
```

### 3. Labels Múltiples

**Test 13** - Actualizado para aceptar múltiples labels:

```bash
# ANTES
if [ "$LABELS" -eq 1 ]; then

# DESPUÉS
if [ "$LABELS" -ge 1 ]; then
```

**Razón**: Ahora hay 2 programas con label 'LOOP' (test_loop.asm y test_isa.asm)

## Estado Actual de los Programas ASM

### PE0: test_suma.asm
```asm
LOAD R0 100      # 6.0
LOAD R1 104      # 3.5
FADD R2 R0 R1    # R2 = 9.5
STORE R2 100
HALT
```
**Resultado**: R2 = 9.5 ✓

### PE1: test_producto.asm
```asm
LOAD R0 200      # 6.0
LOAD R1 204      # 3.5
FMUL R2 R0 R1    # R2 = 21.0
FADD R3 R2 R0    # R3 = 27.0
STORE R3 200
HALT
```
**Resultado**: R3 = 27.0 ✓

### PE2: test_loop.asm
```asm
LOAD R0 300      # 6.0
INC R0           # 8 veces → R0 = 14
...
LOOP:
    DEC R0       # Decrementa hasta 0
    INC R1       # Cuenta iteraciones
    JNZ R0 LOOP
HALT
```
**Resultado**: R0 = 0, R1 = 14 ✓

### PE3: test_isa.asm
```asm
LOAD R0 400      # 6.0
LOAD R1 404      # 3.5
FMUL R2 R0 R1    # R2 = 21.0
FADD R3 R2 R0    # R3 = 27.0
LOOP:
    DEC R3       # Decrementa hasta 0
    JNZ R3 LOOP
HALT
```
**Resultado**: R3 = 0 ✓

## Resultados Finales

```
======================================
         RESUMEN DE PRUEBAS
======================================
Total de pruebas: 13
Pasadas: 13
Falladas: 0
Porcentaje de éxito: 100%

✓✓✓ TODAS LAS PRUEBAS PASARON ✓✓✓
```

### Tests Exitosos
- ✅ TEST 1: Compilación
- ✅ TEST 2: Ejecución sin crashes
- ✅ TEST 3: Creación de 5 threads
- ✅ TEST 4: No hay señales duplicadas
- ✅ TEST 5: Terminación de PEs
- ✅ TEST 6: Terminación del bus
- ✅ TEST 7: PE0 R2 = 9.5 (suma)
- ✅ TEST 8: PE1 R3 = 27.0 (producto)
- ✅ TEST 9: PE2 R0=0, R1=14 (loop)
- ✅ TEST 10: PE3 R3=0 (loop)
- ✅ TEST 11: Writebacks MESI (⚠️ warn pero pasa)
- ✅ TEST 12: Invalidaciones (⚠️ warn pero pasa)
- ✅ TEST 13: Labels resueltos (2 labels)

## Mejoras Técnicas

1. **Robustez**: El parsing ahora es resiliente a output mezclado de threads
2. **Precisión**: Usa patrones regex específicos en lugar de índices de columna
3. **Mantenibilidad**: Más fácil entender qué valor se está buscando
4. **Flexibilidad**: Acepta múltiples ocurrencias de labels

## Archivos Modificados

- ✅ `tests/verify.sh`: Actualizado con parsing robusto y valores correctos
- ✅ Compatible con los nuevos programas ASM
- ✅ 100% de tests pasando

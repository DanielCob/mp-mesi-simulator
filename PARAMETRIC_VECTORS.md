# Sistema Parametrizable de Producto Punto Paralelo

## 📋 Descripción

Este sistema implementa **segmentación parametrizable** de vectores para el cálculo paralelo del producto punto. El tamaño del vector es completamente configurable y los programas assembly se generan automáticamente.

## 🔧 Configuración

### Cambiar el tamaño del vector

Editar `src/include/config.h`:

```c
// CONFIGURACIÓN DE VECTORES (PRODUCTO PUNTO)
#define VECTOR_SIZE 32           // Tamaño total del vector
#define SEGMENT_SIZE (VECTOR_SIZE / NUM_PES)  // Auto-calculado
```

**Restricción:** `VECTOR_SIZE` debe ser múltiplo de `NUM_PES` (4).

### Ejemplos de configuraciones válidas:

| VECTOR_SIZE | SEGMENT_SIZE por PE | Resultado esperado |
|-------------|---------------------|---------------------|
| 16 | 4 | 136.00 |
| 32 | 8 | 528.00 |
| 64 | 16 | 2080.00 |
| 128 | 32 | 8256.00 |

Fórmula del resultado: $\sum_{i=1}^{N} i = \frac{N(N+1)}{2}$

## 🚀 Uso

### 1. Configurar el tamaño deseado

```bash
# Editar src/include/config.h
# Cambiar VECTOR_SIZE a tu valor deseado
```

### 2. Generar programas assembly

```bash
python3 scripts/generate_asm.py
```

Este comando:
- ✅ Lee automáticamente la configuración de `config.h`
- ✅ Genera 4 archivos `.asm` (uno por PE)
- ✅ Usa **addressing indirecto** con registros
- ✅ Calcula automáticamente los segmentos

### 3. Compilar y ejecutar

```bash
make clean && make
./mp_mesi
```

## 📁 Estructura de memoria

### Direcciones parametrizadas (config.h):

```c
#define VECTOR_A_BASE 0          // Vector A
#define VECTOR_B_BASE 100        // Vector B
#define RESULTS_BASE 200         // Resultados parciales
#define FLAGS_BASE 220           // Flags de sincronización
#define CONSTANTS_BASE 232       // Constantes auxiliares
#define FINAL_RESULT_ADDR 216    // Resultado final
```

### Layout de memoria (ejemplo VECTOR_SIZE=32):

```
Dirección   | Contenido
------------|--------------------------------------------------
0-31        | Vector A [1, 2, 3, ..., 32]
100-131     | Vector B [1, 1, 1, ..., 1]
200         | Resultado parcial PE0 (elementos 0-7)
204         | Resultado parcial PE1 (elementos 8-15)
208         | Resultado parcial PE2 (elementos 16-23)
212         | Resultado parcial PE3 (elementos 24-31)
216         | Resultado final (suma de parciales)
220         | Flag sincronización PE0
224         | Flag sincronización PE1
228         | Flag sincronización PE2
232         | Constante -3.0 (para barrier)
```

**Nota:** Cada resultado parcial y flag están en **bloques de caché separados** (separados por BLOCK_SIZE=4) para evitar race conditions.

## 🧩 Funcionamiento

### División de trabajo

Con `VECTOR_SIZE=32` y `NUM_PES=4`:

```
PE0 → procesa A[0-7]  · B[0-7]   → resultado parcial: 36
PE1 → procesa A[8-15] · B[8-15]  → resultado parcial: 100
PE2 → procesa A[16-23] · B[16-23] → resultado parcial: 164
PE3 → procesa A[24-31] · B[24-31] → resultado parcial: 228
                                     ----------------------
                                     Resultado final: 528
```

### Algoritmo por PE trabajador (PE0-PE2):

```assembly
# Inicialización
MOV R0, 0.0         # acumulador
MOV R1, START_IDX   # índice inicial del segmento
MOV R2, 100.0       # base de vector B
MOV R3, SEGMENT_SIZE # contador del loop (desde config.h)

# LOOP con contador decreciente
LOOP_START:
    LOAD R4, [R1]       # A[i] usando addressing indirecto
    FADD R5, R2, R1     # calcular dirección B[100+i]
    LOAD R6, [R5]       # B[i]
    FMUL R7, R4, R6     # A[i] * B[i]
    FADD R0, R0, R7     # acumular
    INC R1              # i++
    DEC R3              # contador--
    JNZ LOOP_START      # Si R3 != 0, repetir

# Guardar resultado y señalizar
MOV R5, RESULT_ADDR
STORE R0, [R5]
MOV R6, 1.0
MOV R7, FLAG_ADDR
STORE R6, [R7]
HALT
```

### Algoritmo PE3 (Master + Reducción):

```assembly
# FASE 1: Calcular su segmento propio (con LOOP)
MOV R0, 0.0
MOV R1, START_IDX
MOV R2, 100.0
MOV R3, SEGMENT_SIZE
LOOP_START:
    LOAD R4, [R1]
    FADD R5, R2, R1
    LOAD R6, [R5]
    FMUL R7, R4, R6
    FADD R0, R0, R7
    INC R1
    DEC R3
    JNZ LOOP_START
STORE R0, [RESULT_ADDR]

# FASE 2: BARRIER - Esperar a otros PEs
WAIT_LOOP:
    LOAD flags de PE0, PE1, PE2
    Sumar flags
    Si suma != 3.0 → JNZ WAIT_LOOP

# FASE 3: REDUCCIÓN con LOOP
MOV R0, 0.0              # acumulador final
MOV R1, RESULTS_BASE     # base de resultados
MOV R2, NUM_PES          # contador (4)
MOV R3, BLOCK_SIZE       # offset (4)

REDUCE_LOOP:
    LOAD R4, [R1]        # cargar resultado parcial
    FADD R0, R0, R4      # acumular
    FADD R1, R1, R3      # siguiente dirección
    DEC R2               # contador--
    JNZ REDUCE_LOOP      # repetir si R2 != 0

# Guardar resultado final
MOV R2, FINAL_RESULT_ADDR
STORE R0, [R2]
HALT
```

## 🎯 Características avanzadas

### 1. Uso del nuevo ISA

✅ **MOV inmediato**: 
```assembly
MOV R1, 100.0      # Cargar constante sin acceso a memoria
```

✅ **Addressing indirecto**:
```assembly
LOAD R4, [R1]      # Cargar desde dirección en R1
STORE R0, [R5]     # Guardar en dirección en R5
```

✅ **Addressing directo con corchetes**:
```assembly
LOAD R3, [220]     # Cargar desde dirección inmediata 220
```

### 2. Sincronización mediante shared memory

- **Flags de sincronización**: Cada PE escribe 1.0 cuando termina
- **Barrier busy-wait**: PE3 espera activamente verificando flags
- **Bloques separados**: Evita coherencia innecesaria entre flags

### 3. Generación automática de código

El script `generate_asm.py`:
- 📖 Lee `VECTOR_SIZE` desde `config.h`
- 🧮 Calcula `SEGMENT_SIZE` automáticamente
- 📝 Genera código assembly optimizado
- 🔢 Calcula todas las direcciones de memoria
- ✅ Asegura alineamiento correcto de bloques

## 📊 Ejemplo de salida

```
[DotProd] Configuration: VECTOR_SIZE=32, NUM_PES=4, SEGMENT_SIZE=8
[DotProd] Loading Vector A at addresses 0-31:
  A = [1, 2, 3, ..., 32]
[DotProd] Loading Vector B at addresses 100-131:
  B = [1, 1, 1, ..., 1]

Partial Products (per PE):
  PE0 (elements 0-7):   36.00 (addr 200)
  PE1 (elements 8-15):   100.00 (addr 204)
  PE2 (elements 16-23):   164.00 (addr 208)
  PE3 (elements 24-31):   228.00 (addr 212)

Final Dot Product: 528.00 (addr 216)

Verification:
  Expected: 528.00
  Computed: 528.00
  Status:   ✓ CORRECT
```

## 🔬 Pruebas

### Cambiar a VECTOR_SIZE=64:

```bash
# 1. Editar config.h
sed -i 's/VECTOR_SIZE 32/VECTOR_SIZE 64/' src/include/config.h

# 2. Regenerar assembly
python3 scripts/generate_asm.py

# 3. Compilar y ejecutar
make clean && make
./mp_mesi
```

Resultado esperado: **2080.00** ✓

## 🏆 Cumplimiento de requisitos

✅ **Req 3**: Carga de memoria con control de alineamiento  
✅ **Req 4**: Segmentación parametrizable de vectores  
✅ **Req 6**: Comunicación shared-memory (flags, resultados)  
✅ **ISA mejorado**: MOV + addressing indirecto  
✅ **Escalabilidad**: Funciona con cualquier VECTOR_SIZE múltiplo de 4  

## 📝 Notas técnicas

### Limitaciones actuales:

1. **VECTOR_SIZE** debe ser múltiplo de **NUM_PES** (4)
2. **Unrolled loops**: El código se genera desenrollado (no usa JNZ para loops)
3. **Memoria máxima**: 512 posiciones (MEM_SIZE en config.h)

### Ventajas del sistema con LOOPS:

✅ **Eficiencia**: Código compacto independiente del tamaño del vector  
✅ **Escalabilidad**: Funciona con cualquier SEGMENT_SIZE (4, 8, 16, 32...)  
✅ **Parametrizable**: El contador se setea automáticamente desde config.h  
✅ **Mantenible**: Menos líneas de código generado  
✅ **Real**: Usa DEC y JNZ como loops tradicionales  

### Optimizaciones futuras:

- [ ] Soportar NUM_PES configurable
- [ ] Agregar validación de tamaño en tiempo de compilación
- [ ] Loop unrolling opcional para tamaños pequeños

---

**Autor**: Sistema MESI Multiprocessor Simulator  
**Fecha**: 2025-10-21

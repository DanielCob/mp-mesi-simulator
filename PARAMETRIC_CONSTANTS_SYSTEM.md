# Sistema de Constantes Parametrizadas

## Objetivo

Hacer que el código assembly sea **completamente reutilizable** sin necesidad de regenerar los archivos `.asm` cuando cambian parámetros como `VECTOR_SIZE`, `NUM_PES`, etc.

## Arquitectura

### Tabla de Constantes en Memoria

Todas las constantes del sistema se almacenan en una **tabla en memoria** que comienza en la dirección `240` (`CONSTANTS_BASE`).

#### Estructura de la Tabla

```
┌─────────────────────────────────────────────────────┐
│         CONSTANTES GLOBALES (240-246)               │
├──────────────┬──────────────────────────────────────┤
│  Dirección   │  Contenido                           │
├──────────────┼──────────────────────────────────────┤
│     240      │  VECTOR_B_BASE (100)                 │
│     241      │  RESULTS_BASE (200)                  │
│     242      │  FLAGS_BASE (220)                    │
│     243      │  BLOCK_SIZE (4)                      │
│     244      │  NUM_PES (4)                         │
│     245      │  FINAL_RESULT (216)                  │
│     246      │  BARRIER_CHECK (-(NUM_PES-1) = -3)   │
└──────────────┴──────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│      CONSTANTES POR PE (248+)                       │
│      4 valores por PE en direcciones consecutivas   │
├──────────────┬──────────────────────────────────────┤
│   PE0 (248-251)                                     │
│     248      │  start_index = 0                     │
│     249      │  segment_size = 4                    │
│     250      │  result_addr = 200                   │
│     251      │  flag_addr = 220                     │
├──────────────┼──────────────────────────────────────┤
│   PE1 (252-255)                                     │
│     252      │  start_index = 4                     │
│     253      │  segment_size = 4                    │
│     254      │  result_addr = 204                   │
│     255      │  flag_addr = 224                     │
├──────────────┼──────────────────────────────────────┤
│   PE2 (256-259)                                     │
│     256      │  start_index = 8                     │
│     257      │  segment_size = 4                    │
│     258      │  result_addr = 208                   │
│     259      │  flag_addr = 228                     │
├──────────────┼──────────────────────────────────────┤
│   PE3 (260-263)                                     │
│     260      │  start_index = 12                    │
│     261      │  segment_size = 4                    │
│     262      │  result_addr = 212                   │
│     263      │  flag_addr = 232                     │
└──────────────┴──────────────────────────────────────┘
```

### Definiciones en C (`config.h`)

```c
// Tabla de constantes (base address 240)
#define CONSTANTS_BASE 240

// Constantes globales (direcciones 240-246)
#define CONST_VECTOR_B_BASE    240  // Dirección base de vector B
#define CONST_RESULTS_BASE     241  // Dirección base de resultados parciales
#define CONST_FLAGS_BASE       242  // Dirección base de flags de sincronización
#define CONST_BLOCK_SIZE       243  // Tamaño de bloque de caché
#define CONST_NUM_PES          244  // Número de PEs
#define CONST_FINAL_RESULT     245  // Dirección del resultado final
#define CONST_BARRIER_CHECK    246  // Valor para verificar barrier (-(NUM_PES-1))

// Constantes por PE (base address 248)
#define CONST_PE_BASE 248
#define CONST_PE(pe_id, offset) (CONST_PE_BASE + (pe_id) * 4 + (offset))

// Offsets dentro de cada bloque de PE
#define PE_START_INDEX    0  // Índice inicial del segmento
#define PE_SEGMENT_SIZE   1  // Tamaño del segmento
#define PE_RESULT_ADDR    2  // Dirección donde guardar resultado parcial
#define PE_FLAG_ADDR      3  // Dirección del flag de sincronización
```

### Inicialización en C (`dotprod.c`)

```c
// Inicializar constantes globales
mem->data[CONST_VECTOR_B_BASE] = (double)VECTOR_B_BASE;
mem->data[CONST_RESULTS_BASE] = (double)RESULTS_BASE;
mem->data[CONST_FLAGS_BASE] = (double)FLAGS_BASE;
mem->data[CONST_BLOCK_SIZE] = (double)BLOCK_SIZE;
mem->data[CONST_NUM_PES] = (double)NUM_PES;
mem->data[CONST_FINAL_RESULT] = (double)FINAL_RESULT_ADDR;
mem->data[CONST_BARRIER_CHECK] = (double)(-(NUM_PES - 1));

// Inicializar constantes por PE
for (int pe = 0; pe < NUM_PES; pe++) {
    mem->data[CONST_PE(pe, PE_START_INDEX)] = (double)start_idx;
    mem->data[CONST_PE(pe, PE_SEGMENT_SIZE)] = (double)segment_size;
    mem->data[CONST_PE(pe, PE_RESULT_ADDR)] = (double)(RESULTS_BASE + pe * BLOCK_SIZE);
    mem->data[CONST_PE(pe, PE_FLAG_ADDR)] = (double)(FLAGS_BASE + pe * BLOCK_SIZE);
}
```

## Uso en Assembly

### Patrón de Carga de Constantes

Todos los archivos `.asm` siguen el mismo patrón para cargar constantes:

```assembly
# Cargar constante global
MOV R4, 240.0        # Dirección de CONST_VECTOR_B_BASE
LOAD R2, [R4]        # R2 = valor de VECTOR_B_BASE (100.0)

# Cargar constantes específicas del PE
MOV R4, 248.0        # Base de constantes para PE0 (248 + 0*4)
LOAD R1, [R4]        # R1 = start_index
INC R4               # Siguiente dirección
LOAD R3, [R4]        # R3 = segment_size
INC R4
LOAD R5, [R4]        # R5 = result_addr
INC R4
LOAD R7, [R4]        # R7 = flag_addr
```

### Ejemplo Completo (Worker PE)

```assembly
# ============================================================================
# FASE 1: Carga de constantes
# ============================================================================

# Constantes globales
MOV R4, 240.0        # CONST_VECTOR_B_BASE
LOAD R2, [R4]        # R2 = VECTOR_B_BASE

# Constantes específicas de PE0 (direcciones 248-251)
MOV R4, 248.0        # Base de constantes para PE0
LOAD R1, [R4]        # R1 = start_index
INC R4
LOAD R3, [R4]        # R3 = segment_size (contador del loop)
INC R4
LOAD R5, [R4]        # R5 = result_addr
INC R4
LOAD R7, [R4]        # R7 = flag_addr

# ============================================================================
# FASE 2: Cálculo del producto parcial
# ============================================================================
MOV R0, 0.0         # Acumulador

LOOP_START:
LOAD R4, [R1]       # R4 = A[i]
FADD R6, R2, R1     # R6 = VECTOR_B_BASE + i
LOAD R6, [R6]       # R6 = B[i]
FMUL R4, R4, R6     # R4 = A[i] * B[i]
FADD R0, R0, R4     # acumulador += producto
INC R1              # i++
DEC R3              # contador--
JNZ LOOP_START      # Repetir si contador != 0

# ============================================================================
# FASE 3: Guardar resultado y señalizar
# ============================================================================
STORE R0, [R5]      # Guardar resultado parcial (R5 = result_addr)
MOV R6, 1.0         # Flag value
STORE R6, [R7]      # Señalizar finalización (R7 = flag_addr)

HALT
```

## Ventajas del Sistema

### 1. **Reutilización de Código**
Los mismos archivos `.asm` funcionan con cualquier configuración:
- Diferentes tamaños de vector (`VECTOR_SIZE`)
- Diferente número de PEs (`NUM_PES`)
- Diferentes distribuciones de trabajo

### 2. **Workflow Simplificado**

**ANTES** (código hardcodeado):
```bash
# Cambiar VECTOR_SIZE en config.h
vim src/include/config.h

# Regenerar archivos .asm
python3 scripts/generate_asm.py

# Recompilar
make clean && make
./mp_mesi
```

**AHORA** (código parametrizado):
```bash
# Cambiar VECTOR_SIZE en config.h
vim src/include/config.h

# Solo recompilar (NO regenerar .asm)
make clean && make
./mp_mesi
```

### 3. **Separación de Responsabilidades**

- **Código estático** (`.asm`): Lógica del algoritmo, nunca cambia
- **Configuración dinámica** (tabla de constantes): Se actualiza en cada compilación

## Generación de Assembly

El script `generate_asm.py` ahora genera código **genérico** que:

1. **NO contiene valores hardcodeados** de configuración
2. **Carga todo desde memoria** usando la tabla de constantes
3. **Es idéntico** para diferentes configuraciones (solo cambian los comentarios con el `pe_id`)

### Ejemplo de Generación

```python
def generate_worker_pe(pe_id):
    pe_const_base = 248 + pe_id * 4  # CONST_PE_BASE + pe_id * 4
    
    code = f"""
# Constantes globales
MOV R4, 240.0        # CONST_VECTOR_B_BASE
LOAD R2, [R4]        # R2 = VECTOR_B_BASE

# Constantes específicas de PE{pe_id}
MOV R4, {float(pe_const_base)}  # Base de constantes para PE{pe_id}
LOAD R1, [R4]        # R1 = start_index
INC R4
LOAD R3, [R4]        # R3 = segment_size
# ... resto del código
"""
    return code
```

**Nota**: El único valor que cambia entre PEs es `pe_const_base` (248, 252, 256, 260), que indica dónde leer las constantes específicas de cada PE.

## Casos de Uso

### Cambiar Tamaño de Vector

```c
// En config.h
#define VECTOR_SIZE 20  // Era 16

// Recompilar (sin regenerar .asm)
// Los mismos archivos .asm funcionarán porque:
// - Cargarán el nuevo segment_size desde memoria
// - Procesarán 5 elementos por PE en vez de 4
```

### Cambiar Distribución de Trabajo

```c
// En dotprod.c
int segment_sizes[] = {3, 5, 4, 4};  // Distribución custom

// Actualizar tabla de constantes
for (int pe = 0; pe < NUM_PES; pe++) {
    mem->data[CONST_PE(pe, PE_SEGMENT_SIZE)] = (double)segment_sizes[pe];
}

// Los .asm ya existentes se adaptarán automáticamente
```

## Verificación del Sistema

Para verificar que el sistema parametrizado funciona:

```bash
# 1. Ejecutar con VECTOR_SIZE=16
make clean && make && ./mp_mesi

# 2. Cambiar VECTOR_SIZE=20 en config.h
vim src/include/config.h

# 3. Solo recompilar (SIN regenerar .asm)
make clean && make && ./mp_mesi

# 4. Verificar que:
#    - Los archivos .asm NO cambiaron (git status)
#    - El resultado es correcto (210.0 para vector de unos)
#    - Cada PE procesó 5 elementos (en vez de 4)
```

## Resultados

Con `VECTOR_SIZE=16`:
- PE0: elementos 0-3 = 1+2+3+4 = **10.0**
- PE1: elementos 4-7 = 5+6+7+8 = **26.0**
- PE2: elementos 8-11 = 9+10+11+12 = **42.0**
- PE3: elementos 12-15 = 13+14+15+16 = **58.0**
- **Total: 136.0** ✅

## Implementación Técnica

### Direccionamiento Indirecto en 2 Pasos

```assembly
# Paso 1: Cargar la DIRECCIÓN de la constante
MOV R4, 240.0        # R4 = dirección (240)

# Paso 2: Cargar el VALOR desde esa dirección
LOAD R2, [R4]        # R2 = mem[240] = 100.0 (VECTOR_B_BASE)
```

Este patrón se repite para todas las constantes, permitiendo que el código assembly sea agnóstico de los valores reales.

### Optimización: MOV Inmediato para Constantes Fijas

Algunas constantes **nunca cambian** y pueden optimizarse:

```assembly
# Constante que NUNCA cambia: el valor -3.0 para el barrier (con NUM_PES=4)
MOV R7, -3.0         # Directo (MOV immediate)

# En vez de:
MOV R4, 246.0        # CONST_BARRIER_CHECK
LOAD R7, [R4]        # Cargar desde memoria
```

**Nota**: Esta optimización solo se aplica si `NUM_PES` es constante. Si `NUM_PES` puede variar, se debe cargar desde memoria.

## Resumen

El sistema de constantes parametrizadas logra:

✅ **Código assembly reutilizable** - Mismo `.asm` para cualquier configuración  
✅ **Workflow simplificado** - Solo recompilar, no regenerar  
✅ **Separación clara** - Lógica vs. configuración  
✅ **Mantenibilidad** - Cambios en un solo lugar (config.h)  
✅ **Flexibilidad** - Fácil experimentar con diferentes parámetros  

---

**Archivo generado**: 2024  
**Proyecto**: MESI Multiprocessor Simulator - Parametric Constants System

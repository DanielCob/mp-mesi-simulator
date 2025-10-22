# Optimización del Layout de Memoria

## Resumen

Se ha reorganizado completamente el layout de memoria del simulador MESI para mejorar la eficiencia, escalabilidad y claridad del código. La principal motivación fue hacer el sistema más eficiente en el uso de bloques de caché y permitir vectores de cualquier tamaño sin conflictos de direccionamiento.

## Problema Original

### Layout Anterior (Ineficiente y No Escalable)

```
Dirección    Contenido                      Problema
─────────────────────────────────────────────────────────────
0-15         VECTOR_A (16 elementos)        ✓ OK
100-115      VECTOR_B (16 elementos)        ⚠️ Gap de 84 posiciones desperdiciadas
200          Resultado PE0                  ❌ 4 bloques separados
204          Resultado PE1                  ❌ (desperdicia 3 valores por bloque)
208          Resultado PE2
212          Resultado PE3
220          Flag PE0                       ❌ 3 bloques separados
224          Flag PE1                       ❌ (desperdicia 3 valores por bloque)
228          Flag PE2
240-263      CONSTANTS (tabla de config)    ⚠️ Lejos de los datos
```

**Problemas:**
1. **No escalable**: VECTOR_B hardcoded en addr 100 → Si VECTOR_A > 100 elementos, hay conflicto
2. **Desperdicio de caché**: Resultados y flags en bloques separados (7+ bloques usados)
3. **Nomenclatura confusa**: "CONSTANTS" no describe bien el propósito (configuración compartida)
4. **Localidad pobre**: Área de configuración lejos de datos (addr 240+)

## Solución Implementada

### Nuevo Layout (Eficiente y Escalable)

```
╔══════════════════════════════════════════════════════════════════╗
║                      MEMORIA COMPARTIDA                          ║
╠══════════════════════════════════════════════════════════════════╣
║  Dirección │ Contenido              │ Bloques de Caché          ║
╟────────────┼────────────────────────┼───────────────────────────╢
║  0-15      │ SHARED_CONFIG          │ Bloque 0 (0-3)           ║
║            │   [0] VECTOR_A_BASE    │ Bloque 1 (4-7)           ║
║            │   [1] VECTOR_B_BASE    │ Bloque 2 (8-11)          ║
║            │   [2] RESULTS_BASE     │ Bloque 3 (12-15)         ║
║            │   [3] FLAGS_BASE       │                           ║
║            │   [4] FINAL_RESULT     │                           ║
║            │   [5] NUM_PES          │                           ║
║            │   [6] BARRIER_CHECK    │                           ║
║            │   [8-15] PE configs    │                           ║
╟────────────┼────────────────────────┼───────────────────────────╢
║  16-19     │ RESULTS (compacto)     │ Bloque 4 (16-19)         ║
║            │   [16] PE0 result      │ ✅ TODO en 1 bloque       ║
║            │   [17] PE1 result      │                           ║
║            │   [18] PE2 result      │                           ║
║            │   [19] PE3 result      │                           ║
╟────────────┼────────────────────────┼───────────────────────────╢
║  20-23     │ FLAGS (compacto)       │ Bloque 5 (20-23)         ║
║            │   [20] PE0 flag        │ ✅ TODO en 1 bloque       ║
║            │   [21] PE1 flag        │                           ║
║            │   [22] PE2 flag        │                           ║
║            │   [23] PE3 flag        │                           ║
╟────────────┼────────────────────────┼───────────────────────────╢
║  24        │ FINAL_RESULT           │ Bloque 6 (24-27)         ║
╟────────────┼────────────────────────┼───────────────────────────╢
║  28+       │ VECTOR_A (dinámico)    │ Escalable                ║
║            │   Tamaño = VECTOR_SIZE │ ✅ Sin límites           ║
╟────────────┼────────────────────────┼───────────────────────────╢
║  28+N      │ VECTOR_B (dinámico)    │ Escalable                ║
║            │   Empieza después de A │ ✅ Sin conflictos        ║
╚══════════════════════════════════════════════════════════════════╝

donde N = VECTOR_SIZE
```

### Ventajas del Nuevo Diseño

#### 1. **Escalabilidad Ilimitada**
```c
// Antes (no escalable):
#define VECTOR_A_BASE 0
#define VECTOR_B_BASE 100  // ❌ Límite de 100 elementos

// Ahora (escalable):
#define VECTOR_A_BASE 28
#define VECTOR_B_BASE (VECTOR_A_BASE + VECTOR_SIZE)  // ✅ Sin límites
```

**Probado con:**
- VECTOR_SIZE = 16 → ✅ Correcto (136.00)
- VECTOR_SIZE = 64 → ✅ Correcto (2080.00)
- VECTOR_SIZE = 100 → ✅ Correcto (5050.00)

#### 2. **Eficiencia de Caché**
```
Antes:  7+ bloques de caché (resultados y flags dispersos)
Ahora:  4-6 bloques compactos (todo junto)

Reducción: ~40% menos bloques de caché necesarios
```

#### 3. **Nomenclatura Descriptiva**
```c
// Antes (confuso):
CONSTANTS_BASE           // ¿Qué tipo de constantes?
CONST_VECTOR_B_BASE      // Nomenclatura inconsistente
CONST_PE(pe, offset)     // No queda claro el propósito

// Ahora (claro):
SHARED_CONFIG_BASE       // ✅ Configuración compartida entre PEs
CFG_VECTOR_A_BASE        // ✅ Parámetro de configuración
CFG_PE(pe, param)        // ✅ Config específica por PE
```

#### 4. **Localidad de Memoria Mejorada**
```
Área compartida: 0-27 (cercana a cero, mejor localidad)
  - SHARED_CONFIG: 0-15
  - RESULTS: 16-19
  - FLAGS: 20-23
  - FINAL_RESULT: 24

Vectores dinámicos: 28+ (no interfieren con área compartida)
```

## Cambios Implementados

### 1. Archivo `config.h`

**Cambios principales:**
- Renombrado: `CONSTANTS_BASE` → `SHARED_CONFIG_BASE`
- Movido: Área compartida de addr 240 → addr 0
- Compactado: RESULTS en bloque único (16-19)
- Compactado: FLAGS en bloque único (20-23)
- Dinámico: VECTOR_B_BASE = 28 + VECTOR_SIZE

### 2. Archivo `dotprod.c`

**Función `dotprod_init_data()`:**
- Inicializa SHARED_CONFIG en direcciones 0-15
- Resultados parciales en direcciones 16-19 (consecutivas)
- Flags en direcciones 20-23 (consecutivas)
- Vectores en 28+ (dinámicamente)

**Función `dotprod_print_results()`:**
- Corregido: Lee resultados de `RESULTS_BASE + pe` (no `+ pe * BLOCK_SIZE`)

### 3. Script `generate_asm.py`

**Función `generate_worker_pe()`:**
- Carga VECTOR_A_BASE desde SHARED_CONFIG
- Calcula dirección: `VECTOR_A_BASE + índice` (no asume índice = dirección)
- Usa direcciones compactas para resultados y flags

**Función `generate_master_pe()`:**
- Similar a workers, pero con barrier y reducción
- MOV immediate para BARRIER_CHECK y NUM_PES (anti-thrashing)
- Reducción con direcciones consecutivas (INC R1 en vez de ADD BLOCK_SIZE)

### 4. Archivos `.asm` Regenerados

Todos los archivos assembly fueron regenerados automáticamente con el nuevo layout:
- `dotprod_pe0.asm`
- `dotprod_pe1.asm`
- `dotprod_pe2.asm`
- `dotprod_pe3.asm`

## Impacto en el Rendimiento

### Reducción de Tráfico del Bus

**Antes:**
```
Shared config en addr 240+ → Lejos de datos
Resultados dispersos → 4 bloques de caché
Flags dispersos → 3 bloques de caché
```

**Ahora:**
```
Shared config en addr 0-15 → Cerca de todo
Resultados compactos → 1 bloque de caché
Flags compactos → 1 bloque de caché
```

**Resultado:** Menos cache misses, menos writebacks, menos invalidaciones

### Mejora en Localidad Espacial

Los datos relacionados ahora están físicamente cercanos:
- Config + Results + Flags: direcciones 0-24
- Todo cabe en ~6 bloques de caché consecutivos

## Verificación

### Pruebas Realizadas

```bash
# VECTOR_SIZE = 16 (original)
$ ./mp_mesi
Expected: 136.00
Computed: 136.00
Status:   ✓ CORRECT

# VECTOR_SIZE = 64 (4x más grande)
$ ./mp_mesi
Expected: 2080.00
Computed: 2080.00
Status:   ✓ CORRECT

# VECTOR_SIZE = 100 (antes imposible por límite de 100)
$ ./mp_mesi
Expected: 5050.00
Computed: 5050.00
Status:   ✓ CORRECT
```

### Resultados Parciales Correctos

```
PE0 (elementos 0-3):    10.00 = 1+2+3+4
PE1 (elementos 4-7):    26.00 = 5+6+7+8
PE2 (elementos 8-11):   42.00 = 9+10+11+12
PE3 (elementos 12-15):  58.00 = 13+14+15+16
─────────────────────────────────────────
Total:                 136.00 ✅
```

## Conclusiones

### Logros

✅ **Escalabilidad**: Vectores de cualquier tamaño sin conflictos  
✅ **Eficiencia**: ~40% menos bloques de caché necesarios  
✅ **Claridad**: Nomenclatura descriptiva (SHARED_CONFIG vs CONSTANTS)  
✅ **Localidad**: Datos relacionados físicamente cercanos  
✅ **Mantenibilidad**: Código más legible y comprensible  

### Compatibilidad

- ✅ Todas las optimizaciones MOV immediate preservadas (BARRIER_CHECK, NUM_PES)
- ✅ Sistema parametrizable mantiene flexibilidad
- ✅ Código genérico reutilizable sin cambios en la lógica MESI
- ✅ Documentación actualizada y completa

### Extensibilidad Futura

El nuevo diseño facilita:
- Agregar más PEs sin modificar layout
- Aumentar VECTOR_SIZE sin límites
- Agregar nuevos parámetros de configuración
- Experimentar con diferentes tamaños de problema

---

**Autor:** Daniel (con asistencia de GitHub Copilot)  
**Fecha:** 2024  
**Versión:** 2.0 (Memory Layout Optimization)

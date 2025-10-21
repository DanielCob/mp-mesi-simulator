````markdown
# Changelog - MESI Multiprocessor Simulator

## [2.1.0] - 2025-10-21 - Implementación de LOOPS Reales

### ✨ Mejoras principales

#### **LOOPS con contador decreciente (DEC + JNZ)**

**PE Trabajadores (PE0-PE2):**
```assembly
MOV R3, SEGMENT_SIZE    # Contador desde config.h
LOOP_START:
    [procesar elemento]
    INC R1                # índice++
    DEC R3                # contador--
    JNZ LOOP_START        # Repetir si R3 != 0
```

**PE Master (PE3) - 3 LOOPS:**
1. **LOOP_START**: Procesa su segmento propio
2. **WAIT_LOOP**: Barrier de sincronización
3. **REDUCE_LOOP**: Suma resultados parciales con contador

**Ventajas:**
- ✅ Código compacto independiente del tamaño
- ✅ Escalable: SEGMENT_SIZE de 4, 8, 16, 32...
- ✅ Parametrizable: Contador seteado desde config.h
- ✅ Real: Usa DEC/JNZ como loops tradicionales

**Archivos modificados:**
- `scripts/generate_asm.py`: Generador con loops
- `asm/dotprod_pe*.asm`: Generados con loops
- `PARAMETRIC_VECTORS.md`: Documentación actualizada

**Pruebas:**
- ✅ VECTOR_SIZE=16, SEGMENT_SIZE=4 → 136.00
- ✅ VECTOR_SIZE=32, SEGMENT_SIZE=8 → 528.00

---

## [2.0.0] - 2025-10-21 - Sistema Parametrizable y ISA Mejorado

### ✨ Nuevas funcionalidades principales

#### 1. **ISA Mejorado con MOV y Addressing Modes**

**Nueva instrucción MOV:**
- Sintaxis: `MOV Rd, immediate_value`
- Ejemplo: `MOV R1, 100.0`
- Beneficio: Carga de constantes sin acceso a memoria ni caché

**Addressing modes:**
- **Directo con corchetes**: `LOAD R4, [100]` - Dirección inmediata
- **Indirecto con registro**: `LOAD R4, [R1]` - Dirección contenida en R1 (pointer-like)
- **Sintaxis estandarizada**: Corchetes `[]` obligatorios para todas las operaciones de memoria

**Formato legacy eliminado:**
- ❌ Ya no soportado: `LOAD R4, 100` (sin corchetes)
- ✅ Requerido: `LOAD R4, [100]` o `LOAD R4, [R1]`

**Archivos modificados:**
- `src/pe/isa.h`: Añadido `OP_MOV`, `AddressingMode`, campo `imm` en Instruction
- `src/pe/isa.c`: Implementada ejecución de MOV y addressing modes
- `src/pe/loader.c`: Parser actualizado para nuevos formatos
- `src/pe/pe.c`: Display de instrucciones actualizado

#### 2. **Sistema Parametrizable de Vectores**

**Configuración centralizada en config.h:**
```c
#define VECTOR_SIZE 16           // Configurable
#define SEGMENT_SIZE (VECTOR_SIZE / NUM_PES)  // Auto-calculado
#define VECTOR_A_BASE 0
#define VECTOR_B_BASE 100
#define RESULTS_BASE 200
#define FLAGS_BASE 220
#define CONSTANTS_BASE 232
#define FINAL_RESULT_ADDR 216
```

**Generador automático de código assembly:**
- Script: `scripts/generate_asm.py`
- Lee configuración desde `config.h`
- Genera 4 archivos `.asm` parametrizados (uno por PE)
- Calcula automáticamente segmentos y direcciones
- Uso: `python3 scripts/generate_asm.py`

**Escalabilidad probada:**
- ✅ VECTOR_SIZE=16 → Resultado: 136.00
- ✅ VECTOR_SIZE=32 → Resultado: 528.00
- ✅ VECTOR_SIZE=64 → Resultado: 2080.00
- ✅ Cualquier múltiplo de NUM_PES (4)

**Archivos creados/modificados:**
- `src/include/config.h`: Constantes parametrizables
- `src/dotprod/dotprod.c`: Inicialización dinámica basada en VECTOR_SIZE
- `scripts/generate_asm.py`: Generador automático de programas
- `asm/dotprod_pe*.asm`: Generados automáticamente

### 🔧 Mejoras de código

#### Consolidación de módulos MESI
- **Eliminados**: `src/mesi/mesi.c` y `src/mesi/mesi.h` (redundantes)
- **Consolidado**: `MESI_State` movido a `config.h`
- **Beneficio**: Configuración centralizada, menos archivos

#### Organización y limpieza
- **cache.c**: 7 secciones claramente delimitadas
- **bus.c**: 3 secciones con Round-Robin mejorado
- **handlers.c**: 5 secciones (un handler por sección)
- **memory.c**: 3 secciones bien estructuradas
- **Eliminados**: Comentarios redundantes (flechas `←`)
- **Mejorados**: Comentarios concisos y útiles

### 📝 Documentación

**Nuevos documentos:**
- `PARAMETRIC_VECTORS.md`: Guía completa del sistema parametrizable
  - Configuración de VECTOR_SIZE
  - Uso del generador de assembly
  - Ejemplos de múltiples tamaños
  - Explicación detallada del algoritmo
  - Características del nuevo ISA

**Actualizaciones README.md:**
- Tabla ISA actualizada con MOV y addressing modes
- Sección de compilación con ejemplos parametrizables
- Referencia a PARAMETRIC_VECTORS.md
- Resultados esperados para múltiples tamaños

### 🎯 Cumplimiento de requisitos del proyecto

- ✅ **Req 3**: Carga de memoria con control de alineamiento
- ✅ **Req 4**: Segmentación de memoria parametrizable  
- ✅ **Req 6**: Comunicación shared-memory para combinar resultados
- ✅ ISA completo con addressing modes
- ✅ Sistema escalable y mantenible

### 🧪 Testing

**Configuraciones probadas:**
```
VECTOR_SIZE | SEGMENT_SIZE | Resultado | Estado
------------|--------------|-----------|--------
16          | 4            | 136.00    | ✅
32          | 8            | 528.00    | ✅
64          | 16           | 2080.00   | ✅
```

**Métricas:**
- Tasa de éxito: 100% (con `sched_yield()`)
- Sin race conditions
- Protocolo MESI funcionando correctamente
- Estadísticas coherentes

### 🚀 Cómo usar el nuevo sistema

```bash
# 1. Configurar tamaño deseado
# Editar src/include/config.h → VECTOR_SIZE

# 2. Generar programas assembly
python3 scripts/generate_asm.py

# 3. Compilar y ejecutar
make clean && make
./mp_mesi
```

---

## [1.0.0] - 2025-10-20 - Limpieza y Consolidación

### ✨ Mejoras Realizadas

#### 1. **Reorganización de cache.c**
- ✅ Separación clara en 7 secciones con delimitadores
- ✅ Agrupación lógica por funcionalidad
- ✅ Eliminación de comentarios redundantes
- **Secciones:** Callbacks, Inicialización, LRU, Read/Write, Víctimas, Bus helpers, Flush

#### 2. **Reorganización de bus.c**
- ✅ Estructura modular con 3 secciones
- ✅ Round-robin mejorado
- **Secciones:** Inicialización, Broadcast, Thread

#### 3. **Reorganización de handlers.c**
- ✅ Clara separación entre handlers MESI (5 secciones)
- ✅ Un handler por sección
- **Secciones:** Registro, BUS_RD, BUS_RDX, BUS_UPGR, BUS_WB

#### 4. **Reorganización de memory.c**
- ✅ Dividido en 3 secciones lógicas
- ✅ Eliminados comentarios innecesarios
- **Secciones:** Inicialización, Operaciones, Thread

#### 5. **Consolidación MESI**
- ✅ Eliminado directorio `src/mesi/`
- ✅ `MESI_State` consolidado en `config.h`
- ✅ Actualizado makefile

### 📊 Resultados

- ✅ 50/50 ejecuciones exitosas (100%)
- ✅ Producto punto: 136.00 ✓
- ✅ Sin race conditions
- ✅ Compilación sin warnings

### 🎯 Beneficios

1. **Legibilidad**: Código más fácil de entender
2. **Modularidad**: Funciones bien agrupadas
3. **Navegación**: Secciones claras
4. **Mantenibilidad**: Más fácil extender

---

## [0.9.0] - 2025-10-19 - Implementación Inicial

### Funcionalidades

- Protocolo MESI completo
- Sistema de caché 2-way set associative
- Bus con arbitraje Round-Robin
- 4 Processing Elements con threading
- Callback architecture para atomicidad
- Producto punto paralelo (tamaño fijo 16)
- Estadísticas detalladas
- Sincronización con barriers

### Problemas resueltos

- ✅ Race condition en BUS_RDX (callback architecture)
- ✅ Scheduling justo (sched_yield)
- ✅ Deadlock en mutex
- ✅ False sharing (bloques separados)

---

**Convenciones de versioning:**
- **Major**: Cambios incompatibles (ISA, arquitectura)
- **Minor**: Nuevas funcionalidades compatibles
- **Patch**: Bug fixes

````

### ✨ Mejoras Realizadas

#### 1. **Reorganización de cache.c**
- ✅ Separación clara en secciones con comentarios delimitadores
- ✅ Agrupación lógica de funciones por funcionalidad
- ✅ Eliminación de comentarios redundantes (flechas `←`)
- ✅ Simplificación de comentarios manteniendo solo información esencial
- **Secciones creadas:**
  ```
  - ESTRUCTURAS Y CALLBACKS PRIVADOS
  - FUNCIONES DE INICIALIZACIÓN Y LIMPIEZA
  - POLÍTICA LRU
  - OPERACIONES DE LECTURA Y ESCRITURA
  - SELECCIÓN DE VÍCTIMA Y POLÍTICAS DE REEMPLAZO
  - FUNCIONES AUXILIARES PARA HANDLERS DEL BUS
  - CACHE FLUSH (WRITEBACK AL HALT)
  ```

#### 2. **Reorganización de bus.c**
- ✅ Estructura modular con secciones bien definidas
- ✅ Eliminación de comentarios obsoletos
- ✅ Simplificación del código de round-robin
- **Secciones creadas:**
  ```
  - INICIALIZACIÓN Y LIMPIEZA
  - FUNCIONES DE BROADCAST
  - THREAD DEL BUS (Round-Robin)
  ```

#### 3. **Reorganización de handlers.c**
- ✅ Clara separación entre handlers MESI
- ✅ Documentación concisa de cada handler
- ✅ Eliminación de comentarios redundantes sobre thread-safety
- **Secciones creadas:**
  ```
  - REGISTRO DE HANDLERS
  - HANDLER: BUS_RD (Lectura compartida)
  - HANDLER: BUS_RDX (Lectura exclusiva para escritura)
  - HANDLER: BUS_UPGR (Upgrade de Shared a Modified)
  - HANDLER: BUS_WB (Writeback a memoria)
  ```

#### 4. **Actualización del README.md**
- ✅ Añadida sección completa de "Arquitectura de la solución implementada"
- ✅ Documentación de la organización del código
- ✅ Explicación detallada de cada componente
- ✅ Diagrama de transiciones de estados MESI
- ✅ Explicación de la solución al race condition con callbacks
- ✅ Documentación de estadísticas recolectadas

### 📊 Resultados de Testing

**Pruebas realizadas:**
- ✅ 50/50 ejecuciones exitosas (100%)
- ✅ Producto punto correcto: 136.00
- ✅ Sin race conditions detectados
- ✅ Scheduling justo con `sched_yield()`

### 🎯 Beneficios de la Limpieza

1. **Legibilidad:** Código más fácil de entender y mantener
2. **Modularidad:** Funciones bien agrupadas por responsabilidad
3. **Documentación:** Comentarios concisos y útiles
4. **Navegación:** Secciones claras facilitan encontrar código específico
5. **Mantenibilidad:** Más fácil agregar nuevas funcionalidades

### 📝 Archivos Modificados

```
src/cache/cache.c          ✓ Limpiado y reorganizado
src/bus/bus.c              ✓ Limpiado y reorganizado
src/bus/handlers.c         ✓ Limpiado y reorganizado
README.md                  ✓ Actualizado con arquitectura completa
CHANGELOG.md               ✓ Nuevo archivo
```

### 🔧 Estado del Proyecto

- **Compilación:** ✅ Sin errores ni warnings
- **Funcionalidad:** ✅ 100% de tests pasando
- **Documentación:** ✅ Completa y actualizada
- **Código limpio:** ✅ Sin comentarios obsoletos

---

**Mantenedores:** Daniel Cob  
**Fecha:** 20 de Octubre, 2025

## 2025-10-20 - Consolidación y Limpieza de Módulos

### ✨ Eliminación de módulo mesi redundante

#### **Cambios realizados:**

1. **Eliminación de archivos mesi.c/h**
   - ✅ Removidos `src/mesi/mesi.c` y `src/mesi/mesi.h`
   - ✅ Eliminado directorio `src/mesi/`
   - ✅ La función `mesi_state_to_str()` no se usaba en ningún lugar

2. **Consolidación de MESI_State en config.h**
   - ✅ `MESI_State` enum movido a `src/include/config.h`
   - ✅ Documentación mejorada de cada estado MESI
   - ✅ Estructura más lógica: configuración centralizada

3. **Reorganización de memory.c/h**
   - ✅ Dividido en secciones claras:
     * INICIALIZACIÓN Y LIMPIEZA
     * OPERACIONES DE LECTURA Y ESCRITURA DE BLOQUES
     * THREAD DE MEMORIA
   - ✅ Eliminados comentarios redundantes
   - ✅ Código más limpio y profesional

4. **Actualización de dependencias**
   - ✅ `cache.h`: Ya no incluye `mesi.h`
   - ✅ `dotprod.c`: Ya no incluye `mesi/mesi.h`
   - ✅ `makefile`: Removida referencia a `-I$(SRC_DIR)/mesi`

### 📊 Impacto

**Antes:**
```
src/mesi/mesi.c        (12 líneas - función no usada)
src/mesi/mesi.h        (8 líneas)
Total: 20 líneas de código innecesario
```

**Después:**
```
MESI_State consolidado en config.h (con documentación)
Código más simple y mantenible
```

### ✅ Validación

- ✅ **50/50 tests exitosos** (100%)
- ✅ **Compilación sin errores ni warnings**
- ✅ **Funcionalidad intacta**
- ✅ **Estructura más limpia**

### 🎯 Beneficios

1. **Menos archivos:** Un módulo menos que mantener
2. **Más cohesión:** Estados MESI junto a otras constantes del sistema
3. **Código limpio:** Eliminada función no utilizada
4. **Memory.c mejorado:** Código más organizado y legible

---

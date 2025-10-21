# Arquitectura del Simulador MESI

## 📁 Estructura del Proyecto

```
MESI-MultiProcessor-Model/
├── src/
│   ├── bus/                    # Sistema de bus compartido
│   │   ├── bus.c/h             # Gestión del bus (Round-Robin)
│   │   └── handlers.c/h        # Handlers MESI (BUS_RD, BUS_RDX, etc.)
│   │
│   ├── cache/                  # Sistema de caché
│   │   └── cache.c/h           # Caché 2-way set-associative con MESI
│   │
│   ├── pe/                     # Processing Elements
│   │   ├── pe.c/h              # Gestión de threads de PE
│   │   ├── isa.c/h             # Ejecución de instrucciones (ISA)
│   │   ├── registers.c/h       # Banco de registros
│   │   └── loader.c/h          # Cargador de programas assembly
│   │
│   ├── memory/                 # Memoria principal
│   │   └── memory.c/h          # Memoria compartida con thread dedicado
│   │
│   ├── stats/                  # Sistema de estadísticas
│   │   ├── cache_stats.c/h     # Estadísticas de caché
│   │   ├── bus_stats.c/h       # Estadísticas del bus
│   │   └── memory_stats.c/h    # Estadísticas de memoria
│   │
│   ├── dotprod/                # Aplicación de producto punto
│   │   └── dotprod.c/h         # Inicialización y verificación
│   │
│   ├── include/                # Configuración global
│   │   └── config.h            # Constantes, macros y MESI_State
│   │
│   └── main.c                  # Punto de entrada
│
├── asm/                        # Programas assembly
│   ├── dotprod_pe0.asm         # PE0: elementos [0-3]
│   ├── dotprod_pe1.asm         # PE1: elementos [4-7]
│   ├── dotprod_pe2.asm         # PE2: elementos [8-11]
│   └── dotprod_pe3.asm         # PE3: elementos [12-15] + reducción
│
├── makefile                    # Compilación automatizada
├── README.md                   # Documentación principal
├── CHANGELOG.md                # Historial de cambios
└── ARCHITECTURE.md             # Este archivo
```

## 🔧 Módulos Principales

### 1. **config.h** - Configuración centralizada
- Constantes del sistema (NUM_PES, SETS, WAYS, BLOCK_SIZE)
- **Enum MESI_State:** Estados del protocolo (M, E, S, I)
- Macros de alineamiento y direccionamiento

### 2. **bus/** - Sistema de interconexión
- **bus.c:** Arbitraje Round-Robin, callbacks, sincronización
- **handlers.c:** Implementación de handlers MESI
  * `handle_busrd()` - Lectura compartida
  * `handle_busrdx()` - Lectura exclusiva para escritura
  * `handle_busupgr()` - Upgrade S→M
  * `handle_buswb()` - Writeback a memoria

### 3. **cache/** - Sistema de caché
- **cache.c:** Operaciones read/write, LRU, callbacks
- Arquitectura: 2-way set-associative, 16 sets
- Políticas: Write-allocate, Write-back
- Estados MESI: Modified, Exclusive, Shared, Invalid

### 4. **pe/** - Processing Elements
- **pe.c:** Gestión de threads, ejecución de programas
- **isa.c:** Implementación del ISA (LOAD, STORE, FADD, etc.)
- **registers.c:** Banco de 8 registros de 64 bits
- **loader.c:** Cargador de archivos .asm
- **Scheduling:** sched_yield() después de cada instrucción

### 5. **memory/** - Memoria principal
- **memory.c:** Thread dedicado para procesar solicitudes
- Operaciones de bloque completo (BLOCK_SIZE doubles)
- Thread-safe con mutex y condition variables

### 6. **stats/** - Sistema de estadísticas
- **cache_stats:** Hits/misses, transiciones MESI, invalidaciones
- **bus_stats:** Tráfico, operaciones BusRd/RdX/Upgr/WB
- **memory_stats:** Accesos por PE, bytes transferidos

## 🔄 Flujo de Datos

### Lectura (Cache READ)
```
PE → cache_read() 
  → HIT? → Devolver dato
  → MISS? → cache_select_victim()
          → bus_broadcast(BUS_RD)
          → handler trae bloque
          → Devolver dato
```

### Escritura (Cache WRITE)
```
PE → cache_write()
  → HIT M? → Escribir directamente
  → HIT E? → Escribir, E→M
  → HIT S? → bus_broadcast(BUS_UPGR) → S→M
  → MISS? → bus_broadcast_with_callback(BUS_RDX)
          → handler trae bloque
          → callback escribe valor (atómico)
          → M
```

### Protocolo MESI
```
        ┌─────────┐
        │ INVALID │
        └─────────┘
            ↕
    ┌───────────────────┐
    ↓                   ↓
┌─────────┐       ┌──────────┐
│EXCLUSIVE│ ───→  │ MODIFIED │
└─────────┘       └──────────┘
    ↓                   ↓
┌─────────┐             │
│ SHARED  │ ───────────→┘
└─────────┘  (BUS_UPGR)
```

## 📊 Características Clave

### ✅ Coherencia MESI
- Estados bien definidos con transiciones correctas
- Invalidaciones broadcast
- Write-back de líneas modificadas

### ✅ Sincronización Thread-Safe
- Mutex por caché, bus y memoria
- Condition variables para señalización
- Sin deadlocks (unlock antes de bus calls)

### ✅ Callbacks para Atomicidad
- Escrituras atómicas en BUS_RDX
- Ejecutadas en contexto del bus thread
- Eliminan race conditions

### ✅ Scheduling Justo
- sched_yield() después de cada instrucción
- Simulación realista de time-slicing
- 100% tasa de éxito

### ✅ Estadísticas Completas
- Métricas por PE y globales
- Hit rates, miss rates
- Transiciones MESI
- Tráfico del bus

## 🎯 Diseño Limpio

- **Sin módulos redundantes:** mesi.c/h eliminado
- **Organización modular:** Código dividido en secciones claras
- **Comentarios concisos:** Solo información esencial
- **Nombres descriptivos:** Funciones y variables auto-documentadas
- **Separación de concerns:** Cada módulo tiene una responsabilidad

---

**Última actualización:** 20 de Octubre, 2025

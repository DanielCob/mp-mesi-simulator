# Sistema de Estadísticas Completo - MESI MultiProcessor

## 📊 Resumen

Este documento describe el sistema completo de estadísticas implementado en el simulador MESI MultiProcessor, que incluye:

1. **Estadísticas de Caché** (por PE)
2. **Estadísticas de Memoria Principal**
3. **Estadísticas del Bus**

---

## 🎯 Objetivos Implementados

### ✅ Estadísticas de Caché (por PE)

Se implementó un sistema completo de estadísticas para cada caché que registra:

- **Cache Hits y Misses**: Contador de aciertos y fallos en lecturas y escrituras
- **Invalidaciones**: Tanto recibidas como enviadas por cada PE
- **Operaciones de Lectura/Escritura**: Todas las operaciones realizadas
- **Tráfico por PE**: Transacciones de bus (BusRd, BusRdX, BusUpgr, Writeback)
- **Transiciones MESI**: Todas las transiciones de estado del protocolo (10 tipos)

### ✅ Estadísticas de Memoria Principal

Se registran **todos los accesos a memoria principal**:

- **Accesos por PE**: Lecturas y escrituras atribuidas al PE que las solicita
- **Bytes transferidos**: Total de datos leídos/escritos en KB y MB
- **Tráfico total**: Suma de todos los accesos a memoria
- **Desglose por operación**: Lecturas vs escrituras

### ✅ Estadísticas del Bus

Se cuenta **todo el tráfico del bus** incluyendo:

- **Transacciones por tipo**: BUS_RD, BUS_RDX, BUS_UPGR, BUS_WB
- **Invalidaciones**: Contador específico para invalidaciones broadcast
- **Bytes transferidos**: Total de datos movidos por el bus
- **Uso por PE**: Distribución de transacciones entre PEs
- **Métricas de eficiencia**: Bytes/transacción, ratios read/write

---

## 🏗️ Arquitectura de Implementación

### Estructuras de Datos

#### 1. CacheStats (src/include/stats.h)

```c
typedef struct {
    // Operaciones básicas
    int read_hits;
    int read_misses;
    int write_hits;
    int write_misses;
    int total_reads;
    int total_writes;
    
    // Invalidaciones
    int invalidations_received;
    int invalidations_sent;
    
    // Tráfico del bus
    int bus_rd_count;
    int bus_rdx_count;
    int bus_upgr_count;
    int wb_count;
    double traffic_kb;
    
    // Transiciones MESI
    MESITransitions transitions;
} CacheStats;
```

#### 2. MemoryStats (src/include/memory_stats.h)

```c
typedef struct {
    // Accesos globales
    int total_reads;
    int total_writes;
    long bytes_read;
    long bytes_written;
    
    // Accesos por PE
    int reads_per_pe[NUM_PES];
    int writes_per_pe[NUM_PES];
    
    pthread_mutex_t lock;
} MemoryStats;
```

#### 3. BusStats (src/include/bus_stats.h)

```c
typedef struct {
    // Transacciones por tipo
    int bus_rd_count;
    int bus_rdx_count;
    int bus_upgr_count;
    int bus_wb_count;
    int total_transactions;
    
    // Coherencia
    int invalidations_sent;
    
    // Tráfico
    long bytes_transferred;
    
    // Por PE
    int transactions_per_pe[NUM_PES];
    
    pthread_mutex_t lock;
} BusStats;
```

---

## 🔧 Puntos de Integración

### Cache (src/cache/cache.c)

Las estadísticas se registran en:

- **cache_read()**: Registra hits/misses de lectura
- **cache_write()**: Registra hits/misses de escritura
- **cache_set_state()**: Registra transiciones MESI e invalidaciones
- **cache_select_victim()**: Registra writebacks cuando evict un bloque Modified

### Memoria (src/memory/memory.c)

Las estadísticas se registran en:

- **mem_thread_func()**: Al procesar READ_BLOCK y WRITE_BLOCK
- **mem_read_block()**: Registra lectura con PE ID
- **mem_write_block()**: Registra escritura con PE ID

### Bus (src/bus/bus.c)

Las estadísticas se registran en:

- **bus_thread_func()**: Registra todas las transacciones antes de llamar handlers
- **Switch statement**: Diferencia entre BUS_RD, BUS_RDX, BUS_UPGR, BUS_WB

### Handlers (src/bus/handlers.c)

Las estadísticas se registran en:

- **handle_busrdx()**: Cuenta invalidaciones cuando se invalidan caches en E o S
- **handle_busupgr()**: Registra invalidaciones del protocolo
- Todos los handlers pasan `pe_id` a las funciones de memoria

---

## 📈 Formato de Salida

### Estadísticas de Caché

```
╔════════════════════════════════════════════════════════════════╗
║            📊 ESTADÍSTICAS DE CACHÉ - PE 0                     ║
╠════════════════════════════════════════════════════════════════╣
║  💾 OPERACIONES DE CACHÉ                                      ║
╟────────────────────────────────────────────────────────────────╢
║    - Lecturas:               5 (hits: 3, misses: 2)          ║
║    - Escrituras:             3 (hits: 2, misses: 1)          ║
║    - Hit rate:            60.00%                             ║
...
```

### Estadísticas de Memoria

```
╔════════════════════════════════════════════════════════════════╗
║                 ESTADÍSTICAS DE MEMORIA PRINCIPAL              ║
╠════════════════════════════════════════════════════════════════╣
║  📝 ACCESOS A MEMORIA                                         ║
╟────────────────────────────────────────────────────────────────╢
║    - Lecturas:              8                                ║
║    - Escrituras:            0                                ║
║    - Total:                 8                                ║
...
```

### Estadísticas del Bus

```
╔════════════════════════════════════════════════════════════════╗
║                    ESTADÍSTICAS DEL BUS                        ║
╠════════════════════════════════════════────════════════════════╣
║  🚌 TRANSACCIONES DEL BUS                                     ║
╟────────────────────────────────────────────────────────────────╢
║    - BUS_RD (lecturas):                    0              ║
║    - BUS_RDX (escrituras excl.):           8              ║
║    - BUS_UPGR (upgrades):                  0              ║
║    - BUS_WB (writebacks):                  0              ║
...
```

---

## 🧪 Validación

### Tests Exitosos

Todos los tests pasan correctamente:

```
[TEST 1] Compilación... ✓ PASS
[TEST 2] Ejecución sin crashes... ✓ PASS
[TEST 3] Creación de threads... ✓ PASS
...
Total: 13/13 pruebas pasadas (100%)
```

### Verificación de Datos

- **Coherencia PE-Memoria**: Los accesos registrados en memoria coinciden con los cache misses de cada PE
- **Coherencia Bus-Cache**: Las transacciones del bus suman correctamente con las operaciones de cache
- **Thread-Safety**: Todos los contadores usan mutex para evitar race conditions

---

## 📝 Modificaciones Realizadas

### Archivos Nuevos

1. **src/include/memory_stats.h** - Definición de MemoryStats
2. **src/memory/memory_stats.c** - Implementación de estadísticas de memoria
3. **src/include/bus_stats.h** - Definición de BusStats
4. **src/bus/bus_stats.c** - Implementación de estadísticas del bus

### Archivos Modificados

1. **src/cache/cache.h** - Agregado `CacheStats stats` y `int pe_id`
2. **src/cache/cache.c** - Integrado registro de estadísticas
3. **src/memory/memory.h** - Agregado `MemoryStats stats` y `int pe_id` a MemRequest
4. **src/memory/memory.c** - Registro de accesos con PE ID
5. **src/bus/bus.h** - Agregado `BusStats stats`
6. **src/bus/bus.c** - Registro de transacciones
7. **src/bus/handlers.c** - Actualizado para pasar `pe_id` y contar invalidaciones
8. **src/main.c** - Agregado display de estadísticas de memoria y bus
9. **makefile** - Agregado `src/include` y `src/stats` a INCLUDES

### Firmas Modificadas

```c
// Antes:
void mem_read_block(Memory* mem, int addr, double block[]);
void mem_write_block(Memory* mem, int addr, const double block[]);

// Después:
void mem_read_block(Memory* mem, int addr, double block[], int pe_id);
void mem_write_block(Memory* mem, int addr, const double block[], int pe_id);
```

---

## 🎯 Métricas Clave

### Por Caché
- **Hit Rate**: Porcentaje de accesos que encuentran dato en caché
- **Miss Rate**: Porcentaje de accesos que requieren bus/memoria
- **Invalidation Rate**: Frecuencia de invalidaciones por coherencia

### Por Memoria
- **Accesos Totales**: Suma de lecturas + escrituras
- **Tráfico (MB)**: Volumen de datos transferidos
- **Distribución por PE**: Qué PE genera más accesos

### Por Bus
- **Transacciones/segundo**: Actividad del bus
- **Bytes/transacción**: Eficiencia del uso del bus
- **Read/Write Ratio**: Balance entre lecturas y escrituras

---

## 🚀 Uso

### Compilar

```bash
make clean && make
```

### Ejecutar

```bash
./mp_mesi
```

Las estadísticas se muestran automáticamente al final de la ejecución, después de que todos los PEs terminan.

---

## 📊 Ejemplo de Salida Completa

```
================================================================================
                         ESTADÍSTICAS DEL SIMULADOR                             
================================================================================

[Estadísticas de Caché por PE - 4 PEs]
[Resumen Comparativo de Caches]
[Estadísticas de Memoria Principal]
[Estadísticas del Bus]
```

---

## ✨ Características Destacadas

1. **Thread-Safe**: Todos los contadores protegidos con mutex
2. **Formato Profesional**: Uso de Unicode box-drawing characters y emojis
3. **Granularidad Fina**: Estadísticas por PE y agregadas
4. **Métricas Útiles**: Ratios, porcentajes, conversiones de unidades
5. **Cero Overhead**: Estadísticas no afectan la lógica del protocolo MESI
6. **Completo**: Cubre cache, memoria y bus exhaustivamente

---

## 📚 Referencias

- **STATISTICS.md**: Documentación del sistema de estadísticas de caché
- **THREAD_SAFETY_DIAGRAM.md**: Diagramas de sincronización
- **ZERO_FLAG.md**: Sistema de flag zero para JNZ
- **README.md**: Descripción general del proyecto

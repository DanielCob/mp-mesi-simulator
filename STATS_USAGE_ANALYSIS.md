# Análisis de Uso de Estadísticas

## Resumen Ejecutivo

Análisis de campos y funciones en los módulos de estadísticas (`src/stats/`) para identificar código no utilizado.

---

## 1. CacheStats (cache_stats.h/c)

### Estructura `CacheStats`

| Campo | ¿Se Usa? | Dónde se registra | Dónde se muestra |
|-------|----------|-------------------|------------------|
| `read_hits` | ✅ SÍ | `stats_record_read_hit()` | `stats_print()`, `stats_print_summary()` |
| `read_misses` | ✅ SÍ | `stats_record_read_miss()` | `stats_print()`, `stats_print_summary()` |
| `write_hits` | ✅ SÍ | `stats_record_write_hit()` | `stats_print()`, `stats_print_summary()` |
| `write_misses` | ✅ SÍ | `stats_record_write_miss()` | `stats_print()`, `stats_print_summary()` |
| `invalidations_sent` | ✅ SÍ | `stats_record_invalidation_sent()` | `stats_print()` |
| `invalidations_received` | ✅ SÍ | `stats_record_invalidation_received()` | `stats_print()` |
| `total_reads` | ✅ SÍ | `stats_record_read_hit/miss()` | `stats_print()`, `stats_print_summary()` |
| `total_writes` | ✅ SÍ | `stats_record_write_hit/miss()` | `stats_print()`, `stats_print_summary()` |
| `bus_reads` | ✅ SÍ | `stats_record_read_miss()` | `stats_print()`, `stats_print_summary()` |
| `bus_read_x` | ✅ SÍ | `stats_record_write_miss()` | `stats_print()`, `stats_print_summary()` |
| `bus_upgrades` | ✅ SÍ | `cache.c` (directo) | `stats_print()`, `stats_print_summary()` |
| `bus_writebacks` | ✅ SÍ | `cache.c` (directo) | `stats_print()`, `stats_print_summary()` |
| `transitions.*` | ✅ SÍ | `stats_record_mesi_transition()` | `stats_print()` |
| `bytes_read_from_bus` | ✅ SÍ | `stats_record_bus_traffic()` | `stats_print()`, `stats_print_summary()` |
| `bytes_written_to_bus` | ✅ SÍ | `stats_record_bus_traffic()` | `stats_print()`, `stats_print_summary()` |

**Conclusión**: ✅ **TODOS los campos se usan**

### Funciones

| Función | ¿Se Usa? | Dónde se llama |
|---------|----------|----------------|
| `stats_init()` | ✅ SÍ | `cache_init()` |
| `stats_record_read_hit()` | ✅ SÍ | `cache_read()` |
| `stats_record_read_miss()` | ✅ SÍ | `cache_read()` |
| `stats_record_write_hit()` | ✅ SÍ | `cache_write()` |
| `stats_record_write_miss()` | ✅ SÍ | `cache_write()` |
| `stats_record_invalidation_received()` | ✅ SÍ | `cache_set_state()` |
| `stats_record_invalidation_sent()` | ✅ SÍ | `cache_write()` |
| `stats_record_bus_traffic()` | ✅ SÍ | `cache_read()`, `cache_write()`, `cache_select_victim()` |
| `stats_record_mesi_transition()` | ✅ SÍ | `cache_set_state()` |
| `stats_print()` | ✅ SÍ | `main.c` |
| `stats_print_summary()` | ✅ SÍ | `main.c` |

**Conclusión**: ✅ **TODAS las funciones se usan**

---

## 2. BusStats (bus_stats.h/c)

### Estructura `BusStats`

| Campo | ¿Se Usa? | Dónde se registra | Dónde se muestra |
|-------|----------|-------------------|------------------|
| `bus_rd_count` | ✅ SÍ | `bus_stats_record_bus_rd()` | `bus_stats_print()` |
| `bus_rdx_count` | ✅ SÍ | `bus_stats_record_bus_rdx()` | `bus_stats_print()` |
| `bus_upgr_count` | ✅ SÍ | `bus_stats_record_bus_upgr()` | `bus_stats_print()` |
| `bus_wb_count` | ✅ SÍ | `bus_stats_record_bus_wb()` | `bus_stats_print()` |
| `invalidations_sent` | ✅ SÍ | `bus_stats_record_invalidations()` | `bus_stats_print()` |
| `total_transactions` | ✅ SÍ | Todas las `bus_stats_record_*()` | `bus_stats_print()` |
| `bytes_transferred` | ✅ SÍ | `bus_stats_record_*_transfer()` | `bus_stats_print()` |
| `bytes_data` | ✅ SÍ | `bus_stats_record_data_transfer()` | `bus_stats_print()` |
| `bytes_control` | ✅ SÍ | `bus_stats_record_control_transfer()` | `bus_stats_print()` |
| `busy_cycles` | ❌ **NO** | Nunca | Nunca |
| `idle_cycles` | ❌ **NO** | Nunca | Nunca |
| `transactions_per_pe[]` | ✅ SÍ | Todas las `bus_stats_record_*()` | `bus_stats_print()` |

**Conclusión**: ⚠️ **2 campos NO se usan**: `busy_cycles`, `idle_cycles`

### Funciones

| Función | ¿Se Usa? | Dónde se llama |
|---------|----------|----------------|
| `bus_stats_init()` | ✅ SÍ | `bus_init()` |
| `bus_stats_record_bus_rd()` | ✅ SÍ | `bus.c` |
| `bus_stats_record_bus_rdx()` | ✅ SÍ | `bus.c` |
| `bus_stats_record_bus_upgr()` | ✅ SÍ | `bus.c` |
| `bus_stats_record_bus_wb()` | ✅ SÍ | `bus.c` |
| `bus_stats_record_invalidations()` | ✅ SÍ | `handlers.c` |
| `bus_stats_record_transfer()` | ❌ **NO** | Nunca (reemplazada por data/control) |
| `bus_stats_record_data_transfer()` | ✅ SÍ | `bus.c` |
| `bus_stats_record_control_transfer()` | ✅ SÍ | `bus.c` |
| `bus_stats_print()` | ✅ SÍ | `main.c` |

**Conclusión**: ⚠️ **1 función NO se usa**: `bus_stats_record_transfer()`

---

## 3. MemoryStats (memory_stats.h/c)

### Estructura `MemoryStats`

| Campo | ¿Se Usa? | Dónde se registra | Dónde se muestra |
|-------|----------|-------------------|------------------|
| `reads` | ✅ SÍ | `memory_stats_record_read()` | `memory_stats_print()` |
| `writes` | ✅ SÍ | `memory_stats_record_write()` | `memory_stats_print()` |
| `total_accesses` | ✅ SÍ | `memory_stats_record_*()` | `memory_stats_print()` |
| `bytes_read` | ✅ SÍ | `memory_stats_record_read()` | `memory_stats_print()` |
| `bytes_written` | ✅ SÍ | `memory_stats_record_write()` | `memory_stats_print()` |
| `reads_per_pe[]` | ✅ SÍ | `memory_stats_record_read()` | `memory_stats_print()` |
| `writes_per_pe[]` | ✅ SÍ | `memory_stats_record_write()` | `memory_stats_print()` |

**Conclusión**: ✅ **TODOS los campos se usan**

### Funciones

| Función | ¿Se Usa? | Dónde se llama |
|---------|----------|----------------|
| `memory_stats_init()` | ✅ SÍ | `mem_init()` |
| `memory_stats_record_read()` | ✅ SÍ | `mem_thread_func()` |
| `memory_stats_record_write()` | ✅ SÍ | `mem_thread_func()` |
| `memory_stats_print()` | ✅ SÍ | `main.c` |

**Conclusión**: ✅ **TODAS las funciones se usan**

---

## Resumen General

### Elementos NO Utilizados

#### 1. **BusStats - Campos**
- `busy_cycles` - Campo definido pero nunca usado
- `idle_cycles` - Campo definido pero nunca usado

**Razón**: Estos campos requerirían un modelo de temporización/ciclos que el simulador actual no implementa.

#### 2. **BusStats - Funciones**
- `bus_stats_record_transfer()` - Función legacy, reemplazada por `data_transfer` y `control_transfer`

**Razón**: Se refactorizó para separar tráfico de datos vs control, pero se dejó la función antigua.

---

## Recomendaciones

### Opción 1: Eliminar Código No Usado (Recomendado)

**Ventajas**:
- Código más limpio y mantenible
- Menos confusión para futuros desarrolladores
- Reduce superficie de código

**Desventajas**:
- Si en el futuro se quiere agregar conteo de ciclos, hay que volver a implementar

### Opción 2: Mantener con Comentarios

**Ventajas**:
- Infraestructura lista para futuras expansiones
- No requiere cambios ahora

**Desventajas**:
- Código "muerto" confunde
- Aumenta complejidad sin beneficio

### Opción 3: Implementar Conteo de Ciclos

**Ventajas**:
- Estadística adicional útil
- Aprovecha campos existentes

**Desventajas**:
- Requiere modelo de temporización
- Más complejo de implementar correctamente

---

## Implementación Recomendada

### Paso 1: Eliminar campos no usados de `BusStats`

```c
// ELIMINAR estas líneas de bus_stats.h:
// uint64_t busy_cycles;
// uint64_t idle_cycles;
```

### Paso 2: Eliminar función legacy

```c
// ELIMINAR de bus_stats.h:
// void bus_stats_record_transfer(BusStats* stats, int bytes);

// ELIMINAR de bus_stats.c:
// void bus_stats_record_transfer(BusStats* stats, int bytes) { ... }
```

### Paso 3: Verificar compilación

```bash
make clean && make
./mp_mesi
```

---

## Verificación de Uso

Para verificar qué se usa:

```bash
# Buscar referencias a busy_cycles
grep -r "busy_cycles" src/

# Buscar referencias a idle_cycles
grep -r "idle_cycles" src/

# Buscar llamadas a bus_stats_record_transfer
grep -r "bus_stats_record_transfer(" src/
```

**Resultado actual**: Solo aparecen en definiciones, nunca en uso real.

---

## Conclusión

El módulo de estadísticas está **bien diseñado** y la mayoría del código se usa. Solo hay **3 elementos innecesarios**:

1. ❌ `BusStats.busy_cycles` - Campo no usado
2. ❌ `BusStats.idle_cycles` - Campo no usado  
3. ❌ `bus_stats_record_transfer()` - Función legacy no usada

**Recomendación**: Eliminar estos 3 elementos para mantener el código limpio.

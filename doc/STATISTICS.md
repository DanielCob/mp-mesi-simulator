# Sistema de Estadísticas del Simulador MESI

## Resumen

El simulador incluye un completo sistema de estadísticas que registra métricas de rendimiento, coherencia, tráfico del bus y transiciones de estados MESI para cada Processing Element (PE).

## Métricas Registradas

### 1. Rendimiento de Caché

#### Hits y Misses
- **Read Hits**: Lecturas satisfechas por la caché local
- **Read Misses**: Lecturas que requirieron acceso al bus
- **Write Hits**: Escrituras en líneas ya presentes
- **Write Misses**: Escrituras que requirieron traer el bloque

#### Tasas Calculadas
- **Hit Rate**: Porcentaje de accesos satisfechos por la caché
- **Miss Rate**: Porcentaje de accesos que requirieron el bus

### 2. Coherencia de Caché

#### Invalidaciones
- **Invalidaciones Recibidas**: Número de veces que esta caché fue invalidada por otros PEs
- **Invalidaciones Enviadas**: Número de veces que este PE causó invalidaciones en otras cachés

Estas métricas ayudan a entender:
- Qué tan frecuentemente se comparten datos entre PEs
- El overhead del protocolo MESI
- Patrones de acceso conflictivos

### 3. Tráfico del Bus

#### Operaciones
- **BusRd**: Solicitudes de lectura (read miss, estado I→E o I→S)
- **BusRdX**: Solicitudes de escritura exclusiva (write miss, I→M)
- **BusUpgr**: Actualizaciones de S→M (write hit en estado Shared)
- **Writebacks**: Escrituras de líneas Modified a memoria

#### Bytes Transferidos
- **Bytes Leídos**: Datos recibidos del bus (desde memoria u otra caché)
- **Bytes Escritos**: Datos enviados al bus (writebacks)
- **Tráfico Total**: Suma de todos los bytes transferidos

### 4. Transiciones de Estados MESI

Se registran todas las transiciones entre estados:

#### Desde Invalid (I)
- **I → E**: Read miss sin copias compartidas
- **I → S**: Read miss con copias en otras cachés
- **I → M**: Write miss

#### Desde Exclusive (E)
- **E → M**: Write hit en estado exclusivo
- **E → S**: Otra caché lee el bloque
- **E → I**: Invalidación

#### Desde Shared (S)
- **S → M**: Write hit + upgrade (invalida otras copias)
- **S → I**: Invalidación por escritura de otro PE

#### Desde Modified (M)
- **M → S**: Otra caché lee (requiere writeback)
- **M → I**: Invalidación (requiere writeback)

## Estructura de Datos

### CacheStats

```c
typedef struct {
    // Cache hits y misses
    uint64_t read_hits;
    uint64_t read_misses;
    uint64_t write_hits;
    uint64_t write_misses;
    
    // Invalidaciones
    uint64_t invalidations_sent;
    uint64_t invalidations_received;
    
    // Operaciones totales
    uint64_t total_reads;
    uint64_t total_writes;
    
    // Tráfico del bus (en bloques)
    uint64_t bus_reads;
    uint64_t bus_read_x;
    uint64_t bus_upgrades;
    uint64_t bus_writebacks;
    
    // Transiciones MESI
    MESITransitions transitions;
    
    // Bytes transferidos
    uint64_t bytes_read_from_bus;
    uint64_t bytes_written_to_bus;
} CacheStats;
```

### MESITransitions

```c
typedef struct {
    // Desde Invalid
    uint64_t I_to_E, I_to_S, I_to_M;
    
    // Desde Exclusive
    uint64_t E_to_M, E_to_S, E_to_I;
    
    // Desde Shared
    uint64_t S_to_M, S_to_I;
    
    // Desde Modified
    uint64_t M_to_S, M_to_I;
} MESITransitions;
```

## Ejemplo de Salida

### Estadísticas Individuales de PE

```
╔════════════════════════════════════════════════════════════════╗
║            ESTADÍSTICAS DE PE0                                ║
╠════════════════════════════════════════════════════════════════╣
║  📊 RENDIMIENTO DE CACHÉ                                      ║
╟────────────────────────────────────────────────────────────────╢
║  Lecturas:                                                     ║
║    - Hits:            2    Misses:          0              ║
║    - Total:           2                                     ║
║                                                                ║
║  Escrituras:                                                   ║
║    - Hits:            1    Misses:          2              ║
║    - Total:           3                                     ║
║                                                                ║
║  Total:                                                        ║
║    - Hits:            3    (60.00%)                         ║
║    - Misses:          2    (40.00%)                         ║
║    - Accesos:          5                                    ║
╟────────────────────────────────────────────────────────────────╢
║  🔄 COHERENCIA (Invalidaciones)                               ║
╟────────────────────────────────────────────────────────────────╢
║    - Recibidas:          0                                  ║
║    - Enviadas:           0                                  ║
╟────────────────────────────────────────────────────────────────╢
║  🚌 TRÁFICO DEL BUS                                           ║
╟────────────────────────────────────────────────────────────────╢
║    - BusRd (lecturas):               0                      ║
║    - BusRdX (escrituras):            2                      ║
║    - BusUpgr (upgrades):             0                      ║
║    - Writebacks:                     0                      ║
║                                                                ║
║    - Bytes leídos:           64 (0.06 KB)                ║
║    - Bytes escritos:          0 (0.00 KB)                ║
║    - Tráfico total:  0.000061 MB                           ║
╚════════════════════════════════════════════════════════════════╝
```

### Resumen Comparativo

```
╔════════════════════════════════════════════════════════════════════════════════╗
║                    RESUMEN COMPARATIVO DE ESTADÍSTICAS                        ║
╠════════════════════════════════════════════════════════════════════════════════╣
║  HIT RATES POR PE                                                              ║
╟────────────────────────────────────────────────────────────────────────────────╢
║  PE  │  Accesos  │   Hits    │  Misses   │  Hit Rate  │  Miss Rate            ║
╟──────┼───────────┼───────────┼───────────┼────────────┼────────────────────────╢
║  0   │         5 │         3 │         2 │   60.00%  │   40.00%              ║
║  1   │         5 │         3 │         2 │   60.00%  │   40.00%              ║
║  2   │         3 │         1 │         2 │   33.33%  │   66.67%              ║
║  3   │         4 │         2 │         2 │   50.00%  │   50.00%              ║
╟──────┼───────────┼───────────┼───────────┼────────────┼────────────────────────╢
║ TOTAL│        17 │         9 │         8 │   52.94%  │   47.06%              ║
╚════════════════════════════════════════════════════════════════════════════════╝
```

## Interpretación de Resultados

### Hit Rate

- **> 90%**: Excelente localidad espacial y temporal
- **70-90%**: Buena localidad, caché efectiva
- **50-70%**: Localidad moderada
- **< 50%**: Pobre localidad, mucho tráfico en el bus

### Invalidaciones

- **Altas invalidaciones**: Indica false sharing o true sharing intenso
- **Bajas invalidaciones**: Datos mayormente privados o read-only

### Tráfico del Bus

- **Alto BusRd**: Muchos cache misses en lecturas
- **Alto BusRdX**: Muchos write misses
- **Alto BusUpgr**: Muchas escrituras a datos compartidos
- **Altos Writebacks**: Muchas líneas sucias evictadas

### Transiciones MESI

#### Patrones Comunes

**Datos Privados:**
```
I → M (write miss)
E → M (write hit)
```

**Datos Compartidos (read-only):**
```
I → S (read miss compartido)
```

**Datos Compartidos (read-write):**
```
I → S (read miss)
S → M (write, con upgrade)
M → S (otro PE lee, writeback)
```

**Producer-Consumer:**
```
PE0: I → M (write)
PE0: M → S (PE1 lee)
PE1: I → S (read)
PE1: S → M (write)
```

## Uso en Análisis de Rendimiento

### 1. Identificar Bottlenecks

```
Si miss_rate > 50%:
  → Revisar patrón de acceso
  → Considerar aumentar tamaño de caché
  → Mejorar localidad de datos
```

### 2. Detectar False Sharing

```
Si invalidations_received es alto pero hit_rate es bajo:
  → Posible false sharing
  → Alinear datos a límites de línea de caché
```

### 3. Optimizar Distribución de Trabajo

```
Comparar tráfico entre PEs:
  → PEs con mucho más tráfico están sobrecargados
  → Balancear carga de trabajo
```

### 4. Evaluar Eficiencia del Protocolo

```
Ratio writebacks/write_misses:
  → Alto: Muchos datos modificados evictados
  → Bajo: Datos se mantienen en caché hasta el final
```

## Funciones de la API

### Inicialización
```c
void stats_init(CacheStats* stats);
```

### Registro de Eventos
```c
void stats_record_read_hit(CacheStats* stats);
void stats_record_read_miss(CacheStats* stats);
void stats_record_write_hit(CacheStats* stats);
void stats_record_write_miss(CacheStats* stats);
void stats_record_invalidation_received(CacheStats* stats);
void stats_record_invalidation_sent(CacheStats* stats);
void stats_record_bus_traffic(CacheStats* stats, uint64_t bytes_read, uint64_t bytes_written);
void stats_record_mesi_transition(CacheStats* stats, int from, int to);
```

### Visualización
```c
void stats_print(const CacheStats* stats, int pe_id);
void stats_print_summary(const CacheStats* stats_array, int num_pes);
```

## Integración con Caché

Las estadísticas se actualizan automáticamente en:

1. **cache_read()**: Registra hits/misses de lectura
2. **cache_write()**: Registra hits/misses de escritura
3. **cache_set_state()**: Registra transiciones MESI e invalidaciones
4. **cache_select_victim()**: Registra writebacks
5. **Handlers del bus**: Actualizan tráfico y transiciones

## Conclusión

El sistema de estadísticas proporciona visibilidad completa del comportamiento del protocolo MESI, permitiendo:

- Análisis de rendimiento detallado
- Detección de problemas de coherencia
- Optimización de patrones de acceso
- Comparación entre PEs
- Validación de la implementación MESI

Todas las métricas se calculan automáticamente y se muestran en formato fácil de leer al finalizar la simulación.

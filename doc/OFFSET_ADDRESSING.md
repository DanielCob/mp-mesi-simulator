# Sistema de Direccionamiento con Offset

## Visión General

El simulador MESI implementa un **sistema completo de direccionamiento con offset** que refleja el comportamiento real de las cachés con bloques de múltiples palabras. Cada acceso a memoria se descompone en:

- **Dirección BASE**: Inicio del bloque (debe estar alineada a `BLOCK_SIZE`)
- **OFFSET**: Posición dentro del bloque (0 a `BLOCK_SIZE-1`)

## Arquitectura

### Estructura de Bloques

```
Cada bloque contiene BLOCK_SIZE = 4 doubles (32 bytes)

Bloque 0: [addr 0,  addr 1,  addr 2,  addr 3 ]
Bloque 1: [addr 4,  addr 5,  addr 6,  addr 7 ]
Bloque 2: [addr 8,  addr 9,  addr 10, addr 11]
          ...
```

### Descomposición de Direcciones

```
Dirección completa = BASE + OFFSET

Ejemplo: Dirección 13
  BASE   = 12  (bloque que contiene la dirección)
  OFFSET = 1   (posición dentro del bloque)
  Bloque: [12, 13, 14, 15]
```

## Macros Implementadas

### En `src/include/config.h`

```c
// Obtener dirección base del bloque
#define GET_BLOCK_BASE(addr)   (ALIGN_DOWN(addr))

// Obtener offset dentro del bloque
#define GET_BLOCK_OFFSET(addr) ((addr) % BLOCK_SIZE)

// Validar offset
#define IS_VALID_OFFSET(offset) ((offset) >= 0 && (offset) < BLOCK_SIZE)

// Construir dirección desde base + offset
#define MAKE_ADDRESS(base, offset) ((base) + (offset))
```

### Ejemplos de Uso

```c
int addr = 13;
int base = GET_BLOCK_BASE(addr);      // → 12
int offset = GET_BLOCK_OFFSET(addr);  // → 1

// Reconstruir
int addr2 = MAKE_ADDRESS(base, offset);  // → 13
```

## Flujo de Operaciones

### 1. Lectura (cache_read)

```c
double cache_read(Cache* cache, int addr, int pe_id) {
    // 1. Extraer base y offset
    int block_base = GET_BLOCK_BASE(addr);  // addr 13 → base 12
    int offset = GET_BLOCK_OFFSET(addr);     // addr 13 → offset 1
    
    // 2. Buscar el BLOQUE en cache (por base)
    if (HIT) {
        return victim->data[offset];  // ← Lee posición específica
    }
    
    // 3. En MISS, traer BLOQUE completo del bus
    bus_broadcast(cache->bus, BUS_RD, block_base, pe_id);
    
    // 4. Retornar dato en el offset
    return victim->data[offset];
}
```

### 2. Escritura (cache_write)

```c
void cache_write(Cache* cache, int addr, double value, int pe_id) {
    // 1. Extraer base y offset
    int block_base = GET_BLOCK_BASE(addr);
    int offset = GET_BLOCK_OFFSET(addr);
    
    // 2. Buscar el BLOQUE en cache
    if (HIT && state == M) {
        set->lines[i].data[offset] = value;  // ← Escribe en posición específica
        return;
    }
    
    // 3. En MISS, traer BLOQUE completo
    bus_broadcast(cache->bus, BUS_RDX, block_base, pe_id);
    
    // 4. Escribir en el offset
    victim->data[offset] = value;
}
```

### 3. Transferencias del Bus

El bus **siempre transfiere bloques completos** (4 doubles):

```c
void handle_busrd(Bus* bus, int addr, int src_pe) {
    // addr es la dirección BASE del bloque
    
    if (otro_cache_tiene_el_bloque) {
        double block[BLOCK_SIZE];
        cache_get_block(cache, addr, block);  // Obtiene [addr, addr+1, addr+2, addr+3]
        cache_set_block(requestor, addr, block);  // Establece bloque completo
    } else {
        double block[BLOCK_SIZE];
        mem_read_block(bus->memory, addr, block);  // Lee 4 doubles consecutivos
        cache_set_block(requestor, addr, block);
    }
}
```

### 4. Operaciones de Memoria

La memoria también opera con bloques:

```c
// Lee 4 doubles consecutivos desde addr
void mem_read_block(Memory* mem, int addr, double block[BLOCK_SIZE]) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        block[i] = mem->data[addr + i];  // Lee [addr, addr+1, addr+2, addr+3]
    }
}

// Escribe 4 doubles consecutivos a partir de addr
void mem_write_block(Memory* mem, int addr, const double block[BLOCK_SIZE]) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        mem->data[addr + i] = block[i];  // Escribe [addr, addr+1, addr+2, addr+3]
    }
}
```

## Beneficios del Sistema

### 1. Localidad Espacial

```assembly
# Vector A[] = {a0, a1, a2, a3, a4, ...} en addr 100
LOAD R0, 100   # MISS - Trae bloque [100, 101, 102, 103]
LOAD R1, 101   # HIT  - Ya está en cache (offset 1)
LOAD R2, 102   # HIT  - Ya está en cache (offset 2)
LOAD R3, 103   # HIT  - Ya está en cache (offset 3)
LOAD R4, 104   # MISS - Trae bloque [104, 105, 106, 107]
```

**Resultado**: 2 misses, 3 hits → **Hit rate = 60%**  
**Sin bloques**: 5 misses, 0 hits → **Hit rate = 0%**

### 2. Reducción de Tráfico del Bus

```
Sin bloques:
  - 5 accesos = 5 transacciones de bus
  - 5 × 8 bytes = 40 bytes transferidos

Con bloques (BLOCK_SIZE=4):
  - 5 accesos = 2 transacciones de bus
  - 2 × 32 bytes = 64 bytes transferidos
  - Pero muchos más hits en accesos futuros
```

### 3. Coherencia Simplificada

El protocolo MESI se aplica al **bloque completo**, no a palabras individuales:

```
Estado del bloque [100-103]: M/E/S/I
  - Todas las palabras tienen el mismo estado
  - Simplifica las transiciones MESI
  - Reduce complejidad del protocolo
```

## Ejemplo Completo

### Código Assembly

```assembly
# Suma de vectores: C[i] = A[i] + B[i]
# A en addr 100, B en addr 200, C en addr 300

LOAD R0, 100    # Carga A[0] → MISS, trae [100-103]
LOAD R1, 200    # Carga B[0] → MISS, trae [200-203]
FADD R2, R0, R1 # R2 = A[0] + B[0]
STORE R2, 300   # Guarda C[0] → MISS, trae [300-303]

LOAD R0, 101    # Carga A[1] → HIT (offset 1 de bloque [100-103])
LOAD R1, 201    # Carga B[1] → HIT (offset 1 de bloque [200-203])
FADD R2, R0, R1 # R2 = A[1] + B[1]
STORE R2, 301   # Guarda C[1] → HIT (offset 1 de bloque [300-303])
```

### Análisis de Accesos

| Instrucción | Dirección | BASE | OFFSET | Resultado |
|-------------|-----------|------|--------|-----------|
| LOAD R0, 100 | 100 | 100 | 0 | MISS - Trae [100-103] |
| LOAD R1, 200 | 200 | 200 | 0 | MISS - Trae [200-203] |
| STORE R2, 300 | 300 | 300 | 0 | MISS - Trae [300-303] |
| LOAD R0, 101 | 101 | 100 | 1 | **HIT** - Ya en cache |
| LOAD R1, 201 | 201 | 200 | 1 | **HIT** - Ya en cache |
| STORE R2, 301 | 301 | 300 | 1 | **HIT** - Ya en cache |

**Totales**: 3 misses, 3 hits → **Hit rate = 50%**

## Diagrama de Memoria

```
Dirección   Offset   Bloque    Contenido
-----------+---------+---------+-------------
    0      |    0    | Bloque 0 | data[0]
    1      |    1    |          | data[1]
    2      |    2    |          | data[2]
    3      |    3    |          | data[3]
-----------+---------+---------+-------------
    4      |    0    | Bloque 1 | data[4]
    5      |    1    |          | data[5]
    6      |    2    |          | data[6]
    7      |    3    |          | data[7]
-----------+---------+---------+-------------
   ...
-----------+---------+---------+-------------
  100      |    0    | Bloque 25 | A[0]
  101      |    1    |           | A[1]
  102      |    2    |           | A[2]
  103      |    3    |           | A[3]
-----------+---------+---------+-------------
```

## Funciones de API

### Cache (`src/cache/cache.c`)

```c
// Lectura/escritura con offset automático
double cache_read(Cache* cache, int addr, int pe_id);
void cache_write(Cache* cache, int addr, double value, int pe_id);

// Operaciones de bloque completo (para handlers)
void cache_get_block(Cache* cache, int addr, double block[BLOCK_SIZE]);
void cache_set_block(Cache* cache, int addr, const double block[BLOCK_SIZE]);
```

### Memoria (`src/memory/memory.c`)

```c
// Operaciones single (deprecated, offset 0 implícito)
double mem_read(Memory* mem, int addr);
void mem_write(Memory* mem, int addr, double value);

// Operaciones de bloque completo (recomendado)
void mem_read_block(Memory* mem, int addr, double block[BLOCK_SIZE]);
void mem_write_block(Memory* mem, int addr, const double block[BLOCK_SIZE]);
```

## Pruebas y Verificación

### Programa de Demostración

```bash
# Compilar y ejecutar demo de offset
gcc -o test_offset_demo test_offset_demo.c -I.
./test_offset_demo
```

### Verificación Completa

```bash
# Todas las pruebas incluyen verificación de offset
bash tests/verify.sh
```

## Salida de Debug

El sistema imprime información de offset cuando es relevante:

```
[PE0] READ addr=13 → block_base=12, offset=1
[PE0] READ HIT en set 12 (way 0, estado E, offset 1) -> valor=42.00
```

```
[Bus] Read miss, leyendo BLOQUE desde memoria addr=12
[MEMORY] READ_BLOCK addr=12 (reading 4 doubles)
[Memoria] Devolviendo bloque [0.00, 42.00, 0.00, 0.00]
```

## Compatibilidad

### Funciones Legacy

Las funciones antiguas (`cache_get_data`, `mem_read`) siguen funcionando pero usan **offset 0 implícito**:

```c
// Antigua (solo lee offset 0)
double val = cache_get_data(cache, 100);  // Lee data[0] del bloque 100

// Nueva (lee cualquier offset)
double val = cache_read(cache, 102, pe_id);  // Lee data[2] del bloque 100
```

## Resumen

| Aspecto | Implementación |
|---------|----------------|
| **Tamaño de bloque** | 4 doubles (32 bytes) |
| **Offset range** | 0-3 |
| **Transferencias** | Siempre bloques completos |
| **Coherencia** | Aplicada al bloque completo |
| **Macros** | `GET_BLOCK_BASE()`, `GET_BLOCK_OFFSET()` |
| **API de bloques** | `cache_*_block()`, `mem_*_block()` |
| **Hit rate típico** | 40-60% con localidad espacial |

## Referencias

- `src/include/config.h` - Macros de direccionamiento
- `src/cache/cache.c` - Implementación de offset en cache
- `src/memory/memory.c` - Operaciones de bloques en memoria
- `src/bus/handlers.c` - Transferencias de bloques en el bus
- `test_offset_demo.c` - Programa de demostración completo

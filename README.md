# Proyecto 01 — Simulador de Sistema Multiprocesador con Coherencia MESI  
**Curso:** CE-4302 Arquitectura de Computadores II  
**Instituto Tecnológico de Costa Rica – Semestre II, 2025**  

**Profesores:**  
- Luis Alonso Barboza Artavia  
- Ronald García Fernández  

**Fechas de entrega:**  
- Grupo 1: 22 de octubre de 2025  
- Grupo 2: 21 de octubre de 2025  

---

## 1. Objetivo general

Diseñar, modelar e implementar un sistema multiprocesador (MP) con coherencia de caché mediante el protocolo MESI, capaz de realizar el cálculo paralelo del producto punto de dos vectores de punto flotante de doble precisión.

---

## 2. Descripción general del sistema

El modelo simula un sistema multiprocesador (MP) de 4 Processing Elements (PEs), donde cada PE ejecuta un hilo de hardware y posee su propia caché privada.  
Los PEs comparten una memoria principal común y se comunican a través de un bus de interconexión que implementa la coherencia MESI.

### Estructura general
- 4 PEs (`PE0`–`PE3`)
- 4 cachés privadas (una por PE)
- 1 memoria principal compartida
- 1 bus de interconexión (arbitraje, broadcast, coherencia)

---

## 3. Especificaciones técnicas

| Componente | Características principales |
|-------------|-----------------------------|
| **Lenguaje permitido** | C / C++ / SystemVerilog (no sintetizable). Python no permitido. |
| **PEs** | 4 PEs, cada uno con 8 registros de 64 bits (`REG0–REG7`). |
| **Caché** | Privada por PE, 2-way set associative, 16 bloques de 32 bytes. <br>Políticas: *write-allocate* y *write-back*. |
| **Memoria principal** | 512 posiciones de 64 bits. |
| **Alineamiento** | **Direcciones deben estar alineadas a múltiplos de BLOCK_SIZE (4)**. <br>Ver [MEMORY_ALIGNMENT.md](MEMORY_ALIGNMENT.md) para detalles. |
| **Interconect (Bus)** | Responsable de arbitraje, comunicación y coherencia (MESI). |
| **Threading** | Cada PE, el bus y la memoria se modelan con *threads* separados (6 total). |

---

## 4. Instrucciones soportadas (ISA)

Cada PE debe soportar las siguientes instrucciones de punto flotante (64 bits):

| Instrucción | Descripción | Ejemplo |
|--------------|-------------|----------|
| `LOAD REG, [dir]` | Carga desde memoria a registro | `LOAD R5, [R0]` |
| `STORE REG, [dir]` | Escribe de registro a memoria | `STORE R4, [R2]` |
| `FMUL Rd, Ra, Rb` | Multiplicación flotante | `FMUL R7, R5, R6` |
| `FADD Rd, Ra, Rb` | Suma flotante | `FADD R4, R4, R7` |
| `INC REG` | Incrementa el registro | `INC R0` |
| `DEC REG` | Decrementa el registro | `DEC R3` |
| `JNZ label` | Salta si el registro ≠ 0 | `JNZ LOOP` |

---

## 5. Problema a resolver: producto punto paralelo

Dado dos vectores `A[]` y `B[]` de tamaño `N` (tipo `double`):

- Cada PE procesa un segmento de tamaño `N/4`.
- Se almacenan los resultados parciales en un arreglo auxiliar.
- Un PE final realiza la suma final de los productos parciales.

### Ejemplo de paralelismo

A = [a0, a1, ..., aN]
B = [b0, b1, ..., bN]

PE0 → segmento A0..A(N/4)
PE1 → segmento A(N/4)..A(N/2)
PE2 → segmento A(N/2)..A(3N/4)
PE3 → segmento A(3N/4)..A(N)


Cada PE calcula su producto parcial, y uno de ellos combina los resultados.

---

## 6. Protocolo MESI implementado

Cada caché implementa coherencia de caché con los siguientes estados:

- **M:** Modified (dato sucio, solo aquí)
- **E:** Exclusive (dato limpio, solo aquí)
- **S:** Shared (dato limpio, compartido)
- **I:** Invalid (no válido)

### Señales del bus
| Señal | Descripción | Acción principal |
|--------|--------------|------------------|
| `BusRd` | Solicitud de lectura | Obtiene bloque de otra caché o memoria |
| `BusRdX` | Lectura exclusiva (para escribir) | Invalida copias en otras cachés |
| `BusUpgr` | Upgrade de S→M | Invalida copias compartidas |
| `BusWB` | Write-back | Escribe línea modificada a memoria |

---

## 7. Requisitos funcionales

1. Modelar cada componente (PE, bus, caché, memoria) como *thread independiente*.  
2. Implementar un cargador de código para las instrucciones de cada PE.  
3. Implementar carga de memoria de datos con control de alineamiento.  
4. Implementar segmentación de memoria (para dividir los vectores).  
5. Implementar el protocolo MESI completo en el bus.  
6. Implementar comunicación *shared-memory* para combinar resultados.  
7. Registrar estadísticas por PE:  
   - Cantidad de *cache misses*  
   - Cantidad de *invalidaciones relevantes*  
   - Cantidad de *operaciones de lectura/escritura*  
   - *Tráfico por PE*  
   - Transiciones de estado MESI  
8. Registrar todos los accesos a memoria principal.  
9. Permitir visualización de:  
   - Estados de líneas de caché  
   - Contenido de memoria  
   - Estadísticas del bus  
   - Ejecución paso a paso o por bloque  
10. Validar que el producto punto final sea correcto.  
11. Mostrar métricas y resultados para análisis.  

---

## 8. Requisitos de entrega

### Archivos requeridos
- Código fuente completo del simulador.  
- Archivo `README.md` (este documento).  
- Documentación técnica y de diseño.  
- Archivos o scripts para compilar (`makefile`).  
- Diagramas de arquitectura y flujo.  
- Reporte técnico en formato PDF.  
- Video de presentación (4:30–5:30 min).  

**No se aceptan binarios ejecutables como entrega.**

---

## 9. Entregables parciales (avances semanales)

| Semana | Entrega | Contenido esperado |
|---------|----------|--------------------|
| 1 | **Planificación y definición** | Revisión del documento, roles del equipo, objetivos, requisitos extraídos. |
| 2 | **Diseño de arquitectura** | Diagramas de bloques, estructura general, diseño del bus, caché y memoria. |
| 3 | **Implementación base** | Módulos de memoria, caché y bus con manejo inicial de coherencia. |
| 4 | **Integración y pruebas** | Validación parcial, corrección de errores, preparación de la entrega final. |

---

## 10. Evaluación del proyecto

| Criterio | Peso | Descripción |
|-----------|------|-------------|
| Avances semanales | 20% | Cumplimiento de entregas parciales y progreso ordenado. |
| Presentación funcional | 40% | Demostración del sistema y defensa oral. |
| Artículo científico | 20% | Documento técnico con resultados y análisis. |
| Presentación final (video) | 20% | Presentación en formato claro, dirigida a público general. |
| **Extra (opcional)** | +10% | Artículo y video completamente en inglés. |

---

## 11. Consideraciones adicionales

- El código debe compilar y ejecutarse correctamente desde el `makefile`.  
- Se recomienda una interfaz gráfica simple si se desea.  
- La entrega debe realizarse en **Tec Digital → Evaluaciones** antes de las 11:59 p.m.  
- Todos los miembros del grupo deben participar en la defensa.  
- Los hilos deben sincronizarse correctamente mediante *mutex* o *semaphores*.  

---

---

## 13. Arquitectura de la solución implementada

### 13.1 Organización del código

El proyecto está organizado de la siguiente manera:

```
MESI-MultiProcessor-Model/
├── src/
│   ├── bus/              # Sistema de bus (arbitraje, handlers, callbacks)
│   │   ├── bus.c/h       # Gestión del bus con Round-Robin
│   │   └── handlers.c/h  # Handlers MESI (BUS_RD, BUS_RDX, BUS_UPGR, BUS_WB)
│   ├── cache/            # Sistema de caché con coherencia MESI
│   │   └── cache.c/h     # Operaciones read/write, política LRU, callbacks
│   ├── pe/               # Processing Elements
│   │   ├── pe.c/h        # Gestión de threads de PE
│   │   ├── isa.c/h       # Ejecución de instrucciones
│   │   ├── registers.c/h # Banco de registros
│   │   └── loader.c/h    # Cargador de programas (.asm)
│   ├── memory/           # Memoria principal compartida
│   │   └── memory.c/h    # Operaciones de lectura/escritura de bloques
│   ├── mesi/             # Protocolo MESI
│   │   └── mesi.c/h      # Definiciones de estados y transiciones
│   ├── stats/            # Sistema de estadísticas
│   │   ├── cache_stats.c/h
│   │   ├── bus_stats.c/h
│   │   └── memory_stats.c/h
│   ├── dotprod/          # Problema del producto punto
│   │   └── dotprod.c/h   # Inicialización y verificación
│   ├── include/          # Configuración global
│   │   └── config.h      # Constantes del sistema
│   └── main.c            # Punto de entrada
├── asm/                  # Programas assembly por PE
│   ├── dotprod_pe0.asm   # PE0: elementos [0-3]
│   ├── dotprod_pe1.asm   # PE1: elementos [4-7]
│   ├── dotprod_pe2.asm   # PE2: elementos [8-11]
│   └── dotprod_pe3.asm   # PE3: elementos [12-15] + reducción
└── makefile              # Compilación automatizada
```

### 13.2 Componentes principales

#### **1. Processing Elements (PE)**
- **Arquitectura:** Cada PE es un thread independiente con:
  - 8 registros de 64 bits (R0-R7)
  - Program Counter (PC)
  - Zero flag
  - Caché privada de 2-way set associative
  
- **Scheduling:** Se utiliza `sched_yield()` después de cada instrucción para simular el tiempo de ejecución y garantizar un scheduling justo entre PEs.

#### **2. Sistema de Caché**
- **Configuración:**
  - 2-way set associative
  - 16 sets (índice de 4 bits)
  - Bloques de 4 doubles (32 bytes)
  - Política LRU para reemplazo

- **Operaciones:**
  - **READ:** Busca en caché → Hit devuelve dato → Miss envía BUS_RD
  - **WRITE:** 
    - Hit en M: escribe directamente
    - Hit en E: escribe y pasa a M
    - Hit en S: envía BUS_UPGR, invalida copias, pasa a M
    - Miss: envía BUS_RDX con callback para escritura atómica

#### **3. Sistema de Bus**
- **Arbitraje:** Round-Robin entre los 4 PEs
- **Thread dedicado:** Procesa solicitudes serializadas
- **Mecanismo de callback:** Permite ejecutar operaciones atómicas después de los handlers
- **Señales soportadas:**
  - `BUS_RD`: Lectura compartida
  - `BUS_RDX`: Lectura exclusiva para escritura
  - `BUS_UPGR`: Upgrade de S a M
  - `BUS_WB`: Writeback a memoria

#### **4. Protocolo MESI**
Transiciones de estados implementadas:

```
┌─────────┐
│ INVALID │
└─────────┘
    ↑  ↓
    │  │ BUS_RD (único lector) → E
    │  │ BUS_RD (compartido)   → S
    │  │ BUS_RDX               → M
    │  │
┌─────────┐    write    ┌──────────┐
│EXCLUSIVE│ ───────────→ │ MODIFIED │
└─────────┘              └──────────┘
    │                         │
    │ BUS_RD (otro PE)        │ BUS_RD/BUS_RDX
    ↓                         ↓
┌─────────┐    write    ┌──────────┐
│ SHARED  │ ───────────→ │ MODIFIED │
└─────────┘ BUS_UPGR    └──────────┘
```

### 13.3 Solución al race condition de BUS_RDX

**Problema original:** En WRITE MISS, después de que el handler BUS_RDX trae el bloque, el PE debe escribir su valor. Había una ventana de tiempo donde otro PE podía leer el bloque antes de que se escribiera el valor, causando inconsistencias.

**Solución implementada (Callback Architecture):**

1. **PE prepara contexto de escritura:**
   ```c
   WriteCallbackContext ctx = {
       .victim = victim,
       .offset = offset,
       .value = value,
       ...
   };
   ```

2. **PE desbloquea su caché y llama al bus:**
   ```c
   pthread_mutex_unlock(&cache->mutex);
   bus_broadcast_with_callback(bus, BUS_RDX, addr, pe_id, 
                                write_callback, &ctx);
   ```

3. **Bus thread ejecuta en orden:**
   - **Handler:** Trae bloque de memoria/otra caché
   - **Callback:** Escribe el valor y marca estado M
   - **Señal:** Notifica al PE que completó

4. **Ventajas:**
   - **Atomicidad:** La escritura ocurre en el contexto del bus thread (serializado)
   - **Sin deadlock:** PE libera su mutex antes de llamar al bus
   - **Sin race condition:** Callback ejecuta antes de señalizar al PE

### 13.4 Estadísticas recolectadas

Por cada PE:
- Read hits/misses
- Write hits/misses
- Hit rate / Miss rate
- Transiciones de estado MESI (M→S, E→M, S→I, etc.)
- Invalidaciones recibidas/enviadas
- Tráfico del bus (bytes leídos/escritos)
- Operaciones de BusRd, BusRdX, BusUpgr, Writebacks

Globales:
- Tráfico total del bus
- Accesos a memoria principal
- Uso del bus por PE

### 13.5 Compilación y ejecución

```bash
# Compilar
make clean && make

# Ejecutar producto punto
./mp_mesi dotprod

# Ejecutar tests
./tests/verify.sh
```

**Resultado esperado:** 
- Producto punto: 136.00
- Tasa de éxito: 100% (con sched_yield)

---

## 14. Referencias

1. Hennessy, J. L., & Patterson, D. A. *Computer Architecture: A Quantitative Approach.* Elsevier, 2017.  
2. Wikipedia: [Producto escalar](https://es.wikipedia.org/wiki/Producto_escalar)  
3. Greaves, D. J. *Modern System-on-Chip Design on Arm.* Arm Education Media.  
4. Canal de YouTube: [keeroyz](https://www.youtube.com/user/keeroyz)

---

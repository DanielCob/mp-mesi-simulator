# Diseño del simulador MP con coherencia MESI

Este documento resume, de forma breve y práctica, los componentes clave del simulador, sus interacciones y las decisiones de diseño.

---

## 1) Visión general

- 4 Processing Elements (PEs) con cachés privadas (2-way, 16 sets, 4 doubles por bloque).
- Memoria principal compartida (512 doubles).
- Bus con arbitraje Round-Robin que implementa coherencia MESI.
- Problema objetivo: producto punto paralelo de dos vectores `double`.

---

## 2) Protocolo MESI (resumen)

Estados por línea de caché:
- M: Modified (única copia, sucia)
- E: Exclusive (única copia, limpia)
- S: Shared (compartida, limpia)
- I: Invalid

Señales de bus:
- BUS_RD: lectura compartida (I→S o I→E dependiendo de si hay sharers)
- BUS_RDX: lectura exclusiva para escritura (I→M, invalida otros)
- BUS_UPGR: S→M sin recargar línea (invalida otros sharers)
- BUS_WB: write-back a memoria

Contabilización:
- Métricas por PE (hits, misses, invalidaciones, transiciones MESI, tráfico, conteo de señales).
- Métricas globales del bus (tráfico de datos y control, invalidaciones broadcast, uso por PE).

---

## 3) Caché y políticas

- 2-way set associative, LRU por set (localidad temporal).
- Tamaño de bloque: 4 doubles -> 32 bytes (localidad espacial).
- Write-allocate + write-back.
- Transiciones MESI en read/write de acuerdo con el resultado del acceso y las señales del bus.

Edge cases cubiertos:
- Evicción con write-back en M.
- Lecturas/escrituras desalineadas: se reportan con logs.

---

## 4) Bus y atomicidad de escritura

- Un hilo dedicado procesa las solicitudes serializadas.
- Arquitectura con callback para WRITE MISS:
  1) PE solicita BUS_RDX para traer línea.
  2) Handler del bus trae línea desde memoria/u otra caché.
  3) Callback del PE escribe el valor y fija estado M, aún dentro del contexto del bus.
- Beneficios: evita condiciones de carrera, respeta orden global y facilita estadísticas.

---

## 5) Estructura de memoria y configuración

- Área de configuración compartida (direcciones bajas) con:
  - Rutas a A/B, resultados parciales, flags, resultado final.
  - Parámetros por-PE: start_index y segment_size.
  - NUM_PES y valor de barrera `-(NUM_PES-1)`.
- Área de sincronización/resultado compacta (RESULTS, FLAGS, FINAL_RESULT).
- Vectores A y B al final del espacio, con posible desalineamiento configurable.

Los programas ASM se generan desde `scripts/generate_asm.py` leyendo `src/include/config.h` (acepta hex/expresiones) y parametrizando los índices de configuración y direcciones de sync.

---

## 6) Depuración y visibilidad

- Logger con niveles y colores (controlados por env), salida serializada con mutex.
- CLI de depuración (SIM_DEBUG=1):
  - pause/cont, step/steppe, break pc <n>
  - cache <pe> (líneas válidas, estado/tag/base y contenido)
  - regs <pe|all>, memline <addr>
  - stats bus
  - quit
- Direcciones impresas en hex para coherencia.

---

## 7) Pruebas rápidas y perfiles

- Resultado de producto punto se verifica contra cálculo esperado.
- Estadísticas por PE y globales impresas al final.
- SIM_MAX_ITERS permite limitar (o deshabilitar) iteraciones por PE para evitar bucles infinitos.

---
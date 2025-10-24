## Simulador MP con coherencia MESI — Guía rápida

### Propósito
Simula un sistema multiprocesador de 4 PEs con coherencia de caché MESI para calcular en paralelo el producto punto de dos vectores de `double`. Incluye estadísticas detalladas y un depurador interactivo opcional.

---

## Cómo compilar y ejecutar
Abra una terminal de linux en la carpeta que contiene el Makefile (la misma de este README.md), y ejecute los siguientes comandos:

```bash
make            # genera ASM, compila y enlaza
./mp_mesi       # ejecuta el simulador
```

```bash
make            
SIM_DEBUG=1 LOG_LEVEL=DEBUG ./mp_mesi     # ejecuta el simulador con el debugger
```

Notas:
- El `make` genera automáticamente los programas ASM según `src/include/config.h`.
- Para ejecutar con depurador interactivo, activa las siguiente variables de entorno a través de este comando `SIM_DEBUG=1 LOG_LEVEL=DEBUG ./mp_mesi`.

---
## Comandos del makefile
- `make`: compila
- `make run`: compila y corre (sin depurador)
- `make clean`: elimina `/obj`
- `make cleanall`: elimina `/obj` y `/asm`

---

## Variables de entorno
Estas son otras variables de entorno que también se pueden usar.

- `LOG_LEVEL=ERROR|WARN|INFO|DEBUG`
   - Nivel de log global.
- `LOG_COLOR=auto|always|never` y/o `NO_COLOR=1`
   - Controla colores en consola (respeta NO_COLOR estándar).
- `SIM_DEBUG=1`
   - Habilita CLI de depuración (pause/cont, step, breakpoints, cache/regs/mem/stats).
   - Si quieres ver mensajes de debug de todo el sistema (PEs, Caches, Bus y memoria), ejecuta agrega la variable de entorno LOG_LEVEL=DEBUG
- `SIM_MAX_ITERS=N`
   - Límite de iteraciones por PE. 0 o negativo = sin límite.

---

## Parámetros editables en `src/include/config.h`
Es posible editar estos parámetros para cambiar los vectores a cargar:

- VECTOR_SIZE: tamaño del vector (p. ej., 16, 19, 64). Regenera ASM al compilar.
- VECTOR_A_FILE, VECTOR_B_FILE: rutas a CSVs de entrada. Disponibles:
   - `data/vector_decimals_a_16.csv`, `data/vector_decimals_b_16.csv`
   - `data/vector_decimals_a_19.csv`, `data/vector_decimals_b_19.csv`
   - `data/vector_decimals_a_64.csv`, `data/vector_decimals_b_64.csv`
- MISALIGNMENT_OFFSET: desalineamiento global de vectores (en elementos). (3)

 #### Nota: El tamaño de vector debe ser igual al tamaño del vector cargado, para obtener el resultado esperado.

También es posible editar estos parámetros para cambiar el comportamiento del sistema.
- NUM_PES, SETS, WAYS, BLOCK_SIZE, MEM_SIZE: parámetros de arquitectura.
- BUS_CONTROL_SIGNAL_SIZE, INVALIDATION_CONTROL_SIGNAL_SIZE: tamaño (bytes) del tráfico de control de bus.
- ASM_DOTPROD_PE*_PATH: rutas de programas ASM (solo cambiar si se reubican archivos).

Direcciones de memoria (áreas de config/sync/vectores) se derivan automáticamente; no es necesario editarlas.

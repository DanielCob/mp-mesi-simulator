# Implementación de LOOPS en Assembly

## 🔄 Descripción

El sistema usa **loops reales con contador decreciente** en lugar de código desenrollado. Los loops usan las instrucciones `DEC` (decrementar) y `JNZ` (Jump if Not Zero) para crear bucles compactos y eficientes.

## 📊 Estructura del LOOP

### Patrón básico:

```assembly
# Inicializar contador
MOV R3, SEGMENT_SIZE    # R3 = número de iteraciones

LOOP_START:
    # [Cuerpo del loop - procesar elemento]
    
    # Actualizar índice
    INC R1              # i++
    
    # Control del loop
    DEC R3              # contador--
    JNZ LOOP_START      # Si R3 != 0, volver a LOOP_START
```

## 🎯 ¿Cómo funciona?

### 1. Instrucción DEC (Decrement)

La instrucción `DEC` hace dos cosas:
1. **Decrementa** el valor del registro: `R3 = R3 - 1.0`
2. **Actualiza zero_flag**: 
   - Si resultado == 0.0 → `zero_flag = 1`
   - Si resultado != 0.0 → `zero_flag = 0`

```c
// En isa.c
case OP_DEC:
    double val = reg_read(rf, inst->rd);
    val -= 1.0;
    reg_write(rf, inst->rd, val);
    rf->zero_flag = (val == 0.0) ? 1 : 0;  // Actualizar flag
    rf->pc++;
    break;
```

### 2. Instrucción JNZ (Jump if Not Zero)

La instrucción `JNZ` salta a una etiqueta **solo si** `zero_flag == 0`:

```c
// En isa.c
case OP_JNZ:
    if (rf->zero_flag == 0) {  // Si resultado anterior != 0
        rf->pc = inst->label;   // Saltar a etiqueta
    } else {
        rf->pc++;               // Continuar secuencialmente
    }
    break;
```

### 3. Flujo de ejecución

**Primera iteración** (R3 = 4.0):
```
LOOP_START:     PC=15
    LOAD...     PC=16
    FADD...     PC=17
    ...
    INC R1      PC=23, R1 = 1.0
    DEC R3      PC=24, R3 = 3.0, zero_flag = 0
    JNZ         PC=15 (SALTA porque zero_flag=0)
```

**Segunda iteración** (R3 = 3.0):
```
LOOP_START:     PC=15
    ...
    DEC R3      PC=24, R3 = 2.0, zero_flag = 0
    JNZ         PC=15 (SALTA porque zero_flag=0)
```

**Última iteración** (R3 = 1.0):
```
LOOP_START:     PC=15
    ...
    DEC R3      PC=24, R3 = 0.0, zero_flag = 1
    JNZ         PC=25 (NO SALTA porque zero_flag=1)
    MOV R5...   PC=25 (continúa después del loop)
```

## 📝 Ejemplo completo: PE0

```assembly
# ============================================================================
# Inicialización
# ============================================================================
MOV R0, 0.0         # R0 = acumulador
MOV R1, 0.0         # R1 = índice (i = 0)
MOV R2, 100.0       # R2 = base vector B
MOV R3, 4.0         # R3 = contador (SEGMENT_SIZE desde config.h)

# ============================================================================
# LOOP: Procesar 4 elementos
# ============================================================================
LOOP_START:         # Etiqueta (PC = 15 por ejemplo)
LOAD R4, [R1]       # R4 = A[i]
FADD R5, R2, R1     # R5 = 100 + i
LOAD R6, [R5]       # R6 = B[i]
FMUL R7, R4, R6     # R7 = A[i] * B[i]
FADD R0, R0, R7     # acum += A[i] * B[i]
INC R1              # i++
DEC R3              # contador--, actualiza zero_flag
JNZ LOOP_START      # Si contador != 0, volver a LOOP_START

# Cuando R3 llega a 0, sale del loop
MOV R5, 200.0       # Continúa aquí después del loop
STORE R0, [R5]
...
```

## 🔢 Tabla de ejecución (SEGMENT_SIZE=4)

| Iteración | R1 (índice) | R3 (contador) | zero_flag | Acción JNZ |
|-----------|-------------|---------------|-----------|------------|
| 1 | 0→1 | 4→3 | 0 | SALTA a LOOP_START |
| 2 | 1→2 | 3→2 | 0 | SALTA a LOOP_START |
| 3 | 2→3 | 2→1 | 0 | SALTA a LOOP_START |
| 4 | 3→4 | 1→0 | **1** | **NO SALTA** (sale del loop) |

## 🎨 PE3: Tres loops diferentes

### LOOP 1: Procesar segmento propio
```assembly
MOV R3, 4.0        # SEGMENT_SIZE
LOOP_START:
    [procesar elemento]
    DEC R3
    JNZ LOOP_START
```

### LOOP 2: Barrier (busy-wait)
```assembly
WAIT_LOOP:
    LOAD flags de PE0, PE1, PE2
    FADD R6, R1, R2     # Sumar flags
    FADD R6, R6, R3
    MOV R7, 232.0
    LOAD R7, [R7]       # R7 = -3.0
    FADD R6, R6, R7     # R6 = suma - 3.0
    JNZ WAIT_LOOP       # Si suma != 3.0, esperar
```

### LOOP 3: Reducción
```assembly
MOV R2, 4.0         # NUM_PES (contador)
REDUCE_LOOP:
    LOAD R4, [R1]
    FADD R0, R0, R4
    FADD R1, R1, R3    # siguiente dirección
    DEC R2
    JNZ REDUCE_LOOP
```

## ⚙️ Parametrización desde config.h

El contador del loop se inicializa automáticamente:

```c
// En config.h
#define VECTOR_SIZE 32
#define SEGMENT_SIZE (VECTOR_SIZE / NUM_PES)  // = 8
```

```python
# En generate_asm.py
code = f"""
MOV R3, {float(SEGMENT_SIZE)}        # R3 = 8.0 (leído desde config.h)
"""
```

```assembly
# Resultado generado
MOV R3, 8.0        # Contador automático para 8 iteraciones
LOOP_START:
    ...
    DEC R3
    JNZ LOOP_START
```

## 📈 Ventajas

| Aspecto | Sin loops (unrolled) | Con loops (DEC+JNZ) |
|---------|---------------------|---------------------|
| **Líneas de código** | ~40 por PE | ~20 por PE |
| **Escalabilidad** | Crece con SEGMENT_SIZE | Constante |
| **Legibilidad** | Repetitivo | Compacto |
| **Mantenibilidad** | Difícil cambiar lógica | Un solo lugar |
| **Parametrizable** | No | ✅ Sí (desde config.h) |

## 🚀 Comparación de tamaño

### SEGMENT_SIZE = 4 (sin loop):
```assembly
# Elemento 0: 6 instrucciones
# Elemento 1: 6 instrucciones  
# Elemento 2: 6 instrucciones
# Elemento 3: 6 instrucciones
# Total: 24 instrucciones del cuerpo
```

### SEGMENT_SIZE = 4 (con loop):
```assembly
MOV R3, 4.0        # 1 instrucción inicial
LOOP_START:        # 8 instrucciones del cuerpo
    LOAD...
    FADD...
    LOAD...
    FMUL...
    FADD...
    INC...
    DEC...
    JNZ...
# Total: 9 instrucciones (ejecutadas 4 veces)
```

### SEGMENT_SIZE = 32:
- **Sin loop**: 192 instrucciones de código
- **Con loop**: 9 instrucciones (ejecutadas 32 veces)
- **Ahorro**: 183 líneas de código generado

## 🎯 Conclusión

El uso de loops con `DEC` y `JNZ` hace el código:
- ✅ **Más compacto**: Menos líneas generadas
- ✅ **Escalable**: Funciona con cualquier SEGMENT_SIZE
- ✅ **Parametrizable**: Contador desde config.h
- ✅ **Eficiente**: Misma cantidad de instrucciones ejecutadas
- ✅ **Tradicional**: Patrón estándar de loops en assembly

---

**Implementado en**: v2.1.0  
**Fecha**: 2025-10-21

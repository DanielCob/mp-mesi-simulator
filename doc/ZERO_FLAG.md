# Sistema de Bandera de Cero (Zero Flag)

## Resumen

Se ha implementado un sistema de **bandera de cero** (zero flag) en el simulador MESI, que simplifica el uso de la instrucción `JNZ` y hace más intuitiva la escritura de código assembly.

## Arquitectura

### RegisterFile

El `RegisterFile` ahora incluye un campo adicional:

```c
typedef struct {
    double regs[NUM_REGISTERS];  // REG0 - REG7
    uint64_t pc;                 // Program Counter
    int zero_flag;               // Bandera de cero
} RegisterFile;
```

### Bandera de Cero

- **Valor 1**: La última operación aritmética resultó en 0.0
- **Valor 0**: La última operación aritmética resultó en un valor diferente de 0.0

## Operaciones que Actualizan la Bandera

Las siguientes instrucciones actualizan automáticamente la bandera de cero:

1. **FADD** (suma flotante)
2. **FMUL** (multiplicación flotante)
3. **INC** (incremento)
4. **DEC** (decremento)

### Ejemplo

```asm
LOAD R0 300      # R0 = 6.0
INC R0           # R0 = 7.0, zero_flag = 0
DEC R0           # R0 = 6.0, zero_flag = 0
DEC R0           # R0 = 5.0, zero_flag = 0
# ... más decrementos ...
DEC R0           # R0 = 0.0, zero_flag = 1  ← Bandera se activa
```

## Instrucción JNZ

### Sintaxis Anterior (con registro explícito)

```asm
LOOP:
    DEC R0
    JNZ R0 LOOP    # Salta si R0 != 0
```

### Sintaxis Nueva (con zero_flag)

```asm
LOOP:
    DEC R0
    JNZ LOOP       # Salta si zero_flag == 0 (última op != 0)
```

### Comportamiento

- **JNZ label**: Salta al label si `zero_flag == 0` (última operación no fue cero)
- Si `zero_flag == 1` (última operación fue cero), continúa con la siguiente instrucción

## Ventajas

### 1. Simplicidad en el Código Assembly

El código es más limpio y legible:

```asm
# Antes
LOOP:
    INC R1
    DEC R0
    JNZ R0 LOOP

# Ahora
LOOP:
    INC R1
    DEC R0
    JNZ LOOP
```

### 2. Flexibilidad

Cualquier registro puede ser usado para controlar el loop, no solo R0:

```asm
# Loop controlado por R3
FADD R3 R1 R2
JNZ LOOP          # Salta si R3 != 0

# Loop controlado por R5
DEC R5
JNZ LOOP          # Salta si R5 != 0
```

### 3. Similitud con Arquitecturas Reales

Este comportamiento es similar a arquitecturas como x86, ARM, y RISC-V, donde las operaciones aritméticas actualizan flags de estado.

## Patrón Común de Uso

### Countdown Loop

```asm
LOAD R0 300      # Cargar contador
INC R0           # Ajustar valor inicial
# ... más incrementos ...

LOOP:
    # Cuerpo del loop
    INC R1       # Hacer algo útil
    
    # Control del loop
    DEC R0       # Decrementar contador
    JNZ LOOP     # Repetir si contador != 0
    
HALT             # Terminar cuando contador == 0
```

### Ejemplo Real (PE2 - test_loop.asm)

```asm
LOAD R0 300      # R0 = 6.0
INC R0           # R0 = 7.0
INC R0           # R0 = 8.0
# ... 6 incrementos más ...
INC R0           # R0 = 14.0

LOOP:
    INC R1       # R1++
    DEC R0       # R0--, actualiza zero_flag
    JNZ LOOP     # Salta si zero_flag == 0
    
HALT             # Llega aquí cuando R0 == 0
# Resultado: R0=0, R1=14
```

## Debugging

La bandera de cero se muestra en el estado final de los registros:

```
========== PE2 Register File ==========
PC: 12
Zero Flag: 1
Registers:
  R0: 0.000000    R1: 14.000000   R2: 0.000000    R3: 0.000000
```

Durante la ejecución, los mensajes de debug muestran:

```
[PE2] DEC: R0 (1.000000) - 1 = 0.000000
[PE2] JNZ: zero_flag=1 (última op == 0), no salta
```

## Implementación Técnica

### Función de Actualización

```c
void reg_update_zero_flag(RegisterFile* rf, double value) {
    rf->zero_flag = (value == 0.0) ? 1 : 0;
}
```

### Uso en Operaciones Aritméticas

```c
case OP_DEC:
    val_a = reg_read(rf, inst->rd);
    result = val_a - 1.0;
    reg_write(rf, inst->rd, result);
    reg_update_zero_flag(rf, result);  // Actualizar bandera
    rf->pc++;
    break;
```

### Uso en JNZ

```c
case OP_JNZ:
    if (rf->zero_flag == 0) {
        // Última operación != 0, saltar
        rf->pc = inst->label;
    } else {
        // Última operación == 0, no saltar
        rf->pc++;
    }
    break;
```

## Comparación con Otras Arquitecturas

### x86

```asm
dec ecx          ; Decrementa ECX, actualiza ZF
jnz loop         ; Salta si ZF == 0
```

### ARM

```asm
subs r0, r0, #1  ; Resta 1 de R0, actualiza flags
bne loop         ; Salta si no es cero (Z flag == 0)
```

### MESI Simulator

```asm
DEC R0           ; Decrementa R0, actualiza zero_flag
JNZ LOOP         ; Salta si zero_flag == 0
```

## Conclusión

El sistema de bandera de cero hace que el código assembly sea:
- Más legible
- Más fácil de escribir
- Más similar a arquitecturas reales
- Más flexible (cualquier registro puede controlar un loop)

Esta implementación sigue las mejores prácticas de diseño de arquitecturas de computadoras modernas.

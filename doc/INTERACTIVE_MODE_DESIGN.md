# Diseño: Modo Interactivo Minimalista

## 🎯 Objetivo

Permitir inspección del simulador **sin ralentizar la ejecución normal**, con una interfaz mínima de solo texto.

---

## 🚀 Solución: Modo de Arranque Configurable

### **Problema Identificado:**
- Los PEs ejecutan demasiado rápido (milisegundos)
- No hay tiempo de escribir comandos durante la ejecución
- Necesitamos controlar el ritmo sin complicar el código

### **Solución:**
Agregar un **flag global de control** que los PEs chequean antes de ejecutar cada instrucción.

---

## 📐 Arquitectura

### **1. Estructura de Control**

```c
// src/include/debug_ctrl.h
#ifndef DEBUG_CTRL_H
#define DEBUG_CTRL_H

#include <pthread.h>
#include <stdbool.h>

typedef enum {
    MODE_NORMAL,      // Ejecución sin pausas
    MODE_STEP,        // Pausa después de cada instrucción
    MODE_CONTINUE,    // Continuar N instrucciones
    MODE_QUIT         // Terminar ejecución
} DebugMode;

typedef struct {
    DebugMode mode;
    int steps_remaining;      // Para "continuar N pasos"
    bool wait_for_input;      // Si debe esperar input del usuario
    
    pthread_mutex_t lock;
    pthread_cond_t cond;
    
} DebugController;

// Inicializar controlador
void debug_init(DebugController* ctrl);

// PEs llaman esto antes de cada instrucción
void debug_checkpoint(DebugController* ctrl, int pe_id);

// Thread principal usa esto para controlar
void debug_set_mode(DebugController* ctrl, DebugMode mode);
void debug_step(DebugController* ctrl, int n);

#endif
```

### **2. Implementación del Control**

```c
// src/debug/debug_ctrl.c
#include "debug_ctrl.h"

void debug_init(DebugController* ctrl) {
    ctrl->mode = MODE_NORMAL;  // Por defecto, sin pausas
    ctrl->steps_remaining = 0;
    ctrl->wait_for_input = false;
    pthread_mutex_init(&ctrl->lock, NULL);
    pthread_cond_init(&ctrl->cond, NULL);
}

void debug_checkpoint(DebugController* ctrl, int pe_id) {
    pthread_mutex_lock(&ctrl->lock);
    
    // Si modo normal, continuar sin pausa
    if (ctrl->mode == MODE_NORMAL) {
        pthread_mutex_unlock(&ctrl->lock);
        return;
    }
    
    // Si modo quit, detener
    if (ctrl->mode == MODE_QUIT) {
        pthread_mutex_unlock(&ctrl->lock);
        pthread_exit(NULL);
    }
    
    // Si modo continue con pasos restantes, decrementar
    if (ctrl->mode == MODE_CONTINUE && ctrl->steps_remaining > 0) {
        ctrl->steps_remaining--;
        if (ctrl->steps_remaining == 0) {
            ctrl->mode = MODE_STEP;  // Volver a paso a paso
        }
        pthread_mutex_unlock(&ctrl->lock);
        return;
    }
    
    // Si modo step, esperar señal del usuario
    if (ctrl->mode == MODE_STEP) {
        ctrl->wait_for_input = true;
        
        // Esperar hasta que el usuario dé el comando
        while (ctrl->wait_for_input) {
            pthread_cond_wait(&ctrl->cond, &ctrl->lock);
        }
    }
    
    pthread_mutex_unlock(&ctrl->lock);
}

void debug_continue(DebugController* ctrl) {
    pthread_mutex_lock(&ctrl->lock);
    ctrl->wait_for_input = false;
    pthread_cond_signal(&ctrl->cond);
    pthread_mutex_unlock(&ctrl->lock);
}

void debug_step(DebugController* ctrl, int n) {
    pthread_mutex_lock(&ctrl->lock);
    ctrl->mode = MODE_CONTINUE;
    ctrl->steps_remaining = n;
    ctrl->wait_for_input = false;
    pthread_cond_signal(&ctrl->cond);
    pthread_mutex_unlock(&ctrl->lock);
}

void debug_set_mode(DebugController* ctrl, DebugMode mode) {
    pthread_mutex_lock(&ctrl->lock);
    ctrl->mode = mode;
    if (mode == MODE_NORMAL) {
        ctrl->wait_for_input = false;
        pthread_cond_broadcast(&ctrl->cond);
    }
    pthread_mutex_unlock(&ctrl->lock);
}
```

### **3. Integración en PE**

```c
// src/pe/pe.c (modificación)

void* pe_run(void* arg) {
    PE* pe = (PE*)arg;
    
    // ... cargar programa ...
    
    while (pe->rf.pc < pe->program_size) {
        // ⭐ CHECKPOINT: Pausar aquí si modo interactivo
        debug_checkpoint(global_debug_ctrl, pe->id);
        
        // Mostrar info si estamos en modo step
        if (global_debug_ctrl->mode == MODE_STEP) {
            pe_print_state(pe);  // Función helper
        }
        
        // Ejecutar instrucción normal
        Instruction inst = pe->program[pe->rf.pc];
        isa_execute(inst, pe);
        
        pe->rf.pc++;
    }
    
    printf("[PE%d] ✓ Finalizado\n", pe->id);
    return NULL;
}
```

### **4. Funciones de Visualización (Estilo GDB)**

```c
// src/pe/pe_debug.c (nuevo archivo)

void pe_print_state(PE* pe) {
    // Mostrar instrucción actual
    Instruction inst = pe->program[pe->rf.pc];
    printf("\nPE%d at PC=%lu: ", pe->id, pe->rf.pc);
    print_instruction(&inst);
    printf("\n");
}

void pe_print_registers(PE* pe) {
    printf("PE%d registers:\n", pe->id);
    printf("  R0: %.2f    R1: %.2f    R2: %.2f    R3: %.2f\n",
           pe->rf.r[0], pe->rf.r[1], pe->rf.r[2], pe->rf.r[3]);
    printf("  PC: %lu     ZF: %d\n", pe->rf.pc, pe->rf.zero_flag);
}

void cache_print_state(Cache* cache, int pe_id) {
    printf("Cache PE%d:\n", pe_id);
    printf("  Set  Way  State  Tag    LRU\n");
    
    for (int set = 0; set < SETS; set++) {
        for (int way = 0; way < WAYS; way++) {
            CacheLine* line = &cache->lines[set][way];
            
            if (line->state != I) {
                char state = mesi_state_to_char(line->state);
                printf("  %3d  %3d    %c     %3d     %d\n",
                       set, way, state, line->tag, line->lru_bit);
            }
        }
    }
    
    // Si no hay líneas válidas
    bool empty = true;
    for (int set = 0; set < SETS && empty; set++) {
        for (int way = 0; way < WAYS && empty; way++) {
            if (cache->lines[set][way].state != I) {
                empty = false;
            }
        }
    }
    if (empty) {
        printf("  (empty - all lines in Invalid state)\n");
    }
}

void memory_print_range(Memory* mem, int start, int end) {
    printf("Memory [%d-%d]:\n", start, end);
    for (int addr = start; addr <= end; addr++) {
        if ((addr - start) % 4 == 0) {
            if (addr != start) printf("\n");
            printf("  0x%04x: ", addr);
        }
        printf("%.2f ", mem->data[addr]);
    }
    printf("\n");
}

void bus_print_stats(Bus* bus) {
    printf("Bus statistics:\n");
    printf("  Transactions: %d total\n", bus->stats.total_transactions);
    printf("    BUS_RD:   %d\n", bus->stats.bus_rd_count);
    printf("    BUS_RDX:  %d\n", bus->stats.bus_rdx_count);
    printf("    BUS_UPGR: %d\n", bus->stats.bus_upgr_count);
    printf("    BUS_WB:   %d\n", bus->stats.bus_wb_count);
    printf("  Invalidations sent: %d\n", bus->stats.invalidations_sent);
    printf("  Traffic: %ld bytes (%.2f KB)\n", 
           bus->stats.bytes_transferred,
           bus->stats.bytes_transferred / 1024.0);
}
```

### **5. Main con Menú Inicial**

```c
// src/main.c (modificación)

#include "debug_ctrl.h"

DebugController* global_debug_ctrl = NULL;

void print_menu() {
    printf("\n");
    printf("MESI MultiProcessor Simulator\n");
    printf("-----------------------------\n");
    printf("Execution mode:\n");
    printf("  1. Normal execution (fast)\n");
    printf("  2. Step-by-step debugging\n");
    printf("  3. Exit\n");
    printf("\nSelect [1-3]: ");
}

void interactive_loop(DebugController* ctrl, PE* pes, Cache* caches, 
                      Memory* mem, Bus* bus) {
    char input[256];
    
    while (1) {
        printf("\n> ");
        fgets(input, sizeof(input), stdin);
        
        // Eliminar newline
        input[strcspn(input, "\n")] = 0;
        
        // Comando vacío = siguiente paso
        if (strlen(input) == 0) {
            debug_continue(ctrl);
            break;
        }
        
        // Parsear comando
        char cmd[32], arg1[32], arg2[32];
        int n = sscanf(input, "%s %s %s", cmd, arg1, arg2);
        
        if (strcmp(cmd, "c") == 0) {
            // Continuar N pasos (o hasta el final)
            if (n > 1) {
                int steps = atoi(arg1);
                debug_step(ctrl, steps);
            } else {
                debug_set_mode(ctrl, MODE_NORMAL);
                pthread_cond_broadcast(&ctrl->cond);
            }
            break;
        }
        else if (strcmp(cmd, "p") == 0) {
            // Imprimir estado
            if (n == 1) {
                // Sin argumentos: imprimir todo
                for (int i = 0; i < NUM_PES; i++) {
                    pe_print_state(&pes[i]);
                }
            }
            else if (strcmp(arg1, "cache") == 0) {
                int pe_id = atoi(arg2);
                cache_print_state(&caches[pe_id], pe_id);
            }
            else if (strcmp(arg1, "mem") == 0) {
                int addr = atoi(arg2);
                memory_print_range(mem, addr, addr + 10);
            }
            else if (strcmp(arg1, "bus") == 0) {
                bus_stats_print(&bus->stats);
            }
            else if (strcmp(arg1, "regs") == 0) {
                int pe_id = atoi(arg2);
                pe_print_registers(&pes[pe_id]);
            }
        }
        else if (strcmp(cmd, "q") == 0) {
            debug_set_mode(ctrl, MODE_QUIT);
            pthread_cond_broadcast(&ctrl->cond);
            break;
        }
        else if (strcmp(cmd, "h") == 0 || strcmp(cmd, "help") == 0) {
            printf("\nAvailable commands:\n");
            printf("  <ENTER>       Next instruction\n");
            printf("  c             Continue to end\n");
            printf("  c N           Continue N instructions\n");
            printf("  p             Print all PE states\n");
            printf("  p cache N     Show cache of PE N\n");
            printf("  p mem ADDR    Show memory from ADDR\n");
            printf("  p bus         Show bus statistics\n");
            printf("  p regs N      Show registers of PE N\n");
            printf("  q             Quit\n");
            printf("  h             Help\n");
        }
        else {
            printf("Comando no reconocido. Use 'h' para ayuda.\n");
        }
    }
}

int main() {
    // Inicializar controlador debug
    DebugController debug_ctrl;
    debug_init(&debug_ctrl);
    global_debug_ctrl = &debug_ctrl;
    
    // Mostrar menú
    print_menu();
    
    int choice;
    scanf("%d", &choice);
    getchar();  // Consumir newline
    
    if (choice == 3) {
        printf("Saliendo...\n");
        return 0;
    }
    
    if (choice == 2) {
        debug_set_mode(&debug_ctrl, MODE_STEP);
        printf("\nStep-by-step mode enabled\n");
        printf("Commands: <ENTER>=next, c=continue, p=print, q=quit, h=help\n");
    } else {
        debug_set_mode(&debug_ctrl, MODE_NORMAL);
        printf("\nRunning in normal mode...\n");
    }
    
    // ... resto del main actual ...
    // (inicializar memoria, bus, caches, PEs, crear threads)
    
    Memory mem;
    mem_init(&mem);
    pthread_t mem_thread;
    pthread_create(&mem_thread, NULL, mem_thread_func, &mem);
    
    // ... etc ...
    
    // Si modo step, iniciar loop interactivo
    if (debug_ctrl.mode == MODE_STEP) {
        // Thread para manejar input del usuario
        pthread_t input_thread;
        pthread_create(&input_thread, NULL, input_handler_thread, 
                      (void*)&debug_ctrl);
    }
    
    // Crear threads de PEs (ahora verificarán el checkpoint)
    for (int i = 0; i < NUM_PES; i++) {
        pthread_create(&pe_threads[i], NULL, pe_run, &pes[i]);
    }
    
    // Esperar threads...
    for (int i = 0; i < NUM_PES; i++)
        pthread_join(pe_threads[i], NULL);
    
    // ... resto igual ...
    
    return 0;
}
```

### **6. Thread de Input (Alternativa)**

Si prefieres que el input no bloquee:

```c
void* input_handler_thread(void* arg) {
    DebugController* ctrl = (DebugController*)arg;
    
    while (ctrl->mode != MODE_QUIT) {
        // Esperar a que el simulador pause
        pthread_mutex_lock(&ctrl->lock);
        while (!ctrl->wait_for_input) {
            pthread_cond_wait(&ctrl->cond, &ctrl->lock);
        }
        pthread_mutex_unlock(&ctrl->lock);
        
        // Mostrar prompt y leer comando
        interactive_loop(ctrl, global_pes, global_caches, 
                        global_mem, global_bus);
    }
    
    return NULL;
}
```

---

## 🎨 Ejemplo de Sesión (Estilo GDB)

```bash
$ ./mp_mesi

MESI MultiProcessor Simulator
-----------------------------
Execution mode:
  1. Normal execution (fast)
  2. Step-by-step debugging
  3. Exit

Select [1-3]: 2

Step-by-step mode enabled
Commands: <ENTER>=next, c=continue, p=print, q=quit, h=help

PE0 at PC=0: LOAD R0, 100

> p regs 0
PE0 registers:
  R0: 0.00    R1: 0.00    R2: 0.00    R3: 0.00
  PC: 0       ZF: 0

> p cache 0
Cache PE0:
  Set  Way  State  Tag    LRU
  (empty - all lines in Invalid state)

> <ENTER>

[Bus] BUS_RD from PE0 addr=100
[Cache PE0] Miss, loading from memory (I->E)

PE0 at PC=1: LOAD R1, 104

> p cache 0
Cache PE0:
  Set  Way  State  Tag    LRU
    3    0    E     25     1

> p regs 0
PE0 registers:
  R0: 6.00    R1: 0.00    R2: 0.00    R3: 0.00
  PC: 1       ZF: 0

> p mem 100
Memory [100-110]:
  0x0064: 6.00 3.50 0.00 0.00 
  0x0068: 6.00 3.50 0.00 0.00 
  0x006c: 8.00 4.00 0.00 0.00 

> c 5
Continuing for 5 instructions...

PE0 at PC=6: HALT

> p bus
Bus statistics:
  Transactions: 8 total
    BUS_RD:   0
    BUS_RDX:  8
    BUS_UPGR: 0
    BUS_WB:   0
  Invalidations sent: 0
  Traffic: 256 bytes (0.25 KB)

> c

Continuing to end...
[PE0] Finished
[PE1] Finished
[PE2] Finished
[PE3] Finished

================================================================================
                         SIMULATOR STATISTICS                             
================================================================================
...
```

---

## ✅ Ventajas de Este Diseño

1. **✅ Mínimo**: Solo texto, sin librerías externas
2. **✅ Control total**: Decides cuándo pausar
3. **✅ No invasivo**: Código original casi sin cambios
4. **✅ Flexible**: Modo normal sigue siendo rápido
5. **✅ Simple**: Un solo thread extra para input

---

## 🚀 Implementación Incremental

### **Fase 1** (1 hora):
- Crear `debug_ctrl.h/c`
- Agregar checkpoint en `pe_run()`
- Agregar menú inicial en `main()`

### **Fase 2** (2 horas):
- Implementar `pe_print_state()`
- Implementar comandos básicos (ENTER, c, q)

### **Fase 3** (2 horas):
- Agregar comandos de visualización (p cache, p mem, p bus)
- Agregar comando de ayuda

### **Fase 4** (opcional):
- Agregar breakpoints por dirección
- Agregar watchpoints

---

## 📝 Notas de Implementación

### **¿Por qué no pausar con signals (SIGINT)?**
- Los threads pueden recibir la señal en cualquier momento
- Difícil sincronizar 4 PEs
- Mejor usar condición variable controlada

### **¿Qué pasa con los PEs que siguen ejecutando?**
- El checkpoint debe ser **atómico**
- Solo uno puede pedir input a la vez
- Los demás esperan su turno

### **¿Cómo evitar race conditions en el print?**
- Usar el mismo mutex del debug_ctrl
- O crear un mutex separado para stdout

---

¿Te gusta este diseño minimalista? Es muy limpio y cumple tus requisitos perfectamente. 🚀

#ifndef CONFIG_H
#define CONFIG_H

// CONFIGURACIÓN DEL SISTEMA

#define NUM_PES 4
#define SETS 16
#define WAYS 2
#define BLOCK_SIZE 4  // 4 doubles (32 bytes)
#define MEM_SIZE 512

// CONFIGURACIÓN DE VECTORES (PRODUCTO PUNTO)
#define VECTOR_SIZE 16           // Tamaño total del vector (debe ser múltiplo de NUM_PES)
#define SEGMENT_SIZE (VECTOR_SIZE / NUM_PES)  // Elementos por PE

// Direcciones de memoria para vectores
#define VECTOR_A_BASE 0          // Vector A empieza en dirección 0
#define VECTOR_B_BASE 100        // Vector B empieza en dirección 100
#define RESULTS_BASE 200         // Resultados parciales (4 bloques separados: 200, 204, 208, 212)
#define FLAGS_BASE 220           // Flags de sincronización (bloques separados: 220, 224, 228, 232)
#define CONSTANTS_BASE 232       // Constantes auxiliares (ej: -3.0 para barrier)
#define FINAL_RESULT_ADDR 216    // Resultado final del producto punto

// PROTOCOLO MESI
typedef enum { 
    M,
    E,
    S,
    I
} MESI_State;

// MACROS DE ALINEAMIENTO Y DIRECCIONAMIENTO

// Alineamiento de memoria a bloques
#define MEM_ALIGNMENT BLOCK_SIZE
#define IS_ALIGNED(addr) (((addr) % MEM_ALIGNMENT) == 0)
#define ALIGN_DOWN(addr) ((addr) - ((addr) % MEM_ALIGNMENT))
#define ALIGN_UP(addr) (IS_ALIGNED(addr) ? (addr) : ALIGN_DOWN((addr) + MEM_ALIGNMENT))

// Direccionamiento dentro del bloque
#define GET_BLOCK_BASE(addr)   (ALIGN_DOWN(addr))
#define GET_BLOCK_OFFSET(addr) ((addr) % BLOCK_SIZE)

// Validación de offset
#define IS_VALID_OFFSET(offset) ((offset) >= 0 && (offset) < BLOCK_SIZE)

// Construir dirección desde base + offset
#define MAKE_ADDRESS(base, offset) ((base) + (offset))

#endif
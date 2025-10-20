#ifndef CONFIG_H
#define CONFIG_H

#define NUM_PES 4
#define SETS 16
#define WAYS 2 // Apesar de que es parametrizable, la política de reemplazo actual es LRU entre 2 ways
#define BLOCK_SIZE 4 // 4 doubles (32 bytes)
#define MEM_SIZE 512

// Alineamiento de memoria
#define MEM_ALIGNMENT BLOCK_SIZE
#define IS_ALIGNED(addr) (((addr) % MEM_ALIGNMENT) == 0)
#define ALIGN_DOWN(addr) ((addr) - ((addr) % MEM_ALIGNMENT))
#define ALIGN_UP(addr) (IS_ALIGNED(addr) ? (addr) : ALIGN_DOWN((addr) + MEM_ALIGNMENT))

// Direccionamiento dentro del bloque
// Dirección completa = BASE + OFFSET
// Ejemplo: addr=13 → BASE=12, OFFSET=1
#define GET_BLOCK_BASE(addr)   (ALIGN_DOWN(addr))
#define GET_BLOCK_OFFSET(addr) ((addr) % BLOCK_SIZE)

// Validación de offset
#define IS_VALID_OFFSET(offset) ((offset) >= 0 && (offset) < BLOCK_SIZE)

// Construir dirección desde base + offset
#define MAKE_ADDRESS(base, offset) ((base) + (offset))

#endif
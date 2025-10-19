#ifndef CONFIG_H
#define CONFIG_H

#define NUM_PES 4
#define SETS 16
#define WAYS 2
#define BLOCK_SIZE 4 // 4 doubles (32 bytes)
#define MEM_SIZE 512

// Alineamiento de memoria
#define MEM_ALIGNMENT BLOCK_SIZE
#define IS_ALIGNED(addr) (((addr) % MEM_ALIGNMENT) == 0)
#define ALIGN_DOWN(addr) ((addr) - ((addr) % MEM_ALIGNMENT))
#define ALIGN_UP(addr) (IS_ALIGNED(addr) ? (addr) : ALIGN_DOWN((addr) + MEM_ALIGNMENT))

#endif
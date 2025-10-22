#ifndef CONFIG_H
#define CONFIG_H

// SYSTEM CONFIGURATION
#define NUM_PES 4
#define SETS 16
#define WAYS 2
#define BLOCK_SIZE 4  // 4 doubles (32 bytes)
#define MEM_SIZE 512

// ASM program paths
#define ASM_DOTPROD_PE0_PATH   "asm/dotprod_pe0.asm"
#define ASM_DOTPROD_PE1_PATH   "asm/dotprod_pe1.asm"
#define ASM_DOTPROD_PE2_PATH   "asm/dotprod_pe2.asm"
#define ASM_DOTPROD_PE3_PATH   "asm/dotprod_pe3.asm"

// VECTOR CONFIGURATION (Dot Product)
#define VECTOR_SIZE 16

// CSV input files
#define VECTOR_A_FILE          "data/vector_decimals_a_16.csv"
#define VECTOR_B_FILE          "data/vector_decimals_b_16.csv"

#define MISALIGNMENT_OFFSET    0    // Global misalignment (0 = aligned)

// Work distribution with residue: PE0-PE2 process base, PE3 (master) handles residue
#define SEGMENT_SIZE_WORKER (VECTOR_SIZE / NUM_PES)        // Base elements per worker PE
#define RESIDUE ((VECTOR_SIZE) % NUM_PES)                  // Residual elements
#define SEGMENT_SIZE_MASTER (SEGMENT_SIZE_WORKER + RESIDUE) // PE3 handles base + residue

// COHERENCE PROTOCOL OVERHEAD
#define BUS_CONTROL_SIGNAL_SIZE 12   // bytes: msg (4) + addr (4) + src_pe (4)
#define INVALIDATION_CONTROL_SIGNAL_SIZE 8 // bytes: msg (4) + addr (4)

// MEMORY LAYOUT
// Shared configuration region
#define SHARED_CONFIG_ADDR         0x0             // Inicio de configuración
#define CFG_GLOBAL_SIZE            8               // Parámetros globales (0-7)
#define CFG_PARAMS_PER_PE          2               // start_index, segment_size
#define CFG_PE_AREA_SIZE           (NUM_PES * CFG_PARAMS_PER_PE)
#define CFG_TOTAL_SIZE             (CFG_GLOBAL_SIZE + CFG_PE_AREA_SIZE)

// Global parameters (addresses 0-7)
#define CFG_VECTOR_A_ADDR          0x0             // Address where vector A starts
#define CFG_VECTOR_B_ADDR          0x1             // Address where vector B starts
#define CFG_RESULTS_ADDR           0x2             // Address for partial results
#define CFG_FLAGS_ADDR             0x3             // Address for synchronization flags
#define CFG_FINAL_RESULT_ADDR      0x4             // Address for final result
#define CFG_NUM_PES_ADDR           0x5             // Number of PEs in the system
#define CFG_BARRIER_CHECK_ADDR     0x6             // Barrier check value: -(NUM_PES-1)
#define CFG_RESERVED_ADDR          0x7             // Reserved for future use

// Per-PE configuration (starts at addr 8)
#define CFG_PE_START_ADDR          CFG_GLOBAL_SIZE
#define CFG_PE(pe_id, param_offset) (CFG_PE_START_ADDR + (pe_id) * CFG_PARAMS_PER_PE + (param_offset))

// Per-PE parameter offsets (to use with CFG_PE)
#define PE_START_INDEX             0               // Start index of the segment
#define PE_SEGMENT_SIZE            1               // Number of elements to process

// Shared space for synchronization and final results
#define SYNC_AREA_START            CFG_TOTAL_SIZE
#define RESULTS_ADDR               SYNC_AREA_START
#define FLAGS_ADDR                 (RESULTS_ADDR + NUM_PES)
#define FINAL_RESULT_ADDR          (FLAGS_ADDR + NUM_PES)

// Vectors area with optional misalignment
#define SYNC_AREA_END              (FINAL_RESULT_ADDR + 1)
#define VECTORS_START_ALIGNED      ALIGN_UP(SYNC_AREA_END)
#define VECTOR_A_ADDR              (VECTORS_START_ALIGNED + ((MISALIGNMENT_OFFSET) % BLOCK_SIZE))
#define VECTOR_B_ADDR              (VECTOR_A_ADDR + VECTOR_SIZE + ((MISALIGNMENT_OFFSET) % BLOCK_SIZE))

// PROTOCOLO MESI
typedef enum { 
    M,
    E,
    S,
    I
} MESI_State;

// ALIGNMENT AND ADDRESSING MACROS
// Memory block alignment
#define MEM_ALIGNMENT BLOCK_SIZE
#define IS_ALIGNED(addr) (((addr) % MEM_ALIGNMENT) == 0)
#define ALIGN_DOWN(addr) ((addr) - ((addr) % MEM_ALIGNMENT))
#define ALIGN_UP(addr) (IS_ALIGNED(addr) ? (addr) : ALIGN_DOWN((addr) + MEM_ALIGNMENT))

// Addressing within a block
#define GET_BLOCK_BASE(addr)   (ALIGN_DOWN(addr))
#define GET_BLOCK_OFFSET(addr) ((addr) % BLOCK_SIZE)

// Offset validation
#define IS_VALID_OFFSET(offset) ((offset) >= 0 && (offset) < BLOCK_SIZE)

// Build address from base + offset
#define MAKE_ADDRESS(base, offset) ((base) + (offset))

#endif
#ifndef CONFIG_H
#define CONFIG_H

// CONFIGURACIÓN DEL SISTEMA
#define NUM_PES 4
#define SETS 16
#define WAYS 2
#define BLOCK_SIZE 4  // 4 doubles (32 bytes)
#define MEM_SIZE 512

// CONFIGURACIÓN DE VECTORES (PRODUCTO PUNTO)
#define VECTOR_SIZE 16

// Distribución con residuo: PE0-PE2 procesan base, PE3 (master) maneja residuo
#define SEGMENT_SIZE_WORKER (VECTOR_SIZE / NUM_PES)        // Tamaño base por PE trabajador
#define RESIDUE ((VECTOR_SIZE) % NUM_PES)                   // Elementos residuales
#define SEGMENT_SIZE_MASTER (SEGMENT_SIZE_WORKER + RESIDUE) // PE3 maneja base + residuo

// Rutas de archivos CSV
#define VECTOR_A_FILE          "data/vector_decimals_a.csv"
#define VECTOR_B_FILE          "data/vector_decimals_b.csv"

#define MISALIGNMENT_OFFSET    0    // Desalineamiento global (0 = alineado)

// SEGMENTACIÓN DE MEMORIA
// Configuración de memoria compartida
#define SHARED_CONFIG_ADDR         0               // Inicio de configuración
#define CFG_GLOBAL_SIZE            8               // Parámetros globales (0-7)
#define CFG_PARAMS_PER_PE          2               // start_index, segment_size
#define CFG_PE_AREA_SIZE           (NUM_PES * CFG_PARAMS_PER_PE)
#define CFG_TOTAL_SIZE             (CFG_GLOBAL_SIZE + CFG_PE_AREA_SIZE)

// Parámetros globales del sistema (direcciones 0-7)
#define CFG_VECTOR_A_ADDR          0               // Dirección donde empieza vector A
#define CFG_VECTOR_B_ADDR          1               // Dirección donde empieza vector B
#define CFG_RESULTS_ADDR           2               // Dirección de resultados parciales
#define CFG_FLAGS_ADDR             3               // Dirección de flags de sincronización
#define CFG_FINAL_RESULT_ADDR      4               // Dirección del resultado final
#define CFG_NUM_PES_ADDR           5               // Número de PEs en el sistema
#define CFG_BARRIER_CHECK_ADDR     6               // Valor para verificar barrier: -(NUM_PES-1)
#define CFG_RESERVED_ADDR          7               // Reservado para expansión

// Configuración específica por PE (empieza en addr 8)
#define CFG_PE_START_ADDR          CFG_GLOBAL_SIZE
#define CFG_PE(pe_id, param_offset) (CFG_PE_START_ADDR + (pe_id) * CFG_PARAMS_PER_PE + (param_offset))

// Offsets de parámetros por PE (para usar con CFG_PE)
#define PE_START_INDEX             0               // Índice inicial del segmento
#define PE_SEGMENT_SIZE            1               // Cantidad de elementos a procesar

// Espacio compartido para sincronización y resultados finales
#define SYNC_AREA_START            CFG_TOTAL_SIZE
#define RESULTS_ADDR               SYNC_AREA_START
#define FLAGS_ADDR                 (RESULTS_ADDR + NUM_PES)
#define FINAL_RESULT_ADDR          (FLAGS_ADDR + NUM_PES)

// Área de vectores con desalineamiento
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
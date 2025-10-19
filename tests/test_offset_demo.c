#include <stdio.h>
#include "src/include/config.h"

int main() {
    printf("==================================================\n");
    printf("     Demostración de Direccionamiento con Offset\n");
    printf("==================================================\n\n");
    
    printf("Configuración del sistema:\n");
    printf("  BLOCK_SIZE = %d doubles (cada double = 8 bytes)\n", BLOCK_SIZE);
    printf("  Tamaño de bloque en bytes = %d bytes\n\n", BLOCK_SIZE * 8);
    
    printf("Conceptos clave:\n");
    printf("  - Un BLOQUE contiene %d doubles consecutivos\n", BLOCK_SIZE);
    printf("  - Dirección BASE: Inicio del bloque (múltiplo de %d)\n", BLOCK_SIZE);
    printf("  - OFFSET: Posición dentro del bloque (0-%d)\n", BLOCK_SIZE-1);
    printf("  - Dirección completa = BASE + OFFSET\n\n");
    
    printf("==================================================\n");
    printf("Ejemplo 1: Acceso a dirección 0\n");
    printf("==================================================\n");
    int addr1 = 0;
    printf("Dirección: %d\n", addr1);
    printf("  BASE    = GET_BLOCK_BASE(%d)   = %d\n", addr1, GET_BLOCK_BASE(addr1));
    printf("  OFFSET  = GET_BLOCK_OFFSET(%d) = %d\n", addr1, GET_BLOCK_OFFSET(addr1));
    printf("  Alineado: %s\n\n", IS_ALIGNED(addr1) ? "SI" : "NO");
    
    printf("El cache trae el BLOQUE completo [0, 1, 2, 3]\n");
    printf("Pero solo lee/escribe en la posición OFFSET=0\n\n");
    
    printf("==================================================\n");
    printf("Ejemplo 2: Acceso a dirección 13\n");
    printf("==================================================\n");
    int addr2 = 13;
    printf("Dirección: %d\n", addr2);
    printf("  BASE    = GET_BLOCK_BASE(%d)   = %d\n", addr2, GET_BLOCK_BASE(addr2));
    printf("  OFFSET  = GET_BLOCK_OFFSET(%d) = %d\n", addr2, GET_BLOCK_OFFSET(addr2));
    printf("  Alineado: %s\n\n", IS_ALIGNED(addr2) ? "SI" : "NO");
    
    printf("El cache trae el BLOQUE completo [12, 13, 14, 15]\n");
    printf("Y accede a la posición OFFSET=1 (que es addr 13)\n\n");
    
    printf("==================================================\n");
    printf("Ejemplo 3: Vector en memoria\n");
    printf("==================================================\n");
    printf("Vector: double A[8] almacenado desde addr=100\n");
    printf("Índice | Dirección | BASE | OFFSET | Bloque\n");
    printf("-------|-----------|------|--------|--------\n");
    for (int i = 0; i < 8; i++) {
        int addr = 100 + i;
        printf("  A[%d] |    %3d    | %3d  |   %d    | [%d-%d]\n",
               i, addr, GET_BLOCK_BASE(addr), GET_BLOCK_OFFSET(addr),
               GET_BLOCK_BASE(addr), GET_BLOCK_BASE(addr) + BLOCK_SIZE - 1);
    }
    
    printf("\n");
    printf("Observación:\n");
    printf("  - A[0]-A[3] están en el mismo bloque [100-103]\n");
    printf("  - A[4]-A[7] están en el mismo bloque [104-107]\n");
    printf("  - Acceder a A[0] y A[2] solo requiere 1 cache miss\n");
    printf("  - Acceder a A[0] y A[4] requiere 2 cache misses\n\n");
    
    printf("==================================================\n");
    printf("Ejemplo 4: Construcción de direcciones\n");
    printf("==================================================\n");
    int base = 100;
    printf("Base del bloque: %d\n", base);
    printf("Construir direcciones dentro del bloque:\n");
    for (int offset = 0; offset < BLOCK_SIZE; offset++) {
        int addr = MAKE_ADDRESS(base, offset);
        printf("  MAKE_ADDRESS(%d, %d) = %d\n", base, offset, addr);
    }
    
    printf("\n");
    printf("==================================================\n");
    printf("Ventajas del sistema con offset:\n");
    printf("==================================================\n");
    printf("1. LOCALIDAD ESPACIAL:\n");
    printf("   - Un bloque trae 4 doubles consecutivos\n");
    printf("   - Accesos cercanos reutilizan el mismo bloque\n\n");
    
    printf("2. EFICIENCIA:\n");
    printf("   - Reduce tráfico del bus (1 transferencia = 4 datos)\n");
    printf("   - Mejor aprovechamiento del ancho de banda\n\n");
    
    printf("3. REALISMO:\n");
    printf("   - Refleja cómo funcionan caches reales\n");
    printf("   - Line size típico: 32-128 bytes\n\n");
    
    printf("4. COHERENCIA:\n");
    printf("   - MESI se aplica al BLOQUE completo\n");
    printf("   - Simplifica el protocolo de coherencia\n\n");
    
    printf("==================================================\n");
    printf("Ejemplo práctico en código:\n");
    printf("==================================================\n");
    printf("// Leer elementos consecutivos de un array\n");
    printf("LOAD R0, 100  // Addr 100, BASE=100, OFFSET=0 → MISS, trae [100-103]\n");
    printf("LOAD R1, 101  // Addr 101, BASE=100, OFFSET=1 → HIT! Ya está en cache\n");
    printf("LOAD R2, 102  // Addr 102, BASE=100, OFFSET=2 → HIT! Ya está en cache\n");
    printf("LOAD R3, 103  // Addr 103, BASE=100, OFFSET=3 → HIT! Ya está en cache\n");
    printf("LOAD R4, 104  // Addr 104, BASE=104, OFFSET=0 → MISS, trae [104-107]\n\n");
    
    printf("Resultado: 2 misses, 3 hits (hit rate = 60%%)\n");
    printf("Sin bloques: 5 misses, 0 hits (hit rate = 0%%)\n\n");
    
    return 0;
}

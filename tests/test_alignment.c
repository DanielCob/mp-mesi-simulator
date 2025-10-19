#include <stdio.h>
#include "src/include/config.h"

int main() {
    printf("=========================================\n");
    printf("  Prueba de Alineamiento de Memoria\n");
    printf("=========================================\n\n");
    
    printf("Configuración:\n");
    printf("  BLOCK_SIZE = %d\n", BLOCK_SIZE);
    printf("  MEM_ALIGNMENT = %d\n", MEM_ALIGNMENT);
    printf("  MEM_SIZE = %d\n\n", MEM_SIZE);
    
    printf("Pruebas de alineamiento:\n");
    printf("-----------------------------------------\n");
    
    // Direcciones de prueba
    int test_addrs[] = {0, 1, 2, 3, 4, 5, 8, 11, 12, 15, 16, 20, 23, 24, 100, 103, 128};
    int n_tests = sizeof(test_addrs) / sizeof(test_addrs[0]);
    
    for (int i = 0; i < n_tests; i++) {
        int addr = test_addrs[i];
        int is_aligned = IS_ALIGNED(addr);
        
        printf("Addr %3d: %s", addr, is_aligned ? "✓ ALIGNED  " : "✗ MISALIGNED");
        
        if (!is_aligned) {
            printf(" → ALIGN_DOWN=%3d, ALIGN_UP=%3d", 
                   ALIGN_DOWN(addr), ALIGN_UP(addr));
        }
        printf("\n");
    }
    
    printf("\n");
    printf("=========================================\n");
    printf("Conclusión:\n");
    printf("  - Solo direcciones múltiplos de %d están alineadas\n", MEM_ALIGNMENT);
    printf("  - Las funciones cache_read/write y mem_read/write\n");
    printf("    verifican alineamiento y corrigen automáticamente\n");
    printf("  - Direcciones desalineadas generan warnings en stderr\n");
    printf("=========================================\n");
    
    return 0;
}

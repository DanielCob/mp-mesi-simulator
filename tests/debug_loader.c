#include "pe/loader.h"
#include <stdio.h>

int main() {
    Program* prog = load_program("asm/test_jnz.asm");
    if (!prog) {
        printf("Error cargando programa\n");
        return 1;
    }
    
    printf("\nPrograma cargado: %d instrucciones\n\n", prog->size);
    
    for (int i = 0; i < prog->size; i++) {
        Instruction* inst = &prog->code[i];
        printf("[%2d] ", i);
        
        switch (inst->op) {
            case OP_LOAD:  printf("LOAD  R%d, [%d]", inst->rd, inst->addr); break;
            case OP_STORE: printf("STORE R%d, [%d]", inst->rd, inst->addr); break;
            case OP_FADD:  printf("FADD  R%d, R%d, R%d", inst->rd, inst->ra, inst->rb); break;
            case OP_FMUL:  printf("FMUL  R%d, R%d, R%d", inst->rd, inst->ra, inst->rb); break;
            case OP_INC:   printf("INC   R%d", inst->rd); break;
            case OP_DEC:   printf("DEC   R%d", inst->rd); break;
            case OP_JNZ:   printf("JNZ   R%d, %d", inst->rd, inst->label); break;
            case OP_HALT:  printf("HALT"); break;
        }
        printf("\n");
    }
    
    free_program(prog);
    return 0;
}

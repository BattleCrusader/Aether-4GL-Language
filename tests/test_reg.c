#include "aether/asm_ir.h"
#include <stdio.h>

int main(void) {
    printf("ASM_REG_RAX=%d\n", ASM_REG_RAX);
    printf("ASM_REG_RBP=%d\n", ASM_REG_RBP);
    printf("rax name: %s\n", asm_reg_name(ASM_REG_RAX, ASM_ARCH_X86_64));
    printf("rbp name: %s\n", asm_reg_name(ASM_REG_RBP, ASM_ARCH_X86_64));
    printf("from_name('rax'): %d\n", asm_reg_from_name("rax"));
    printf("from_name('rbp'): %d\n", asm_reg_from_name("rbp"));
    printf("from_name('rbx'): %d\n", asm_reg_from_name("rbx"));
    return 0;
}

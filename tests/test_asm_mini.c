#include "aether/asm_ir.h"
#include "aether/asm_parser.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("Testing asm_parse_string...\n");
    AsmBlock block;
    int ok = asm_parse_string("mov rax, 42\n", &block, "test.asm");
    printf("ok=%d, count=%d\n", ok, block.count);
    if (block.count > 0) {
        printf("type=%d, mnemonic=%s\n", block.elements[0].type, block.elements[0].instr.mnemonic);
    }
    return 0;
}

#include "aether/asm_ir.h"
#include "aether/asm_parser.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *source = "[org 0x7C00]\n[bits 16]\n    xor ax, ax\n";
    AsmParser p;
    asm_parser_init(&p, source, strlen(source), "boot.asm");
    AsmBlock block;
    int ok = asm_parse_block(&p, &block);
    printf("ok=%d count=%d error=%s\n", ok, block.count, p.error_msg);
    for (int i = 0; i < block.count; i++) {
        AsmElement *e = &block.elements[i];
        if (e->type == ASM_ELEM_DIRECTIVE)
            printf("  [%d] DIR: %s type=%d val=%ld\n", i, e->directive.name, e->directive.type, (long)e->directive.value);
        else if (e->type == ASM_ELEM_INSTRUCTION)
            printf("  [%d] INSTR: %s\n", i, e->instr.mnemonic);
    }
    asm_block_free(&block);
    return 0;
}

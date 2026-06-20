/* ================================================================
 * test_asm.c — Multi-Target Assembler Test Suite
 *
 * Tests the NASM parser and all three backends (x86_64, ARM64, RISC-V).
 * ================================================================ */

#include "aether/asm_ir.h"
#include "aether/asm_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Forward declarations for backend constructors */
AsmBackend *asm_backend_create_x86_64(void);
AsmBackend *asm_backend_create_arm64(void);
AsmBackend *asm_backend_create_riscv64(void);

static int total_tests = 0;
static int passed_tests = 0;

#define TEST(name) do { \
    total_tests++; \
    printf("  TEST: %s ... ", name); \
} while(0)

#define PASS() do { \
    printf("PASS\n"); \
    passed_tests++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
} while(0)

/* --- Test 1: Parse a simple instruction --- */
static void test_parse_mov(void) {
    TEST("parse mov rax, 42");

    AsmBlock block;
    int ok = asm_parse_string("    mov rax, 42\n", &block, "test.asm");

    if (!ok) { FAIL("parse failed"); return; }
    if (block.count != 1) { FAIL("expected 1 element"); return; }
    if (block.elements[0].type != ASM_ELEM_INSTRUCTION) { FAIL("expected instruction"); return; }
    if (strcmp(block.elements[0].instr.mnemonic, "mov") != 0) { FAIL("expected mov"); return; }
    if (block.elements[0].instr.operand_count != 2) { FAIL("expected 2 operands"); return; }
    if (block.elements[0].instr.operands[0].type != ASM_OP_REGISTER) { FAIL("expected register"); return; }
    if (block.elements[0].instr.operands[1].type != ASM_OP_IMMEDIATE) { FAIL("expected immediate"); return; }
    if (block.elements[0].instr.operands[1].immediate != 42) { FAIL("expected 42"); return; }

    PASS();
}

/* --- Test 2: Parse memory operand --- */
static void test_parse_memory(void) {
    TEST("parse mov rax, [rbp+8]");

    AsmBlock block;
    int ok = asm_parse_string("    mov rax, [rbp+8]\n", &block, "test.asm");

    if (!ok) { FAIL("parse failed"); return; }
    if (block.count != 1) { FAIL("expected 1 element"); return; }
    if (block.elements[0].type != ASM_ELEM_INSTRUCTION) { FAIL("expected instruction"); return; }

    AsmOperand *op = &block.elements[0].instr.operands[1];
    if (op->type != ASM_OP_MEMORY) { FAIL("expected memory operand"); return; }
    if (op->mem.mode != ASM_ADDR_BASE_DISP) { FAIL("expected base+disp"); return; }
    if (op->mem.displacement != 8) { FAIL("expected disp=8"); return; }

    PASS();
}

/* --- Test 3: Parse label --- */
static void test_parse_label(void) {
    TEST("parse label");

    AsmBlock block;
    int ok = asm_parse_string("loop:\n    nop\n", &block, "test.asm");

    if (!ok) { FAIL("parse failed"); return; }
    if (block.count != 2) { FAIL("expected 2 elements"); return; }
    if (block.elements[0].type != ASM_ELEM_LABEL) { FAIL("expected label"); return; }
    if (strcmp(block.elements[0].label.name, "loop") != 0) { FAIL("expected 'loop'"); return; }

    PASS();
}

/* --- Test 4: Parse directive --- */
static void test_parse_directive(void) {
    TEST("parse section directive");

    AsmBlock block;
    int ok = asm_parse_string("section .text\n", &block, "test.asm");

    if (!ok) { FAIL("parse failed"); return; }
    if (block.count != 1) { FAIL("expected 1 element"); return; }
    if (block.elements[0].type != ASM_ELEM_DIRECTIVE) { FAIL("expected directive"); return; }
    if (block.elements[0].directive.type != ASM_DIR_SECTION) { FAIL("expected section"); return; }

    PASS();
}

/* --- Test 5: x86_64 backend round-trip --- */
static void test_x86_64_roundtrip(void) {
    TEST("x86_64 backend round-trip");

    const char *source = "    mov rax, 42\n"
                         "    add rax, rbx\n"
                         "    mov [rbp-8], rax\n"
                         "    ret\n";

    AsmBlock block;
    int ok = asm_parse_string(source, &block, "test.asm");
    if (!ok) { FAIL("parse failed"); return; }

    AsmBackend *backend = asm_backend_create_x86_64();
    if (!backend) { FAIL("backend creation failed"); return; }

    size_t out_len;
    char *output = backend->emit(&block, &out_len);
    if (!output) { FAIL("emit failed"); return; }

    /* Check output contains expected instructions */
    if (!strstr(output, "mov rax, 42")) { FAIL("missing 'mov rax, 42'"); backend->destroy(output); return; }
    if (!strstr(output, "add rax, rbx")) { FAIL("missing 'add rax, rbx'"); backend->destroy(output); return; }
    if (!strstr(output, "mov [rbp-8], rax")) { FAIL("missing 'mov [rbp-8], rax'"); backend->destroy(output); return; }
    if (!strstr(output, "ret")) { FAIL("missing 'ret'"); backend->destroy(output); return; }

    backend->destroy(output);
    PASS();
}

/* --- Test 6: ARM64 backend --- */
static void test_arm64_output(void) {
    TEST("ARM64 backend");

    const char *source = "    mov rax, 42\n"
                         "    add rax, rbx\n"
                         "    ret\n";

    AsmBlock block;
    int ok = asm_parse_string(source, &block, "test.asm");
    if (!ok) { FAIL("parse failed"); return; }

    AsmBackend *backend = asm_backend_create_arm64();
    if (!backend) { FAIL("backend creation failed"); return; }

    size_t out_len;
    char *output = backend->emit(&block, &out_len);
    if (!output) { FAIL("emit failed"); return; }

    if (!strstr(output, "mov x0, #42")) { FAIL("missing 'mov x0, #42'"); backend->destroy(output); return; }
    if (!strstr(output, "add x0, x19")) { FAIL("missing 'add x0, x19'"); backend->destroy(output); return; }
    if (!strstr(output, "ret")) { FAIL("missing 'ret'"); backend->destroy(output); return; }

    backend->destroy(output);
    PASS();
}

/* --- Test 7: RISC-V backend --- */
static void test_riscv64_output(void) {
    TEST("RISC-V backend");

    const char *source = "    mov rax, 42\n"
                         "    add rax, rbx\n"
                         "    ret\n";

    AsmBlock block;
    int ok = asm_parse_string(source, &block, "test.asm");
    if (!ok) { FAIL("parse failed"); return; }

    AsmBackend *backend = asm_backend_create_riscv64();
    if (!backend) { FAIL("backend creation failed"); return; }

    size_t out_len;
    char *output = backend->emit(&block, &out_len);
    if (!output) { FAIL("emit failed"); return; }

    if (!strstr(output, "mv a0, 42")) { FAIL("missing 'mv a0, 42'"); backend->destroy(output); return; }
    if (!strstr(output, "add a0, s1")) { FAIL("missing 'add a0, s1'"); backend->destroy(output); return; }
    if (!strstr(output, "ret")) { FAIL("missing 'ret'"); backend->destroy(output); return; }

    backend->destroy(output);
    PASS();
}

/* --- Test 8: Full boot sector round-trip --- */
static void test_bootsector(void) {
    TEST("boot sector round-trip");

    const char *source =
        "    xor ax, ax\n"
        "    mov ds, ax\n"
        "    mov si, msg\n"
        "    mov ah, 0x0E\n"
        ".loop:\n"
        "    lodsb\n"
        "    or al, al\n"
        "    jz .done\n"
        "    int 0x10\n"
        "    jmp .loop\n"
        ".done:\n"
        "    hlt\n"
        "msg:\n"
        "    db \"Hello\", 0\n"
        "    dw 0xAA55\n";

    AsmBlock block;
    int ok = asm_parse_string(source, &block, "boot.asm");
    if (!ok) { FAIL("parse failed"); return; }

    AsmBackend *backend = asm_backend_create_x86_64();
    size_t out_len;
    char *output = backend->emit(&block, &out_len);
    if (!output) { FAIL("emit failed"); return; }

    if (!strstr(output, "0xAA55")) { FAIL("missing boot sig"); backend->destroy(output); return; }

    backend->destroy(output);
    PASS();
}

/* --- Test 9: Complex addressing modes --- */
static void test_addressing_modes(void) {
    TEST("complex addressing modes");

    const char *source =
        "    mov rax, [rbx]\n"
        "    mov rax, [rbx+rcx*8]\n"
        "    mov rax, [rbx+rcx*4+16]\n"
        "    mov rax, [rel message]\n";

    AsmBlock block;
    int ok = asm_parse_string(source, &block, "test.asm");
    if (!ok) { FAIL("parse failed"); return; }

    if (block.count != 4) { FAIL("expected 4 instructions"); return; }

    /* Check addressing modes */
    if (block.elements[0].instr.operands[1].mem.mode != ASM_ADDR_BASE) { FAIL("expected base mode"); return; }
    if (block.elements[1].instr.operands[1].mem.mode != ASM_ADDR_BASE_INDEX) { FAIL("expected base+index"); return; }
    if (block.elements[2].instr.operands[1].mem.mode != ASM_ADDR_BASE_INDEX_DISP) { FAIL("expected base+index+disp"); return; }
    if (block.elements[3].instr.operands[1].mem.mode != ASM_ADDR_RIP_REL) { FAIL("expected rip-rel"); return; }

    PASS();
}

/* --- Test 10: Data directives --- */
static void test_data_directives(void) {
    TEST("data directives");

    const char *source =
        "    db 0x48, 0x65, 0x6C\n"
        "    dw 0xAA55\n"
        "    dd 42\n"
        "    dq 0xDEADBEEF\n"
        "    resb 64\n";

    AsmBlock block;
    int ok = asm_parse_string(source, &block, "test.asm");
    if (!ok) { FAIL("parse failed"); return; }

    if (block.count != 5) { FAIL("expected 5 elements"); return; }
    if (block.elements[0].directive.type != ASM_DIR_DB) { FAIL("expected db"); return; }
    if (block.elements[1].directive.type != ASM_DIR_DW) { FAIL("expected dw"); return; }
    if (block.elements[2].directive.type != ASM_DIR_DD) { FAIL("expected dd"); return; }
    if (block.elements[3].directive.type != ASM_DIR_DQ) { FAIL("expected dq"); return; }
    if (block.elements[4].directive.type != ASM_DIR_RESB) { FAIL("expected resb"); return; }
    if (block.elements[4].directive.value != 64) { FAIL("expected 64"); return; }

    PASS();
}

/* --- Test 11: ARM64 push/pop expansion --- */
static void test_arm64_push_pop(void) {
    TEST("ARM64 push/pop expansion");

    const char *source = "    push rax\n    pop rbx\n";

    AsmBlock block;
    int ok = asm_parse_string(source, &block, "test.asm");
    if (!ok) { FAIL("parse failed"); return; }

    AsmBackend *backend = asm_backend_create_arm64();
    size_t out_len;
    char *output = backend->emit(&block, &out_len);
    if (!output) { FAIL("emit failed"); return; }

    if (!strstr(output, "stp x0, [sp, #-16]!")) { FAIL("missing push expansion"); backend->destroy(output); return; }
    if (!strstr(output, "ldp x19, [sp], #16")) { FAIL("missing pop expansion"); backend->destroy(output); return; }

    backend->destroy(output);
    PASS();
}

/* --- Test 12: RISC-V push/pop expansion --- */
static void test_riscv_push_pop(void) {
    TEST("RISC-V push/pop expansion");

    const char *source = "    push rax\n    pop rbx\n";

    AsmBlock block;
    int ok = asm_parse_string(source, &block, "test.asm");
    if (!ok) { FAIL("parse failed"); return; }

    AsmBackend *backend = asm_backend_create_riscv64();
    size_t out_len;
    char *output = backend->emit(&block, &out_len);
    if (!output) { FAIL("emit failed"); return; }

    if (!strstr(output, "sd a0, -16(sp)")) { FAIL("missing push"); backend->destroy(output); return; }
    if (!strstr(output, "ld s1, 0(sp)")) { FAIL("missing pop"); backend->destroy(output); return; }

    backend->destroy(output);
    PASS();
}

/* --- Test 13: Conditional jump translation --- */
static void test_cond_jumps(void) {
    TEST("conditional jump translation");

    const char *source = "    jz .done\n    jnz .loop\n    jg .greater\n";

    AsmBlock block;
    int ok = asm_parse_string(source, &block, "test.asm");
    if (!ok) { FAIL("parse failed"); return; }

    /* ARM64 */
    AsmBackend *arm = asm_backend_create_arm64();
    size_t arm_len;
    char *arm_out = arm->emit(&block, &arm_len);
    if (!arm_out) { FAIL("ARM64 emit failed"); return; }
    if (!strstr(arm_out, "b.eq .done")) { FAIL("ARM64 missing b.eq"); arm->destroy(arm_out); return; }
    if (!strstr(arm_out, "b.ne .loop")) { FAIL("ARM64 missing b.ne"); arm->destroy(arm_out); return; }
    if (!strstr(arm_out, "b.gt .greater")) { FAIL("ARM64 missing b.gt"); arm->destroy(arm_out); return; }
    arm->destroy(arm_out);

    /* RISC-V */
    AsmBackend *rv = asm_backend_create_riscv64();
    size_t rv_len;
    char *rv_out = rv->emit(&block, &rv_len);
    if (!rv_out) { FAIL("RISC-V emit failed"); return; }
    if (!strstr(rv_out, "beqz .done")) { FAIL("RISC-V missing beqz"); rv->destroy(rv_out); return; }
    if (!strstr(rv_out, "bnez .loop")) { FAIL("RISC-V missing bnez"); rv->destroy(rv_out); return; }
    rv->destroy(rv_out);

    PASS();
}

int main(void) {
    printf("=== Multi-Target Assembler Test Suite ===\n\n");

    test_parse_mov();
    test_parse_memory();
    test_parse_label();
    test_parse_directive();
    test_x86_64_roundtrip();
    test_arm64_output();
    test_riscv64_output();
    test_bootsector();
    test_addressing_modes();
    test_data_directives();
    test_arm64_push_pop();
    test_riscv_push_pop();
    test_cond_jumps();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           passed_tests, total_tests, total_tests - passed_tests);

    return (passed_tests == total_tests) ? 0 : 1;
}

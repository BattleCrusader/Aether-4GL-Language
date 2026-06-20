/* ================================================================
 * asm_ir.c — Aether Multi-Target Assembler IR implementation
 *
 * Register table, block construction helpers, and backend factory.
 * ================================================================ */

#include "aether/asm_ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* --- Global register translation table --- */
const AsmRegEntry asm_reg_table[ASM_REG_COUNT] = {
    /* x86_64 GPRs */
    {ASM_REG_RAX,  "rax",  "x0",  "a0",  0, 64},
    {ASM_REG_RBX,  "rbx",  "x19", "s1",  1, 64},
    {ASM_REG_RCX,  "rcx",  "x1",  "a1",  0, 64},
    {ASM_REG_RDX,  "rdx",  "x2",  "a2",  0, 64},
    {ASM_REG_RSI,  "rsi",  "x3",  "a3",  0, 64},
    {ASM_REG_RDI,  "rdi",  "x4",  "a4",  0, 64},
    {ASM_REG_RBP,  "rbp",  "x29", "s0",  1, 64},
    {ASM_REG_RSP,  "rsp",  "sp",  "sp",  1, 64},
    {ASM_REG_R8,   "r8",   "x5",  "a5",  0, 64},
    {ASM_REG_R9,   "r9",   "x6",  "a6",  0, 64},
    {ASM_REG_R10,  "r10",  "x7",  "a7",  0, 64},
    {ASM_REG_R11,  "r11",  "x8",  "s2",  0, 64},
    {ASM_REG_R12,  "r12",  "x9",  "s3",  1, 64},
    {ASM_REG_R13,  "r13",  "x10", "s4",  1, 64},
    {ASM_REG_R14,  "r14",  "x11", "s5",  1, 64},
    {ASM_REG_R15,  "r15",  "x12", "s6",  1, 64},

    /* 32-bit sub-registers */
    {ASM_REG_EAX,  "eax",  "w0",  "a0",  0, 32},
    {ASM_REG_EBX,  "ebx",  "w19", "s1",  1, 32},
    {ASM_REG_ECX,  "ecx",  "w1",  "a1",  0, 32},
    {ASM_REG_EDX,  "edx",  "w2",  "a2",  0, 32},
    {ASM_REG_ESI,  "esi",  "w3",  "a3",  0, 32},
    {ASM_REG_EDI,  "edi",  "w4",  "a4",  0, 32},
    {ASM_REG_EBP,  "ebp",  "w29", "s0",  1, 32},
    {ASM_REG_ESP,  "esp",  "wsp", "sp",  1, 32},
    {ASM_REG_R8D,  "r8d",  "w5",  "a5",  0, 32},
    {ASM_REG_R9D,  "r9d",  "w6",  "a6",  0, 32},
    {ASM_REG_R10D, "r10d", "w7",  "a7",  0, 32},
    {ASM_REG_R11D, "r11d", "w8",  "s2",  0, 32},
    {ASM_REG_R12D, "r12d", "w9",  "s3",  1, 32},
    {ASM_REG_R13D, "r13d", "w10", "s4",  1, 32},
    {ASM_REG_R14D, "r14d", "w11", "s5",  1, 32},
    {ASM_REG_R15D, "r15d", "w12", "s6",  1, 32},

    /* 16-bit sub-registers */
    {ASM_REG_AX,   "ax",   NULL,  NULL,  0, 16},
    {ASM_REG_BX,   "bx",   NULL,  NULL,  1, 16},
    {ASM_REG_CX,   "cx",   NULL,  NULL,  0, 16},
    {ASM_REG_DX,   "dx",   NULL,  NULL,  0, 16},
    {ASM_REG_SI,   "si",   NULL,  NULL,  0, 16},
    {ASM_REG_DI,   "di",   NULL,  NULL,  0, 16},
    {ASM_REG_BP,   "bp",   NULL,  NULL,  1, 16},
    {ASM_REG_SP,   "sp",   NULL,  NULL,  1, 16},
    {ASM_REG_R8W,  "r8w",  NULL,  NULL,  0, 16},
    {ASM_REG_R9W,  "r9w",  NULL,  NULL,  0, 16},
    {ASM_REG_R10W, "r10w", NULL,  NULL,  0, 16},
    {ASM_REG_R11W, "r11w", NULL,  NULL,  0, 16},
    {ASM_REG_R12W, "r12w", NULL,  NULL,  1, 16},
    {ASM_REG_R13W, "r13w", NULL,  NULL,  1, 16},
    {ASM_REG_R14W, "r14w", NULL,  NULL,  1, 16},
    {ASM_REG_R15W, "r15w", NULL,  NULL,  1, 16},

    /* 8-bit sub-registers */
    {ASM_REG_AL,   "al",   NULL,  NULL,  0, 8},
    {ASM_REG_BL,   "bl",   NULL,  NULL,  1, 8},
    {ASM_REG_CL,   "cl",   NULL,  NULL,  0, 8},
    {ASM_REG_DL,   "dl",   NULL,  NULL,  0, 8},
    {ASM_REG_AH,   "ah",   NULL,  NULL,  0, 8},
    {ASM_REG_BH,   "bh",   NULL,  NULL,  1, 8},
    {ASM_REG_CH,   "ch",   NULL,  NULL,  0, 8},
    {ASM_REG_DH,   "dh",   NULL,  NULL,  0, 8},
    {ASM_REG_SIL,  "sil",  NULL,  NULL,  0, 8},
    {ASM_REG_DIL,  "dil",  NULL,  NULL,  0, 8},
    {ASM_REG_BPL,  "bpl",  NULL,  NULL,  1, 8},
    {ASM_REG_SPL,  "spl",  NULL,  NULL,  1, 8},
    {ASM_REG_R8B,  "r8b",  NULL,  NULL,  0, 8},
    {ASM_REG_R9B,  "r9b",  NULL,  NULL,  0, 8},
    {ASM_REG_R10B, "r10b", NULL,  NULL,  0, 8},
    {ASM_REG_R11B, "r11b", NULL,  NULL,  0, 8},
    {ASM_REG_R12B, "r12b", NULL,  NULL,  1, 8},
    {ASM_REG_R13B, "r13b", NULL,  NULL,  1, 8},
    {ASM_REG_R14B, "r14b", NULL,  NULL,  1, 8},
    {ASM_REG_R15B, "r15b", NULL,  NULL,  1, 8},

    /* SIMD */
    {ASM_REG_XMM0,  "xmm0",  "v0",  NULL,  0, 128},
    {ASM_REG_XMM1,  "xmm1",  "v1",  NULL,  0, 128},
    {ASM_REG_XMM2,  "xmm2",  "v2",  NULL,  0, 128},
    {ASM_REG_XMM3,  "xmm3",  "v3",  NULL,  0, 128},
    {ASM_REG_XMM4,  "xmm4",  "v4",  NULL,  0, 128},
    {ASM_REG_XMM5,  "xmm5",  "v5",  NULL,  0, 128},
    {ASM_REG_XMM6,  "xmm6",  "v6",  NULL,  0, 128},
    {ASM_REG_XMM7,  "xmm7",  "v7",  NULL,  0, 128},
    {ASM_REG_XMM8,  "xmm8",  "v8",  NULL,  0, 128},
    {ASM_REG_XMM9,  "xmm9",  "v9",  NULL,  0, 128},
    {ASM_REG_XMM10, "xmm10", "v10", NULL,  0, 128},
    {ASM_REG_XMM11, "xmm11", "v11", NULL,  0, 128},
    {ASM_REG_XMM12, "xmm12", "v12", NULL,  0, 128},
    {ASM_REG_XMM13, "xmm13", "v13", NULL,  0, 128},
    {ASM_REG_XMM14, "xmm14", "v14", NULL,  0, 128},
    {ASM_REG_XMM15, "xmm15", "v15", NULL,  0, 128},

    /* Segment registers */
    {ASM_REG_CS, "cs", NULL, NULL, 0, 16},
    {ASM_REG_DS, "ds", NULL, NULL, 0, 16},
    {ASM_REG_ES, "es", NULL, NULL, 0, 16},
    {ASM_REG_FS, "fs", NULL, NULL, 0, 16},
    {ASM_REG_GS, "gs", NULL, NULL, 0, 16},
    {ASM_REG_SS, "ss", NULL, NULL, 0, 16},

    /* ARM64 */
    {ASM_REG_X0,  NULL, "x0",  NULL, 0, 64},
    {ASM_REG_X1,  NULL, "x1",  NULL, 0, 64},
    {ASM_REG_X2,  NULL, "x2",  NULL, 0, 64},
    {ASM_REG_X3,  NULL, "x3",  NULL, 0, 64},
    {ASM_REG_X4,  NULL, "x4",  NULL, 0, 64},
    {ASM_REG_X5,  NULL, "x5",  NULL, 0, 64},
    {ASM_REG_X6,  NULL, "x6",  NULL, 0, 64},
    {ASM_REG_X7,  NULL, "x7",  NULL, 0, 64},
    {ASM_REG_X8,  NULL, "x8",  NULL, 0, 64},
    {ASM_REG_X9,  NULL, "x9",  NULL, 0, 64},
    {ASM_REG_X10, NULL, "x10", NULL, 0, 64},
    {ASM_REG_X11, NULL, "x11", NULL, 0, 64},
    {ASM_REG_X12, NULL, "x12", NULL, 0, 64},
    {ASM_REG_X13, NULL, "x13", NULL, 0, 64},
    {ASM_REG_X14, NULL, "x14", NULL, 0, 64},
    {ASM_REG_X15, NULL, "x15", NULL, 0, 64},
    {ASM_REG_X16, NULL, "x16", NULL, 0, 64},
    {ASM_REG_X17, NULL, "x17", NULL, 0, 64},
    {ASM_REG_X18, NULL, "x18", NULL, 0, 64},
    {ASM_REG_X19, NULL, "x19", NULL, 1, 64},
    {ASM_REG_X20, NULL, "x20", NULL, 1, 64},
    {ASM_REG_X21, NULL, "x21", NULL, 1, 64},
    {ASM_REG_X22, NULL, "x22", NULL, 1, 64},
    {ASM_REG_X23, NULL, "x23", NULL, 1, 64},
    {ASM_REG_X24, NULL, "x24", NULL, 1, 64},
    {ASM_REG_X25, NULL, "x25", NULL, 1, 64},
    {ASM_REG_X26, NULL, "x26", NULL, 1, 64},
    {ASM_REG_X27, NULL, "x27", NULL, 1, 64},
    {ASM_REG_X28, NULL, "x28", NULL, 1, 64},
    {ASM_REG_X29, NULL, "x29", NULL, 1, 64},
    {ASM_REG_X30, NULL, "x30", NULL, 0, 64},
    {ASM_REG_XZR, NULL, "xzr", NULL, 0, 64},

    /* RISC-V */
    {ASM_REG_RV_ZERO, NULL, NULL, "zero", 0, 64},
    {ASM_REG_RV_RA,   NULL, NULL, "ra",   0, 64},
    {ASM_REG_RV_SP,   NULL, NULL, "sp",   1, 64},
    {ASM_REG_RV_GP,   NULL, NULL, "gp",   0, 64},
    {ASM_REG_RV_TP,   NULL, NULL, "tp",   0, 64},
    {ASM_REG_RV_T0,   NULL, NULL, "t0",   0, 64},
    {ASM_REG_RV_T1,   NULL, NULL, "t1",   0, 64},
    {ASM_REG_RV_T2,   NULL, NULL, "t2",   0, 64},
    {ASM_REG_RV_S0,   NULL, NULL, "s0",   1, 64},
    {ASM_REG_RV_S1,   NULL, NULL, "s1",   1, 64},
    {ASM_REG_RV_S2,   NULL, NULL, "s2",   1, 64},
    {ASM_REG_RV_S3,   NULL, NULL, "s3",   1, 64},
    {ASM_REG_RV_S4,   NULL, NULL, "s4",   1, 64},
    {ASM_REG_RV_S5,   NULL, NULL, "s5",   1, 64},
    {ASM_REG_RV_S6,   NULL, NULL, "s6",   1, 64},
    {ASM_REG_RV_S7,   NULL, NULL, "s7",   1, 64},
    {ASM_REG_RV_S8,   NULL, NULL, "s8",   1, 64},
    {ASM_REG_RV_S9,   NULL, NULL, "s9",   1, 64},
    {ASM_REG_RV_S10,  NULL, NULL, "s10",  1, 64},
    {ASM_REG_RV_S11,  NULL, NULL, "s11",  1, 64},
    {ASM_REG_RV_A0,   NULL, NULL, "a0",   0, 64},
    {ASM_REG_RV_A1,   NULL, NULL, "a1",   0, 64},
    {ASM_REG_RV_A2,   NULL, NULL, "a2",   0, 64},
    {ASM_REG_RV_A3,   NULL, NULL, "a3",   0, 64},
    {ASM_REG_RV_A4,   NULL, NULL, "a4",   0, 64},
    {ASM_REG_RV_A5,   NULL, NULL, "a5",   0, 64},
    {ASM_REG_RV_A6,   NULL, NULL, "a6",   0, 64},
    {ASM_REG_RV_A7,   NULL, NULL, "a7",   0, 64},
    {ASM_REG_RV_T3,   NULL, NULL, "t3",   0, 64},
    {ASM_REG_RV_T4,   NULL, NULL, "t4",   0, 64},
    {ASM_REG_RV_T5,   NULL, NULL, "t5",   0, 64},
    {ASM_REG_RV_T6,   NULL, NULL, "t6",   0, 64},
};

/* --- Helper: get register name for a given architecture --- */
const char *asm_reg_name(AsmRegister reg, AsmArch arch) {
    if (reg < 0 || reg >= ASM_REG_COUNT) return NULL;
    const AsmRegEntry *e = &asm_reg_table[reg];
    switch (arch) {
        case ASM_ARCH_X86_64:  return e->x86_64_name;
        case ASM_ARCH_ARM64:   return e->arm64_name;
        case ASM_ARCH_RISCV64: return e->riscv64_name;
    }
    return NULL;
}

/* --- Helper: look up register by name (NASM syntax) --- */
AsmRegister asm_reg_from_name(const char *name) {
    if (!name) return ASM_REG_COUNT;
    for (int i = 0; i < ASM_REG_COUNT; i++) {
        if (asm_reg_table[i].x86_64_name &&
            strcmp(asm_reg_table[i].x86_64_name, name) == 0)
            return (AsmRegister)i;
    }
    return ASM_REG_COUNT;
}

/* --- Helper: get register width in bits --- */
int asm_reg_width(AsmRegister reg) {
    if (reg < 0 || reg >= ASM_REG_COUNT) return 0;
    return asm_reg_table[reg].width_bits;
}

/* --- Helper: is register callee-saved? --- */
int asm_reg_is_callee_saved(AsmRegister reg) {
    if (reg < 0 || reg >= ASM_REG_COUNT) return 0;
    return asm_reg_table[reg].is_callee_saved;
}

/* --- Block construction --- */
void asm_block_init(AsmBlock *block) {
    memset(block, 0, sizeof(AsmBlock));
}

int asm_block_add_instr(AsmBlock *block, const AsmInstruction *instr) {
    if (block->count >= ASM_MAX_INSTRUCTIONS) return -1;
    block->elements[block->count].type = ASM_ELEM_INSTRUCTION;
    block->elements[block->count].instr = *instr;
    block->count++;
    return 0;
}

int asm_block_add_directive(AsmBlock *block, const AsmDirective *dir) {
    if (block->count >= ASM_MAX_INSTRUCTIONS) return -1;
    block->elements[block->count].type = ASM_ELEM_DIRECTIVE;
    block->elements[block->count].directive = *dir;
    block->count++;
    return 0;
}

int asm_block_add_label(AsmBlock *block, const char *name, int is_global) {
    if (block->count >= ASM_MAX_INSTRUCTIONS) return -1;
    AsmElement *e = &block->elements[block->count];
    e->type = ASM_ELEM_LABEL;
    e->label.is_global = is_global;
    e->label.is_local = (name[0] == '.');
    strncpy(e->label.name, name, ASM_MAX_LABEL_LEN - 1);
    block->count++;
    return 0;
}

int asm_block_add_comment(AsmBlock *block, const char *text) {
    if (block->count >= ASM_MAX_INSTRUCTIONS) return -1;
    AsmElement *e = &block->elements[block->count];
    e->type = ASM_ELEM_COMMENT;
    strncpy(e->comment, text, 255);
    block->count++;
    return 0;
}

/* --- Instruction construction --- */
void asm_instr_init(AsmInstruction *instr, const char *mnemonic) {
    memset(instr, 0, sizeof(AsmInstruction));
    strncpy(instr->mnemonic, mnemonic, ASM_MAX_MNEMONIC_LEN - 1);
}

void asm_instr_add_operand(AsmInstruction *instr, const AsmOperand *op) {
    if (instr->operand_count >= ASM_MAX_OPERANDS) return;
    instr->operands[instr->operand_count++] = *op;
}

/* --- Operand construction --- */
void asm_op_reg(AsmOperand *op, AsmRegister reg) {
    memset(op, 0, sizeof(AsmOperand));
    op->type = ASM_OP_REGISTER;
    op->reg = reg;
}

void asm_op_imm(AsmOperand *op, int64_t value) {
    memset(op, 0, sizeof(AsmOperand));
    op->type = ASM_OP_IMMEDIATE;
    op->immediate = value;
}

void asm_op_label(AsmOperand *op, const char *label) {
    memset(op, 0, sizeof(AsmOperand));
    op->type = ASM_OP_LABEL;
    strncpy(op->label, label, ASM_MAX_LABEL_LEN - 1);
}

void asm_op_mem(AsmOperand *op, const AsmMem *mem) {
    memset(op, 0, sizeof(AsmOperand));
    op->type = ASM_OP_MEMORY;
    op->mem = *mem;
}

void asm_op_expr(AsmOperand *op, const char *expr) {
    memset(op, 0, sizeof(AsmOperand));
    op->type = ASM_OP_EXPR;
    strncpy(op->expr, expr, ASM_MAX_LABEL_LEN - 1);
}

/* --- Memory operand construction --- */
void asm_mem_init(AsmMem *mem) {
    memset(mem, 0, sizeof(AsmMem));
    mem->base = ASM_REG_COUNT;
    mem->index = ASM_REG_COUNT;
    mem->scale = 1;
}

void asm_mem_base(AsmMem *mem, AsmRegister base) {
    asm_mem_init(mem);
    mem->mode = ASM_ADDR_BASE;
    mem->base = base;
}

void asm_mem_base_disp(AsmMem *mem, AsmRegister base, int64_t disp) {
    asm_mem_init(mem);
    mem->mode = ASM_ADDR_BASE_DISP;
    mem->base = base;
    mem->displacement = disp;
}

void asm_mem_base_index(AsmMem *mem, AsmRegister base, AsmRegister index, int scale) {
    asm_mem_init(mem);
    mem->mode = ASM_ADDR_BASE_INDEX;
    mem->base = base;
    mem->index = index;
    mem->scale = scale;
}

void asm_mem_full(AsmMem *mem, AsmRegister base, AsmRegister index, int scale, int64_t disp) {
    asm_mem_init(mem);
    mem->mode = ASM_ADDR_BASE_INDEX_DISP;
    mem->base = base;
    mem->index = index;
    mem->scale = scale;
    mem->displacement = disp;
}

void asm_mem_rip_rel(AsmMem *mem, const char *label) {
    asm_mem_init(mem);
    mem->mode = ASM_ADDR_RIP_REL;
    strncpy(mem->label, label, ASM_MAX_LABEL_LEN - 1);
}

/* --- Backend creation (stubs — backends implemented separately) --- */
AsmBackend *asm_backend_create(AsmArch arch) {
    AsmBackend *b = (AsmBackend *)calloc(1, sizeof(AsmBackend));
    if (!b) return NULL;
    b->arch = arch;
    b->emit = NULL;  /* set by backend implementations */
    b->destroy = NULL;
    return b;
}

void asm_backend_free(AsmBackend *backend) {
    if (backend) {
        if (backend->destroy) backend->destroy(NULL);
        free(backend);
    }
}

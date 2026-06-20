/* ================================================================
 * asm_backend_riscv64.c — RISC-V 64 Assembly Backend
 * ================================================================ */

#include "aether/asm_ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

typedef struct { char *buf; size_t len; size_t cap; } Str;
static void str_init(Str *s) { s->buf = NULL; s->len = 0; s->cap = 0; }
static void str_app(Str *s, const char *text) {
    size_t tlen = strlen(text);
    if (s->len + tlen + 1 > s->cap) {
        s->cap = s->cap ? s->cap * 2 : 1024;
        while (s->len + tlen + 1 > s->cap) s->cap *= 2;
        s->buf = realloc(s->buf, s->cap);
    }
    memcpy(s->buf + s->len, text, tlen);
    s->len += tlen; s->buf[s->len] = '\0';
}
static void str_fmt(Str *s, const char *fmt, ...) {
    char tmp[256]; va_list args; va_start(args, fmt); vsnprintf(tmp, sizeof(tmp), fmt, args); va_end(args); str_app(s, tmp);
}

static const char *map_reg(AsmRegister reg) {
    const char *name = asm_reg_name(reg, ASM_ARCH_RISCV64);
    return name ? name : "a0";
}

static void emit_mem(Str *s, const AsmMem *mem) {
    switch (mem->mode) {
        case ASM_ADDR_BASE: str_fmt(s, "(%s)", map_reg(mem->base)); break;
        case ASM_ADDR_BASE_DISP: str_fmt(s, "%ld(%s)", (long)mem->displacement, map_reg(mem->base)); break;
        case ASM_ADDR_BASE_INDEX: str_fmt(s, "(%s)", map_reg(mem->base)); break;
        case ASM_ADDR_BASE_INDEX_DISP: str_fmt(s, "%ld(%s)", (long)mem->displacement, map_reg(mem->base)); break;
        case ASM_ADDR_RIP_REL: str_fmt(s, "(%s)", mem->label); break;
        case ASM_ADDR_DIRECT:
            if (mem->label[0]) str_fmt(s, "(%s)", mem->label);
            else str_fmt(s, "(%ld)", (long)mem->displacement);
            break;
        default: str_app(s, "(a0)"); break;
    }
}

static void emit_op(Str *s, const AsmOperand *op) {
    switch (op->type) {
        case ASM_OP_REGISTER: str_app(s, map_reg(op->reg)); break;
        case ASM_OP_IMMEDIATE: str_fmt(s, "%ld", (long)op->immediate); break;
        case ASM_OP_MEMORY: emit_mem(s, &op->mem); break;
        case ASM_OP_LABEL: str_app(s, op->label); break;
        case ASM_OP_EXPR: str_app(s, op->expr); break;
    }
}

static const char *map_mnemonic(const char *x86) {
    if (strcmp(x86, "mov") == 0) return "mv";
    if (strcmp(x86, "add") == 0) return "add";
    if (strcmp(x86, "sub") == 0) return "sub";
    if (strcmp(x86, "mul") == 0) return "mul";
    if (strcmp(x86, "div") == 0) return "divu";
    if (strcmp(x86, "and") == 0) return "and";
    if (strcmp(x86, "or") == 0) return "or";
    if (strcmp(x86, "xor") == 0) return "xor";
    if (strcmp(x86, "not") == 0) return "not";
    if (strcmp(x86, "neg") == 0) return "neg";
    if (strcmp(x86, "cmp") == 0) return "sub";
    if (strcmp(x86, "ret") == 0) return "ret";
    if (strcmp(x86, "call") == 0) return "jal";
    if (strcmp(x86, "jmp") == 0) return "j";
    if (strcmp(x86, "nop") == 0) return "nop";
    if (strcmp(x86, "int") == 0) return "ecall";
    if (strcmp(x86, "syscall") == 0) return "ecall";
    if (strcmp(x86, "push") == 0) return "sd";
    if (strcmp(x86, "pop") == 0) return "ld";
    if (strcmp(x86, "jz") == 0 || strcmp(x86, "je") == 0) return "beqz";
    if (strcmp(x86, "jnz") == 0 || strcmp(x86, "jne") == 0) return "bnez";
    if (strcmp(x86, "jg") == 0) return "bgt";
    if (strcmp(x86, "jge") == 0) return "bge";
    if (strcmp(x86, "jl") == 0) return "blt";
    if (strcmp(x86, "jle") == 0) return "ble";
    return x86;
}

static void emit_instr(Str *s, const AsmInstruction *instr) {
    const char *rv = map_mnemonic(instr->mnemonic);
    str_app(s, "    ");
    str_app(s, rv);

    if (strcmp(instr->mnemonic, "push") == 0 && instr->operand_count == 1 && instr->operands[0].type == ASM_OP_REGISTER) {
        str_fmt(s, " %s, -16(sp)\n", map_reg(instr->operands[0].reg));
        str_app(s, "    addi sp, sp, -16\n");
        return;
    }
    if (strcmp(instr->mnemonic, "pop") == 0 && instr->operand_count == 1 && instr->operands[0].type == ASM_OP_REGISTER) {
        str_fmt(s, " %s, 0(sp)\n", map_reg(instr->operands[0].reg));
        str_app(s, "    addi sp, sp, 16\n");
        return;
    }

    for (int i = 0; i < instr->operand_count; i++) {
        if (i == 0) str_app(s, " ");
        else str_app(s, ", ");
        emit_op(s, &instr->operands[i]);
    }
    str_app(s, "\n");
}

static void emit_dir(Str *s, const AsmDirective *dir) {
    switch (dir->type) {
        case ASM_DIR_SECTION: str_fmt(s, ".section %s\n", dir->name); break;
        case ASM_DIR_GLOBAL: str_fmt(s, ".globl %s\n", dir->name); break;
        case ASM_DIR_EXTERN: str_fmt(s, ".extern %s\n", dir->name); break;
        case ASM_DIR_ALIGN: str_fmt(s, ".balign %ld\n", (long)dir->value); break;
        case ASM_DIR_DB: {
            str_app(s, ".byte ");
            for (int i = 0; i < dir->data_count; i++) {
                if (i > 0) str_app(s, ", ");
                if (dir->data_operands[i].type == ASM_OP_IMMEDIATE) str_fmt(s, "%ld", (long)dir->data_operands[i].immediate);
            }
            str_app(s, "\n"); break;
        }
        case ASM_DIR_DW: {
            str_app(s, ".half ");
            for (int i = 0; i < dir->data_count; i++) {
                if (i > 0) str_app(s, ", ");
                if (dir->data_operands[i].type == ASM_OP_IMMEDIATE) str_fmt(s, "%ld", (long)dir->data_operands[i].immediate);
            }
            str_app(s, "\n"); break;
        }
        case ASM_DIR_DD: {
            str_app(s, ".word ");
            for (int i = 0; i < dir->data_count; i++) {
                if (i > 0) str_app(s, ", ");
                if (dir->data_operands[i].type == ASM_OP_IMMEDIATE) str_fmt(s, "%ld", (long)dir->data_operands[i].immediate);
            }
            str_app(s, "\n"); break;
        }
        case ASM_DIR_DQ: {
            str_app(s, ".dword ");
            for (int i = 0; i < dir->data_count; i++) {
                if (i > 0) str_app(s, ", ");
                if (dir->data_operands[i].type == ASM_OP_IMMEDIATE) str_fmt(s, "%ld", (long)dir->data_operands[i].immediate);
            }
            str_app(s, "\n"); break;
        }
        case ASM_DIR_RESB: str_fmt(s, ".zero %ld\n", (long)dir->value); break;
        case ASM_DIR_RESW: str_fmt(s, ".zero %ld\n", (long)dir->value * 2); break;
        case ASM_DIR_RESD: str_fmt(s, ".zero %ld\n", (long)dir->value * 4); break;
        case ASM_DIR_RESQ: str_fmt(s, ".zero %ld\n", (long)dir->value * 8); break;
        default: str_fmt(s, "# unknown directive: %s\n", dir->name); break;
    }
}

static char *emit_block(const AsmBlock *block, size_t *out_len) {
    Str s; str_init(&s);
    str_app(&s, ".text\n\n");
    for (int i = 0; i < block->count; i++) {
        const AsmElement *e = &block->elements[i];
        switch (e->type) {
            case ASM_ELEM_INSTRUCTION: emit_instr(&s, &e->instr); break;
            case ASM_ELEM_DIRECTIVE: emit_dir(&s, &e->directive); break;
            case ASM_ELEM_LABEL: str_fmt(&s, "%s:\n", e->label.name); break;
            case ASM_ELEM_COMMENT: str_fmt(&s, "# %s\n", e->comment); break;
        }
    }
    if (out_len) *out_len = s.len;
    return s.buf;
}

static void destroy_output(char *output) { free(output); }

AsmBackend *asm_backend_create_riscv64(void) {
    AsmBackend *b = calloc(1, sizeof(AsmBackend));
    if (!b) return NULL;
    b->arch = ASM_ARCH_RISCV64;
    b->emit = emit_block;
    b->destroy = destroy_output;
    return b;
}

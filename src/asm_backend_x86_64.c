/* ================================================================
 * asm_backend_x86_64.c — x86_64 NASM Backend (Passthrough)
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
    s->len += tlen;
    s->buf[s->len] = '\0';
}
static void str_fmt(Str *s, const char *fmt, ...) {
    char tmp[256]; va_list args; va_start(args, fmt); vsnprintf(tmp, sizeof(tmp), fmt, args); va_end(args); str_app(s, tmp);
}

static void emit_reg(Str *s, AsmRegister reg) {
    const char *name = asm_reg_name(reg, ASM_ARCH_X86_64);
    str_app(s, name ? name : "??");
}

static void emit_size(Str *s, AsmSizeSpec sz) {
    switch (sz) {
        case ASM_SIZE_BYTE: str_app(s, "byte "); break;
        case ASM_SIZE_WORD: str_app(s, "word "); break;
        case ASM_SIZE_DWORD: str_app(s, "dword "); break;
        case ASM_SIZE_QWORD: str_app(s, "qword "); break;
        case ASM_SIZE_OWORD: str_app(s, "oword "); break;
        case ASM_SIZE_TWORD: str_app(s, "tword "); break;
        default: break;
    }
}

static void emit_mem(Str *s, const AsmMem *mem) {
    emit_size(s, mem->size);
    switch (mem->mode) {
        case ASM_ADDR_DIRECT:
            if (mem->label[0]) str_fmt(s, "[%s]", mem->label);
            else str_fmt(s, "[0x%lx]", (unsigned long)mem->displacement);
            break;
        case ASM_ADDR_BASE: str_app(s, "["); emit_reg(s, mem->base); str_app(s, "]"); break;
        case ASM_ADDR_BASE_DISP:
            str_fmt(s, "[%s%+ld]", asm_reg_name(mem->base, ASM_ARCH_X86_64), (long)mem->displacement);
            break;
        case ASM_ADDR_BASE_INDEX:
            str_app(s, "["); emit_reg(s, mem->base); str_app(s, "+"); emit_reg(s, mem->index);
            if (mem->scale > 1) str_fmt(s, "*%d", mem->scale);
            str_app(s, "]");
            break;
        case ASM_ADDR_BASE_INDEX_DISP:
            str_app(s, "["); emit_reg(s, mem->base); str_app(s, "+"); emit_reg(s, mem->index);
            if (mem->scale > 1) str_fmt(s, "*%d", mem->scale);
            str_fmt(s, "%+ld", (long)mem->displacement);
            str_app(s, "]");
            break;
        case ASM_ADDR_RIP_REL: str_fmt(s, "[rel %s]", mem->label); break;
        default: str_app(s, "[?]"); break;
    }
}

static void emit_op(Str *s, const AsmOperand *op) {
    switch (op->type) {
        case ASM_OP_REGISTER: emit_reg(s, op->reg); break;
        case ASM_OP_IMMEDIATE: str_fmt(s, "%ld", (long)op->immediate); break;
        case ASM_OP_MEMORY: emit_mem(s, &op->mem); break;
        case ASM_OP_LABEL: str_app(s, op->label); break;
        case ASM_OP_EXPR: str_app(s, op->expr); break;
    }
}

static void emit_instr(Str *s, const AsmInstruction *instr) {
    str_app(s, "    ");
    if (instr->is_lock_prefix) str_app(s, "lock ");
    if (instr->is_rep_prefix) str_app(s, "rep ");
    str_app(s, instr->mnemonic);
    if (instr->has_size_spec) { str_app(s, " "); emit_size(s, instr->size_spec); }
    for (int i = 0; i < instr->operand_count; i++) {
        if (i == 0) str_app(s, " ");
        else str_app(s, ", ");
        emit_op(s, &instr->operands[i]);
    }
    str_app(s, "\n");
}

static void emit_dir(Str *s, const AsmDirective *dir) {
    switch (dir->type) {
        case ASM_DIR_SECTION: str_fmt(s, "section %s\n", dir->name); break;
        case ASM_DIR_GLOBAL: str_fmt(s, "global %s\n", dir->name); break;
        case ASM_DIR_EXTERN: str_fmt(s, "extern %s\n", dir->name); break;
        case ASM_DIR_ALIGN: str_fmt(s, "align %ld\n", (long)dir->value); break;
        case ASM_DIR_ORG: str_fmt(s, "[org 0x%lx]\n", (unsigned long)dir->value); break;
        case ASM_DIR_BITS: str_fmt(s, "[bits %ld]\n", (long)dir->value); break;
        case ASM_DIR_TIMES: str_fmt(s, "times %ld ", (long)dir->value); break;
        case ASM_DIR_DB: case ASM_DIR_DW: case ASM_DIR_DD: case ASM_DIR_DQ: {
            const char *n = dir->type == ASM_DIR_DB ? "db" : dir->type == ASM_DIR_DW ? "dw" : dir->type == ASM_DIR_DD ? "dd" : "dq";
            str_fmt(s, "%s ", n);
            for (int i = 0; i < dir->data_count; i++) {
                if (i > 0) str_app(s, ", ");
                emit_op(s, &dir->data_operands[i]);
            }
            str_app(s, "\n");
            break;
        }
        case ASM_DIR_RESB: str_fmt(s, "resb %ld\n", (long)dir->value); break;
        case ASM_DIR_RESW: str_fmt(s, "resw %ld\n", (long)dir->value); break;
        case ASM_DIR_RESD: str_fmt(s, "resd %ld\n", (long)dir->value); break;
        case ASM_DIR_RESQ: str_fmt(s, "resq %ld\n", (long)dir->value); break;
        default: str_fmt(s, "; unknown directive: %s\n", dir->name); break;
    }
}

static char *emit_block(const AsmBlock *block, size_t *out_len) {
    Str s; str_init(&s);
    for (int i = 0; i < block->count; i++) {
        const AsmElement *e = &block->elements[i];
        switch (e->type) {
            case ASM_ELEM_INSTRUCTION: emit_instr(&s, &e->instr); break;
            case ASM_ELEM_DIRECTIVE: emit_dir(&s, &e->directive); break;
            case ASM_ELEM_LABEL: str_fmt(&s, "%s:\n", e->label.name); break;
            case ASM_ELEM_COMMENT: str_fmt(&s, "; %s\n", e->comment); break;
        }
    }
    if (out_len) *out_len = s.len;
    return s.buf;
}

static void destroy_output(char *output) { free(output); }

AsmBackend *asm_backend_create_x86_64(void) {
    AsmBackend *b = calloc(1, sizeof(AsmBackend));
    if (!b) return NULL;
    b->arch = ASM_ARCH_X86_64;
    b->emit = emit_block;
    b->destroy = destroy_output;
    return b;
}

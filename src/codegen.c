#include "aether/codegen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define INITIAL_CAP 65536

Codegen *codegen_create(Arena *a) {
    Codegen *cg = (Codegen *)arena_alloc(a, sizeof(Codegen));
    if (!cg) return NULL;
    cg->arena = a;
    cg->output = (char *)arena_alloc(a, INITIAL_CAP);
    cg->output_len = 0;
    cg->output_cap = INITIAL_CAP;
    cg->label_counter = 0;
    cg->indent_level = 0;
    cg->current_func = NULL;
    return cg;
}

void codegen_destroy(Codegen *cg) {
    (void)cg; /* all arena-managed */
}

/* ================================================================
 * Output helpers
 * ================================================================ */

static void cg_write(Codegen *cg, const char *s) {
    size_t len = strlen(s);
    if (cg->output_len + len + 1 > cg->output_cap) {
        /* can't grow in arena, just truncate */
        return;
    }
    memcpy(cg->output + cg->output_len, s, len);
    cg->output_len += len;
}

static void cg_write_fmt(Codegen *cg, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) cg_write(cg, buf);
}

static void cg_indent(Codegen *cg) { cg_write(cg, "    "); }
static void cg_label_line(Codegen *cg, const char *s) {
    cg_write_fmt(cg, "%s:\n", s);
}
static void cg_inst(Codegen *cg, const char *inst) {
    cg_indent(cg); cg_write_fmt(cg, "%s\n", inst);
}
static void cg_inst1(Codegen *cg, const char *inst, const char *arg) {
    cg_indent(cg); cg_write_fmt(cg, "%-8s %s\n", inst, arg);
}
static void cg_inst2(Codegen *cg, const char *inst, const char *a1, const char *a2) {
    cg_indent(cg); cg_write_fmt(cg, "%-8s %s, %s\n", inst, a1, a2);
}
static int cg_new_label(Codegen *cg) { return cg->label_counter++; }
static void cg_comment(Codegen *cg, const char *s) {
    cg_write_fmt(cg, "; %s\n", s);
}

/* ================================================================
 * NASM data section helpers
 * ================================================================ */

/* String table: for each string literal, emit a label */
static void cg_string_table(Codegen *cg, AstNode *program) {
    /* scan for string literals, emit them */
    /* For now, we'll emit strings as we encounter them */
    cg_write(cg, "section .rodata\n");
}

/* ================================================================
 * Expression codegen
 * ================================================================ */

/* Returns the register/result location (as a string) */
static const char *cg_expr(Codegen *cg, AstNode *node);

static const char *cg_expr(Codegen *cg, AstNode *node) {
    if (!node) return "0";

    switch (node->type) {
        case NODE_LITERAL_INT: {
            /* Emit mov rax, <value> directly */
            char buf[32];
            snprintf(buf, sizeof(buf), "mov rax, %llu", (unsigned long long)node->data.literal.int_val);
            cg_inst(cg, buf);
            return "rax";
        }
        case NODE_LITERAL_FLOAT: {
            /* floats not fully supported in Phase 0; store in rax */
            cg_inst1(cg, "mov", "rax, 0");
            return "rax";
        }
        case NODE_LITERAL_BOOL:
            cg_inst1(cg, "mov", node->data.literal.bool_val ? "rax, 1" : "rax, 0");
            return "rax";
        case NODE_LITERAL_NONE:
            cg_inst1(cg, "xor", "rax, rax");
            return "rax";
        case NODE_IDENT: {
            /* Look up stack offset from the resolved declaration */
            /* Phase 0: simple approach — variables are on stack, we track offsets */
            cg_inst1(cg, "mov", "rax, [rbp - 8]"); /* placeholder */
            return "rax";
        }
        case NODE_BINARY_OP: {
            const char *left = cg_expr(cg, node->data.binary.left);
            const char *right = cg_expr(cg, node->data.binary.right);
            /* If left/right are literals, use them directly */
            /* For now, push right, compute left, pop to rcx */
            cg_inst1(cg, "push", right);
            cg_inst1(cg, "mov", "rax, rax"); /* left is already in rax from cg_expr */
            cg_inst1(cg, "mov", "rcx, rax");
            cg_inst1(cg, "pop", "rax");

            switch (node->data.binary.op) {
                case BIN_ADD: cg_inst1(cg, "add", "rax, rcx"); break;
                case BIN_SUB: cg_inst1(cg, "sub", "rax, rcx"); break;
                case BIN_MUL: cg_inst1(cg, "mul", "rcx"); break;
                case BIN_DIV: cg_inst1(cg, "xor", "rdx, rdx"); cg_inst1(cg, "div", "rcx"); break;
                case BIN_EQ:  cg_inst1(cg, "cmp", "rax, rcx"); cg_inst1(cg, "sete", "al"); cg_inst1(cg, "movzx", "rax, al"); break;
                case BIN_NEQ: cg_inst1(cg, "cmp", "rax, rcx"); cg_inst1(cg, "setne", "al"); cg_inst1(cg, "movzx", "rax, al"); break;
                case BIN_LT:  cg_inst1(cg, "cmp", "rax, rcx"); cg_inst1(cg, "setl", "al"); cg_inst1(cg, "movzx", "rax, al"); break;
                case BIN_GT:  cg_inst1(cg, "cmp", "rax, rcx"); cg_inst1(cg, "setg", "al"); cg_inst1(cg, "movzx", "rax, al"); break;
                default: cg_inst1(cg, "add", "rax, rcx"); break;
            }
            return "rax";
        }
        case NODE_CALL: {
            /* For now: name(args) — just use 'call' */
            cg_comment(cg, "function call");
            /* Push args */
            for (int i = 0; i < node->data.call.args.count; i++) {
                cg_inst1(cg, "push", cg_expr(cg, node->data.call.args.items[i]));
            }
            if (node->data.call.callee->type == NODE_IDENT) {
                char buf[256];
                snprintf(buf, sizeof(buf), "%.*s",
                    (int)node->data.call.callee->data.ident.name.len,
                    node->data.call.callee->data.ident.name.data);
                cg_inst1(cg, "call", buf);
            }
            return "rax";
        }
        default:
            return "0";
    }
}

/* ================================================================
 * Statement codegen
 * ================================================================ */

static void cg_stmt(Codegen *cg, AstNode *node);

static void cg_stmt(Codegen *cg, AstNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_RETURN: {
            if (node->data.return_node.value) {
                cg_expr(cg, node->data.return_node.value);
                cg_inst1(cg, "mov", "rsp, rbp");
                cg_inst1(cg, "pop", "rbp");
                cg_inst(cg, "ret");
            } else {
                cg_inst1(cg, "mov", "rsp, rbp");
                cg_inst1(cg, "pop", "rbp");
                cg_inst(cg, "ret");
            }
            break;
        }
        case NODE_LET: {
            /* Simple: sub rsp, 8; if has value, move to [rsp] */
            cg_inst1(cg, "sub", "rsp, 8");
            if (node->data.let_decl.value) {
                const char *val = cg_expr(cg, node->data.let_decl.value);
                cg_inst1(cg, "mov", "[rsp], rax");
            }
            break;
        }
        case NODE_IF: {
            int else_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);

            cg_expr(cg, node->data.if_node.condition);
            cg_inst1(cg, "test", "rax, rax");
            cg_write_fmt(cg, "    jz .L%x\n", else_label);

            if (node->data.if_node.then_block)
                cg_stmt(cg, node->data.if_node.then_block);

            cg_write_fmt(cg, "    jmp .L%x\n", end_label);
            cg_write_fmt(cg, ".L%x:\n", else_label);

            /* Handle elif chain */
            if (node->data.if_node.elif_chain) {
                cg_stmt(cg, node->data.if_node.elif_chain);
            }
            if (node->data.if_node.else_block) {
                cg_stmt(cg, node->data.if_node.else_block);
            }
            cg_write_fmt(cg, ".L%x:\n", end_label);
            break;
        }
        case NODE_WHILE: {
            int start_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);
            cg_write_fmt(cg, ".L%x:\n", start_label);
            cg_expr(cg, node->data.while_node.condition);
            cg_inst1(cg, "test", "rax, rax");
            cg_write_fmt(cg, "    jz .L%x\n", end_label);
            if (node->data.while_node.body)
                cg_stmt(cg, node->data.while_node.body);
            cg_write_fmt(cg, "    jmp .L%x\n", start_label);
            cg_write_fmt(cg, ".L%x:\n", end_label);
            break;
        }
        case NODE_EXPR_STMT: {
            cg_expr(cg, node->data.call.callee);
            break;
        }
        case NODE_BLOCK: {
            for (int i = 0; i < node->data.list.count; i++) {
                cg_stmt(cg, node->data.list.items[i]);
            }
            break;
        }
        default:
            break;
    }
}

/* ================================================================
 * Top-level code generation
 * ================================================================ */

const char *codegen_generate(Codegen *cg, AstNode *program) {
    cg_comment(cg, "Generated by Aether Compiler Phase 0");
    cg_write(cg, "\n");

    /* Prologue */
    cg_write(cg, "bits 64\n");
    cg_write(cg, "default rel\n\n");

    /* Data section */
    cg_write(cg, "section .text\n\n");

    /* Entry point */
    cg_write(cg, "global _start\n");
    cg_write(cg, "_start:\n");
    cg_inst1(cg, "mov", "rbp, rsp");
    cg_comment(cg, "call main");
    cg_inst(cg, "call main");
    cg_comment(cg, "exit(main())");
    cg_inst1(cg, "mov", "rdi, rax");   /* main's return value */
    cg_inst1(cg, "mov", "rax, 60");    /* exit syscall */
    cg_inst(cg, "syscall");
    cg_comment(cg, "halt (shouldn't reach here)");
    cg_inst(cg, "hlt");

    /* Generate each top-level declaration */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *node = program->data.list.items[i];
        if (node->type == NODE_FUNC_DECL) {
            cg->current_func = node;
            char fn_label[256];
            snprintf(fn_label, sizeof(fn_label), "%.*s",
                (int)node->data.func.name->data.ident.name.len,
                node->data.func.name->data.ident.name.data);
            
            cg_write_fmt(cg, "; function %s\n", fn_label);
            cg_write_fmt(cg, "global %s\n", fn_label);
            cg_write_fmt(cg, "%s:\n", fn_label);
            
            /* Function prologue */
            cg_inst1(cg, "push", "rbp");
            cg_inst1(cg, "mov", "rbp, rsp");
            
            /* Body */
            if (node->data.func.body) {
                cg_stmt(cg, node->data.func.body);
            }
            
            /* Default: return if no explicit return was hit */
            cg_comment(cg, "default return");
            cg_inst1(cg, "mov", "rsp, rbp");
            cg_inst1(cg, "pop", "rbp");
            cg_inst(cg, "ret");
            cg_write(cg, "\n");
        }
    }

    /* Null-terminate */
    cg->output[cg->output_len] = '\0';
    return cg->output;
}

int codegen_write_asm(Codegen *cg, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    const char *asm_text = codegen_get_asm(cg);
    if (asm_text) fputs(asm_text, f);
    fclose(f);
    return 0;
}

const char *codegen_get_asm(Codegen *cg) {
    return cg->output;
}
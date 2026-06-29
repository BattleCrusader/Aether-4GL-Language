#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Inline assembly codegen
 *
 * Converts Aether inline assembly (Intel/NASM syntax)
 * to GCC extended asm format.
 *
 * GCC supports -masm=intel, so we can keep Intel syntax
 * almost verbatim. The key differences:
 *   - Register names: %rax instead of rax (GCC inline asm
 *     uses % prefix for registers even in Intel mode)
 *   - Output operands: "=r"(var) constraints
 *   - Clobbers: "rax", "rbx", "memory" etc.
 *
 * Forms:
 *   asm { body }              → __asm__ volatile("body");
 *   asm: (out) { body }       → __asm__ volatile("body" : "=r"(out));
 *   asm: (out) { body } :(clobbers)  → __asm__ volatile("body" : "=r"(out) : : "rax");
 * ────────────────────────────────────────────── */

/* ──────────────────────────────────────────────
 * Emit a basic inline assembly block
 * ────────────────────────────────────────────── */
void c_emit_asm_block(CCodegen *cg, AstNode *node) {
    if (!node || node->type != NODE_ASM_BLOCK) return;

    AstNode *text_node = node->data.asm_block.text;
    if (!text_node || text_node->type != NODE_LITERAL_STRING) return;

    StringView sv = text_node->data.literal.string_val;
    int len = (int)sv.len;

    /* Emit __asm__ volatile("...") */
    c_indent(cg);
    fputs("__asm__ volatile(\"", cg->out);

    /* Emit the asm body, escaping special characters */
    for (int i = 0; i < len; i++) {
        char c = sv.data[i];
        if (c == '"') fputs("\\\"", cg->out);
        else if (c == '\\') fputs("\\\\", cg->out);
        else if (c == '\n') fputs("\\n\"\n", cg->out), c_indent(cg), fputs("    \"", cg->out);
        else if (c == '\t') fputc(' ', cg->out);
        else fputc(c, cg->out);
    }

    fputs("\");\n", cg->out);
}

/* ──────────────────────────────────────────────
 * Emit an inline assembly block with output bindings
 *
 * asm: (outputVar) { body }
 *   → __asm__ volatile("body" : "=r"(outputVar));
 *
 * asm: (outputVar) { body } :(clobbers)
 *   → __asm__ volatile("body" : "=r"(outputVar) : : "rax", "rbx");
 * ────────────────────────────────────────────── */
void c_emit_asm_output(CCodegen *cg, AstNode *node) {
    if (!node || node->type != NODE_ASM_BLOCK) return;

    AstNode *text_node = node->data.asm_block.text;
    if (!text_node || text_node->type != NODE_LITERAL_STRING) return;

    StringView sv = text_node->data.literal.string_val;
    int len = (int)sv.len;

    c_indent(cg);
    fputs("__asm__ volatile(\"", cg->out);

    /* Emit the asm body */
    for (int i = 0; i < len; i++) {
        char c = sv.data[i];
        if (c == '"') fputs("\\\"", cg->out);
        else if (c == '\\') fputs("\\\\", cg->out);
        else if (c == '\n') fputs("\\n\"\n", cg->out), c_indent(cg), fputs("    \"", cg->out);
        else if (c == '\t') fputc(' ', cg->out);
        else fputc(c, cg->out);
    }

    /* Emit output operands */
    fputs("\" : ", cg->out);
    if (node->data.asm_block.outputs.count > 0) {
        for (int i = 0; i < node->data.asm_block.outputs.count; i++) {
            if (i > 0) fputs(", ", cg->out);
            AstNode *output = node->data.asm_block.outputs.items[i];
            if (output->type == NODE_IDENT) {
                StringView oname = output->data.ident.name;
                /* Use "=r" constraint for general purpose register */
                fprintf(cg->out, "\"=r\"(%.*s)", (int)oname.len, oname.data);
            }
        }
    } else {
        fputc(' ', cg->out);
    }

    fputs(");\n", cg->out);
}

/* ──────────────────────────────────────────────
 * Emit a top-level assembly block
 * (module-level asm, e.g. .globl symbols)
 * ────────────────────────────────────────────── */
void c_emit_toplevel_asm(CCodegen *cg, AstNode *node) {
    if (!node || node->type != NODE_ASM_BLOCK) return;

    AstNode *text_node = node->data.asm_block.text;
    if (!text_node || text_node->type != NODE_LITERAL_STRING) return;

    StringView sv = text_node->data.literal.string_val;
    int len = (int)sv.len;

    fputs("__asm__(\"", cg->out);
    for (int i = 0; i < len; i++) {
        char c = sv.data[i];
        if (c == '"') fputs("\\\"", cg->out);
        else if (c == '\\') fputs("\\\\", cg->out);
        else if (c == '\n') fputs("\\n\"\n", cg->out), fputs("    \"", cg->out);
        else fputc(c, cg->out);
    }
    fputs("\");\n", cg->out);
}

#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * String operations codegen
 *
 * Handles:
 *   - String interpolation (BIN_CONCAT chains with itoa for numerics)
 *   - String indexing (s[i] → byte at offset)
 *   - String length (#s → s.len)
 * ────────────────────────────────────────────── */

/* ──────────────────────────────────────────────
 * Emit a string interpolation expression.
 * String interpolation like "Hello {name}!" becomes
 * BIN_CONCAT("Hello ", BIN_CONCAT(name, "!")).
 * Numeric expressions in interpolation are auto-converted
 * via __aether_itoa.
 *
 * This is called from c_expr.c for BIN_CONCAT nodes
 * where either operand is numeric (needs itoa conversion).
 * ────────────────────────────────────────────── */
void c_emit_interp_concat(CCodegen *cg, AstNode *node) {
    if (!node || node->type != NODE_BINARY_OP || node->data.binary.op != BIN_CONCAT) {
        c_emit_expr(cg, node);
        return;
    }

    AstNode *left = node->data.binary.left;
    AstNode *right = node->data.binary.right;

    /* Check if left or right needs itoa conversion */
    bool left_is_num = (left && left->type == NODE_LITERAL_INT);
    bool right_is_num = (right && right->type == NODE_LITERAL_INT);

    if (left_is_num || right_is_num) {
        /* Emit: __aether_concat(__aether_itoa(left), right) or similar */
        fputs("__aether_concat(", cg->out);
        if (left_is_num) {
            fputs("__aether_itoa(", cg->out);
            c_emit_expr(cg, left);
            fputs(")", cg->out);
        } else {
            c_emit_expr(cg, left);
        }
        fputs(", ", cg->out);
        if (right_is_num) {
            fputs("__aether_itoa(", cg->out);
            c_emit_expr(cg, right);
            fputs(")", cg->out);
        } else {
            c_emit_expr(cg, right);
        }
        fputs(")", cg->out);
    } else {
        /* Both are strings — regular concat */
        fputs("__aether_concat(", cg->out);
        c_emit_expr(cg, left);
        fputs(", ", cg->out);
        c_emit_expr(cg, right);
        fputs(")", cg->out);
    }
}

/* ──────────────────────────────────────────────
 * Emit string indexing: s[i] → s.ptr[i]
 * ────────────────────────────────────────────── */
void c_emit_string_index(CCodegen *cg, AstNode *target, AstNode *index) {
    c_emit_expr(cg, target);
    fputs(".ptr[", cg->out);
    c_emit_expr(cg, index);
    fputc(']', cg->out);
}

/* ──────────────────────────────────────────────
 * Emit string length: #s → s.len
 * ────────────────────────────────────────────── */
void c_emit_string_length(CCodegen *cg, AstNode *target) {
    c_emit_expr(cg, target);
    fputs(".len", cg->out);
}

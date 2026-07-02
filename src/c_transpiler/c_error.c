#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Error handling codegen — try/catch/throw
 *
 * Aether's error handling uses a return-struct approach:
 *   throws func() → return type is { value, error_tag }
 *   try { body } catch(e) { handler } → check error tag
 *   throw expr → set error tag and return
 *
 * For the C transpiler, we use setjmp/longjmp for try/catch
 * and a simple error struct for throws functions.
 * ────────────────────────────────────────────── */

/* ──────────────────────────────────────────────
 * Emit a throw statement
 *   throw expr → { error_tag = 1; return error_struct; }
 * ────────────────────────────────────────────── */
void c_emit_throw(CCodegen *cg, AstNode *node) {
    c_indent(cg);
    fputs("// throw\n", cg->out);
    if (cg->current_throws_func) {
        /* Inside a throws function: return error struct with err=1 */
        StringView fn = cg->current_throws_func->data.func.name->data.ident.name;
        c_indent(cg);
        fprintf(cg->out, "ThrowResult_%.*s __err = {0};\n", (int)fn.len, fn.data);
        c_indent(cg);
        fprintf(cg->out, "__err.err = 1;\n", (int)fn.len, fn.data);
        c_indent(cg);
        fputs("return __err;\n", cg->out);
    } else {
        /* Outside a throws function: use longjmp for try/catch */
        c_indent(cg);
        fputs("__aether_error_tag = 1;\n", cg->out);
        if (node->data.throw_node.value) {
            c_indent(cg);
            fputs("__aether_error_value = ", cg->out);
            c_emit_expr(cg, node->data.throw_node.value);
            fputs(";\n", cg->out);
        }
        c_indent(cg);
        fputs("longjmp(__aether_jmp_buf, 1);\n", cg->out);
    }
}

/* ──────────────────────────────────────────────
 * Emit a try/catch block
 *   try { body } catch(e) { handler }
 *   → setjmp-based: if (setjmp(__aether_jmp_buf) == 0) { body } else { handler }
 * ────────────────────────────────────────────── */
void c_emit_try(CCodegen *cg, AstNode *node) {
    c_indent(cg);
    fputs("{\n", cg->out);
    cg->indent++;
    c_indent(cg);
    fputs("int __aether_try_val = setjmp(__aether_jmp_buf);\n", cg->out);
    c_indent(cg);
    fputs("if (__aether_try_val == 0) {\n", cg->out);
    cg->indent++;
    c_emit_block(cg, node->data.try_node.body);
    cg->indent--;
    c_indent(cg);
    fputs("} else {\n", cg->out);
    cg->indent++;
    /* Catch blocks — iterate over catch arms */
    for (int ci = 0; ci < node->data.try_node.catch_arms.count; ci++) {
        AstNode *catch_arm = node->data.try_node.catch_arms.items[ci];
        if (catch_arm->data.catch_arm.body) {
            c_emit_block(cg, catch_arm->data.catch_arm.body);
        }
    }
    cg->indent--;
    c_indent(cg);
    fputs("}\n", cg->out);
    /* Finally block — always executes after try/catch */
    if (node->data.try_node.finally_body) {
        c_indent(cg);
        fputs("// finally\n", cg->out);
        c_emit_block(cg, node->data.try_node.finally_body);
    }
    cg->indent--;
    c_indent(cg);
    fputs("}\n", cg->out);
}

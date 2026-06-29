#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Contract codegen — pre/post conditions
 *
 * Aether contracts:
 *   @pre(condition)  → assert at function entry
 *   @post(condition) → assert at function exit
 *
 * For the C transpiler, these become simple
 * if-checks that print and abort on failure.
 * ────────────────────────────────────────────── */

/* ──────────────────────────────────────────────
 * Emit a precondition check
 *   @pre(condition) → if (!(condition)) { fprintf(stderr, "..."); exit(1); }
 * ────────────────────────────────────────────── */
void c_emit_precondition(CCodegen *cg, AstNode *condition, const char *func_name) {
    c_indent(cg);
    fputs("if (!(", cg->out);
    c_emit_expr(cg, condition);
    fputs(")) {\n", cg->out);
    cg->indent++;
    c_indent(cg);
    fprintf(cg->out, "fprintf(stderr, \"Precondition failed in %s\\n\");\n", func_name);
    c_indent(cg);
    fputs("exit(1);\n", cg->out);
    cg->indent--;
    c_indent(cg);
    fputs("}\n", cg->out);
}

/* ──────────────────────────────────────────────
 * Emit a postcondition check
 *   @post(condition) → same at function exit
 * ────────────────────────────────────────────── */
void c_emit_postcondition(CCodegen *cg, AstNode *condition, const char *func_name) {
    c_indent(cg);
    fputs("if (!(", cg->out);
    c_emit_expr(cg, condition);
    fputs(")) {\n", cg->out);
    cg->indent++;
    c_indent(cg);
    fprintf(cg->out, "fprintf(stderr, \"Postcondition failed in %s\\n\");\n", func_name);
    c_indent(cg);
    fputs("exit(1);\n", cg->out);
    cg->indent--;
    c_indent(cg);
    fputs("}\n", cg->out);
}

#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Function codegen — emit C function declarations
 * ────────────────────────────────────────────── */

/* Emit a function prototype (forward declaration) */
void c_emit_func_prototype(CCodegen *cg, AstNode *node) {
    if (node->type != NODE_FUNC_DECL) return;

    StringView fname = node->data.func.name->data.ident.name;
    int param_count = node->data.func.params.count;
    AstNode *return_type_node = node->data.func.return_type;

    /* Skip main() — C handles it specially */
    int is_main = (fname.len == 4 && memcmp(fname.data, "main", 4) == 0);
    if (is_main) return;

    /* Emit return type */
    if (return_type_node) {
        c_emit_type(cg, return_type_node);
    } else {
        fputs("void", cg->out);
    }
    fputc(' ', cg->out);

    /* Emit function name */
    fprintf(cg->out, "%.*s(", (int)fname.len, fname.data);

    /* Emit parameters */
    for (int i = 0; i < param_count; i++) {
        if (i > 0) fputs(", ", cg->out);
        AstNode *param = node->data.func.params.items[i];
        AstNode *param_type = param->data.param.type;
        StringView pname = param->data.param.name->data.ident.name;

        if (param_type) {
            c_emit_type(cg, param_type);
        } else {
            fputs("uint64_t", cg->out);
        }
        fputc(' ', cg->out);
        fprintf(cg->out, "%.*s", (int)pname.len, pname.data);
    }
    fputs(");\n", cg->out);
}

/* Emit a full function definition */
void c_emit_func_decl(CCodegen *cg, AstNode *node) {
    if (node->type != NODE_FUNC_DECL) return;

    StringView fname = node->data.func.name->data.ident.name;
    int param_count = node->data.func.params.count;
    AstNode *return_type_node = node->data.func.return_type;
    AstNode *body = node->data.func.body;

    /* Check if this is main() with a string parameter */
    int is_main = (fname.len == 4 && memcmp(fname.data, "main", 4) == 0);
    int main_has_string_param = 0;
    StringView main_param_name;
    if (is_main && param_count > 0) {
        AstNode *first_param = node->data.func.params.items[0];
        AstNode *ptype = first_param->data.param.type;
        if (ptype && ptype->type == NODE_TYPE_PRIMITIVE &&
            ptype->data.type_node.prim == PRIM_STRING) {
            main_has_string_param = 1;
            main_param_name = first_param->data.param.name->data.ident.name;
        }
    }

    /* Emit return type */
    if (is_main) {
        fputs("int", cg->out);
    } else if (return_type_node) {
        c_emit_type(cg, return_type_node);
    } else {
        fputs("void", cg->out);
    }
    fputc(' ', cg->out);

    /* Emit function name and parameters */
    if (is_main) {
        fprintf(cg->out, "main(int argc, char **argv)");
    } else {
        fprintf(cg->out, "%.*s(", (int)fname.len, fname.data);
        for (int i = 0; i < param_count; i++) {
            if (i > 0) fputs(", ", cg->out);
            AstNode *param = node->data.func.params.items[i];
            AstNode *param_type = param->data.param.type;
            StringView pname = param->data.param.name->data.ident.name;

            if (param_type) {
                c_emit_type(cg, param_type);
            } else {
                fputs("uint64_t", cg->out);
            }
            fputc(' ', cg->out);
            fprintf(cg->out, "%.*s", (int)pname.len, pname.data);
        }
        fputs(")", cg->out);
    }

    /* Emit body */
    if (body) {
        fputs(" {\n", cg->out);
        cg->indent++;

        /* If main has a string param, create it from argv[1] */
        if (main_has_string_param) {
            c_indent(cg);
            fprintf(cg->out, "string %.*s = { 0, NULL };\n",
                (int)main_param_name.len, main_param_name.data);
            c_indent(cg);
            fputs("if (argc > 1) {\n", cg->out);
            cg->indent++;
            c_indent(cg);
            fprintf(cg->out, "    %.*s.len = strlen(argv[1]);\n",
                (int)main_param_name.len, main_param_name.data);
            c_indent(cg);
            fprintf(cg->out, "    %.*s.ptr = argv[1];\n",
                (int)main_param_name.len, main_param_name.data);
            cg->indent--;
            c_indent(cg);
            fputs("}\n", cg->out);
        }

        c_emit_block(cg, body);
        cg->indent--;
        c_indent(cg);
        fputs("}\n\n", cg->out);
    } else {
        fputs(";\n\n", cg->out);
    }
}

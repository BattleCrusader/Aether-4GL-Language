#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* ──────────────────────────────────────────────
 * Function codegen — emit C function declarations
 * ────────────────────────────────────────────── */

/* Mangle an operator symbol into a C-safe function name.
 * e.g. "op_+" → "op_plus", "op_-" → "op_minus", "op_==" → "op_eq_eq"
 * Unicode chars are encoded as "uXXXX" (hex codepoint).
 * Returns a pointer to a static buffer. */
static const char *mangle_func_name(StringView fname) {
    static char buf[128];
    /* Check if this is an operator overload (starts with "op_") */
    if (fname.len >= 3 && memcmp(fname.data, "op_", 3) == 0) {
        int pos = 0;
        pos += snprintf(buf + pos, sizeof(buf) - pos, "op_");
        for (size_t i = 3; i < fname.len && pos < (int)sizeof(buf) - 8; i++) {
            unsigned char c = (unsigned char)fname.data[i];
            if (isalnum(c) || c == '_') {
                buf[pos++] = (char)c;
            } else if (c >= 128) {
                /* Multi-byte UTF-8: decode the codepoint */
                uint32_t cp = 0;
                if ((c & 0xE0) == 0xC0) {
                    cp = c & 0x1F;
                    if (i + 1 < fname.len) { i++; cp = (cp << 6) | ((unsigned char)fname.data[i] & 0x3F); }
                } else if ((c & 0xF0) == 0xE0) {
                    cp = c & 0x0F;
                    if (i + 1 < fname.len) { i++; cp = (cp << 6) | ((unsigned char)fname.data[i] & 0x3F); }
                    if (i + 1 < fname.len) { i++; cp = (cp << 6) | ((unsigned char)fname.data[i] & 0x3F); }
                } else if ((c & 0xF8) == 0xF0) {
                    cp = c & 0x07;
                    if (i + 1 < fname.len) { i++; cp = (cp << 6) | ((unsigned char)fname.data[i] & 0x3F); }
                    if (i + 1 < fname.len) { i++; cp = (cp << 6) | ((unsigned char)fname.data[i] & 0x3F); }
                    if (i + 1 < fname.len) { i++; cp = (cp << 6) | ((unsigned char)fname.data[i] & 0x3F); }
                }
                pos += snprintf(buf + pos, sizeof(buf) - pos, "u%04X", (unsigned)cp);
            } else if (c == '+') { memcpy(buf + pos, "plus", 4); pos += 4; }
            else if (c == '-') { memcpy(buf + pos, "minus", 5); pos += 5; }
            else if (c == '*') { memcpy(buf + pos, "star", 4); pos += 4; }
            else if (c == '/') { memcpy(buf + pos, "slash", 5); pos += 5; }
            else if (c == '%') { memcpy(buf + pos, "percent", 7); pos += 7; }
            else if (c == '=') { memcpy(buf + pos, "eq", 2); pos += 2; }
            else if (c == '!') { memcpy(buf + pos, "bang", 4); pos += 4; }
            else if (c == '<') { memcpy(buf + pos, "lt", 2); pos += 2; }
            else if (c == '>') { memcpy(buf + pos, "gt", 2); pos += 2; }
            else if (c == '&') { memcpy(buf + pos, "amp", 3); pos += 3; }
            else if (c == '|') { memcpy(buf + pos, "pipe", 4); pos += 4; }
            else if (c == '^') { memcpy(buf + pos, "caret", 5); pos += 5; }
            else if (c == '~') { memcpy(buf + pos, "tilde", 5); pos += 5; }
            else { buf[pos++] = '_'; }
        }
        buf[pos] = '\0';
        return buf;
    }
    /* Normal function name: use as-is */
    int len = (int)fname.len;
    if (len > 127) len = 127;
    memcpy(buf, fname.data, len);
    buf[len] = '\0';
    return buf;
}

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
    if (node->data.func.is_sys) {
        fputs("__aether_sys_", cg->out);
    }
    fputs(mangle_func_name(fname), cg->out);
    fputc('(', cg->out);

    /* Emit parameters */
    for (int i = 0; i < param_count; i++) {
        if (i > 0) fputs(", ", cg->out);
        AstNode *param = node->data.func.params.items[i];
        AstNode *param_type = param->data.param.type;
        StringView pname = param->data.param.name->data.ident.name;

        if (param_type) {
            if (param->data.param.is_varargs) {
                fputs("slice", cg->out);
            } else {
                c_emit_type(cg, param_type);
            }
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

    /* Generic functions: emit as regular functions with uint64_t for generic params */
    bool is_generic = (node->data.func.type_params.count > 0);

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
        if (node->data.func.is_sys) {
            fputs("__aether_sys_", cg->out);
        }
        fputs(mangle_func_name(fname), cg->out);
        fputc('(', cg->out);
        for (int i = 0; i < param_count; i++) {
            if (i > 0) fputs(", ", cg->out);
            AstNode *param = node->data.func.params.items[i];
            AstNode *param_type = param->data.param.type;
            StringView pname = param->data.param.name->data.ident.name;

            if (param_type) {
                if (param->data.param.is_varargs) {
                    fputs("slice", cg->out);
                } else {
                    c_emit_type(cg, param_type);
                }
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
            fprintf(cg->out, "    %.*s.data = argv[1];\n",
                (int)main_param_name.len, main_param_name.data);
            cg->indent--;
            c_indent(cg);
            fputs("}\n", cg->out);
        }

        if (is_generic) {
            /* Generic function: emit body with uint64_t for generic params */
            c_emit_block(cg, body);
        } else {
            c_emit_block(cg, body);
        }
        cg->indent--;
        c_indent(cg);
        fputs("}\n\n", cg->out);
    } else {
        /* No body = function declaration only (extern/FFI) */
        /* Emit as extern declaration */
        fputs("; // extern\n\n", cg->out);
    }
}

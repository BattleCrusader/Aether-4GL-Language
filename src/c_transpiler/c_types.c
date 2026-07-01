#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Type mapping — Aether → C type names
 * ────────────────────────────────────────────── */
const char *c_prim_type_name(PrimType prim) {
    switch (prim) {
        case PRIM_VOID:   return "void";
        case PRIM_BOOL:   return "uint8_t";
        case PRIM_BYTE:
        case PRIM_U8:     return "uint8_t";
        case PRIM_U16:    return "uint16_t";
        case PRIM_U32:    return "uint32_t";
        case PRIM_U64:    return "uint64_t";
        case PRIM_U128:   return "unsigned __int128";
        case PRIM_I8:     return "int8_t";
        case PRIM_I16:    return "int16_t";
        case PRIM_I32:    return "int32_t";
        case PRIM_I64:    return "int64_t";
        case PRIM_F32:    return "float";
        case PRIM_F64:    return "double";
        case PRIM_STRING: return "string";
    }
    return "uint64_t";
}

void c_emit_prim_type(CCodegen *cg, PrimType prim) {
    fputs(c_prim_type_name(prim), cg->out);
}

/* ──────────────────────────────────────────────
 * Map Aether type AST node to C type name
 * ────────────────────────────────────────────── */
const char *c_type_name(AstNode *type_node) {
    if (!type_node) return "void";
    switch (type_node->type) {
        case NODE_TYPE_PRIMITIVE:
            return c_prim_type_name(type_node->data.type_node.prim);
        case NODE_TYPE_PTR:
            return "void*";  /* generic pointer */
        case NODE_TYPE_REF: {
            /* ref T → T* (pointer to the base type) */
            /* owned T → T* (unique ownership pointer) */
            /* rc T → void* (reference-counted pointer, retain/release manage it) */
            if (type_node->data.type_node.is_rc) {
                return "void*";
            }
            /* ref T or owned T → T* */
            if (type_node->data.type_node.elem_type) {
                static char buf[256];
                const char *base = c_type_name(type_node->data.type_node.elem_type);
                int len = snprintf(buf, sizeof(buf), "%s*", base);
                if (len > 0 && len < 256) return buf;
            }
            return "void*";
        }
        case NODE_TYPE_ARRAY:
            return "slice";  /* dynamic array → slice */
        case NODE_TYPE_SLICE:
            return "slice";
        case NODE_TYPE_OPTIONAL: {
            /* Optional types: use the base type with NULL convention */
            if (type_node->data.type_node.elem_type) {
                return c_type_name(type_node->data.type_node.elem_type);
            }
            return "void*";
        }
        case NODE_TYPE_TUPLE:
            return "uint64_t";  /* tuples stored as packed uint64_t */
        case NODE_TYPE_FN:
            return "void*";  /* function pointer */
        case NODE_TYPE_INFER:
            return "uint64_t";  /* inferred type defaults to uint64_t */
        case NODE_TYPE_NAMED: {
            /* Named type — use the name string */
            StringView sv = type_node->data.type_node.name;
            /* Generic type params (single uppercase letter like T, U, V) → uint64_t */
            if (sv.len == 1 && sv.data[0] >= 'A' && sv.data[0] <= 'Z') {
                return "uint64_t";
            }
            static char buf[256];
            int len = (int)sv.len;
            if (len > 255) len = 255;
            memcpy(buf, sv.data, len);
            buf[len] = '\0';
            return buf;
        }
        default:
            return "uint64_t";
    }
}

void c_emit_type(CCodegen *cg, AstNode *type_node) {
    fputs(c_type_name(type_node), cg->out);
}

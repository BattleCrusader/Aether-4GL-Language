#include "codegen_internal.h"

/* Type helpers
 * ================================================================ */

int type_size(AstNode *type) {
    if (!type) return 8; /* default (u64) */
    if (type->type == NODE_TYPE_PRIMITIVE) {
        switch (type->data.type_node.prim) {
            case PRIM_VOID: return 0;
            case PRIM_BOOL: case PRIM_BYTE: case PRIM_U8: case PRIM_I8: return 1;
            case PRIM_U16: case PRIM_I16: return 2;
            case PRIM_U32: case PRIM_I32: case PRIM_F32: return 4;
            case PRIM_U64: case PRIM_I64: case PRIM_F64: case PRIM_STRING: return 8;
        }
    }
    if (type->type == NODE_TYPE_PTR || type->type == NODE_TYPE_REF) return 8;
    if (type->type == NODE_TYPE_OPTIONAL) {
        return 1 + type_size(type->data.type_node.elem_type);
    }
    if (type->type == NODE_TYPE_ARRAY && type->data.type_node.array_size > 0) {
        return type->data.type_node.array_size * type_size(type->data.type_node.elem_type);
    }
    if (type->type == NODE_TYPE_NAMED) {
        char tn[256];
        int nlen = (int)type->data.type_node.name.len;
        if (nlen > 255) nlen = 255;
        memcpy(tn, type->data.type_node.name.data, nlen);
        tn[nlen] = '\0';
        StructLayout *sl = find_struct_layout(tn);
        if (sl) return sl->total_size;
        /* Check enum layouts too */
        for (StructLayout *el = enum_layouts; el; el = el->next) {
            if (strcmp(el->name, tn) == 0) return el->total_size;
        }
    }
    return 8; /* default pointer-sized */
}

/* Extract PrimType from a type annotation node */
PrimType prim_from_type(AstNode *type) {
    if (!type || type->type != NODE_TYPE_PRIMITIVE) return PRIM_VOID;
    return type->data.type_node.prim;
}

/* Check if an expression node evaluates to a numeric type (u8-u64, i8-i64, bool, byte) */
bool is_numeric_expr(AstNode *node) {
    if (!node) return false;
    if (node->type == NODE_LITERAL_INT || node->type == NODE_LITERAL_FLOAT ||
        node->type == NODE_LITERAL_BOOL || node->type == NODE_LITERAL_CHAR) {
        return true;
    }
    /* Binary ops that produce numeric results (add, sub, mul, etc.) */
    if (node->type == NODE_BINARY_OP) {
        BinOp op = node->data.binary.op;
        if (op == BIN_ADD || op == BIN_SUB || op == BIN_MUL || op == BIN_DIV || op == BIN_MOD ||
            op == BIN_BIT_AND || op == BIN_BIT_OR || op == BIN_BIT_XOR ||
            op == BIN_SHL || op == BIN_SHR ||
            op == BIN_ADD_ASSIGN || op == BIN_SUB_ASSIGN || op == BIN_MUL_ASSIGN || op == BIN_DIV_ASSIGN) {
            return true;
        }
        return false;
    }
    /* Unary ops that produce numeric results */
    if (node->type == NODE_UNARY_OP) {
        UnaryOp op = node->data.unary.op;
        if (op == UNARY_NEG || op == UNARY_BIT_NOT || op == UNARY_INC || op == UNARY_DEC) {
            return true;
        }
        return false;
    }
    if (node->type == NODE_IDENT && node->data.ident.resolved) {
        AstNode *decl = node->data.ident.resolved;
        /* Variadic params are array pointers, not numeric */
        if (decl->type == NODE_PARAM && decl->data.param.is_varargs) return false;
        /* For-loop variables are always u64 */
        if (decl->type == NODE_IDENT) return true;
        AstNode *type_node = NULL;
        if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
        else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
        if (type_node && type_node->type == NODE_TYPE_PRIMITIVE) {
            PrimType pt = type_node->data.type_node.prim;
            return pt == PRIM_U8 || pt == PRIM_U16 || pt == PRIM_U32 || pt == PRIM_U64 ||
                   pt == PRIM_I8 || pt == PRIM_I16 || pt == PRIM_I32 || pt == PRIM_I64 ||
                   pt == PRIM_BOOL || pt == PRIM_BYTE;
        }
        /* No type annotation — check if initializer value is numeric */
        if (!type_node && decl->type == NODE_LET && decl->data.let_decl.value) {
            return is_numeric_expr(decl->data.let_decl.value);
        }
    }
    /* Also check by name lookup for idents that weren't resolved by semantic analysis */
    if (node->type == NODE_IDENT && !node->data.ident.resolved) {
        return true;
    }
    /* Function calls that return numeric types */
    if (node->type == NODE_CALL && node->data.call.callee &&
        node->data.call.callee->type == NODE_IDENT) {
        /* Check resolved declaration first */
        if (node->data.call.callee->data.ident.resolved &&
            node->data.call.callee->data.ident.resolved->type == NODE_FUNC_DECL) {
            AstNode *ret_type = node->data.call.callee->data.ident.resolved->data.func.return_type;
            if (ret_type && ret_type->type == NODE_TYPE_PRIMITIVE) {
                PrimType pt = ret_type->data.type_node.prim;
                return pt == PRIM_U8 || pt == PRIM_U16 || pt == PRIM_U32 || pt == PRIM_U64 ||
                       pt == PRIM_I8 || pt == PRIM_I16 || pt == PRIM_I32 || pt == PRIM_I64 ||
                       pt == PRIM_BOOL || pt == PRIM_BYTE;
            }
        }
        /* If not resolved, check by name against known functions in the program */
        /* For now, assume function calls return u64 (the default) */
        return true;
    }
    return false;
}

/* Check if an expression node evaluates to a string type */
bool is_string_expr(AstNode *node) {
    if (!node) return false;
    if (node->type == NODE_LITERAL_STRING) return true;
    /* BIN_CONCAT chains produce strings (interpolation) */
    if (node->type == NODE_BINARY_OP && node->data.binary.op == BIN_CONCAT) return true;
    if (node->type == NODE_IDENT && node->data.ident.resolved) {
        AstNode *decl = node->data.ident.resolved;
        AstNode *type_node = NULL;
        if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
        else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
        if (type_node && type_node->type == NODE_TYPE_PRIMITIVE) {
            return type_node->data.type_node.prim == PRIM_STRING;
        }
        if (!type_node && decl->type == NODE_LET && decl->data.let_decl.value) {
            return is_string_expr(decl->data.let_decl.value);
        }
    }
    /* Function calls that return string types */
    if (node->type == NODE_CALL && node->data.call.callee &&
        node->data.call.callee->type == NODE_IDENT) {
        if (node->data.call.callee->data.ident.resolved &&
            node->data.call.callee->data.ident.resolved->type == NODE_FUNC_DECL) {
            AstNode *ret_type = node->data.call.callee->data.ident.resolved->data.func.return_type;
            if (ret_type && ret_type->type == NODE_TYPE_PRIMITIVE) {
                return ret_type->data.type_node.prim == PRIM_STRING;
            }
        }
    }
    return false;
}

/* ================================================================ */
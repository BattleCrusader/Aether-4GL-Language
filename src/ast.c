#include "aether/ast.h"
#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 * Node creation
 * ================================================================ */

AstNode *node_create(Arena *a, NodeType type, Location loc) {
    AstNode *node = (AstNode *)arena_alloc(a, sizeof(AstNode));
    if (!node) return NULL;
    node->type = type;
    node->loc = loc;
    return node;
}

AstNode *node_program(Arena *a) {
    AstNode *node = node_create(a, NODE_PROGRAM, NO_LOCATION);
    if (node) {
        node->data.list = (AstNodeList){0};
    }
    return node;
}

AstNode *node_func_decl(Arena *a, Location loc, AstNode *name, bool is_pub, bool is_static) {
    AstNode *node = node_create(a, NODE_FUNC_DECL, loc);
    if (node) {
        node->data.func.name = name;
        node->data.func.params = (AstNodeList){0};
        node->data.func.is_pub = is_pub;
        node->data.func.is_static = is_static;
        node->data.func.sys_index = -1;
        node->data.func.entry_addr = -1;
        node->data.func.is_kernel_layout = false;
        node->data.func.has_layout = false;
        node->data.func.layout_start = 0;
        node->data.func.layout_max = 0;
        node->data.func.layout_bits = 0;
        node->data.func.layout_signature = 0;
        node->data.func.layout_file = (StringView){0};
    }
    return node;
}

AstNode *node_param(Arena *a, Location loc, AstNode *name, AstNode *type, bool is_mut, bool is_varargs) {
    AstNode *node = node_create(a, NODE_PARAM, loc);
    if (node) {
        node->data.param.name = name;
        node->data.param.type = type;
        node->data.param.is_mut = is_mut;
        node->data.param.is_varargs = is_varargs;
    }
    return node;
}

AstNode *node_let(Arena *a, Location loc, AstNode *name, AstNode *type, AstNode *value, bool is_mut) {
    AstNode *node = node_create(a, NODE_LET, loc);
    if (node) {
        node->data.let_decl.name = name;
        node->data.let_decl.type = type;
        node->data.let_decl.value = value;
        node->data.let_decl.is_mut = is_mut;
    }
    return node;
}

AstNode *node_if(Arena *a, Location loc, AstNode *cond, AstNode *then_block, AstNode *elif_chain, AstNode *else_block) {
    AstNode *node = node_create(a, NODE_IF, loc);
    if (node) {
        node->data.if_node.condition = cond;
        node->data.if_node.then_block = then_block;
        node->data.if_node.elif_chain = elif_chain;
        node->data.if_node.else_block = else_block;
    }
    return node;
}

AstNode *node_while(Arena *a, Location loc, AstNode *cond, AstNode *body) {
    AstNode *node = node_create(a, NODE_WHILE, loc);
    if (node) {
        node->data.while_node.condition = cond;
        node->data.while_node.body = body;
    }
    return node;
}

AstNode *node_for(Arena *a, Location loc, AstNode *var, AstNode *iterable, AstNode *body) {
    AstNode *node = node_create(a, NODE_FOR, loc);
    if (node) {
        node->data.for_node.var = var;
        node->data.for_node.iterable = iterable;
        node->data.for_node.body = body;
    }
    return node;
}

AstNode *node_return(Arena *a, Location loc, AstNode *value) {
    AstNode *node = node_create(a, NODE_RETURN, loc);
    if (node) node->data.return_node.value = value;
    return node;
}

AstNode *node_break(Arena *a, Location loc) {
    return node_create(a, NODE_BREAK, loc);
}

AstNode *node_continue(Arena *a, Location loc) {
    return node_create(a, NODE_CONTINUE, loc);
}

AstNode *node_block(Arena *a, Location loc) {
    AstNode *node = node_create(a, NODE_BLOCK, loc);
    if (node) node->data.list = (AstNodeList){0};
    return node;
}

AstNode *node_ident(Arena *a, Location loc, StringView name) {
    AstNode *node = node_create(a, NODE_IDENT, loc);
    if (node) {
        node->data.ident.name = name;
        node->data.ident.resolved = NULL;
    }
    return node;
}

AstNode *node_int_literal(Arena *a, Location loc, uint64_t val) {
    AstNode *node = node_create(a, NODE_LITERAL_INT, loc);
    if (node) {
        node->data.literal.int_val = val;
        node->data.literal.is_negative = false;
    }
    return node;
}

AstNode *node_float_literal(Arena *a, Location loc, double val) {
    AstNode *node = node_create(a, NODE_LITERAL_FLOAT, loc);
    if (node) node->data.literal.float_val = val;
    return node;
}

AstNode *node_string_literal(Arena *a, Location loc, StringView val) {
    AstNode *node = node_create(a, NODE_LITERAL_STRING, loc);
    if (node) node->data.literal.string_val = val;
    return node;
}

AstNode *node_char_literal(Arena *a, Location loc, uint32_t val) {
    AstNode *node = node_create(a, NODE_LITERAL_CHAR, loc);
    if (node) node->data.literal.char_val = val;
    return node;
}

AstNode *node_bool_literal(Arena *a, Location loc, bool val) {
    AstNode *node = node_create(a, NODE_LITERAL_BOOL, loc);
    if (node) node->data.literal.bool_val = val;
    return node;
}

AstNode *node_none_literal(Arena *a, Location loc) {
    return node_create(a, NODE_LITERAL_NONE, loc);
}

AstNode *node_binary(Arena *a, Location loc, BinOp op, AstNode *left, AstNode *right) {
    AstNode *node = node_create(a, NODE_BINARY_OP, loc);
    if (node) {
        node->data.binary.op = op;
        node->data.binary.left = left;
        node->data.binary.right = right;
    }
    return node;
}

AstNode *node_unary(Arena *a, Location loc, UnaryOp op, AstNode *operand) {
    AstNode *node = node_create(a, NODE_UNARY_OP, loc);
    if (node) {
        node->data.unary.op = op;
        node->data.unary.operand = operand;
    }
    return node;
}

AstNode *node_call(Arena *a, Location loc, AstNode *callee) {
    AstNode *node = node_create(a, NODE_CALL, loc);
    if (node) {
        node->data.call.callee = callee;
        node->data.call.args = (AstNodeList){0};
    }
    return node;
}

AstNode *node_index(Arena *a, Location loc, AstNode *target, AstNode *index) {
    AstNode *node = node_create(a, NODE_INDEX, loc);
    if (node) {
        node->data.index.target = target;
        node->data.index.index = index;
    }
    return node;
}

AstNode *node_field_access(Arena *a, Location loc, AstNode *target, AstNode *field) {
    AstNode *node = node_create(a, NODE_FIELD_ACCESS, loc);
    if (node) {
        node->data.field.target = target;
        node->data.field.field = field;
    }
    return node;
}

AstNode *node_type_prim(Arena *a, Location loc, PrimType prim) {
    AstNode *node = node_create(a, NODE_TYPE_PRIMITIVE, loc);
    if (node) node->data.type_node.prim = prim;
    return node;
}

AstNode *node_type_named(Arena *a, Location loc, StringView name) {
    AstNode *node = node_create(a, NODE_TYPE_NAMED, loc);
    if (node) node->data.type_node.name = name;
    return node;
}

AstNode *node_match(Arena *a, Location loc, AstNode *value) {
    AstNode *node = node_create(a, NODE_MATCH, loc);
    if (node) {
        node->data.match_node.value = value;
        node->data.match_node.arms = (AstNodeList){0};
    }
    return node;
}

AstNode *node_match_arm(Arena *a, Location loc, AstNode *pattern, AstNode *body) {
    AstNode *node = node_create(a, NODE_MATCH_ARM, loc);
    if (node) {
        node->data.match_arm.pattern = pattern;
        node->data.match_arm.body = body;
    }
    return node;
}

AstNode *node_asm_block(Arena *a, Location loc, AstNode *text) {
    AstNode *node = node_create(a, NODE_ASM_BLOCK, loc);
    if (node) node->data.asm_block.text = text;
    return node;
}

AstNode *node_struct_decl(Arena *a, Location loc, AstNode *name, bool is_pub) {
    AstNode *node = node_create(a, NODE_STRUCT_DECL, loc);
    if (node) {
        node->data.struct_decl.name = name;
        node->data.struct_decl.fields = (AstNodeList){0};
        node->data.struct_decl.is_pub = is_pub;
    }
    return node;
}

AstNode *node_enum_decl(Arena *a, Location loc, AstNode *name, bool is_pub) {
    AstNode *node = node_create(a, NODE_ENUM_DECL, loc);
    if (node) {
        node->data.enum_decl.name = name;
        node->data.enum_decl.variants = (AstNodeList){0};
        node->data.enum_decl.is_pub = is_pub;
    }
    return node;
}

AstNode *node_defer(Arena *a, Location loc, AstNode *body) {
    AstNode *node = node_create(a, NODE_DEFER, loc);
    if (node) node->data.defer_node.body = body;
    return node;
}

AstNode *node_region(Arena *a, Location loc, StringView name, AstNode *body) {
    AstNode *node = node_create(a, NODE_REGION, loc);
    if (node) {
        node->data.region_node.name = name;
        node->data.region_node.body = body;
    }
    return node;
}

AstNode *node_expr_stmt(Arena *a, Location loc, AstNode *expr) {
    AstNode *node = node_create(a, NODE_EXPR_STMT, loc);
    if (node) node->data.call.callee = expr;
    return node;
}

AstNode *node_try(Arena *a, Location loc, AstNode *body) {
    AstNode *node = node_create(a, NODE_TRY, loc);
    if (node) {
        node->data.try_node.body = body;
        node->data.try_node.catch_arms = (AstNodeList){0};
    }
    return node;
}

AstNode *node_throw(Arena *a, Location loc, AstNode *value) {
    AstNode *node = node_create(a, NODE_THROW, loc);
    if (node) node->data.throw_node.value = value;
    return node;
}

AstNode *node_catch_arm(Arena *a, Location loc, AstNode *type, AstNode *var, AstNode *body) {
    AstNode *node = node_create(a, NODE_CATCH_ARM, loc);
    if (node) {
        node->data.catch_arm.type = type;
        node->data.catch_arm.var = var;
        node->data.catch_arm.body = body;
    }
    return node;
}

/* ================================================================
 * List helpers
 * ================================================================ */

void node_list_append(AstNodeList *list, AstNode *node) {
    if (!list || !node) return;
    if (list->count >= list->cap) {
        int new_cap = list->cap ? list->cap * 2 : 8;
        AstNode **new_items = (AstNode **)realloc(list->items, sizeof(AstNode *) * new_cap);
        if (!new_items) return;
        list->items = new_items;
        list->cap = new_cap;
    }
    list->items[list->count++] = node;
}

void node_list_append_sorted(AstNodeList *list, AstNode *node) {
    node_list_append(list, node);
}

/* ================================================================
 * Name helpers
 * ================================================================ */

const char *node_type_name(NodeType type) {
    switch (type) {
        case NODE_PROGRAM: return "PROGRAM";
        case NODE_FUNC_DECL: return "FUNC_DECL";
        case NODE_PARAM: return "PARAM";
        case NODE_STRUCT_DECL: return "STRUCT_DECL";
        case NODE_CLASS_DECL: return "CLASS_DECL";
        case NODE_FIELD: return "FIELD";
        case NODE_ENUM_DECL: return "ENUM_DECL";
        case NODE_ENUM_VARIANT: return "ENUM_VARIANT";
        case NODE_CONST_DECL: return "CONST_DECL";
        case NODE_IMPORT: return "IMPORT";
        case NODE_MODULE_DECL: return "MODULE_DECL";
        case NODE_TYPE_ALIAS: return "TYPE_ALIAS";
        case NODE_TRAIT_DECL: return "TRAIT_DECL";
        case NODE_IMPL_BLOCK: return "IMPL_BLOCK";
        case NODE_POOL_DECL: return "POOL_DECL";
        case NODE_PROTOCOL_DECL: return "PROTOCOL_DECL";
        case NODE_BLOCK: return "BLOCK";
        case NODE_LET: return "LET";
        case NODE_ASSIGN: return "ASSIGN";
        case NODE_IF: return "IF";
        case NODE_WHILE: return "WHILE";
        case NODE_FOR: return "FOR";
        case NODE_RETURN: return "RETURN";
        case NODE_BREAK: return "BREAK";
        case NODE_CONTINUE: return "CONTINUE";
        case NODE_DEFER: return "DEFER";
        case NODE_EXPR_STMT: return "EXPR_STMT";
        case NODE_MATCH: return "MATCH";
        case NODE_MATCH_ARM: return "MATCH_ARM";
        case NODE_LITERAL_INT: return "LITERAL_INT";
        case NODE_LITERAL_FLOAT: return "LITERAL_FLOAT";
        case NODE_LITERAL_STRING: return "LITERAL_STRING";
        case NODE_LITERAL_CHAR: return "LITERAL_CHAR";
        case NODE_LITERAL_BOOL: return "LITERAL_BOOL";
        case NODE_LITERAL_NONE: return "LITERAL_NONE";
        case NODE_IDENT: return "IDENT";
        case NODE_BINARY_OP: return "BINARY_OP";
        case NODE_UNARY_OP: return "UNARY_OP";
        case NODE_CALL: return "CALL";
        case NODE_INDEX: return "INDEX";
        case NODE_SLICE: return "SLICE";
        case NODE_FIELD_ACCESS: return "FIELD_ACCESS";
        case NODE_CAST: return "CAST";
        case NODE_TERNARY: return "TERNARY";
        case NODE_LAMBDA: return "LAMBDA";
        case NODE_ARRAY_LIT: return "ARRAY_LIT";
        case NODE_TYPE_PRIMITIVE: return "TYPE_PRIMITIVE";
        case NODE_TYPE_NAMED: return "TYPE_NAMED";
        case NODE_TYPE_ARRAY: return "TYPE_ARRAY";
        case NODE_TYPE_SLICE: return "TYPE_SLICE";
        case NODE_TYPE_TUPLE: return "TYPE_TUPLE";
        case NODE_TYPE_OPTIONAL: return "TYPE_OPTIONAL";
        case NODE_TYPE_REF: return "TYPE_REF";
        case NODE_TYPE_PTR: return "TYPE_PTR";
        case NODE_TYPE_FN: return "TYPE_FN";
        case NODE_TYPE_INFER: return "TYPE_INFER";
        case NODE_ASM_BLOCK: return "ASM_BLOCK";
        case NODE_ATTR: return "ATTR";
        case NODE_REGION: return "REGION";
        case NODE_RUN_BLOCK: return "RUN_BLOCK";
        case NODE_PROPERTY: return "PROPERTY";
        case NODE_TRY: return "TRY";
        case NODE_THROW: return "THROW";
        case NODE_CATCH_ARM: return "CATCH_ARM";
    }
    return "UNKNOWN";
}

const char *binop_name(BinOp op) {
    switch (op) {
        case BIN_ADD: return "+"; case BIN_SUB: return "-"; case BIN_MUL: return "*";
        case BIN_DIV: return "/"; case BIN_MOD: return "%";
        case BIN_EQ: return "=="; case BIN_NEQ: return "!=";
        case BIN_LT: return "<"; case BIN_GT: return ">"; case BIN_LE: return "<="; case BIN_GE: return ">=";
        case BIN_AND: return "&&"; case BIN_OR: return "||";
        case BIN_BIT_AND: return "&"; case BIN_BIT_OR: return "|"; case BIN_BIT_XOR: return "^";
        case BIN_SHL: return "<<"; case BIN_SHR: return ">>";
        case BIN_ASSIGN: return "="; case BIN_ADD_ASSIGN: return "+=";
        case BIN_SUB_ASSIGN: return "-="; case BIN_MUL_ASSIGN: return "*="; case BIN_DIV_ASSIGN: return "/=";
        case BIN_RANGE: return ".."; case BIN_RANGE_INCLUSIVE: return "..=";
    }
    return "?";
}

const char *unaryop_name(UnaryOp op) {
    switch (op) {
        case UNARY_NEG: return "-"; case UNARY_NOT: return "!"; case UNARY_BIT_NOT: return "~";
        case UNARY_REF: return "ref"; case UNARY_DEREF: return "*";
        case UNARY_ADDR: return "&"; case UNARY_OWNED: return "owned"; case UNARY_MUT: return "mut";
    }
    return "?";
}

const char *primtype_name(PrimType pt) {
    switch (pt) {
        case PRIM_VOID: return "void"; case PRIM_BOOL: return "bool"; case PRIM_BYTE: return "byte";
        case PRIM_U8: return "u8"; case PRIM_U16: return "u16"; case PRIM_U32: return "u32"; case PRIM_U64: return "u64";
        case PRIM_I8: return "i8"; case PRIM_I16: return "i16"; case PRIM_I32: return "i32"; case PRIM_I64: return "i64";
        case PRIM_F32: return "f32"; case PRIM_F64: return "f64";
        case PRIM_STRING: return "string";
    }
    return "?";
}

/* ================================================================
 * AST dump for debugging
 * ================================================================ */

void ast_dump_indent(int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
}

static void dump_list(AstNodeList *list, int depth, const char *label) {
    if (list->count == 0) return;
    ast_dump_indent(depth);
    printf("%s: %d items\n", label, list->count);
    for (int i = 0; i < list->count; i++) {
        ast_dump(list->items[i], depth + 1);
    }
}

void ast_dump(AstNode *node, int depth) {
    if (!node) { ast_dump_indent(depth); printf("(null)\n"); return; }
    
    ast_dump_indent(depth);
    printf("%s", node_type_name(node->type));
    if (node->loc.file) printf(" [%s:%d:%d]", node->loc.file, node->loc.line, node->loc.col);
    printf("\n");
    
    switch (node->type) {
        case NODE_PROGRAM:
            dump_list(&node->data.list, depth + 1, "decls");
            break;
        case NODE_FUNC_DECL:
            ast_dump_indent(depth + 1);
            printf("name: "); ast_dump(node->data.func.name, 0);
            if (node->data.func.return_type) {
                ast_dump_indent(depth + 1);
                printf("return: "); ast_dump(node->data.func.return_type, 0);
            }
            dump_list(&node->data.func.params, depth + 1, "params");
            if (node->data.func.body) {
                ast_dump(node->data.func.body, depth + 1);
            }
            break;
        case NODE_LET:
            ast_dump_indent(depth + 1);
            printf("name: "); ast_dump(node->data.let_decl.name, 0);
            if (node->data.let_decl.type) {
                ast_dump_indent(depth + 1);
                printf("type: "); ast_dump(node->data.let_decl.type, 0);
            }
            if (node->data.let_decl.value) {
                ast_dump_indent(depth + 1);
                printf("= "); ast_dump(node->data.let_decl.value, 0);
            }
            break;
        case NODE_BLOCK:
            dump_list(&node->data.list, depth + 1, "stmts");
            break;
        case NODE_IDENT:
            printf(" '%.*s'", (int)node->data.ident.name.len, node->data.ident.name.data);
            printf("\n");
            break;
        case NODE_LITERAL_INT:
            printf(" %llu\n", (unsigned long long)node->data.literal.int_val);
            break;
        case NODE_LITERAL_FLOAT:
            printf(" %f\n", node->data.literal.float_val);
            break;
        case NODE_LITERAL_STRING:
            printf(" \"%.*s\"\n", (int)node->data.literal.string_val.len, node->data.literal.string_val.data);
            break;
        case NODE_LITERAL_CHAR:
            printf(" '%c'\n", (unsigned char)node->data.literal.char_val);
            break;
        case NODE_LITERAL_BOOL:
            printf(" %s\n", node->data.literal.bool_val ? "true" : "false");
            break;
        case NODE_LITERAL_NONE:
            printf("\n");
            break;
        case NODE_BINARY_OP:
            ast_dump_indent(depth + 1);
            printf("op: %s\n", binop_name(node->data.binary.op));
            ast_dump(node->data.binary.left, depth + 1);
            ast_dump(node->data.binary.right, depth + 1);
            break;
        case NODE_UNARY_OP:
            ast_dump_indent(depth + 1);
            printf("op: %s\n", unaryop_name(node->data.unary.op));
            ast_dump(node->data.unary.operand, depth + 1);
            break;
        case NODE_CALL:
            ast_dump_indent(depth + 1);
            printf("callee: "); ast_dump(node->data.call.callee, 0);
            dump_list(&node->data.call.args, depth + 1, "args");
            break;
        case NODE_IF:
            ast_dump_indent(depth + 1); printf("cond: "); ast_dump(node->data.if_node.condition, 0);
            ast_dump_indent(depth + 1); printf("then: "); ast_dump(node->data.if_node.then_block, 0);
            if (node->data.if_node.elif_chain) {
                ast_dump_indent(depth + 1); printf("elif: "); ast_dump(node->data.if_node.elif_chain, 0);
            }
            if (node->data.if_node.else_block) {
                ast_dump_indent(depth + 1); printf("else: "); ast_dump(node->data.if_node.else_block, 0);
            }
            break;
        case NODE_WHILE:
            ast_dump_indent(depth + 1); printf("cond: "); ast_dump(node->data.while_node.condition, 0);
            ast_dump_indent(depth + 1); printf("body: "); ast_dump(node->data.while_node.body, 0);
            break;
        case NODE_FOR:
            ast_dump_indent(depth + 1); printf("var: "); ast_dump(node->data.for_node.var, 0);
            ast_dump_indent(depth + 1); printf("in: "); ast_dump(node->data.for_node.iterable, 0);
            ast_dump_indent(depth + 1); printf("body: "); ast_dump(node->data.for_node.body, 0);
            break;
        case NODE_RETURN:
            if (node->data.return_node.value) {
                ast_dump_indent(depth + 1);
                ast_dump(node->data.return_node.value, 0);
            } else {
                printf("\n");
            }
            break;
        case NODE_MATCH:
            ast_dump_indent(depth + 1); printf("value: "); ast_dump(node->data.match_node.value, 0);
            dump_list(&node->data.match_node.arms, depth + 1, "arms");
            break;
        case NODE_MATCH_ARM:
            ast_dump_indent(depth + 1); printf("pattern: "); ast_dump(node->data.match_arm.pattern, 0);
            if (node->data.match_arm.body) {
                ast_dump_indent(depth + 1); printf("body: "); ast_dump(node->data.match_arm.body, 0);
            }
            break;
        case NODE_ASM_BLOCK:
            if (node->data.asm_block.text) {
                ast_dump_indent(depth + 1);
                ast_dump(node->data.asm_block.text, 0);
            }
            break;
        case NODE_STRUCT_DECL:
        case NODE_CLASS_DECL:
            ast_dump_indent(depth + 1); printf("name: "); ast_dump(node->data.struct_decl.name, 0);
            dump_list(&node->data.struct_decl.fields, depth + 1, "fields");
            break;
        case NODE_ENUM_DECL:
            ast_dump_indent(depth + 1); printf("name: "); ast_dump(node->data.enum_decl.name, 0);
            dump_list(&node->data.enum_decl.variants, depth + 1, "variants");
            break;
        case NODE_TYPE_NAMED:
            printf(" '%.*s'\n", (int)node->data.type_node.name.len, node->data.type_node.name.data);
            break;
        case NODE_TYPE_PRIMITIVE:
            printf(" %s\n", primtype_name(node->data.type_node.prim));
            break;
        default:
            printf("\n");
            break;
    }
}
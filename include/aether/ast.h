#ifndef AETHER_AST_H
#define AETHER_AST_H

#include "defs.h"
#include "lexer.h"
#include "arena.h"
#include "vector.h"

/*
 * AST node types for the Aether language.
 * All nodes are arena-allocated. No individual frees.
 */

/* Forward declaration */
typedef struct AstNode AstNode;

/* ================================================================
 * Node types
 * ================================================================ */

typedef enum {
    /* Top-level declarations */
    NODE_PROGRAM,
    NODE_FUNC_DECL,
    NODE_PARAM,
    NODE_STRUCT_DECL,
    NODE_FIELD,
    NODE_ENUM_DECL,
    NODE_ENUM_VARIANT,
    NODE_CONST_DECL,
    NODE_IMPORT,
    NODE_MODULE_DECL,
    NODE_TYPE_ALIAS,
    NODE_TRAIT_DECL,
    NODE_IMPL_BLOCK,
    NODE_CLASS_DECL,
    NODE_POOL_DECL,
    NODE_PROTOCOL_DECL,

    /* Statements */
    NODE_BLOCK,
    NODE_LET,
    NODE_ASSIGN,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_RETURN,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_DEFER,
    NODE_EXPR_STMT,
    NODE_MATCH,
    NODE_MATCH_ARM,
    NODE_TRY,
    NODE_THROW,
    NODE_CATCH_ARM,

    /* Expressions */
    NODE_LITERAL_INT,
    NODE_LITERAL_FLOAT,
    NODE_LITERAL_STRING,
    NODE_LITERAL_CHAR,
    NODE_LITERAL_BOOL,
    NODE_LITERAL_NONE,
    NODE_IDENT,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_CALL,
    NODE_INDEX,
    NODE_SLICE,
    NODE_FIELD_ACCESS,
    NODE_CAST,
    NODE_TERNARY,
    NODE_LAMBDA,
    NODE_ARRAY_LIT,

    /* Types */
    NODE_TYPE_PRIMITIVE,
    NODE_TYPE_NAMED,
    NODE_TYPE_ARRAY,
    NODE_TYPE_SLICE,
    NODE_TYPE_TUPLE,
    NODE_TYPE_OPTIONAL,
    NODE_TYPE_REF,
    NODE_TYPE_PTR,
    NODE_TYPE_FN,
    NODE_TYPE_INFER,

    /* Special */
    NODE_ASM_BLOCK,
    NODE_UNSAFE,
    NODE_ATTR,
    NODE_REGION,
    NODE_RUN_BLOCK,
    NODE_PROPERTY,
} NodeType;

/* ================================================================
 * Binary operator types
 * ================================================================ */

typedef enum {
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_MOD,
    BIN_EQ, BIN_NEQ, BIN_LT, BIN_GT, BIN_LE, BIN_GE,
    BIN_AND, BIN_OR,
    BIN_BIT_AND, BIN_BIT_OR, BIN_BIT_XOR,
    BIN_SHL, BIN_SHR,
    BIN_ASSIGN, BIN_ADD_ASSIGN, BIN_SUB_ASSIGN,
    BIN_MUL_ASSIGN, BIN_DIV_ASSIGN,
    BIN_RANGE, BIN_RANGE_INCLUSIVE,
    BIN_CONCAT,  /* string concatenation */
    BIN_OR_ELSE, /* optional unwrap: x or default */
    BIN_POWER,   /* ** power operator */
} BinOp;

/* ================================================================
 * Unary operator types
 * ================================================================ */

typedef enum {
    UNARY_NEG, UNARY_NOT, UNARY_BIT_NOT,
    UNARY_REF, UNARY_DEREF, UNARY_ADDR,
    UNARY_OWNED, UNARY_MUT, UNARY_HEAP,
    UNARY_INC, UNARY_DEC, UNARY_ARRAY_LEN,
} UnaryOp;

/* ================================================================
 * Primitive types
 * ================================================================ */

typedef enum {
    PRIM_VOID, PRIM_BOOL, PRIM_BYTE,
    PRIM_U8, PRIM_U16, PRIM_U32, PRIM_U64,
    PRIM_I8, PRIM_I16, PRIM_I32, PRIM_I64,
    PRIM_F32, PRIM_F64,
    PRIM_STRING,
} PrimType;

/* ================================================================
 * Node data (tagged union)
 * ================================================================ */

typedef struct {
    AstNode **items;
    int count;
    int cap;
} AstNodeList;

/* Function declaration */
typedef enum {
    ACCESS_PUB,
    ACCESS_PRIVATE,
    ACCESS_INTERNAL,
} AccessLevel;

typedef struct {
    AstNode *name;          /* ident node */
    AstNodeList params;     /* list of PARAM nodes */
    AstNode *return_type;   /* type node (NULL for void) */
    AstNode *body;          /* BLOCK node (NULL for extern) */
    bool is_asm;            /* asm func */
    bool is_sys;            /* sys func */
    bool is_pub;            /* public */
    AccessLevel access;      /* pub / private / internal */
    bool is_static;         /* static */
    int sys_index;          /* syscall table index (-1 if not sys) */
    AstNodeList defer_list; /* deferred bodies for LIFO scope exit */
    AstNodeList type_params; /* generic type parameter identifiers */
    bool is_throws;         /* function has throws return */
    bool is_operator;       /* true if this is an operator overload (op_add, etc.) */
    bool is_exported;       /* true if marked with @export */
    bool is_kernel_layout;  /* true if marked with @kernel_layout */
    bool is_inline;         /* true if 'inline' keyword used */
    bool is_force_inline;   /* true if @force_inline attribute set */
    bool is_no_inline;      /* true if @no_inline attribute set */
    int64_t entry_addr;     /* load address from @entry(addr), -1 if not set */
    bool has_layout;        /* true if @layout attribute is set */
    uint64_t layout_start;  /* start address from @layout(start=N) */
    uint64_t layout_max;    /* max size from @layout(max=N) */
    int layout_bits;        /* bits mode from @layout(bits=N): 16, 32, or 64; 0=default(64) */
    int layout_signature;   /* boot signature word from @layout(signature=N), 0=none */
    StringView layout_file; /* output filename from @layout(file="name") */
    AstNodeList pre_conditions;  /* pre(expr) contract expressions */
    AstNodeList post_conditions; /* post(expr) contract expressions */
    /* @Test(expect: N) — test annotation */
    bool has_test;            /* true if @Test attribute is set */
    int64_t test_expect_int;  /* numeric expect value (exit code), -1 if not set */
    StringView test_expect_str; /* string expect value (empty if not set) */
} FuncDecl;

/* Parameter */
typedef struct {
    AstNode *name;
    AstNode *type;
    bool is_mut;
    bool is_varargs;
    AstNode *default_value; /* NULL if no default */
} Param;

/* Return statement */
typedef struct {
    AstNode *value;         /* NULL if no value (bare return) */
} ReturnNode;

/* Let declaration */
typedef struct {
    AstNode *name;
    AstNode *type;          /* NULL for inferred */
    AstNode *value;
    bool is_mut;
    bool is_global;         /* true for file-scope let declarations */
} LetDecl;

/* If/elif/else */
typedef struct {
    AstNode *condition;
    AstNode *then_block;
    AstNode *elif_chain;    /* linked list: elif->elif->else */
    AstNode *else_block;    /* NULL if no else */
    bool is_if_let;         /* true if this is an if-let pattern binding */
    AstNode *if_let_pattern; /* the pattern in if-let (stored in elif_chain hack for compat) */
} IfNode;

/* While loop */
typedef struct {
    AstNode *condition;
    AstNode *body;
} WhileNode;

/* For loop */
typedef struct {
    AstNode *var;           /* loop variable */
    AstNode *iterable;      /* range or collection */
    AstNode *body;
    AstNode *index_var;     /* index variable for for i, val in arr (NULL if not present) */
} ForNode;

/* Match */
typedef struct {
    AstNode *value;         /* matched expression */
    AstNodeList arms;       /* list of MATCH_ARM */
} MatchNode;

/* Match arm */
typedef struct {
    AstNode *pattern;       /* pattern expression */
    AstNodeList guards;     /* guard conditions (empty if none) */
    AstNode *body;          /* result expression */
} MatchArm;

/* Binary operation */
typedef struct {
    BinOp op;
    AstNode *left;
    AstNode *right;
} BinaryOpNode;

/* Unary operation */
typedef struct {
    UnaryOp op;
    AstNode *operand;
} UnaryOpNode;

/* Function call */
typedef struct {
    AstNode *callee;
    AstNodeList args;
} CallNode;

/* Identifier */
typedef struct {
    StringView name;
    AstNode *resolved;      /* points to declaration (set by semantic analysis) */
} IdentNode;

/* Literal */
typedef struct {
    union {
        uint64_t int_val;
        double float_val;
        StringView string_val;
        uint32_t char_val;
        bool bool_val;
    };
    bool is_negative;       /* for negative int literals */
} LiteralNode;

/* Index expression: a[i] */
typedef struct {
    AstNode *target;
    AstNode *index;
} IndexNode;

/* Slice expression: a[i..j] */
typedef struct {
    AstNode *target;
    AstNode *start;         /* NULL means 'from beginning' */
    AstNode *end;           /* NULL means 'to end' */
} SliceNode;

/* Field access: a.b */
typedef struct {
    AstNode *target;
    AstNode *field;         /* ident node */
} FieldAccess;

/* Type node */
typedef struct {
    PrimType prim;          /* for PRIMITIVE types */
    AstNode *elem_type;     /* for ARRAY/SLICE/PTR/OPTIONAL */
    AstNodeList tuple_types; /* for TUPLE types */
    AstNode *return_type;   /* for FN types */
    AstNodeList param_types; /* for FN types */
    StringView name;        /* for NAMED types */
    bool is_ref;            /* ref T */
    bool is_owned;          /* owned T */
    bool is_rc;             /* rc T */
    bool is_mut;            /* mut T */
    int array_size;         /* for fixed-size arrays (-1 if unknown) */
} TypeNode;

/* Lambda/closure */
typedef struct {
    AstNodeList params;
    AstNode *return_type;
    AstNode *body;
    AstNode *captures;      /* captured variables (set by semantic analysis) */
} LambdaNode;

/* Array literal: [1, 2, 3] */
typedef struct {
    AstNodeList elements;
} ArrayLit;

/* Struct declaration */
typedef struct {
    AstNode *name;
    AstNodeList fields;
    AstNodeList methods;    /* method function decls */
    bool is_pub;
} StructDecl;

/* Enum declaration */
typedef struct {
    AstNode *name;
    AstNodeList variants;
    bool is_pub;
} EnumDecl;

/* Enum variant (can carry data) */
typedef struct {
    AstNode *name;
    AstNodeList payload_types;  /* types of data carried */
} EnumVariant;

/* Trait declaration */
typedef struct {
    AstNode *name;              /* trait name */
    AstNodeList methods;        /* function declarations (signatures only) */
    bool is_pub;
} TraitDecl;

/* Impl block: impl TraitName for TypeName { methods } */
typedef struct {
    StringView trait_name;      /* the trait being implemented */
    StringView type_name;       /* the type implementing it */
    AstNodeList methods;        /* method bodies */
} ImplBlock;

/* Inline assembly */
typedef struct {
    AstNode *text;          /* STRING_LITERAL containing the asm text */
    AstNodeList outputs;    /* output bindings */
    AstNodeList inputs;     /* input bindings */
    AstNodeList clobbers;   /* clobbered registers */
} AsmBlock;

/* Defer statement */
typedef struct {
    AstNode *body;          /* deferred block or statement */
} DeferNode;

/* Region allocation block */
typedef struct {
    StringView name;        /* region name */
    AstNode *body;          /* region body (BLOCK) */
} RegionNode;

/* Try block: try { body } catch Type(var) { handler } ... finally { body } */
typedef struct {
    AstNode *body;          /* the try body (BLOCK) */
    AstNodeList catch_arms; /* list of CATCH_ARM nodes */
    AstNode *finally_body;  /* finally block (BLOCK), NULL if no finally */
} TryNode;

/* Throw statement: throw expr */
typedef struct {
    AstNode *value;         /* the thrown value */
} ThrowNode;

/* Catch arm: catch Type(var) { handler } or catch _ { handler } */
typedef struct {
    AstNode *type;          /* the caught error type (TYPE_NAMED), NULL for catch-all */
    AstNode *var;           /* the variable to bind (IDENT), NULL if no binding */
    AstNode *body;          /* handler block */
    bool is_catch_all;      /* true if this is catch _ { } */
} CatchArm;

/* Attribute: @name or @name(payload) or @name(key=val, ...) */
typedef struct {
    StringView name;        /* attribute name (e.g. "export", "entry") */
    int64_t int_value;      /* for numeric payloads like @entry(addr), -1 if not set */
    /* @layout(start=N, max=M, bits=N, signature=N, file="name") params */
    bool has_layout_start;
    int64_t layout_start;
    bool has_layout_max;
    int64_t layout_max;
    int layout_bits;         /* 16, 32, or 64; default 64 */
    int layout_signature;    /* boot signature word (e.g. 0xAA55), 0=none */
    StringView layout_file; /* empty if not set */
    /* @module_abi(version=N) params */
    bool has_module_abi;
    int64_t module_abi_version; /* ABI version from @module_abi(version=N), -1 if not set */
    /* @Test(expect: N) params — test annotation */
    bool has_test;
    int64_t test_expect_int;     /* numeric expect value (exit code) */
    StringView test_expect_str;  /* string expect value (empty if not set) */
} AttrData;

/* Module declaration */
typedef struct {
    AstNode *name;          /* ident node for the module name */
    AstNodeList body;       /* declarations inside the module body */
    int module_abi_version; /* ABI version from @module_abi(version=N), -1 if unset */
} ModuleDecl;

/* Pool declaration: pool Name of size N, count M, alignment A */
typedef struct {
    AstNode *name;          /* pool name */
    uint64_t size;          /* element size */
    uint64_t count;         /* number of elements */
    uint64_t alignment;     /* alignment requirement (0 = default) */
} PoolDecl;

/* Protocol declaration: protocol Name { func_decls } */
typedef struct {
    AstNode *name;          /* protocol name */
    AstNodeList methods;    /* function declarations (signatures only) */
} ProtocolDecl;

/* ================================================================
 * AST Node structure (tagged union of all the above)
 * ================================================================ */

struct AstNode {
    NodeType type;
    Location loc;
    union {
        FuncDecl func;
        Param param;
        LetDecl let_decl;
        IfNode if_node;
        WhileNode while_node;
        ForNode for_node;
        MatchNode match_node;
        MatchArm match_arm;
        ReturnNode return_node;
        BinaryOpNode binary;
        UnaryOpNode unary;
        CallNode call;
        IdentNode ident;
        LiteralNode literal;
        IndexNode index;
        SliceNode slice;
        FieldAccess field;
        TypeNode type_node;
        LambdaNode lambda;
        ArrayLit array_lit;
        StructDecl struct_decl;
        EnumDecl enum_decl;
        EnumVariant enum_variant;
        AsmBlock asm_block;
        DeferNode defer_node;
        RegionNode region_node;
        TraitDecl trait_decl;
        ImplBlock impl_block;
        ModuleDecl module_decl;
        PoolDecl pool_decl;
        ProtocolDecl protocol_decl;
        TryNode try_node;
        ThrowNode throw_node;
        CatchArm catch_arm;
        AttrData attr;
        AstNodeList list;   /* generic list for blocks */
    } data;
};

/* ================================================================
 * AST creation functions
 * All return arena-allocated nodes.
 * ================================================================ */

AstNode *node_create(Arena *a, NodeType type, Location loc);

/* Convenience constructors */
AstNode *node_program(Arena *a);
AstNode *node_func_decl(Arena *a, Location loc, AstNode *name, bool is_pub, bool is_static);
AstNode *node_param(Arena *a, Location loc, AstNode *name, AstNode *type, bool is_mut, bool is_varargs);
AstNode *node_let(Arena *a, Location loc, AstNode *name, AstNode *type, AstNode *value, bool is_mut);
AstNode *node_if(Arena *a, Location loc, AstNode *cond, AstNode *then_block, AstNode *elif_chain, AstNode *else_block);
AstNode *node_while(Arena *a, Location loc, AstNode *cond, AstNode *body);
AstNode *node_for(Arena *a, Location loc, AstNode *var, AstNode *iterable, AstNode *body);
AstNode *node_return(Arena *a, Location loc, AstNode *value);
AstNode *node_break(Arena *a, Location loc);
AstNode *node_continue(Arena *a, Location loc);
AstNode *node_block(Arena *a, Location loc);
AstNode *node_ident(Arena *a, Location loc, StringView name);
AstNode *node_int_literal(Arena *a, Location loc, uint64_t val);
AstNode *node_float_literal(Arena *a, Location loc, double val);
AstNode *node_string_literal(Arena *a, Location loc, StringView val);
AstNode *node_char_literal(Arena *a, Location loc, uint32_t val);
AstNode *node_bool_literal(Arena *a, Location loc, bool val);
AstNode *node_none_literal(Arena *a, Location loc);
AstNode *node_binary(Arena *a, Location loc, BinOp op, AstNode *left, AstNode *right);
AstNode *node_unary(Arena *a, Location loc, UnaryOp op, AstNode *operand);
AstNode *node_call(Arena *a, Location loc, AstNode *callee);
AstNode *node_index(Arena *a, Location loc, AstNode *target, AstNode *index);
AstNode *node_field_access(Arena *a, Location loc, AstNode *target, AstNode *field);
AstNode *node_type_prim(Arena *a, Location loc, PrimType prim);
AstNode *node_type_named(Arena *a, Location loc, StringView name);
AstNode *node_match(Arena *a, Location loc, AstNode *value);
AstNode *node_match_arm(Arena *a, Location loc, AstNode *pattern, AstNode *body);
AstNode *node_asm_block(Arena *a, Location loc, AstNode *text);
AstNode *node_struct_decl(Arena *a, Location loc, AstNode *name, bool is_pub);
AstNode *node_enum_decl(Arena *a, Location loc, AstNode *name, bool is_pub);
AstNode *node_defer(Arena *a, Location loc, AstNode *body);
AstNode *node_region(Arena *a, Location loc, StringView name, AstNode *body);
AstNode *node_expr_stmt(Arena *a, Location loc, AstNode *expr);
AstNode *node_try(Arena *a, Location loc, AstNode *body);
AstNode *node_throw(Arena *a, Location loc, AstNode *value);
AstNode *node_catch_arm(Arena *a, Location loc, AstNode *type, AstNode *var, AstNode *body, bool is_catch_all);

/* List helpers */
void node_list_append(AstNodeList *list, AstNode *node);
void node_list_append_sorted(AstNodeList *list, AstNode *node);

/* ================================================================
 * AST utilities
 * ================================================================ */

/* Pretty-print an AST node (for debugging) */
void ast_dump(AstNode *node, int depth);
void ast_dump_indent(int depth);

/* Get a human-readable name for a node type */
const char *node_type_name(NodeType type);
const char *binop_name(BinOp op);
const char *unaryop_name(UnaryOp op);
const char *primtype_name(PrimType pt);

#endif /* AETHER_AST_H */
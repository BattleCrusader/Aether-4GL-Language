#include "aether/codegen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define INITIAL_CAP 65536

/* ================================================================
 * Struct field layout tracker
 * ================================================================ */

/* Forward declaration needed by struct layout builder */
static int type_size(AstNode *type);

typedef struct FieldLayout {
    const char *name;          /* field name */
    int offset;                /* byte offset from struct base */
    int size;                  /* field size */
    struct FieldLayout *next;
} FieldLayout;

typedef struct StructLayout {
    const char *name;          /* struct type name */
    int total_size;            /* total bytes */
    FieldLayout *fields;       /* linked list of fields */
    struct StructLayout *next;
} StructLayout;

static StructLayout *struct_layouts = NULL; /* global list of known structs */

/* Build struct layout from a STRUCT_DECL node */
static StructLayout *build_struct_layout(Arena *a, AstNode *node) {
    if (node->type != NODE_STRUCT_DECL) return NULL;
    
    StructLayout *sl = (StructLayout *)arena_alloc(a, sizeof(StructLayout));
    sl->name = arena_strndup(a,
        node->data.struct_decl.name->data.ident.name.data,
        node->data.struct_decl.name->data.ident.name.len);
    
    int offset = 0;
    FieldLayout *prev = NULL;
    
    for (int i = 0; i < node->data.struct_decl.fields.count; i++) {
        AstNode *field = node->data.struct_decl.fields.items[i];
        FieldLayout *fl = (FieldLayout *)arena_alloc(a, sizeof(FieldLayout));
        fl->name = arena_strndup(a,
            field->data.param.name->data.ident.name.data,
            field->data.param.name->data.ident.name.len);
        fl->size = type_size(field->data.param.type);
        fl->offset = offset;
        offset += fl->size;
        fl->next = NULL;
        
        if (prev) prev->next = fl;
        else sl->fields = fl;
        prev = fl;
    }
    
    sl->total_size = offset;
    sl->next = struct_layouts;
    struct_layouts = sl;
    return sl;
}

/* Look up a struct layout by name */
static StructLayout *find_struct_layout(const char *name) {
    for (StructLayout *sl = struct_layouts; sl; sl = sl->next) {
        if (strcmp(sl->name, name) == 0) return sl;
    }
    return NULL;
}

/* Look up a field offset within a struct */
static int find_field_offset(StructLayout *sl, const char *field_name) {
    for (FieldLayout *fl = sl->fields; fl; fl = fl->next) {
        if (strcmp(fl->name, field_name) == 0) return fl->offset;
    }
    return 0;
}

typedef struct VarSlot {
    AstNode *node;           /* the LET or PARAM node this slot belongs to */
    const char *name;        /* debug: variable name */
    int stack_offset;        /* negative offset from rbp */
    int size;                /* bytes allocated (rounded up to 8) */
    int actual_size;         /* actual type size before rounding */
    PrimType prim;           /* PRIM_VOID if not primitive / unknown */
    struct VarSlot *next;    /* chain in current function */
} VarSlot;

/* ================================================================
 * Type helpers
 * ================================================================ */

static int type_size(AstNode *type) {
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
    }
    return 8; /* default pointer-sized */
}

/* Extract PrimType from a type annotation node */
static PrimType prim_from_type(AstNode *type) {
    if (!type || type->type != NODE_TYPE_PRIMITIVE) return PRIM_VOID;
    return type->data.type_node.prim;
}

/* ================================================================ */

/* ================================================================ */

Codegen *codegen_create(Arena *a) {
    Codegen *cg = (Codegen *)arena_alloc(a, sizeof(Codegen));
    if (!cg) return NULL;
    cg->arena = a;
    cg->output = (char *)arena_alloc(a, INITIAL_CAP);
    cg->output_len = 0;
    cg->output_cap = INITIAL_CAP;
    cg->label_counter = 0;
    cg->indent_level = 0;
    cg->current_func = NULL;
    return cg;
}

void codegen_destroy(Codegen *cg) {
    (void)cg;
}

static void cg_inst(Codegen *cg, const char *inst);
static void cg_inst1(Codegen *cg, const char *inst, const char *arg);

/* ================================================================
 * Output helpers
 * ================================================================ */

static void cg_write(Codegen *cg, const char *s) {
    size_t len = strlen(s);
    if (cg->output_len + len + 1 > cg->output_cap) return;
    memcpy(cg->output + cg->output_len, s, len);
    cg->output_len += len;
}

static void cg_write_fmt(Codegen *cg, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) cg_write(cg, buf);
}

static void cg_indent(Codegen *cg) { cg_write(cg, "    "); }

static void cg_inst(Codegen *cg, const char *inst) {
    cg_indent(cg); cg_write_fmt(cg, "%s\n", inst);
}

static void cg_inst1(Codegen *cg, const char *inst, const char *arg) {
    cg_indent(cg); cg_write_fmt(cg, "%-8s %s\n", inst, arg);
}

static void cg_inst2(Codegen *cg, const char *inst, const char *a1, const char *a2) {
    cg_indent(cg); cg_write_fmt(cg, "%-8s %s, %s\n", inst, a1, a2);
}

static int string_label_counter = 0;

static int cg_new_label(Codegen *cg) { return cg->label_counter++; }

/* String data tracker for .rodata emission */
typedef struct StringEntry {
    StringView sv;
    int label_num;
    struct StringEntry *next;
} StringEntry;

static StringEntry *string_entries = NULL;

/* Emit a section .rodata entry for a string literal and return its label */
static const char *cg_emit_string(Codegen *cg, StringView sv) {
    int n = string_label_counter++;
    char *label = (char *)arena_alloc(cg->arena, 64);
    snprintf(label, 64, ".Lstr%d", n);
    
    /* Track for later emission */
    StringEntry *e = (StringEntry *)arena_alloc(cg->arena, sizeof(StringEntry));
    e->sv = sv;
    e->label_num = n;
    e->next = string_entries;
    string_entries = e;
    
    return label;
}

static void cg_dump_rodata(Codegen *cg); /* forward */

static void cg_comment(Codegen *cg, const char *s) {
    cg_write_fmt(cg, "; %s\n", s);
}

/* Emit load from stack slot with correct register width */
static void cg_load_var(Codegen *cg, int offset, int actual_size) {
    char buf[64];
    switch (actual_size) {
        case 1: snprintf(buf, sizeof(buf), "movzx rax, byte [rbp%+d]", offset); break;
        case 2: snprintf(buf, sizeof(buf), "movzx rax, word [rbp%+d]", offset); break;
        case 4: snprintf(buf, sizeof(buf), "mov eax, [rbp%+d]", offset); break;
        default: snprintf(buf, sizeof(buf), "mov rax, [rbp%+d]", offset); break;
    }
    cg_inst(cg, buf);
}

/* Emit store to stack slot with correct register width */
static void cg_store_var(Codegen *cg, int offset, int actual_size) {
    char buf[64];
    switch (actual_size) {
        case 1: snprintf(buf, sizeof(buf), "mov byte [rbp%+d], al", offset); break;
        case 2: snprintf(buf, sizeof(buf), "mov word [rbp%+d], ax", offset); break;
        case 4: snprintf(buf, sizeof(buf), "mov dword [rbp%+d], eax", offset); break;
        default: snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", offset); break;
    }
    cg_inst(cg, buf);
}

/* ================================================================
 * Stack frame computation
 * ================================================================ */

/* Walk a function's body and collect all variable declarations,
   computing their stack offsets. Returns the total frame size. */
static VarSlot *compute_frame(Arena *a, AstNode *func, int *out_frame_size);

static void frame_collect(Arena *a, AstNode *node, VarSlot **list, int *offset) {
    if (!node) return;

    switch (node->type) {
        case NODE_BLOCK:
            for (int i = 0; i < node->data.list.count; i++)
                frame_collect(a, node->data.list.items[i], list, offset);
            break;

        case NODE_LET: {
            int raw_size = 8;
            PrimType ptype = PRIM_VOID;
            if (node->data.let_decl.type) {
                raw_size = type_size(node->data.let_decl.type);
                ptype = prim_from_type(node->data.let_decl.type);
            }
            int vsize = (raw_size + 7) & ~7; /* align to 8 */

            *offset += vsize;

            VarSlot *slot = (VarSlot *)arena_alloc(a, sizeof(VarSlot));
            slot->node = node;
            slot->name = arena_strndup(a,
                node->data.let_decl.name->data.ident.name.data,
                node->data.let_decl.name->data.ident.name.len);
            slot->stack_offset = -(*offset);
            slot->size = vsize;
            slot->actual_size = raw_size;
            slot->prim = ptype;
            slot->next = *list;
            *list = slot;

            /* Visit initializer for nested allocations */
            if (node->data.let_decl.value)
                frame_collect(a, node->data.let_decl.value, list, offset);
            break;
        }

        case NODE_IF:
            frame_collect(a, node->data.if_node.condition, list, offset);
            frame_collect(a, node->data.if_node.then_block, list, offset);
            if (node->data.if_node.elif_chain)
                frame_collect(a, node->data.if_node.elif_chain, list, offset);
            if (node->data.if_node.else_block)
                frame_collect(a, node->data.if_node.else_block, list, offset);
            break;

        case NODE_WHILE:
            frame_collect(a, node->data.while_node.condition, list, offset);
            frame_collect(a, node->data.while_node.body, list, offset);
            break;

        case NODE_FOR:
            frame_collect(a, node->data.for_node.var, list, offset);
            frame_collect(a, node->data.for_node.iterable, list, offset);
            frame_collect(a, node->data.for_node.body, list, offset);
            break;

        default:
            break;
    }
}

static VarSlot *compute_frame(Arena *a, AstNode *func, int *out_frame_size) {
    VarSlot *slots = NULL;
    int offset = 0; /* grows negative (stack goes down) */

    /* Allocate slots for parameters */
    for (int i = 0; i < func->data.func.params.count; i++) {
        AstNode *param = func->data.func.params.items[i];
        int psize = 8;
        if (param->data.param.type)
            psize = type_size(param->data.param.type);
        psize = (psize + 7) & ~7;
        offset += psize;

        VarSlot *slot = (VarSlot *)arena_alloc(a, sizeof(VarSlot));
        slot->node = param;
        slot->name = arena_strndup(a,
            param->data.param.name->data.ident.name.data,
            param->data.param.name->data.ident.name.len);
        slot->stack_offset = -offset;
        slot->size = psize;
        slot->actual_size = psize;
        slot->prim = prim_from_type(param->data.param.type);
        slot->next = slots;
        slots = slot;
    }

    /* Walk body for let declarations */
    if (func->data.func.body)
        frame_collect(a, func->data.func.body, &slots, &offset);

    /* Align frame to 16 bytes (SysV ABI) */
    offset = (offset + 15) & ~15;

    *out_frame_size = offset;
    return slots;
}

/* Look up a variable's stack offset by its AST node */
static int find_var_offset(VarSlot *slots, AstNode *node) {
    while (slots) {
        if (slots->node == node) return slots->stack_offset;
        slots = slots->next;
    }
    return -8; /* fallback */
}

static int find_var_offset_by_name(VarSlot *slots, const char *name) {
    while (slots) {
        if (strcmp(slots->name, name) == 0) return slots->stack_offset;
        slots = slots->next;
    }
    return -8;
}

static int find_var_size(VarSlot *slots, AstNode *node) {
    while (slots) {
        if (slots->node == node) return slots->actual_size;
        slots = slots->next;
    }
    return 8;
}

static int find_var_size_by_name(VarSlot *slots, const char *name) {
    while (slots) {
        if (strcmp(slots->name, name) == 0) return slots->actual_size;
        slots = slots->next;
    }
    return 8;
}

/* ================================================================
 * Expression codegen
 * ================================================================ */

static void cg_expr(Codegen *cg, AstNode *node, VarSlot *slots);

static void cg_expr(Codegen *cg, AstNode *node, VarSlot *slots) {
    if (!node) return;

    switch (node->type) {
        case NODE_LITERAL_INT: {
            uint64_t val = node->data.literal.int_val;
            char buf[32];
            snprintf(buf, sizeof(buf), "mov rax, %llu", (unsigned long long)val);
            cg_inst(cg, buf);
            break;
        }

        case NODE_LITERAL_BOOL:
            cg_inst1(cg, "mov", node->data.literal.bool_val ? "rax, 1" : "rax, 0");
            break;

        case NODE_LITERAL_NONE:
            cg_inst1(cg, "xor", "rax, rax");
            break;

        case NODE_LITERAL_STRING: {
            /* Emit lea to string in .rodata */
            const char *label = cg_emit_string(cg, node->data.literal.string_val);
            char buf[128];
            snprintf(buf, sizeof(buf), "lea rax, [rel %s]", label);
            cg_inst(cg, buf);
            break;
        }

        case NODE_LITERAL_CHAR:
            cg_inst1(cg, "movzx", "eax, byte 0");
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "mov rax, %u", (unsigned)node->data.literal.char_val);
                cg_inst(cg, buf);
            }
            break;

        case NODE_IDENT: {
            int offset = -8;
            int actual_size = 8;
            if (node->data.ident.resolved) {
                offset = find_var_offset(slots, node->data.ident.resolved);
                actual_size = find_var_size(slots, node->data.ident.resolved);
            } else {
                const char *vname = arena_strndup(cg->arena,
                    node->data.ident.name.data, node->data.ident.name.len);
                offset = find_var_offset_by_name(slots, vname);
                actual_size = find_var_size_by_name(slots, vname);
            }
            cg_load_var(cg, offset, actual_size);
            break;
        }

        case NODE_FIELD_ACCESS: {
            /* s.field — load based on struct var's stack offset + field offset */
            cg_comment(cg, "field access");
            if (node->data.field.target && node->data.field.target->type == NODE_IDENT) {
                int var_offset = -8;
                if (node->data.field.target->data.ident.resolved)
                    var_offset = find_var_offset(slots, node->data.field.target->data.ident.resolved);
                
                /* Find the struct type of this variable */
                const char *struct_name = NULL;
                AstNode *decl = node->data.field.target->data.ident.resolved;
                if (decl && (decl->type == NODE_LET || decl->type == NODE_PARAM)) {
                    AstNode *type_node = decl->type == NODE_LET ? decl->data.let_decl.type : decl->data.param.type;
                    if (type_node && type_node->type == NODE_TYPE_NAMED) {
                        struct_name = arena_strndup(cg->arena, type_node->data.type_node.name.data, type_node->data.type_node.name.len);
                    }
                }
                
                if (struct_name) {
                    StructLayout *sl = find_struct_layout(struct_name);
                    if (sl && node->data.field.field && node->data.field.field->type == NODE_IDENT) {
                        int field_off = find_field_offset(sl,
                            arena_strndup(cg->arena,
                                node->data.field.field->data.ident.name.data,
                                node->data.field.field->data.ident.name.len));
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov rax, [rbp%+d]", var_offset + field_off);
                        cg_inst(cg, buf);
                    }
                }
            }
            break;
        }

        case NODE_BINARY_OP: {
            /* Right side first (push), then left (in rax).
               After this block: rax=left, rcx=right */
            cg_expr(cg, node->data.binary.right, slots);
            cg_inst1(cg, "push", "rax");

            cg_expr(cg, node->data.binary.left, slots);
            cg_inst1(cg, "pop", "rcx");   /* rcx = right, rax = left */

            switch (node->data.binary.op) {
                case BIN_ADD: cg_inst1(cg, "add",  "rax, rcx"); break;
                case BIN_SUB: cg_inst1(cg, "sub",  "rax, rcx"); break;
                case BIN_MUL: cg_inst1(cg, "mul",  "rcx"); break;
                case BIN_DIV: cg_inst(cg, "xor rdx, rdx"); cg_inst1(cg, "div", "rcx"); break;
                case BIN_MOD: cg_inst(cg, "xor rdx, rdx"); cg_inst1(cg, "div", "rcx"); cg_inst1(cg, "mov", "rax, rdx"); break;
                case BIN_EQ:  cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "sete al");  cg_inst(cg, "movzx rax, al"); break;
                case BIN_NEQ: cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "setne al"); cg_inst(cg, "movzx rax, al"); break;
                case BIN_LT:  cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "setl al");  cg_inst(cg, "movzx rax, al"); break;
                case BIN_GT:  cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "setg al");  cg_inst(cg, "movzx rax, al"); break;
                case BIN_LE:  cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "setle al"); cg_inst(cg, "movzx rax, al"); break;
                case BIN_GE:  cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "setge al"); cg_inst(cg, "movzx rax, al"); break;
                case BIN_AND: cg_inst1(cg, "test", "rax, rax"); cg_inst(cg, "jz .Lfalse"); cg_inst1(cg, "test", "rcx, rcx"); cg_inst(cg, "setnz al"); cg_inst(cg, "movzx rax, al"); break;
                case BIN_OR:  cg_inst1(cg, "test", "rax, rax"); cg_inst(cg, "jnz .Ltrue"); cg_inst1(cg, "test", "rcx, rcx"); cg_inst(cg, "setnz al"); cg_inst(cg, "movzx rax, al"); break;
                case BIN_RANGE: cg_comment(cg, "range"); cg_inst1(cg, "mov", "rax, rcx"); break;
                default: cg_inst1(cg, "add", "rax, rcx"); break;
            }
            break;
        }

        case NODE_UNARY_OP: {
            cg_expr(cg, node->data.unary.operand, slots);
            switch (node->data.unary.op) {
                case UNARY_NEG: cg_inst1(cg, "neg", "rax"); break;
                case UNARY_NOT: cg_inst(cg, "test rax, rax"); cg_inst(cg, "sete al"); cg_inst(cg, "movzx rax, al"); break;
                case UNARY_BIT_NOT: cg_inst1(cg, "not", "rax"); break;
                default: break;
            }
            break;
        }

        case NODE_CALL: {
            cg_comment(cg, "function call");
            int argc = node->data.call.args.count;

            /* Push args in reverse order (rightmost first) for stack-based */
            for (int i = argc - 1; i >= 0; i--) {
                cg_expr(cg, node->data.call.args.items[i], slots);
                cg_inst1(cg, "push", "rax");
            }

            if (node->data.call.callee->type == NODE_IDENT) {
                char buf[256];
                snprintf(buf, sizeof(buf), "%.*s",
                    (int)node->data.call.callee->data.ident.name.len,
                    node->data.call.callee->data.ident.name.data);
                cg_inst1(cg, "call", buf);
            }

            /* Clean up args from stack */
            if (argc > 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "add rsp, %d", argc * 8);
                cg_inst(cg, buf);
            }
            break;
        }

        case NODE_ASSIGN:
            /* Assignment: left = right — handled as BIN_ASSIGN */
            break;

        default:
            cg_inst1(cg, "mov", "rax, 0");
            break;
    }
}

/* ================================================================
 * Statement codegen
 * ================================================================ */

static void cg_stmt(Codegen *cg, AstNode *node, VarSlot *slots);

static void cg_stmt(Codegen *cg, AstNode *node, VarSlot *slots) {
    if (!node) return;

    switch (node->type) {
        case NODE_RETURN: {
            if (node->data.return_node.value) {
                cg_expr(cg, node->data.return_node.value, slots);
            }
            cg_inst1(cg, "mov", "rsp, rbp");
            cg_inst1(cg, "pop", "rbp");
            cg_inst(cg, "ret");
            break;
        }

        case NODE_LET: {
            int offset = find_var_offset(slots, node);
            int actual_size = find_var_size(slots, node);
            if (node->data.let_decl.value) {
                cg_expr(cg, node->data.let_decl.value, slots);
                cg_store_var(cg, offset, actual_size);
            } else {
                /* Zero-initialize */
                char buf[64];
                snprintf(buf, sizeof(buf), "mov qword [rbp%+d], 0", offset);
                cg_inst(cg, buf);
            }
            break;
        }

        case NODE_EXPR_STMT: {
            cg_expr(cg, node->data.call.callee, slots);
            break;
        }

        case NODE_BLOCK: {
            for (int i = 0; i < node->data.list.count; i++) {
                cg_stmt(cg, node->data.list.items[i], slots);
            }
            break;
        }

        case NODE_IF: {
            int else_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);

            cg_expr(cg, node->data.if_node.condition, slots);
            cg_inst1(cg, "test", "rax, rax");
            cg_write_fmt(cg, "    jz .L%x\n", else_label);

            if (node->data.if_node.then_block)
                cg_stmt(cg, node->data.if_node.then_block, slots);

            cg_write_fmt(cg, "    jmp .L%x\n", end_label);
            cg_write_fmt(cg, ".L%x:\n", else_label);

            /* elif chain */
            if (node->data.if_node.elif_chain)
                cg_stmt(cg, node->data.if_node.elif_chain, slots);

            if (node->data.if_node.else_block)
                cg_stmt(cg, node->data.if_node.else_block, slots);

            cg_write_fmt(cg, ".L%x:\n", end_label);
            break;
        }

        case NODE_WHILE: {
            int start_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);
            cg_write_fmt(cg, ".L%x:\n", start_label);
            cg_expr(cg, node->data.while_node.condition, slots);
            cg_inst1(cg, "test", "rax, rax");
            cg_write_fmt(cg, "    jz .L%x\n", end_label);
            if (node->data.while_node.body)
                cg_stmt(cg, node->data.while_node.body, slots);
            cg_write_fmt(cg, "    jmp .L%x\n", start_label);
            cg_write_fmt(cg, ".L%x:\n", end_label);
            break;
        }

        case NODE_FOR: {
            /* Basic: for var in start..end { body } */
            int start_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);

            /* If for has iterable, evaluate it */
            if (node->data.for_node.iterable) {
                /* Range literal: we need separate start and end */
                cg_comment(cg, "for loop");
                /* Iterable is BIN_RANGE: left=start, right=end */
                if (node->data.for_node.iterable->type == NODE_BINARY_OP) {
                    AstNode *range = node->data.for_node.iterable;
                    /* Emit start value to rcx for loop counter */
                    cg_expr(cg, range->data.binary.left, slots);
                    cg_inst1(cg, "mov", "rcx, rax");

                    cg_write_fmt(cg, ".L%x:\n", start_label);
                    /* Compare counter with end */
                    cg_expr(cg, range->data.binary.right, slots);
                    cg_inst1(cg, "cmp", "rcx, rax");
                    cg_write_fmt(cg, "    jge .L%x\n", end_label);

                    /* If there's a loop variable, store counter to it */
                    if (node->data.for_node.var) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov [rbp-8], rcx"); /* placeholder */
                        cg_inst(cg, buf);
                    }

                    /* Body */
                    if (node->data.for_node.body)
                        cg_stmt(cg, node->data.for_node.body, slots);

                    /* Increment counter */
                    cg_inst1(cg, "inc", "rcx");
                    cg_write_fmt(cg, "    jmp .L%x\n", start_label);
                    cg_write_fmt(cg, ".L%x:\n", end_label);
                }
            }
            break;
        }

        case NODE_DEFER: {
            cg_comment(cg, "defer (NYI - treated as regular block)");
            break;
        }

        default:
            break;
    }
}

/* ================================================================
 * Function-level code generation (2-pass: frame then body)
 * ================================================================ */

static void cg_func(Codegen *cg, AstNode *func) {
    int frame_size = 0;
    VarSlot *slots = compute_frame(cg->arena, func, &frame_size);

    char fn_label[256];
    snprintf(fn_label, sizeof(fn_label), "%.*s",
        (int)func->data.func.name->data.ident.name.len,
        func->data.func.name->data.ident.name.data);

    cg_write_fmt(cg, "; function %s (frame=%d)\n", fn_label, frame_size);
    cg_write_fmt(cg, "global %s\n", fn_label);
    cg_write_fmt(cg, "%s:\n", fn_label);

    /* Prologue */
    cg_inst1(cg, "push", "rbp");
    cg_inst1(cg, "mov", "rbp, rsp");

    /* Allocate stack frame */
    if (frame_size > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "sub rsp, %d", frame_size);
        cg_inst(cg, buf);
    }

    /* Save incoming args from registers to their stack slots */
    for (int i = 0; i < func->data.func.params.count && i < 6; i++) {
        const char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        AstNode *param = func->data.func.params.items[i];
        int offset = find_var_offset(slots, param);
        char buf[64];
        snprintf(buf, sizeof(buf), "mov [rbp%+d], %s", offset, regs[i]);
        cg_inst(cg, buf);
    }
    /* Args > 6 are on the stack already; they're at rbp+16, rbp+24, etc.
       For now we only handle up to 6 args. */

    /* Body */
    if (func->data.func.body) {
        cg_stmt(cg, func->data.func.body, slots);
    }

    /* Default return */
    cg_comment(cg, "default return");
    cg_inst1(cg, "mov", "rsp, rbp");
    cg_inst1(cg, "pop", "rbp");
    cg_inst(cg, "ret");
    cg_write(cg, "\n");
}

/* ================================================================
 * Top-level code generation
 * ================================================================ */

const char *codegen_generate(Codegen *cg, AstNode *program) {
    cg_comment(cg, "Generated by Aether Compiler Phase 1");
    cg_write(cg, "\n");

    /* Build struct layouts from top-level declarations */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *node = program->data.list.items[i];
        if (node->type == NODE_STRUCT_DECL) {
            build_struct_layout(cg->arena, node);
        }
    }

    cg_write(cg, "bits 64\n");
    cg_write(cg, "default rel\n\n");

    cg_write(cg, "section .text\n\n");

    /* Entry point */
    cg_write(cg, "global _start\n");
    cg_write(cg, "_start:\n");
    cg_inst1(cg, "mov", "rbp, rsp");
    cg_comment(cg, "call main()");
    cg_inst(cg, "call main");
    cg_comment(cg, "syscall exit(rdi = rax)");
    cg_inst1(cg, "mov", "rdi, rax");
    cg_inst1(cg, "mov", "rax, 60");
    cg_inst(cg, "syscall");
    cg_comment(cg, "halt");
    cg_inst(cg, "hlt");
    cg_write(cg, "\n");

    /* Generate each function */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *node = program->data.list.items[i];
        if (node->type == NODE_FUNC_DECL) {
            cg->current_func = node;
            cg_func(cg, node);
        }
    }

    /* Emit string table */
    cg_write(cg, "section .rodata\n");
    for (StringEntry *e = string_entries; e; e = e->next) {
        cg_write_fmt(cg, ".Lstr%d: db \"", e->label_num);
        /* Escape the string for NASM */
        for (size_t i = 0; i < e->sv.len; i++) {
            char c = e->sv.data[i];
            if (c == '"') cg_write(cg, "\\\"");
            else if (c == '\\') cg_write(cg, "\\\\");
            else if (c == '\n') cg_write(cg, "\\n");
            else if (c == '\t') cg_write(cg, "\\t");
            else if (c >= 32 && c < 127) { char buf[2] = {c, 0}; cg_write(cg, buf); }
            else { cg_write_fmt(cg, "\\x%02x", (unsigned char)c); }
        }
        cg_write(cg, "\", 0\n");
    }

    cg->output[cg->output_len] = '\0';
    return cg->output;
}

int codegen_write_asm(Codegen *cg, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    const char *asm_text = codegen_get_asm(cg);
    if (asm_text) fputs(asm_text, f);
    fclose(f);
    return 0;
}

const char *codegen_get_asm(Codegen *cg) {
    return cg->output;
}
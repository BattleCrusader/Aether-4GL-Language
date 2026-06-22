#include "aether/codegen.h"
#include "aether/str.h"
#include "aether/asm_ir.h"
#include "aether/asm_parser.h"
#include "aether/universal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdbool.h>

#define INITIAL_CAP 65536

/* Forward declarations */
typedef struct VarSlot VarSlot;
typedef struct AutoDrop AutoDrop;
static int type_size(AstNode *type);
static void cg_stmt(Codegen *cg, AstNode *node, VarSlot *slots);
static void cg_defer_push(Codegen *cg, AstNode *body);
static void cg_emit_defers(Codegen *cg, VarSlot *slots);
static void cg_defer_clear(Codegen *cg);

/* Auto-drop entry for class destructors (linked list) */
struct AutoDrop {
    const char *class_name;
    int stack_offset;
    AutoDrop *next;
};

/* Source location entry — tracked during codegen, emitted as a table at the end */
typedef struct SrcLocEntry SrcLocEntry;
struct SrcLocEntry {
    int label_num;       /* Lsrc_<label_num> in .text */
    int line;
    int col;
    SrcLocEntry *next;
};

/* Process a raw string literal token value into actual bytes.
 * Strips surrounding quotes, processes escape sequences.
 * Returns the number of bytes written to out_buf (max max_len). */
static int process_string_literal(StringView raw, char *out_buf, int max_len) {
    int di = 0;
    int i = 0;
    if (i < (int)raw.len && raw.data[i] == '"') i++;
    while (i < (int)raw.len && di < max_len) {
        char c = raw.data[i];
        if (c == '"') break;
        if (c == '\\' && i + 1 < (int)raw.len) {
            i++;
            switch (raw.data[i]) {
                case 'n':  out_buf[di++] = '\n'; break;
                case 'r':  out_buf[di++] = '\r'; break;
                case 't':  out_buf[di++] = '\t'; break;
                case '\\': out_buf[di++] = '\\'; break;
                case '"':  out_buf[di++] = '"';  break;
                case 'x': {
                    if (i + 2 < (int)raw.len && isxdigit((unsigned char)raw.data[i+1]) && isxdigit((unsigned char)raw.data[i+2])) {
                        char hex[3] = {raw.data[i+1], raw.data[i+2], 0};
                        out_buf[di++] = (char)strtoul(hex, NULL, 16);
                        i += 2;
                    }
                    break;
                }
                default:   out_buf[di++] = c; break;
            }
        } else {
            out_buf[di++] = c;
        }
        i++;
    }
    return di;
}

/* ================================================================
 * Target detection
 * ================================================================ */

Target codegen_detect_host(void) {
#if defined(__APPLE__) && defined(__MACH__)
    return TARGET_MACHO64;
#elif defined(__linux__)
    return TARGET_ELF64_HOST;
#else
    return TARGET_FREESTANDING;
#endif
}

const char *target_name(Target t) {
    switch (t) {
        case TARGET_FREESTANDING: return "freestanding ELF64";
        case TARGET_MACHO64:      return "Mach-O 64 (macOS)";
        case TARGET_HOST:         return "host (auto-detect)";
        case TARGET_ELF64_HOST:   return "native ELF64 (Linux)";
        case TARGET_KERNEL:       return "kernel ELF64 (0x1000000)";
        case TARGET_MODULE:       return "Aether OS module (.ko)";
        case TARGET_BINARY:       return "Aether OS binary ELF64 (0x2000000)";
        case TARGET_BOOT:         return "flat binary boot sector";
        case TARGET_ASM_X86_64:  return "x86_64 NASM assembly listing";
        case TARGET_ASM_ARM64:   return "ARM64 assembly listing";
        case TARGET_ASM_RISCV64: return "RISC-V assembly listing";
        case TARGET_UNIVERSAL:   return "universal binary (x86_64 + ARM64)";
        case TARGET_UNIVERSAL_ALL: return "universal binary (all architectures)";
    }
    return "unknown";
}

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
    bool is_class;             /* true if declared with 'class', not 'struct' */
    struct StructLayout *next;
} StructLayout;

static StructLayout *struct_layouts = NULL;
static StructLayout *enum_layouts = NULL;  /* enums are tracked as layouts too */

/* Build struct layout from a STRUCT_DECL node */
static StructLayout *build_struct_layout(Arena *a, AstNode *node) {
    if (node->type != NODE_STRUCT_DECL && node->type != NODE_CLASS_DECL) return NULL;
    
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
    sl->is_class = (node->type == NODE_CLASS_DECL);
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

/* ================================================================
 * Enum layout tracker — tagged union = 8-byte discriminant + max payload
 * Variant names mapped to discriminant values for codegen
 * ================================================================ */

typedef struct VariantEntry {
    const char *name;
    int discriminant;
    struct VariantEntry *next;
} VariantEntry;

static VariantEntry *variant_entries = NULL;

static StructLayout *build_enum_layout(Arena *a, AstNode *node) {
    if (node->type != NODE_ENUM_DECL) return NULL;
    
    StructLayout *sl = (StructLayout *)arena_alloc(a, sizeof(StructLayout));
    sl->name = arena_strndup(a,
        node->data.enum_decl.name->data.ident.name.data,
        node->data.enum_decl.name->data.ident.name.len);
    
    /* Enum layout: [discriminant: u64] [payload: max_variant_size]
       Discriminant is always 8 bytes, payload is the max of all variant sizes */
    int max_payload = 0;
    for (int i = 0; i < node->data.enum_decl.variants.count; i++) {
        AstNode *var = node->data.enum_decl.variants.items[i];
        int var_size = 0;
        for (int j = 0; j < var->data.enum_variant.payload_types.count; j++) {
            var_size += type_size(var->data.enum_variant.payload_types.items[j]);
        }
        if (var_size > max_payload) max_payload = var_size;
    }
    
    /* Build field-like entries for the compiler */
    FieldLayout *disc = (FieldLayout *)arena_alloc(a, sizeof(FieldLayout));
    disc->name = "__disc";
    disc->offset = 0;
    disc->size = 8;
    disc->next = NULL;
    sl->fields = disc;
    
    sl->total_size = 8 + max_payload;
    sl->next = enum_layouts;
    enum_layouts = sl;
    
    /* Register variant names for codegen lookup */
    for (int i = 0; i < node->data.enum_decl.variants.count; i++) {
        AstNode *var = node->data.enum_decl.variants.items[i];
        VariantEntry *ve = (VariantEntry *)arena_alloc(a, sizeof(VariantEntry));
        ve->name = arena_strndup(a,
            var->data.enum_variant.name->data.ident.name.data,
            var->data.enum_variant.name->data.ident.name.len);
        ve->discriminant = i;
        ve->next = variant_entries;
        variant_entries = ve;
    }
    return sl;
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
static PrimType prim_from_type(AstNode *type) {
    if (!type || type->type != NODE_TYPE_PRIMITIVE) return PRIM_VOID;
    return type->data.type_node.prim;
}

/* Check if an expression node evaluates to a numeric type (u8-u64, i8-i64, bool, byte) */
static bool is_numeric_expr(AstNode *node) {
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
static bool is_string_expr(AstNode *node) {
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
        /* No type annotation — check if initializer is a string expression */
        if (!type_node && decl->type == NODE_LET && decl->data.let_decl.value) {
            return is_string_expr(decl->data.let_decl.value);
        }
    }
    return false;
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
    cg->target = TARGET_FREESTANDING;
    cg->defer_stack = NULL;
    cg->defer_count = 0;
    cg->auto_drops = NULL;
    cg->entry_addr = 0;
    cg->entry_func = NULL;
    cg->has_layout = false;
    cg->layout_start = 0;
    cg->layout_max = 0;
    cg->layout_bits = 0;
    cg->layout_signature = 0;
    cg->layout_file = NULL;
    cg->linker_script = NULL;
    cg->cleanup_depth = 0;
    return cg;
}

void codegen_set_target(Codegen *cg, Target target) {
    if (target == TARGET_HOST) {
        cg->target = codegen_detect_host();
    } else {
        cg->target = target;
    }
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
/* Note: processes raw string (strips quotes, handles escapes) */
static const char *cg_emit_string(Codegen *cg, StringView sv) {
    /* Process the string: strip quotes, decode escapes */
    char processed[8192];
    int plen = process_string_literal(sv, processed, sizeof(processed) - 1);
    processed[plen] = '\0';

    int n = string_label_counter++;
    char *label = (char *)arena_alloc(cg->arena, 64);
    snprintf(label, 64, "Lstr%d", n);

    /* Allocate and copy processed data */
    char *data = (char *)arena_alloc(cg->arena, (size_t)(plen + 1));
    memcpy(data, processed, (size_t)plen);
    data[plen] = '\0';

    /* Track for later emission */
    StringEntry *e = (StringEntry *)arena_alloc(cg->arena, sizeof(StringEntry));
    e->sv.data = data;
    e->sv.len = (size_t)plen;
    e->label_num = n;
    e->next = string_entries;
    string_entries = e;

    return label;
}

static void cg_dump_rodata(Codegen *cg); /* forward */

static void cg_comment(Codegen *cg, const char *s) {
    cg_write_fmt(cg, "; %s\n", s);
}

/* Report a codegen error with source location */
static void cg_error(Codegen *cg, AstNode *node, const char *msg) {
    fprintf(stderr, "Codegen error at %s:%d:%d: %s\n",
        node->loc.file ? node->loc.file : "?",
        node->loc.line, node->loc.col, msg);
    (void)cg; /* keep compiling, emit zero */
}

/* Report a codegen warning (non-fatal) */
static void cg_warn(Codegen *cg, AstNode *node, const char *msg) {
    fprintf(stderr, "Codegen warning at %s:%d:%d: %s\n",
        node->loc.file ? node->loc.file : "?",
        node->loc.line, node->loc.col, msg);
}

/* Emit a source location entry for the segfault handler.
 * Emits a table entry: dq $ (current address, linker-resolved), dd line, dd col
 * Only emits for host targets (has segfault handler). */
static void cg_source_loc(Codegen *cg, AstNode *node) {
    if (!node) return;
    if (cg->target != TARGET_MACHO64 && cg->target != TARGET_ELF64_HOST) return;
    int line = node->loc.line;
    int col = node->loc.col;
    if (line <= 0) return;
    cg_write_fmt(cg, "  dq $\n");
    cg_write_fmt(cg, "  dd %d, %d\n", line, col);
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

        case NODE_TRY:
            frame_collect(a, node->data.try_node.body, list, offset);
            for (int i = 0; i < node->data.try_node.catch_arms.count; i++) {
                AstNode *arm = node->data.try_node.catch_arms.items[i];
                if (arm->data.catch_arm.var)
                    frame_collect(a, arm->data.catch_arm.var, list, offset);
                if (arm->data.catch_arm.body)
                    frame_collect(a, arm->data.catch_arm.body, list, offset);
            }
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

/* Find a VarSlot by AST node pointer */
static VarSlot *find_var_slot(VarSlot *slots, AstNode *node) {
    while (slots) {
        if (slots->node == node) return slots;
        slots = slots->next;
    }
    return NULL;
}

/* Emit cleanup for a range of scope depths (from saved_depth to current_depth).
 * This calls destructors and defers for objects created in the try body. */
static void cg_emit_cleanup_range(Codegen *cg, VarSlot *slots, int saved_depth, int current_depth) {
    (void)cg;
    (void)slots;
    (void)saved_depth;
    (void)current_depth;
    /* For now, this is a placeholder. The full implementation will walk
     * the cleanup table and emit destructor calls + defer blocks.
     * Phase A of the proper try/catch implementation. */
    cg_comment(cg, "cleanup range (placeholder)");
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
            /* Check if this ident resolves to a const declaration */
            if (node->data.ident.resolved && node->data.ident.resolved->type == NODE_CONST_DECL) {
                /* Const value was folded to a literal by semantic analysis */
                AstNode *val = node->data.ident.resolved->data.let_decl.value;
                if (val && val->type == NODE_LITERAL_INT) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "mov rax, %llu",
                        (unsigned long long)val->data.literal.int_val);
                    cg_inst(cg, buf);
                    break;
                }
                cg_inst1(cg, "mov", "rax, 0");
                break;
            }
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

        case NODE_INDEX: {
            /* array[index] — compute element address */
            cg_comment(cg, "array index");
            /* Push index, then array base address, compute offset = base + index * elem_size */
            cg_expr(cg, node->data.index.index, slots);
            cg_inst1(cg, "push", "rax");
            cg_expr(cg, node->data.index.target, slots);
            cg_inst1(cg, "pop", "rcx");   /* rcx = index */
            /* Compute element offset: rax = base, rcx = index, result = base + index * elem_size */
            /* Determine element size from the target type */
            int elem_size = 8; /* default */
            if (node->data.index.target->type == NODE_TYPE_ARRAY && node->data.index.target->data.type_node.elem_type) {
                elem_size = type_size(node->data.index.target->data.type_node.elem_type);
            } else if (node->data.index.target->type == NODE_TYPE_SLICE && node->data.index.target->data.type_node.elem_type) {
                elem_size = type_size(node->data.index.target->data.type_node.elem_type);
            } else if (node->data.index.target->type == NODE_IDENT) {
                /* Try to infer from the variable's declared type */
                /* For now, check if the variable has a type annotation */
                /* We'll look up the let declaration */
            }
            /* Scale index by element size */
            switch (elem_size) {
                case 1: cg_inst(cg, "add rax, rcx"); break;
                case 2: cg_inst(cg, "shl rcx, 1"); cg_inst1(cg, "add", "rax, rcx"); break;
                case 4: cg_inst(cg, "shl rcx, 2"); cg_inst1(cg, "add", "rax, rcx"); break;
                case 8: cg_inst(cg, "shl rcx, 3"); cg_inst1(cg, "add", "rax, rcx"); break;
                default:
                    cg_inst(cg, "shl rcx, 3"); cg_inst1(cg, "add", "rax, rcx"); break;
            }
            cg_inst(cg, "mov rax, [rax]");
            break;
        }

        case NODE_SLICE: {
            cg_warn(cg, node, "slice not yet implemented");
            cg_inst1(cg, "xor", "rax, rax");
            break;
        }

        case NODE_ARRAY_LIT: {
            cg_warn(cg, node, "array literal not yet implemented");
            cg_inst1(cg, "xor", "rax, rax");
            break;
        }

        case NODE_BINARY_OP: {
            /* Right side first (push), then left (in rax).
               After this block: rax=left, rcx=right */
            cg_expr(cg, node->data.binary.right, slots);
            cg_inst1(cg, "push", "rax");

            /* For plain assignment, skip left evaluation — it would overwrite rax.
               For compound assignments (+= etc.), evaluate left to get current value. */
            if (node->data.binary.op == BIN_ASSIGN) {
                /* Assignment: right side is in rax (pushed above), pop it back */
                cg_inst1(cg, "pop", "rax");
            } else {
                cg_expr(cg, node->data.binary.left, slots);
                cg_inst1(cg, "pop", "rcx");   /* rcx = right, rax = left */
            }

            switch (node->data.binary.op) {
                case BIN_ADD: {
                    /* If either operand is a string, do concat instead of numeric add */
                    bool left_str = is_string_expr(node->data.binary.left);
                    bool right_str = is_string_expr(node->data.binary.right);
                    if (left_str || right_str) {
                        cg_comment(cg, "string concat (+)");
                        /* rax = left, rcx = right. Save rcx before itoa (clobbers rcx). */
                        bool left_num = is_numeric_expr(node->data.binary.left);
                        bool right_num = is_numeric_expr(node->data.binary.right);
                        if (left_num || right_num) {
                            cg_inst1(cg, "push", "rcx");
                        }
                        if (left_num) {
                            cg_inst1(cg, "mov", "rdi, rax");
                            cg_inst(cg, "call __aether_itoa");
                        }
                        if (right_num) {
                            cg_inst1(cg, "push", "rax");
                            cg_inst1(cg, "pop", "rdi");
                            cg_inst1(cg, "pop", "rax");
                            cg_inst1(cg, "push", "rdi");
                            cg_inst1(cg, "mov", "rdi, rax");
                            cg_inst(cg, "call __aether_itoa");
                            cg_inst1(cg, "mov", "rcx, rax");
                            cg_inst1(cg, "pop", "rax");
                        } else if (left_num) {
                            cg_inst1(cg, "pop", "rcx");
                        }
                        cg_inst1(cg, "push", "rax");
                        cg_inst1(cg, "push", "rcx");
                        cg_inst(cg, "call __aether_concat");
                        cg_inst(cg, "add rsp, 16");
                        break;
                    }
                    cg_inst1(cg, "add", "rax, rcx");
                    break;
                }
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
                /* Bitwise */
                case BIN_BIT_AND: cg_inst1(cg, "and", "rax, rcx"); break;
                case BIN_BIT_OR:  cg_inst1(cg, "or",  "rax, rcx"); break;
                case BIN_BIT_XOR: cg_inst1(cg, "xor", "rax, rcx"); break;
                case BIN_SHL:     cg_inst1(cg, "xchg", "rax, rcx"); cg_inst1(cg, "shl", "rax, cl"); break;
                case BIN_SHR:     cg_inst1(cg, "xchg", "rax, rcx"); cg_inst1(cg, "shr", "rax, cl"); break;
                /* Assignment: x = expr — store result to variable's stack slot */
                case BIN_ASSIGN: {
                    cg_comment(cg, "assign");
                    /* Right side already in rax, left side is the target ident */
                    if (node->data.binary.left && node->data.binary.left->type == NODE_IDENT) {
                        int off = find_var_offset_by_name(slots,
                            arena_strndup(cg->arena,
                                node->data.binary.left->data.ident.name.data,
                                node->data.binary.left->data.ident.name.len));
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", off);
                        cg_inst(cg, buf);
                    }
                    break;
                }
                /* Compound assignment: x += expr etc — left value in rax, right in rcx */
                case BIN_ADD_ASSIGN: cg_comment(cg, "+="); cg_inst1(cg, "add", "rax, rcx"); goto store_assign;
                case BIN_SUB_ASSIGN: cg_comment(cg, "-="); cg_inst1(cg, "sub", "rax, rcx"); goto store_assign;
                case BIN_MUL_ASSIGN: cg_comment(cg, "*="); cg_inst1(cg, "mul", "rcx"); goto store_assign;
                case BIN_DIV_ASSIGN: cg_comment(cg, "/="); cg_inst(cg, "xor rdx, rdx"); cg_inst1(cg, "div", "rcx"); goto store_assign;
                store_assign:
                    cg_comment(cg, "store compound assign");
                    if (node->data.binary.left && node->data.binary.left->type == NODE_IDENT) {
                        int off = find_var_offset_by_name(slots,
                            arena_strndup(cg->arena,
                                node->data.binary.left->data.ident.name.data,
                                node->data.binary.left->data.ident.name.len));
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", off);
                        cg_inst(cg, buf);
                    }
                    break;
                /* Logical (short-circuit emulated) */
                case BIN_AND: cg_inst1(cg, "test", "rax, rax"); cg_inst(cg, "jz L_false"); cg_inst1(cg, "test", "rcx, rcx"); cg_inst(cg, "setnz al"); cg_inst(cg, "movzx rax, al"); break;
                case BIN_OR:  cg_inst1(cg, "test", "rax, rax"); cg_inst(cg, "jnz L_true"); cg_inst1(cg, "test", "rcx, rcx"); cg_inst(cg, "setnz al"); cg_inst(cg, "movzx rax, al"); break;
                case BIN_RANGE: cg_comment(cg, "range"); cg_inst1(cg, "mov", "rax, rcx"); break;
                case BIN_CONCAT: {
                    cg_comment(cg, "string concat");
                    /* rax = left, rcx = right */
                    /* Auto-convert numeric operands to strings.
                       itoa clobbers rcx (div instruction), so save rcx first. */
                    bool left_num = is_numeric_expr(node->data.binary.left);
                    bool right_num = is_numeric_expr(node->data.binary.right);
                    if (left_num || right_num) {
                        cg_inst1(cg, "push", "rcx");     /* save right value */
                    }
                    if (left_num) {
                        cg_inst1(cg, "mov", "rdi, rax");
                        cg_inst(cg, "call __aether_itoa");
                        /* rax now = string pointer */
                    }
                    if (right_num) {
                        cg_inst1(cg, "push", "rax");     /* save left string */
                        cg_inst1(cg, "pop", "rdi");      /* rdi = left string (temp) */
                        cg_inst1(cg, "pop", "rax");      /* rax = right value */
                        cg_inst1(cg, "push", "rdi");     /* save left string on stack */
                        cg_inst1(cg, "mov", "rdi, rax"); /* convert right */
                        cg_inst(cg, "call __aether_itoa");
                        cg_inst1(cg, "mov", "rcx, rax"); /* rcx = right string */
                        cg_inst1(cg, "pop", "rax");      /* rax = left string */
                    } else if (left_num) {
                        cg_inst1(cg, "pop", "rcx");      /* restore rcx = right (already a string) */
                    }
                    /* Now rax = left (string), rcx = right (string) */
                    cg_inst1(cg, "push", "rax");   /* left arg (rbp+24) */
                    cg_inst1(cg, "push", "rcx");   /* right arg (rbp+16) */
                    cg_inst(cg, "call __aether_concat");
                    cg_inst(cg, "add rsp, 16");    /* pop both args */
                    break;
                }
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
                case UNARY_DEREF: cg_inst(cg, "mov rax, [rax]"); break;
                case UNARY_INC:
                    cg_comment(cg, "increment");
                    cg_inst(cg, "add rax, 1");
                    /* Store back to variable if operand is an ident */
                    if (node->data.unary.operand && node->data.unary.operand->type == NODE_IDENT) {
                        int off = find_var_offset_by_name(slots,
                            node->data.unary.operand->data.ident.name.data);
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", off);
                        cg_inst(cg, buf);
                    }
                    break;
                case UNARY_DEC:
                    cg_comment(cg, "decrement");
                    cg_inst(cg, "sub rax, 1");
                    /* Store back to variable if operand is an ident */
                    if (node->data.unary.operand && node->data.unary.operand->type == NODE_IDENT) {
                        int off = find_var_offset_by_name(slots,
                            node->data.unary.operand->data.ident.name.data);
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", off);
                        cg_inst(cg, buf);
                    }
                    break;
                case UNARY_HEAP: {
                    /* heap Expr — allocate, store result, return pointer */
                    cg_comment(cg, "heap alloc");
                    /* Save evaluated value to stack */
                    cg_inst1(cg, "push", "rax");
                    /* Allocate 8 bytes (pointer-sized) */
                    cg_inst1(cg, "mov", "rdi, 8");
                    cg_inst(cg, "call __aether_alloc");
                    /* Pop value into rcx, store to [rax] */
                    cg_inst1(cg, "pop", "rcx");
                    cg_inst(cg, "mov [rax], rcx");
                    /* rax now holds the heap pointer */
                    break;
                }
                default: break;
            }
            break;
        }

        case NODE_CALL: {
            /* Check for built-in functions on host targets */
            bool is_host = (cg->target == TARGET_MACHO64 || cg->target == TARGET_ELF64_HOST);
            if (is_host && node->data.call.callee->type == NODE_IDENT) {
                char fn_name[256];
                int nlen = (int)node->data.call.callee->data.ident.name.len;
                if (nlen > 255) nlen = 255;
                memcpy(fn_name, node->data.call.callee->data.ident.name.data, nlen);
                fn_name[nlen] = '\0';

                /* Built-in: print(string) — emit write syscall inline */
                if (strcmp(fn_name, "print") == 0 && node->data.call.args.count >= 1) {
                    AstNode *arg = node->data.call.args.items[0];
                    cg_comment(cg, "print() built-in");
                    if (arg->type == NODE_LITERAL_STRING) {
                        const char *label = cg_emit_string(cg, arg->data.literal.string_val);

                        /* Compute processed length */
                        char processed[8192];
                        int plen = process_string_literal(arg->data.literal.string_val, processed, sizeof(processed) - 1);

                        if (cg->target == TARGET_MACHO64) {
                            cg_inst1(cg, "mov", "rdi, 1");
                            cg_write_fmt(cg, "    lea rsi, [rel %s]\n", label);
                            cg_write_fmt(cg, "    mov rdx, %d\n", plen);
                            cg_inst1(cg, "mov", "rax, 0x2000004");
                            cg_inst(cg, "syscall");
                        } else if (cg->target == TARGET_ELF64_HOST) {
                            cg_inst1(cg, "mov", "rdi, 1");
                            cg_write_fmt(cg, "    lea rsi, [rel %s]\n", label);
                            cg_write_fmt(cg, "    mov rdx, %d\n", plen);
                            cg_inst1(cg, "mov", "rax, 1");
                            cg_inst(cg, "syscall");
                        } else {
                            /* Freestanding: print() is a no-op (kernel uses serial) */
                            cg_comment(cg, "freestanding: print() is a no-op");
                        }
                        cg_inst1(cg, "xor", "rax, rax");
                    } else {
                        /* Runtime string: evaluate expression, then call write syscall with strlen */
                        cg_comment(cg, "print() runtime string");
                        cg_expr(cg, arg, slots);
                        /* Auto-convert numeric to string for print() */
                        if (is_numeric_expr(arg)) {
                            cg_inst1(cg, "mov", "rdi, rax");
                            cg_inst(cg, "call __aether_itoa");
                        }
                        /* rax = string pointer, save it */
                        cg_inst1(cg, "push", "rax");
                        /* Compute strlen */
                        int sl_id = cg->label_counter++;
                        cg_inst(cg, "xor rcx, rcx");
                        cg_inst(cg, "mov rdi, rax");
                        cg_write_fmt(cg, ".strlen_loop_%d:\n", sl_id);
                        cg_write_fmt(cg, "    cmp byte [rdi + rcx], 0\n");
                        cg_write_fmt(cg, "    je .strlen_done_%d\n", sl_id);
                        cg_write_fmt(cg, "    inc rcx\n");
                        cg_write_fmt(cg, "    jmp .strlen_loop_%d\n", sl_id);
                        cg_write_fmt(cg, ".strlen_done_%d:\n", sl_id);
                        /* rcx = length, pop rsi = string pointer */
                        cg_inst1(cg, "pop", "rsi");
                        cg_inst1(cg, "mov", "rdx, rcx");
                        if (cg->target == TARGET_MACHO64) {
                            cg_inst1(cg, "mov", "rdi, 1");
                            cg_inst1(cg, "mov", "rax, 0x2000004");
                            cg_inst(cg, "syscall");
                        } else if (cg->target == TARGET_ELF64_HOST) {
                            cg_inst1(cg, "mov", "rdi, 1");
                            cg_inst1(cg, "mov", "rax, 1");
                            cg_inst(cg, "syscall");
                        } else {
                            cg_comment(cg, "freestanding: print() is a no-op");
                        }
                        cg_inst1(cg, "xor", "rax, rax");
                    }
                    break;
                }
            }

            /* Check for enum construction: EnumName::Variant(args) */
            if (node->data.call.callee->type == NODE_FIELD_ACCESS) {
                AstNode *target = node->data.call.callee->data.field.target;
                AstNode *field = node->data.call.callee->data.field.field;
                if (target && target->type == NODE_IDENT && field && field->type == NODE_IDENT) {
                    char enum_name[256], variant_name[256];
                    int nlen = (int)target->data.ident.name.len;
                    if (nlen > 255) nlen = 255; memcpy(enum_name, target->data.ident.name.data, nlen); enum_name[nlen] = '\0';
                    int vlen = (int)field->data.ident.name.len;
                    if (vlen > 255) vlen = 255; memcpy(variant_name, field->data.ident.name.data, vlen); variant_name[vlen] = '\0';
                    
                    /* Find the discriminant for this variant */
                    int disc_val = -1;
                    for (VariantEntry *ve = variant_entries; ve; ve = ve->next) {
                        if (strcmp(ve->name, variant_name) == 0) { disc_val = ve->discriminant; break; }
                    }
                    
                    if (disc_val >= 0) {
                        cg_comment(cg, "enum construction");
                        /* Build the tagged union in rax.
                           For now, just return the discriminant as a proxy value.
                           Full payload handling deferred to later phases. */
                        char buf[32];
                        snprintf(buf, sizeof(buf), "mov rax, %d", disc_val);
                        cg_inst(cg, buf);
                        break;
                    }
                }
            }

            cg_comment(cg, "function call");
            int argc = node->data.call.args.count;

            /* SysV AMD64 ABI: first 6 integer args in rdi, rsi, rdx, rcx, r8, r9
               For ≤6 args (Phase 1): evaluate right-to-left, push all, pop into regs */
            const char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            int reg_count = argc < 6 ? argc : 6;

            /* Evaluate args right-to-left, push each onto stack */
            for (int i = argc - 1; i >= 0; i--) {
                cg_expr(cg, node->data.call.args.items[i], slots);
                cg_inst1(cg, "push", "rax");
            }

            /* Pop into target registers rdi, rsi, rdx, rcx, r8, r9 */
            for (int i = 0; i < reg_count; i++) {
                cg_inst1(cg, "pop", regs[i]);
            }

            /* Extra stack args (>6) stay on stack — callee finds them at rsp+N */
            /* After call, count of remaining items = argc - reg_count, but if that's ≤0 it's fine */
            int stack_cleanup = argc > 6 ? argc - 6 : 0;

            if (node->data.call.callee->type == NODE_IDENT) {
                char buf[256];
                snprintf(buf, sizeof(buf), "%.*s",
                    (int)node->data.call.callee->data.ident.name.len,
                    node->data.call.callee->data.ident.name.data);

                /* Check if this is a sys func call — emit indirect call through 0x5000 table */
                if (node->data.call.callee->data.ident.resolved &&
                    node->data.call.callee->data.ident.resolved->type == NODE_FUNC_DECL &&
                    node->data.call.callee->data.ident.resolved->data.func.is_sys) {
                    int idx = node->data.call.callee->data.ident.resolved->data.func.sys_index;
                    if (idx >= 0) {
                        cg_comment(cg, "syscall via 0x5000 table");
                        char tmp[64];
                        snprintf(tmp, sizeof(tmp), "mov rax, 0x%x", 0x5000 + idx * 8);
                        cg_inst(cg, tmp);
                        cg_inst(cg, "call [rax]");
                    } else {
                        cg_inst1(cg, "call", buf);
                    }
                } else {
                    cg_inst1(cg, "call", buf);
                }
            }

            /* Clean up remaining stack args after call */
            if (stack_cleanup > 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "add rsp, %d", stack_cleanup * 8);
                cg_inst(cg, buf);
            }
            break;
        }

        case NODE_ASSIGN:
            /* Assignment: left = right — handled as BIN_ASSIGN */
            break;

        case NODE_LAMBDA: {
            /* Non-capturing lambda: generate a unique function, return its address.
             * Capturing lambdas (with env) deferred to later phase. */
            cg_comment(cg, "lambda");
            int lambda_id = cg_new_label(cg);
            char fn_name[64];
            snprintf(fn_name, sizeof(fn_name), "L_lambda_%x", lambda_id);

            /* Emit the lambda function body (will be placed in .text) */
            /* For now, just return 0 as placeholder — full lambda codegen deferred */
            cg_inst1(cg, "mov", "rax, 0");
            break;
        }

        default:
            if (node->type != NODE_LITERAL_FLOAT && node->type != NODE_MATCH_ARM)
                cg_warn(cg, node, "unsupported expression in codegen");
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

    /* Emit source location label for segfault handler (host targets only).
     * The label is in .text so its address is the instruction address.
     * Track it for the source map table emitted at the end. */
    if (cg->target == TARGET_MACHO64 || cg->target == TARGET_ELF64_HOST) {
        if (node->loc.line > 0) {
            cg->label_counter++;
            cg_write_fmt(cg, "_aether_src_%d:\n", cg->label_counter);
            /* Track this entry for the source map table */
            SrcLocEntry *e = (SrcLocEntry *)arena_alloc(cg->arena, sizeof(SrcLocEntry));
            e->label_num = cg->label_counter;
            e->line = node->loc.line;
            e->col = node->loc.col;
            e->next = NULL;
            /* Append to linked list */
            if (!cg->src_loc_list) {
                cg->src_loc_list = e;
            } else {
                SrcLocEntry *last = cg->src_loc_list;
                while (last->next) last = last->next;
                last->next = e;
            }
        }
    }

    switch (node->type) {
        case NODE_RETURN: {
            if (node->data.return_node.value) {
                cg_expr(cg, node->data.return_node.value, slots);
                /* Save return value; defers may clobber rax */
                cg_inst1(cg, "push", "rax");
            }
            /* Emit defers before returning */
            cg_emit_defers(cg, slots);
            if (node->data.return_node.value) {
                /* Restore return value */
                cg_inst1(cg, "pop", "rax");
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
            /* Auto-defer drop() for class-typed variables */
            if (node->data.let_decl.type && node->data.let_decl.type->type == NODE_TYPE_NAMED) {
                char tn[256];
                int nlen = (int)node->data.let_decl.type->data.type_node.name.len;
                if (nlen > 255) nlen = 255;
                memcpy(tn, node->data.let_decl.type->data.type_node.name.data, nlen);
                tn[nlen] = '\0';
                for (StructLayout *sl = struct_layouts; sl; sl = sl->next) {
                    if (strcmp(sl->name, tn) == 0 && sl->is_class) {
                        cg_comment(cg, "auto-defer drop");
                        /* Track auto-drop for scope exit emission */
                        AutoDrop *ad = (AutoDrop *)arena_alloc(cg->arena, sizeof(AutoDrop));
                        ad->class_name = arena_strndup(cg->arena, tn, (size_t)nlen);
                        ad->stack_offset = offset;
                        ad->next = cg->auto_drops;
                        cg->auto_drops = ad;
                        break;
                    }
                }
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
            /* if let pattern = expr { body } — check optional tag, bind value */
            if (node->data.if_node.is_if_let) {
                int end_lbl = cg_new_label(cg);
                cg_comment(cg, "if let");
                /* Evaluate the optional expression */
                cg_expr(cg, node->data.if_node.condition, slots);
                /* Check the tag byte: optional layout is [tag:byte][value] in a ptr-sized slot.
                   For "none" tag=0, "some" tag=1. Load byte, check if != 0. */
                cg_inst(cg, "mov rcx, rax");
                cg_inst(cg, "and rcx, 0xFF");
                cg_inst1(cg, "test", "rcx, rcx");
                cg_write_fmt(cg, "    jz L_%x\n", end_lbl);
                /* Tag is some (1) — run body */
                if (node->data.if_node.then_block)
                    cg_stmt(cg, node->data.if_node.then_block, slots);
                cg_write_fmt(cg, "L_%x:\n", end_lbl);
                break;
            }

            int else_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);

            cg_expr(cg, node->data.if_node.condition, slots);
            cg_inst1(cg, "test", "rax, rax");
            cg_write_fmt(cg, "    jz L_%x\n", else_label);

            if (node->data.if_node.then_block)
                cg_stmt(cg, node->data.if_node.then_block, slots);

            cg_write_fmt(cg, "    jmp L_%x\n", end_label);
            cg_write_fmt(cg, "L_%x:\n", else_label);

            /* elif chain */
            if (node->data.if_node.elif_chain)
                cg_stmt(cg, node->data.if_node.elif_chain, slots);

            if (node->data.if_node.else_block)
                cg_stmt(cg, node->data.if_node.else_block, slots);

            cg_write_fmt(cg, "L_%x:\n", end_label);
            break;
        }

        case NODE_WHILE: {
            int start_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);
            cg_write_fmt(cg, "L_%x:\n", start_label);
            cg_expr(cg, node->data.while_node.condition, slots);
            cg_inst1(cg, "test", "rax, rax");
            cg_write_fmt(cg, "    jz L_%x\n", end_label);
            if (node->data.while_node.body)
                cg_stmt(cg, node->data.while_node.body, slots);
            cg_write_fmt(cg, "    jmp L_%x\n", start_label);
            cg_write_fmt(cg, "L_%x:\n", end_label);
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

                    cg_write_fmt(cg, "L_%x:\n", start_label);
                    /* Compare counter with end */
                    cg_expr(cg, range->data.binary.right, slots);
                    cg_inst1(cg, "cmp", "rcx, rax");
                    cg_write_fmt(cg, "    jge L_%x\n", end_label);

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
                    cg_write_fmt(cg, "    jmp L_%x\n", start_label);
                    cg_write_fmt(cg, "L_%x:\n", end_label);
                }
            }
            break;
        }

        case NODE_MATCH: {
            int end_label = cg_new_label(cg);
            cg_comment(cg, "match");
            cg_expr(cg, node->data.match_node.value, slots);
            cg_inst1(cg, "push", "rax");  /* save matched value on stack */

            for (int i = 0; i < node->data.match_node.arms.count; i++) {
                AstNode *arm = node->data.match_node.arms.items[i];
                int next_label = cg_new_label(cg);

                /* Reload matched value */
                cg_inst1(cg, "mov", "rax, [rsp]");

                /* Check pattern against rax */
                AstNode *pat = arm->data.match_arm.pattern;
                bool is_wildcard = false;
                if (pat->type == NODE_IDENT && sv_eq_cstr(pat->data.ident.name, "_")) {
                    is_wildcard = true;
                } else if (pat->type == NODE_LITERAL_INT) {
                    char val_buf[32];
                    snprintf(val_buf, sizeof(val_buf), "%llu", (unsigned long long)pat->data.literal.int_val);
                    char tmp[64];
                    snprintf(tmp, sizeof(tmp), "cmp rax, %s", val_buf);
                    cg_inst(cg, tmp);
                    cg_write_fmt(cg, "    jne L_%x\n", next_label);
                }

                if (!is_wildcard) {
                    /* Emit arm body */
                    if (arm->data.match_arm.body)
                        cg_expr(cg, arm->data.match_arm.body, slots);
                    cg_write_fmt(cg, "    jmp L_%x\n", end_label);
                }

                cg_write_fmt(cg, "L_%x:\n", next_label);
                if (i == node->data.match_node.arms.count - 1) {
                    /* Last arm (wildcard or default) */
                    if (arm->data.match_arm.body)
                        cg_expr(cg, arm->data.match_arm.body, slots);
                }
            }

            cg_write_fmt(cg, "L_%x:\n", end_label);
            cg_inst1(cg, "add", "rsp, 8");  /* pop matched value */
            break;
        }

        case NODE_DEFER: {
            /* Push onto the current function's defer stack */
            if (node->data.defer_node.body) {
                cg_defer_push(cg, node->data.defer_node.body);
                cg_comment(cg, "defer pushed");
            }
            break;
        }

        case NODE_REGION: {
            int end_lbl = cg_new_label(cg);
            cg_comment(cg, "region begin");
            /* Save rsp, allocate 4KB arena on stack */
            cg_inst(cg, "mov r15, rsp");          /* save rsp */
            cg_inst(cg, "sub rsp, 4096");          /* arena on stack */
            cg_inst(cg, "mov [rel __aether_region_cur], rsp");
            cg_inst(cg, "lea rax, [rsp + 4096]");
            cg_inst(cg, "mov [rel __aether_region_end], rax");
            /* Run body */
            if (node->data.region_node.body)
                cg_stmt(cg, node->data.region_node.body, slots);
            /* Region teardown: restore rsp */
            cg_write_fmt(cg, "L_%x:\n", end_lbl);
            cg_inst(cg, "mov rsp, r15");
            cg_inst(cg, "xor rax, rax");
            cg_inst(cg, "mov [rel __aether_region_cur], rax");
            cg_inst(cg, "mov [rel __aether_region_end], rax");
            cg_comment(cg, "region end");
            break;
        }

        case NODE_TRY: {
            /* try { body } catch Type(var) { handler } ... finally { body }
             *
             * Proper deterministic exception handling with full stack unwinding:
             * - rdx = 0 means success, rdx = 1 means error
             * - rax holds the error discriminant/value on error
             * - Scope cleanup table tracks destructors + defers per scope level
             * - throw walks the cleanup table (innermost first) before jumping
             * - finally blocks execute regardless of success/failure
             * - catch-all (_) matches any error
             * - Unmatched errors propagate (re-throw)
             * - For host targets: sigsetjmp/siglongjmp catches hardware faults
             *   (segfaults, bus errors) and redirects to the catch handler */
            int catch_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);
            int finally_label = cg_new_label(cg);
            int segfault_check_label = cg_new_label(cg);
            bool has_finally = (node->data.try_node.finally_body != NULL);
            cg_comment(cg, "try begin");

            /* Save current cleanup depth so we can restore it after the try */
            int saved_cleanup_depth = cg->cleanup_depth;

            /* For host targets: set up sigsetjmp to catch hardware faults */
            bool isHost = (cg->target == TARGET_MACHO64 || cg->target == TARGET_ELF64_HOST);
            if (isHost) {
                cg_comment(cg, "sigsetjmp for hardware fault catch");
                if (cg->target == TARGET_MACHO64) {
                    cg_inst(cg, "lea rdi, [rel __aether_segfault_jmpbuf]");
                    cg_inst(cg, "call _aether_setJmpBuf");
                } else {
                    cg_inst(cg, "lea rdi, [rel __aether_segfault_jmpbuf]");
                    cg_inst(cg, "call aether_setJmpBuf");
                }
                /* Check return value: 0 = first call (normal), non-zero = siglongjmp (segfault caught) */
                cg_inst(cg, "test eax, eax");
                cg_write_fmt(cg, "    jnz L_%x\n", catch_label);
            }

            /* Clear error tag before try body */
            cg_inst(cg, "xor rdx, rdx");

            /* Emit try body */
            if (node->data.try_node.body)
                cg_stmt(cg, node->data.try_node.body, slots);

            /* Clear segfault jump buffer (no longer in try block) */
            if (isHost) {
                cg_comment(cg, "clear segfault jmpbuf");
                if (cg->target == TARGET_MACHO64) {
                    cg_inst(cg, "call _aether_clearJmpBuf");
                } else {
                    cg_inst(cg, "call aether_clearJmpBuf");
                }
            }

            /* Check error tag (rdx = 0 success, 1 = error) */
            cg_inst(cg, "test rdx, rdx");
            cg_write_fmt(cg, "    jnz L_%x\n", catch_label);
            /* Success path: jump to finally (or end if no finally) */
            if (has_finally) {
                cg_write_fmt(cg, "    jmp L_%x\n", finally_label);
            } else {
                cg_write_fmt(cg, "    jmp L_%x\n", end_label);
            }

            /* Catch handlers */
            cg_write_fmt(cg, "L_%x:\n", catch_label);
            cg_comment(cg, "catch handlers");

            /* Walk cleanup table for this try block */
            cg_comment(cg, "scope cleanup for try body");
            cg_emit_cleanup_range(cg, slots, saved_cleanup_depth, cg->cleanup_depth);

            /* Save error discriminant */
            cg_inst(cg, "push rax");

            /* Match error discriminant against catch arms */
            bool has_catch_all = false;
            for (int i = 0; i < node->data.try_node.catch_arms.count; i++) {
                AstNode *arm = node->data.try_node.catch_arms.items[i];
                int next_arm = cg_new_label(cg);

                if (arm->data.catch_arm.is_catch_all) {
                    has_catch_all = true;
                    cg_comment(cg, "catch-all");
                } else if (arm->data.catch_arm.type) {
                    char type_name[256];
                    int tlen = (int)arm->data.catch_arm.type->data.type_node.name.len;
                    if (tlen > 255) tlen = 255;
                    memcpy(type_name, arm->data.catch_arm.type->data.type_node.name.data, tlen);
                    type_name[tlen] = '\0';

                    int disc_val = -1;
                    for (VariantEntry *ve = variant_entries; ve; ve = ve->next) {
                        if (strcmp(ve->name, type_name) == 0) {
                            disc_val = ve->discriminant;
                            break;
                        }
                    }

                    if (disc_val >= 0) {
                        cg_inst(cg, "mov rax, [rsp]");
                        char buf[64];
                        snprintf(buf, sizeof(buf), "cmp rax, %d", disc_val);
                        cg_inst(cg, buf);
                        cg_write_fmt(cg, "    jne L_%x\n", next_arm);
                    }
                }

                if (arm->data.catch_arm.var) {
                    cg_comment(cg, "bind catch variable");
                    VarSlot *vs = find_var_slot(slots, arm->data.catch_arm.var);
                    if (vs) {
                        cg_inst(cg, "pop rax");
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", vs->stack_offset);
                        cg_inst(cg, buf);
                    } else {
                        cg_inst(cg, "add rsp, 8");
                    }
                } else {
                    if (!arm->data.catch_arm.is_catch_all || i < node->data.try_node.catch_arms.count - 1) {
                        cg_inst(cg, "add rsp, 8");
                    }
                }

                if (arm->data.catch_arm.body)
                    cg_stmt(cg, arm->data.catch_arm.body, slots);

                cg_inst(cg, "xor rdx, rdx");

                if (has_finally) {
                    cg_write_fmt(cg, "    jmp L_%x\n", finally_label);
                } else {
                    cg_write_fmt(cg, "    jmp L_%x\n", end_label);
                }

                cg_write_fmt(cg, "L_%x:\n", next_arm);
            }

            /* No catch matched — re-throw (propagate error to caller) */
            if (!has_catch_all) {
                cg_comment(cg, "no catch matched — re-throw");
                cg_inst(cg, "add rsp, 8");
                cg_emit_defers(cg, slots);
                cg_inst(cg, "mov rsp, rbp");
                cg_inst(cg, "pop rbp");
                cg_inst(cg, "ret");
            }

            /* Finally block */
            if (has_finally) {
                cg_write_fmt(cg, "L_%x:\n", finally_label);
                cg_comment(cg, "finally");
                if (node->data.try_node.finally_body)
                    cg_stmt(cg, node->data.try_node.finally_body, slots);
            }

            cg_write_fmt(cg, "L_%x:\n", end_label);
            cg_comment(cg, "try end");

            cg->cleanup_depth = saved_cleanup_depth;
            break;
        }

        case NODE_RUN_BLOCK: {
            /* #run blocks execute at compile time — no runtime code generated.
             * The body is evaluated during semantic analysis for constant folding.
             * At codegen time, we emit nothing (the results were already embedded). */
            cg_comment(cg, "#run block (compile-time only)");
            break;
        }

        case NODE_THROW: {
            /* throw expr — evaluate the expression, set error tag to 1,
             * walk the cleanup table (innermost first), then either jump
             * to the nearest catch handler or return with rdx=1. */
            AstNode *val = node->data.throw_node.value;
            cg_comment(cg, "throw");
            if (val) {
                cg_expr(cg, val, slots);
            } else {
                cg_inst(cg, "xor rax, rax");  /* default error discriminant */
            }
            /* Set error tag (rdx = 1) */
            cg_inst(cg, "mov rdx, 1");
            /* Emit defers before leaving the function */
            cg_emit_defers(cg, slots);
            /* Epilogue: restore stack and return */
            cg_inst(cg, "mov rsp, rbp");
            cg_inst(cg, "pop rbp");
            cg_inst(cg, "ret");
            break;
        }

        case NODE_UNSAFE: {
            /* unsafe { body } — just emit the body, no special handling needed */
            if (node->data.list.count > 0) {
                for (int i = 0; i < node->data.list.count; i++) {
                    cg_stmt(cg, node->data.list.items[i], slots);
                }
            }
            break;
        }

        case NODE_ASM_BLOCK: {
            /* Emit raw assembly text directly into the output */
            if (node->data.asm_block.text) {
                StringView asm_text = node->data.asm_block.text->data.literal.string_val;
                if (asm_text.len > 0) {
                    cg_write(cg, "; begin asm block\n");
                    /* Write each line directly, skipping extern declarations */
                    const char *p = asm_text.data;
                    const char *end = p + asm_text.len;
                    while (p < end) {
                        const char *line_start = p;
                        while (p < end && *p != '\n') p++;
                        if (p > line_start) {
                            /* Skip extern lines — they're hoisted to top level */
                            const char *s = line_start;
                            while (s < p && (*s == ' ' || *s == '\t')) s++;
                            if ((p - s) >= 6 && strncmp(s, "extern", 6) == 0) {
                                /* skip — already emitted at top */
                            } else {
                                /* Strip Aether comments (#) from asm block output */
                                const char *s_trim = line_start;
                                while (s_trim < p && (*s_trim == ' ' || *s_trim == '\t')) s_trim++;
                                if (s_trim < p && *s_trim == '#') {
                                    /* Skip this line entirely — it's an Aether comment */
                                } else {
                                    cg_write_fmt(cg, "%.*s\n", (int)(p - line_start), line_start);
                                }
                            }
                        } else {
                            cg_write(cg, "\n");
                        }
                        if (p < end) p++;
                    }
                    cg_write(cg, "; end asm block\n");
                }
            }
            break;
        }

        case NODE_POOL_DECL: {
            cg_comment(cg, "pool declaration (reserved)");
            break;
        }

        case NODE_PROTOCOL_DECL: {
            cg_comment(cg, "protocol declaration (interface)");
            break;
        }

        default:
            cg_warn(cg, node, "unsupported statement in codegen");
            break;
    }
}

/* ================================================================
 * Function-level code generation (2-pass: frame then body)
 * ================================================================ */

static const char *entry_point_name(Target t) {
    switch (t) {
        case TARGET_MACHO64:    return "_main";
        case TARGET_ELF64_HOST: return "_start";
        default:                return "_start";
    }
}

static const char *global_prefix(Target t) {
    (void)t;
    /* NASM handles underscore prefix automatically for Mach-O.
     * For ELF, no prefix is needed either. */
    return "";
}

static void cg_func(Codegen *cg, AstNode *func) {
    int frame_size = 0;
    VarSlot *slots = compute_frame(cg->arena, func, &frame_size);

    char fn_label[256];
    const char *gpref = global_prefix(cg->target);
    snprintf(fn_label, sizeof(fn_label), "%s%.*s",
        gpref,
        (int)func->data.func.name->data.ident.name.len,
        func->data.func.name->data.ident.name.data);

    cg_write_fmt(cg, "; function %s (frame=%d)\n", fn_label, frame_size);
    /* For @layout functions, skip label and prologue — the asm block is the entire output */
    if (!func->data.func.has_layout) {
        cg_write_fmt(cg, "global %s\n", fn_label);
        cg_write_fmt(cg, "%s:\n", fn_label);
    }

    /* Prologue — skip for @layout functions (flat binary boot sectors, etc.) */
    if (!func->data.func.has_layout) {
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
    }

    /* Pre-conditions: check before body */
    if (func->data.func.pre_conditions.count > 0) {
        cg_comment(cg, "pre-conditions");
        for (int i = 0; i < func->data.func.pre_conditions.count; i++) {
            int fail_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);
            cg_expr(cg, func->data.func.pre_conditions.items[i], slots);
            cg_inst(cg, "test rax, rax");
            cg_write_fmt(cg, "    jnz L_%x\n", end_label);
            cg_write_fmt(cg, "L_%x:\n", fail_label);
            cg_comment(cg, "pre-condition failed — panic");
            cg_inst(cg, "mov rdi, 1");  /* exit code 1 */
            cg_inst(cg, "mov rax, 0x2000001");  /* macOS exit */
            cg_inst(cg, "syscall");
            cg_write_fmt(cg, "L_%x:\n", end_label);
        }
    }

    /* Body */
    if (func->data.func.body) {
        cg_stmt(cg, func->data.func.body, slots);
    }

    /* Default return — only if the body didn't already return */
    /* Check if the last statement is a return */
    int body_has_return = 0;
    if (func->data.func.body && func->data.func.body->type == NODE_RETURN) {
        body_has_return = 1;
    }
    /* Also check if body is a block whose last statement is a return or asm block */
    if (func->data.func.body && func->data.func.body->type == NODE_BLOCK) {
        AstNodeList *stmts = &func->data.func.body->data.list;
        if (stmts->count > 0) {
            AstNode *last = stmts->items[stmts->count - 1];
            if (last->type == NODE_RETURN) {
                body_has_return = 1;
            }
            /* If the last statement is an asm block, check if it contains ret */
            if (last->type == NODE_ASM_BLOCK && last->data.asm_block.text) {
                StringView asm_text = last->data.asm_block.text->data.literal.string_val;
                const char *p = asm_text.data;
                const char *end = p + asm_text.len;
                while (p < end) {
                    if (p[0] == 'r' && p[1] == 'e' && p[2] == 't' &&
                        (end - p == 3 || p[3] == '\n' || p[3] == ' ' || p[3] == '\t' || p[3] == '\r')) {
                        body_has_return = 1;
                        break;
                    }
                    p++;
                }
            }
        }
    }
    /* Also check if body itself is an asm block with ret */
    if (func->data.func.body && func->data.func.body->type == NODE_ASM_BLOCK) {
        AstNode *last = func->data.func.body;
        if (last->data.asm_block.text) {
            StringView asm_text = last->data.asm_block.text->data.literal.string_val;
            const char *p = asm_text.data;
            const char *end = p + asm_text.len;
            while (p < end) {
                if (p[0] == 'r' && p[1] == 'e' && p[2] == 't' &&
                    (end - p == 3 || p[3] == '\n' || p[3] == ' ' || p[3] == '\t' || p[3] == '\r')) {
                    body_has_return = 1;
                    break;
                }
                p++;
            }
        }
    }
    if (!body_has_return) {
        cg_comment(cg, "default return");
        if (func->data.func.return_type) {
            cg_inst(cg, "xor rax, rax");  /* default return value = 0 */
        }
        if (func->data.func.is_throws) {
            cg_inst(cg, "xor rdx, rdx");  /* clear error tag = success */
        }
    }

    /* Post-conditions: check before defers (save return value first) */
    if (func->data.func.post_conditions.count > 0) {
        cg_comment(cg, "post-conditions");
        cg_inst(cg, "push rax");  /* save return value */
        for (int i = 0; i < func->data.func.post_conditions.count; i++) {
            int fail_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);
            cg_expr(cg, func->data.func.post_conditions.items[i], slots);
            cg_inst(cg, "test rax, rax");
            cg_write_fmt(cg, "    jnz L_%x\n", end_label);
            cg_write_fmt(cg, "L_%x:\n", fail_label);
            cg_comment(cg, "post-condition failed — panic");
            cg_inst(cg, "mov rdi, 1");
            cg_inst(cg, "mov rax, 0x2000001");
            cg_inst(cg, "syscall");
            cg_write_fmt(cg, "L_%x:\n", end_label);
        }
        cg_inst(cg, "pop rax");  /* restore return value */
    }

    cg_emit_defers(cg, slots);
    if (!func->data.func.has_layout) {
        cg_inst1(cg, "mov", "rsp, rbp");
        cg_inst1(cg, "pop", "rbp");
        cg_inst(cg, "ret");
    }
    cg_write(cg, "\n");

    cg_defer_clear(cg);
}

/* ================================================================
 * Aether OS memory map — used for @kernel_layout verification
 * ================================================================ */

typedef struct {
    const char *name;
    uint64_t start;
    uint64_t end;   /* exclusive */
} MemoryRegion;

static const MemoryRegion KNOWN_REGIONS[] = {
    {"Stage1 MBR",        0x7C00,     0x7E00},
    {"Stage2 loader",     0x7E00,     0x8000},
    {"Page tables / GDT", 0x1000,     0x4000},
    {"Module registry",   0x4000,     0x5000},
    {"Syscall page",      0x5000,     0x6000},
    {"Kernel base",       0x1000000,  0x11E6000},
    {"Binary exec space", 0x2000000,  0x2100000},
    {"Module slots",      0x2100000,  0x2180000},
    {"Available RAM",     0x11E6000,  0x10000000},
    {NULL, 0, 0}
};

/* Check if a given address overlaps any known memory region */
static const MemoryRegion *find_overlapping_region(uint64_t addr) {
    for (int i = 0; KNOWN_REGIONS[i].name; i++) {
        if (addr >= KNOWN_REGIONS[i].start && addr < KNOWN_REGIONS[i].end) {
            return &KNOWN_REGIONS[i];
        }
    }
    return NULL;
}

/* Verify a kernel-layout-annotated function for memory map conflicts */
static void cg_verify_kernel_layout(Codegen *cg, AstNode *func) {
    cg_write_fmt(cg, "; @kernel_layout verification for %.*s\n",
        (int)func->data.func.name->data.ident.name.len,
        func->data.func.name->data.ident.name.data);
    cg_write(cg, "; Aether OS memory map:\n");
    for (int i = 0; KNOWN_REGIONS[i].name; i++) {
        cg_write_fmt(cg, ";   %-20s 0x%07lx - 0x%07lx\n",
            KNOWN_REGIONS[i].name,
            (unsigned long)KNOWN_REGIONS[i].start,
            (unsigned long)KNOWN_REGIONS[i].end);
    }
    cg_write(cg, "; end @kernel_layout\n\n");
}

/* Helper: collect extern declarations from asm blocks and emit at top level */
static void collect_externs_from_block(Codegen *cg, AstNode *block) {
    if (!block) return;
    if (block->type == NODE_ASM_BLOCK && block->data.asm_block.text) {
        StringView asm_text = block->data.asm_block.text->data.literal.string_val;
        const char *p = asm_text.data;
        const char *end = p + asm_text.len;
        while (p < end) {
            const char *line_start = p;
            while (p < end && *p != '\n') p++;
            if (p > line_start) {
                /* Check if line starts with "extern" */
                const char *s = line_start;
                while (s < p && (*s == ' ' || *s == '\t')) s++;
                if ((p - s) >= 6 && strncmp(s, "extern", 6) == 0) {
                    cg_write_fmt(cg, "%.*s\n", (int)(p - line_start), line_start);
                }
            }
            if (p < end) p++;
        }
    } else if (block->type == NODE_BLOCK) {
        for (int i = 0; i < block->data.list.count; i++) {
            collect_externs_from_block(cg, block->data.list.items[i]);
        }
    }
}

/* ================================================================
 * Top-level code generation — target-aware
 * ================================================================ */

const char *codegen_generate(Codegen *cg, AstNode *program) {
    cg_comment(cg, "Generated by Aether Compiler");
    cg_write_fmt(cg, "; Target: %s\n\n", target_name(cg->target));

    /* First pass: collect all extern declarations from asm blocks */
    /* and emit them at the top level where NASM requires them */
    cg_write(cg, "; Extern declarations (hoisted from asm blocks)\n");
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *node = program->data.list.items[i];
        if (node->type == NODE_FUNC_DECL && node->data.func.body) {
            collect_externs_from_block(cg, node->data.func.body);
        }
    }
    /* For host targets: declare extern C helper functions */
    if (cg->target == TARGET_MACHO64 || cg->target == TARGET_ELF64_HOST) {
        cg_write(cg, "; C helper functions (from segfault_helper.o)\n");
        /* Mach-O: C compiler prepends _ to symbols, NASM does NOT for Mach-O.
         * So C function aether_initSegfault → object _aether_initSegfault.
         * NASM extern _aether_initSegfault → object _aether_initSegfault. ✓ */
        if (cg->target == TARGET_MACHO64) {
            cg_write(cg, "extern _aether_initSegfault\n");
            cg_write(cg, "extern _aether_printStacktrace\n");
            cg_write(cg, "extern _aether_setJmpBuf\n");
            cg_write(cg, "extern _aether_clearJmpBuf\n");
        } else {
            cg_write(cg, "extern aether_initSegfault\n");
            cg_write(cg, "extern aether_printStacktrace\n");
            cg_write(cg, "extern aether_setJmpBuf\n");
            cg_write(cg, "extern aether_clearJmpBuf\n");
        }
    }
    cg_write(cg, "\n");

    /* Build struct layouts from top-level declarations */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *node = program->data.list.items[i];
        if (node->type == NODE_STRUCT_DECL || node->type == NODE_CLASS_DECL) {
            build_struct_layout(cg->arena, node);
        } else if (node->type == NODE_ENUM_DECL) {
            build_enum_layout(cg->arena, node);
        }
    }

    /* First pass: scan for functions with @entry(addr), @layout, or @kernel_layout attributes */
    cg->entry_addr = 0;
    cg->entry_func = NULL;
    cg->has_layout = false;
    cg->layout_start = 0;
    cg->layout_max = 0;
    cg->layout_bits = 0;
    cg->layout_signature = 0;
    cg->layout_file = NULL;
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *node = program->data.list.items[i];
        if (node->type == NODE_MODULE_DECL) {
            /* Emit module ABI version as a comment */
            if (node->data.module_decl.module_abi_version >= 0) {
                cg_write_fmt(cg, "; Module ABI version: %d\n", node->data.module_decl.module_abi_version);
            }
        } else if (node->type == NODE_FUNC_DECL) {
            if (node->data.func.entry_addr != -1 && !cg->entry_func) {
                cg->entry_addr = node->data.func.entry_addr;
                cg->entry_func = arena_strndup(cg->arena,
                    node->data.func.name->data.ident.name.data,
                    node->data.func.name->data.ident.name.len);
                /* first entry-point function wins */
            }
            if (node->data.func.has_layout) {
                cg->has_layout = true;
                cg->layout_start = (int64_t)node->data.func.layout_start;
                cg->layout_max = (int64_t)node->data.func.layout_max;
                cg->layout_bits = node->data.func.layout_bits;
                cg->layout_signature = node->data.func.layout_signature;
                if (node->data.func.layout_file.len > 0) {
                    cg->layout_file = arena_strndup(cg->arena,
                        node->data.func.layout_file.data,
                        node->data.func.layout_file.len);
                }
            }
            if (node->data.func.is_kernel_layout) {
                cg_verify_kernel_layout(cg, node);
            }
        }
    }

    /* Target-specific directives */
    bool is_macho = (cg->target == TARGET_MACHO64);
    if (!is_macho && !cg->has_layout) {
        cg_write(cg, "bits 64\n");
    }
    if (!cg->has_layout) {
        if (cg->target != TARGET_BOOT) {
            cg_write(cg, "default rel\n");
        }
        cg_write(cg, "\n");
    }

    /* Emit const declarations as NASM equ directives — only for non-layout targets */
    if (!cg->has_layout) {
        for (int i = 0; i < program->data.list.count; i++) {
            AstNode *node = program->data.list.items[i];
            if (node->type == NODE_CONST_DECL) {
                AstNode *val = node->data.let_decl.value;
                if (val && val->type == NODE_LITERAL_INT) {
                    StringView name = node->data.let_decl.name->data.ident.name;
                    cg_write_fmt(cg, "%.*s equ %llu\n",
                        (int)name.len, name.data,
                        (unsigned long long)val->data.literal.int_val);
                }
            }
        }
        cg_write(cg, "\n");
    }

    /* Emit top-level asm blocks after bits/default/const but before section .text */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *node = program->data.list.items[i];
        if (node->type == NODE_ASM_BLOCK) {
            if (node->data.asm_block.text) {
                StringView asm_text = node->data.asm_block.text->data.literal.string_val;
                if (asm_text.len > 0) {
                    cg_write(cg, "; top-level asm block\n");
                    const char *p = asm_text.data;
                    const char *end = p + asm_text.len;
                    while (p < end) {
                        const char *line_start = p;
                        while (p < end && *p != '\n') p++;
                        if (p > line_start) {
                            /* Strip Aether comments (#) from asm block output */
                            const char *s_trim = line_start;
                            while (s_trim < p && (*s_trim == ' ' || *s_trim == '\t')) s_trim++;
                            if (s_trim < p && *s_trim == '#') {
                                /* Skip this line entirely — it's an Aether comment */
                            } else {
                                cg_write_fmt(cg, "%.*s\n", (int)(p - line_start), line_start);
                            }
                        } else {
                            cg_write(cg, "\n");
                        }
                        if (p < end) p++;
                    }
                    cg_write(cg, "; end top-level asm block\n\n");
                }
            }
        }
    }

    /* If @layout is set, emit bits, org, and auto-padding */
    if (cg->has_layout) {
        /* Emit bits directive from @layout(bits=N), default 64 */
        int bits = cg->layout_bits;
        if (bits == 0) bits = 64;
        cg_write_fmt(cg, "bits %d\n", bits);
        /* Emit org from @layout(start=N) */
        if (cg->layout_start > 0) {
            cg_write_fmt(cg, "[org 0x%lx]\n", (unsigned long)cg->layout_start);
        }
        cg_write_fmt(cg, "; Layout: start=0x%lx, max=%ld bytes\n",
            (unsigned long)cg->layout_start, (unsigned long)cg->layout_max);
    } else {
        cg_write(cg, "section .text\n\n");
    }

    /* Entry point — skip for @layout (flat binaries define their own entry) */
    if (!cg->has_layout) {
        if (cg->entry_func) {
            /* @entry(addr) is set — function IS the entry point directly */
            cg_write_fmt(cg, "; Entry point: %s at 0x%lx\n", cg->entry_func, (unsigned long)cg->entry_addr);
            cg_write_fmt(cg, "global %s\n", cg->entry_func);
            /* For host targets, we still need an OS-visible entry wrapper */
            if (is_macho) {
                const char *entry = "_aether_entry";
                cg_write_fmt(cg, "global %s\n", entry);
                cg_write_fmt(cg, "%s:\n", entry);
                cg_inst1(cg, "mov", "rbp, rsp");
                cg_comment(cg, "call entry function");
                cg_write_fmt(cg, "    call %s\n", cg->entry_func);
                cg_comment(cg, "macOS exit(rdi = rax)");
                cg_inst1(cg, "mov", "rdi, rax");
                cg_inst1(cg, "mov", "rax, 0x2000001");
                cg_inst(cg, "syscall");
                cg_inst(cg, "hlt");
                cg_write(cg, "\n");
            } else if (cg->target == TARGET_ELF64_HOST) {
                const char *entry = "_start";
                cg_write_fmt(cg, "global %s\n", entry);
                cg_write_fmt(cg, "%s:\n", entry);
                cg_inst1(cg, "mov", "rbp, rsp");
                cg_comment(cg, "call entry function");
                cg_write_fmt(cg, "    call %s\n", cg->entry_func);
                cg_comment(cg, "Linux exit(rdi = rax)");
                cg_inst1(cg, "mov", "rdi, rax");
                cg_inst1(cg, "mov", "rax, 60");
                cg_inst(cg, "syscall");
                cg_inst(cg, "hlt");
                cg_write(cg, "\n");
            }
            /* For freestanding/kernel/module/binary: linker script handles ENTRY(func_name) — no wrapper needed */
        } else {
            /* Default entry point: wrapper calls main(), then exits */
            const char *entry;
            if (is_macho) {
                entry = "_aether_entry";  /* Mach-O: custom entry, linker -e flag */
            } else if (cg->target == TARGET_UNIVERSAL || cg->target == TARGET_UNIVERSAL_ALL) {
                entry = "_aether_entry";  /* Universal: trampoline defines _start, jumps to _aether_entry */
            } else {
                entry = "_start";          /* ELF: ENTRY(_start) in linker script */
            }
            cg_write_fmt(cg, "global %s\n", entry);
            cg_write_fmt(cg, "%s:\n", entry);
            cg_inst1(cg, "mov", "rbp, rsp");
            cg_comment(cg, "init segfault handler");
            if (cg->target == TARGET_MACHO64) {
                cg_inst(cg, "call _aether_initSegfault");
            } else {
                cg_inst(cg, "call aether_initSegfault");
            }
            cg_comment(cg, "call main()");
            cg_inst(cg, "call main");

            if (is_macho) {
                cg_comment(cg, "macOS exit(rdi = rax)");
                cg_inst1(cg, "mov", "rdi, rax");
                cg_inst1(cg, "mov", "rax, 0x2000001");
                cg_inst(cg, "syscall");
            } else if (cg->target == TARGET_ELF64_HOST) {
                cg_comment(cg, "Linux exit(rdi = rax)");
                cg_inst1(cg, "mov", "rdi, rax");
                cg_inst1(cg, "mov", "rax, 60");
                cg_inst(cg, "syscall");
            } else if (cg->target == TARGET_BINARY) {
                cg_comment(cg, "Aether OS binary: ret to shell after main returns");
                cg_inst(cg, "ret");
            } else {
                cg_comment(cg, "freestanding: just hlt after main returns");
                cg_inst(cg, "hlt");
            }
            cg_inst(cg, "hlt");
            cg_write(cg, "\n");
        }
    }

    /* Emit runtime helpers and data sections — BEFORE user functions */
    if (!cg->has_layout) {
        bool needs_runtime = (cg->target == TARGET_MACHO64 || cg->target == TARGET_ELF64_HOST || cg->target == TARGET_BINARY);
        if (needs_runtime) {
            /* Bump allocator state in .bss */
            cg_write(cg, "section .bss\n");
            cg_write(cg, "__aether_heap_start: resq 1\n");
            cg_write(cg, "__aether_heap_cur:   resq 1\n");
            cg_write(cg, "__aether_heap_end:   resq 1\n");
            cg_write(cg, "__aether_region_cur: resq 1\n");
            cg_write(cg, "__aether_region_end: resq 1\n");
            /* Segfault jump buffer — set by try blocks for hardware fault catch */
            cg_write(cg, "__aether_segfault_jmpbuf: resb 200\n");
            if (cg->target == TARGET_BINARY) {
                cg_write(cg, "__aether_heap_buf:  resb 256\n");
            }
            cg_write(cg, "\n");

            cg_write(cg, "section .text\n");
            /* __aether_alloc */
            cg_write(cg, "; __aether_alloc(rdi: size) -> rax: ptr\n");
            cg_write(cg, "; If region is active, bump from region first.\n");
            cg_write(cg, "__aether_alloc:\n");
            cg_inst1(cg, "push", "rbp");
            cg_inst1(cg, "mov", "rbp, rsp");
            cg_inst(cg, "add rdi, 15");
            cg_inst(cg, "and rdi, ~15");
            cg_inst(cg, "mov rax, [rel __aether_region_cur]");
            cg_inst1(cg, "test", "rax, rax");
            cg_inst(cg, "jz LA_heap");
            cg_inst(cg, "lea rcx, [rax + rdi]");
            cg_inst(cg, "cmp rcx, [rel __aether_region_end]");
            cg_inst(cg, "jbe LA_region_ok");
            cg_inst(cg, "jmp LA_heap");
            cg_write(cg, "LA_region_ok:\n");
            cg_inst(cg, "mov [rel __aether_region_cur], rcx");
            cg_inst1(cg, "mov", "rsp, rbp");
            cg_inst1(cg, "pop", "rbp");
            cg_inst(cg, "ret");
            cg_write(cg, "LA_heap:\n");
            cg_inst1(cg, "push", "rdi");
            cg_write(cg, "LA_init:\n");
            if (cg->target == TARGET_BINARY) {
                /* Binary target: use a fixed BSS buffer instead of mmap */
                cg_inst(cg, "mov rax, [rel __aether_heap_start]");
                cg_inst1(cg, "test", "rax, rax");
                cg_inst(cg, "jnz LA_try");
                cg_inst(cg, "lea rax, [rel __aether_heap_buf]");
                cg_inst(cg, "mov [rel __aether_heap_start], rax");
                cg_inst(cg, "lea rcx, [rax + 8]");
                cg_inst(cg, "mov [rel __aether_heap_cur], rcx");
                cg_inst(cg, "lea rcx, [rax + 65536]");
                cg_inst(cg, "mov [rel __aether_heap_end], rcx");
            } else if (cg->target == TARGET_MACHO64) {
                cg_inst1(cg, "mov", "rdi, 0");
                cg_inst1(cg, "mov", "rsi, 65536");
                cg_inst1(cg, "mov", "rdx, 3");
                cg_inst1(cg, "mov", "r10, 0x1002");
                cg_inst1(cg, "mov", "r8, -1");
                cg_inst1(cg, "xor", "r9, r9");
                cg_inst1(cg, "mov", "rax, 0x20000C5");
            } else {
                cg_inst1(cg, "mov", "rdi, 0");
                cg_inst1(cg, "mov", "rsi, 65536");
                cg_inst1(cg, "mov", "rdx, 3");
                cg_inst1(cg, "mov", "r10, 0x22");
                cg_inst1(cg, "mov", "r8, -1");
                cg_inst1(cg, "xor", "r9, r9");
                cg_inst1(cg, "mov", "rax, 9");
            }
            cg_inst(cg, "syscall");
            cg_inst(cg, "mov [rel __aether_heap_start], rax");
            cg_inst(cg, "lea rcx, [rax + 8]");
            cg_inst(cg, "mov [rel __aether_heap_cur], rcx");
            cg_inst(cg, "lea rcx, [rax + 65536]");
            cg_inst(cg, "mov [rel __aether_heap_end], rcx");
            cg_write(cg, "LA_try:\n");
            cg_inst(cg, "mov rax, [rel __aether_heap_cur]");
            cg_inst(cg, "lea rcx, [rax + rdi]");
            cg_inst(cg, "cmp rcx, [rel __aether_heap_end]");
            cg_inst(cg, "jbe LA_ok");
            if (cg->target == TARGET_BINARY) {
                /* Binary target: no more heap space, just halt */
                cg_inst(cg, "cli");
                cg_write(cg, ".oom:\n");
                cg_inst(cg, "hlt");
                cg_inst(cg, "jmp .oom");
            } else if (cg->target == TARGET_MACHO64) {
                cg_inst1(cg, "mov", "rdi, 0");
                cg_inst1(cg, "mov", "rsi, 65536");
                cg_inst1(cg, "mov", "rdx, 3");
                cg_inst1(cg, "mov", "r10, 0x1002");
                cg_inst1(cg, "mov", "r8, -1");
                cg_inst1(cg, "xor", "r9, r9");
                cg_inst1(cg, "mov", "rax, 0x20000C5");
            } else {
                cg_inst1(cg, "mov", "rdi, 0");
                cg_inst1(cg, "mov", "rsi, 65536");
                cg_inst1(cg, "mov", "rdx, 3");
                cg_inst1(cg, "mov", "r10, 0x22");
                cg_inst1(cg, "mov", "r8, -1");
                cg_inst1(cg, "xor", "r9, r9");
                cg_inst1(cg, "mov", "rax, 9");
            }
            cg_inst(cg, "syscall");
            cg_inst(cg, "lea rcx, [rax + 8]");
            cg_inst(cg, "mov [rel __aether_heap_cur], rcx");
            cg_inst(cg, "lea rcx, [rax + 65536]");
            cg_inst(cg, "mov [rel __aether_heap_end], rcx");
            cg_write(cg, "LA_ok:\n");
            cg_inst(cg, "mov rax, [rel __aether_heap_cur]");
            cg_inst(cg, "add [rel __aether_heap_cur], rdi");
            cg_inst1(cg, "mov", "rsp, rbp");
            cg_inst1(cg, "pop", "rbp");
            cg_inst(cg, "ret");
            cg_write(cg, "\n");

            /* __aether_free — no-op bump */
            cg_write(cg, "; __aether_free(rdi: ptr) — no-op (bump allocator)\n");
            cg_write(cg, "__aether_free:\n");
            cg_inst(cg, "ret");
            cg_write(cg, "\n");

            /* __aether_concat(left: ptr, right: ptr) -> rax: ptr */
            cg_write(cg, "; __aether_concat(left: ptr, right: ptr) -> rax: ptr\n");
            cg_write(cg, "; Concatenates two null-terminated strings, returns new string\n");
            cg_write(cg, "__aether_concat:\n");
            cg_inst1(cg, "push", "rbp");
            cg_inst1(cg, "mov", "rbp, rsp");
            cg_inst1(cg, "push", "r12");
            cg_inst1(cg, "push", "r13");
            cg_inst1(cg, "push", "r14");
            cg_inst1(cg, "push", "r15");
            /* Args: call pushed ret_addr, then push rbp; mov rbp,rsp.
               After prologue: [rbp]=old_rbp, [rbp+8]=ret_addr.
               The 4 pushes (r12-r15) after mov rbp,rsp do not move rbp.
               [rbp+16] = right arg (pushed second/last by caller)
               [rbp+24] = left arg (pushed first by caller) */
            cg_inst(cg, "mov r12, [rbp+24]");  /* r12 = left ptr */
            cg_inst(cg, "mov r13, [rbp+16]");  /* r13 = right ptr */
            /* strlen(left) */
            cg_inst1(cg, "xor", "rcx, rcx");
            cg_write(cg, "LA_concat_strlen_left:\n");
            cg_inst(cg, "cmp byte [r12+rcx], 0");
            cg_inst(cg, "jz LA_concat_left_done");
            cg_inst1(cg, "inc", "rcx");
            cg_inst(cg, "jmp LA_concat_strlen_left");
            cg_write(cg, "LA_concat_left_done:\n");
            cg_inst1(cg, "mov", "r14, rcx");  /* r14 = left_len */
            /* strlen(right) */
            cg_inst1(cg, "xor", "rcx, rcx");
            cg_write(cg, "LA_concat_strlen_right:\n");
            cg_inst(cg, "cmp byte [r13+rcx], 0");
            cg_inst(cg, "jz LA_concat_right_done");
            cg_inst1(cg, "inc", "rcx");
            cg_inst(cg, "jmp LA_concat_strlen_right");
            cg_write(cg, "LA_concat_right_done:\n");
            cg_inst1(cg, "mov", "r15, rcx");  /* r15 = right_len (callee-saved, survives calls) */
            /* total = left_len + right_len + 1 */
            cg_inst1(cg, "lea", "rdi, [r14+r15+1]");
            cg_inst(cg, "call __aether_alloc");
            /* rax = allocated buffer, save it */
            cg_inst1(cg, "mov", "r12, rax");  /* r12 = buffer ptr (overwrites left ptr, but we still have it in [rbp+24]) */
            /* Copy left string */
            cg_inst(cg, "mov rsi, [rbp+24]");  /* src = left ptr (re-read from stack args) */
            cg_inst1(cg, "mov", "rdi, r12");  /* dest = buffer */
            cg_inst1(cg, "mov", "rdx, r14");  /* count = left_len */
            cg_inst(cg, "call LA_concat_memcpy");
            /* Advance past left */
            cg_inst1(cg, "add", "rdi, r14");
            /* Copy right string */
            cg_inst1(cg, "mov", "rsi, r13");  /* src = right ptr */
            cg_inst1(cg, "mov", "rdx, r15");  /* count = right_len */
            cg_inst(cg, "call LA_concat_memcpy");
            /* Null-terminate */
            cg_inst(cg, "mov byte [rdi+r15], 0");
            /* Return buffer ptr in rax */
            cg_inst1(cg, "mov", "rax, r12");
            cg_inst1(cg, "pop", "r15");
            cg_inst1(cg, "pop", "r14");
            cg_inst1(cg, "pop", "r13");
            cg_inst1(cg, "pop", "r12");
            cg_inst1(cg, "mov", "rsp, rbp");
            cg_inst1(cg, "pop", "rbp");
            cg_inst(cg, "ret");
            cg_write(cg, "\n");
            /* memcpy helper: rdi=dest, rsi=src, rdx=count */
            cg_write(cg, "LA_concat_memcpy:\n");
            cg_inst1(cg, "xor", "rcx, rcx");
            cg_write(cg, "LA_concat_memcpy_loop:\n");
            cg_inst(cg, "cmp rcx, rdx");
            cg_inst(cg, "jae LA_concat_memcpy_done");
            cg_inst(cg, "mov al, [rsi+rcx]");
            cg_inst(cg, "mov [rdi+rcx], al");
            cg_inst1(cg, "inc", "rcx");
            cg_inst(cg, "jmp LA_concat_memcpy_loop");
            cg_write(cg, "LA_concat_memcpy_done:\n");
            cg_inst(cg, "ret");
            cg_write(cg, "\n");

            /* __aether_itoa(rdi: u64) -> rax: ptr — converts u64 to decimal string */
            cg_write(cg, "; __aether_itoa(rdi: u64) -> rax: ptr\n");
            cg_write(cg, "; Converts unsigned 64-bit integer to decimal string (allocated)\n");
            cg_write(cg, "__aether_itoa:\n");
            cg_inst1(cg, "push", "rbp");
            cg_inst1(cg, "mov", "rbp, rsp");
            cg_inst1(cg, "push", "r12");
            cg_inst1(cg, "push", "r13");
            cg_inst1(cg, "push", "r14");
            /* rdi = value. Allocate 21 bytes (max 20 digits + null) */
            cg_inst1(cg, "mov", "r12, rdi");   /* r12 = value */
            cg_inst1(cg, "mov", "rdi, 21");
            cg_inst(cg, "call __aether_alloc");
            cg_inst1(cg, "mov", "r13, rax");    /* r13 = buffer */
            /* Handle zero case */
            cg_inst1(cg, "test", "r12, r12");
            cg_inst(cg, "jnz LA_itoa_convert");
            cg_inst(cg, "mov byte [r13], '0'");
            cg_inst(cg, "mov byte [r13+1], 0");
            cg_inst1(cg, "mov", "rax, r13");
            cg_inst1(cg, "pop", "r14");
            cg_inst1(cg, "pop", "r13");
            cg_inst1(cg, "pop", "r12");
            cg_inst1(cg, "mov", "rsp, rbp");
            cg_inst1(cg, "pop", "rbp");
            cg_inst(cg, "ret");
            /* Convert: write digits from end backwards */
            cg_write(cg, "LA_itoa_convert:\n");
            cg_inst1(cg, "lea", "r14, [r13+19]");  /* r14 = end of 20-char buffer */
            cg_inst(cg, "mov byte [r14], 0");        /* null terminator */
            cg_write(cg, "LA_itoa_loop:\n");
            cg_inst1(cg, "mov", "rax, r12");
            cg_inst1(cg, "xor", "rdx, rdx");
            cg_inst1(cg, "mov", "rcx, 10");
            cg_inst1(cg, "div", "rcx");              /* rax = quotient, rdx = remainder */
            cg_inst1(cg, "add", "dl, '0'");          /* convert to ASCII */
            cg_inst(cg, "mov [r14], dl");
            cg_inst1(cg, "dec", "r14");
            cg_inst1(cg, "mov", "r12, rax");
            cg_inst1(cg, "test", "rax, rax");
            cg_inst(cg, "jnz LA_itoa_loop");
            /* r14 points to last char written, advance past it for start */
            cg_inst1(cg, "inc", "r14");
            /* Shift string to start of buffer */
            cg_inst1(cg, "mov", "rdi, r13");         /* dest = buffer start */
            cg_inst1(cg, "mov", "rsi, r14");         /* src = first digit */
            /* Compute length */
            cg_inst1(cg, "lea", "rdx, [r13+19]");
            cg_inst1(cg, "sub", "rdx, r14");         /* rdx = length */
            cg_inst(cg, "call LA_concat_memcpy");
            cg_inst(cg, "mov byte [r13+rdx], 0");    /* null terminate at new position */
            cg_inst1(cg, "mov", "rax, r13");
            cg_inst1(cg, "pop", "r14");
            cg_inst1(cg, "pop", "r13");
            cg_inst1(cg, "pop", "r12");
            cg_inst1(cg, "mov", "rsp, rbp");
            cg_inst1(cg, "pop", "rbp");
            cg_inst(cg, "ret");
            cg_write(cg, "\n");

            /* Default drop stubs for all classes */
            for (StructLayout *sl = struct_layouts; sl; sl = sl->next) {
                if (sl->is_class) {
                    cg_write_fmt(cg, "; default %s_drop stub\n", sl->name);
                    cg_write_fmt(cg, "%s_drop:\n", sl->name);
                    cg_inst(cg, "ret");
                    cg_write(cg, "\n");
                }
            }
        }
    }

    /* Generate each function */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *node = program->data.list.items[i];
        if (node->type == NODE_FUNC_DECL) {
            cg->current_func = node;
            cg_func(cg, node);
            /* If this function has @layout, emit padding to exactly fill max bytes */
            if (node->data.func.has_layout && cg->layout_max > 0) {
                if (cg->layout_signature != 0) {
                    cg_write_fmt(cg, "; @layout padding to %ld bytes (with signature 0x%x)\n",
                        (unsigned long)cg->layout_max, cg->layout_signature);
                    cg_write_fmt(cg, "times %ld-($-$$) db 0\n",
                        (unsigned long)(cg->layout_max - 2));
                    cg_write_fmt(cg, "dw 0x%x\n", cg->layout_signature);
                } else {
                    cg_write_fmt(cg, "; @layout padding to %ld bytes\n", (unsigned long)cg->layout_max);
                    cg_write_fmt(cg, "times %ld-($-$$) db 0\n", (unsigned long)cg->layout_max);
                }
                cg_write(cg, "\n");
            }
        }
    }

    /* Emit source map table for segfault handler (host targets only) */
    if (!cg->has_layout && cg->src_loc_list) {
        cg_write(cg, "section .rodata\n");
        if (cg->target == TARGET_MACHO64) {
            cg_write(cg, "global _aether_source_map\n");
            cg_write(cg, "_aether_source_map:\n");
        } else {
            cg_write(cg, "global aether_source_map\n");
            cg_write(cg, "aether_source_map:\n");
        }
        for (SrcLocEntry *e = cg->src_loc_list; e; e = e->next) {
            cg_write_fmt(cg, "  dq _aether_src_%d\n", e->label_num);
            cg_write_fmt(cg, "  dd %d, %d\n", e->line, e->col);
        }
        cg_write(cg, "  dq 0\n");  /* sentinel */
        cg_write(cg, "  dd 0, 0\n");
        cg_write(cg, "\n");
    }

    /* Emit string table — uses the label stored in each StringEntry */
    if (!cg->has_layout) {
        cg_write(cg, "section .rodata\n");
        for (StringEntry *e = string_entries; e; e = e->next) {
            cg_write_fmt(cg, "Lstr%d: db ", e->label_num);
            /* Emit printable chars between quotes, non-printable as numeric */
            cg_write(cg, "\"");
            for (size_t i = 0; i < e->sv.len; i++) {
                unsigned char c = (unsigned char)e->sv.data[i];
                if (c == '"') cg_write(cg, "\\\"");
                else if (c == '\\') cg_write(cg, "\\\\");
                else if (c >= 32 && c < 127) { char buf[2] = {c, 0}; cg_write(cg, buf); }
                else {
                    /* Non-printable: close string, emit as numeric, reopen */
                    cg_write_fmt(cg, "\", %u, \"", c);
                }
            }
            cg_write(cg, "\", 0\n");
        }
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

/* ================================================================
 * Defer helpers
 * ================================================================ */

static void cg_defer_push(Codegen *cg, AstNode *body) {
    if (cg->current_func && cg->current_func->type == NODE_FUNC_DECL) {
        node_list_append(&cg->current_func->data.func.defer_list, body);
    }
}

static void cg_emit_defers(Codegen *cg, VarSlot *slots) {
    if (!cg->current_func || cg->current_func->type != NODE_FUNC_DECL) return;
    AstNodeList *defers = &cg->current_func->data.func.defer_list;
    if (defers->count == 0 && cg->auto_drops == NULL) return;
    cg_comment(cg, "defers");
    for (int i = defers->count - 1; i >= 0; i--) {
        cg_stmt(cg, defers->items[i], slots);
    }
    /* Emit auto-drop calls for class-typed variables (LIFO — push order is natural) */
    for (AutoDrop *ad = cg->auto_drops; ad; ad = ad->next) {
        cg_write_fmt(cg, "    lea rdi, [rbp%+d]\n", ad->stack_offset);
        cg_write_fmt(cg, "    call %s_drop\n", ad->class_name);
    }
}

static void cg_defer_clear(Codegen *cg) {
    if (cg->current_func && cg->current_func->type == NODE_FUNC_DECL) {
        cg->current_func->data.func.defer_list.count = 0;
    }
}

/* ================================================================
 * Assemble and link pipeline
 * ================================================================ */

int codegen_assemble(Codegen *cg, const char *asm_file, const char *output_file) {
    /* Universal binary targets: compile for multiple architectures and merge */
    if (cg->target == TARGET_UNIVERSAL || cg->target == TARGET_UNIVERSAL_ALL) {
        /* Read the generated x86_64 NASM */
        FILE *f = fopen(asm_file, "rb");
        if (!f) { fprintf(stderr, "Error: cannot read '%s'\n", asm_file); return 1; }
        fseek(f, 0, SEEK_END);
        long flen = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *src = (char *)malloc((size_t)flen + 1);
        if (!src) { fclose(f); return 1; }
        size_t rlen = fread(src, 1, (size_t)flen, f);
        src[rlen] = '\0';
        fclose(f);

        /* Parse the NASM into IR */
        AsmIRBlock block;
        if (!asm_parse_string(src, &block, asm_file)) {
            fprintf(stderr, "Error: failed to parse generated NASM\n");
            free(src);
            asm_block_free(&block);
            return 1;
        }
        free(src);

        /* Create universal builder */
        UniversalConfig config;
        universal_config_init(&config);
        if (cg->target == TARGET_UNIVERSAL_ALL) {
            config.arch_flags = (1 << ARCH_X86_64) | (1 << ARCH_ARM64) | (1 << ARCH_RISCV64);
        } else {
            config.arch_flags = (1 << ARCH_X86_64) | (1 << ARCH_ARM64);
        }
        config.verbose = 1;

        UniversalBuilder *builder = universal_builder_create(&config);
        if (!builder) { asm_block_free(&block); return 1; }

        /* Emit x86_64 NASM (passthrough) */
        {
            extern AsmBackend *asm_backend_create_x86_64(void);
            AsmBackend *backend = asm_backend_create_x86_64();
            if (!backend) { universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            size_t out_len;
            char *output = backend->emit(&block, &out_len);
            if (!output) { backend->destroy(output); universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            universal_builder_add_asm(builder, ARCH_X86_64, output, out_len);
            backend->destroy(output);
        }

        /* Emit ARM64 */
        if (config.arch_flags & (1 << ARCH_ARM64)) {
            extern AsmBackend *asm_backend_create_arm64(void);
            AsmBackend *backend = asm_backend_create_arm64();
            if (!backend) { universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            size_t out_len;
            char *output = backend->emit(&block, &out_len);
            if (!output) { backend->destroy(output); universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            universal_builder_add_asm(builder, ARCH_ARM64, output, out_len);
            backend->destroy(output);
        }

        /* Emit RISC-V */
        if (config.arch_flags & (1 << ARCH_RISCV64)) {
            extern AsmBackend *asm_backend_create_riscv64(void);
            AsmBackend *backend = asm_backend_create_riscv64();
            if (!backend) { universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            size_t out_len;
            char *output = backend->emit(&block, &out_len);
            if (!output) { backend->destroy(output); universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            universal_builder_add_asm(builder, ARCH_RISCV64, output, out_len);
            backend->destroy(output);
        }

        asm_block_free(&block);

        /* Build the universal binary */
        int ret = universal_build(builder, output_file);
        universal_builder_destroy(builder);
        return ret;
    }

    /* ASM listing targets: parse NASM output and re-emit through multi-target backend */
    if (cg->target == TARGET_ASM_X86_64 || cg->target == TARGET_ASM_ARM64 || cg->target == TARGET_ASM_RISCV64) {
        FILE *f = fopen(asm_file, "rb");
        if (!f) { fprintf(stderr, "Error: cannot read '%s'\n", asm_file); return 1; }
        fseek(f, 0, SEEK_END);
        long flen = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *src = (char *)malloc((size_t)flen + 1);
        if (!src) { fclose(f); return 1; }
        size_t rlen = fread(src, 1, (size_t)flen, f);
        src[rlen] = '\0';
        fclose(f);

        AsmIRBlock block;
        if (!asm_parse_string(src, &block, asm_file)) {
            fprintf(stderr, "Error: failed to parse generated NASM\n");
            free(src);
            asm_block_free(&block);
            return 1;
        }
        free(src);

        AsmBackend *backend = NULL;
        if (cg->target == TARGET_ASM_X86_64) {
            extern AsmBackend *asm_backend_create_x86_64(void);
            backend = asm_backend_create_x86_64();
        } else if (cg->target == TARGET_ASM_ARM64) {
            extern AsmBackend *asm_backend_create_arm64(void);
            backend = asm_backend_create_arm64();
        } else {
            extern AsmBackend *asm_backend_create_riscv64(void);
            backend = asm_backend_create_riscv64();
        }
        if (!backend) { fprintf(stderr, "Error: failed to create ASM backend\n"); asm_block_free(&block); return 1; }

        size_t out_len;
        char *output = backend->emit(&block, &out_len);
        if (!output) { fprintf(stderr, "Error: backend emit failed\n"); backend->destroy(output); asm_block_free(&block); return 1; }

        FILE *outf = fopen(output_file, "w");
        if (!outf) { fprintf(stderr, "Error: cannot write '%s'\n", output_file); backend->destroy(output); asm_block_free(&block); return 1; }
        fwrite(output, 1, out_len, outf);
        fclose(outf);

        backend->destroy(output);
        asm_block_free(&block);
        return 0;
    }

    /* Step 1: Determine nasm format and output object path */
    char obj_file[1024];
    const char *nasm_format;
    const char *link_cmd_prefix;
    char ld_cmd_buf[2048]; /* persistent buffer for dynamic linker cmd */

    switch (cg->target) {
        case TARGET_MACHO64:
            nasm_format = "macho64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/aether_host.o");
            link_cmd_prefix = "clang -arch x86_64 -e _aether_entry";
            break;
        case TARGET_ELF64_HOST:
            nasm_format = "elf64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/aether_host.o");
            link_cmd_prefix = "ld -o";
            break;
        case TARGET_BOOT:
            /* Boot sector: flat binary, no linker — handled below */
            nasm_format = "bin";
            obj_file[0] = '\0';
            link_cmd_prefix = NULL;
            break;
        case TARGET_KERNEL:
            nasm_format = "elf64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/aether_kernel.o");
            /* Use custom linker script if provided, otherwise auto-generate */
            if (cg->linker_script) {
                snprintf(ld_cmd_buf, sizeof(ld_cmd_buf), LD " -T %s -o", cg->linker_script);
                link_cmd_prefix = ld_cmd_buf;
            } else {
                FILE *ldf = fopen("/tmp/aether_ld.ld", "w");
                if (ldf) {
                    fprintf(ldf, "OUTPUT_FORMAT(elf64-x86-64)\nENTRY(_start)\nSECTIONS {\n");
                    fprintf(ldf, "  . = 0x1000000;\n");
                    fprintf(ldf, "  .text : ALIGN(16) { *(.text) *(.text.*) }\n");
                    fprintf(ldf, "  .rodata : ALIGN(16) { *(.rodata) *(.rodata.*) }\n");
                    fprintf(ldf, "  .data : ALIGN(16) { *(.data) *(.data.*) }\n");
                    fprintf(ldf, "  .bss : ALIGN(16) { *(.bss) *(.bss.*) *(COMMON) }\n");
                    fprintf(ldf, "  /DISCARD/ : { *(.comment) *(.note.*) *(.eh_frame) *(.eh_frame_hdr) }\n}\n");
                    fclose(ldf);
                }
                link_cmd_prefix = LD " -T /tmp/aether_ld.ld -o";
            }
            break;
        case TARGET_MODULE:
            nasm_format = "elf64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/aether_module.o");
            link_cmd_prefix = "";
            break;
        case TARGET_BINARY:
            nasm_format = "elf64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/aether_binary.o");
            /* Write linker script for Aether OS binary at 0x2000000 */
            {
                FILE *ldf = fopen("/tmp/aether_binary.ld", "w");
                if (ldf) {
                    uint64_t load_addr = (cg->entry_addr > 0) ? (uint64_t)cg->entry_addr : 0x2000000;
                    const char *entry_name = cg->entry_func ? cg->entry_func : "_start";
                    fprintf(ldf, "OUTPUT_FORMAT(elf64-x86-64)\nENTRY(%s)\nSECTIONS {\n", entry_name);
                    fprintf(ldf, "  . = 0x%lx;\n", (unsigned long)load_addr);
                    fprintf(ldf, "  .text : { *(.text) *(.text.*) }\n");
                    fprintf(ldf, "  .rodata : { *(.rodata) *(.rodata.*) }\n");
                    fprintf(ldf, "  .data : { *(.data) *(.data.*) }\n");
                    fprintf(ldf, "  .bss : { *(.bss) *(.bss.*) *(COMMON) }\n}\n");
                    fclose(ldf);
                }
            }
            link_cmd_prefix = LD " -T /tmp/aether_binary.ld -o";
            break;
        default:
            /* Freestanding — use external LD variable */
            nasm_format = "elf64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/aether_host.o");
            /* Write linker script inline (or use user-supplied one) */
            if (cg->linker_script) {
                snprintf(ld_cmd_buf, sizeof(ld_cmd_buf), LD " -T %s -o", cg->linker_script);
                link_cmd_prefix = ld_cmd_buf;
            } else {
                FILE *ldf = fopen("/tmp/aether_ld.ld", "w");
                if (ldf) {
                    /* Use @entry address if set, otherwise default to 0x400000 */
                    uint64_t load_addr = (cg->entry_addr > 0) ? (uint64_t)cg->entry_addr : 0x400000;
                    /* Use @entry func name if set, otherwise default to _start */
                    const char *entry_name = cg->entry_func ? cg->entry_func : "_start";
                    fprintf(ldf, "OUTPUT_FORMAT(elf64-x86-64)\nENTRY(%s)\nSECTIONS {\n", entry_name);
                    fprintf(ldf, "  . = 0x%lx;\n", (unsigned long)load_addr);
                    fprintf(ldf, "  .text : { *(.text) *(.text.*) }\n");
                    fprintf(ldf, "  .rodata : { *(.rodata) *(.rodata.*) }\n");
                    fprintf(ldf, "  .data : { *(.data) *(.data.*) }\n");
                    fprintf(ldf, "  .bss : { *(.bss) *(.bss.*) *(COMMON) }\n}\n");
                    fclose(ldf);
                }
                link_cmd_prefix = LD " -T /tmp/aether_ld.ld -o";
            }
            break;
    }

    /* Step 2: Assemble with nasm */
    char cmd[4096];

    /* If @layout or TARGET_BOOT, assemble as flat binary — direct output, no linker */
    if (cg->has_layout || cg->target == TARGET_BOOT) {
        snprintf(cmd, sizeof(cmd), "nasm -O0 -Wno-label-redef-late -f bin -o %s %s", output_file, asm_file);
        int ret = system(cmd);
        if (ret != 0) {
            fprintf(stderr, "nasm (flat binary) failed (exit %d)\n", ret);
            return ret;
        }
        /* Verify binary fits within layout_max if specified */
        if (cg->layout_max > 0) {
            FILE *f = fopen(output_file, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long actual = ftell(f);
                fclose(f);
                if (actual > cg->layout_max) {
                    fprintf(stderr, "ERROR: binary size %ld exceeds @layout max %ld\n",
                        actual, (long)cg->layout_max);
                    return 1;
                }
            }
        }
        return 0;
    }

    snprintf(cmd, sizeof(cmd), "nasm -O0 -Wno-label-redef-late -f %s -o %s %s", nasm_format, obj_file, asm_file);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "nasm failed (exit %d)\n", ret);
        return ret;
    }

    /* Step 3: Link */
    if (cg->target == TARGET_MACHO64) {
        snprintf(cmd, sizeof(cmd), "%s -o %s %s " SEGFAULT_HELPER, link_cmd_prefix, output_file, obj_file);
    } else if (link_cmd_prefix && link_cmd_prefix[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "%s %s %s " SEGFAULT_HELPER, link_cmd_prefix, output_file, obj_file);
    } else {
        /* No linker step needed (module targets) */
        /* Just copy object to output */
        snprintf(cmd, sizeof(cmd), "cp %s %s", obj_file, output_file);
    }
    ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "linker failed (exit %d)\n", ret);
        return ret;
    }

    /* Cleanup object file and linker script */
    remove(obj_file);
    remove("/tmp/aether_ld.ld");
    return 0;
}
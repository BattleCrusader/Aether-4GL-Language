#ifndef AETHER_CODEGEN_INTERNAL_H
#define AETHER_CODEGEN_INTERNAL_H

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
#include <sys/stat.h>

/* ── Forward declarations ──────────────────────────────────── */

typedef struct VarSlot VarSlot;
typedef struct AutoDrop AutoDrop;
typedef struct SrcLocEntry SrcLocEntry;
typedef struct GlobalSlot GlobalSlot;
typedef struct StructLayout StructLayout;
typedef struct VariantEntry VariantEntry;

/* ── Struct layout tracker ─────────────────────────────────── */

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

/* ── Enum layout tracker ───────────────────────────────────── */

typedef struct VariantEntry {
    const char *name;
    int discriminant;
    int payload_size;
    struct VariantEntry *next;
} VariantEntry;

/* ── Enum layout globals ───────────────────────────────────── */

extern VariantEntry *variant_entries;

/* ── Variable slot (stack frame) ────────────────────────────── */

struct VarSlot {
    AstNode *node;          /* AST node for this variable (for lookup) */
    const char *name;
    int offset;             /* stack offset from rbp (negative = local) */
    int stack_offset;       /* same as offset, for backward compat */
    int size;               /* size in bytes (aligned) */
    int actual_size;        /* actual type size before alignment */
    PrimType prim;          /* primitive type for codegen */
    bool is_mut;
    bool is_global;         /* file-scope let — stored in .bss, not on stack */
    int global_index;       /* index into cg->global_slots for .bss access */
    struct VarSlot *next;   /* linked list */
};

/* ── Auto-drop entry ────────────────────────────────────────── */

struct AutoDrop {
    const char *class_name;
    int stack_offset;
    AutoDrop *next;
};

/* ── Source location entry ──────────────────────────────────── */

struct SrcLocEntry {
    int label_num;       /* _aether_src_<label_num> in .text */
    int line;
    int col;
    const char *file;    /* source file name */
    SrcLocEntry *next;
};

/* ── Global variable slot ───────────────────────────────────── */

struct GlobalSlot {
    const char *name;
    int size;
    int index;
};

/* ── mkdir -p helper ────────────────────────────────────────── */

int mkdir_p(const char *path);

/* ── String literal processing ──────────────────────────────── */

int process_string_literal(StringView raw, char *out_buf, int max_len);

/* ── Type helpers ────────────────────────────────────────────── */

int type_size(AstNode *type);
PrimType prim_from_type(AstNode *type);
bool is_numeric_expr(AstNode *node);
bool is_string_expr(AstNode *node);
const char *prim_type_name(PrimType pt);
const char *type_node_name(AstNode *type);
void type_node_name_free(const char *name);

/* ── Struct/enum layout globals ────────────────────────────── */

extern StructLayout *struct_layouts;
extern StructLayout *enum_layouts;

/* ── Struct/enum layout ────────────────────────────────────── */

StructLayout *build_struct_layout(Arena *a, AstNode *node);
StructLayout *find_struct_layout(const char *name);
int find_field_offset(StructLayout *sl, const char *field_name);
StructLayout *build_enum_layout(Arena *a, AstNode *node);

/* ── Output helpers ─────────────────────────────────────────── */

void cg_write(Codegen *cg, const char *s);
void cg_write_fmt(Codegen *cg, const char *fmt, ...);
void cg_inst(Codegen *cg, const char *inst);
void cg_inst1(Codegen *cg, const char *inst, const char *arg);
void cg_inst2(Codegen *cg, const char *inst, const char *a1, const char *a2);
void cg_comment(Codegen *cg, const char *msg);
void cg_warn(Codegen *cg, AstNode *node, const char *msg);
void cg_load_var(Codegen *cg, VarSlot *slot);
void cg_store_var(Codegen *cg, VarSlot *slot);
const char *cg_emit_string(Codegen *cg, StringView sv);
void cg_indent(Codegen *cg);

/* ── String entry tracking (shared between output.c and top.c) ── */
typedef struct StringEntry {
    StringView sv;
    int label_num;
    struct StringEntry *next;
} StringEntry;
extern StringEntry *string_entries;
extern int string_label_counter;

/* ── Stack frame ────────────────────────────────────────────── */

VarSlot *compute_frame(Arena *a, AstNode *func, int *out_frame_size);
VarSlot *find_var_slot(VarSlot *slots, AstNode *node);
int find_var_offset(VarSlot *slots, AstNode *node);
int find_var_offset_by_name(VarSlot *slots, const char *name);
int find_var_size(VarSlot *slots, AstNode *node);
int find_var_size_by_name(VarSlot *slots, const char *name);
void cg_emit_cleanup_range(Codegen *cg, VarSlot *slots, int from, int to);

/* ── Expression codegen ─────────────────────────────────────── */

void cg_expr(Codegen *cg, AstNode *node, VarSlot *slots);

/* ── Statement codegen ─────────────────────────────────────── */

void cg_stmt(Codegen *cg, AstNode *node, VarSlot *slots);

/* ── Function codegen ───────────────────────────────────────── */

const char *entry_point_name(Target t);
void cg_func(Codegen *cg, AstNode *node);

/* ── Defer helpers ──────────────────────────────────────────── */

void cg_defer_push(Codegen *cg, AstNode *body);
void cg_emit_defers(Codegen *cg, VarSlot *slots);
void cg_defer_clear(Codegen *cg);

/* ── Memory map ────────────────────────────────────────────── */

void cg_verify_kernel_layout(Codegen *cg, AstNode *program);
void collect_externs_from_block(Codegen *cg, AstNode *node);

/* ── Aelib metadata ─────────────────────────────────────────── */

char *decl_name(AstNode *node);
bool decl_is_pub(AstNode *node);
uint8_t *build_func_type_data(AstNode *func, size_t *out_size);
uint8_t *build_struct_type_data(AstNode *decl, size_t *out_size);
uint8_t *build_enum_type_data(AstNode *decl, size_t *out_size);

/* ── Label allocator ────────────────────────────────────────── */

#define INITIAL_CAP 262144

static inline int cg_new_label(Codegen *cg) {
    return cg->label_counter++;
}

#endif /* AETHER_CODEGEN_INTERNAL_H */

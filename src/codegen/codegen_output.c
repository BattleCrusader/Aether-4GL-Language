#include "codegen_internal.h"

/* Output helpers
 * ================================================================ */

void cg_write(Codegen *cg, const char *s) {
    size_t len = strlen(s);
    if (cg->output_len + len + 1 > cg->output_cap) return;
    memcpy(cg->output + cg->output_len, s, len);
    cg->output_len += len;
}

void cg_write_fmt(Codegen *cg, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) cg_write(cg, buf);
}

void cg_indent(Codegen *cg) { cg_write(cg, "    "); }

void cg_load_var(Codegen *cg, VarSlot *slot) {
    char buf[64];
    snprintf(buf, sizeof(buf), "mov rax, [rbp%+d]", slot->stack_offset);
    cg_inst(cg, buf);
}

void cg_store_var(Codegen *cg, VarSlot *slot) {
    char buf[64];
    snprintf(buf, sizeof(buf), "mov [rbp%+d], rax", slot->stack_offset);
    cg_inst(cg, buf);
}

void cg_inst(Codegen *cg, const char *inst) {
    cg_indent(cg); cg_write_fmt(cg, "%s\n", inst);
}

void cg_inst1(Codegen *cg, const char *inst, const char *arg) {
    cg_indent(cg); cg_write_fmt(cg, "%-8s %s\n", inst, arg);
}

void cg_inst2(Codegen *cg, const char *inst, const char *a1, const char *a2) {
    cg_indent(cg); cg_write_fmt(cg, "%-8s %s, %s\n", inst, a1, a2);
}

int string_label_counter = 0;

/* String data tracker for .rodata emission */
StringEntry *string_entries = NULL;

/* Emit a section .rodata entry for a string literal and return its label */
/* Note: processes raw string (strips quotes, handles escapes) */
const char *cg_emit_string(Codegen *cg, StringView sv) {
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

void cg_comment(Codegen *cg, const char *s) {
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
void cg_warn(Codegen *cg, AstNode *node, const char *msg) {
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
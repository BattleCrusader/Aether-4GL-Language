#include "aether/c_transpiler.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Runtime helpers — emit C runtime functions
 * ────────────────────────────────────────────── */
void c_emit_runtime(CCodegen *cg) {
    /* Emit string type — .data field name matches NASM codegen convention */
    fputs("typedef struct { uint64_t len; const char *data; } string;\n\n", cg->out);

    /* Emit optional type — for T? parameters.
     * In C, we use the value type directly. Optionality is handled
     * by checking if ptr == NULL (for strings) or value == 0 (for ints). */
    fputs("typedef string optional;\n\n", cg->out);

    /* Emit slice type with element size */
    fputs("typedef struct { void *data; uint64_t len; uint64_t elem_size; } slice;\n\n", cg->out);

    /* Emit __aether_concat */
    fputs("static string __aether_concat(string a, string b) {\n", cg->out);
    fputs("    char *buf = malloc(a.len + b.len + 1);\n", cg->out);
    fputs("    if (!buf) return (string){ 0, NULL };\n", cg->out);
    fputs("    memcpy(buf, a.data, a.len);\n", cg->out);
    fputs("    memcpy(buf + a.len, b.data, b.len);\n", cg->out);
    fputs("    buf[a.len + b.len] = '\\0';\n", cg->out);
    fputs("    return (string){ a.len + b.len, buf };\n", cg->out);
    fputs("}\n\n", cg->out);

    /* Emit __aether_itoa */
    fputs("static string __aether_itoa(uint64_t value) {\n", cg->out);
    fputs("    char buf[21];\n", cg->out);
    fputs("    int len = 0;\n", cg->out);
    fputs("    if (value == 0) { buf[len++] = '0'; }\n", cg->out);
    fputs("    else {\n", cg->out);
    fputs("        char temp[21];\n", cg->out);
    fputs("        int i = 0;\n", cg->out);
    fputs("        while (value > 0) { temp[i++] = '0' + (value % 10); value /= 10; }\n", cg->out);
    fputs("        while (i > 0) { buf[len++] = temp[--i]; }\n", cg->out);
    fputs("    }\n", cg->out);
    fputs("    char *result = malloc(len + 1);\n", cg->out);
    fputs("    memcpy(result, buf, len);\n", cg->out);
    fputs("    result[len] = '\\0';\n", cg->out);
    fputs("    return (string){ len, result };\n", cg->out);
    fputs("}\n\n", cg->out);

    /* Emit print — only for host targets; stdlib provides its own for TARGET_LIB */
    if (cg->target != TARGET_LIB) {
        fputs("static void print(string s) {\n", cg->out);
        fputs("    fwrite(s.data, 1, s.len, stdout);\n", cg->out);
        fputs("}\n\n", cg->out);
    }

    /* Emit __aether_alloc / __aether_free */
    fputs("static void* __aether_alloc(uint64_t size) { return malloc((size_t)size); }\n", cg->out);
    fputs("static void __aether_free(void *ptr) { free(ptr); }\n\n", cg->out);

    /* Emit __aether_slice_concat — concatenate two slices */
    fputs("static slice __aether_slice_concat(slice a, slice b) {\n", cg->out);
    fputs("    if (b.len == 0) return a;\n", cg->out);
    fputs("    if (a.len == 0) return b;\n", cg->out);
    fputs("    uint64_t es = a.elem_size ? a.elem_size : b.elem_size;\n", cg->out);
    fputs("    if (!es) es = 8;\n", cg->out);
    fputs("    char *buf = malloc((a.len + b.len) * es);\n", cg->out);
    fputs("    if (!buf) return (slice){ NULL, 0, es };\n", cg->out);
    fputs("    memcpy(buf, a.data, a.len * es);\n", cg->out);
    fputs("    memcpy(buf + a.len * es, b.data, b.len * es);\n", cg->out);
    fputs("    return (slice){ buf, a.len + b.len, es };\n", cg->out);
    fputs("}\n\n", cg->out);

    /* Emit __aether_string_eq — compare two strings */
    fputs("static int __aether_string_eq(string a, string b) {\n", cg->out);
    fputs("    if (a.len != b.len) return 0;\n", cg->out);
    fputs("    return memcmp(a.data, b.data, a.len) == 0;\n", cg->out);
    fputs("}\n\n", cg->out);

    /* Emit error handling globals — used by try/catch/throw */
    fputs("int __aether_error_tag = 0;\n", cg->out);
    fputs("uint64_t __aether_error_value = 0;\n", cg->out);
    fputs("jmp_buf __aether_jmp_buf;\n\n", cg->out);

    /* Emit RC retain/release helpers */
    fputs("static void __aether_rc_retain(void *ptr) {\n", cg->out);
    fputs("    if (!ptr) return;\n", cg->out);
    fputs("    uint64_t *rc = (uint64_t*)ptr;\n", cg->out);
    fputs("    (*rc)++;\n", cg->out);
    fputs("}\n\n", cg->out);
    fputs("static void __aether_rc_release(void *ptr) {\n", cg->out);
    fputs("    if (!ptr) return;\n", cg->out);
    fputs("    uint64_t *rc = (uint64_t*)ptr;\n", cg->out);
    fputs("    if (*rc > 0) (*rc)--;\n", cg->out);
    fputs("    if (*rc == 0) free(ptr);\n", cg->out);
    fputs("}\n\n", cg->out);
}

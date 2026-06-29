#include "aether/c_transpiler.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Runtime helpers — emit C runtime functions
 * ────────────────────────────────────────────── */
void c_emit_runtime(CCodegen *cg) {
    /* Emit string type */
    fputs("typedef struct { uint64_t len; const char *ptr; } string;\n\n", cg->out);

    /* Emit optional type */
    fputs("typedef struct { uint8_t has_value; uint64_t value; } optional;\n\n", cg->out);

    /* Emit slice type */
    fputs("typedef struct { void *ptr; uint64_t len; } slice;\n\n", cg->out);

    /* Emit __aether_concat */
    fputs("static string __aether_concat(string a, string b) {\n", cg->out);
    fputs("    char *buf = malloc(a.len + b.len + 1);\n", cg->out);
    fputs("    if (!buf) return (string){ 0, NULL };\n", cg->out);
    fputs("    memcpy(buf, a.ptr, a.len);\n", cg->out);
    fputs("    memcpy(buf + a.len, b.ptr, b.len);\n", cg->out);
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

    /* Emit print */
    fputs("static void print(string s) {\n", cg->out);
    fputs("    fwrite(s.ptr, 1, s.len, stdout);\n", cg->out);
    fputs("}\n\n", cg->out);

    /* Emit __aether_alloc / __aether_free */
    fputs("static void* __aether_alloc(uint64_t size) { return malloc((size_t)size); }\n", cg->out);
    fputs("static void __aether_free(void *ptr) { free(ptr); }\n\n", cg->out);
}

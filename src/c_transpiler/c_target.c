#include "aether/c_transpiler.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Target emission — emit C preamble and postamble
 * ────────────────────────────────────────────── */

void c_emit_target_preamble(CCodegen *cg) {
    /* Emit standard includes */
    fputs("#include <stdio.h>\n", cg->out);
    fputs("#include <stdlib.h>\n", cg->out);
    fputs("#include <string.h>\n", cg->out);
    fputs("#include <stdint.h>\n", cg->out);
    fputs("#include <stdbool.h>\n\n", cg->out);
}

void c_emit_target_postamble(CCodegen *cg) {
    /* Nothing needed for host target */
    (void)cg;
}

/* ──────────────────────────────────────────────
 * Compile C source to native binary
 * ────────────────────────────────────────────── */
int c_compile(CCodegen *cg, const char *c_path, const char *output_path) {
    char cmd[4096];
    int ret;

    switch (cg->target) {
        case TARGET_HOST:
        case TARGET_MACHO64:
        case TARGET_ELF64_HOST:
            /* Standard host compilation */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c11 -O%d -o \"%s\" \"%s\" 2>&1",
                cg->opt_level, output_path, c_path);
            ret = system(cmd);
            break;

        case TARGET_FREESTANDING:
        case TARGET_KERNEL:
            /* Freestanding compilation */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c11 -ffreestanding -O%d -nostdlib -o \"%s\" \"%s\" 2>&1",
                cg->opt_level, output_path, c_path);
            ret = system(cmd);
            break;

        case TARGET_BOOT:
            /* Flat binary via objcopy */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c11 -ffreestanding -O%d -c -o /tmp/aether_boot.o \"%s\" && "
                "ld -Ttext 0x7C00 --oformat binary -o \"%s\" /tmp/aether_boot.o 2>&1",
                cg->opt_level, c_path, output_path);
            ret = system(cmd);
            break;

        case TARGET_UNIVERSAL:
            /* Universal binary: compile for x86_64 + ARM64, lipo together */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c11 -O%d -arch x86_64 -c -o /tmp/aether_univ_x86_64.o \"%s\" && "
                "gcc -std=c11 -O%d -arch arm64 -c -o /tmp/aether_univ_arm64.o \"%s\" && "
                "lipo -create /tmp/aether_univ_x86_64.o /tmp/aether_univ_arm64.o -output \"%s\" && "
                "rm -f /tmp/aether_univ_x86_64.o /tmp/aether_univ_arm64.o 2>&1",
                cg->opt_level, c_path, cg->opt_level, c_path, output_path);
            ret = system(cmd);
            break;

        case TARGET_UNIVERSAL_ALL:
            /* Universal binary: all available architectures */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c11 -O%d -arch x86_64 -arch arm64 -c -o /tmp/aether_univ_all.o \"%s\" && "
                "lipo -create /tmp/aether_univ_all.o -output \"%s\" && "
                "rm -f /tmp/aether_univ_all.o 2>&1",
                cg->opt_level, c_path, output_path);
            ret = system(cmd);
            break;

        case TARGET_LIB:
            /* .aelib library — handled by aelib.c, not here */
            fprintf(stderr, "C: TARGET_LIB not supported in c_compile\n");
            return 1;

        default:
            /* Fallback to host compilation */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c11 -O%d -o \"%s\" \"%s\" 2>&1",
                cg->opt_level, output_path, c_path);
            ret = system(cmd);
            break;
    }

    return ret;
}

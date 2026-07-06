#include "aether/c_transpiler.h"
#include "aether/aelib.h"
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
    fputs("#include <stdbool.h>\n", cg->out);
    fputs("#include <setjmp.h>\n", cg->out);
    fputs("#include <assert.h>\n", cg->out);
    fputs("#include <unistd.h>\n\n", cg->out);
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

    /* Run the C fixer on the generated code before compiling */
    char fixer_path[1024];
    snprintf(fixer_path, sizeof(fixer_path), "%s/tools/c_fixer.py",
        getenv("AETHER_ROOT") ? getenv("AETHER_ROOT") : ".");

    /* Try to find the fixer relative to the binary */
    {
        /* Check common locations */
        const char *locations[] = {
            "tools/c_fixer.py",
            "../tools/c_fixer.py",
            "/Volumes/Backup/Development/Project_Aether/compiler/tools/c_fixer.py",
            NULL
        };
        int found = 0;
        for (int li = 0; locations[li]; li++) {
            FILE *f = fopen(locations[li], "r");
            if (f) {
                fclose(f);
                snprintf(fixer_path, sizeof(fixer_path), "%s", locations[li]);
                found = 1;
                break;
            }
        }
        if (found) {
            snprintf(cmd, sizeof(cmd), "python3 \"%s\" \"%s\" 2>/dev/null", fixer_path, c_path);
            system(cmd);
        }
    }

    switch (cg->target) {
        case TARGET_HOST:
        case TARGET_MACHO64:
        case TARGET_ELF64_HOST: {
            /* Standard host compilation — extract .o from imported .aelib files */
            char aelib_args[4096] = "";
            for (int i = 0; i < cg->aelib_import_count; i++) {
                char o_path[1024];
                snprintf(o_path, sizeof(o_path), "/tmp/aelib_%d.o", i);
                if (aelib_extract_object(cg->aelib_import_paths[i], o_path) == 0) {
                    size_t cur = strlen(aelib_args);
                    snprintf(aelib_args + cur, sizeof(aelib_args) - cur,
                        " \"%s\"", o_path);
                }
            }
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c23 -O%d -o \"%s\" \"%s\"%s 2>&1",
                cg->opt_level, output_path, c_path, aelib_args);
            ret = system(cmd);
            break;
        }

        case TARGET_FREESTANDING:
        case TARGET_KERNEL:
            /* Freestanding compilation */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c23 -ffreestanding -O%d -nostdlib -o \"%s\" \"%s\" 2>&1",
                cg->opt_level, output_path, c_path);
            ret = system(cmd);
            break;

        case TARGET_BOOT:
            /* Flat binary via objcopy */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c23 -ffreestanding -O%d -c -o /tmp/aether_boot.o \"%s\" && "
                "ld -Ttext 0x7C00 --oformat binary -o \"%s\" /tmp/aether_boot.o 2>&1",
                cg->opt_level, c_path, output_path);
            ret = system(cmd);
            break;

        case TARGET_UNIVERSAL:
            /* Universal binary: compile for x86_64 + ARM64, lipo together */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c23 -O%d -arch x86_64 -c -o /tmp/aether_univ_x86_64.o \"%s\" && "
                "gcc -std=c23 -O%d -arch arm64 -c -o /tmp/aether_univ_arm64.o \"%s\" && "
                "lipo -create /tmp/aether_univ_x86_64.o /tmp/aether_univ_arm64.o -output \"%s\" && "
                "rm -f /tmp/aether_univ_x86_64.o /tmp/aether_univ_arm64.o 2>&1",
                cg->opt_level, c_path, cg->opt_level, c_path, output_path);
            ret = system(cmd);
            break;

        case TARGET_UNIVERSAL_ALL:
            /* Universal binary: all available architectures */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c23 -O%d -arch x86_64 -arch arm64 -c -o /tmp/aether_univ_all.o \"%s\" && "
                "lipo -create /tmp/aether_univ_all.o -output \"%s\" && "
                "rm -f /tmp/aether_univ_all.o 2>&1",
                cg->opt_level, c_path, output_path);
            ret = system(cmd);
            break;

        case TARGET_LIB:
            /* .aelib library: compile to .o file for archiving.
               Use -masm=intel for NASM-style inline asm in stdlib. */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c23 -O%d -masm=intel -c -o \"%s\" \"%s\" 2>&1",
                cg->opt_level, output_path, c_path);
            ret = system(cmd);
            break;

        default:
            /* Fallback to host compilation */
            snprintf(cmd, sizeof(cmd),
                "gcc -std=c23 -O%d -o \"%s\" \"%s\" 2>&1",
                cg->opt_level, output_path, c_path);
            ret = system(cmd);
            break;
    }

    return ret;
}

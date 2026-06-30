/*
 * aether.c — Bootstrap compiler CLI.
 * Reads .ae files, tokenizes, parses, analyzes, generates NASM,
 * assembles with nasm, and links with ld (freestanding ELF64)
 * or clang (Mach-O 64 for macOS).
 *
 * Also supports:
 *   aether init <name>   — scaffold a new project
 *   aether.toml          — project manifest discovery
 *   --linker-script      — custom linker script
 *   New targets: kernel, module, binary, boot
 */

#include "aether/tokenizer.h"
#include "aether/lexer.h"
#include "aether/ast.h"
#include "aether/parser.h"
#include "aether/semantic.h"
#include "aether/codegen.h"
#include "aether/c_transpiler.h"
#include "aether/optimizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>   /* for close, mkstemp, unlink */
#include <errno.h>

static void usage(const char *prog) {
    fprintf(stderr, "Aether Compiler v0.5 (commit %s, branch %s)\n",
        GIT_HASH, GIT_BRANCH);
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s [options] <file.ae>              Compile a source file\n", prog);
    fprintf(stderr, "  %s run <file.ae> [args]             Compile and run immediately\n", prog);
    fprintf(stderr, "  %s init|new <project-name>          Scaffold a new project\n", prog);
    fprintf(stderr, "  %s fmt <file.ae>                    Format source code\n", prog);
    fprintf(stderr, "  %s doc <file.ae>                    Generate documentation\n", prog);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -o, --output <file>      Output file (default: /tmp/<name>)\n");
    fprintf(stderr, "  --target <target>        Output target (default: host):\n");
    fprintf(stderr, "    host                   Auto-detect host format (default)\n");
    fprintf(stderr, "    x86_64-freestanding    Aether OS ELF64\n");
    fprintf(stderr, "    macho64                Mach-O 64 (macOS)\n");
    fprintf(stderr, "    elf64-host             Native ELF64 (Linux)\n");
    fprintf(stderr, "    kernel                 Kernel ELF at 0x1000000\n");
    fprintf(stderr, "    module                 Aether OS .ko module\n");
    fprintf(stderr, "    binary                 /bin/ userland ELF at 0x2000000\n");
    fprintf(stderr, "    boot                   Flat binary boot sector\n");
    fprintf(stderr, "    universal              Universal binary (x86_64 + ARM64)\n");
    fprintf(stderr, "    universal-all           Universal binary (all architectures)\n");
    fprintf(stderr, "    lib                    .aelib library archive\n");
    fprintf(stderr, "  -L, --linker-script <f>  Custom linker script\n");
    fprintf(stderr, "  -S                       Stop after C source emission (emit .c)\n");
    fprintf(stderr, "  -O0                      No optimization (fastest compile)\n");
    fprintf(stderr, "  -O1                      Basic optimization (default)\n");
    fprintf(stderr, "  -O2                      Full optimization\n");
    fprintf(stderr, "  -Oz                      Optimize for size\n");
    fprintf(stderr, "  --dump-ast               Print AST and exit\n");
    fprintf(stderr, "  --dump-tokens            Print tokens and exit\n");
    fprintf(stderr, "  --version                Print version and exit\n");
    fprintf(stderr, "  -v, --verbose            Verbose output\n");
}

/* Create directory (mkdir -p equivalent) */
static int mkdir_p(const char *path) {
    char tmp[1024];
    size_t len = strlen(path);
    if (len > sizeof(tmp) - 1) return -1;
    memcpy(tmp, path, len + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

/* Scaffold a new project: aether init <name> */
static int cmd_init(const char *prog, const char *name) {
    (void)prog;
    char dir[1024];
    char path[2048];

    snprintf(dir, sizeof(dir), "%s", name);
    if (mkdir_p(dir) != 0 && errno != EEXIST) {
        fprintf(stderr, "Error: cannot create directory '%s'\n", dir);
        return 1;
    }

    /* Create src/ */
    snprintf(path, sizeof(path), "%s/src", dir);
    mkdir(path, 0755);

    /* Create tests/ */
    snprintf(path, sizeof(path), "%s/tests", dir);
    mkdir(path, 0755);

    /* Create aether.toml */
    snprintf(path, sizeof(path), "%s/aether.toml", dir);
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Error: cannot create '%s'\n", path);
        return 1;
    }
    fprintf(f, "[project]\n");
    fprintf(f, "name = \"%s\"\n", name);
    fprintf(f, "version = \"0.1.0\"\n");
    fprintf(f, "\n");
    fprintf(f, "[build]\n");
    fprintf(f, "target = \"host\"\n");
    fprintf(f, "output = \"%s\"\n", name);
    fprintf(f, "# linker-script = \"link.ld\"\n");
    fclose(f);

    /* Create src/main.ae */
    snprintf(path, sizeof(path), "%s/src/main.ae", dir);
    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Error: cannot create '%s'\n", path);
        return 1;
    }
    fprintf(f, "# %s — Aether project\n", name);
    fprintf(f, "# Generated by `aether init`\n\n");
    fprintf(f, "func main(): u64 {\n");
    fprintf(f, "    print(\"Hello from Aether!\\n\")\n");
    fprintf(f, "    return 0\n");
    fprintf(f, "}\n");
    fclose(f);

    printf("Created project '%s' at %s/\n", name, dir);
    printf("  %s/aether.toml\n", dir);
    printf("  %s/src/main.ae\n", dir);
    printf("  %s/tests/\n", dir);
    printf("\n");
    printf("To build: cd %s && aether src/main.ae\n", dir);
    return 0;
}

/* Simple TOML-like parsing: read a file line by line, extract key = value */
/* Returns 0 on success, -1 on error. */
/* Supported keys: target, output, linker-script (under [build] section) */
static int parse_aether_toml(const char *path,
                             char *target_out, size_t target_sz,
                             char *output_out, size_t output_sz,
                             char *linker_script_out, size_t linker_sz)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    int in_build_section = 0;
    char line[512];
    int ret = 0;

    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* Skip blank and comment lines */
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;

        /* Section headers */
        if (*p == '[') {
            in_build_section = (strcmp(p, "[build]") == 0);
            continue;
        }

        if (!in_build_section) continue;

        /* Parse key = value */
        const char *eq = strchr(p, '=');
        if (!eq) continue;

        char key[128];
        int klen = (int)(size_t)(eq - p);
        /* Trim trailing spaces from key */
        while (klen > 0 && (p[klen-1] == ' ' || p[klen-1] == '\t')) klen--;
        if (klen > 127) klen = 127;
        memcpy(key, p, klen);
        key[klen] = '\0';

        /* Skip spaces after = */
        const char *val = eq + 1;
        while (*val == ' ' || *val == '\t') val++;

        /* Strip surrounding quotes */
        const char *vstart = val;
        size_t vlen = strlen(val);
        if (vlen > 1 && val[0] == '"' && val[vlen-1] == '"') {
            vstart = val + 1;
            vlen = vlen - 2;
        }

        if (strcmp(key, "target") == 0) {
            size_t copy = vlen < target_sz - 1 ? vlen : target_sz - 1;
            memcpy(target_out, vstart, copy);
            target_out[copy] = '\0';
        } else if (strcmp(key, "output") == 0) {
            size_t copy = vlen < output_sz - 1 ? vlen : output_sz - 1;
            memcpy(output_out, vstart, copy);
            output_out[copy] = '\0';
        } else if (strcmp(key, "linker-script") == 0 || strcmp(key, "linker_script") == 0) {
            size_t copy = vlen < linker_sz - 1 ? vlen : linker_sz - 1;
            memcpy(linker_script_out, vstart, copy);
            linker_script_out[copy] = '\0';
        }
    }

    fclose(f);
    return ret;
}

/* Map a string target name to Target enum */
/* Returns -1 on unknown target */
static int parse_target_name(const char *t) {
    if (strcmp(t, "host") == 0)               return (int)TARGET_HOST;
    if (strcmp(t, "x86_64-freestanding") == 0) return (int)TARGET_FREESTANDING;
    if (strcmp(t, "macho64") == 0)             return (int)TARGET_MACHO64;
    if (strcmp(t, "elf64-host") == 0)          return (int)TARGET_ELF64_HOST;
    if (strcmp(t, "kernel") == 0)              return (int)TARGET_KERNEL;
    if (strcmp(t, "module") == 0)              return (int)TARGET_MODULE;
    if (strcmp(t, "binary") == 0)              return (int)TARGET_BINARY;
    if (strcmp(t, "boot") == 0)                return (int)TARGET_BOOT;
    if (strcmp(t, "asm-x86_64") == 0)          return (int)TARGET_ASM_X86_64;
    if (strcmp(t, "asm-arm64") == 0)           return (int)TARGET_ASM_ARM64;
    if (strcmp(t, "asm-riscv64") == 0)         return (int)TARGET_ASM_RISCV64;
    if (strcmp(t, "universal") == 0)           return (int)TARGET_UNIVERSAL;
    if (strcmp(t, "universal-all") == 0)       return (int)TARGET_UNIVERSAL_ALL;
    if (strcmp(t, "lib") == 0)                 return (int)TARGET_LIB;
    /* Parse "universal-x86_64-arm64" style explicit arch lists */
    if (strncmp(t, "universal-", 10) == 0)     return (int)TARGET_UNIVERSAL;
    return -1;
}

/* Return true if the file exists */
static int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* ================================================================
 * P09.12 — aether fmt: Source code formatter
 * ================================================================ */
static int cmd_fmt(const char *path) {
    /* Read the source file */
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Error: cannot open '%s'\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *source = (char *)malloc((size_t)flen + 1);
    if (!source) { fclose(f); return 1; }
    size_t rlen = fread(source, 1, (size_t)flen, f);
    source[rlen] = '\0';
    fclose(f);

    /* Tokenize to understand structure */
    Tokenizer *t = tokenizer_create(source, rlen, path);
    if (!t) { free(source); return 1; }

    /* Build a formatted output by re-emitting tokens with normalized spacing.
     * This is a simplified formatter that normalizes indentation and spacing. */
    char *out = (char *)malloc(rlen * 2 + 1);
    if (!out) { tokenizer_destroy(t); free(source); return 1; }
    size_t out_pos = 0;
    int indent = 0;
    int at_line_start = 1;
    int prev_newline = 0;

    while (1) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_ERROR) {
            fprintf(stderr, "Error: %.*s\n", (int)tok.text.len, tok.text.data);
            tokenizer_destroy(t); free(source); free(out);
            return 1;
        }

        /* Handle newlines */
        if (tok.type == TOKEN_NEWLINE) {
            if (!prev_newline) {
                out[out_pos++] = '\n';
                prev_newline = 1;
                at_line_start = 1;
            }
            continue;
        }
        prev_newline = 0;

        /* Add indentation at line start */
        if (at_line_start) {
            for (int i = 0; i < indent; i++) {
                out[out_pos++] = ' ';
                out[out_pos++] = ' ';
                out[out_pos++] = ' ';
                out[out_pos++] = ' ';
            }
            at_line_start = 0;
        }

        /* Add space before certain tokens */
        if (tok.type == TOKEN_KW_IF || tok.type == TOKEN_KW_WHILE ||
            tok.type == TOKEN_KW_FOR || tok.type == TOKEN_KW_RETURN ||
            tok.type == TOKEN_KW_LET || tok.type == TOKEN_KW_FUNC) {
            if (out_pos > 0 && out[out_pos-1] != '\n' && out[out_pos-1] != ' ')
                out[out_pos++] = ' ';
        }

        /* Emit the token text */
        size_t tlen = tok.text.len;
        if (out_pos + tlen + 1 > rlen * 2) break;
        memcpy(out + out_pos, tok.text.data, tlen);
        out_pos += tlen;

        /* Add space after certain tokens */
        if (tok.type == TOKEN_COMMA || tok.type == TOKEN_SEMICOLON) {
            out[out_pos++] = ' ';
        }
        if (tok.type == TOKEN_COLON) {
            out[out_pos++] = ' ';
        }
        if (tok.type == TOKEN_ARROW) {
            out[out_pos++] = ' ';
        }
    }

    out[out_pos] = '\0';
    tokenizer_destroy(t);

    /* Write formatted output back to the file */
    f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot write '%s'\n", path);
        free(source); free(out);
        return 1;
    }
    fwrite(out, 1, out_pos, f);
    fclose(f);

    printf("Formatted: %s\n", path);
    free(source);
    free(out);
    return 0;
}

/* ================================================================
 * P09.14 — aether asm: Show generated assembly
 * ================================================================ */
static int cmd_asm(const char *prog, int argc, char **argv) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    Target target = TARGET_ASM_X86_64; /* default: x86_64 assembly */
    int verbose = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            const char *t = argv[++i];
            if (strcmp(t, "x86_64") == 0 || strcmp(t, "asm-x86_64") == 0)
                target = TARGET_ASM_X86_64;
            else if (strcmp(t, "arm64") == 0 || strcmp(t, "asm-arm64") == 0)
                target = TARGET_ASM_ARM64;
            else if (strcmp(t, "riscv64") == 0 || strcmp(t, "asm-riscv64") == 0)
                target = TARGET_ASM_RISCV64;
            else {
                fprintf(stderr, "Unknown asm target: %s (use x86_64, arm64, or riscv64)\n", t);
                return 1;
            }
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(prog);
            return 1;
        }
    }

    if (!input_file) {
        fprintf(stderr, "Error: no input file specified\n");
        usage(prog);
        return 1;
    }

    /* Read, parse, analyze, generate */
    FILE *f = fopen(input_file, "rb");
    if (!f) { fprintf(stderr, "Error: cannot open '%s'\n", input_file); return 1; }
    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *source = (char *)malloc((size_t)flen + 1);
    if (!source) { fclose(f); return 1; }
    size_t rlen = fread(source, 1, (size_t)flen, f);
    source[rlen] = '\0';
    fclose(f);

    Parser *parser = parser_create(source, rlen, input_file);
    AstNode *program = parser_parse(parser);
    if (parser->error_count > 0) {
        fprintf(stderr, "Parse failed with %d errors\n", parser->error_count);
        parser_destroy(parser); free(source);
        return 1;
    }

    Arena *sa_arena = arena_create();
    SemanticAnalyzer *sa = semantic_create(sa_arena);
    semantic_analyze(sa, program);
    if (sa->error_count > 0) {
        fprintf(stderr, "Semantic analysis failed\n");
        parser_destroy(parser); free(source); arena_destroy(sa_arena);
        return 1;
    }

    Arena *cg_arena = arena_create();
    Codegen *cg = codegen_create(cg_arena);
    codegen_set_target(cg, target);
    codegen_generate(cg, program);

    const char *asm_text = codegen_get_asm(cg);
    if (!asm_text) {
        fprintf(stderr, "Code generation failed\n");
        parser_destroy(parser); free(source);
        arena_destroy(sa_arena); arena_destroy(cg_arena);
        return 1;
    }

    if (output_file) {
        FILE *of = fopen(output_file, "w");
        if (!of) { fprintf(stderr, "Error: cannot write '%s'\n", output_file); return 1; }
        fwrite(asm_text, 1, strlen(asm_text), of);
        fclose(of);
        printf("Wrote assembly to %s\n", output_file);
    } else {
        /* Print to stdout with source annotations */
        printf("; Assembly for: %s\n", input_file);
        printf("; Target: %s\n", target_name(target));
        printf("; ========================================\n");
        printf("%s", asm_text);
    }

    parser_destroy(parser);
    free(source);
    arena_destroy(sa_arena);
    arena_destroy(cg_arena);
    return 0;
}

/* ================================================================
 * P09.15 — aether inspect: ELF binary inspection
 * ================================================================ */
static int cmd_inspect(const char *path) {
    /* Read the ELF file */
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Error: cannot open '%s'\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read ELF header */
    unsigned char header[64];
    if (flen < 64) {
        fprintf(stderr, "Error: file too small to be an ELF\n");
        fclose(f); return 1;
    }
    if (fread(header, 1, 64, f) != 64) {
        fprintf(stderr, "Error: cannot read ELF header\n");
        fclose(f); return 1;
    }

    /* Verify ELF magic */
    if (header[0] != 0x7F || header[1] != 'E' || header[2] != 'L' || header[3] != 'F') {
        fprintf(stderr, "Error: not an ELF file (bad magic)\n");
        fclose(f); return 1;
    }

    printf("=== ELF Binary: %s ===\n", path);
    printf("  Class:      %s\n", header[4] == 1 ? "ELF32" : header[4] == 2 ? "ELF64" : "Unknown");
    printf("  Endian:     %s\n", header[5] == 1 ? "Little" : header[5] == 2 ? "Big" : "Unknown");
    printf("  OS/ABI:     %s\n", header[7] == 0 ? "UNIX System V" : "Other");
    printf("  Type:       ");
    uint16_t e_type;
    memcpy(&e_type, header + 16, 2);
    switch (e_type) {
        case 0: printf("NONE\n"); break;
        case 1: printf("REL (Relocatable)\n"); break;
        case 2: printf("EXEC (Executable)\n"); break;
        case 3: printf("DYN (Shared object)\n"); break;
        default: printf("0x%x\n", e_type); break;
    }

    uint16_t e_machine;
    memcpy(&e_machine, header + 18, 2);
    printf("  Machine:    ");
    switch (e_machine) {
        case 0x3E: printf("x86_64\n"); break;
        case 0xB7: printf("ARM64 (AArch64)\n"); break;
        case 0xF3: printf("RISC-V\n"); break;
        default: printf("0x%x\n", e_machine); break;
    }

    uint64_t e_entry;
    memcpy(&e_entry, header + 24, 8);
    printf("  Entry:      0x%lx\n", (unsigned long)e_entry);

    uint64_t e_shoff;
    memcpy(&e_shoff, header + 40, 8);
    uint16_t e_shnum;
    memcpy(&e_shnum, header + 60, 2);
    uint16_t e_shentsize;
    memcpy(&e_shentsize, header + 58, 2);
    uint16_t e_shstrndx;
    memcpy(&e_shstrndx, header + 62, 2);

    printf("  Sections:   %d (at offset 0x%lx)\n", e_shnum, (unsigned long)e_shoff);

    /* Read section headers */
    if (e_shoff > 0 && e_shnum > 0 && e_shentsize >= 64) {
        /* Read section name string table */
        unsigned char *shstrtab = NULL;
        if (e_shstrndx < e_shnum) {
            unsigned char shdr[64];
            fseek(f, (long)(e_shoff + e_shstrndx * e_shentsize), SEEK_SET);
            if (fread(shdr, 1, 64, f) == 64) {
                uint64_t sh_offset;
                uint64_t sh_size;
                memcpy(&sh_offset, shdr + 24, 8);
                memcpy(&sh_size, shdr + 32, 8);
                if (sh_size > 0) {
                    shstrtab = (unsigned char *)malloc((size_t)sh_size);
                    if (shstrtab) {
                        fseek(f, (long)sh_offset, SEEK_SET);
                        fread(shstrtab, 1, (size_t)sh_size, f);
                    }
                }
            }
        }

        printf("\n  Section Headers:\n");
        printf("    %-20s %-10s %-10s %s\n", "Name", "Type", "Size", "Flags");
        printf("    %-20s %-10s %-10s %s\n", "----", "----", "----", "-----");

        for (int i = 0; i < e_shnum; i++) {
            unsigned char shdr[64];
            fseek(f, (long)(e_shoff + i * e_shentsize), SEEK_SET);
            if (fread(shdr, 1, 64, f) != 64) break;

            uint32_t sh_name;
            uint32_t sh_type;
            uint64_t sh_size;
            uint64_t sh_flags;
            memcpy(&sh_name, shdr, 4);
            memcpy(&sh_type, shdr + 4, 4);
            memcpy(&sh_flags, shdr + 8, 8);
            memcpy(&sh_size, shdr + 32, 8);

            const char *sname = (shstrtab && sh_name > 0) ?
                (const char *)(shstrtab + sh_name) : "";
            const char *stype = "?";
            switch (sh_type) {
                case 0: stype = "NULL"; break;
                case 1: stype = "PROGBITS"; break;
                case 2: stype = "SYMTAB"; break;
                case 3: stype = "STRTAB"; break;
                case 4: stype = "RELA"; break;
                case 8: stype = "NOBITS"; break;
            }

            char flags[8] = {0};
            int fi = 0;
            if (sh_flags & 1) flags[fi++] = 'W';
            if (sh_flags & 2) flags[fi++] = 'A';
            if (sh_flags & 4) flags[fi++] = 'X';
            if (sh_flags & 0x10) flags[fi++] = 'M';
            if (sh_flags & 0x20) flags[fi++] = 'S';

            printf("    %-20s %-10s %-10lu %s\n",
                   sname, stype, (unsigned long)sh_size, flags);
        }

        /* Read symbol table */
        for (int i = 0; i < e_shnum; i++) {
            unsigned char shdr[64];
            fseek(f, (long)(e_shoff + i * e_shentsize), SEEK_SET);
            if (fread(shdr, 1, 64, f) != 64) break;

            uint32_t sh_type;
            uint64_t sh_offset;
            uint64_t sh_size;
            uint64_t sh_entsize;
            memcpy(&sh_type, shdr + 4, 4);
            memcpy(&sh_offset, shdr + 24, 8);
            memcpy(&sh_size, shdr + 32, 8);
            memcpy(&sh_entsize, shdr + 56, 8);

            if (sh_type == 2 && sh_entsize > 0) { /* SYMTAB */
                int sym_count = (int)(sh_size / sh_entsize);
                printf("\n  Symbol Table:\n");
                printf("    %-20s %-10s %s\n", "Name", "Value", "Type");
                printf("    %-20s %-10s %s\n", "----", "-----", "----");

                /* Find associated string table */
                uint32_t sh_link;
                memcpy(&sh_link, shdr + 40, 4);
                unsigned char *strtab = NULL;
                if (sh_link < (uint32_t)e_shnum) {
                    unsigned char strhdr[64];
                    fseek(f, (long)(e_shoff + sh_link * e_shentsize), SEEK_SET);
                    if (fread(strhdr, 1, 64, f) == 64) {
                        uint64_t str_offset, str_size;
                        memcpy(&str_offset, strhdr + 24, 8);
                        memcpy(&str_size, strhdr + 32, 8);
                        if (str_size > 0) {
                            strtab = (unsigned char *)malloc((size_t)str_size);
                            if (strtab) {
                                fseek(f, (long)str_offset, SEEK_SET);
                                fread(strtab, 1, (size_t)str_size, f);
                            }
                        }
                    }
                }

                for (int j = 0; j < sym_count; j++) {
                    unsigned char sym[24];
                    fseek(f, (long)(sh_offset + j * sh_entsize), SEEK_SET);
                    if (fread(sym, 1, 24, f) != 24) break;

                    uint32_t sym_name;
                    uint64_t sym_value;
                    unsigned char sym_info;
                    memcpy(&sym_name, sym, 4);
                    memcpy(&sym_value, sym + 8, 8);
                    sym_info = sym[12];

                    const char *sname = (strtab && sym_name > 0) ?
                        (const char *)(strtab + sym_name) : "";
                    char stype_c = '?';
                    switch (sym_info & 0x0F) {
                        case 0: stype_c = 'N'; break; /* NOTYPE */
                        case 1: stype_c = 'O'; break; /* OBJECT */
                        case 2: stype_c = 'F'; break; /* FUNC */
                        case 3: stype_c = 'S'; break; /* SECTION */
                    }

                    if (sname[0]) {
                        printf("    %-20s 0x%08lx %c\n",
                               sname, (unsigned long)sym_value, stype_c);
                    }
                }
                free(strtab);
            }
        }

        free(shstrtab);
    }

    /* File size */
    printf("\n  File size: %ld bytes\n", flen);

    fclose(f);
    return 0;
}

/* ================================================================
 * P09.13 — aether doc: Documentation generator
 * ================================================================ */
static int cmd_doc(const char *path) {
    /* Read the source file and extract doc comments */
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Error: cannot open '%s'\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *source = (char *)malloc((size_t)flen + 1);
    if (!source) { fclose(f); return 1; }
    size_t rlen = fread(source, 1, (size_t)flen, f);
    source[rlen] = '\0';
    fclose(f);

    /* Tokenize and extract doc comments */
    Tokenizer *t = tokenizer_create(source, rlen, path);
    if (!t) { free(source); return 1; }

    printf("# Documentation for: %s\n\n", path);
    int in_block_comment = 0;

    while (1) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_ERROR) break;

        /* Print function signatures as documentation */
        if (tok.type == TOKEN_KW_FUNC) {
            printf("\n");
            while (tok.type != TOKEN_NEWLINE && tok.type != TOKEN_EOF) {
                tok = tokenizer_next(t);
                if (tok.type == TOKEN_EOF) break;
                printf("%.*s ", (int)tok.text.len, tok.text.data);
            }
            printf("\n");
        }
    }

    tokenizer_destroy(t);
    free(source);
    return 0;
}

int main(int argc, char **argv) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    int stop_after_asm = 0;
    int dump_ast = 0;
    int dump_tokens = 0;
    int verbose = 0;
    int run_mode = 0;
    int opt_level = 1; /* -O1 default */
    Target target = TARGET_HOST; /* default: auto-detect */
    const char *linker_script = NULL;

    /* Check for subcommands: init, new, run, fmt, asm, inspect, version, help */
    if (argc > 1) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0) {
            printf("Aether Compiler v0.5 (commit %s, branch %s)\n", GIT_HASH, GIT_BRANCH);
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[1], "init") == 0 || strcmp(argv[1], "new") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Error: 'init' requires a project name\n");
                usage(argv[0]);
                return 1;
            }
            return cmd_init(argv[0], argv[2]);
        }
        if (strcmp(argv[1], "run") == 0) {
            run_mode = 1;
        }
        if (strcmp(argv[1], "doc") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Error: 'doc' requires a file\n");
                usage(argv[0]);
                return 1;
            }
            return cmd_doc(argv[2]);
        }
        if (strcmp(argv[1], "fmt") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Error: 'fmt' requires a file\n");
                usage(argv[0]);
                return 1;
            }
            return cmd_fmt(argv[2]);
        }
        if (strcmp(argv[1], "asm") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Error: 'asm' requires a file\n");
                usage(argv[0]);
                return 1;
            }
            return cmd_asm(argv[0], argc, argv);
        }
        if (strcmp(argv[1], "inspect") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Error: 'inspect' requires a file\n");
                usage(argv[0]);
                return 1;
            }
            return cmd_inspect(argv[2]);
        }
    }

    /* Parse args */
    int argi = (run_mode) ? 2 : 1;
    int run_args_start = 0;  /* index of first program arg in argv */
    for (int i = argi; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) output_file = argv[++i];
        } else if (strcmp(argv[i], "--target") == 0) {
            if (i + 1 < argc) {
                const char *t = argv[++i];
                int parsed = parse_target_name(t);
                if (parsed < 0) {
                    fprintf(stderr, "Unknown target: %s\n", t);
                    usage(argv[0]);
                    return 1;
                }
                target = (Target)parsed;
            }
        } else if (strcmp(argv[i], "-L") == 0 || strcmp(argv[i], "--linker-script") == 0) {
            if (i + 1 < argc) linker_script = argv[++i];
        } else if (strcmp(argv[i], "-S") == 0) {
            stop_after_asm = 1;
        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            dump_ast = 1;
        } else if (strcmp(argv[i], "--dump-tokens") == 0) {
            dump_tokens = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-O0") == 0) {
            opt_level = 0;
        } else if (strcmp(argv[i], "-O1") == 0) {
            opt_level = 1;
        } else if (strcmp(argv[i], "-O2") == 0) {
            opt_level = 2;
        } else if (strcmp(argv[i], "-Oz") == 0) {
            opt_level = 3;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            if (run_mode && input_file) {
                /* In run mode, extra non-flag args are program arguments */
                if (run_args_start == 0) run_args_start = i;
            } else {
                input_file = argv[i];
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    /* If no input file, check for aether.toml in cwd */
    if (!input_file) {
        if (file_exists("aether.toml")) {
            char toml_target[256] = {0};
            char toml_output[256] = {0};
            char toml_linker[256] = {0};
            if (parse_aether_toml("aether.toml", toml_target, sizeof(toml_target),
                                  toml_output, sizeof(toml_output),
                                  toml_linker, sizeof(toml_linker)) == 0)
            {
                if (toml_target[0]) {
                    int p = parse_target_name(toml_target);
                    if (p >= 0) target = (Target)p;
                    if (verbose) printf("aether.toml: target = %s\n", toml_target);
                }
                if (toml_output[0] && !output_file) {
                    char *dup = (char *)malloc(strlen(toml_output) + 1);
                    if (dup) { memcpy(dup, toml_output, strlen(toml_output) + 1); output_file = dup; }
                    if (verbose) printf("aether.toml: output = %s\n", toml_output);
                }
                if (toml_linker[0] && !linker_script) {
                    char *dup = (char *)malloc(strlen(toml_linker) + 1);
                    if (dup) { memcpy(dup, toml_linker, strlen(toml_linker) + 1); linker_script = dup; }
                    if (verbose) printf("aether.toml: linker-script = %s\n", toml_linker);
                }
            }
            fprintf(stderr, "Error: aether.toml found but no .ae file specified on command line\n");
            usage(argv[0]);
            return 1;
        }
        fprintf(stderr, "Error: no input file specified\n");
        usage(argv[0]);
        return 1;
    }

    /* Default output name */
    if (!output_file) {
        if (target == TARGET_HOST || target == TARGET_MACHO64 || target == TARGET_ELF64_HOST) {
            /* Write to /tmp/kernel/ so binaries don't pollute source tree */
            const char *fname = strrchr(input_file, '/');
            fname = fname ? fname + 1 : input_file;
            const char *dot = strrchr(fname, '.');
            if (dot && strcmp(dot, ".ae") == 0) {
                size_t base_len = (size_t)(dot - fname);
                char buf[1024];
                int n = snprintf(buf, sizeof(buf), "/tmp/kernel/%.*s", (int)base_len, fname);
                if (n > 0 && n < (int)sizeof(buf)) {
                    output_file = (const char *)malloc((size_t)n + 1);
                    if (output_file) memcpy((char *)output_file, buf, (size_t)n + 1);
                }
            } else {
                output_file = "/tmp/kernel/aether.out";
            }
        } else if (target == TARGET_LIB) {
            /* Default .aelib output: same name as input with .aelib extension */
            const char *fname = strrchr(input_file, '/');
            fname = fname ? fname + 1 : input_file;
            const char *dot = strrchr(fname, '.');
            if (dot && strcmp(dot, ".ae") == 0) {
                size_t base_len = (size_t)(dot - fname);
                char buf[1024];
                int n = snprintf(buf, sizeof(buf), "%.*s.aelib", (int)base_len, fname);
                if (n > 0 && n < (int)sizeof(buf)) {
                    output_file = (const char *)malloc((size_t)n + 1);
                    if (output_file) memcpy((char *)output_file, buf, (size_t)n + 1);
                }
            } else {
                output_file = "lib.aelib";
            }
        } else {
            output_file = "a.out";
        }
    }

    /* Ensure output directory exists */
    if (output_file && output_file[0] == '/') {
        char dir_copy[1024];
        snprintf(dir_copy, sizeof(dir_copy), "%s", output_file);
        char *slash = strrchr(dir_copy, '/');
        if (slash) {
            *slash = '\0';
            mkdir_p(dir_copy);
        }
    }

    /* Read input file */
    FILE *f = fopen(input_file, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s'\n", input_file);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long file_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *source = (char *)malloc((size_t)file_len + 1);
    if (!source) { fclose(f); return 1; }
    size_t read_len = fread(source, 1, (size_t)file_len, f);
    source[read_len] = '\0';
    fclose(f);

    /* Phase 1: Tokenize */
    if (dump_tokens) {
        Tokenizer *t = tokenizer_create(source, read_len, input_file);
        int i = 0;
        while (1) {
            Token tok = tokenizer_next(t);
            printf("%3d: [%-12s] '%.*s'", i, token_type_name(tok.type),
                   (int)tok.text.len, tok.text.data);
            if (tok.type == TOKEN_INT_LITERAL)
                printf(" val=%llu", (unsigned long long)tok.val.int_value);
            if (tok.type == TOKEN_FLOAT_LITERAL)
                printf(" val=%f", tok.val.float_value);
            printf("\n");
            i++;
            if (tok.type == TOKEN_EOF) break;
            if (tok.type == TOKEN_ERROR) {
                printf("  ERROR: %.*s\n", (int)tok.text.len, tok.text.data);
                tokenizer_destroy(t);
                free(source);
                return 1;
            }
        }
        tokenizer_destroy(t);
        free(source);
        return 0;
    }

    /* Phase 2: Parse */
    Parser *parser = parser_create(source, read_len, input_file);
    AstNode *program = parser_parse(parser);

    if (parser->error_count > 0) {
        fprintf(stderr, "Parse failed with %d errors\n", parser->error_count);
        ast_dump(program, 0);
        parser_destroy(parser);
        free(source);
        return 1;
    }

    if (dump_ast) {
        printf("=== AST ===\n");
        ast_dump(program, 0);
        parser_destroy(parser);
        free(source);
        return 0;
    }

    if (verbose) {
        printf("Parse OK (%d top-level decls)\n", program->data.list.count);
    }

    /* Phase 2.5: Resolve imports — read imported files and merge their declarations */
    /* Save .aelib import paths for linking (must persist until after codegen) */
    char **aelib_import_paths = NULL;
    int aelib_import_count = 0;
    int aelib_import_cap = 0;

    {
        /* Build a set of already-imported paths to avoid cycles */
        char **imported_paths = NULL;
        int imported_count = 0;
        int imported_cap = 0;

        /* Scan for NODE_IMPORT nodes and resolve them */
        for (int i = 0; i < program->data.list.count; i++) {
            AstNode *decl = program->data.list.items[i];
            if (decl->type != NODE_IMPORT) continue;

            /* Extract path from string literal */
            StringView sv = decl->data.literal.string_val;
            const char *path_start = sv.data;
            size_t path_len = sv.len;
            if (path_len >= 2 && path_start[0] == '"' && path_start[path_len-1] == '"') {
                path_start++;
                path_len -= 2;
            }

            /* Build full path relative to input file's directory */
            char import_path[1024];
            const char *last_slash = strrchr(input_file, '/');
            if (last_slash) {
                size_t dir_len = (size_t)(last_slash - input_file);
                snprintf(import_path, sizeof(import_path), "%.*s/%.*s",
                    (int)dir_len, input_file, (int)path_len, path_start);
            } else {
                snprintf(import_path, sizeof(import_path), "%.*s",
                    (int)path_len, path_start);
            }

            /* Check for circular import */
            bool already = false;
            for (int j = 0; j < imported_count; j++) {
                if (strcmp(imported_paths[j], import_path) == 0) { already = true; break; }
            }
            if (already) continue;

            /* Track this import */
            if (imported_count >= imported_cap) {
                int nc = imported_cap ? imported_cap * 2 : 8;
                char **na = (char **)realloc(imported_paths, nc * sizeof(char *));
                if (!na) { fprintf(stderr, "Error: out of memory\n"); return 1; }
                imported_paths = na;
                imported_cap = nc;
            }
            imported_paths[imported_count] = strdup(import_path);
            imported_count++;

            if (verbose) printf("Resolving import: %s\n", import_path);

            /* Try .ae first, then .aelib */
            char ae_path[1024], aelib_path[1024];
            bool is_aelib = false;
            snprintf(ae_path, sizeof(ae_path), "%s", import_path);
            snprintf(aelib_path, sizeof(aelib_path), "%s", import_path);

            /* If path doesn't have an extension, try .ae then .aelib */
            const char *dot = strrchr(import_path, '.');
            if (!dot || (strcmp(dot, ".ae") != 0 && strcmp(dot, ".aelib") != 0)) {
                /* No recognized extension — try .ae first */
                size_t base_len = strlen(import_path);
                snprintf(ae_path, sizeof(ae_path), "%.*s.ae", (int)base_len, import_path);
                snprintf(aelib_path, sizeof(aelib_path), "%.*s.aelib", (int)base_len, import_path);
            } else if (strcmp(dot, ".aelib") == 0) {
                is_aelib = true;
            }

            /* Try to open the file */
            FILE *ifile = NULL;
            char *resolved_path = NULL;
            char std_resolved[1024]; /* persistent buffer for std path resolution */

            if (!is_aelib) {
                ifile = fopen(ae_path, "rb");
                if (ifile) resolved_path = ae_path;
            }

            if (!ifile) {
                ifile = fopen(aelib_path, "rb");
                if (ifile) {
                    resolved_path = aelib_path;
                    is_aelib = true;
                }
            }

            if (!ifile) {
                /* Try the import path directly (it may already be a valid path
                 * like "std/test.ae" relative to CWD) */
                char sp_ae[1024], sp_aelib[1024];
                snprintf(sp_ae, sizeof(sp_ae), "%.*s.ae", (int)path_len, path_start);
                snprintf(sp_aelib, sizeof(sp_aelib), "%.*s.aelib", (int)path_len, path_start);
                if (verbose) printf("  std fallback: trying '%s' and '%s'\n", sp_ae, sp_aelib);
                if (!is_aelib) {
                    ifile = fopen(sp_ae, "rb");
                    if (ifile) { snprintf(std_resolved, sizeof(std_resolved), "%s", sp_ae); resolved_path = std_resolved; }
                }
                if (!ifile) {
                    ifile = fopen(sp_aelib, "rb");
                    if (ifile) { snprintf(std_resolved, sizeof(std_resolved), "%s", sp_aelib); resolved_path = std_resolved; is_aelib = true; }
                }
            }

            if (!ifile) {
                fprintf(stderr, "Error: cannot open import '%s' (tried .ae, .aelib, and std paths)\n", import_path);
                return 1;
            }

            if (is_aelib) {
                /* ── .aelib import: create synthetic extern declarations ── */
                if (verbose) printf("Import is .aelib: %s\n", resolved_path);

                /* Read raw symbol table from the file directly */
                fseek(ifile, 0, SEEK_SET);
                uint8_t hdr_buf[46];
                size_t meta_off = 0, meta_sz = 0;
                if (fread(hdr_buf, 1, 46, ifile) == 46) {
                    meta_off = (size_t)((uint64_t)hdr_buf[30] | ((uint64_t)hdr_buf[31] << 8) |
                        ((uint64_t)hdr_buf[32] << 16) | ((uint64_t)hdr_buf[33] << 24) |
                        ((uint64_t)hdr_buf[34] << 32) | ((uint64_t)hdr_buf[35] << 40) |
                        ((uint64_t)hdr_buf[36] << 48) | ((uint64_t)hdr_buf[37] << 56));
                    meta_sz = (size_t)((uint64_t)hdr_buf[38] | ((uint64_t)hdr_buf[39] << 8) |
                        ((uint64_t)hdr_buf[40] << 16) | ((uint64_t)hdr_buf[41] << 24) |
                        ((uint64_t)hdr_buf[42] << 32) | ((uint64_t)hdr_buf[43] << 40) |
                        ((uint64_t)hdr_buf[44] << 48) | ((uint64_t)hdr_buf[45] << 56));
                }
                fclose(ifile);

                int new_decls = 0;
                uint8_t *meta = NULL; /* keep alive for StringView references */
                if (meta_off > 0 && meta_sz > 0) {
                    /* Read metadata section */
                    FILE *mf = fopen(resolved_path, "rb");
                    if (mf) {
                        fseek(mf, (long)meta_off, SEEK_SET);
                        meta = (uint8_t *)malloc(meta_sz);
                        if (meta && fread(meta, 1, meta_sz, mf) == meta_sz) {
                            /* Parse symbol count (at offset 10 in meta header) */
                            uint32_t sc = (uint32_t)meta[10] | ((uint32_t)meta[11] << 8) |
                                          ((uint32_t)meta[12] << 16) | ((uint32_t)meta[13] << 24);
                            for (uint32_t si = 0; si < sc; si++) {
                                size_t entry_off = 14 + (size_t)si * 18;
                                uint32_t name_off = (uint32_t)meta[entry_off] | ((uint32_t)meta[entry_off+1] << 8) |
                                    ((uint32_t)meta[entry_off+2] << 16) | ((uint32_t)meta[entry_off+3] << 24);
                                uint8_t kind = meta[entry_off + 4];
                                uint8_t flags = meta[entry_off + 5];
                                uint32_t td_off2 = (uint32_t)meta[entry_off+10] | ((uint32_t)meta[entry_off+11] << 8) |
                                    ((uint32_t)meta[entry_off+12] << 16) | ((uint32_t)meta[entry_off+13] << 24);
                                uint32_t td_sz2 = (uint32_t)meta[entry_off+14] | ((uint32_t)meta[entry_off+15] << 8) |
                                    ((uint32_t)meta[entry_off+16] << 16) | ((uint32_t)meta[entry_off+17] << 24);

                                /* Resolve name (absolute offset within metadata) */
                                const char *name_ptr = (const char *)meta + name_off;
                                size_t name_len = strlen(name_ptr);

                                if (verbose) printf("  .aelib symbol: '%s' kind=%d pub=%d\n",
                                    name_ptr, kind, (flags & 1));

                                /* Create synthetic extern function declaration */
                                Location loc = {0};
                                StringView sv = {name_ptr, name_len};
                                AstNode *ident = node_ident(parser->arena, loc, sv);
                                AstNode *func = node_func_decl(parser->arena, loc, ident,
                                    (flags & 1) != 0, false);
                                func->data.func.body = NULL; /* extern */

                                /* Parse type data to get return type and params */
                                if (td_sz2 > 0 && kind == 0) { /* function */
                                    const uint8_t *td = meta + td_off2;
                                    size_t tp = 0;
                                    /* Return type name (null-terminated) */
                                    const char *ret_name = (const char *)td;
                                    size_t ret_len = strlen(ret_name);
                                    StringView ret_sv = {ret_name, ret_len};
                                    func->data.func.return_type = node_type_named(parser->arena, loc, ret_sv);
                                    tp += ret_len + 1; /* skip return type + null */

                                    if (tp < td_sz2) {
                                        uint8_t pc = td[tp++];
                                        for (int pi = 0; pi < (int)pc && tp < td_sz2; pi++) {
                                            /* param name */
                                            const char *pn = (const char *)td + tp;
                                            while (tp < td_sz2 && td[tp] != 0) tp++;
                                            tp++; /* skip null */
                                            /* param type */
                                            const char *pt = (const char *)td + tp;
                                            while (tp < td_sz2 && td[tp] != 0) tp++;
                                            tp++; /* skip null */
                                            /* is_mut */
                                            if (tp < td_sz2) tp++;

                                            StringView pn_sv = {pn, strlen(pn)};
                                            StringView pt_sv = {pt, strlen(pt)};
                                            AstNode *pname = node_ident(parser->arena, loc, pn_sv);
                                            AstNode *ptype = node_type_named(parser->arena, loc, pt_sv);
                                            AstNode *param = node_param(parser->arena, loc, pname, ptype, false, false);
                                            node_list_append(&func->data.func.params, param);
                                        }
                                    }
                                }

                                /* Add to program */
                                int new_count = program->data.list.count + 1;
                                AstNode **new_items = (AstNode **)malloc(new_count * sizeof(AstNode *));
                                if (new_items) {
                                    for (int k = 0; k < program->data.list.count; k++)
                                        new_items[k] = program->data.list.items[k];
                                    new_items[program->data.list.count] = func;
                                    program->data.list.items = new_items;
                                    program->data.list.count = new_count;
                                    program->data.list.cap = new_count;
                                }
                                new_decls++;
                            }
                        }
                        fclose(mf);
                    }
                }
                /* NOTE: meta is intentionally NOT freed here — StringView fields
                 * in the synthetic AST nodes point into it. It will be freed
                 * at the end of main() alongside the source buffer. */

                /* Track the .aelib path for linking */
                if (imported_count >= imported_cap) {
                    int nc = imported_cap ? imported_cap * 2 : 8;
                    char **na = (char **)realloc(imported_paths, nc * sizeof(char *));
                    if (!na) { fprintf(stderr, "Error: out of memory\n"); return 1; }
                    imported_paths = na;
                    imported_cap = nc;
                }
                imported_paths[imported_count] = strdup(resolved_path);
                imported_count++;

                /* Save .aelib path for linking (persistent list) */
                if (aelib_import_count >= aelib_import_cap) {
                    int nc = aelib_import_cap ? aelib_import_cap * 2 : 8;
                    char **na = (char **)realloc(aelib_import_paths, nc * sizeof(char *));
                    if (!na) { fprintf(stderr, "Error: out of memory\n"); return 1; }
                    aelib_import_paths = na;
                    aelib_import_cap = nc;
                }
                aelib_import_paths[aelib_import_count++] = strdup(resolved_path);

                /* Remove the import node */
                for (int k = i; k < program->data.list.count - 1; k++)
                    program->data.list.items[k] = program->data.list.items[k + 1];
                program->data.list.count--;
                i--;

                if (verbose) printf("Import resolved: %s (%d synthetic decls, program now has %d decls)\n",
                    resolved_path, new_decls, program->data.list.count);

            } else {
                /* ── .ae source import (existing behavior) ── */
                fseek(ifile, 0, SEEK_END);
                long ilen = ftell(ifile);
                fseek(ifile, 0, SEEK_SET);
                char *isrc = (char *)malloc((size_t)ilen + 1);
                if (!isrc) { fclose(ifile); return 1; }
                size_t rlen = fread(isrc, 1, (size_t)ilen, ifile);
                isrc[rlen] = '\0';
                fclose(ifile);

                /* Parse the imported file using the main parser's arena so nodes persist */
                Parser *iparser = parser_create_with_arena(isrc, rlen, resolved_path, parser->arena);
                AstNode *iprog = parser_parse(iparser);
                if (iparser->error_count > 0) {
                    fprintf(stderr, "Import '%s' has %d parse errors\n", resolved_path, iparser->error_count);
                    iparser->arena = NULL;
                    parser_destroy(iparser);
                    free(isrc);
                    return 1;
                }

                /* Append imported decls to end, then remove the import node */
                int new_count = program->data.list.count + iprog->data.list.count;
                AstNode **new_items = (AstNode **)malloc(new_count * sizeof(AstNode *));
                if (!new_items) { fprintf(stderr, "Error: out of memory\n"); return 1; }
                for (int k = 0; k < program->data.list.count; k++)
                    new_items[k] = program->data.list.items[k];
                int write_idx = program->data.list.count;
                for (int j = 0; j < iprog->data.list.count; j++) {
                    AstNode *idecl = iprog->data.list.items[j];
                    if (idecl->type == NODE_IMPORT) continue;
                    new_items[write_idx++] = idecl;
                }
                program->data.list.items = new_items;
                program->data.list.count = write_idx;
                program->data.list.cap = new_count;

                /* Remove the import node */
                for (int k = i; k < program->data.list.count - 1; k++)
                    program->data.list.items[k] = program->data.list.items[k + 1];
                program->data.list.count--;
                i--;

                iparser->arena = NULL;
                parser_destroy(iparser);

                if (verbose) printf("Import resolved: %s (program now has %d decls)\n",
                    resolved_path, program->data.list.count);
            }
        }

        for (int i = 0; i < imported_count; i++) free(imported_paths[i]);
        free(imported_paths);
    }

    /* Phase 3: Semantic analysis */
    Arena *sa_arena = arena_create();
    SemanticAnalyzer *sa = semantic_create(sa_arena);
    semantic_analyze(sa, program);
    if (sa->error_count > 0) {
        fprintf(stderr, "Semantic analysis failed with %d errors\n", sa->error_count);
        parser_destroy(parser);
        free(source);
        arena_destroy(sa_arena);
        return 1;
    }

    /* Phase 3.5: Optimization */
    if (opt_level > 0) {
        if (verbose) printf("Optimizing (-O%d)...\n", opt_level);
        OptimizerConfig opt_cfg;
        optimizer_config_init(&opt_cfg);
        if (opt_level == 1) {
            /* -O1: basic optimizations only */
            opt_cfg.constant_folding = true;
            opt_cfg.dead_code_elimination = true;
            opt_cfg.inlining = true;
            opt_cfg.escape_analysis = false;
            opt_cfg.region_elision = false;
            opt_cfg.devirtualization = false;
            opt_cfg.loop_unrolling = false;
            opt_cfg.memory_fusion = false;
        }
        /* -O2: all optimizations (default init) */
        program = optimize(program, sa_arena, &opt_cfg);
        if (verbose) printf("Optimization complete (%d decls)\n", program->data.list.count);
    }
    if (verbose) printf("After opt: %d decls\n", program->data.list.count);
    if (verbose) {
        printf("  Decls after opt:\n");
        for (int di = 0; di < program->data.list.count; di++) {
            AstNode *d = program->data.list.items[di];
            printf("    [%d] type=%d", di, (int)d->type);
            if (d->type == NODE_FUNC_DECL && d->data.func.name) {
                printf(" name=%.*s", (int)d->data.func.name->data.ident.name.len,
                    d->data.func.name->data.ident.name.data);
            }
            printf("\n");
        }
    }

    /* Phase 4: Code generation — C transpiler (default), NASM for asm targets */
    if (target == TARGET_ASM_X86_64 || target == TARGET_ASM_ARM64 || target == TARGET_ASM_RISCV64) {
        /* These targets use the NASM codegen path (handled in codegen.c) */
        /* Fall through to the existing NASM codegen below */
    } else {
        Arena *c_arena = arena_create();
        CCodegen *cg = c_create(c_arena, target, opt_level);

        /* Generate C code to a temp file */
        char c_path[1024];
        snprintf(c_path, sizeof(c_path), "%s.c", output_file ? output_file : "/tmp/aether_out");

        FILE *c_file = fopen(c_path, "w");
        if (!c_file) {
            fprintf(stderr, "Error: cannot open %s for writing\n", c_path);
            arena_destroy(c_arena);
            parser_destroy(parser);
            free(source);
            arena_destroy(sa_arena);
            return 1;
        }

        if (!c_generate(cg, program, c_file)) {
            fprintf(stderr, "C code generation failed\n");
            fclose(c_file);
            arena_destroy(c_arena);
            parser_destroy(parser);
            free(source);
            arena_destroy(sa_arena);
            return 1;
        }
        fclose(c_file);

        if (stop_after_asm) {
            /* Just emit C source, don't compile */
            if (verbose) printf("Wrote %s\n", c_path);
        } else {
            /* Compile C to native binary */
            if (c_compile(cg, c_path, output_file) != 0) {
                fprintf(stderr, "C compilation failed\n");
                arena_destroy(c_arena);
                parser_destroy(parser);
                free(source);
                arena_destroy(sa_arena);
                return 1;
            }
            /* Clean up .c file */
            remove(c_path);
        }

        if (verbose) printf("C compilation successful\n");

        parser_destroy(parser);
        free(source);
        arena_destroy(sa_arena);
        arena_destroy(c_arena);

        if (run_mode) {
            if (verbose) printf("Running: %s\n", output_file);
            char cmd[4096];
            snprintf(cmd, sizeof(cmd), "%s", output_file);
            int ret = system(cmd);
            return WEXITSTATUS(ret);
        }

        printf("Compilation successful: %s -> %s\n", input_file, output_file);
        return 0;
    }

    /* NASM codegen path for lib/asm/freestanding targets */
    {
        Arena *cg_arena = arena_create();
        Codegen *cg = codegen_create(cg_arena);
        codegen_set_target(cg, target);
        codegen_generate(cg, program);

        /* Extract metadata for .aelib library target */
        if (target == TARGET_LIB) {
            codegen_extract_metadata(cg, program);
        }

        const char *asm_text = codegen_get_asm(cg);
        if (!asm_text) {
            fprintf(stderr, "Code generation failed\n");
            parser_destroy(parser); free(source);
            arena_destroy(sa_arena); arena_destroy(cg_arena);
            return 1;
        }

        /* Write assembly to temp file */
        char asm_path[1024];
        snprintf(asm_path, sizeof(asm_path), "%s.asm", output_file ? output_file : "/tmp/aether_out");
        FILE *af = fopen(asm_path, "w");
        if (!af) { fprintf(stderr, "Error: cannot write '%s'\n", asm_path); return 1; }
        fwrite(asm_text, 1, strlen(asm_text), af);
        fclose(af);

        /* Assemble and link via codegen.c's assemble function */
        int ret = codegen_assemble(cg, asm_path, output_file);
        remove(asm_path);

        if (ret != 0) {
            fprintf(stderr, "Assembly/link failed\n");
            parser_destroy(parser); free(source);
            arena_destroy(sa_arena); arena_destroy(cg_arena);
            return 1;
        }

        if (verbose) printf("NASM compilation successful\n");
        parser_destroy(parser); free(source);
        arena_destroy(sa_arena); arena_destroy(cg_arena);

        if (run_mode) {
            if (verbose) printf("Running: %s\n", output_file);
            char cmd[4096];
            snprintf(cmd, sizeof(cmd), "%s", output_file);
            int r = system(cmd);
            return WEXITSTATUS(r);
        }

        printf("Compilation successful: %s -> %s\n", input_file, output_file);
        return 0;
    }
}
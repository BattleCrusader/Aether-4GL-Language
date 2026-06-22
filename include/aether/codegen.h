#ifndef AETHER_CODEGEN_H
#define AETHER_CODEGEN_H

#include "defs.h"
#include "ast.h"
#include "arena.h"

/*
 * Code Generator — walks the AST and emits NASM assembly text.
 * Output goes to a string buffer, then written to a .asm file.
 */

typedef enum {
    TARGET_FREESTANDING,   /* x86_64 ELF64 for Aether OS (default) */
    TARGET_MACHO64,        /* x86_64 Mach-O for macOS */
    TARGET_HOST,           /* auto-detect: Mach-O on macOS, ELF64 on Linux */
    TARGET_ELF64_HOST,     /* native ELF64 for Linux host */
    TARGET_KERNEL,         /* x86_64 kernel ELF at 0x1000000 */
    TARGET_MODULE,         /* .ko module ELF for Aether OS */
    TARGET_BINARY,         /* /bin/ userland ELF at 0x2000000 */
    TARGET_BOOT,           /* flat binary boot sector */
    TARGET_ASM_X86_64,     /* emit x86_64 NASM assembly listing */
    TARGET_ASM_ARM64,      /* emit ARM64 assembly listing */
    TARGET_ASM_RISCV64,    /* emit RISC-V assembly listing */
    TARGET_UNIVERSAL,      /* universal binary: x86_64 + ARM64 */
    TARGET_UNIVERSAL_ALL,  /* universal binary: all architectures */
} Target;

typedef struct AutoDrop AutoDrop;

typedef struct {
    Arena *arena;
    char *output;            /* accumulated output buffer */
    size_t output_len;
    size_t output_cap;
    int label_counter;       /* for unique labels */
    int indent_level;        /* current asm indentation */
    AstNode *current_func;   /* function being generated */
    Target target;           /* output target format */
    AstNode *defer_stack;    /* linked list of deferred ASTs (LIFO) */
    int defer_count;         /* number of deferred items */
    AutoDrop *auto_drops;    /* linked list of class auto-drop entries */
    int64_t entry_addr;      /* load address from @entry(addr), 0 = default */
    const char *entry_func;  /* name of the entry-point function, NULL = default */
    /* @layout state — for flat binary boot sectors etc. */
    bool has_layout;
    int64_t layout_start;    /* [org N] origin */
    int64_t layout_max;      /* max binary size, pad/error if exceeded */
    int layout_bits;         /* bits mode: 16, 32, or 64; 0=default(64) */
    int layout_signature;    /* boot signature word (e.g. 0xAA55), 0=none */
    const char *layout_file; /* output filename, NULL = use normal pipeline */
    /* User-supplied linker script path, NULL = auto-generate */
    const char *linker_script;
    int cleanup_depth;       /* current scope cleanup depth for try/catch unwinding */
} Codegen;

Codegen *codegen_create(Arena *a);

/* Set the output target format */
void codegen_set_target(Codegen *cg, Target target);

/* Generate assembly for a complete program */
const char *codegen_generate(Codegen *cg, AstNode *program);

/* Write generated assembly to a file. Returns 0 on success. */
int codegen_write_asm(Codegen *cg, const char *path);

/* Get the generated assembly text */
const char *codegen_get_asm(Codegen *cg);

/* Assemble and link the generated .asm into an executable.
 * Returns 0 on success, nonzero on failure. */
int codegen_assemble(Codegen *cg, const char *asm_file, const char *output_file);

/* Detect host target */
Target codegen_detect_host(void);

/* Get human-readable target name */
const char *target_name(Target t);

#endif /* AETHER_CODEGEN_H */
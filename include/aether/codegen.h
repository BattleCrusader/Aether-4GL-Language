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
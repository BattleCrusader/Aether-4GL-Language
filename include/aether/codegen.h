#ifndef AETHER_CODEGEN_H
#define AETHER_CODEGEN_H

#include "defs.h"
#include "ast.h"
#include "arena.h"

/*
 * Code Generator — walks the AST and emits NASM assembly text.
 * Output goes to a string buffer, then written to a .asm file.
 */

typedef struct {
    Arena *arena;
    char *output;            /* accumulated output buffer */
    size_t output_len;
    size_t output_cap;
    int label_counter;       /* for unique labels */
    int indent_level;        /* current asm indentation */
    AstNode *current_func;   /* function being generated */
} Codegen;

Codegen *codegen_create(Arena *a);
void codegen_destroy(Codegen *cg);

/* Generate assembly for a complete program */
const char *codegen_generate(Codegen *cg, AstNode *program);

/* Write generated assembly to a file. Returns 0 on success. */
int codegen_write_asm(Codegen *cg, const char *path);

/* Get the generated assembly text */
const char *codegen_get_asm(Codegen *cg);

#endif /* AETHER_CODEGEN_H */
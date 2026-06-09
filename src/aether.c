/*
 * aether.c — Bootstrap compiler CLI.
 * Reads .ae files, tokenizes, parses, analyzes, generates NASM,
 * assembles with nasm, and links with ld.
 */

#include "aether/tokenizer.h"
#include "aether/lexer.h"
#include "aether/ast.h"
#include "aether/parser.h"
#include "aether/semantic.h"
#include "aether/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr, "Aether Compiler v0.1 (Phase 0 Bootstrap)\n");
    fprintf(stderr, "Usage: %s [options] <file.ae>\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o, --output <file>    Output file (default: a.out)\n");
    fprintf(stderr, "  -S                     Stop after assembly (emit .asm)\n");
    fprintf(stderr, "  --dump-ast             Print AST and exit\n");
    fprintf(stderr, "  --dump-tokens          Print tokens and exit\n");
    fprintf(stderr, "  -v, --verbose          Verbose output\n");
}

int main(int argc, char **argv) {
    const char *input_file = NULL;
    const char *output_file = "a.out";
    int stop_after_asm = 0;
    int dump_ast = 0;
    int dump_tokens = 0;
    int verbose = 0;

    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) output_file = argv[++i];
        } else if (strcmp(argv[i], "-S") == 0) {
            stop_after_asm = 1;
        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            dump_ast = 1;
        } else if (strcmp(argv[i], "--dump-tokens") == 0) {
            dump_tokens = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (!input_file) {
        fprintf(stderr, "Error: no input file specified\n");
        usage(argv[0]);
        return 1;
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

    /* Phase 4: Code generation */
    Arena *cg_arena = arena_create();
    Codegen *cg = codegen_create(cg_arena);
    codegen_generate(cg, program);

    /* Determine output filenames */
    char asm_file[1024];
    char obj_file[1024];

    if (stop_after_asm) {
        snprintf(asm_file, sizeof(asm_file), "%s", output_file);
    } else {
        /* Use /tmp/ for intermediate files */
        unsigned long hash = 5381;
        for (const char *p = input_file; *p; p++)
            hash = ((hash << 5) + hash) + (unsigned char)*p;
        snprintf(asm_file, sizeof(asm_file), "/tmp/aether_%lx.asm", hash);
        snprintf(obj_file, sizeof(obj_file), "/tmp/aether_%lx.o", hash);
    }

    /* Write .asm file */
    if (codegen_write_asm(cg, asm_file) != 0) {
        fprintf(stderr, "Error: cannot write '%s'\n", asm_file);
        parser_destroy(parser);
        free(source);
        arena_destroy(sa_arena);
        arena_destroy(cg_arena);
        return 1;
    }

    if (verbose) printf("Wrote %s\n", asm_file);

    if (stop_after_asm) {
        if (verbose) printf("Stopping after assembly (-S flag)\n");
        parser_destroy(parser);
        free(source);
        arena_destroy(sa_arena);
        arena_destroy(cg_arena);
        return 0;
    }

    /* Phase 5: Assemble with nasm */
    char cmd[4096];
    int ret;

    snprintf(cmd, sizeof(cmd), "nasm -f elf64 -o %s %s", obj_file, asm_file);
    if (verbose) printf("Running: %s\n", cmd);
    ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "nasm failed (exit %d)\n", ret);
        /* Try to find nasm */
        ret = system("which nasm 2>/dev/null");
        if (ret != 0) {
            fprintf(stderr, "nasm not found. Install it or use -S to stop at .asm\n");
        }
        parser_destroy(parser);
        free(source);
        arena_destroy(sa_arena);
        arena_destroy(cg_arena);
        return 1;
    }

    /* Phase 7: Link with ld */
    {
        /* Write linker script inline to avoid path issues */
        FILE *ldf = fopen("/tmp/aether_ld.ld", "w");
        if (ldf) {
            fprintf(ldf, "OUTPUT_FORMAT(elf64-x86-64)\nENTRY(_start)\nSECTIONS {\n");
            fprintf(ldf, "  . = 0x400000;\n");
            fprintf(ldf, "  .text : { *(.text) *(.text.*) }\n");
            fprintf(ldf, "  .rodata : { *(.rodata) *(.rodata.*) }\n");
            fprintf(ldf, "  .data : { *(.data) *(.data.*) }\n");
            fprintf(ldf, "  .bss : { *(.bss) *(.bss.*) *(COMMON) }\n}\n");
            fclose(ldf);
        }
        snprintf(cmd, sizeof(cmd), LD " -T /tmp/aether_ld.ld -o %s %s", output_file, obj_file);
        if (verbose) printf("Running: %s\n", cmd);
        ret = system(cmd);
    }
    if (ret != 0) {
        fprintf(stderr, "ld failed (exit %d)\n", ret);
        parser_destroy(parser);
        free(source);
        arena_destroy(sa_arena);
        arena_destroy(cg_arena);
        return 1;
    }

    if (verbose) printf("Wrote %s\n", output_file);

    /* Cleanup temp files */
    remove(asm_file);
    remove(obj_file);
    remove("/tmp/aether_ld.ld");

    parser_destroy(parser);
    free(source);
    arena_destroy(sa_arena);
    arena_destroy(cg_arena);

    printf("Compilation successful: %s -> %s\n", input_file, output_file);
    return 0;
}
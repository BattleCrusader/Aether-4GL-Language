/*
 * aether.c — Bootstrap compiler CLI.
 * Reads .ae files, tokenizes, parses, analyzes, generates NASM,
 * assembles with nasm, and links with ld (freestanding ELF64)
 * or clang (Mach-O 64 for macOS).
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
    fprintf(stderr, "Aether Compiler v0.2 (Phase 2 — Host Native)\n");
    fprintf(stderr, "Usage: %s [options] <file.ae>\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o, --output <file>    Output file (default: a.out)\n");
    fprintf(stderr, "  --target <target>      Output target:\n");
    fprintf(stderr, "                           host           Auto-detect host format (default)\n");
    fprintf(stderr, "                           x86_64-freestanding  Aether OS ELF64\n");
    fprintf(stderr, "  -S                     Stop after assembly (emit .asm)\n");
    fprintf(stderr, "  --dump-ast             Print AST and exit\n");
    fprintf(stderr, "  --dump-tokens          Print tokens and exit\n");
    fprintf(stderr, "  -v, --verbose          Verbose output\n");
}

int main(int argc, char **argv) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    int stop_after_asm = 0;
    int dump_ast = 0;
    int dump_tokens = 0;
    int verbose = 0;
    Target target = TARGET_HOST; /* default: auto-detect */

    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) output_file = argv[++i];
        } else if (strcmp(argv[i], "--target") == 0) {
            if (i + 1 < argc) {
                const char *t = argv[++i];
                if (strcmp(t, "host") == 0) {
                    target = TARGET_HOST;
                } else if (strcmp(t, "x86_64-freestanding") == 0) {
                    target = TARGET_FREESTANDING;
                } else if (strcmp(t, "macho64") == 0) {
                    target = TARGET_MACHO64;
                } else if (strcmp(t, "elf64-host") == 0) {
                    target = TARGET_ELF64_HOST;
                } else {
                    fprintf(stderr, "Unknown target: %s\n", t);
                    usage(argv[0]);
                    return 1;
                }
            }
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

    /* Default output name */
    if (!output_file) {
        if (target == TARGET_HOST || target == TARGET_MACHO64 || target == TARGET_ELF64_HOST) {
            /* Strip .ae extension for host-native */
            const char *dot = strrchr(input_file, '.');
            if (dot && strcmp(dot, ".ae") == 0) {
                size_t base_len = (size_t)(dot - input_file);
                output_file = (const char *)malloc(base_len + 1);
                if (output_file) {
                    char *p = (char *)output_file;
                    memcpy(p, input_file, base_len);
                    p[base_len] = '\0';
                }
            } else {
                output_file = "a.out";
            }
        } else {
            output_file = "a.out";
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
    codegen_set_target(cg, target);
    codegen_generate(cg, program);

    if (verbose) {
        printf("Target: %s\n", target_name(cg->target));
    }

    /* Determine output filenames */
    char asm_file[1024];

    if (stop_after_asm) {
        snprintf(asm_file, sizeof(asm_file), "%s", output_file);
    } else {
        /* Use /tmp/ for intermediate files */
        unsigned long hash = 5381;
        for (const char *p = input_file; *p; p++)
            hash = ((hash << 5) + hash) + (unsigned char)*p;
        snprintf(asm_file, sizeof(asm_file), "/tmp/aether_%lx.asm", hash);
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

    /* Phase 5: Assemble and link */
    int ret = codegen_assemble(cg, asm_file, output_file);
    if (ret != 0) {
        fprintf(stderr, "Assembly/link failed\n");
        parser_destroy(parser);
        free(source);
        arena_destroy(sa_arena);
        arena_destroy(cg_arena);
        return 1;
    }

    if (verbose) printf("Wrote %s\n", output_file);

    /* Cleanup temp files */
    remove(asm_file);

    parser_destroy(parser);
    free(source);
    arena_destroy(sa_arena);
    arena_destroy(cg_arena);

    printf("Compilation successful: %s -> %s\n", input_file, output_file);
    return 0;
}
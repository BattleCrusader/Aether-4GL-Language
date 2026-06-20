/*
 * test_tokenizer.c — Robust tokenizer tests.
 * Tests verify properties of tokens rather than fragile exact sequences.
 */

#include "aether/tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  TEST: %s ... ", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)

#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; return; } while(0)

#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* Count token types in a source */
typedef struct {
    int total;
    int stmt_keywords; /* func, let, if, elif, else, while, for, return, break, continue */
    int types;         /* identifiers */
    int literals;      /* INT, FLOAT, STRING, CHAR */
    int operators;     /* + - * / etc */
    int keywords;      /* all keyword tokens */
    int indent_related; /* INDENT + DEDENT */
    int brackets;      /* ( ) [ ] { } */
} TokenCounts;

static void count_tokens(const char *source, TokenCounts *c) {
    memset(c, 0, sizeof(TokenCounts));
    Tokenizer *t = tokenizer_create(source, strlen(source), "test");
    if (!t) { FAIL("tokenizer_create"); return; }

    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_ERROR) {
            tokenizer_destroy(t);
            FAIL("error token encountered");
            return;
        }
        c->total++;
        if (tok.type >= TOKEN_KW_FUNC && tok.type <= TOKEN_KW_AT) c->keywords++;
        if (tok.type == TOKEN_INDENT || tok.type == TOKEN_DEDENT) c->indent_related++;
        if (tok.type == TOKEN_INT_LITERAL || tok.type == TOKEN_FLOAT_LITERAL ||
            tok.type == TOKEN_STRING_LITERAL || tok.type == TOKEN_CHAR_LITERAL) c->literals++;
        if (tok.type == TOKEN_LPAREN || tok.type == TOKEN_RPAREN ||
            tok.type == TOKEN_LBRACKET || tok.type == TOKEN_RBRACKET ||
            tok.type == TOKEN_LBRACE || tok.type == TOKEN_RBRACE) c->brackets++;
        switch (tok.type) {
            case TOKEN_PLUS: case TOKEN_MINUS: case TOKEN_STAR: case TOKEN_SLASH: case TOKEN_PERCENT:
            case TOKEN_EQ: case TOKEN_EQ_EQ: case TOKEN_BANG_EQ:
            case TOKEN_LT: case TOKEN_GT: case TOKEN_LT_EQ: case TOKEN_GT_EQ:
            case TOKEN_AND_AND: case TOKEN_PIPE_PIPE:
            case TOKEN_ARROW: case TOKEN_DOT_DOT: case TOKEN_DOT_DOT_EQ:
                c->operators++;
            default: break;
        }
    }
    tokenizer_destroy(t);
}

static int token_count(const char *source) {
    int count = 0;
    Tokenizer *t = tokenizer_create(source, strlen(source), "test");
    if (!t) return -1;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF || tok.type == TOKEN_ERROR) break;
        count++;
    }
    tokenizer_destroy(t);
    return count;
}

/* ================================================================
 * Tests
 * ================================================================ */

static void test_empty() {
    TEST("empty file");
    ASSERT(token_count("") == 0, "empty should yield 0 tokens");
    PASS();
}

static void test_basic_func() {
    TEST("basic function - keyword detection");
    TokenCounts c;
    count_tokens("func main() {\n    return 42\n}\n", &c);
    ASSERT(c.total >= 10, "should have at least 10 tokens");
    ASSERT(c.keywords >= 2, "should have func + return keywords");
    ASSERT(c.literals == 1, "should have 1 integer literal");
    ASSERT(c.brackets >= 4, "should have parens + braces");
    PASS();
}

static void test_let_decl() {
    TEST("let declarations");
    const char *src = "func main() {\n    let x = 42\n    let mut y = 10\n}\n";
    int count = token_count(src);
    ASSERT(count > 15, "should have many tokens");
    
    /* Check let keyword appears */
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int let_count = 0, mut_count = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_KW_LET) let_count++;
        if (tok.type == TOKEN_KW_MUT) mut_count++;
    }
    tokenizer_destroy(t);
    ASSERT(let_count == 2, "should have 2 'let' keywords");
    ASSERT(mut_count == 1, "should have 1 'mut' keyword");
    PASS();
}

static void test_if_elif_else() {
    TEST("if/elif/else structure");
    const char *src =
        "func sign(x: int) {\n"
        "    if x > 0 {\n"
        "        return 1\n"
        "    } elif x < 0 {\n"
        "        return -1\n"
        "    } else {\n"
        "        return 0\n"
        "    }\n"
        "}\n";
    
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int if_count = 0, elif_count = 0, else_count = 0, return_count = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_KW_IF) if_count++;
        if (tok.type == TOKEN_KW_ELIF) elif_count++;
        if (tok.type == TOKEN_KW_ELSE) else_count++;
        if (tok.type == TOKEN_KW_RETURN) return_count++;
    }
    tokenizer_destroy(t);
    ASSERT(if_count == 1, "should have 1 if");
    ASSERT(elif_count == 1, "should have 1 elif");
    ASSERT(else_count == 1, "should have 1 else");
    ASSERT(return_count == 3, "should have 3 returns");
    PASS();
}

static void test_comments_skipped() {
    TEST("comments skipped");
    const char *src =
        "# this is a comment\n"
        "func main() {\n"
        "    return 42\n"
        "}\n";
    /* With comments removed, should match basic function */
    int no_comment = token_count(src);
    
    const char *src2 =
        "#{ block comment\n"
        "   still in comment\n"
        "}#\n"
        "func main() {\n"
        "    return 42\n"
        "}\n";
    int block_comment = token_count(src2);
    
    ASSERT(no_comment > 0, "should produce tokens");
    ASSERT(no_comment == block_comment, 
           "line and block comments should produce same count");
    PASS();
}

static void test_number_literals() {
    TEST("number literals - hex/bin/oct/dec");
    const char *src = "let a = 255\nlet b = 0xFF\nlet c = 0b1111\nlet d = 0o77\n";
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int values[] = {0, 0, 0, 0};
    int idx = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_INT_LITERAL && idx < 4) {
            values[idx++] = (int)tok.val.int_value;
        }
    }
    tokenizer_destroy(t);
    ASSERT(idx == 4, "should find 4 integer literals");
    ASSERT(values[0] == 255, "decimal 255");
    ASSERT(values[1] == 255, "hex 0xFF = 255");
    ASSERT(values[2] == 15, "binary 0b1111 = 15");
    ASSERT(values[3] == 63, "octal 0o77 = 63");
    PASS();
}

static void test_number_underscores() {
    TEST("underscores in numbers");
    const char *src = "let a = 1_000_000\nlet b = 0xFFFF_FFFF\n";
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    uint64_t vals[2];
    int idx = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_INT_LITERAL && idx < 2) {
            vals[idx++] = tok.val.int_value;
        }
    }
    tokenizer_destroy(t);
    ASSERT(idx == 2, "should find 2 int literals");
    ASSERT(vals[0] == 1000000, "1_000_000 = 1000000");
    ASSERT(vals[1] == 0xFFFFFFFF, "0xFFFF_FFFF = 4294967295");
    PASS();
}

static void test_floats() {
    TEST("float literals");
    const char *src = "let a = 3.14\nlet b = 1e10\n";
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int found = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_FLOAT_LITERAL) found++;
    }
    tokenizer_destroy(t);
    ASSERT(found == 2, "should find 2 float literals");
    PASS();
}

static void test_strings() {
    TEST("string literals");
    const char *src = "let s = \"hello\"\nlet t = \"escaped\\nstring\"\n";
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int found = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_STRING_LITERAL) found++;
    }
    tokenizer_destroy(t);
    ASSERT(found == 2, "should find 2 string literals");
    PASS();
}

static void test_chars() {
    TEST("char literals");
    const char *src = "'a'\n'\\n'\n'\\x41'\n";
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int vals[3];
    int idx = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_CHAR_LITERAL && idx < 3) {
            vals[idx++] = (int)tok.val.int_value;
        }
    }
    tokenizer_destroy(t);
    ASSERT(idx == 3, "should find 3 char literals");
    ASSERT(vals[0] == 'a', "'a' = 97");
    ASSERT(vals[1] == '\n', "'\\n' = 10");
    ASSERT(vals[2] == 'A', "'\\x41' = 65");
    PASS();
}

static void test_operators() {
    TEST("all operators");
    const char *src = "+ - * / % = == != < > <= >= && || -> .. ..= += -= << >> ?";
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int op_count = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        op_count++;
    }
    tokenizer_destroy(t);
    ASSERT(op_count == 22, "should have 22 operator tokens");
    PASS();
}

static void test_indent_balance() {
    TEST("indent/dedent balance");
    const char *poison_src =
        "func outer() {\n"
        "    func inner() {\n"
        "        let x = 1\n"
        "    }\n"
        "}\n";
    
    Tokenizer *t = tokenizer_create(poison_src, strlen(poison_src), "test");
    int indents = 0, dedents = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_INDENT) indents++;
        if (tok.type == TOKEN_DEDENT) dedents++;
    }
    tokenizer_destroy(t);
    ASSERT(indents == dedents, "indents should equal dedents");
    ASSERT(indents >= 2, "should have at least 2 indent levels");
    PASS();
}

static void test_indent_in_func_body() {
    TEST("indent inside braced func body — no indent tokens");
    const char *src = "func main() {\nlet x = 1\nlet y = 2\n}\n";
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int indents = 0, dedents = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_INDENT) indents++;
        if (tok.type == TOKEN_DEDENT) dedents++;
    }
    tokenizer_destroy(t);
    /* Braces override indentation — no indent/dedent inside func body */
    ASSERT(indents == 0, "braced blocks should have 0 indents");
    ASSERT(dedents == 0, "braced blocks should have 0 dedents");
    PASS();
}

static void test_indent_mixed_brace_and_indent() {
    TEST("mixed brace/indent — braces suppress indent");
    const char *src =
        "func outer() {\n"
        "let x = 1\n"
        "if true {\n"
        "    let y = 2\n"
        "}\n"
        "let z = 3\n"
        "}\n";
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int indents = 0, dedents = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_INDENT) indents++;
        if (tok.type == TOKEN_DEDENT) dedents++;
    }
    tokenizer_destroy(t);
    /* Only the 4-space indented 'let y = 2' inside if should produce indent/dedent */
    ASSERT(indents == dedents, "indent/dedent must balance");
    PASS();
}

static void test_keywords() {
    TEST("all keywords recognized");
    const char *src =
        "func let mut if elif else while for in\n"
        "return true false none asm break continue "
        "struct enum class match case try throw catch "
        "and or not import const ref owned rc heap "
        "region pub static defer unsafe module sys "
        "pre post drop init self type trait impl "
        "pool protocol virtual dyn throws export entry "
        "layout test run prop inline at\n";

    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int kw = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_NEWLINE) continue;
        if (tok.type == TOKEN_IDENT) {
            tokenizer_destroy(t);
            FAIL("unrecognized keyword treated as identifier");
            return;
        }
        ASSERT(tok.type >= TOKEN_KW_FUNC && tok.type <= TOKEN_KW_AT,
               "unexpected token type for keyword");
        kw++;
    }
    tokenizer_destroy(t);
    ASSERT(kw == 61, "should have 61 keywords");
    PASS();
}

static void test_asm_block() {
    TEST("asm block tokens");
    const char *src =
        "func outb() {\n"
        "    asm {\n"
        "        mov dx, 42\n"
        "        ret\n"
        "    }\n"
        "}\n";
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int asm_count = 0, braces = 0, idents = 0;
    while (true) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_KW_ASM) asm_count++;
        if (tok.type == TOKEN_LBRACE || tok.type == TOKEN_RBRACE) braces++;
        if (tok.type == TOKEN_IDENT) idents++;
    }
    tokenizer_destroy(t);
    ASSERT(asm_count == 1, "should have 1 asm keyword");
    ASSERT(braces == 4, "should have 4 braces (2 pairs)");
    ASSERT(idents >= 3, "should have asm instruction mnemonics as idents");
    PASS();
}

/* ================================================================ */

int main() {
    printf("Running tokenizer tests...\n\n");

    test_empty();
    test_basic_func();
    test_let_decl();
    test_if_elif_else();
    test_comments_skipped();
    test_number_literals();
    test_number_underscores();
    test_floats();
    test_strings();
    test_chars();
    test_operators();
    test_indent_balance();
    test_indent_in_func_body();
    test_indent_mixed_brace_and_indent();
    test_keywords();
    test_asm_block();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
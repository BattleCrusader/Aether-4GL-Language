/*
 * test_parser.c — Test the Aether parser/AST construction.
 */

#include "aether/parser.h"
#include "aether/ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { tests_run++; printf("  TEST: %s ... ", name); fflush(stdout); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; return; } while(0)

static AstNode *parse_source(const char *source, const char *name) {
    Parser *p = parser_create(source, strlen(source), name ? name : "test");
    if (!p) return NULL;
    AstNode *program = parser_parse(p);
    if (p->error_count > 0) {
        printf(" (errors: %d)\n", p->error_count);
        ast_dump(program, 0);
        parser_destroy(p);
        return NULL;
    }
    parser_destroy(p);
    return program;
}

static int count_decls(AstNode *program) {
    if (!program || program->type != NODE_PROGRAM) return -1;
    return program->data.list.count;
}

static AstNode *find_func(AstNode *program, const char *name) {
    if (!program) return NULL;
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *n = program->data.list.items[i];
        if (n->type == NODE_FUNC_DECL) {
            if (sv_eq_cstr(n->data.func.name->data.ident.name, name)) return n;
        }
    }
    return NULL;
}

/* ================================================================ */

static void test_empty_program() {
    TEST("empty program");
    AstNode *p = parse_source("", "empty");
    if (!p) { FAIL("parse failed"); return; }
    if (count_decls(p) != 0) { FAIL("should have 0 decls"); return; }
    PASS();
}

static void test_func_no_params() {
    TEST("func with no params");
    AstNode *p = parse_source("func main() { return 42 }", "func1");
    if (!p) { FAIL("parse failed"); return; }
    if (count_decls(p) < 1) { FAIL("should have at least 1 decl"); return; }
    AstNode *f = find_func(p, "main");
    if (!f) { FAIL("could not find func 'main'"); return; }
    if (!f->data.func.body) { FAIL("no body"); return; }
    PASS();
}

static void test_func_with_params() {
    TEST("func with params (colon syntax)");
    const char *src = "func add(a: int, b: int): int { return a + b }";
    AstNode *p = parse_source(src, "func2");
    if (!p) { FAIL("parse failed"); return; }
    AstNode *f = find_func(p, "add");
    if (!f) { FAIL("could not find func 'add'"); return; }
    if (f->data.func.params.count != 2) {
        char msg[64]; snprintf(msg, sizeof(msg), "expected 2 params, got %d", f->data.func.params.count);
        FAIL(msg); return;
    }
    if (!f->data.func.return_type) { FAIL("expected return type"); return; }
    PASS();
}

static void test_let_decl() {
    TEST("let declarations");
    const char *src = "func main() {\n    let x = 42\n    let mut s = \"hello\"\n}";
    AstNode *p = parse_source(src, "let");
    if (!p) { FAIL("parse failed"); return; }
    AstNode *f = find_func(p, "main");
    if (!f) { FAIL("func not found"); return; }
    if (!f->data.func.body) { FAIL("no body"); return; }
    PASS();
}

static void test_if_stmt() {
    TEST("if/elif/else");
    const char *src = "func sign(x: int): int { if x > 0 { return 1 } elif x < 0 { return -1 } else { return 0 } }";
    AstNode *p = parse_source(src, "if-test");
    if (!p) { FAIL("parse failed"); return; }
    AstNode *f = find_func(p, "sign");
    if (!f) { FAIL("func not found"); return; }
    PASS();
}

static void test_while_loop() {
    TEST("while loop");
    const char *src = "func count() { let mut x = 0; while x < 10 { x = x + 1 } }";
    AstNode *p = parse_source(src, "while");
    if (!p) { FAIL("parse failed"); return; }
    PASS();
}

static void test_for_loop() {
    TEST("for loop");
    const char *src = "func sum(): int { let mut s = 0; for i in 0..10 { s = s + i }; return s }";
    AstNode *p = parse_source(src, "for");
    if (!p) { FAIL("parse failed"); return; }
    PASS();
}

static void test_match() {
    TEST("match expression");
    const char *src = "func classify(x: int): string { match x { case 0 -> \"zero\" case _ -> \"other\" } }";
    AstNode *p = parse_source(src, "match");
    if (!p) { FAIL("parse failed"); return; }
    PASS();
}

static void test_struct_decl() {
    TEST("struct declaration");
    const char *src = "struct Point { x: int; y: int }";
    AstNode *p = parse_source(src, "struct");
    if (!p) { FAIL("parse failed"); return; }
    if (count_decls(p) < 1) { FAIL("expected 1 decl"); return; }
    AstNode *s = p->data.list.items[0];
    if (s->type != NODE_STRUCT_DECL) { FAIL("expected struct decl"); return; }
    PASS();
}

static void test_enum_decl() {
    TEST("enum declaration");
    const char *src = "enum Color { Red; Green; Blue; Rgb(u8, u8, u8) }";
    AstNode *p = parse_source(src, "enum");
    if (!p) { FAIL("parse failed"); return; }
    AstNode *e = p->data.list.items[0];
    if (e->type != NODE_ENUM_DECL) { FAIL("expected enum decl"); return; }
    if (e->data.enum_decl.variants.count < 3) {
        char msg[64]; snprintf(msg, sizeof(msg), "expected 3+ variants, got %d", e->data.enum_decl.variants.count);
        FAIL(msg); return;
    }
    PASS();
}

static void test_asm_block() {
    TEST("asm block");
    const char *src = "func outb(port: u16, val: byte) { asm { mov dx port mov al val out dx al } }";
    AstNode *p = parse_source(src, "asm");
    if (!p) { FAIL("parse failed"); return; }
    AstNode *f = find_func(p, "outb");
    if (!f) { FAIL("func not found"); return; }
    PASS();
}

static void test_binary_expr() {
    TEST("binary expressions");
    const char *src = "func calc(): int { return (1 + 2) * 3 - 4 / 2 }";
    AstNode *p = parse_source(src, "binary");
    if (!p) { FAIL("parse failed"); return; }
    PASS();
}

static void test_string_literal() {
    TEST("string literal");
    const char *src = "func greet() { print(\"hello, world!\") }";
    AstNode *p = parse_source(src, "string-lit");
    if (!p) { FAIL("parse failed"); return; }
    PASS();
}

static void test_complex_program() {
    TEST("complete program with multiple declarations");
    const char *src =
        "struct Point { x: int; y: int }\n"
        "enum Shape { Circle(Point, int); Rect(Point, Point); Triangle(Point, Point, Point) }\n"
        "func area(s: Shape): int { 0 }\n"
        "const PI = 3.14\n";
    AstNode *p = parse_source(src, "complex");
    if (!p) { FAIL("parse failed"); return; }
    if (count_decls(p) < 4) {
        char msg[64]; snprintf(msg, sizeof(msg), "expected 4+ decls, got %d", count_decls(p));
        FAIL(msg); return;
    }
    PASS();
}

/* ================================================================ */

int main() {
    printf("Running parser tests...\n\n");

    test_empty_program();
    test_func_no_params();
    test_func_with_params();
    test_let_decl();
    test_if_stmt();
    test_while_loop();
    test_for_loop();
    test_match();
    test_struct_decl();
    test_enum_decl();
    test_asm_block();
    test_binary_expr();
    test_string_literal();
    test_complex_program();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
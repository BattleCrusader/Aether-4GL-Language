// aether2c.c — Minimal Aether-to-C transpiler, C11, zero dependencies
// Input:  func main(inputString: string): void { print("inputString ", inputString, "\n") }
// Output: Compilable C code

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ─── Tokenizer ────────────────────────────────────────────────

typedef enum {
    TOK_EOF, TOK_IDENT, TOK_STRING, TOK_COLON, TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE, TOK_COMMA, TOK_FUNC, TOK_VOID, TOK_PRINT
} TokenType;

typedef struct { TokenType type; char *text; int len; } Token;

typedef struct {
    const char *src; int pos; int line; int col;
    Token cur; char buf[4096];
} Lexer;

static void lex_next(Lexer *l) {
    while (l->src[l->pos] && (l->src[l->pos] == ' ' || l->src[l->pos] == '\n')) {
        if (l->src[l->pos] == '\n') { l->line++; l->col = 1; }
        l->pos++; l->col++;
    }
    if (!l->src[l->pos]) { l->cur.type = TOK_EOF; return; }
    char c = l->src[l->pos];
    if (c == '(') { l->cur.type = TOK_LPAREN; l->cur.text = "("; l->pos++; l->col++; return; }
    if (c == ')') { l->cur.type = TOK_RPAREN; l->cur.text = ")"; l->pos++; l->col++; return; }
    if (c == '{') { l->cur.type = TOK_LBRACE; l->cur.text = "{"; l->pos++; l->col++; return; }
    if (c == '}') { l->cur.type = TOK_RBRACE; l->cur.text = "}"; l->pos++; l->col++; return; }
    if (c == ':') { l->cur.type = TOK_COLON; l->cur.text = ":"; l->pos++; l->col++; return; }
    if (c == ',') { l->cur.type = TOK_COMMA; l->cur.text = ","; l->pos++; l->col++; return; }
    if (c == '"') {
        int start = l->pos;
        l->pos++; l->col++;
        while (l->src[l->pos] && l->src[l->pos] != '"') {
            if (l->src[l->pos] == '\\' && l->src[l->pos+1]) l->pos++;
            l->pos++; l->col++;
        }
        if (l->src[l->pos] == '"') l->pos++;
        int len = l->pos - start;
        memcpy(l->buf, l->src + start, len); l->buf[len] = 0;
        l->cur.type = TOK_STRING; l->cur.text = l->buf; l->cur.len = len;
        return;
    }
    if (isalpha(c) || c == '_') {
        int start = l->pos;
        while (isalnum(l->src[l->pos]) || l->src[l->pos] == '_') l->pos++;
        int len = l->pos - start;
        memcpy(l->buf, l->src + start, len); l->buf[len] = 0;
        l->cur.text = l->buf; l->cur.len = len;
        if      (!strcmp(l->buf, "func"))  l->cur.type = TOK_FUNC;
        else if (!strcmp(l->buf, "void"))  l->cur.type = TOK_VOID;
        else if (!strcmp(l->buf, "print")) l->cur.type = TOK_PRINT;
        else                               l->cur.type = TOK_IDENT;
        return;
    }
    fprintf(stderr, "lex error at '%c' line %d\n", c, l->line);
    exit(1);
}

static void lex_init(Lexer *l, const char *src) {
    l->src = src; l->pos = 0; l->line = 1; l->col = 1;
    lex_next(l);
}

// ─── Parser + C Emitter ──────────────────────────────────────

static void emit_print_call(Lexer *l, FILE *out) {
    lex_next(l); // skip 'print'
    lex_next(l); // skip '('
    fprintf(out, "    {\n");
    fprintf(out, "        size_t total = 0;\n");
    fprintf(out, "        char *buf = NULL;\n");
    int arg_count = 0;
    while (l->cur.type != TOK_RPAREN) {
        if (arg_count > 0) lex_next(l); // skip ','
        if (l->cur.type == TOK_STRING) {
            int slen = l->cur.len - 2;
            char *content = l->cur.text + 1;
            content[slen] = 0;
            fprintf(out, "        {\n");
            fprintf(out, "            const char *part = \"%s\";\n", content);
            fprintf(out, "            size_t plen = strlen(part);\n");
            fprintf(out, "            buf = realloc(buf, total + plen + 1);\n");
            fprintf(out, "            memcpy(buf + total, part, plen);\n");
            fprintf(out, "            total += plen;\n");
            fprintf(out, "        }\n");
            lex_next(l);
        } else if (l->cur.type == TOK_IDENT) {
            fprintf(out, "        {\n");
            fprintf(out, "            size_t plen = %s.len;\n", l->cur.text);
            fprintf(out, "            buf = realloc(buf, total + plen + 1);\n");
            fprintf(out, "            memcpy(buf + total, %s.ptr, plen);\n", l->cur.text);
            fprintf(out, "            total += plen;\n");
            fprintf(out, "        }\n");
            lex_next(l);
        }
        arg_count++;
    }
    lex_next(l); // skip ')'
    fprintf(out, "        string result = { total, buf };\n");
    fprintf(out, "        print(result);\n");
    fprintf(out, "        free(buf);\n");
    fprintf(out, "    }\n");
}

static void emit_func_body(Lexer *l, FILE *out) {
    lex_next(l); // skip '{'
    fprintf(out, " {\n");
    while (l->cur.type != TOK_RBRACE) {
        if (l->cur.type == TOK_PRINT) {
            emit_print_call(l, out);
        } else {
            fprintf(stderr, "unexpected token '%s' in body\n", l->cur.text);
            exit(1);
        }
    }
    lex_next(l); // skip '}'
    fprintf(out, "}\n\n");
}

static void parse(Lexer *l, FILE *out) {
    // Emit headers
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <string.h>\n");
    fprintf(out, "#include <stdlib.h>\n\n");

    // Emit string type
    fprintf(out, "typedef struct { unsigned long long len; const char *ptr; } string;\n\n");

    // Emit print function
    fprintf(out, "static void print(string s) {\n");
    fprintf(out, "    fwrite(s.ptr, 1, s.len, stdout);\n");
    fprintf(out, "}\n\n");

    // Parse top-level declarations
    while (l->cur.type != TOK_EOF) {
        if (l->cur.type == TOK_FUNC) {
            lex_next(l); // skip 'func'
            char fname[256];
            snprintf(fname, sizeof(fname), "%s", l->cur.text);
            int is_main = !strcmp(fname, "main");
            lex_next(l); // skip function name

            lex_next(l); // skip '('

            if (is_main) {
                fprintf(out, "int main(int argc, char **argv)");
                // Skip all params
                while (l->cur.type != TOK_RPAREN) {
                    if (l->cur.type == TOK_COMMA) lex_next(l);
                    lex_next(l); // skip param name
                    lex_next(l); // skip ':'
                    lex_next(l); // skip type
                }
                lex_next(l); // skip ')'
                lex_next(l); // skip ':'
                lex_next(l); // skip return type
                // Body
                lex_next(l); // skip '{'
                fprintf(out, " {\n");
                fprintf(out, "    string inputString = { 0, NULL };\n");
                fprintf(out, "    if (argc > 1) {\n");
                fprintf(out, "        inputString.len = strlen(argv[1]);\n");
                fprintf(out, "        inputString.ptr = argv[1];\n");
                fprintf(out, "    }\n");
                // Emit body statements
                while (l->cur.type != TOK_RBRACE) {
                    if (l->cur.type == TOK_PRINT) {
                        emit_print_call(l, out);
                    } else {
                        fprintf(stderr, "unexpected token '%s' in main body\n", l->cur.text);
                        exit(1);
                    }
                }
                lex_next(l); // skip '}'
                fprintf(out, "    return 0;\n");
                fprintf(out, "}\n\n");
            } else {
                fprintf(out, "void %s(", fname);
                int first = 1;
                while (l->cur.type != TOK_RPAREN) {
                    if (!first) {
                        fprintf(out, ", ");
                        lex_next(l); // skip ','
                    }
                    first = 0;
                    char pname[256];
                    snprintf(pname, sizeof(pname), "%s", l->cur.text);
                    lex_next(l); // skip param name
                    lex_next(l); // skip ':'
                    fprintf(out, "string %s", pname);
                    lex_next(l); // skip type
                }
                fprintf(out, ")");
                lex_next(l); // skip ')'
                lex_next(l); // skip ':'
                lex_next(l); // skip return type
                emit_func_body(l, out);
            }
        } else {
            fprintf(stderr, "unexpected top-level token '%s'\n", l->cur.text);
            exit(1);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: aether2c <input.ae>\n"); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *src = malloc(sz + 1); fread(src, 1, sz, f); src[sz] = 0; fclose(f);

    Lexer lex; lex_init(&lex, src);
    parse(&lex, stdout);
    free(src);
    return 0;
}

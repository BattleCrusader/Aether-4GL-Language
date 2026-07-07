package main

import (
	"testing"
)

// ============================================================
// Lexer Tests
// ============================================================

func TestLexerKeywords(t *testing.T) {
	source := "func main() {\n\tlet x: i64 = 42\n\tvar y: string = \"hello\"\n\tif x > 0 {\n\t\treturn x\n\t}\n}"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) == 0 {
		t.Fatal("expected tokens, got none")
	}
	if tokens[0].Type != TOKEN_KW_FUNC {
		t.Errorf("expected TOKEN_KW_FUNC, got %s", tokenName(tokens[0].Type))
	}
}

func TestLexerOperators(t *testing.T) {
	source := "let a = 1 + 2 * 3 / 4 % 5\nlet b = a == 1 && b != 2 || c < 3\nlet c = a & b | c ^ d << 2 >> 3\na += 1\nb -= 2\nc *= 3\n++a\nb--\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) < 10 {
		t.Fatalf("expected at least 10 tokens, got %d", len(tokens))
	}
}

func TestLexerLiterals(t *testing.T) {
	source := "let a = 42\nlet b = 0xFF\nlet c = 0b1010\nlet d = 3.14\nlet e = \"hello\\nworld\"\nlet f = 'a'\nlet g = true\nlet h = false\nlet i = none\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) < 10 {
		t.Fatalf("expected at least 10 tokens, got %d", len(tokens))
	}
}

func TestLexerComments(t *testing.T) {
	source := "// this is a comment\nlet x = 1 // inline comment\n/* block comment */\nlet y = 2\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) < 8 {
		t.Fatalf("expected at least 8 tokens, got %d", len(tokens))
	}
}

func TestLexerAllKeywords(t *testing.T) {
	keywords := []string{
		"and", "as", "bool", "break", "byte",
		"catch", "char", "class", "const", "continue",
		"copy", "defer", "double", "dyn", "elif", "else",
		"enum", "export", "extern", "false", "float",
		"for", "func", "heap", "i8", "i16",
		"i32", "i64", "if", "impl", "import",
		"in", "init", "int", "internal", "let",
		"module", "none", "not", "or",
		"pool", "post", "pre", "private", "protocol",
		"public", "region",
		"return", "self", "static", "string", "struct",
		"super", "sys", "throw", "trait", "true",
		"try", "type", "u8", "u16", "u32",
		"u64", "u128", "unsafe", "var", "void",
		"while", "yield", "match", "case",
	}
	for _, kw := range keywords {
		lexer := New(kw+" ", "test.ae")
		tokens, err := lexer.Tokenize()
		if err != nil {
			t.Errorf("tokenization failed for keyword '%s': %v", kw, err)
			continue
		}
		if len(tokens) < 1 {
			t.Errorf("no tokens for keyword '%s'", kw)
			continue
		}
		if tokens[0].Type == TOKEN_IDENT {
			t.Errorf("keyword '%s' was tokenized as IDENT instead of keyword", kw)
		}
	}
}

func TestLexerStringInterpolation(t *testing.T) {
	source := "let s = \"hello {name} world\"\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) < 4 {
		t.Fatalf("expected at least 4 tokens, got %d", len(tokens))
	}
}

// ============================================================
// Parser Tests
// ============================================================

func parseSource(t *testing.T, source string) *Program {
	t.Helper()
	lexer := New(source, "test.ae")
	tokens, _ := lexer.Tokenize()
	parser := NewParser(tokens)
	prog := parser.Parse()
	if len(parser.errors) > 0 {
		t.Fatalf("parse errors: %v", parser.errors)
	}
	return prog
}

func TestParserEmptyProgram(t *testing.T) {
	prog := parseSource(t, "")
	if prog == nil {
		t.Fatal("expected non-nil program")
	}
}

func TestParserFuncDecl(t *testing.T) {
	prog := parseSource(t, "func main() {\n\tlet x: i64 = 42\n}\n")
	if len(prog.Decls) != 1 {
		t.Fatalf("expected 1 declaration, got %d", len(prog.Decls))
	}
	fd, ok := prog.Decls[0].(*FuncDecl)
	if !ok {
		t.Fatalf("expected FuncDecl, got %T", prog.Decls[0])
	}
	if fd.Name != "main" {
		t.Errorf("expected function name 'main', got '%s'", fd.Name)
	}
}

func TestParserFuncWithParams(t *testing.T) {
	prog := parseSource(t, "func add(a: i64, b: i64): i64 {\n\treturn a + b\n}\n")
	if len(prog.Decls) != 1 {
		t.Fatalf("expected 1 declaration, got %d", len(prog.Decls))
	}
	fd := prog.Decls[0].(*FuncDecl)
	if len(fd.Params) != 2 {
		t.Errorf("expected 2 params, got %d", len(fd.Params))
	}
	if fd.ReturnType == nil || fd.ReturnType.Name != "i64" {
		t.Errorf("expected return type i64, got %v", fd.ReturnType)
	}
}

func TestParserIfElse(t *testing.T) {
	prog := parseSource(t, "func test() {\n\tif x > 0 {\n\t\treturn x\n\t} elif x == 0 {\n\t\treturn 0\n\t} else {\n\t\treturn -x\n\t}\n}\n")
	if len(prog.Decls) != 1 {
		t.Fatalf("expected 1 declaration, got %d", len(prog.Decls))
	}
}

func TestParserWhileLoop(t *testing.T) {
	parseSource(t, "func test() {\n\twhile x < 10 {\n\t\tx = x + 1\n\t}\n}\n")
}

func TestParserForLoop(t *testing.T) {
	parseSource(t, "func test() {\n\tfor i in 0..10 {\n\t\tprint(i)\n\t}\n}\n")
}

func TestParserMatch(t *testing.T) {
	parseSource(t, "func test(x: i64): i64 {\n\tmatch x {\n\t\tcase 1 -> 10\n\t\tcase 2 -> 20\n\t\telse -> 0\n\t}\n}\n")
}

func TestParserBinaryExpr(t *testing.T) {
	parseSource(t, "func test() {\n\tlet a = 1 + 2 * 3\n\tlet b = (a + 1) / 2\n\tlet c = a == b && c != d\n\tlet d = a & b | c ^ d\n}\n")
}

func TestParserUnaryExpr(t *testing.T) {
	parseSource(t, "func test() {\n\tlet a = -x\n\tlet b = !flag\n\tlet c = ~bits\n\t++counter\n\tvalue--\n}\n")
}

func TestParserArrayLiteral(t *testing.T) {
	parseSource(t, "func test() {\n\tlet arr = [1, 2, 3, 4, 5]\n\tlet first = arr[0]\n}\n")
}

func TestParserStringInterpolation(t *testing.T) {
	parseSource(t, "func test() {\n\tlet name = \"world\"\n\tlet s = \"hello {name}\"\n}\n")
}

func TestParserDeferTryThrow(t *testing.T) {
	parseSource(t, "func test() {\n\tdefer {\n\t\tcleanup()\n\t}\n\ttry {\n\t\trisky()\n\t} catch e {\n\t\tprint(e)\n\t}\n\tthrow \"error\"\n}\n")
}

func TestParserStructDecl(t *testing.T) {
	prog := parseSource(t, "struct Point {\n\tx: i64,\n\ty: i64,\n}\n")
	if len(prog.Decls) != 1 {
		t.Fatalf("expected 1 declaration, got %d", len(prog.Decls))
	}
}

func TestParserEnumDecl(t *testing.T) {
	parseSource(t, "enum Color {\n\tRed,\n\tGreen,\n\tBlue,\n}\n")
}

func TestParserCompoundAssignment(t *testing.T) {
	parseSource(t, "func test() {\n\tvar x: i64 = 10\n\tx += 5\n\tx -= 3\n\tx *= 2\n\tx /= 4\n}\n")
}

func TestParserMethodCall(t *testing.T) {
	parseSource(t, "func test() {\n\tlet len = str.length()\n\tlet result = obj.method(arg1, arg2)\n}\n")
}

// ============================================================
// Semantic Analyzer Tests
// ============================================================

func TestSemanticBasic(t *testing.T) {
	prog := parseSource(t, "func main() {\n\tlet x: i64 = 42\n}\n")
	sem := NewSemanticAnalyzer()
	sem.Analyze(prog)
	if len(sem.errors) > 0 {
		t.Fatalf("semantic errors: %v", sem.errors)
	}
}

func TestSemanticLetWithoutInit(t *testing.T) {
	prog := parseSource(t, "func main() {\n\tlet x: i64\n}\n")
	sem := NewSemanticAnalyzer()
	sem.Analyze(prog)
	if len(sem.errors) == 0 {
		t.Error("expected semantic error for let without initializer")
	}
}

func TestSemanticUndefinedSymbol(t *testing.T) {
	prog := parseSource(t, "func main() {\n\tlet x = undefinedVar\n}\n")
	sem := NewSemanticAnalyzer()
	sem.Analyze(prog)
	if len(sem.errors) == 0 {
		t.Error("expected semantic error for undefined variable")
	}
}

// ============================================================
// Integration Tests
// ============================================================

func TestFullPipeline(t *testing.T) {
	prog := parseSource(t, "func main() {\n\tlet x: i64 = 42\n}\n")
	sem := NewSemanticAnalyzer()
	sem.Analyze(prog)
	if len(sem.errors) > 0 {
		t.Fatalf("semantic errors: %v", sem.errors)
	}
	cg := NewCodegen(prog)
	text, data, rodata, bss := cg.Generate()
	if len(cg.errors) > 0 {
		t.Fatalf("codegen errors: %v", cg.errors)
	}
	if len(text) == 0 {
		t.Error("expected non-empty text section")
	}
	lnk := NewLinker()
	err := lnk.Link(text, data, rodata, bss, cg.labels, "/tmp/test_pipeline_output")
	if err != nil {
		t.Fatalf("linking failed: %v", err)
	}
}

// ============================================================
// Edge Case Tests
// ============================================================

func TestLexerEmptyInput(t *testing.T) {
	lexer := New("", "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) != 1 || tokens[0].Type != TOKEN_EOF {
		t.Errorf("expected single EOF token, got %d tokens", len(tokens))
	}
}

func TestLexerUnterminatedString(t *testing.T) {
	lexer := New("let s = \"hello\n", "test.ae")
	_, err := lexer.Tokenize()
	if err == nil {
		t.Error("expected error for unterminated string")
	}
}

func TestParserEmptyBlock(t *testing.T) {
	parseSource(t, "func main() {\n}\n")
}

func TestParserNestedBlocks(t *testing.T) {
	parseSource(t, "func test() {\n\tif true {\n\t\tif false {\n\t\t\tlet x = 1\n\t\t}\n\t}\n}\n")
}

func TestLexerHexBinaryOctal(t *testing.T) {
	source := "let a = 0xFF\nlet b = 0b1010\nlet c = 0o77\nlet d = 1_000_000\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) < 8 {
		t.Fatalf("expected at least 8 tokens, got %d", len(tokens))
	}
}

func TestParserMultipleFunctions(t *testing.T) {
	prog := parseSource(t, "func add(a: i64, b: i64): i64 {\n\treturn a + b\n}\n\nfunc sub(a: i64, b: i64): i64 {\n\treturn a - b\n}\n\nfunc main() {\n\tlet x = add(1, 2)\n\tlet y = sub(5, 3)\n}\n")
	if len(prog.Decls) != 3 {
		t.Fatalf("expected 3 declarations, got %d", len(prog.Decls))
	}
}

func TestParserRangeExpr(t *testing.T) {
	parseSource(t, "func test() {\n\tfor i in 0..10 {\n\t\tprint(i)\n\t}\n\tfor i in 0..=100 {\n\t\tprint(i)\n\t}\n}\n")
}

func TestParserTuple(t *testing.T) {
	parseSource(t, "func test() {\n\tlet pair = (1, \"hello\")\n\tlet (a, b) = pair\n}\n")
}

func TestParserForEach(t *testing.T) {
	parseSource(t, "func test() {\n\tfor i, val in arr {\n\t\tprint(i)\n\t\tprint(val)\n\t}\n}\n")
}

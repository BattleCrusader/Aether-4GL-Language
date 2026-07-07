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
	parseSource(t, "func test() {\n	for i, val in arr {\n		print(i)\n		print(val)\n	}\n}\n")
}

// ============================================================
// Additional Edge Case Tests
// ============================================================

func TestLexerNestedBlockComments(t *testing.T) {
	source := "/* outer /* inner */ still comment */\nlet x = 1\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) < 4 {
		t.Fatalf("expected at least 4 tokens, got %d", len(tokens))
	}
}

func TestLexerUnterminatedBlockComment(t *testing.T) {
	source := "/* unterminated\nlet x = 1\n"
	lexer := New(source, "test.ae")
	_, err := lexer.Tokenize()
	if err == nil {
		t.Error("expected error for unterminated block comment")
	}
}

func TestLexerFloatExponent(t *testing.T) {
	source := "let a = 1e10\nlet b = 2.5e-3\nlet c = 0xFF.0p-3\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) < 6 {
		t.Fatalf("expected at least 6 tokens, got %d", len(tokens))
	}
}

func TestLexerEmptyString(t *testing.T) {
	source := "let s = \"\"\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) < 4 {
		t.Fatalf("expected at least 4 tokens, got %d", len(tokens))
	}
}

func TestLexerEscapeSequences(t *testing.T) {
	source := `let s = "hello\nworld	tab\\backslash\"quote"
`
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) < 4 {
		t.Fatalf("expected at least 4 tokens, got %d", len(tokens))
	}
}

func TestLexerUnderscoreNumbers(t *testing.T) {
	source := "let a = 1_2_3_456\nlet b = 0xFF_FF_FF\nlet c = 0b1010_0101\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) < 6 {
		t.Fatalf("expected at least 6 tokens, got %d", len(tokens))
	}
}

func TestLexerIdentifiers(t *testing.T) {
	source := "let _x = 1\nlet _ = 2\nlet camelCase = 3\nlet snake_case_name = 4\nlet x123 = 5\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	// Count IDENT tokens
	identCount := 0
	for _, tok := range tokens {
		if tok.Type == TOKEN_IDENT {
			identCount++
		}
	}
	if identCount < 5 {
		t.Fatalf("expected at least 5 identifiers, got %d", identCount)
	}
}

func TestLexerRangeVsFloat(t *testing.T) {
	source := "let r = 0..10\nlet f = 3.14\nlet ri = 0..=100\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	// Check that .. is tokenized as DOT_DOT, not as part of a float
	hasDotDot := false
	hasFloat := false
	for _, tok := range tokens {
		if tok.Type == TOKEN_DOT_DOT {
			hasDotDot = true
		}
		if tok.Type == TOKEN_FLOAT {
			hasFloat = true
		}
	}
	if !hasDotDot {
		t.Error("expected DOT_DOT token for range operator")
	}
	if !hasFloat {
		t.Error("expected FLOAT token for 3.14")
	}
}

func TestLexerCharLiteral(t *testing.T) {
	source := "let c = 'a'\nlet nl = '\\n'\n"
	lexer := New(source, "test.ae")
	tokens, err := lexer.Tokenize()
	if err != nil {
		t.Fatalf("tokenization failed: %v", err)
	}
	if len(tokens) < 4 {
		t.Fatalf("expected at least 4 tokens, got %d", len(tokens))
	}
}

func TestParserEmptyStruct(t *testing.T) {
	prog := parseSource(t, "struct Empty {\n}\n")
	if len(prog.Decls) != 1 {
		t.Fatalf("expected 1 declaration, got %d", len(prog.Decls))
	}
}

func TestParserEmptyEnum(t *testing.T) {
	prog := parseSource(t, "enum Empty {\n}\n")
	if len(prog.Decls) != 1 {
		t.Fatalf("expected 1 declaration, got %d", len(prog.Decls))
	}
}

func TestParserFuncNoParamsNoReturn(t *testing.T) {
	prog := parseSource(t, "func main() {\n}\n")
	if len(prog.Decls) != 1 {
		t.Fatalf("expected 1 declaration, got %d", len(prog.Decls))
	}
	fd := prog.Decls[0].(*FuncDecl)
	if len(fd.Params) != 0 {
		t.Errorf("expected 0 params, got %d", len(fd.Params))
	}
	if fd.ReturnType != nil {
		t.Errorf("expected nil return type, got %v", fd.ReturnType)
	}
}

func TestParserDeeplyNestedIf(t *testing.T) {
	parseSource(t, "func test() {\n	if a {\n		if b {\n			if c {\n				if d {\n					let x = 1\n				}\n			}\n		}\n	}\n}\n")
}

func TestParserChainedMemberAccess(t *testing.T) {
	parseSource(t, "func test() {\n	let val = a.b.c.d\n	let result = obj.method().field\n}\n")
}

func TestParserBreakContinueLabels(t *testing.T) {
	parseSource(t, "func test() {\n	while true {\n		break outer\n		continue inner\n	}\n}\n")
}

func TestParserCopyHeapPrefix(t *testing.T) {
	parseSource(t, "func test() {\n	let a = copy x\n	let b = heap y\n}\n")
}

func TestParserSelfKeyword(t *testing.T) {
	parseSource(t, "func test() {\n	let s = self\n}\n")
}

func TestParserAsKeyword(t *testing.T) {
	parseSource(t, "func test() {\n	let x = val as i64\n}\n")
}

func TestParserScopeBlock(t *testing.T) {
	parseSource(t, "func test() {\n	let x = scope {\n		let y = 1\n		y\n	}\n}\n")
}

func TestParserBitwiseOperators(t *testing.T) {
	parseSource(t, "func test() {\n	let a = x & y\n	let b = x | y\n	let c = x ^ y\n	let d = x << 2\n	let e = x >> 3\n	let f = ~x\n}\n")
}

func TestParserComparisonChain(t *testing.T) {
	parseSource(t, "func test() {\n	let a = x < y\n	let b = x <= y\n	let c = x > y\n	let d = x >= y\n	let e = x == y\n	let f = x != y\n}\n")
}

func TestParserLogicalOperators(t *testing.T) {
	parseSource(t, "func test() {\n	let a = x and y\n	let b = x or y\n	let c = not x\n	let d = x && y\n	let e = x || y\n	let f = !x\n}\n")
}

func TestParserArrayTypeAnnotation(t *testing.T) {
	parseSource(t, "func test(arr: [i64]) {\n	let first = arr[0]\n}\n")
}

func TestParserOptionalType(t *testing.T) {
	parseSource(t, "func test() {\n	let x: i64? = none\n}\n")
}

func TestParserTupleType(t *testing.T) {
	parseSource(t, "func test() {\n	let pair: (i64, string) = (1, \"hello\")\n}\n")
}

func TestParserFunctionType(t *testing.T) {
	parseSource(t, "func test() {\n	let fn: func(i64): string\n}\n")
}

func TestParserVariadicParams(t *testing.T) {
	parseSource(t, "func test(args: ...i64) {\n	let first = args[0]\n}\n")
}

func TestParserEnumWithPayload(t *testing.T) {
	prog := parseSource(t, "enum Option {\n	Some(i64),\n	None,\n}\n")
	if len(prog.Decls) != 1 {
		t.Fatalf("expected 1 declaration, got %d", len(prog.Decls))
	}
}

func TestParserClassDecl(t *testing.T) {
	prog := parseSource(t, "class Animal {\n}\n")
	if len(prog.Decls) != 1 {
		t.Fatalf("expected 1 declaration, got %d", len(prog.Decls))
	}
}

func TestParserImportDecl(t *testing.T) {
	prog := parseSource(t, "import \"std/io.ae\"\nfunc main() {}\n")
	if len(prog.Decls) != 2 {
		t.Fatalf("expected 2 declarations, got %d", len(prog.Decls))
	}
}

func TestParserConstDecl(t *testing.T) {
	prog := parseSource(t, "const MAX: i64 = 100\nfunc main() {}\n")
	if len(prog.Decls) != 2 {
		t.Fatalf("expected 2 declarations, got %d", len(prog.Decls))
	}
}

func TestParserAttributes(t *testing.T) {
	prog := parseSource(t, "@entry\nfunc main() {}\n@test\nfunc testFunc() {}\n")
	if len(prog.Decls) != 2 {
		t.Fatalf("expected 2 declarations, got %d", len(prog.Decls))
	}
}

func TestSemanticVarWithoutInit(t *testing.T) {
	prog := parseSource(t, "func main() {\n	var x: i64\n}\n")
	sem := NewSemanticAnalyzer()
	sem.Analyze(prog)
	// var without initializer is OK (unlike let)
	if len(sem.errors) > 0 {
		t.Fatalf("unexpected semantic errors: %v", sem.errors)
	}
}

func TestSemanticScope(t *testing.T) {
	prog := parseSource(t, "func main() {\n	let x: i64 = 1\n	if true {\n		let y: i64 = x + 1\n	}\n}\n")
	sem := NewSemanticAnalyzer()
	sem.Analyze(prog)
	if len(sem.errors) > 0 {
		t.Fatalf("unexpected semantic errors: %v", sem.errors)
	}
}

func TestSemanticFunctionCall(t *testing.T) {
	prog := parseSource(t, "func add(a: i64, b: i64): i64 {\n	return a + b\n}\nfunc main() {\n	let result = add(1, 2)\n}\n")
	sem := NewSemanticAnalyzer()
	sem.Analyze(prog)
	if len(sem.errors) > 0 {
		t.Fatalf("unexpected semantic errors: %v", sem.errors)
	}
}

func TestCodegenEmptyFunc(t *testing.T) {
	prog := parseSource(t, "func main() {\n}\n")
	cg := NewCodegen(prog)
	text, _, _, _ := cg.Generate()
	if len(cg.errors) > 0 {
		t.Fatalf("codegen errors: %v", cg.errors)
	}
	if len(text) == 0 {
		t.Error("expected non-empty text section")
	}
}

func TestCodegenMultipleFuncs(t *testing.T) {
	prog := parseSource(t, "func helper(): i64 {\n	return 42\n}\nfunc main() {\n	let x = helper()\n}\n")
	cg := NewCodegen(prog)
	text, _, _, _ := cg.Generate()
	if len(cg.errors) > 0 {
		t.Fatalf("codegen errors: %v", cg.errors)
	}
	if len(text) == 0 {
		t.Error("expected non-empty text section")
	}
}

func TestCodegenIfWhileFor(t *testing.T) {
	prog := parseSource(t, "func main() {\n	if true {\n		let x = 1\n	}\n	while false {\n		let y = 2\n	}\n	for i in 0..10 {\n		let z = i\n	}\n}\n")
	cg := NewCodegen(prog)
	text, _, _, _ := cg.Generate()
	if len(cg.errors) > 0 {
		t.Fatalf("codegen errors: %v", cg.errors)
	}
	if len(text) == 0 {
		t.Error("expected non-empty text section")
	}
}

func TestFullPipelineWithAllFeatures(t *testing.T) {
	source := "func add(a: i64, b: i64): i64 {\n	return a + b\n}\n\nfunc main() {\n	let x: i64 = 42\n	let y: i64 = 10\n	let result = add(x, y)\n	if result > 0 {\n		let msg = \"positive\"\n	}\n}\n"
	prog := parseSource(t, source)
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
	err := lnk.Link(text, data, rodata, bss, cg.labels, "/tmp/test_full_pipeline_output")
	if err != nil {
		t.Fatalf("linking failed: %v", err)
	}
}

// ============================================================
// Brace-less block tests
// ============================================================

func TestParserIfWithoutBraces(t *testing.T) {
	parseSource(t, "func test() {\n	if a == b return a\n}\n")
}

func TestParserIfElseWithoutBraces(t *testing.T) {
	parseSource(t, "func test() {\n	if a == b return a else return b\n}\n")
}

func TestParserWhileWithoutBraces(t *testing.T) {
	parseSource(t, "func test() {\n	while x < 10 x = x + 1\n}\n")
}

func TestParserForWithoutBraces(t *testing.T) {
	parseSource(t, "func test() {\n	for i in 0..10 print(i)\n}\n")
}

func TestParserIfOnOneLine(t *testing.T) {
	parseSource(t, "func test() {\n	if a == b { return a }\n}\n")
}

func TestParserWhileOnOneLine(t *testing.T) {
	parseSource(t, "func test() {\n	while x < 10 { x = x + 1 }\n}\n")
}

func TestParserForOnOneLine(t *testing.T) {
	parseSource(t, "func test() {\n	for i in 0..10 { print(i) }\n}\n")
}

func TestParserDeferWithoutBraces(t *testing.T) {
	parseSource(t, "func test() {\n	defer cleanup()\n}\n")
}

func TestParserTryCatchWithoutBraces(t *testing.T) {
	parseSource(t, "func test() {\n	try risky() catch e print(e)\n}\n")
}

func TestParserMatchCaseWithoutBraces(t *testing.T) {
	parseSource(t, "func test(x: i64): i64 {\n	match x {\n		case 1 -> 10\n		case 2 -> 20\n		else -> 0\n	}\n}\n")
}

func TestParserNestedIfWithoutBraces(t *testing.T) {
	parseSource(t, "func test() {\n	if a if b return c\n}\n")
}

func TestParserMixedBracesAndNoBraces(t *testing.T) {
	parseSource(t, "func test() {\n	if a {\n		if b return c\n	} else {\n		if d return e\n	}\n}\n")
}

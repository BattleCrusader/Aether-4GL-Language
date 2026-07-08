// Package bootstrap is the Aether bootstrap compiler — a Go-based compiler
// that compiles Aether source (.ae files) to native ELF64 binaries.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

var (
	flagDumpAST    bool
	flagDumpTokens bool
	flagOutput     string
	flagHelp       bool
)

func init() {
	flag.BoolVar(&flagDumpAST, "dump-ast", false, "Dump the AST and exit")
	flag.BoolVar(&flagDumpTokens, "dump-tokens", false, "Dump tokens and exit")
	flag.StringVar(&flagOutput, "o", "", "Output file path")
	flag.BoolVar(&flagHelp, "help", false, "Show help message")
}

func usage() {
	fmt.Fprintf(os.Stderr, `Aether Bootstrap Compiler

Usage:
  aether [options] <source.ae>...
  aether -o <output> <source.ae>...

Options:
  -dump-ast       Dump the AST and exit
  -dump-tokens    Dump tokens and exit
  -o <path>       Output file path
  -help           Show this help message

Examples:
  aether hello.ae -o hello
  aether aether/*.ae -o aether

`)
}

func main() {
	args := os.Args[1:]
	var flags []string
	var positional []string
	i := 0
	for i < len(args) {
		a := args[i]
		if strings.HasPrefix(a, "-") {
			flags = append(flags, a)
			if a == "-o" && i+1 < len(args) && !strings.HasPrefix(args[i+1], "-") {
				i++
				flags = append(flags, args[i])
			}
		} else {
			positional = append(positional, a)
		}
		i++
	}
	flag.CommandLine.Parse(append(flags, positional...))

	if flag.NArg() < 1 || flagHelp {
		usage()
		if flagHelp {
			return
		}
		fmt.Fprintf(os.Stderr, "error: expected at least one source file argument\n")
		os.Exit(1)
	}

	// Load all source files and concatenate
	var sourceBuilder strings.Builder
	var sourceFiles []string
	loaded := make(map[string]bool)

	for _, srcPath := range flag.Args() {
		err := loadSource(srcPath, &sourceBuilder, &sourceFiles, loaded)
		if err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			os.Exit(1)
		}
	}

	source := sourceBuilder.String()

	// Phase 1: Tokenization
	lexer := New(source, strings.Join(sourceFiles, ", "))
	tokens, err := lexer.Tokenize()
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: tokenization failed: %v\n", err)
		os.Exit(1)
	}

	if flagDumpTokens {
		fmt.Println("=== Tokens ===")
		for _, tok := range tokens {
			fmt.Printf("%s %q [%d:%d]\n", tokenName(tok.Type), tok.Lexeme, tok.Line, tok.Column)
		}
		return
	}

	// Phase 2: Parsing
	parser := NewParser(tokens)
	prog := parser.Parse()
	if len(parser.errors) > 0 {
		for _, err := range parser.errors {
			fmt.Fprintf(os.Stderr, "error: %s\n", err)
		}
		os.Exit(1)
	}

	if flagDumpAST {
		fmt.Println("=== AST ===")
		fmt.Println(PrintTree(prog, 0))
		return
	}

	// Phase 3: Semantic Analysis
	sem := NewSemanticAnalyzer()
	sem.Analyze(prog)
	// Phase 2: semantic errors are non-fatal — codegen handles what it can
	if len(sem.errors) > 0 && flagDumpAST {
		for _, err := range sem.errors {
			fmt.Fprintf(os.Stderr, "warning: %s\n", err)
		}
	}

	// Phase 4: Code Generation
	cg := NewCodegen(prog)
	text, data, rodata, bss := cg.Generate()
	if len(cg.errors) > 0 {
		for _, err := range cg.errors {
			fmt.Fprintf(os.Stderr, "error: %s\n", err)
		}
		os.Exit(1)
	}

	// Phase 5: Linking
	lnk := NewLinker()
	outPath := flagOutput
	if outPath == "" {
		stem := filepath.Base(flag.Arg(0))
		if ext := filepath.Ext(stem); ext == ".ae" {
			stem = stem[:len(stem)-len(ext)]
		}
		outPath = stem
	}

	err = lnk.Link(text, data, rodata, bss, cg.labels, outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: linking failed: %v\n", err)
		os.Exit(1)
	}

	fmt.Printf("Compiled %s -> %s\n", strings.Join(flag.Args(), ", "), outPath)
}

func loadSource(path string, builder *strings.Builder, files *[]string, loaded map[string]bool) error {
	abs, err := filepath.Abs(path)
	if err != nil {
		return fmt.Errorf("failed to resolve %q: %w", path, err)
	}
	if loaded[abs] {
		return nil
	}
	loaded[abs] = true

	src, err := os.ReadFile(path)
	if err != nil {
		return fmt.Errorf("failed to read %q: %w", path, err)
	}

	*files = append(*files, path)
	builder.Write(src)
	builder.WriteString("\n")

	return nil
}

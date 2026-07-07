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
  aether [options] <source.ae>
  aether -o <output> <source.ae>

Options:
  -dump-ast       Dump the AST and exit
  -dump-tokens    Dump tokens and exit
  -o <path>       Output file path
  -help           Show this help message

Examples:
  aether hello.ae -o hello
  aether hello.ae --dump-ast

`)
}

func main() {
	// Reorder args so flags come before positional args
	// Handle flag-value pairs (e.g., -o output)
	args := os.Args[1:]
	var flags []string
	var positional []string
	i := 0
	for i < len(args) {
		a := args[i]
		if strings.HasPrefix(a, "-") {
			flags = append(flags, a)
			// If this flag takes a value and next arg isn't a flag, include it
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
		fmt.Fprintf(os.Stderr, "error: expected a source file argument\n")
		os.Exit(1)
	}

	srcPath := flag.CommandLine.Arg(0)

	// Read source file
	src, err := os.ReadFile(srcPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: failed to read source file %q: %v\n", srcPath, err)
		os.Exit(1)
	}

	// Phase 1: Tokenization
	lexer := New(string(src), srcPath)
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
	if len(sem.errors) > 0 {
		for _, err := range sem.errors {
			fmt.Fprintf(os.Stderr, "error: %s\n", err)
		}
		os.Exit(1)
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
		stem := filepath.Base(srcPath)
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

	fmt.Printf("Compiled %s -> %s\n", srcPath, outPath)
}

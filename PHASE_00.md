# Phase 00 — Bootstrap Toolchain

**Goal**: A C-based bootstrap compiler that can tokenize, parse, and compile a minimal Aether source file (`.ae`) into a valid x86_64 ELF64 binary that runs on Aether OS or QEMU.

**Strategy**: Build in layers. Each milestone produces a working artifact that can be tested and verified before moving on.

---

## Task Breakdown

### P00.01 — Project Structure (`P00.01`) 🟢
- [x] Create directory tree: `src/`, `include/`, `tests/`, `tools/`
- [x] Create `Makefile` with targets: `build`, `test`, `clean`
- [x] Create `include/aether/` with header files

### P00.02 — Build System (`P00.02`) 🟢
- [x] Write `Makefile` with proper C11 compilation flags
- [x] Add NASM invocation for .asm → .o
- [x] Add linker invocation for .o → .elf
- [x] Add `test` target that compiles fixtures + runs them in QEMU
- [x] Verify `make` produces a binary

### P00.03 — Tokenizer / Lexer (`P00.03`) 🟢
- [ ] Token type enum: TOKEN_EOF, TOKEN_IDENT, TOKEN_NUMBER, TOKEN_STRING, TOKEN_CHAR, TOKEN_KEYWORD, TOKEN_OPERATOR, TOKEN_NEWLINE, TOKEN_INDENT, TOKEN_DEDENT, TOKEN_COMMENT
- [ ] Token struct: `type`, `start` (source ptr), `len`, `line`, `col`
- [ ] Keywords table: `func`, `let`, `mut`, `if`, `elif`, `else`, `while`, `for`, `in`, `return`, `true`, `false`, `none`, `asm`, `break`, `continue`, `struct`, `enum`, `class`, `match`, `case`, `try`, `throw`, `catch`, `and`, `or`, `not`, `import`, `const`, `ref`, `owned`, `heap`, `region`, `pub`, `static`, `defer`, `unsafe`, `module`, `sys`, `throws`
- [ ] `#` line comments (skip to newline)
- [ ] `#{` `}#` block comments (nestable, skip entire block)
- [ ] Integer literals: decimal (`42`), hex (`0xFF`), binary (`0b1010`), octal (`0o77`), with `_` separators
- [ ] Float literals: `3.14`, `1e10`, `0xFF.0p-3`
- [ ] String literals: double-quoted, escape sequences (`\n`, `\t`, `\\`, `\"`, `\xNN`, `\u{NNNN}`)
- [ ] Char literals: single-quoted: `'a'`, `'\n'`, `'\x41'`
- [ ] Identifiers: `[a-zA-Z_][a-zA-Z0-9_]*`, keyword resolution
- [ ] Operators and punctuation: `+`, `-`, `*`, `/`, `%`, `=`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`, `&`, `|`, `^`, `~`, `<<`, `>>`, `->`, `..`, `..=`, `:`, `,`, `.`, `(`, `)`, `[`, `]`, `{`, `}`, `|` (lambda pipe)
- [ ] **Indentation engine**: track indent levels, emit TOKEN_INDENT/TOKEN_DEDENT tokens at line boundaries
- [ ] Newline handling: statements end at newlines, continuation via operator at line end or `\`
- [ ] Tokenizer test suite: test each token type, edge cases, indentation
- [ ] **MILESTONE**: `tokenizer_test` executable that tokenizes a file and dumps tokens

### P00.04 — Lexer / Stream (`P00.04`) 🟢
- [x] Lexer struct wrapping tokenizer: `lexer_next()`, `lexer_peek()`, `lexer_expect(type)`
- [x] Lexer re-synchronization on error
- [x] Lexer test suite

### P00.05 — AST Definitions (`P00.05`) 🟢
- [x] AST node types: PROGRAM, FUNC_DECL, PARAM, LET, IF, WHILE, FOR, RETURN, BREAK, CONTINUE, binary/unary ops, CALL, IDENT, literals, ASM_BLOCK, MATCH, STRUCT, ENUM, FIELD, INDEX, SLICE, types
  - [ ] `AST_PROGRAM` — top-level list of declarations
  - [ ] `AST_FUNC_DECL` — function with name, params, return type, body
  - [ ] `AST_PARAM` — parameter with name, type
  - [ ] `AST_LET` — variable declaration
  - [ ] `AST_ASSIGN` — variable assignment
  - [ ] `AST_IF` — if / elif / else chain
  - [ ] `AST_WHILE` — while loop
  - [ ] `AST_FOR` — for loop with range or collection
  - [ ] `AST_RETURN` — return statement
  - [ ] `AST_BREAK` / `AST_CONTINUE` — loop control
  - [ ] `AST_BINARY_OP` — binary expression (+, -, etc.)
  - [ ] `AST_UNARY_OP` — unary expression (!, -, ~)
  - [ ] `AST_CALL` — function call
  - [ ] `AST_IDENT` — identifier reference
  - [ ] `AST_INT_LITERAL` — integer literal
  - [ ] `AST_FLOAT_LITERAL` — float literal
  - [ ] `AST_STRING_LITERAL` — string literal
  - [ ] `AST_CHAR_LITERAL` — char literal
  - [ ] `AST_BOOL_LITERAL` — true/false
  - [ ] `AST_NONE_LITERAL` — none
  - [ ] `AST_BLOCK` — sequence of statements (indentation block)
  - [ ] `AST_TYPE` — type annotation
  - [ ] `AST_ASM_BLOCK` — inline assembly
  - [ ] `AST_DEFER` — defer statement
  - [ ] `AST_MATCH` — match expression
  - [ ] `AST_STRUCT_DECL` — struct definition
  - [ ] `AST_FIELD` — struct/field member
  - [ ] `AST_INDEX` — array indexing
  - [ ] `AST_SLICE` — array slicing `a[i..j]`
- [ ] AST node struct: `type`, `line`, `col`, union of type-specific data (tagged union)
- [ ] AST arena allocator (all nodes allocated from a region, freed as batch)
- [ ] AST pretty printer for debugging (`ast_dump(node, depth)`)
- [ ] **MILESTONE**: ast_test that parses source into AST and dumps it

### P00.06 — Parser (`P00.06`)
- [ ] **Expressions** (Pratt parser with precedence):
  - [ ] Precedence levels: lowest=assignment, ternary, logical OR, logical AND, comparison, bitwise OR, XOR, AND, shift, additive, multiplicative, prefix, postfix, call, primary
  - [ ] Primary: literals, identifiers, parenthesized, blocks, match
  - [ ] Prefix: `-`, `!`, `~`, `&`, `*` (deref), `ref`, `owned`, `mut`
  - [ ] Postfix: `()`, `[]`, `.`, `?` (optional unwrap)
  - [ ] Binary: arithmetic, comparison, logical, bitwise, range (`..`, `..=`)
  - [ ] Ternary: n/a (if-else is expression)
  - [ ] Lambda: `|params| expr`
- [ ] **Top-level declarations**:
  - [ ] `func name(params) rettype block`
  - [ ] `struct name { fields }`
  - [ ] `enum name { variants }`
  - [ ] `const name = expr`
  - [ ] `import "path"`
  - [ ] `module name { decls }`
- [ ] **Statements**:
  - [ ] `let [mut] name [: type] = expr`
  - [ ] `return [expr]`
  - [ ] `if expr block [elif expr block]* [else block]`
  - [ ] `while expr block`
  - [ ] `for [i in] expr block`
  - [ ] `match expr { arms }`
  - [ ] `asm { ... }` (raw NASM text block)
  - [ ] `defer block`
  - [ ] `break`, `continue`
  - [ ] Expression statement (just an expression)
- [ ] **Blocks**: indentation-sensitive parsing (use INDENT/DEDENT tokens)
- [ ] Error recovery: skip to next statement boundary on parse error, report all errors
- [ ] **MILESTONE**: parse all test/parse_*.ae files successfully

### P00.07 — Semantic Analysis (`P00.07`)
- [ ] Name resolution: find function/type declarations by name
- [ ] Scope tracking: nested scopes, variable shadowing
- [ ] Type checking: basic type compatibility
- [ ] Return checking: ensure all paths in function return correct type
- [ ] Mutability checking: cannot assign to immutable variable
- [ ] **MILESTONE**: `semantic_test` passes for all valid test programs

### P00.08 — NASM Code Generation (`P00.08`)
- [ ] NASM string builder: emit instructions, labels, directives
- [ ] Function prologue/epilogue: push rbp, mov rbp rsp / leave ret
- [ ] Variable storage: stack slots for local variables, `let` → sub rsp, N
- [ ] Expression codegen:
  - [ ] Integer/float/string/char/bool/none literals
  - [ ] Identifier lookup (stack offset)
  - [ ] Binary operations (add, sub, mul, div, mod, comparisons)
  - [ ] Unary operations (neg, not, bitwise not)
  - [ ] Function calls (push args, call)
  - [ ] Array/slice indexing
- [ ] Statement codegen:
  - [ ] `let` → stack allocation + initialization
  - [ ] Assignment → mov to stack slot
  - [ ] `if/elif/else` → cmp + conditional jumps
  - [ ] `while` → loop label + conditional jump
  - [ ] `for` → counter init + increment + comparison
  - [ ] `return` → mov rax + leave ret
  - [ ] `break`/`continue` → jmp to loop labels
  - [ ] `defer` → push deferred code onto label stack
  - [ ] `match` → chained cmp/je
- [ ] `asm { ... }` — raw passthrough into emitted NASM
- [ ] String table: `.rodata` section for string literals
- [ ] NASM output file writer
- [ ] **MILESTONE**: `codegen_test` produces valid .asm file for any test program

### P00.09 — ELF64 Output (`P00.09`)
- [ ] ELF64 header struct and writer:
  - [ ] ELF header (magic, class, data, version, OS/ABI, entry point, phoff, shoff, flags, ehsize, phentsize, phnum, shentsize, shnum, shstrndx)
  - [ ] Program header (type, flags, offset, vaddr, paddr, filesz, memsz, align)
  - [ ] Section header (name, type, flags, addr, offset, size, link, info, addralign, entsize)
- [ ] Section generation:
  - [ ] `.text` — executable code at fixed address
  - [ ] `.rodata` — read-only data (strings, constants)
  - [ ] `.data` — initialized data
  - [ ] `.bss` — zero-initialized data
  - [ ] `.shstrtab` — section name string table
  - [ ] `.symtab` — symbol table (for module exports)
- [ ] Fixed-address loading: ELF for 0x2000000 (binary) or 0x1000000 (kernel)
- [ ] Linker script equivalent: map sections to fixed addresses
- [ ] Entry point setup: `_start` → call `main` → call `exit`
- [ ] **MILESTONE**: `elf_test` produces a valid `hello.elf` loaded at correct address

### P00.10 — NASM Assembler Integration (`P00.10`)
- [ ] Check if NASM is installed, error with install instructions if not
- [ ] Pipe generated .asm → nasm → .o → .elf
- [ ] Handle nasm exit codes and errors
- [ ] Alternative: fall back to flat binary with `objcopy -O binary`

### P00.11 — `aether build` CLI (`P00.11`)
- [ ] Minimal CLI in C: `aether build <file.ae> [--output <file.elf>] [--target x86_64-freestanding]`
- [ ] Pipeline: `.ae` → internal tokenize → parse → analyze → codegen → emit .asm → nasm → .o → ld → .elf
- [ ] Error reporting: print tokenizer/parser/semantic errors with file:line:col
- [ ] Exit codes: 0 on success, 1 on error
- [ ] `--output <path>` flag
- [ ] `--dump-ast` flag (print AST for debugging)
- [ ] `--dump-asm` flag (print generated NASM, don't assemble)
- [ ] Handle multi-file compilation (simple: pass `src/*.ae`)

### P00.12 — hello.ae End-to-End (`P00.12`)
- [ ] Write test program `hello.ae`
- [ ] Compile with `aether build hello.ae --output hello.elf`
- [ ] Verify ELF structure with `readelf -a hello.elf`
- [ ] Run in QEMU: `qemu-system-x86_64 -kernel hello.elf`
- [ ] **MILESTONE**: End-to-end pipeline produces a working binary

### P00.13 — Phase 0 Verification and Cleanup (`P00.13`)
- [ ] Run full test suite
- [ ] Fix all failures
- [ ] Verify `make clean && make test` from clean checkout
- [ ] Update STATUS.md — mark Phase 0 complete
- [ ] Create PHASE_01.md

---

## Legend

| Status | Meaning |
|--------|---------|
| 🔴 NOT STARTED | Planned |
| 🔵 IN PROGRESS | Being worked on |
| 🟢 COMPLETE | Done and verified |
| 🟡 BLOCKED | Waiting on dependency |
| ⚫ CANCELLED | Removed |
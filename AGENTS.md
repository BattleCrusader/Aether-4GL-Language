# Aether Compiler — AGENTS.md

> **Primary entry point for AI agents (Claude Code, Codex, Cursor, Copilot, etc.)**
> Read this first before making any changes. This file is kept up to date with the actual state of the codebase.

---

## Quick Facts

- **Language**: Aether (self-hosting goal)
- **Bootstrap**: Go (throwaway — compiles Aether source to native binary)
- **Output**: Native ELF64/Mach-O binary via direct codegen
- **Build**: `go build ./cmd/bootstrap` → `./aether`
- **Test**: `./aether tests/fixtures/*.ae`
- **Source**: `/Volumes/Backup/Development/Project_Aether/compiler/`
- **Branch**: `feature/P40.00-error-context-operator` (active development)

---

## Project Structure

```
compiler/
├── cmd/bootstrap/           # Bootstrap compiler (Go — throwaway)
│   └── main.go              # Bootstrap compiler entry point
├── aether/                  # Aether compiler source (written in Aether — the goal)
│   └── *.ae                 # Aether compiler modules
├── std/                     # Aether standard library
│   ├── io.ae
│   ├── mem.ae
│   ├── str.ae
│   ├── test.ae
│   └── *.ae
├── tests/fixtures/          # Aether test programs
├── SPECIFICATION.md         # Language specification
├── PLAN.md                  # Bootstrap plan
└── README.md
```

---

## The Bootstrap Pipeline (Option A — Direct Compilation)

```
Bootstrap compiler (Go)
    ├── Reads: aether/*.ae (Aether compiler source)
    ├── Emits: native binary (ELF64/Mach-O) directly
    └── Produces: ./aether (the real Aether compiler — written in Aether)

./aether hello.ae
    └── Produces: hello (native binary)

./aether aether/*.ae
    └── Self-hosting: compiles the compiler with itself
```

The bootstrap tool (Go) is throwaway. Once `./aether` works, the Go tool is deleted.

---

## Bootstrap Compiler (Go)

The bootstrap compiler lives in `cmd/bootstrap/` and compiles Aether source directly to a native binary.

| Component | Purpose |
|-----------|---------|
| `lexer.go` | Tokenize Aether source |
| `parser.go` | Recursive descent + Pratt parser → AST |
| `semantic.go` | Type checking, name resolution |
| `codegen.go` | Emit ELF64/Mach-O native code directly |
| `linker.go` | Linking, binary output |

### Build and Run

```bash
# Build the bootstrap compiler
go build -o aether ./cmd/bootstrap

# Compile an Aether program
./aether tests/fixtures/hello.ae -o hello

# Run it
./hello
```

### Bootstrap Constraints

- The bootstrap compiler is written in Go
- It emits native code directly — no C, no LLVM
- The Aether compiler source (`aether/*.ae`) uses only v1 features
- No features beyond v1 in the compiler source (no generics, no traits, no inheritance)

---

## Aether v1 Feature Set (Bootstrap Compiler Target)

The Aether compiler is written in Aether using only these features:

```
func, class, struct, enum
let, var
if, elif, else
while, for, in
match, case
return, break, continue
throw, try, catch
defer
import
public, private
copy, heap
asm { }
@entry, @export, @test, @force_inline
ref dyn Trait
```

### NOT in v1
`trait`, `impl`, inheritance, generics, closures, `yield`, `pool`, `post`/`pre` contracts, `#run`, `#embed`, operator overloading, custom operators.

---

## Aether Compiler Source Files (Target)

```
aether/
├── main.ae        — CLI, pipeline orchestration
├── tokenizer.ae   — token definitions
├── lexer.ae       — indentation engine
├── ast.ae         — AST node helpers
├── parser.ae      — recursive descent parser
├── semantic.ae    — type checker
├── codegen.ae     — native code emission
└── stdlib.ae      — builtin functions
```

---

## Key Design Rules

1. **Bootstrap tool is Go only** — no C, no LLVM, no other languages
2. **Aether compiler is pure Aether** — once compiled, it compiles itself
3. **No features beyond v1** — compiler source uses only the v1 feature set
4. **Direct native codegen** — bootstrap emits ELF64/Mach-O directly
5. **Throwaway bootstrap** — once `./aether` works, the Go tool is deleted
6. **All objects are reference types** — `ref` and `rc` removed in favor of implicit ownership
7. **`var` = mutable, `let` = immutable** — `mut` keyword removed
8. **`copy` forces pass-by-value** — when the programmer needs an explicit copy

---

## Reference: SPECIFICATION.md

The `SPECIFICATION.md` contains the full language specification. Key sections:

- §3.3 Keywords (68 keywords)
- §5 Variables and Bindings (`let`, `var`, `copy`)
- §8.4 Ownership and Memory Management (implicit, compiler-inferred)
- §11 ASM (inline assembly with NASM syntax)

---

## Reference: PLAN.md

The `PLAN.md` contains the 5-phase bootstrap plan:

1. Bootstrap tool (Go) — core language compiles
2. Aether compiler source (Aether) — full compiler written in Aether
3. First compile — bootstrap compiles the Aether compiler
4. Self-hosting — Aether compiles itself
5. Retire bootstrap — Go tool deleted

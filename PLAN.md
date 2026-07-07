# Bootstrap Plan — Option A: Direct Compilation

> **Purpose:** Get a native Aether compiler as fast as possible.
> Everything else is secondary.

---

## What We're Building

A **bootstrap compiler** written in Go that:
1. Reads Aether source (`.ae` files)
2. Compiles to a native ELF64/Mach-O binary directly
3. Produces the **real Aether compiler** — written in Aether, not Go

Once step 3 is achieved, the bootstrap tool is thrown away. The Aether compiler compiles itself and any Aether program.

---

## The Pipeline

```
Bootstrap compiler (Go)
    ├── Reads: aether/*.ae  (Aether compiler source)
    ├── Emits: native binary (ELF64/Mach-O)
    └── Produces: ./aether  (the Aether compiler — written in Aether)

./aether hello.ae
    └── Produces: hello  (native binary)

./aether aether/*.ae
    └── Self-hosting: compiles the compiler with itself
```

---

## Scope for v1 — Bootstrap Only

### Bootstrap Compiler (Go)

| Component | Description |
|-----------|-------------|
| **Lexer** | Tokenize Aether source (keywords, operators, identifiers, literals) |
| **Parser** | Recursive descent + Pratt parser → AST |
| **Semantic Analyzer** | Name resolution, type checking, type inference |
| **Codegen** | Emit native code (ELF64/Mach-O) directly — no intermediate C |
| **Standard Library** | Generated as part of the compilation output |

### Aether Language — v1 Minimal Feature Set

The Aether compiler source uses only v1 features:

| Feature | Status |
|---------|--------|
| `func` functions | ✅ |
| `class` (no inheritance, no traits) | ✅ |
| `struct` | ✅ |
| `enum` | ✅ |
| `let` / `var` | ✅ |
| `if` / `elif` / `else` | ✅ |
| `while` / `for` / `in` | ✅ |
| `match` / `case` | ✅ |
| `return` | ✅ |
| `break` / `continue` | ✅ |
| `throw` / `try` / `catch` | ✅ |
| `defer` | ✅ |
| `import` | ✅ |
| `public` / `private` | ✅ |
| `@entry`, `@export` attributes | ✅ |
| `@test` attribute | ✅ |
| Basic types: `u8`-`u64`, `i8`-`i64`, `f32`, `f64`, `bool`, `byte`, `string` | ✅ |
| Arrays (`[T; n]`), slices (`[T]`), strings | ✅ |
| Automatic memory management (escape analysis) | ✅ |
| `copy` keyword | ✅ |
| `heap` keyword | ✅ |
| Inline assembly (`asm { }`) | ✅ |
| `ref dyn Trait` dynamic dispatch | ✅ |
| `@force_inline`, `@export`, `@entry`, `@test` | ✅ |

### NOT in v1

These are deferred to v2+:

- `trait` / `impl Trait for Type`
- `dyn Trait` (use `ref dyn Trait` for dynamic dispatch)
- Inheritance (`extends`)
- Generics (`<T>`)
- Closures / lambdas
- Concurrency / fibers / `yield`
- `#run` compile-time execution
- `pool`
- Custom operators
- `post` / `pre` contracts
- `#embed`
- Operator overloading

### Aether Compiler Source Files (Target)

```
aether/
├── compiler/
│   ├── main.ae        — entry point, CLI, pipeline orchestration
│   ├── tokenizer.ae   — keyword table, tokenization
│   ├── lexer.ae       — lexer stream, indentation engine
│   ├── ast.ae         — AST node creation helpers
│   ├── parser.ae      — recursive descent + Pratt parser
│   ├── semantic.ae    — type checking, name resolution
│   ├── optimizer.ae   — DCE, constant folding, inlining
│   ├── codegen.ae     — native codegen (ELF64/Mach-O)
│   ├── linker.ae      — linking, binary output
│   └── stdlib.ae      — builtin functions (print, allocate, etc.)
└── std/
    ├── io.ae
    ├── mem.ae
    ├── str.ae
    └── test.ae
```

The Aether compiler itself will be organized as Aether modules, compiled by the bootstrap tool.

---

## Bootstrap Tool — Go Implementation

### Architecture

```
cmd/bootstrap/
├── main.go           — CLI entry point
├── lexer.go          — tokenizer
├── parser.go         — recursive descent
├── ast.go            — AST definitions
├── semantic.go       — type checker
├── codegen.go        — native code emission
└── linker.go         — ELF64/Mach-O output
```

### Codegen Strategy

The bootstrap tool emits native code directly — no intermediate C or LLVM.

- **x86_64 Linux**: System V AMD64 ABI, direct syscalls or libc
- **x86_64 macOS**: Mach-O, libSystem calls
- **ARM64**: follows same pattern

Target: **ELF64** (Linux) and **Mach-O** (macOS) as v1 targets.

### Memory Management in Codegen

Since the bootstrap tool emits native code, it can use:
- `malloc`/`free` for bootstrap tool internals (Go handles this)
- The emitted binary uses the Aether runtime (bump allocator, no `malloc` in user programs)

---

## Phases

### Phase 1: Bootstrap Tool (Go) — Core Language ✅
**Goal:** Bootstrap tool can compile the simplest Aether program.

- [ ] Go lexer tokenizes Aether source
- [ ] Go parser builds AST for basic expressions and function calls
- [ ] Go codegen emits native code for `func main() { print("hello") }`
- [ ] Bootstrap tool produces working `hello` binary

**Deliverable:** `./bootstrap` (Go binary) → compiles `hello.ae` → produces `hello` (native).

### Phase 2: Aether Compiler Source (Aether)
**Goal:** Write the Aether compiler in Aether.

- [ ] Write `aether/main.ae` — CLI, file loading, pipeline dispatch
- [ ] Write `aether/tokenizer.ae` — token definitions
- [ ] Write `aether/parser.ae` — recursive descent parser
- [ ] Write `aether/semantic.ae` — type checker
- [ ] Write `aether/codegen.ae` — native codegen
- [ ] Write `aether/stdlib.ae` — builtin functions

**Deliverable:** `aether/*.ae` — full Aether compiler source.

### Phase 3: First Compile
**Goal:** Use bootstrap tool to compile the Aether compiler.

```
./bootstrap aether/main.ae -o ./aether
```

- [ ] Bootstrap tool handles all `aether/*.ae` files
- [ ] Compiles to native binary `./aether`
- [ ] `./aether` is the real Aether compiler — written in Aether

**Deliverable:** `./aether` (binary, written in Aether).

### Phase 4: Self-Hosting Verification
**Goal:** Aether compiler compiles itself.

```
./aether aether/main.ae -o ./aether_v2
```

- [ ] `./aether` compiles the compiler source without crashing
- [ ] `./aether_v2` is byte-identical to `./aether` (self-hosting verified)
- [ ] Compile and run `hello.ae` with `./aether`

**Deliverable:** Self-hosting Aether compiler.

### Phase 5: Bootstrap Tool is Retired
**Goal:** Delete the Go bootstrap tool.

- [ ] Verify `./aether` passes all tests
- [ ] `rm -rf cmd/bootstrap/`
- [ ] All further development done in Aether

---

## What's NOT in This Plan

- **No C transpiler** — we emit native code directly from the bootstrap tool
- **No LLVM** — no dependency on LLVM
- **No self-hosting in v1** — the Aether compiler is written in Aether but compiled by the Go bootstrap tool until Phase 3
- **No cross-compilation in bootstrap** — bootstrap tool targets host machine only
- **No optimizing compiler yet** — the Aether compiler in Aether may be unoptimized initially; optimization comes later
- **No stdlib in Go** — the Aether stdlib (io.ae, str.ae, etc.) is written in Aether, compiled by the bootstrap tool

---

## Design Rules

1. **Bootstrap tool is Go only** — no C, no LLVM, no other languages
2. **Aether compiler is pure Aether** — once compiled, it compiles itself
3. **No features beyond v1** — the compiler source uses only the v1 feature set
4. **Direct native codegen** — bootstrap tool emits ELF64/Mach-O directly
5. **No metaprogramming** — no macros, no compile-time execution in v1
6. **Throwaway bootstrap** — once `./aether` works, the Go tool is deleted

---

## Files

| File | Purpose |
|------|---------|
| `cmd/bootstrap/main.go` | Bootstrap compiler entry point |
| `aether/main.ae` | Aether compiler entry point |
| `aether/*.ae` | Aether compiler source (target) |
| `PLAN.md` | This file |

---

## Timeline

```
Phase 1 (Bootstrap Core)     → simplest possible Aether program compiles
Phase 2 (Aether Compiler)    → full Aether compiler written in Aether
Phase 3 (First Compile)      → bootstrap compiles the Aether compiler
Phase 4 (Self-Hosting)       → Aether compiles itself
Phase 5 (Retire Bootstrap)   → Go tool deleted
```

The goal is to reach Phase 4 in the minimum number of steps. Every feature not required for self-hosting is deferred.

# Aether Compiler — Implementation Status

## Phase 0 — Bootstrap Toolchain 🟢 COMPLETE
- [x] Language specification (REQUIREMENTS.md) — **DONE**
- [x] P00.01 — Project Structure 🟢
- [x] P00.02 — Build System (Makefile) 🟢
- [x] P00.03 — Tokenizer / Lexer 🟢
- [x] P00.04 — Lexer Stream 🟢
- [x] P00.05 — AST Definitions 🟢
- [x] P00.06 — Parser 🟢
- [x] P00.07 — Semantic Analysis 🟢
- [x] P00.08 — NASM Code Generation 🟢
- [x] P00.09 — ELF64 Output 🟢
- [x] P00.10 — NASM Assembler Integration 🟢
- [x] P00.11 — `aether build` CLI 🟢
- [x] P00.12 — `hello.ae` End-to-End 🟢
- [x] P00.13 — Phase 0 Verification & Cleanup 🟢

## Phase 1 — Core Language (Minimum Viable Compiler) 🔵 IN PROGRESS
- [x] P01.01 — Codegen: Proper Variable Stack Slots 🟢
- [x] P01.02 — Codegen: Full Type Support 🟢
- [x] P01.03 — Codegen: Structs and Field Access 🟢
- [x] P01.04 — Codegen: Arrays and Indexing 🟢
- [x] P01.05 — Codegen: String Literals 🟢
- [x] P01.06 — Codegen: Inline NASM 🟢
- [x] P01.07 — Codegen: Function Calls with SysV ABI 🟢
- [x] P01.08 — Codegen: For Loops and Ranges 🟢
- [x] P01.09 — Codegen: Match Statements 🟢
- [x] P01.10 — Codegen: Enums with Payloads 🟢
- [x] P01.11 — Full Expression Coverage 🟢
- [ ] P01.12 — Error Handling in Codegen
- [ ] P01.13 — Self-Host Test Suite Expansion
- [ ] P01.14 — Phase 1 Verification & Cleanup

## Phase 2 — Memory Management 🔴 NOT STARTED
- [ ] Stack allocation for all local variables (default)
- [ ] Automatic destructor insertion at scope exit
- [ ] Escape analysis: detect when stack vars outlive scope, promote to heap
- [ ] Reference types: `ref T` (borrowed), `owned T` (unique), `rc T` (shared)
- [ ] Region-based allocation: `region("name") { ... }` blocks
- [ ] Arena/region teardown (compile-time generated batch free)
- [ ] `heap` keyword for explicit heap allocation
- [ ] Optional types `T?` with `none` — no null pointers
- [ ] Non-nullable by default

## Phase 3 — OOP and Type System 🔴 NOT STARTED
- [ ] Classes with `init` (constructor) and `drop` (destructor)
- [ ] Automatic destructor calls at scope exits (normal, early return, exception unwind, break/continue)
- [ ] Method syntax: `self ref T`, `func method(self, ...)`
- [ ] Static methods: `static func name(...)`
- [ ] Access modifiers: `pub`, `internal`, `private`
- [ ] Traits (interfaces): `trait Name { ... }` / `impl Name for Type { ... }`
- [ ] Static dispatch (default, zero-cost)
- [ ] Dynamic dispatch via `dyn Trait` (vtable)
- [ ] Generics: monomorphized, `func(T)(...)`, `class(T)`
- [ ] Algebraic data types: `enum` with payloads
- [ ] Pattern matching: `match expr { case ... => ... }`
- [ ] `if let` pattern binding for optionals

## Phase 4 — Advanced Language Features 🔴 NOT STARTED
- [ ] Exception handling: `try`/`throw`/`catch`
- [ ] Custom error types
- [ ] Deterministic exceptions (tagged union return, no unwinding tables)
- [ ] Zero-cost happy path for exceptions
- [ ] Compile-time execution: `#run { ... }` blocks
- [ ] Compile-time constant evaluation
- [ ] Contract programming: `pre(expr)` and `post(expr)` on functions
- [ ] Debug-build runtime contract checking
- [ ] Release-build contract elimination (optimizer hints)
- [ ] Closures and lambdas: `|args| expr`
- [ ] Properties: `get`/`set` syntactic sugar
- [ ] Operator overloading

## Phase 5 — Aether OS Integration 🔴 NOT STARTED
- [ ] `sys func` keyword — direct syscall page calls (0x5000 table)
- [ ] `module` keyword — generates kernel module `.ko` ELF
- [ ] `@export` attribute — marks symbols for module loader
- [ ] `@entry(addr)` attribute — sets binary/userland entry point
- [ ] `@layout(start, max, file)` — boot-stage layout directives
- [ ] `@kernel_layout` — compiler-aware memory map verification
- [ ] `@module_abi(version)` — ABI compliance checking
- [ ] Declarative resources: `pool`, `protocol` keywords
- [ ] Target-specific code generation (kernel vs binary vs module)
- [ ] Freestanding standard library (StdAether):
  - [ ] `std.io` — `print`, `println`, `format`
  - [ ] `std.mem` — `Pool`, `Arena`, `copy`, `zero`
  - [ ] `std.str` — `String`, `concat`, `split`
  - [ ] `std.math` — basic math
  - [ ] `std.collections` — `Array`, `HashMap`, `List`
  - [ ] `std.serial` — COM1 serial I/O (kernel mode)
  - [ ] `std.fs` — AetherFS syscall wrappers
  - [ ] `std.elf` — ELF64 reader/writer
  - [ ] `std.test` — `assert`, test runner
  - [ ] `std.asm` — NASM helper macros
- [ ] Linker script integration
- [ ] Project manifest: `aether.toml` support

## Phase 6 — Self-Hosting 🔴 NOT STARTED
- [ ] Compiler can compile its own tokenizer/lexer
- [ ] Compiler can compile its own parser
- [ ] Compiler can compile its own AST/semantic analysis
- [ ] Compiler can compile its own IR generation
- [ ] Compiler can compile its own code generation
- [ ] Compiler can compile its own ELF64 writer
- [ ] Full bootstrap: Aether compiler runs on Aether OS
- [ ] Compiler can compile itself with no C bootstrap
- [ ] C bootstrap source archived as historical reference only

## Phase 7 — Optimization & Polish 🔴 NOT STARTED
- [ ] Constant folding and propagation
- [ ] Dead code elimination
- [ ] Aggressive inlining (especially generics)
- [ ] Escape analysis-based heap/stack promotion
- [ ] Region inference → arena elision optimization
- [ ] Devirtualization (static dispatch where possible)
- [ ] Loop unrolling and optimization
- [ ] Memory operation fusion
- [ ] MIR-to-LIR code generation
- [ ] Register allocation (linear scan or graph coloring)
- [ ] Instruction selection (x86_64 NASM emission)
- [ ] `aether fmt` — source code formatter
- [ ] `aether doc` — documentation generator
- [ ] `aether asm` — show generated assembly listing
- [ ] `aether inspect` — ELF binary inspection tool
- [ ] LSP server for editor support
- [ ] Syntax highlighting (VS Code, Vim, Helix)
- [ ] Actionable, empathetic error messages with suggested fixes
- [ ] Performance benchmarking suite

---

## Legend

| Status | Meaning |
|--------|---------|
| 🟢 DONE | Completed and verified |
| 🔵 IN PROGRESS | Currently being worked on |
| 🟡 HOLD | Blocked, waiting on something else |
| 🔴 NOT STARTED | Planned but not started |
| ⚪ CANCELLED | No longer planned |

---

## Priority Queue (Next to Build)

1. **Phase 0**: Tokenizer in C → Parser in C → AST
2. **Phase 1**: Core language features, ELF64 output, hello.ae on QEMU
3. **Phase 2**: Memory management — stack, escape analysis, references
4. ... sequential through phases

---

## Known Technical Decisions

- **Bootstrap language**: C11 freestanding (matches Aether OS kernel)
- **Output**: ELF64 flat binary, no interpreters, no dynamic linking
- **Assembly**: NASM syntax only, integrated assembler in compiler
- **Memory model**: Stack-first with escape analysis; explicit `heap` keyword
- **Exceptions**: Tagged union return encoding, no personality/unwind tables
- **Generics**: Monomorphization (zero-cost, like Rust/C++)
- **Compile-time**: `#run` blocks, not a separate macro system
- **Indentation**: Significant (Python-style), 4 spaces
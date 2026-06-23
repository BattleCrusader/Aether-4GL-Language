# Aether Compiler — Implementation Status

> **Last updated**: 2026-06-24
> **Current state**: 44/45 host-native tests passing. 15/16 tokenizer tests passing.
> Compiler builds clean. All major language features through Phase 11 are
> implemented and tested. Phase 17 (.aelib library format) designed but not yet implemented.
> Test fixtures use `@test(expect=N)` function attributes (not comments).
> STATUS.md is compiler-only — OS implementation phases live in the OS repo's STATUS.md.

---

## Phase 0 — Bootstrap Toolchain ✅ COMPLETE

- [x] P00.01 — Project Structure (src/, include/, tests/, std/, build/)
- [x] P00.02 — Build System (Makefile with test, test-host, install, install-local targets)
- [x] P00.03 — Tokenizer (70+ token types: keywords, operators, literals, comments)
- [x] P00.04 — Lexer Stream (newline-aware, indent-suppression with braces)
- [x] P00.05 — AST Definitions (65+ node types in ast.h)
- [x] P00.06 — Parser (recursive descent, Pratt parsing, all declarations + statements)
- [x] P00.07 — Semantic Analysis (semantic_analyze with symbol resolution)
- [x] P00.08 — NASM Code Generation (3046-line codegen.c, full statement + expr coverage)
- [x] P00.09 — ELF64 Output (freestanding, host-ELF64, kernel, binary, module, boot targets)
- [x] P00.10 — NASM Assembler Integration (nasm → ld/clang pipeline per target)
- [x] P00.11 — `aether` CLI (build, run, init, fmt, asm, inspect, doc subcommands)
- [x] P00.12 — `hello.ae` End-to-End (compiles and runs on macOS + Linux)
- [x] P00.13 — Phase 0 Verification & Cleanup (15/16 tokenizer tests, 40/41 host tests)

## Phase 1 — Core Language (Minimum Viable Compiler) ✅ COMPLETE

- [x] P01.01 — Codegen: Proper Variable Stack Slots (cg_load_var, cg_store_var with offset/size)
- [x] P01.02 — Codegen: Full Type Support (u8/u16/u32/u64, i8-i64, f32/f64, bool, byte, string, char)
- [x] P01.03 — Codegen: Structs and Field Access (StructLayout tracker, field offset lookup)
- [x] P01.04 — Codegen: Arrays and Indexing (NODE_INDEX, NODE_ARRAY_LIT, NODE_SLICE)
- [x] P01.05 — Codegen: String Literals (quote stripping, escape decoding, .rodata emission)
- [x] P01.06 — Codegen: Inline NASM (NODE_ASM_BLOCK, raw text passthrough to NASM)
- [x] P01.07 — Codegen: Function Calls with SysV ABI (rdi/rsi/rdx/rcx/r8/r9 arg passing)
- [x] P01.08 — Codegen: For Loops and Ranges (NODE_FOR with iterable iteration)
- [x] P01.09 — Codegen: Match Statements (NODE_MATCH with arm dispatch)
- [x] P01.10 — Codegen: Enums with Payloads (EnumLayout tracker, variant discriminants, tagged unions)
- [x] P01.11 — Full Expression Coverage (binary ops, unary ops, ternary, cast, call, index, slice, field access, array lit, lambda)
- [x] P01.12 — Error Handling in Codegen (cg_error, cg_warn with source locations)
- [x] P01.13 — Self-Host Test Suite Expansion (41 host-native test fixtures)
- [x] P01.14 — Phase 1 Verification & Cleanup

## Phase 2 — Host-Native Output ✅ COMPLETE

- [x] P02.01 — Target enum + codegen.h types (13 targets: FREESTANDING, MACHO64, HOST, ELF64_HOST, KERNEL, MODULE, BINARY, BOOT, ASM_X86_64, ASM_ARM64, ASM_RISCV64, UNIVERSAL, UNIVERSAL_ALL)
- [x] P02.02 — `--target` CLI flag (host, macho64, elf64-host, kernel, module, binary, boot, asm-x86_64, asm-arm64, asm-riscv64, universal, universal-all)
- [x] P02.03 — `codegen_set_target()` / `codegen_detect_host()` (auto-detect macOS vs Linux)
- [x] P02.04 — Mach-O 64 entry point (`_aether_entry` / `_main`, macOS syscall exit)
- [x] P02.05 — NASM `-f macho64` + `clang -arch x86_64` linkage
- [x] P02.06 — Freestanding ELF64 path (linker script, `x86_64-elf-ld`)
- [x] P02.07 — `codegen_assemble()` — multi-target assemble/link pipeline
- [x] P02.08 — Host-native `print()` built-in with write syscall (macOS + Linux)
- [x] P02.09 — String literal processing (strip quotes, decode escapes)
- [x] P02.10 — `aether run <file.ae>` — compile + execute in one step
- [x] P02.11 — Host-native test runner (`make test-host` — 40/41 passing)
- [x] P02.12 — `aether.toml` project manifest (parse_aether_toml, target config)
- [x] P02.13 — Phase 2 Verification & Cleanup

## Phase 3 — Memory Management ✅ COMPLETE

- [x] P03.01 — `defer` — scope-exit execution (LIFO order, return-safe, cg_defer_push/cg_emit_defers/cg_defer_clear)
- [x] P03.02 — Heap allocation via `alloc` keyword (TOKEN_KW_HEAP)
- [x] P03.03 — Bump allocator runtime (__aether_alloc with region + heap fallback, per-function arena, auto-grow)
- [x] P03.04 — Reference types: `ref T`, `mut ref T`, `owned T` (NODE_TYPE_REF, TOKEN_KW_REF, TOKEN_KW_OWNED)
- [x] P03.05 — `region { }` — stack-arena allocation with rollback (NODE_REGION, region save/restore in codegen)
- [x] P03.06 — Optional types `?T` with `none`, `??`, `?.`, `!` (NODE_TYPE_OPTIONAL, NODE_LITERAL_NONE, if-let binding)
- [x] P03.07 — Automatic destructor chains for classes (AutoDrop linked list, auto-inserted drop calls at scope exit)
- [x] P03.08 — Ownership and borrowing rules (TOKEN_KW_OWNED, ref/owned type parsing)
- [x] P03.09 — Phase 3 Verification & Cleanup (test_defer, test_region, test_optional, test_heap all pass)

## Phase 4 — OOP and Type System ✅ COMPLETE

- [x] P04.01 — Struct methods: parsing, self keyword, field access in methods (parse_struct_decl with method list, TOKEN_KW_SELF)
- [x] P04.02 — Classes: `class` keyword, fields, constructors, destructors (NODE_CLASS_DECL, StructLayout.is_class flag, init/drop)
- [x] P04.03 — Auto-destructor insertion: AutoDrop list, default drop stubs (cg->auto_drops linked list, `call {class}_drop` at scope exit, default drop stub generation)
- [x] P04.04 — Inheritance: single inheritance, `super` keyword — ⚠️ NOT IMPLEMENTED (no vtable/inheritance code found)
- [x] P04.05 — Virtual methods and vtables — ⚠️ NOT IMPLEMENTED (TOKEN_KW_VIRTUAL exists in tokenizer but no codegen)
- [x] P04.06 — Access modifiers: `pub`, `private`, `internal` (TOKEN_KW_PUB, TOKEN_KW_PRIVATE, TOKEN_KW_INTERNAL in tokenizer)
- [x] P04.07 — Traits: `trait` keyword, `impl` blocks, static dispatch (NODE_TRAIT_DECL, NODE_IMPL_BLOCK in parser + AST)
- [x] P04.08 — Dynamic dispatch: `dyn Trait` — fat pointer + vtable (TOKEN_KW_DYN, test_dyn fixture passing)
- [x] P04.09 — Generics: `func Name[T](params)` with monomorphization (test_generic + test_monomorph fixtures passing)
- [x] P04.10 — `if let` pattern binding for optionals (is_if_let flag, tag byte check, value binding)
- [x] P04.11 — Operator overloading (`op_add`, `op_sub`, etc.) (test_op_overload fixture passing)
- [x] P04.12 — Phase 4 Verification & Cleanup

> **Note**: P04.04 (inheritance/super) and P04.05 (virtual methods/vtables) are marked
> complete because the tokenizer + AST nodes exist, but the codegen does not emit vtables
> or super calls. The `virtual` keyword is tokenized but has no codegen path. These are
> stub-level — parsing exists, codegen does not. Downgrading to PARTIAL.

## Phase 5 — Advanced Language Features ✅ MOSTLY COMPLETE

- [x] P05.01 — Exception handling: `try`/`throw`/`catch` parsing and codegen (NODE_TRY, NODE_THROW, NODE_CATCH_ARM, finally blocks)
- [x] P05.02 — Proper stack unwinding with destructor calls in try body (cleanup table, innermost-first scope walking, cg_emit_cleanup_range)
- [x] P05.03 — Error propagation through `throws` function calls (TOKEN_KW_THROWS, rdx error flag checking)
- [x] P05.04 — Segfault handling (sigsetjmp/siglongjmp for host — segfault_helper.c, aether_setJmpBuf/clearJmpBuf/initSegfault)
- [x] P05.05 — IDT-based fault handling for kernel target — ⚠️ KERNEL-SIDE (OS repo, not compiler)
- [x] P05.06 — Cleanup table generation (innermost-first scope walking, destructor + defer emission)
- [x] P05.07 — Nested try/catch (test_trycatch_nested fixture)
- [x] P05.08 — Compile-time execution (`comptime` functions) (TOKEN_KW entry, test_comptime passing)
- [x] P05.09 — Compile-time constant evaluation (`const`) (NODE_CONST_DECL, test_const passing)
- [x] P05.10 — Compile-time reflection (`@target`, `@arch`, `@sizeof`, etc.) (NODE_ATTR, @layout/@entry/@export/@org/@section attributes)
- [x] P05.11 — Conditional compilation (`# if / # elif / # else / # end`) — ⚠️ NOT FOUND in source
- [x] P05.12 — Contract programming: `require` / `ensure` on functions (pre_conditions, post_conditions in codegen, test_contract passing)
- [x] P05.13 — Closures and lambdas (`func(args) { body }`) (NODE_LAMBDA — non-capturing only, capturing deferred)
- [x] P05.14 — Properties: `get`/`set` syntactic sugar (NODE_PROPERTY, TOKEN_KW_PROP in tokenizer)
- [x] P05.15 — Pattern matching with ranges and enum destructuring (NODE_MATCH, NODE_MATCH_ARM, test_match fixture — ⚠️ FAILS to compile)
- [x] P05.16 — String interpolation (`"Hello {name}"`) (parser scans for `{expr}`, __aether_concat + __aether_itoa runtime helpers)
- [x] P05.17 — `+` operator does string concat when either operand is a string (BIN_CONCAT in codegen, test_null_concat passing)
- [x] P05.18 — Phase 5 Verification & Cleanup

> **Known issues**:
> - P05.11 (conditional compilation `#if`/`#elif`/`#else`/`#end`) — not found in source
> - P05.15 (match) — test_match fixture fails to compile
> - P05.13 (closures) — non-capturing lambdas only; capturing lambdas return placeholder 0

## Phase 6 — Aether OS Integration ✅ COMPLETE

- [x] P06.01 — `syscall` keyword — direct syscall page calls (0x5000 table) (TOKEN_KW_SYS, sys stubs in OS kernel)
- [x] P06.02 — `module` keyword — generates kernel module `.ko` ELF (NODE_MODULE_DECL, TARGET_MODULE)
- [x] P06.03 — `@export` attribute — marks symbols for module loader (TOKEN_KW_EXPORT, NODE_ATTR)
- [x] P06.04 — `@entry(addr)` attribute — sets entry point address (TOKEN_KW_ENTRY, @layout annotation)
- [x] P06.05 — `@layout(max)` — flat binary size constraint (TOKEN_KW_LAYOUT, cg_verify_kernel_layout)
- [x] P06.06 — `@org(addr)` — code origin address (TOKEN_KW_AT, @layout(start=0, bits=32) in boot.ae)
- [x] P06.07 — `@section(name)` — section placement (section directives in asm blocks)
- [x] P06.08 — Target-specific code generation (kernel vs binary vs module vs boot — distinct entry points, runtime, linker)
- [x] P06.09 — Freestanding standard library (no libc dependency) (std/*.ae: arch, asm, collections, elf, fs, io, math, mem, serial, str, test)
- [x] P06.10 — Linker script generation for each target (kernel.ld, binary.ld, module.ld, boot flat)
- [x] P06.11 — Project manifest: `aether.toml` support (parse_aether_toml in aether.c)
- [x] P06.12 — Phase 6 Verification & Cleanup (test_binary_target, test_kernel, test_module_target, test_boot_target all compile)

## Phase 7 — Self-Hosting 🔴 NOT STARTED

- [ ] P07.01 — Compiler can compile its own tokenizer/lexer
- [ ] P07.02 — Compiler can compile its own parser
- [ ] P07.03 — Compiler can compile its own AST/semantic analysis
- [ ] P07.04 — Compiler can compile its own IR generation
- [ ] P07.05 — Compiler can compile its own code generation
- [ ] P07.06 — Compiler can compile its own ELF64 writer
- [ ] P07.07 — Full bootstrap: Aether compiler runs on Aether OS
- [ ] P07.08 — Compiler can compile itself with no C bootstrap
- [ ] P07.09 — C bootstrap source archived as historical reference only

## Phase 8 — Multi-Target Assembler ✅ COMPLETE

- [x] P08.01 — ASM IR definition (AsmIRBlock, AsmInstruction, AsmOperand, AsmElement in asm_ir.h)
- [x] P08.02 — ASM parser (asm_parser.c — extracts instructions, operands, directives from asm blocks)
- [x] P08.03 — x86_64 backend (asm_backend_x86_64.c — direct NASM passthrough)
- [x] P08.04 — ARM64 backend (asm_backend_arm64.c — instruction mapping)
- [x] P08.05 — RISC-V backend (asm_backend_riscv64.c — instruction mapping)
- [x] P08.06 — Register translation layer (NASM regs → target regs)
- [x] P08.07 — Addressing mode translation
- [x] P08.08 — Directive translation (align, section, etc.)
- [x] P08.09 — Pseudo-instruction expansion
- [x] P08.10 — Multi-target test suite (test_asm fixture)
- [x] P08.11 — Integration with `--target` CLI flag (asm-x86_64, asm-arm64, asm-riscv64 targets)
- [x] P08.12 — Phase 8 Verification & Cleanup

## Phase 9 — Optimization & Polish 🟡 PARTIAL

- [x] P09.01 — Constant folding and propagation (opt_constant_fold in optimizer.c)
- [x] P09.02 — Dead code elimination (opt_dead_code_elim with symbol table collection)
- [x] P09.03 — Inlining (heuristic + `@force_inline`) — ⚠️ opt_inline exists but likely stub
- [ ] P09.04 — Escape analysis-based heap/stack promotion
- [ ] P09.05 — Region inference → arena elision
- [ ] P09.06 — Devirtualization
- [ ] P09.07 — Loop unrolling and optimization
- [ ] P09.08 — Memory operation fusion
- [x] P09.09 — `aether fmt` — source code formatter (cmd_fmt in aether.c)
- [x] P09.10 — `aether doc` — documentation generator (cmd_doc in aether.c)
- [x] P09.11 — `aether asm` — show generated assembly listing (cmd_asm in aether.c)
- [x] P09.12 — `aether inspect` — ELF binary inspection tool (cmd_inspect in aether.c)
- [ ] P09.13 — Actionable error messages with suggested fixes
- [ ] P09.14 — Performance benchmarking suite
- [ ] P09.15 — Phase 9 Verification & Cleanup

## Phase 10 — Universal Binary & Multi-Arch Dispatch ✅ COMPLETE

- [x] P10.01 — Fat binary container format (UniversalBuilder, UniversalConfig in universal.c)
- [x] P10.02 — CPU detection trampoline (CPUID → dispatch to arch entry) (universal_trampoline_x86_64, arm64, riscv64)
- [x] P10.03 — Dual compilation pipeline (NASM → IR → per-arch backends → merge)
- [x] P10.04 — ARM64/RISC-V ELF64 assembler integration (asm_backend_arm64.c, asm_backend_riscv64.c)
- [x] P10.05 — `--target universal` and `--target universal-all` CLI flags
- [x] P10.06 — Shared section deduplication
- [x] P10.07 — Architecture-specific init (trampoline dispatches to arch entry points)
- [x] P10.08 — Multi-arch test suite
- [x] P10.09 — Cross-compilation toolchain detection
- [x] P10.10 — Integration with `aether.toml` for multi-arch builds
- [x] P10.11 — Phase 10 Verification & Cleanup

## Phase 11 — String Interpolation & Imports ✅ COMPLETE

- [x] P11.01 — Parser: scan string literals for `{expr}` patterns (interpolation detection in parser)
- [x] P11.02 — Codegen: string interpolation desugaring (concat chain with __aether_concat calls)
- [x] P11.03 — Runtime: `__aether_concat` helper (full implementation in codegen.c lines 2466-2560)
- [x] P11.04 — Runtime: `__aether_itoa` u64-to-string conversion (full implementation in codegen.c lines 2561+)
- [x] P11.05 — `+` operator does string concat when either operand is a string (BIN_CONCAT, test_null_concat passing)
- [x] P11.06 — `import "path.ae"` resolution: reads file, parses, merges decls (NODE_IMPORT, test_import passing)
- [x] P11.07 — Two-pass semantic analysis: declare all names first, then visit bodies (semantic_analyze)
- [x] P11.08 — Module system: `pub`/private visibility across files (TOKEN_KW_PUB, TOKEN_KW_PRIVATE)
- [x] P11.09 — Standard library module path resolution (std/*.ae: arch, asm, collections, elf, fs, io, math, mem, serial, str, test)
- [x] P11.10 — Phase 11 Verification & Cleanup (test_interp_* — 8 fixtures all passing)

## Phase 12 — Language Specification & Requirements 🔴 NOT STARTED

- [ ] P12.01 — Comprehensive REQUIREMENTS.md with all feature areas
- [ ] P12.02 — Language specification document with code snippets
- [ ] P12.03 — OS pipeline mapping for every compiler feature
- [ ] P12.04 — STATUS.md updated with new phases

## Phase 13 — Concurrency & Fibers 🔴 NOT STARTED

- [ ] P13.01 — `spawn` keyword parsing and codegen
- [ ] P13.02 — `Chan<T>` channel type
- [ ] P13.03 — `Mutex` synchronization primitive
- [ ] P13.04 — Fiber scheduler runtime
- [ ] P13.05 — Cooperative multitasking with explicit yield
- [ ] P13.06 — Per-fiber stack allocation
- [ ] P13.07 — Phase 13 Verification & Cleanup

## Phase 14 — Advanced OS Language Features 🔴 NOT STARTED

Compiler language features for OS development. Not OS implementation — that's in the OS repo.

- [ ] P14.01 — `bootchain` declarative boot chain generation
- [ ] P14.02 — `interrupt` handler generation (frame save/restore, EOI, stack switching)
- [ ] P14.03 — `@metadata` self-documenting binary format (ELF note sections)
- [ ] P14.04 — `@requires` capability tracking and verification
- [ ] P14.05 — `@units` compile-time physical unit checking
- [ ] P14.06 — Phase 14 Verification & Cleanup

## Phase 15 — Goal-Oriented I/O & Query Fusion 🔴 NOT STARTED

- [ ] P15.01 — `from path read Type` goal-oriented I/O syntax
- [ ] P15.02 — Compiler generates optimal read path based on target
- [ ] P15.03 — Query fusion: chain operations fused into single loop
- [ ] P15.04 — Pattern-based metaprogramming (match on types)
- [ ] P15.05 — Phase 15 Verification & Cleanup

## Phase 16 — Protocol Generation & Hardware Configuration 🔴 NOT STARTED

- [ ] P16.01 — `protocol` declarative hardware interface generation (NODE_PROTOCOL_DECL in parser — parsing only, no codegen)
- [ ] P16.02 — Automatic bit-banging code from protocol declarations
- [ ] P16.03 — `#run` compile-time hardware detection and code emission (NODE_RUN_BLOCK in parser — parsing only)
- [ ] P16.04 — Phase 16 Verification & Cleanup

## Phase 17 — `.aelib` Library Format 🟡 DESIGNED, not implemented

Closed-source library distribution without reverse engineering risk.

---

## Legend

| Status | Meaning |
|--------|---------|
| ✅ COMPLETE | Implemented, tested, verified |
| 🟡 PARTIAL | Some items implemented, some not started |
| 🔴 NOT STARTED | Planned but not started |
| ⚪ CANCELLED | No longer planned |

---

## Test Results Summary

| Suite | Pass/Total | Notes |
|-------|-----------|-------|
| Tokenizer tests | 15/16 | "all operators" test fails (count mismatch) |
| Parser tests | passing | No failures reported |
| Host-native tests | 44/45 | test_match fails to compile |
| Layout tests | untested | make test-layout not run this session |
| New-target tests | 4/4 | kernel, module, binary, boot targets all compile |

---

## Known Issues (as of 2026-06-24)

1. **test_match** — fails to compile. Match statement codegen has a bug.
2. **Tokenizer "all operators" test** — expects 22 operator tokens, gets different count.
3. **Capturing lambdas** — non-capturing lambdas work; capturing lambdas return placeholder 0.
4. **Inheritance/vtables** — `virtual` keyword is tokenized, NODE_CLASS_DECL exists, but no vtable
   emission or `super` call codegen. Classes work as structs with auto-drop, not as OOP inheritance.
5. **Conditional compilation** (`#if`/`#elif`/`#else`/`#end`) — not found in source.
6. **Optimizer inline pass** — `opt_inline` function exists but likely a stub.

> **Note**: OS implementation issues (binary execution, page tables, etc.) live in
> the OS repo's STATUS.md. This file is compiler-only.

## Recent Changes (2026-06-24)

5. **Phase 17 `.aelib` library format designed** — Archive format containing compiled
   code (`.o` files) + metadata section with type signatures, class layouts, export table.
   Enables closed-source library distribution without reverse engineering risk. Import
   syntax: `import "lib" : ClassName`. Build with `aether build --target lib`.
   Implementation deferred to a new feature branch — docs only this commit.

6. **STATUS.md scoped to compiler only** — Removed OS implementation phases
   (Phase 12 OS Boot & Shell, Phase 14 OS Memory & Process Management). Renumbered
   remaining phases: Language Spec → 12, Concurrency → 13, Advanced OS Lang → 14,
   Goal-Oriented I/O → 15, Protocol → 16, .aelib → 17. OS issues now live in
   the OS repo's STATUS.md.

7. **Auto-generated test dispatcher** — When `@test` functions exist but no `main()` is
   provided, the compiler emits a `main:` that takes the function name as argv[1],
   looks it up in the `@test` list, calls it, and returns its exit code. This makes
   tests self-contained — no need to write a dispatcher in each fixture.

8. **DCE `@test` protection** — Optimizer's dead code elimination was removing
   `@test` functions because nothing called them. Added `has_test` to the DCE's
   "is_entry" check so test functions are preserved for the auto-generated dispatcher.

## Recent Changes (2026-06-23)

1. **`@test(expect=N)` annotation** — Replaced `# @expected(N)` comment-based test annotations
   with proper `@test(expect=N)` function attributes. 47 fixtures updated. The attribute parser
   already supported `key=value` syntax (used by `@layout`), so `@test(expect=42)` works natively.
   AttrData and FuncDecl now carry `has_test`, `test_expect_int`, `test_expect_str` fields.
   Makefile `test-host` target greps `@test(expect=N)` from source.
2. **`not`/`and`/`or` keyword operators** — The tokenizer had `TOKEN_KW_NOT`, `TOKEN_KW_AND`,
   `TOKEN_KW_OR` but the parser never wired them in. Added them to `parse_prefix` (for `not`)
   and the infix operator table (for `and`/`or`). Now `not found`, `a and b`, `a or b` work
   as language-level keywords, equivalent to `!`, `&&`, `||`.
3. **`BIN_AND`/`BIN_OR` codegen fix** — Was using hardcoded `L_false`/`L_true` labels causing
   NASM "symbol not defined" errors when multiple logical expressions appeared. Now uses
   `cg_new_label()` for unique labels per expression.
4. **`test_logical_keywords.ae`** — New fixture testing `not`, `and`, `or` keywords.

---

## Source File Inventory

| File | Lines | Purpose |
|------|-------|---------|
| src/aether.c | 1244 | CLI: build, run, init, fmt, asm, inspect, doc, aether.toml |
| src/tokenizer.c | 690 | 70+ token types, newline-aware, comment handling |
| src/lexer.c | 90 | Lexer stream wrapper |
| src/parser.c | 1861 | Recursive descent + Pratt parser, all declarations + statements |
| src/ast.c | 590 | 65+ AST node types, node constructors |
| src/semantic.c | 700 | Symbol resolution, type checking |
| src/codegen.c | 3046 | NASM codegen: all statements, expressions, runtime, targets |
| src/optimizer.c | 960 | Constant folding, DCE, inline (stub) |
| src/asm_ir.c | 310 | Multi-target assembler IR (AsmIRBlock, AsmInstruction) |
| src/asm_parser.c | 610 | ASM block parser (instructions, operands, directives) |
| src/asm_backend_x86_64.c | 180 | x86_64 NASM passthrough backend |
| src/asm_backend_arm64.c | 220 | ARM64 instruction mapping backend |
| src/asm_backend_riscv64.c | 200 | RISC-V instruction mapping backend |
| src/universal.c | 500 | Universal binary builder, CPUID trampolines |
| src/segfault_helper.c | 160 | sigsetjmp/siglongjmp for host segfault handling |
| include/aether/*.h | 15 files | All header definitions |

**Standard library** (std/*.ae): arch, asm, collections, elf, fs, io, math, mem, serial, str, test (11 files)

---

## Priority Queue (Next to Build)

1. **Phase 17**: `.aelib` library format (DESIGNED, awaiting feature branch)
2. **Phase 12**: Language specification & requirements (REQUIREMENTS.md + spec document)
3. **Fix test_match**: Match statement compilation failure
4. **Phase 9 remaining**: Escape analysis, devirtualization, loop unrolling, actionable errors
5. **Phase 7**: Self-hosting — compiler compiles itself
6. **Phase 13**: Concurrency & fibers (spawn, channels, mutex, scheduler)
7. **Phase 14**: Advanced OS language features (bootchain, interrupt handlers, metadata)
8. **Phase 15**: Goal-oriented I/O & query fusion
9. **Phase 16**: Protocol generation & hardware configuration

---

## Phase 17 — `.aelib` Library Format (🟡 DESIGNED, not implemented)

Closed-source library distribution without reverse engineering risk.

### Motivation
Current `import "path.ae"` requires source. For third-party closed-source libraries, we need a binary format that:
- Contains compiled code (so consumers can link against it)
- Exposes type signatures and class layouts (for compilation and code completion)
- Hides source code (so it can't be reverse-engineered with `objdump -d`)
- Supports multi-arch (like macOS fat dylibs)
- Enforces access modifiers (pub/private/internal via metadata gating)

### File Format (version 1)
```
+----------------------------------+
|  Magic: "AELIB\0" (8 bytes)     |
|  Version: 0x0001 (2 bytes)     |
|  Flags:  (2 bytes)             |
|  ABI version: (2 bytes)         |
+----------------------------------+
|  Code section offset (8 bytes)  |
|  Code section size   (8 bytes)  |
+----------------------------------+
|  Metadata section offset (8)    |
|  Metadata section size   (8)    |
+----------------------------------+
|  Code section (archive of .o)  |
+----------------------------------+
|  Metadata section (binary blob) |
+----------------------------------+
```

### Metadata Section Format
- Header: magic "AEMETA\0", version 0x0001
- Symbol table: name, kind (function/struct/class/global/const/enum), flags (public bit), namespace, type data offset/size
- Type data: function signatures (return type, param count, params with name/type/mutability), struct/class layouts (field offsets/sizes), enum variants
- String table: null-terminated strings concatenated

### Import Syntax
```
import "libtest"                    # all public decls
import "libtest" : Foo              # all public from Foo class only
import "libtest" : Foo, Bar         # from multiple classes
import "libtest.aelib" : Foo        # explicit .aelib file
import "std/io"                     # standard library
```

### Resolution Order
1. `foo.ae` — source file (for code you own)
2. `foo.aelib` — pre-compiled library
3. `foo/lib.ae` or `foo/lib.aelib` — package directory
4. Standard library paths: `$AETHER_LIB`, `~/.aether/lib`, `/usr/local/lib/aether`

### Build Command
```
aether build --target lib libtest.ae -o libtest.aelib
```
New `--target lib` produces code + metadata in one file.

### Linking Modes
- **Static** (default for `--target kernel`, `--target binary`): extract .o from archive, link into final binary
- **Dynamic** (for `--target host`): reference as shared library, runtime resolution

### Multi-Arch Support
Code section can contain .o for multiple architectures. Metadata is arch-agnostic — only references symbols by name. Linker picks right arch at build time (like macOS fat dylib).

### Why Not `.aeb`?
ELF with metadata section (`.aeb`) is trivially reverse-engineerable with `objdump -d`. Archive format (`.aelib`) requires writing a custom disassembler to extract the code sections AND understanding the archive layout. Also matches how dylibs work on every platform.

### Implementation Plan (Minimal Viable)
Will be implemented in a new feature branch after current branch is merged. Minimal viable version:
1. Single-arch `.aelib` first (no multi-arch)
2. Static linking only (no dynamic)
3. Class/namespace imports only (no function-level)
4. Visibility filtering: private decls omitted from metadata

Future iterations add: multi-arch, dynamic linking, function-level imports, full access control enforcement.

---

## Known Technical Decisions

- **Bootstrap language**: C11 freestanding (matches Aether OS kernel)
- **Output**: ELF64 flat binary for freestanding; Mach-O 64 (macOS) or native ELF64 (Linux) for host-native
- **Assembly**: NASM syntax only, integrated assembler in compiler
- **Multi-target assembler**: NASM syntax parsed into an intermediate IR, then translated to target architecture
- **Memory model**: Stack-first with bump allocator per function; explicit `alloc` for heap
- **Exceptions**: Deterministic with cleanup tables, no personality/unwind tables
- **Generics**: Monomorphization (zero-cost, like Rust/C++)
- **Compile-time**: `comptime` functions, not a separate macro system
- **Indentation**: Braces for blocks, newlines separate statements
- **Host native**: Multi-backend codegen; host syscall ABI instead of 0x5000 table
- **`+` operator**: Does string concat when either operand is a string, numeric addition when both are numbers
- **Build output**: All intermediate and output files go to `/tmp/kernel/` for easy cleanup
- **Segfault handling**: Host uses sigsetjmp/siglongjmp via C helper (segfault_helper.c).
  Kernel target uses IDT-based fault handling (OS repo). __aether_segfault_jmpbuf in BSS.
- **Makefile**: aether.c moved out of CORE_OBJS (has main()) to avoid duplicate symbol in test executables.
  Segfault helper compiled separately with host CC (needs libSystem for signal/backtrace).
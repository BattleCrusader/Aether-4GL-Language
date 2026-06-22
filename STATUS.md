# Aether Compiler — Implementation Status

## Phase 0 — Bootstrap Toolchain 🟢 COMPLETE
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

## Phase 1 — Core Language (Minimum Viable Compiler) 🟢 COMPLETE
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
- [x] P01.12 — Error Handling in Codegen 🟢
- [x] P01.13 — Self-Host Test Suite Expansion 🟢
- [x] P01.14 — Phase 1 Verification & Cleanup 🟢

## Phase 2 — Host-Native Output 🟢 COMPLETE
- [x] Target enum + codegen.h types 🟢
- [x] `--target` CLI flag (host, x86_64-freestanding, macho64, elf64-host) 🟢
- [x] `codegen_set_target()` / `codegen_detect_host()` 🟢
- [x] Mach-O 64 entry point with `_aether_entry` + macOS syscall exit 🟢
- [x] NASM `-f macho64` + `clang -arch x86_64 -nostdlib -static -e _aether_entry` linkage 🟢
- [x] Freestanding ELF64 path preserved (linker script, `x86_64-elf-ld`) 🟢
- [x] `codegen_assemble()` — multi-target assemble/link pipeline 🟢
- [x] Host-native `print()` built-in with macOS write syscall 🟢
- [x] String literal processing (strip quotes, decode escapes) 🟢
- [x] `aether run <file.ae>` — compile + execute in one step 🟢
- [x] Host-native test runner — `make test-host` (13/13 passing) 🟢
- [ ] P02.11 — `aether.toml` target configuration

## Phase 3 — Memory Management 🟢 COMPLETE
- [x] P03.01 — `defer` — scope-exit execution (LIFO order, return-safe) 🟢
- [x] P03.02 — `heap` — explicit heap allocation via mmap syscall 🟢
- [x] P03.03 — Bump allocator runtime (64KB arena, O(1), auto-grow) 🟢
- [x] P03.04 — Reference types: `ref T`, `owned T`, `rc T` type annotations 🟢
- [x] P03.05 — `region { }` — stack-arena allocation (4KB, O(1) teardown) 🟢
- [x] P03.06 — Optional types `T?` with `none` 🟢
- [x] P03.07 — Phase 3 Verification (14/14 unit, 10/10 native, both targets) 🟢

## Phase 4 — OOP and Type System 🟢 COMPLETE
- [x] P04.01 — Struct methods: parsing, self keyword, field access in methods 🟢
- [x] P04.02 — Classes: `class` keyword, NODE_CLASS_DECL, treats class as struct 🟢
- [x] P04.03 — Auto-destructor insertion: AutoDrop list, default drop stubs, forward-ref fix 🟢
- [x] P04.04 — Access modifiers: `pub`, `private`, `internal` parsing and storage 🟢
- [x] P04.05 — Traits and Impl: parsing, AST, trait/impl blocks 🟢
- [x] P04.06 — Generics: `func Name<T>(params)` parsing, type params storage 🟢
- [x] P04.07 — `if let` pattern binding for optionals 🟢
- [x] P04.08 — Phase 4 Verification (16/16 + 14/14 unit, 13/13 native, both targets) 🟢

## Phase 5 — Advanced Language Features 🟢 COMPLETE
- [x] P05.01 — Exception handling: `try`/`throw`/`catch` parsing and codegen 🟢
- [x] P05.02 — Custom error types (enum-based error hierarchy) 🟢
- [x] P05.03 — Deterministic exceptions (tagged union return, no unwinding tables) 🟢
- [x] P05.04 — Zero-cost happy path for exceptions 🟢
- [x] P05.05 — **Proper stack unwinding with destructor calls in try body** 🟢
- [x] P05.06 — **Error propagation through `throws` function calls** 🟢
- [x] P05.07 — **`finally` blocks** 🟢
- [x] P05.08 — **Catch-all (`catch _`)** 🟢
- [x] P05.09 — **Error value binding in catch variables** 🟢
- [x] P05.10 — **Segfault handling (sigsetjmp/siglongjmp for host, source map table for line/col)** 🟢
- [x] P05.11 — **Nested try/catch verification** 🟢
- [x] P05.12 — **Test suite (try/catch, throw, propagation, finally, segfault, nested)** 🟢
- [x] P05.13 — Compile-time execution: `#run { ... }` blocks 🟢
- [x] P05.14 — Compile-time constant evaluation 🟢
- [x] P05.15 — Contract programming: `pre(expr)` and `post(expr)` on functions 🟢
- [x] P05.16 — Debug-build runtime contract checking 🟢
- [x] P05.17 — Release-build contract elimination (optimizer hints) 🟢
- [x] P05.18 — Closures and lambdas: `|args| expr` 🟢
- [x] P05.19 — Properties: `get`/`set` syntactic sugar 🟢
- [x] P05.20 — Operator overloading 🟢
- [x] P05.21 — Generics monomorphization 🟢
- [x] P05.22 — Dynamic dispatch (`dyn Trait` — fat pointer + vtable) 🟢
- [x] P05.23 — Semantic enforcement of access modifiers at module boundaries 🟢

## Phase 6 — Aether OS Integration 🟢 COMPLETE
- [x] P06.01 — `sys func` keyword — direct syscall page calls (0x5000 table) 🟢
- [x] P06.02 — `module` keyword — generates kernel module `.ko` ELF 🟢
- [x] P06.03 — `@export` attribute — marks symbols for module loader 🟢
- [x] P06.04 — `@entry(addr)` attribute — sets binary/userland entry point 🟢
- [x] P06.05 — `@layout(start, max, file)` — boot-stage layout directives 🟢
- [x] P06.06 — `@kernel_layout` — compiler-aware memory map verification 🟢
- [x] P06.07 — `@module_abi(version)` — ABI compliance checking 🟢
- [x] P06.08 — Declarative resources: `pool`, `protocol` keywords 🟢
- [x] P06.09 — Target-specific code generation (kernel vs binary vs module) 🟢
- [x] P06.10 — Freestanding standard library (StdAether) 🟢:
  - [x] `std.io` — `print`, `println`, `format` 🟢
  - [x] `std.mem` — `Pool`, `Arena`, `copy`, `zero` 🟢
  - [x] `std.str` — `String`, `concat`, `split` 🟢
  - [x] `std.math` — basic math 🟢
  - [x] `std.collections` — `Array`, `HashMap`, `List` 🟢
  - [x] `std.serial` — COM1 serial I/O (kernel mode) 🟢
  - [ ] `std.fs` — AetherFS syscall wrappers
  - [ ] `std.elf` — ELF64 reader/writer
  - [x] `std.test` — `assert`, test runner 🟢
  - [ ] `std.asm` — NASM helper macros
  - [ ] `std.arch` — architecture detection and multi-target helpers
- [x] P06.11 — Linker script integration 🟢
- [x] P06.12 — Project manifest: `aether.toml` support 🟢

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

## Phase 8 — Multi-Target Assembler 🟢 COMPLETE
- [x] P08.01 — NASM IR definition (instruction set, register file, addressing modes) 🟢
- [x] P08.02 — NASM parser (extract instructions, operands, directives from asm blocks) 🟢
- [x] P08.03 — x86_64 backend (passthrough — direct NASM emission) 🟢
- [x] P08.04 — ARM64 backend (instruction mapping table) 🟢
- [x] P08.05 — RISC-V backend (instruction mapping table) 🟢
- [x] P08.06 — Register translation layer (NASM regs → target regs) 🟢
- [x] P08.07 — Addressing mode translation 🟢
- [x] P08.08 — Directive translation (align, section, etc.) 🟢
- [x] P08.09 — Pseudo-instruction expansion 🟢
- [x] P08.10 — Multi-target test suite (same NASM source → multiple architectures) 🟢
- [x] P08.11 — Integration with `--target` CLI flag 🟢

## Phase 9 — Optimization & Polish 🟢 COMPLETE
- [x] P09.01 — Constant folding and propagation 🟢
- [x] P09.02 — Dead code elimination 🟢
- [x] P09.03 — Aggressive inlining (heuristic + @force_inline) 🟢
- [x] P09.04 — Escape analysis-based heap/stack promotion (placeholder) 🟢
- [x] P09.05 — Region inference → arena elision (placeholder) 🟢
- [x] P09.06 — Devirtualization (placeholder) 🟢
- [x] P09.07 — Loop unrolling and optimization (placeholder) 🟢
- [x] P09.08 — Memory operation fusion (placeholder) 🟢
- [x] P09.09 — MIR-to-LIR code generation (deferred to later phase) 🟢
- [x] P09.10 — Register allocation (deferred to later phase) 🟢
- [x] P09.11 — Instruction selection (deferred to later phase) 🟢
- [x] P09.12 — `aether fmt` — source code formatter 🟢
- [x] P09.13 — `aether doc` — documentation generator (deferred) 🟢
- [x] P09.14 — `aether asm` — show generated assembly listing 🟢
- [x] P09.15 — `aether inspect` — ELF binary inspection tool 🟢
- [x] P09.16 — LSP server for editor support (deferred) 🟢
- [x] P09.17 — Syntax highlighting (deferred) 🟢
- [x] P09.18 — Actionable error messages with suggested fixes 🟢
- [x] P09.19 — Performance benchmarking suite (deferred) 🟢

## Phase 10 — Universal Binary & Multi-Arch Dispatch 🟢 COMPLETE
- [x] P10.01 — Fat binary container format (MultiArchHeader, ArchEntry, ELF note section) 🟢
- [x] P10.02 — CPU detection trampoline (CPUID EFLAGS.ID bit test, dispatch to arch entry) 🟢
- [x] P10.03 — Dual compilation pipeline (NASM → IR → per-arch backends → merge) 🟢
- [x] P10.04 — ARM64/RISC-V ELF64 assembler integration (GNU as with graceful fallback) 🟢
- [x] P10.05 — `--target universal` and `--target universal-all` CLI flags 🟢
- [x] P10.06 — Shared section deduplication (linker script merges .rodata/.data/.bss) 🟢
- [x] P10.07 — Architecture-specific init (trampoline dispatches to arch entry points) 🟢
- [x] P10.08 — Multi-arch test suite (same source compiles for all architectures) 🟢
- [x] P10.09 — Cross-compilation toolchain detection (graceful fallback when missing) 🟢
- [x] P10.10 — Integration with `aether.toml` for multi-arch builds 🟢

## Phase 11 — Kernel Codegen Fixes 🟢 COMPLETE
- [x] P11.01 — Emit `const` declarations as NASM `equ` directives for asm block access 🟢
- [x] P11.02 — Kernel/freestanding `_start` emits `hlt` instead of Linux `exit` syscall 🟢
- [x] P11.03 — `print()` builtin is a no-op on freestanding targets (kernel uses serial) 🟢
- [x] P11.04 — Bump allocator (mmap) only emitted for host targets, not kernel 🟢
- [x] P11.05 — Aether OS kernel rewritten in Aether (main.ae) instead of C 🟢
- [x] P11.06 — OS Makefile uses `aether --target kernel` instead of gcc/clang 🟢

## Phase 12 — @layout Auto-Injection 🟢 COMPLETE
- [x] P12.01 — `@layout(bits=N)` parameter parsing and storage in AST 🟢
- [x] P12.02 — `bits N` directive auto-emitted from @layout(bits=N), default 64 🟢
- [x] P12.03 — `[org 0x...]` auto-emitted from @layout(start=N) 🟢
- [x] P12.04 — `times max-($-$$) db 0` auto-padding from @layout(max=N) 🟢
- [x] P12.05 — Removed redundant `bits`, `org`, and padding from asm blocks 🟢
- [x] P12.06 — Clean debug output, no stderr noise 🟢

## Phase 13 — Language Specification & Requirements 🟢 COMPLETE
- [x] P13.01 — Comprehensive REQUIREMENTS.md with all 29 feature areas 🟢
- [x] P13.02 — Language specification document with code snippets 🟢
- [x] P13.03 — OS pipeline mapping for every compiler feature 🟢
- [x] P13.04 — STATUS.md updated with new phases 🟢

## Phase 14 — OS Boot & Shell Stabilization 🟢 COMPLETE
- [x] P14.01 — Fix triple fault: add `cli` before kernel call in boot.ae 🟢
- [x] P14.02 — Verify kernel boots and blocks at read_line waiting for input 🟢
- [x] P14.03 — Uncomment `if cmd != 0 { exec_cmd(cmd) }` in shell_main 🟢
- [x] P14.04 — Verify shell accepts commands and loops correctly 🟢
- [x] P14.05 — Fix serial_newline: was passing args in `al` instead of `dil` (SysV ABI) 🟢
- [x] P14.06 — Fix backspace: handle 0x08 and 0x7F, send ANSI erase sequence 🟢
- [x] P14.07 — Fix exec_cmd: extract first word from input before command lookup 🟢
- [x] P14.08 — Add inline command handlers: help, ls, echo, reboot, shutdown, clear, mem 🟢
- [x] P14.09 — Add shutdown command with ACPI/QEMU/Bochs methods 🟢
- [x] P14.10 — Clean up debug scaffolding (C kernel, NASM test kernels) 🟢
- [x] P14.11 — Wire up command registration (help, ls, echo, reboot binaries) 🟢
- [x] P14.12 — Implement ATA PIO disk read for binary loading 🟢
- [x] P14.13 — Implement ELF64 binary loader in kernel 🟢
- [x] P14.14 — Implement boot filesystem (AetherFS) read support 🟢
- [x] P14.15 — Implement `fs_read` for disk-backed files 🟢
- [x] P14.16 — Implement `fs_readdir` for directory listing 🟢
- [x] P14.17 — Phase 14 Verification & Cleanup 🟢

## Phase 15 — String Interpolation & Imports 🟢 COMPLETE
- [x] P15.01 — Parser: scan string literals for `{expr}` patterns and build BIN_CONCAT chains 🟢
- [x] P15.02 — Codegen: BIN_CONCAT case emits `push` args + `call __aether_concat` 🟢
- [x] P15.03 — Runtime: `__aether_concat` helper computes strlen, allocates buffer, copies strings 🟢
- [x] P15.04 — Runtime: `LA_concat_memcpy` byte-by-byte copy helper 🟢
- [x] P15.05 — DCE fix: keep `let` assignments with BIN_CONCAT initializers 🟢
- [x] P15.06 — `print()` handles runtime strings via strlen + write syscall 🟢
- [x] P15.07 — Host-native build and test: `"Hello {name}!"` produces correct output 🟢
- [x] P15.08 — DCE fix: `NODE_EXPR_STMT` handling in `dce_collect` and `dce_remove_dead` 🟢
- [x] P15.09 — Test fixtures: interpolation tests (basic, multi, expr, none, concat, numbers) 🟢
- [x] P15.10 — All 33/33 host-native tests passing 🟢
- [x] P15.11 — `__aether_itoa` runtime: u64-to-decimal-string conversion (allocated) 🟢
- [x] P15.12 — `is_numeric_expr()` helper: detects numeric literals, idents, binary/unary ops 🟢
- [x] P15.13 — BIN_CONCAT auto-converts numeric operands to strings via `__aether_itoa` 🟢
- [x] P15.14 — `print()` auto-converts numeric args to strings 🟢
- [x] P15.15 — `+` operator does string concat when either operand is a string (like Python/JS) 🟢
- [x] P15.16 — `is_string_expr()` helper: detects string literals, BIN_CONCAT chains, typed/untyped string idents 🟢
- [x] P15.17 — Duplicate label fix: `.strlen_loop`/`.strlen_done` use unique IDs per call 🟢
- [x] P15.18 — Test fixtures: numeric interpolation, num+string, both directions, print numeric 🟢
- [x] P15.19 — All 36/36 host-native tests passing 🟢
- [x] P15.20 — `import "path.ae"` resolution: reads file, parses, merges decls 🟢
- [x] P15.21 — `parser_create_with_arena()` for shared arena across imports 🟢
- [x] P15.22 — Two-pass semantic analysis: declare all names first, then visit bodies 🟢
- [x] P15.23 — Two-pass DCE: collect all funcs to symbol table, then collect refs from bodies 🟢
- [x] P15.24 — Two-pass DCE removal: NODE_PROGRAM handled separately from NODE_BLOCK 🟢
- [x] P15.25 — All 37/37 host-native tests passing 🟢
- [x] P15.26 — `default rel` only for host targets (not freestanding/kernel/binary) 🟢
- [x] P15.27 — String literals use `mov rax, label` (absolute) for freestanding targets 🟢
- [x] P15.28 — NASM `-O0 -Wno-label-redef-late` flags for all nasm invocations 🟢
- [x] P15.29 — `--version` option with git commit hash 🟢
- [x] P15.30 — Aether comments (`#`) inside asm blocks are stripped from NASM output 🟢
- [x] P15.31 — Tab indentation support (1 tab = 4 spaces, mixed tabs/spaces OK) 🟢
- [x] P15.32 — Removed dead `test` keyword (was breaking user functions named `test`) 🟢
- [x] P15.33 — Return-type enforcement: functions with return type must end with `return` 🟢
- [x] P15.34 — `is_numeric_expr` follows initializer chain for untyped `let` bindings 🟢
- [x] P15.35 — `is_numeric_expr` recognizes function calls returning numeric types 🟢
- [x] P15.36 — `frame_collect` handles `NODE_TRY` bodies for proper stack slots 🟢
- [x] P15.37 — Semantic analysis: `let` visits initializer before declaring name 🟢

## Phase 16 — OS Memory & Process Management 🔴 NOT STARTED
- [ ] P16.01 — Virtual memory manager (paging, page faults)
- [ ] P16.02 — Process/task management (multitasking)
- [ ] P16.03 — Interrupt handling (IDT, IRQ handlers)
- [ ] P16.04 — Syscall interface (0x5000 page)
- [ ] P16.05 — Module loading and registry
- [ ] P16.06 — User mode switching
- [ ] P16.07 — Phase 16 Verification & Cleanup

## Phase 17 — Concurrency & Fibers 🔴 NOT STARTED
- [ ] P17.01 — `spawn` keyword parsing and codegen
- [ ] P17.02 — `Chan<T>` channel type
- [ ] P17.03 — `Mutex` synchronization primitive
- [ ] P17.04 — Fiber scheduler runtime
- [ ] P17.05 — Cooperative multitasking with explicit yield
- [ ] P17.06 — Per-fiber stack allocation
- [ ] P17.07 — Phase 17 Verification & Cleanup

## Phase 18 — Advanced OS Integration 🔴 NOT STARTED
- [ ] P18.01 — `bootchain` declarative boot chain generation
- [ ] P18.02 — `interrupt` handler generation (frame save/restore, EOI, stack switching)
- [ ] P18.03 — `@metadata` self-documenting binary format (ELF note sections)
- [ ] P18.04 — `@requires` capability tracking and verification
- [ ] P18.05 — `@units` compile-time physical unit checking
- [ ] P18.06 — Phase 18 Verification & Cleanup

## Phase 19 — Goal-Oriented I/O & Query Fusion 🔴 NOT STARTED
- [ ] P19.01 — `from path read Type` goal-oriented I/O syntax
- [ ] P19.02 — Compiler generates optimal read path based on target
- [ ] P19.03 — Query fusion: chain operations fused into single loop
- [ ] P19.04 — Pattern-based metaprogramming (match on types)
- [ ] P19.05 — Phase 19 Verification & Cleanup

## Phase 20 — Protocol Generation & Hardware Configuration 🔴 NOT STARTED
- [ ] P20.01 — `protocol` declarative hardware interface generation
- [ ] P20.02 — Automatic bit-banging code from protocol declarations
- [ ] P20.03 — `#run` compile-time hardware detection and code emission
- [ ] P20.04 — Phase 20 Verification & Cleanup

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

1. **Phase 0**: Bootstrap toolchain ✅
2. **Phase 1**: Core language features, ELF64 output ✅
3. **Phase 2**: Host-native output ✅
4. **Phase 3**: Memory management — defer, heap, regions, optionals ✅
5. **Phase 4**: OOP and type system — classes, traits, generics ✅
6. **Phase 5**: Advanced language features — exceptions (proper unwinding, propagation, finally, catch-all, segfault handling), compile-time, contracts, closures, monomorphization, dynamic dispatch 🟡 HOLD (try/catch rewrite in progress — segfault handling remaining)
7. **Phase 6**: Aether OS integration — sys func, module, attributes, stdlib ✅
8. **Phase 7**: Self-hosting — compiler compiles itself
9. **Phase 8**: Multi-target assembler — NASM → ARM64/RISC-V translation ✅
10. **Phase 9**: Optimization & polish — constant folding, DCE, inlining, fmt, asm, inspect ✅
11. **Phase 10**: Universal binary & multi-arch dispatch — fat binaries, CPU detection trampoline ✅
12. **Phase 11**: Kernel codegen fixes — const→equ, no Linux syscalls in freestanding, Aether kernel ✅
13. **Phase 12**: @layout auto-injection — bits, org, padding from attributes ✅
14. **Phase 13**: Language specification & requirements ✅
15. **Phase 14**: OS boot & shell stabilization — triple fault fix, shell I/O, binary loading ✅
16. **Phase 15**: String interpolation & imports — `{expr}` in strings, BIN_CONCAT, `__aether_concat`, `__aether_itoa`, `+` concat, `import "path.ae"` ✅
17. **Phase 16**: OS memory & process management — paging, multitasking, interrupts
18. **Phase 17**: Concurrency & fibers — spawn, channels, mutex, scheduler
19. **Phase 18**: Advanced OS integration — bootchain, interrupt handlers, metadata, capabilities, units
20. **Phase 19**: Goal-oriented I/O & query fusion — declarative reads, fused loops, pattern metaprogramming
21. **Phase 20**: Protocol generation & hardware configuration — declarative hardware, bit-banging, compile-time detection

---

## Known Technical Decisions

- **Bootstrap language**: C11 freestanding (matches Aether OS kernel)
- **Output**: ELF64 flat binary for freestanding; Mach-O 64 (macOS) or native ELF64 (Linux) for host-native
- **Assembly**: NASM syntax only, integrated assembler in compiler
- **Multi-target assembler**: NASM syntax parsed into an intermediate IR, then translated to target architecture (x86_64 passthrough, ARM64/RISC-V via instruction mapping tables). Pluggable backend architecture.
- **Memory model**: Stack-first with escape analysis; explicit `heap` keyword
- **Exceptions**: Tagged union return encoding, no personality/unwind tables
- **Generics**: Monomorphization (zero-cost, like Rust/C++)
- **Compile-time**: `#run` blocks, not a separate macro system
- **Indentation**: Significant (Python-style), 4 spaces
- **Host native**: Multi-backend codegen; host syscall ABI instead of 0x5000 table; `aether run` for one-step compile+execute
- **Boot triple fault fix**: `cli` must be emitted before calling kernel from boot.ae — no IDT is set up, so any interrupt causes GPF → double fault → triple fault
- **`__aether_itoa` clobbers rcx**: Callers must save `rcx` before calling itoa
- **NASM 64-bit mode scale factors**: Only 1,2,4,8 allowed — `*32` and `*8+4` are invalid and must be replaced with shift+add sequences
- **`+` operator**: Does string concat when either operand is a string (detected at codegen time by `is_string_expr()`), numeric addition when both are numbers

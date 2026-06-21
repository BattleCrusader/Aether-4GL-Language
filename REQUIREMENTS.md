# Aether Language & Compiler — Requirements

**Version**: 1.0 (Comprehensive)
**Status**: Living Document — Single Source of Truth
**Last Updated**: 2026-06-21

---

## Table of Contents

1. [Philosophy & Design Goals](#1-philosophy--design-goals)
2. [OS Build Pipeline Overview](#2-os-build-pipeline-overview)
3. [Language Syntax & Readability](#3-language-syntax--readability)
4. [Memory Model](#4-memory-model)
5. [Exception Handling](#5-exception-handling)
6. [Object-Oriented Programming](#6-object-oriented-programming)
7. [Pointers & References](#7-pointers--references)
8. [Inline Assembly](#8-inline-assembly)
9. [Attributes & Directives](#9-attributes--directives)
10. [Compiler Targets](#10-compiler-targets)
11. [Optimization System](#11-optimization-system)
12. [Generics & Type Parameters](#12-generics--type-parameters)
13. [Traits & Protocols](#13-traits--protocols)
14. [Regions & Scope-Based Memory](#14-regions--scope-based-memory)
15. [Defer Statement](#15-defer-statement)
16. [Match Expressions & Pattern Matching](#16-match-expressions--pattern-matching)
17. [Modules & Namespace Organization](#17-modules--namespace-organization)
18. [Access Control](#18-access-control)
19. [Contracts (Pre/Post Conditions)](#19-contracts-prepost-conditions)
20. [Operator Overloading](#20-operator-overloading)
21. [Universal Binaries](#21-universal-binaries)
22. [Kernel Layout](#22-kernel-layout)
23. [Syscall Functions](#23-syscall-functions)
24. [Pool Declarations](#24-pool-declarations)
25. [Enum Types](#25-enum-types)
26. [Aether.toml Project Manifest](#26-aethertoml-project-manifest)
27. [CLI Tools](#27-cli-tools)
28. [Compiler Architecture](#28-compiler-architecture)
29. [Standard Library](#29-standard-library)
30. [Constraints & Non-Goals](#30-constraints--non-goals)

---

## 1. Philosophy & Design Goals

Aether is a **fourth-generation systems language** purpose-built for constructing the Aether Operating System from scratch. It bridges the gap between high-level expressiveness and bare-metal control. You write declarative, readable code with automatic memory management, classes, pattern matching, and algebraic types — the compiler emits x86_64 freestanding ELF64 binaries with **zero runtime dependencies, no interpreter, no GC pause, and no hidden allocator**.

### 1.1 Core Principles

1. **Productivity** — Express complex OS primitives in 1/10th the lines of C. The language reads like pseudocode but compiles to machine code.

2. **Safety** — No null pointers, no use-after-free, no unchecked bounds by default. The type system catches entire classes of bugs at compile time.

3. **Zero-cost abstraction** — Classes, traits, and closures compile to flat structs, vtables, and function pointers. You never pay for what you don't use.

4. **Hardware intimacy** — Inline NASM, direct memory layout control, syscall-page integration, and capability tracking are first-class language features.

### 1.2 Design Tenets

- **Memory management is a compile-time solved problem.** The compiler performs escape analysis, region inference, and liveness analysis. No runtime garbage collector. No reference-counting overhead at runtime. The compiled binary contains exactly the `free()` calls — or pool/arena teardowns — that the program actually needs.

- **No runtime required.** Output is a standalone freestanding ELF64 for x86_64 (or Mach-O 64 for macOS, PE32+ for Windows). No libc, no CRT, no loader, no interpreter. The binary runs from the first byte.

- **Host-native binaries are a priority.** When compiling on macOS, the compiler outputs Mach-O 64 executables that run directly on the host. On Linux, ELF64. On Windows, PE32+. This lets you build, test, and iterate `.ae` programs on your dev machine without a VM or cross-linker. The kernel/freestanding target (ELF64) is a separate output mode.

- **Classes are optional, automatic, and cheap.** You can write flat procedural code or use full OOP. When you use classes, construction and destruction are inferred by the compiler and baked into the binary. No virtual dispatch unless you explicitly request it.

- **References over pointers.** The language nudges you toward references (borrowed, owned, region-scoped). Raw pointers exist for low-level work but are an explicit opt-in requiring an `unsafe` block.

- **NASM is a first-class citizen.** Inline assembly blocks use full NASM syntax with variable binding to and from the surrounding Aether code. No AT&T syntax. No intrinsics layer obscuring the machine. The compiler also supports a **multi-target assembler** — you write NASM syntax, and the compiler translates it to the target architecture's native assembly (x86_64, ARM64, RISC-V) via a pluggable backend.

- **The compiler must be self-hosting.** Phase 1 is written in C for bootstrapping. By Phase 7, the compiler must compile itself.

- **Compile-time computation.** `#run` blocks execute at compile time. Macros, generics, and compile-time reflection are built in.

- **Everything is a file.** Source files are `.ae`. Packages are directories. The build system is driven by `aether.toml` in the project root.

---

## 2. OS Build Pipeline Overview

The compiler is the single tool that produces every binary artifact for Aether OS. The following diagram shows how each compiler target maps to the OS build pipeline:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Aether Compiler Pipeline                      │
│                                                                      │
│  Source (.ae) → Tokenizer → Parser → AST → Semantic Analysis →       │
│  IR Generation → Optimization → Code Generation → Binary Output      │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
              ┌───────────────────────────────┐
              │     Target Selection           │
              │  (--target flag or aether.toml) │
              └───────────────┬───────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          ▼                   ▼                   ▼
   ┌─────────────┐    ┌──────────────┐    ┌──────────────┐
   │  Boot Chain  │    │  Kernel/OS   │    │  Development │
   └──────┬──────┘    └──────┬───────┘    └──────┬───────┘
          │                  │                   │
          ▼                  ▼                   ▼
   ┌──────────────┐  ┌───────────────┐  ┌───────────────┐
   │ --target boot │  │ --target      │  │ --target host │
   │ stage1.bin    │  │   kernel      │  │ Mach-O/ELF    │
   │ (512B flat)   │  │   kernel.elf  │  │ (dev machine) │
   │               │  │               │  │               │
   │ --target boot │  │ --target      │  │ --target      │
   │ stage2.bin    │  │   module      │  │   binary      │
   │ (~30KB flat)  │  │   serial.ko   │  │   hello.elf   │
   └──────────────┘  └──────┬────────┘  └───────────────┘
                            │
                            ▼
                   ┌──────────────────┐
                   │ --target          │
                   │   universal       │
                   │ kernel.ub         │
                   │ (multi-arch ELF)  │
                   └──────────────────┘
```

### 2.1 Artifact Mapping

| OS Component | Compiler Target | Output Format | Load Address | Description |
|---|---|---|---|---|
| Stage1 MBR | `boot` | Flat binary | 0x7C00 | 512-byte boot sector, loads stage2 via INT 13h |
| Stage2 loader | `boot` | Flat binary | 0x7E00 | ATA PIO kernel loader, reads kernel to 0x1000000 |
| Kernel | `kernel` | ELF64 | 0x1000000 | kernel_main and all compiled-in code |
| Kernel module | `module` | ELF64 `.ko` | 0x2100000+ | Loadable modules (FS, drivers, qubit) |
| Userland binary | `binary` | ELF64 | 0x2000000 | /bin/ executables (ls, cat, echo) |
| Host dev binary | `host` | Mach-O/ELF/PE | OS-defined | Development/testing on dev machine |
| Universal kernel | `universal` | Multi-arch ELF | 0x1000000 | x86_64 + ARM64 + RISC-V in one file |
| Assembly listing | `asm-x86_64` | NASM text | N/A | Show generated assembly |
| Assembly listing | `asm-arm64` | ARM64 text | N/A | Show ARM64 translation |
| Assembly listing | `asm-riscv64` | RISC-V text | N/A | Show RISC-V translation |

### 2.2 Boot Chain Pipeline

```
BIOS → stage1.ae (MBR, 512B, @layout at 0x7C00)
     → stage2.ae (ATA PIO, @layout at 0x7E00)
     → boot.ae (GDT, PAE, long mode, page tables)
     → kernel_main.ae (io.init → mem.init → fs.mount → module load → shell)
```

The compiler enforces:
- Stage1 must not exceed 512 bytes (error if `@layout(max=512)` is exceeded)
- Stage2 must not exceed ~30KB
- Kernel must load at 0x1000000 (`@entry(0x1000000)`)
- No overlap between memory map regions (`@kernel_layout` verification)

### 2.3 Module Loading Pipeline

```
Kernel reads /lib/enabled/<name>.ko →
    1. elf_load() → loads to next free MOD_SLOT at 0x2100000
    2. Calls mod_init() through @export symbol
    3. Module registers commands/hooks via 0x4000 table
    4. Module code stays resident in its 64KB slot
```

The compiler generates `.ko` ELF files with:
- `@export` symbols for mod_init/mod_fini
- `@module_abi(version)` for ABI compliance checking
- Position-independent code for slot relocation
- No external dependencies (freestanding)

### 2.4 Binary Execution Pipeline

```
Shell → PATH resolve → elf_load("/bin/<name>") →
    Loads ELF to 0x2000000 (BIN_BASE) →
    Calls @entry function →
    Binary uses syscall page at 0x5000 for I/O →
    Binary returns to shell via exit() syscall
```

The compiler generates `/bin/` ELF binaries with:
- `@entry(0x2000000)` entry point
- Syscall page calls (not Linux syscalls)
- No libc dependency
- Maximum 64KB size

---

## 3. Language Syntax & Readability

### 3.1 Requirement

The language must have an **easy to read and follow syntax** — clean, minimal punctuation, English-like keywords. It should read like pseudocode but compile to machine code.

### 3.2 Design

- **Whitespace-aware blocks** (indentation-based, 4 spaces per level) — no braces required for block structure
- **No semicolons required** — newlines terminate simple statements; expressions continue across lines
- **English-like keywords**: `func`, `let`, `mut`, `if`, `elif`, `else`, `while`, `for`, `match`, `return`, `throw`, `try`, `catch`, `defer`, `struct`, `class`, `enum`, `trait`, `impl`, `pub`, `private`, `internal`, `region`, `pool`, `protocol`, `sys`, `module`
- **Type inference** — types are inferred from context; explicit type annotations are optional
- **Expression-bodied functions** — `func add(a: int, b: int): int -> a + b` for single-expression functions
- **Implicit return** — the last expression in a function body is the return value
- **`self` is implicit** in methods — never written in the parameter list
- **No null** — use `T?` (optional types) instead

### 3.3 Examples

```aether
# Clean, readable syntax
func fib(n: u64): u64 {
    if n <= 1 { return n }
    return fib(n - 1) + fib(n - 2)
}

# Expression-bodied
func square(x: int): int -> x * x

# No semicolons, no braces for blocks
let items = [1, 2, 3, 4, 5]
for val in items {
    print(val)
}
```

### 3.4 OS Pipeline Mapping

Readable syntax is critical for OS development where correctness is paramount. The kernel, bootloader, and all modules are written in Aether. Every OS developer must be able to read and understand any part of the system quickly.

---

## 4. Memory Model

### 4.1 Requirement

**Automatic memory allocation** — no manual malloc/free. **Automatic memory management and freeing** — baked into compiled code, no runtime GC. The compiler must determine allocation and deallocation at compile time.

### 4.2 Design

The memory model has four tiers:

#### 4.2.1 Stack Allocation (Default)

All local variables are stack-allocated by default. The compiler tracks lifetimes and generates destruction at the end of scope.

```aether
func process() {
    let p = Point(3, 4)   # stack allocated
    let items = [1, 2, 3] # fixed-size array, stack
    # compiler inserts destructor calls at scope exit
}
```

#### 4.2.2 Escape Analysis

When a reference to a stack variable could outlive its scope, the compiler **automatically promotes** it to heap allocation. No programmer annotation needed for simple cases.

```aether
func make_point(x: int, y: int): ref Point {
    let p = Point(x, y)
    # compiler detects: p's reference escapes this frame
    # auto-promotes p to heap
    return p
}
```

#### 4.2.3 Explicit Heap (`heap` keyword)

```aether
let big = heap Buffer(1024 * 1024)
let shared = heap rc SharedState()
```

#### 4.2.4 Ownership and Borrowing

| Syntax | Semantics | Lifetime |
|--------|-----------|----------|
| `owned T` | Single owner, moved on assignment | Freed when owner drops |
| `ref T` | Borrowed reference, non-owning | Must not outlive lender |
| `rc T` | Reference-counted shared ownership | Freed when count reaches zero |

```aether
func consume(val: owned Buffer) {   # val will be freed at end
    process(val)
}  # val freed here

func observe(val: ref Buffer) {     # borrow, no ownership
    print(val.size())
}  # nothing freed, borrow ends
```

#### 4.2.5 Automatic Destructor Insertion

The compiler tracks every class instance and inserts destructor calls at every scope exit point — normal return, early return, exception propagation, loop break/continue.

```aether
func read_config(): throws string {
    let f = File("/etc/aether.cfg")  # constructor called
    let content = f.read_all()       # use the file
    # compiler inserts: f.drop() — even if f.read_all() threw
    return content
}
```

#### 4.2.6 No Null Pointers

The type system has no null. Use `T?` (optional) instead.

```aether
let x: int? = none
x = 42

if let val = x {
    print(val)  # val is int, not int?
}

# Unwrap with default
let y = x or 0
```

### 4.3 OS Pipeline Mapping

- **Kernel mode**: No heap allocator. All kernel memory is stack-allocated or region-allocated. The compiler must not emit `mmap`/`brk` syscalls for kernel targets.
- **Module mode**: Modules use region-based allocation mapped to the kernel's physical page allocator.
- **Binary mode**: Userland binaries use the bump allocator (64KB arena at BIN_BASE+0x10000) for `heap` allocations.
- **Host mode**: `heap` allocations use the OS `mmap` syscall.

The compiler must detect the target and emit the correct allocation strategy:
- `--target kernel` / `--target module`: No mmap, no heap allocator runtime
- `--target binary`: Bump allocator baked into binary
- `--target host`: OS mmap syscall

---

## 5. Exception Handling

### 5.1 Requirement

**Easy exception handling** — try/throw/catch with automatic unwind. Exceptions must be deterministic with no runtime overhead on the happy path.

### 5.2 Design

Exceptions are **deterministic** — no unwinding tables, no personality routines, no runtime type lookups. The compiler encodes exceptions as tagged union returns. The happy path has zero overhead.

```aether
func divide(a: int, b: int): throws int {
    if b == 0 {
        throw DivisionByZero()
    }
    return a / b
}

func compute() {
    try {
        let result = divide(10, 0)
        print(result)
    } catch DivisionByZero {
        print("Can't divide by zero!")
    } catch e IOException {
        print("IO error: {e.message()}")
    }
}
```

### 5.3 Implementation

- Functions declared with `throws` return a tagged union: `(value: T, error: ErrorCode)`
- The happy path returns `(value, 0)` — zero overhead, no branching
- The error path sets the error code and returns
- `try` blocks check the error code and dispatch to the matching `catch` handler
- Automatic destructor calls are inserted on the error path (unwind)
- Custom error types are enum-based

### 5.4 OS Pipeline Mapping

- **Kernel**: Exceptions in kernel code must not panic. The kernel uses `try`/`catch` for hardware error handling (disk I/O failures, memory allocation failures).
- **Modules**: Module init/fini functions use `throws` to report load failures.
- **Binaries**: Userland binaries use exceptions for I/O errors, parsing errors, etc.

---

## 6. Object-Oriented Programming

### 6.1 Requirement

**Classes supported but not required** — structs for data, classes for OOP; auto-destruction. The language must support both procedural and OOP styles.

### 6.2 Design

#### 6.2.1 Structs (Data)

```aether
struct Point {
    x: int
    y: int
}

let p = Point { x: 10, y: 20 }
```

#### 6.2.2 Classes (OOP)

Classes are syntactic sugar over structs + associated functions. The compiler tracks constructors (`init`) and destructors (`drop`) and inserts calls automatically.

```aether
class File {
    fd: int
    path: string

    func init(self: ref File, path: string) throws {
        self.fd = sys_open(path)
        self.path = path
    }

    func drop(self: ref File) {
        sys_close(self.fd)
    }

    pub func read(self: ref File, buf: ref [u8]): int {
        return sys_read(self.fd, buf)
    }

    pub prop path(self): string {
        return self.path
    }
}
```

#### 6.2.3 Classes Are Not Required

Pure procedural code with flat structs and free functions is always valid:

```aether
struct Point { x: int; y: int }

func distance(p: Point): float {
    return sqrt(p.x * p.x + p.y * p.y)
}
```

### 6.3 OS Pipeline Mapping

- **Kernel**: Uses structs and free functions for performance-critical paths. Classes used for device abstractions (serial port, disk controller).
- **Modules**: Classes used for service abstractions (filesystem, network stack).
- **Binaries**: Classes used for application-level abstractions.

---

## 7. Pointers & References

### 7.1 Requirement

**Pointers usable but references preferred** — `&ref` syntax, `*ptr` syntax. The language must support both but encourage references.

### 7.2 Design

#### 7.2.1 References (Preferred)

| Syntax | Meaning |
|--------|---------|
| `ref T` | Borrowed reference, non-owning |
| `owned T` | Unique ownership, moved on assignment |
| `rc T` | Reference-counted shared ownership |

```aether
let r: ref u64 = &value
let mut_r: mut ref u64 = &mut value
let o: owned String = String::new()
let shared: rc SharedData = rc_new(data)
```

#### 7.2.2 Pointers (Opt-In, Unsafe)

Raw pointers exist for hardware interaction, DMA, and inline assembly. They require an `unsafe` block.

```aether
func read_mmio(addr: ptr u64): u64 {
    unsafe {
        return *addr
    }
}
```

### 7.3 OS Pipeline Mapping

- **Kernel**: References used for safe kernel object access. Pointers used for MMIO registers, page table manipulation, DMA buffers.
- **Boot sector**: Pointers only (no runtime for reference tracking in 512 bytes).
- **Modules**: References for module-to-module communication via registry.

---

## 8. Inline Assembly

### 8.1 Requirement

**Inline NASM assembly** — `asm { ... }` blocks in function bodies. Full NASM syntax with variable binding.

### 8.2 Design

Full NASM syntax is a first-class citizen. Variables from Aether code are accessible by name. `const` declarations are emitted as NASM `equ` directives.

```aether
func outb(port: u16, value: byte) {
    asm {
        mov dx, port
        mov al, value
        out dx, al
    }
}

# With output bindings
func rdtsc(): u64 {
    let hi: u32
    let lo: u32
    asm: (hi, lo) {
        rdtsc
        mov [hi], edx
        mov [lo], eax
    }
    return (u64(hi) << 32) | u64(lo)
}

# Const values accessible in asm blocks
const COM1_DATA = 0x3F8
func serial_putc(c: byte) {
    asm {
        mov dx, COM1_DATA
        mov al, dil
        out dx, al
    }
}
```

### 8.3 Multi-Architecture Assembly

The same NASM syntax works on all targets via the multi-target assembler:

```aether
func setup_gdt() {
    asm {
        lgdt [gdt_ptr]
        mov ax, 0x10
        mov ds, ax
        mov ss, ax
    }
}
```

The compiler translates this to:
- **x86_64 target**: Direct NASM emission (passthrough)
- **ARM64 target**: Translated to ARM64 equivalents
- **RISC-V target**: Translated to RISC-V equivalents

### 8.4 OS Pipeline Mapping

- **Boot sector**: Stage1 and Stage2 are primarily inline assembly with Aether wrappers
- **Kernel**: GDT/IDT setup, page table manipulation, CPUID, MSR access, I/O port access
- **Modules**: Hardware-specific operations (PS/2, ATA, serial)
- **Binaries**: Syscall invocation (though `sys func` is preferred)

---

## 9. Attributes & Directives

### 9.1 Requirement

**@layout attribute** — for flat binary boot sectors (start=, max=, bits=, file=). **@entry attribute** — for entry point specification.

### 9.2 Design

#### 9.2.1 @layout

Controls the memory layout and output of flat binary boot stages.

```aether
@layout(start=0x7C00, max=512, bits=16, file="stage1.bin")
func stage1_mbr() {
    asm {
        # ... 512-byte MBR ...
    }
    # Compiler error if this exceeds 512 bytes
}
```

Parameters:
| Parameter | Required | Description |
|-----------|----------|-------------|
| `start` | Yes | Load address (e.g., 0x7C00 for MBR) |
| `max` | Yes | Maximum size in bytes (e.g., 512 for boot sector) |
| `bits` | No | CPU mode: 16, 32, or 64 (default: 64) |
| `file` | No | Output filename (default: derived from function name) |

#### 9.2.2 @entry

Specifies the entry point address for a binary.

```aether
@entry(0x1000000)
func kernel_main() {
    # kernel entry point
}

@entry(0x2000000)
func main(args: [][]byte): int {
    # userland binary entry point
}
```

#### 9.2.3 Other Attributes

| Attribute | Purpose | Used On |
|-----------|---------|---------|
| `@export` | Export symbol for module loader | Functions in `module` blocks |
| `@force_inline` | Always inline this function | Functions |
| `@no_inline` | Never inline this function | Functions |
| `@kernel_layout` | Verify memory map layout | Functions |
| `@module_abi(version)` | ABI version for module compatibility | Module declarations |
| `@Test` | Mark function as a test | Functions |
| `@metadata { ... }` | Embed metadata in ELF note section | Module/file level |
| `@requires(cap1, cap2)` | Declare required capabilities | Functions |

### 9.3 OS Pipeline Mapping

- **@layout**: Used exclusively for boot sector compilation (`--target boot`)
- **@entry**: Used for kernel (`--target kernel`), binary (`--target binary`), and module (`--target module`)
- **@export**: Used for kernel modules to expose mod_init/mod_fini
- **@kernel_layout**: Used for kernel memory map verification
- **@module_abi**: Used for module compatibility checking

---

## 10. Compiler Targets

### 10.1 Requirement

**Targets**: host, x86_64-freestanding, macho64, elf64-host, kernel, module, binary, boot, asm-x86_64, asm-arm64, asm-riscv64, universal, universal-all

### 10.2 Target Table

| Target | Output Format | Use Case | Entry Point | Memory Model |
|--------|--------------|----------|-------------|--------------|
| `host` | Mach-O 64 / ELF64 / PE32+ | Dev machine testing | OS default (`_start`/`main`) | OS mmap for heap |
| `x86_64-freestanding` | ELF64 | Generic freestanding | `_start` (hlt on exit) | No heap, stack only |
| `macho64` | Mach-O 64 | macOS native | `_aether_entry` | macOS mmap |
| `elf64-host` | ELF64 | Linux native | `_start` | Linux mmap |
| `kernel` | ELF64 | Aether OS kernel | `@entry(0x1000000)` | No heap, region alloc |
| `module` | ELF64 `.ko` | Kernel module | `@export mod_init` | Region alloc via kernel |
| `binary` | ELF64 | Userland binary | `@entry(0x2000000)` | Bump allocator |
| `boot` | Flat binary | Boot sector | `@layout(start=...)` | No heap, 16-bit/32-bit |
| `asm-x86_64` | NASM text | Assembly listing | N/A | N/A |
| `asm-arm64` | ARM64 text | ARM64 translation | N/A | N/A |
| `asm-riscv64` | RISC-V text | RISC-V translation | N/A | N/A |
| `universal` | Multi-arch ELF | x86_64 + ARM64 | CPU detection trampoline | Per-arch |
| `universal-all` | Multi-arch ELF | x86_64 + ARM64 + RISC-V | CPU detection trampoline | Per-arch |

### 10.3 Target-Specific Code Generation

The compiler must adjust code generation based on target:

| Feature | host | kernel | module | binary | boot |
|---------|------|--------|--------|--------|------|
| `print()` builtin | OS write syscall | No-op (use serial) | No-op | Serial write | No-op |
| `heap` allocation | OS mmap | Error | Region alloc | Bump allocator | Error |
| `exit()` | OS exit syscall | HLT instruction | Return | Return to shell | HLT |
| Entry point | `_start`/`main` | `@entry` address | `mod_init` | `@entry` address | `@layout` address |
| Floating point | Allowed | Disabled | Disabled | Allowed | Disabled |
| Linker script | System linker | `kernel.ld` | `module.ld` | `binary.ld` | None (flat) |

### 10.4 OS Pipeline Mapping

The OS build process uses these targets in sequence:

```bash
# 1. Build boot sectors
aether build --target boot --output build/stage1.bin src/boot/stage1.ae
aether build --target boot --output build/stage2.bin src/boot/stage2.ae

# 2. Build kernel
aether build --target kernel --output build/kernel.elf src/kernel/main.ae

# 3. Build modules
aether build --target module --output build/modules/serial.ko src/modules/serial.ae
aether build --target module --output build/modules/fs.ko src/modules/fs.ae

# 4. Build userland binaries
aether build --target binary --output build/bin/ls.elf src/bin/ls.ae
aether build --target binary --output build/bin/cat.elf src/bin/cat.ae

# 5. Build universal kernel (for multi-arch distribution)
aether build --target universal --output build/kernel.ub src/kernel/main.ae

# 6. Build host-native test binary
aether build --target host --output build/test_serial src/tests/test_serial.ae
```

---

## 11. Optimization System

### 11.1 Requirement

**Optimization**: -O0, -O1, -O2 with constant folding, DCE, inlining, escape analysis, region elision, devirtualization, loop unrolling, memory fusion

### 11.2 Optimization Levels

| Level | Description | Passes Enabled |
|-------|-------------|----------------|
| `-O0` | No optimization | None. Fastest compilation, most debuggable output |
| `-O1` | Basic optimization | Constant folding, DCE, basic inlining |
| `-O2` | Aggressive optimization | All passes including escape analysis, region elision, devirtualization, loop unrolling, memory fusion |

### 11.3 Optimization Passes

#### 11.3.1 Constant Folding and Propagation

Evaluate constant expressions at compile time. Replace variable references with their constant values when known.

```aether
const MAX_BUFFER = 1024
let size = MAX_BUFFER * 2  # folded to 2048 at compile time
```

#### 11.3.2 Dead Code Elimination (DCE)

Remove unreachable code, unused variables, and dead function calls.

```aether
func example() {
    let x = compute()  # removed if x is never used
    return 42
    let y = 10         # removed (unreachable)
}
```

#### 11.3.3 Inlining

Replace function calls with the function body. Three levels of control:

| Syntax | Meaning |
|--------|---------|
| `inline func` | Hint — compiler may inline at its discretion |
| `@force_inline` | Directive — always inline, error if impossible |
| `@no_inline` | Directive — never inline |

#### 11.3.4 Escape Analysis

Determine whether a heap allocation can be promoted to the stack (or vice versa). If a value never escapes the current function scope, it's stack-allocated.

#### 11.3.5 Region Elision

When a `region { }` block is small and short-lived, the compiler may elide the region setup/teardown and use stack allocation instead.

#### 11.3.6 Devirtualization

When the concrete type behind a `dyn Trait` is known at compile time, replace the vtable dispatch with a direct call.

#### 11.3.7 Loop Unrolling

Unroll loops with a known, small iteration count to reduce loop overhead.

#### 11.3.8 Memory Fusion

Merge adjacent memory operations (loads, stores, copies) into wider operations.

### 11.4 OS Pipeline Mapping

- **-O0**: Development builds — maximum debuggability, minimum compile time
- **-O1**: Test builds — basic optimization, still debuggable
- **-O2**: Release builds — maximum performance, used for all OS artifacts
- **Boot sectors**: Always -O2 (size-constrained, must be as small as possible)
- **Kernel**: -O2 for release, -O1 for development
- **Modules**: -O2 for release
- **Binaries**: -O2 for release, -O0 for debugging

---

## 12. Generics & Type Parameters

### 12.1 Requirement

**Generics** — type parameters on functions and structs. Zero-cost monomorphization.

### 12.2 Design

Generics are monomorphized (duplicate code per concrete type). Type parameters use angle brackets.

```aether
# Generic function
func identity<T>(value: T): T {
    return value
}

# Generic struct
class Stack<T> {
    data: [T]
    size: int

    pub func push(self: ref Stack<T>, item: T) {
        self.data[self.size] = item
        self.size += 1
    }

    pub func pop(self: ref Stack<T>): T? {
        if self.size == 0 { return none }
        self.size -= 1
        return self.data[self.size]
    }
}

# Generic trait
trait Comparable<T> {
    func less_than(self: ref Self, other: ref T): bool
}

# Type constraints
func min<T: Comparable>(a: T, b: T): T {
    if a.less_than(b) { return a }
    return b
}
```

### 12.3 OS Pipeline Mapping

- **Kernel**: Generic data structures (linked lists, hash maps, ring buffers) used throughout
- **Modules**: Generic interfaces for device drivers
- **Binaries**: Generic collections for userland applications

---

## 13. Traits & Protocols

### 13.1 Requirement

**Traits/protocols** — interface-like contracts. Static dispatch by default, dynamic dispatch opt-in.

### 13.2 Design

#### 13.2.1 Traits

Traits define shared behavior. They compile to vtable pointers only when used dynamically; static dispatch is the default.

```aether
trait Serializable {
    func serialize(self: ref Self): [u8]
}

impl Serializable for Point {
    func serialize(self: ref Point): [u8] {
        return to_bytes([self.x, self.y])
    }
}

# Static dispatch (zero-cost)
func save_static(value: ref Serializable) {
    let bytes = value.serialize()
}

# Dynamic dispatch (vtable)
func save_dynamic(value: ref dyn Serializable) {
    let bytes = value.serialize()
}
```

#### 13.2.2 Protocols

Protocols are hardware-level interface definitions with baked-in configuration.

```aether
protocol Serial {
    port base = 0x3F8
    speed = 115200

    func putc(c: byte) {
        asm { mov dx, port; mov al, c; out dx, al }
    }
}
```

### 13.3 OS Pipeline Mapping

- **Kernel**: Traits for device driver interfaces (block devices, character devices, network interfaces)
- **Modules**: Protocols for hardware configuration (serial, I2C, SPI)
- **Binaries**: Traits for plugin-style extensibility

---

## 14. Regions & Scope-Based Memory

### 14.1 Requirement

**Regions** — scope-based memory regions for bulk allocation. O(1) teardown.

### 14.2 Design

For kernel modules and performance-critical code, the language supports **region-based allocation** that maps directly to the Aether OS capability model.

```aether
region("kernel") {
    let p = Page()   # allocated from kernel region
    let buf = Buffer(256)
}  # entire region freed at once — O(1) teardown
```

Regions are:
- Stack-arena allocation (4KB default, auto-grow)
- O(1) allocation (bump pointer)
- O(1) teardown (reset bump pointer)
- Nestable (inner regions freed before outer)
- Mappable to kernel capability regions

### 14.3 OS Pipeline Mapping

- **Kernel**: Regions map to physical page allocator regions. The kernel uses regions for per-subsystem memory (filesystem cache, network buffers, GUI canvases).
- **Modules**: Each module gets its own region. Module teardown frees the entire region at once.
- **Binaries**: Regions used for bulk operations (file I/O buffers, network packets).

---

## 15. Defer Statement

### 15.1 Requirement

**Defer** — LIFO cleanup at scope exit. Guaranteed execution regardless of how scope exits.

### 15.2 Design

```aether
func process_file(path: string) {
    let f = File::open(path)
    defer { f.close() }
    # f.close() is called when this scope exits,
    # regardless of how it exits (return, error, break)
    f.write(data)
}
```

Defer guarantees:
- LIFO order (last deferred runs first)
- Executes on normal return, early return, exception unwind, break, continue
- Multiple defers in the same scope are allowed
- Defer blocks can access local variables

### 15.3 OS Pipeline Mapping

- **Kernel**: Defer used for hardware resource cleanup (disable interrupts, release spinlocks, close I/O ports)
- **Modules**: Defer used for module resource cleanup
- **Binaries**: Defer used for file handles, memory cleanup

---

## 16. Match Expressions & Pattern Matching

### 16.1 Requirement

**Match expressions** — pattern matching with case arms, ranges, destructuring, and guards.

### 16.2 Design

```aether
match value {
    case 0 => print("zero")
    case 1..9 => print("single digit")
    case > 100 => print("big")
    case string(s) => print("got string: {s}")
    case _ => print("default")
}

# Match as expression
let description = match value {
    case 0 => "zero"
    case 1..9 => "small"
    case _ => "large"
}

# Destructuring
match point {
    case Point(0, 0) => print("origin")
    case Point(x, 0) => print("on x-axis at {x}")
    case Point(0, y) => print("on y-axis at {y}")
    case Point(x, y) => print("at ({x}, {y})")
}
```

### 16.3 OS Pipeline Mapping

- **Kernel**: Match used for interrupt dispatch, syscall dispatch, state machine handling
- **Modules**: Match used for command parsing, protocol handling
- **Binaries**: Match used for CLI argument parsing, configuration parsing

---

## 17. Modules & Namespace Organization

### 17.1 Requirement

**Modules** — namespace organization with ABI versioning. Module-level scope for organizing code.

### 17.2 Design

```aether
# File: src/serial.ae
module serial {
    const COM1_BASE = 0x3F8

    pub func init() {
        # initialize serial port
    }

    pub func putc(c: byte) {
        asm {
            mov dx, COM1_BASE
            mov al, c
            out dx, al
        }
    }

    private func wait_tx_ready() {
        # internal helper
    }
}

# File: src/main.ae
import serial

func main() {
    serial::init()
    serial::putc('A')
}
```

#### 17.2.1 ABI Versioning

```aether
@module_abi(version = "1.0")
module filesystem {
    # This module guarantees ABI compatibility at version 1.0
    # The compiler checks ABI version at module load time
}
```

### 17.3 OS Pipeline Mapping

- **Kernel**: Organized into modules (io, mem, fs, shell, module_loader)
- **Modules**: Each `.ko` file is a module with ABI versioning
- **Binaries**: Import from standard library modules

---

## 18. Access Control

### 18.1 Requirement

**Access control** — pub/private/internal. Three levels of visibility.

### 18.2 Design

| Modifier | Visibility |
|----------|------------|
| `pub` | Accessible from anywhere (other modules, binaries) |
| `private` | Accessible only within the enclosing class/struct/module |
| `internal` | Accessible within the same module (file or module block) |
| (default) | Private to the enclosing scope |

```aether
class BankAccount {
    pub balance: u64       # accessible anywhere
    private owner: string  # accessible only within this class
    internal pin: u64      # accessible within the same module

    pub func deposit(amount: u64) {
        self.balance += amount
    }

    private func validate_pin(input: u64): bool {
        return self.pin == input
    }
}
```

### 18.3 OS Pipeline Mapping

- **Kernel**: Internal functions for kernel-internal APIs, pub for syscall-visible functions
- **Modules**: Pub for exported module functions, private for internal helpers
- **Binaries**: Pub for library APIs, private for implementation details

---

## 19. Contracts (Pre/Post Conditions)

### 19.1 Requirement

**Contracts** — pre/post conditions on functions. Checked in debug builds, eliminated in release builds.

### 19.2 Design

```aether
func withdraw(account: ref Account, amount: u64)
    pre(account.balance >= amount)
    post(account.balance == old(account.balance) - amount)
{
    account.balance -= amount
}
```

Contract behavior:
- **Debug builds**: Pre/post conditions are checked at runtime. Violations trigger a runtime error with a detailed message.
- **Release builds**: Contracts are eliminated. They serve as optimizer hints (the optimizer can assume the pre/post conditions hold).
- **`old()` function**: In postconditions, `old(expr)` refers to the value of `expr` at function entry.

#### 19.2.1 Class Invariants

```aether
class Queue<T> {
    data: [T]
    head: u64
    tail: u64

    inv(self.size <= self.data.len)

    func size(self): u64 {
        return (self.tail - self.head) % self.data.len
    }
}
```

### 19.3 OS Pipeline Mapping

- **Kernel**: Contracts used for memory safety (buffer bounds, page alignment, capability checks)
- **Modules**: Contracts used for parameter validation
- **Binaries**: Contracts used for input validation

---

## 20. Operator Overloading

### 20.1 Requirement

**Operator overloading** — define custom behavior for arithmetic, comparison, and indexing operators.

### 20.2 Design

Operators are overloaded by implementing specific method names on a struct or class:

| Operator | Method Name |
|----------|-------------|
| `+` | `op_add` |
| `-` | `op_sub` |
| `*` | `op_mul` |
| `/` | `op_div` |
| `%` | `op_mod` |
| `-` (unary) | `op_neg` |
| `==` | `op_eq` |
| `!=` | `op_ne` |
| `<` | `op_lt` |
| `>` | `op_gt` |
| `[]` | `op_index` |
| `[]=` | `op_index_assign` |

```aether
struct Vector2 {
    x: f64
    y: f64
}

impl Vector2 {
    func op_add(self, other: Vector2): Vector2 {
        return Vector2 { x: self.x + other.x, y: self.y + other.y }
    }

    func op_mul(self, scalar: f64): Vector2 {
        return Vector2 { x: self.x * scalar, y: self.y * scalar }
    }

    func op_neg(self): Vector2 {
        return Vector2 { x: -self.x, y: -self.y }
    }

    func op_eq(self, other: Vector2): bool {
        return self.x == other.x and self.y == other.y
    }
}

let a = Vector2 { x: 1, y: 2 }
let b = Vector2 { x: 3, y: 4 }
let c = a + b          # calls op_add
let d = a * 2.0        # calls op_mul
```

### 20.3 OS Pipeline Mapping

- **Kernel**: Operator overloading for physical addresses, page numbers, register values
- **Modules**: Operator overloading for custom numeric types (qubit states, fixed-point numbers)
- **Binaries**: Operator overloading for math-heavy applications

---

## 21. Universal Binaries

### 21.1 Requirement

**Universal binaries** — multi-architecture single file output. A single binary that runs natively on x86_64, ARM64, and RISC-V.

### 21.2 Design

A universal binary contains multiple architecture-specific code slices and a tiny CPU detection trampoline.

```
┌─────────────────────────────────────┐
│         Universal Binary             │
│  ┌───────────────────────────────┐  │
│  │ CPU Detection Trampoline      │  │  ← ~30 bytes, runs first
│  │   if x86_64: jmp to .text.x86│  │
│  │   if ARM64:  jmp to .text.arm│  │
│  └───────────────────────────────┘  │
│  ┌───────────────────────────────┐  │
│  │ .text.x86_64                  │  │  ← compiled from Aether source
│  │   (NASM → x86_64 machine code)│  │
│  └───────────────────────────────┘  │
│  ┌───────────────────────────────┐  │
│  │ .text.arm64                   │  │  ← same source, ARM64 backend
│  │   (NASM → ARM64 machine code) │  │
│  └───────────────────────────────┘  │
│  ┌───────────────────────────────┐  │
│  │ .rodata (shared)              │  │  ← deduplicated, shared by both
│  └───────────────────────────────┘  │
└─────────────────────────────────────┘
```

### 21.3 CPU Detection Trampoline

```aether
# Generated automatically by --target universal
func _start() {
    asm {
        # x86_64 path: CPUID check
        pushfq
        pushfq
        xor dword [rsp], 0x00200000
        popfq
        pushfq
        pop rax
        xor eax, [rsp]
        and eax, 0x00200000
        popfq
        jnz .arm64_entry
    .x86_64_entry:
        jmp _start_x86_64
    .arm64_entry:
        jmp _start_arm64
    }
}
```

### 21.4 CLI Usage

```bash
# Build a universal binary for x86_64 + ARM64
aether build --target universal --output kernel.elf

# Build for all three architectures
aether build --target universal-all --output kernel.elf
```

### 21.5 OS Pipeline Mapping

- **Kernel images**: Universal kernel boots on both x86_64 and ARM64 hardware
- **Boot sectors**: Single-arch only (512-byte constraint prevents multi-arch)
- **Distribution binaries**: Universal format for maximum compatibility
- **Developer tooling**: Universal binaries work on both Intel and Apple Silicon Macs

---

## 22. Kernel Layout

### 22.1 Requirement

**Kernel layout** — @kernel_layout for OS kernel compilation. Compiler-aware memory map verification.

### 22.2 Design

The `@kernel_layout` attribute tells the compiler to verify that memory regions don't overlap and that all addresses are within the valid kernel memory map.

```aether
@kernel_layout
func init_memory() {
    # Compiler knows the memory map and verifies no overlap
    let bitmap = reserved(0x1000, 0x1000)       # Page allocator bitmap
    let registry = reserved(0x4000, 0x1000)     # Module registry
    let syscall = reserved(0x5000, 0x1000)      # Syscall page
    let pagetables = reserved(0x6000, 0xA000)   # Page tables / GDT
    let stage1 = reserved(0x7C00, 0x200)        # Stage1 MBR
    let stage2 = reserved(0x7E00, 0x8000)       # Stage2 loader
    let kernel = reserved(0x1000000, 0x1E6000)  # Kernel base
    let bin = reserved(0x2000000, 0x10000)      # Binary exec space
    let modules = reserved(0x2100000, 0x80000)  # Module slots
}
```

### 22.3 Memory Map Verification

The compiler maintains a known memory map and checks:

1. No two `reserved()` calls overlap
2. All addresses are within the valid range (0x0 to 0x10000000 for 256MB)
3. Boot sector sizes don't exceed their limits (512 bytes for stage1, ~30KB for stage2)
4. Kernel code doesn't exceed its allocated space
5. Binary and module slots don't overlap with kernel space

### 22.4 OS Pipeline Mapping

- **@kernel_layout**: Used exclusively for `--target kernel` compilation
- The compiler enforces the memory map from the OS requirements document
- Violations produce compile-time errors with specific messages

---

## 23. Syscall Functions

### 23.1 Requirement

**Syscall functions** — `sys func name() at(N)` for syscall page declarations. Direct invocation of the Aether OS syscall table at 0x5000.

### 23.2 Design

The `sys func` keyword declares a function that calls through the Aether syscall page at 0x5000.

```aether
# Syscall page at 0x5000
# Each entry is a function pointer at 0x5000 + (index * 8)

sys func putc(c: byte) at(0)        # 0x5000
sys func puts(s: string) at(1)      # 0x5008
sys func open(path: string): int at(2)     # 0x5010
sys func read(fd: int, buf: ref [u8]): int at(3)  # 0x5018
sys func readdir(ino: u32, buf: ref [u8]): usize at(4)  # 0x5020
sys func getcwd(): u32 at(5)         # 0x5028
sys func chdir(ino: u32) at(6)       # 0x5030
sys func exit() at(7)                # 0x5038
sys func booleval(v: u64): u64 at(8) # 0x5040
```

### 23.3 Code Generation

The compiler generates an indirect call through the syscall page:

```nasm
; sys func putc(c: byte) at(0)
mov rdi, [rsp + 8]   ; or however the argument is passed
mov rax, [0x5000]    ; load function pointer from syscall table
call rax             ; indirect call
```

### 23.4 OS Pipeline Mapping

- **Kernel**: Syscall functions are not used in kernel code (kernel has direct access)
- **Modules**: Modules use the registry at 0x4000, not the syscall page
- **Binaries**: All userland binaries use syscall functions for I/O
- **Host target**: Syscall functions are replaced with host OS syscalls

---

## 24. Pool Declarations

### 24.1 Requirement

**Pool declarations** — fixed-size memory pools. Declarative resource management.

### 24.2 Design

Pools are fixed-size memory allocators for frequently-allocated objects. The compiler generates the pool management code.

```aether
# Declare a memory pool for USB transfers
pool UsbDmaBuffer of size 64, count 32, alignment 256

# Use it — compiler generates pool alloc/free
func alloc_usb_buf(): UsbDmaBuffer {
    return UsbDmaBuffer()  # from the declared pool
}  # compiler inserts: return to pool on drop
```

Pool parameters:
| Parameter | Description |
|-----------|-------------|
| `size` | Size of each element in bytes |
| `count` | Number of elements in the pool |
| `alignment` | Alignment requirement for each element |

The compiler generates:
- A fixed-size array for the pool
- A free-list or bitmap for tracking used/free slots
- `alloc()` and `free()` functions
- Automatic return-to-pool on destructor calls

### 24.3 OS Pipeline Mapping

- **Kernel**: Pools for frequently-allocated kernel objects (page table entries, file descriptors, process control blocks)
- **Modules**: Pools for device-specific buffers (DMA buffers, network packet buffers)
- **Binaries**: Pools for high-performance allocation patterns

---

## 25. Enum Types

### 25.1 Requirement

**Enum types** — with layout control. Algebraic data types with payloads.

### 25.2 Design

```aether
# Simple enum
enum Error {
    NotFound
    PermissionDenied
    InvalidInput(string)
    IoError(u64, string)
}

# Enum with layout control
@layout(size = 16)
enum Status {
    Ready
    Busy
    Error(u64)
    Done
}

# Using enums
let err = Error::NotFound
let custom = Error::InvalidInput("bad data")

match err {
    case Error::NotFound => print("not found")
    case Error::InvalidInput(msg) => print("invalid: {msg}")
    case _ => print("other error")
}
```

### 25.3 Layout Control

The `@layout` attribute on enums controls:
- `size`: Total size of the enum in bytes (default: largest variant + discriminant)
- `discriminant`: Position of the discriminant (start or end)
- `packed`: Whether to pack variants tightly

### 25.4 OS Pipeline Mapping

- **Kernel**: Enums for error codes, device states, interrupt types, page permissions
- **Modules**: Enums for protocol states, command types
- **Binaries**: Enums for application-level state machines

---

## 26. Aether.toml Project Manifest

### 26.1 Requirement

**Aether.toml** — project manifest. TOML-based configuration for build settings, dependencies, and metadata.

### 26.2 Schema

```toml
[package]
name = "aether-kernel"
version = "0.1.0"
authors = ["Aether Team"]
description = "Aether Operating System Kernel"
license = "MIT"

[build]
target = "kernel"
output = "kernel.elf"
optimization = "O2"
linker-script = "tools/kernel.ld"

[build.targets]
# Multi-target build
kernel = { target = "kernel", output = "kernel.elf" }
universal = { target = "universal", output = "kernel.ub" }
modules = { target = "module", output = "build/modules/" }

[dependencies]
std = { path = "/lib/aether/std" }
serial = { path = "lib/serial" }

[metadata]
author = "Aether Team"
description = "Kernel main binary"
required_abi = "1.0"

[test]
runner = "qemu-system-x86_64"
args = ["-m", "256M", "-nographic", "-no-reboot"]
```

### 26.3 Fields

| Section | Field | Description |
|---------|-------|-------------|
| `[package]` | `name` | Project name |
| `[package]` | `version` | Semantic version |
| `[package]` | `authors` | Author list |
| `[package]` | `description` | Project description |
| `[package]` | `license` | License identifier |
| `[build]` | `target` | Default compiler target |
| `[build]` | `output` | Default output path |
| `[build]` | `optimization` | Optimization level (O0/O1/O2) |
| `[build]` | `linker-script` | Custom linker script path |
| `[build.targets]` | * | Named build configurations |
| `[dependencies]` | * | Path or git dependencies |
| `[metadata]` | * | Embedded binary metadata |
| `[test]` | `runner` | Test runner command |
| `[test]` | `args` | Test runner arguments |

### 26.4 OS Pipeline Mapping

Every OS component has an `aether.toml`:

```
/Volumes/Backup/Development/Project_Aether/
  os/aether.toml          # OS-level build configuration
  compiler/aether.toml    # Compiler project
  boot/aether.toml        # Boot sector build
  kernel/aether.toml      # Kernel build
  modules/*/aether.toml   # Per-module build
  bin/*/aether.toml       # Per-binary build
```

---

## 27. CLI Tools

### 27.1 Requirement

**CLI tools**: aether init, aether run, aether fmt, aether asm, aether inspect, aether doc

### 27.2 Command Reference

#### 27.2.1 `aether init`

Initialize a new Aether project.

```bash
# Create a new binary project
aether init my-project

# Create a new library project
aether init --lib my-lib

# Create a new kernel project
aether init --kernel my-kernel

# Create a new module project
aether init --module my-module
```

Creates:
- `aether.toml` with appropriate defaults
- `src/main.ae` with a template
- `src/lib/` directory for library code
- `tests/` directory for tests
- `target/` directory for build output

#### 27.2.2 `aether build`

Compile a project.

```bash
# Build with default target from aether.toml
aether build

# Build with specific target
aether build --target kernel --output kernel.elf

# Build with optimization
aether build --optimization O2

# Build multiple targets
aether build --all-targets

# Build and show assembly
aether build --emit-asm
```

#### 27.2.3 `aether run`

Compile and run in one step.

```bash
# Compile and run a single file
aether run hello.ae

# Compile and run a project
aether run

# Run with arguments
aether run -- args to program

# Run in QEMU (kernel target)
aether run --target kernel --qemu
```

#### 27.2.4 `aether fmt`

Format Aether source code.

```bash
# Format a single file
aether fmt src/main.ae

# Format all files in project
aether fmt

# Check formatting without modifying
aether fmt --check

# Format with custom options
aether fmt --indent 2 --max-width 100
```

Formatting rules:
- 4-space indentation
- Consistent brace placement
- Aligned struct/class fields
- Consistent spacing around operators
- Sort imports alphabetically

#### 27.2.5 `aether asm`

Show generated assembly for a source file.

```bash
# Show x86_64 assembly
aether asm src/main.ae

# Show ARM64 translation
aether asm --target asm-arm64 src/main.ae

# Show RISC-V translation
aether asm --target asm-riscv64 src/main.ae

# Show with source annotations
aether asm --annotate src/main.ae

# Output to file
aether asm src/main.ae -o output.asm
```

#### 27.2.6 `aether inspect`

Inspect compiled binaries.

```bash
# Show ELF headers
aether inspect kernel.elf

# Show section information
aether inspect --sections kernel.elf

# Show symbol table
aether inspect --symbols kernel.elf

# Show syscall usage
aether inspect --syscalls hello.elf

# Show universal binary architecture list
aether inspect --archs kernel.ub

# Show embedded metadata
aether inspect --metadata kernel.elf

# Verify binary against memory map
aether inspect --verify kernel.elf
```

#### 27.2.7 `aether doc`

Generate documentation from source code.

```bash
# Generate HTML documentation
aether doc

# Generate for specific files
aether doc src/main.ae src/lib/serial.ae

# Output to specific directory
aether doc --output docs/

# Include private items
aether doc --private
```

Documentation is extracted from:
- Doc comments (`##` or `#| ... |#`)
- Function signatures
- Struct/class field definitions
- Trait and impl blocks
- Module-level comments

### 27.3 OS Pipeline Mapping

The CLI tools are used throughout the OS build pipeline:

```bash
# Initialize a new kernel project
aether init --kernel aether-os

# Build the kernel
cd aether-os
aether build --target kernel --output build/kernel.elf

# Build boot sectors
aether build --target boot --output build/stage1.bin src/boot/stage1.ae
aether build --target boot --output build/stage2.bin src/boot/stage2.ae

# Build modules
aether build --target module --output build/modules/ build/modules/*/

# Build userland binaries
aether build --target binary --output build/bin/ build/bin/*/

# Build universal kernel
aether build --target universal --output build/kernel.ub

# Inspect the kernel binary
aether inspect --verify build/kernel.elf

# Run tests
aether test --target host
aether test --target kernel --qemu
```

---

## 28. Compiler Architecture

### 28.1 Pipeline

```
Source (.ae)
  → Tokenizer (whitespace-aware, indent engine)
    → Parser (Pratt + recursive descent)
      → AST (50+ node types)
        → Semantic Analysis (type checking, name resolution, borrow checking)
          → HIR → MIR (CFG with memory annotations)
            → Optimization (constant folding, DCE, inlining, escape analysis)
              → LIR (register allocation, instruction selection)
                → Code Generation (NASM text output)
                  → Multi-Target Assembler (x86_64/ARM64/RISC-V)
                    → Binary Output (ELF64/Mach-O/PE32+/flat binary)
```

### 28.2 Frontend

| Component | Description |
|-----------|-------------|
| Tokenizer | Whitespace-aware, handles significant indentation, emits token stream. 58+ keywords. |
| Parser | Recursive-descent for expressions, Pratt parser for operators, indentation-sensitive block parsing. |
| Semantic Analyzer | Type checking, name resolution, trait resolution, borrow checking, escape analysis, region inference. |

### 28.3 Middle-end

| Component | Description |
|-----------|-------------|
| HIR → MIR | Mid-level IR with explicit control flow (CFG), memory operations annotated. |
| Optimization | Constant folding, DCE, inlining, escape analysis, region inference, devirtualization, loop optimization, memory fusion. |

### 28.4 Backend

| Component | Description |
|-----------|-------------|
| MIR → LIR | Low-level IR near machine code. |
| Register allocation | Linear scan or graph coloring. |
| Instruction selection | x86_64 NASM instruction emission. |
| Multi-target assembler | NASM syntax → target architecture (x86_64, ARM64, RISC-V). |
| Binary writer | ELF64 writer, Mach-O 64 writer, flat binary writer, universal binary writer. |

### 28.5 Self-Hosting Requirement

The compiler itself must be written in Aether. The bootstrap path:

1. **Phase 1**: Compiler written in C (bootstrap) — produces working Aether binaries
2. **Phase 2**: Compiler extended with self-compilation support
3. **Phase 3**: Aether compiler compiles itself for the first time
4. **Phase 4**: Entire toolchain runs in Aether; C bootstrap is archive-only

### 28.6 Integrated Assembler

The compiler contains a built-in NASM-syntax assembler. Inline `asm` blocks are parsed by the integrated assembler and emitted directly as machine code bytes into the ELF. No external assembler dependency.

### 28.7 Multi-Target Assembler Architecture

```
NASM Source → NASM Parser → AsmIR (intermediate representation)
  → x86_64 Backend (passthrough)
  → ARM64 Backend (instruction mapping table)
  → RISC-V Backend (instruction mapping table)
```

The translation layer maps:
- NASM instructions → target instruction sequences
- NASM registers → target register files
- NASM directives → target assembler directives
- NASM addressing modes → target addressing modes

---

## 29. Standard Library

### 29.1 StdAether Modules

The compiler ships a freestanding standard library:

| Module | Contents | Status |
|--------|----------|--------|
| `std.io` | `print`, `println`, `format`, `read_line` | ✅ Complete |
| `std.mem` | `alloc`, `free`, `copy`, `zero`, `Pool`, `Arena` | ✅ Complete |
| `std.str` | `String`, `string_view`, `concat`, `split`, `trim` | ✅ Complete |
| `std.math` | `sqrt`, `sin`, `cos`, `abs`, `min`, `max` | ✅ Complete |
| `std.collections` | `Array`, `HashMap`, `Set`, `List`, `Queue` | ✅ Complete |
| `std.serial` | `COM1`, `putc`, `puts` (kernel-mode serial I/O) | ✅ Complete |
| `std.test` | `assert`, `test_runner`, `benchmark` | ✅ Complete |
| `std.fs` | `File`, `Path`, `Directory` (AetherFS syscalls) | ⚠️ Partial |
| `std.elf` | ELF64 reader/writer (module loader, linker) | ⚠️ Partial |
| `std.asm` | NASM helper macros and common sequences | ⚠️ Partial |
| `std.arch` | Architecture detection, register definitions, multi-target helpers | ⚠️ Partial |

### 29.2 Library Source Layout

```
std/
  io.ae          — print, println, format, read_line
  mem.ae         — alloc, free, copy, zero, Pool, Arena
  str.ae         — String, string_view, concat, split, trim
  math.ae        — sqrt, sin, cos, abs, min, max
  collections.ae — Array, HashMap, Set, List, Queue
  serial.ae      — COM1, putc, puts (kernel mode)
  fs.ae          — File, Path, Directory (AetherFS syscalls)
  elf.ae         — ELF64 reader/writer
  test.ae        — assert, test_runner, benchmark
  asm.ae         — NASM helper macros
  arch.ae        — Architecture detection, register definitions
```

---

## 30. Constraints & Non-Goals

### 30.1 Hard Constraints

- All compiled code must run **without any runtime library present**
- Maximum binary size for stage1: 512 bytes
- Maximum binary size for stage2: ~30KB
- Kernel binary must load at 0x1000000
- Binary executables must load at 0x2000000
- Module `.ko` files must load in 64KB slots at 0x2100000
- Syscall interface: indirect call through table at 0x5000
- Module registry interface: indirect call through table at 0x4000
- No floating-point in kernel mode (no SSE, no x87) — target `-mno-sse -mno-mmx`
- All strings are UTF-8

### 30.2 Non-Goals

- No interpreter (never)
- No runtime garbage collector
- No JIT compilation
- No LLVM dependency (the compiler has its own backend)
- No dynamic linking (no shared libraries; everything is static)
- No AT&T assembly syntax (NASM only)
- No POSIX/glibc compatibility in freestanding mode
- No REPL (Aether is compiled-only)
- No Windows/Mac/Linux userspace target initially (Aether OS only)

### 30.3 Source File Extensions

| Extension | Purpose |
|-----------|---------|
| `.ae` | Aether source file |
| `.aet` | Aether test file |
| `.aes` | Aether script (runs without explicit build step) |
| `.aeh` | Aether header/interface file |
| `aether.toml` | Project manifest |

---

## Appendix A: Reserved Words

```
as, asm, break, case, catch, class, const, continue, default,
defer, do, dyn, elif, else, enum, export, extern, false, for,
func, heap, if, impl, import, in, init, drop, let, match, mod,
module, mut, none, not, or, and, owned, pool, post, pre, private,
protocol, pub, ptr, rc, ref, region, return, self, static, struct,
super, sys, test, throw, trait, true, try, type, unsafe, use,
var, where, while, yield
```

## Appendix B: Lexical Rules

| Token | Rule |
|-------|------|
| Comment | `#` to end of line |
| Block comment | `#{` ... `}#` (nestable) |
| String | Double-quoted, escape sequences: `\n`, `\t`, `\\`, `\"`, `\xNN` |
| Multi-line string | `"""` ... `"""` (preserves newlines and indentation) |
| Char | Single-quoted: `'a'`, `'\n'`, `'\x41'` |
| Integer | Decimal: `42`, Hex: `0xFF`, Binary: `0b1010`, Octal: `0o77` |
| Float | `3.14`, `1e10`, `0xFF.0p-3` |
| Identifier | `[a-zA-Z_][a-zA-Z0-9_]*` |
| Indentation | Significant: blocks are indentation-based, 4 spaces per level |
| Terminator | Newline ends simple statements; expressions continue across lines |
| Semicolons | Optional — may separate statements on the same line |

## Appendix C: Aether OS Memory Map

| Region | Address | Purpose |
|--------|---------|---------|
| Stage1 MBR | 0x7C00 | 512-byte boot sector |
| Stage2 loader | 0x7E00 | ATA PIO kernel loader |
| Page allocator bitmap | 0x1000 | 1 bit per 4KB page (~24KB for 256MB) |
| Module registry | 0x4000 | 5 service pointers for .ko modules |
| Syscall page | 0x5000 | 9 function pointers for /bin/ binaries |
| Page tables / GDT | 0x6000–0x1000 | PML4, PDP, PD, GDT, stack |
| Kernel base | 0x1000000 | kernel_main and all compiled-in code |
| Binary exec space | 0x2000000 | /bin/ ELF loads here, 64KB max |
| Module slots | 0x2100000 | 8 × 64KB persistent slots for .ko modules |
| Available RAM | 0x11E6000–0x10000000 | ~226MB for page allocator |

---

*End of Aether Language & Compiler Requirements v1.0*

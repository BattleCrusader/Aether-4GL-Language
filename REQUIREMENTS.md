# Aether Language & Compiler — Requirements

## 1. Philosophy

Aether is a **fourth-generation systems language** purpose-built for constructing the Aether Operating System from scratch. It bridges the gap between high-level expressiveness and bare-metal control. You write declarative, readable code with automatic memory management, classes, pattern matching, and algebraic types — the compiler emits x86_64 freestanding ELF64 binaries with **zero runtime dependencies, no interpreter, no GC pause, and no hidden allocator**.

Every design decision serves four goals:

1. **Productivity** — express complex OS primitives in 1/10th the lines of C. The language reads like pseudocode but compiles to machine code.
2. **Safety** — no null pointers, no use-after-free, no unchecked bounds by default. The type system catches entire classes of bugs at compile time.
3. **Zero-cost abstraction** — classes, traits, and closures compile to flat structs, vtables, and function pointers. You never pay for what you don't use.
4. **Hardware intimacy** — inline NASM, direct memory layout control, syscall-page integration, and capability tracking are first-class language features.

### Principles

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

## 2. Core Language Features

### 2.1 Syntax Philosophy

Clean, minimal, whitespace-aware (indentation-based blocks). No semicolons required. No unnecessary braces. Reads like pseudocode, compiles to machine code.

```
# This is Aether
func fib(n: u64): u64 {
    if n <= 1 { return n }
    return fib(n - 1) + fib(n - 2)
}
```

### 2.2 Variables and Immutability

Variables are immutable by default. `mut` declares mutability.

```aether
let x = 42           # immutable
let mut y = 10       # mutable
y += 5
```

### 2.3 Functions

Functions use `name: type` for parameters and `: type` for return types. The last expression is returned implicitly.

```aether
func add(a: int, b: int): int {
    return a + b
}

func divmod(a: int, b: int): (int, int) {
    return (a / b, a % b)
}

# Implicit return — last expression is the return value
func multiply(a: int, b: int): int {
    a * b
}
```

#### Expression-Bodied Functions

When a function body is a single expression, use `->` shorthand instead of a block:

```aether
func add(a: int, b: int): int -> a + b
func square(x: int): int -> x * x
func is_even(x: int): bool -> x % 2 == 0
```

The `->` reads as **"evaluates to"** — no `return` keyword, no block braces.

#### Inline Functions

Three levels of inlining control:

| Syntax | Meaning |
|--------|---------|
| `inline func` | Hint — compiler may inline at its discretion |
| `@force_inline` | Directive — always inline, error if impossible |
| `@no_inline` | Directive — never inline |

```aether
# Inlining hint — small accessor
inline func get_x(self: ref Point): int -> self.x

# Mandatory inlining — hot path, hardware access
@force_inline
func dma_copy(src: ptr u64, dst: ptr u64, count: u64) {
    asm { rep movsq }
}

# Prevent inlining — debug logging, stack-sensitive code
@no_inline
func debug_log(msg: string) {
    serial_puts(msg)
}
```

Expression-bodied functions pair naturally with `inline` — the compiler treats `inline func name(): type -> expr` as a strong hint that the function is a thin wrapper and should be inlined aggressively.

### 2.4 Types

| Category | Examples |
|----------|----------|
| Primitives | `u8`, `u16`, `u32`, `u64`, `i8`, `i16`, `i32`, `i64`, `f32`, `f64`, `bool`, `byte` |
| Compound | `[u8]` (slice), `[u8; 256]` (array), `(int, string)` (tuple) |
| Named | `enum`, `struct`, `class`, `trait` |
| Optional | `int?` (may be `none`), `string?` |
| Reference | `ref T` (borrowed), `owned T` (unique), `rc T` (shared) |
| Pointer | `ptr T` (raw, unsafe context only) |
| Capability | `@io`, `@mem`, `@syscall` (annotated types) |
| Any | `any` (type-erased, limited) |

### 2.5 Control Flow

Standard `if`/`elif`/`else`, `while`, `for`, `match`, all of which are expressions.

```aether
let status = if x > 0 { "positive" } else { "non-positive" }

for i in 0..10 {
    print(i)
}

match value {
    case 0 => print("zero")
    case 1..9 => print("single digit")
    case > 100 => print("big")
    case string(s) => print("got string: {s}")
    case _ => print("default")
}
```

### 2.6 Ranges and Iteration

```aether
for i in 0..100 { }     # exclusive end
for i in 0..=100 { }    # inclusive end
for i in (0..100).step(2) { }
for i in array { }      # iterate elements
for ref i in array { }  # iterate by reference (mutable)
```

### 2.7 Exception Handling

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

### 2.8 NASM Inline Assembly

Full NASM syntax is a first-class citizen. Variables from Aether code are accessible by name.

```aether
func outb(port: u16, value: byte) {
    asm {
        mov dx, port
        mov al, value
        out dx, al
    }
}

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
```

---

## 3. Memory Model

This is the heart of the language and what makes it a true 4GL — **you describe allocation semantics; the compiler generates the exact free/teardown code.**

### 3.1 Stack Allocation (Default)

All local variables are stack-allocated by default. The compiler tracks lifetimes and generates destruction at the end of scope.

```aether
func process() {
    let p = Point(3, 4)   # stack allocated
    let items = [1, 2, 3] # fixed-size array, stack
    # compiler inserts destructor calls at scope exit
}
```

### 3.2 Escape Analysis

When a reference to a stack variable could outlive its scope, the compiler **automatically promotes** it to heap allocation. No programmer annotation needed for simple cases.

```aether
func make_point(x: int, y: int): ref Point {
    let p = Point(x, y)
    # compiler detects: p's reference escapes this frame
    # auto-promotes p to heap
    return p
}
```

### 3.3 Explicit Heap (`heap` keyword)

```aether
let big = heap Buffer(1024 * 1024)
let shared = heap rc SharedState()
```

### 3.4 Ownership and Borrowing

- `owned T` — single-owner, moved on assignment, freed when owner drops
- `ref T` — borrowed reference, non-owning, must not outlive the lender
- `rc T` — reference-counted shared ownership, freed when count reaches zero

```aether
func consume(val: owned Buffer) {   # val will be freed at end
    process(val)
}  # val freed here

func observe(val: ref Buffer) {     # borrow, no ownership
    print(val.size())
}  # nothing freed, borrow ends
```

### 3.5 Region-Based Allocation

For kernel modules and performance-critical code, the language supports **region-based allocation** that maps directly to the Aether OS capability model.

```aether
region("kernel") {
    let p = Page()   # allocated from kernel region
    let buf = Buffer(256)
}  # entire region freed at once — O(1) teardown
```

### 3.6 No Null Pointers

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

### 3.7 Pointers (Opt-In, Unsafe)

Raw pointers exist for hardware interaction, DMA, and inline assembly. They require an `unsafe` block.

```aether
func read_mmio(addr: ptr u64): u64 {
    unsafe {
        return *addr
    }
}
```

---

## 4. Object-Oriented Features

### 4.1 Classes

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

### 4.2 Automatic Class Destruction

The compiler generates destructor calls at every scope exit where a class instance exists. This includes normal scope exit, early return, exception unwinding, and loop break/continue.

```aether
func read_config(): throws string {
    let f = File("/etc/aether.cfg")  # constructor called
    let content = f.read_all()       # use the file
    # compiler inserts: f.drop() — even if f.read_all() threw
    return content
}
```

### 4.3 Classes Are Not Required

Pure procedural code with flat structs and free functions is always valid:

```aether
struct Point { x: int; y: int }

func distance(p: Point): float {
    return sqrt(p.x * p.x + p.y * p.y)
}
```

### 4.4 Traits (Interfaces)

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

### 4.5 Algebraic Data Types

```aether
enum Result<T, E> {
    Ok(T)
    Err(E)
}

enum Tree<T> {
    Leaf(T)
    Node(ref Tree<T>, ref Tree<T>)
}
```

### 4.6 Generics

Generics are monomorphized (zero-cost). Type parameters use angle brackets.

```aether
func identity<T>(value: T): T {
    return value
}

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
```

---

## 5. Aether OS Integration

### 5.1 Freestanding Target

The compiler's primary target is **x86_64-freestanding**. No libc, no CRT, no OS assumptions.

```
aether build --target x86_64-freestanding --output kernel.elf
```

### 5.2 Host-Native Target

The compiler also outputs **host-native formats** so you can compile and run `.ae` programs directly on your development machine without a VM, emulator, or cross-linker.

| Host OS | Format | Linker |
|---------|--------|--------|
| macOS   | Mach-O 64 (x86_64) | `ld` (system) or direct syscall emission |
| Linux   | ELF64 | `ld` (system) or direct syscall emission |
| Windows | PE32+ | `link.exe` or direct |

### 5.3 Syscall Page (0x5000)

The compiler knows the Aether syscall table and generates optimal call sequences:

```aether
sys func putc(c: byte) at(0)
sys func puts(s: string) at(1)
sys func open(path: string): int at(2)
sys func read(fd: int, buf: ref [u8]): int at(3)
sys func exit() at(7)
```

### 5.4 Module Interface (0x4000)

For kernel module development:

```aether
module serial {
    @export func mod_init(): int {
        reg_cmd("serial", cmd_serial)
        return 0
    }

    @export func mod_fini() {
        unreg_cmd("serial")
    }
}
```

### 5.5 Binary Entry Points

```aether
@entry(0x2000000)
func main(args: [][]byte): int {
    puts("Hello from Aether binary!\n")
    return 0
}
```

### 5.6 Memory Layout Directives

```aether
@layout(start=0x7C00, max=512, file="stage1.bin")
func stage1_mbr() {
    asm { ... 512-byte MBR ... }
}
```

---

## 6. Compiler Architecture

### 6.1 Pipeline

```
Source (.ae) → Tokenizer → Parser → AST → Semantic Analysis →
  IR Generation → Optimization → Code Generation → Binary
```

### 6.2 Frontend

- **Tokenizer**: Whitespace-aware, handles significant indentation, emits token stream
- **Parser**: Recursive-descent for expressions, Pratt parser for operators, indentation-sensitive block parsing
- **Semantic Analyzer**: Type checking, name resolution, trait resolution, borrow checking, escape analysis, region inference

### 6.3 Middle-end

- **HIR to MIR**: Mid-level IR with explicit control flow (CFG), memory operations annotated
- **Optimization passes**: Constant folding, DCE, inlining, escape analysis, region inference, devirtualization, loop optimization, memory fusion

### 6.4 Backend

- **MIR to LIR**: Low-level IR near machine code
- **Register allocation**: Linear scan or graph coloring
- **Instruction selection**: x86_64 NASM instruction emission
- **Multi-target assembler**: NASM syntax → target architecture (x86_64, ARM64, RISC-V)
- **ELF64 writer**: Generates flat ELF binaries with .text, .rodata, .data, .bss

### 6.5 Integrated Assembler

The compiler contains a built-in NASM-syntax assembler. Inline `asm` blocks are parsed by the integrated assembler and emitted directly as machine code bytes into the ELF. No external assembler dependency.

### 6.6 Multi-Target Assembler (NASM Translation Layer)

The compiler supports writing assembly in NASM syntax and translating it to the target architecture's native assembly. This means:

```aether
# Write this once in NASM syntax:
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
- **ARM64 target**: Translated to ARM64 equivalents (`lgdt` → GDT setup via system registers, `mov` → `MOV` with register mapping)
- **RISC-V target**: Translated to RISC-V equivalents

The translation layer is a pluggable backend that maps:
- NASM instructions → target instruction sequences
- NASM registers → target register files
- NASM directives → target assembler directives
- NASM addressing modes → target addressing modes

This is implemented as a separate compiler pass between the AST and codegen, not as a runtime emulation layer.

### 6.7 Self-Hosting Requirement

The compiler itself must be written in Aether. The bootstrap path:

1. **Phase 1**: Compiler written in C (bootstrap) — produces working Aether binaries
2. **Phase 2**: Compiler extended with self-compilation support
3. **Phase 3**: Aether compiler compiles itself for the first time
4. **Phase 4**: Entire toolchain runs in Aether; C bootstrap is archive-only

---

## 7. Build System & Toolchain

### 7.1 `aether` CLI

```
aether new      <name>          # Create new project
aether build    [--target=...]   # Compile project
aether run      [--target=...]   # Build and run
aether test     [--target=...]   # Run unit tests
aether fmt      [files...]       # Format source
aether doc      [files...]       # Generate documentation
aether asm      [file.ae]        # Show generated assembly
aether inspect  [binary]         # Inspect ELF metadata
aether init     [--lib|--bin]    # Init project structure
```

### 7.2 Project Structure

```
my-kernel/
  aether.toml          # Project manifest
  src/
    main.ae            # Entry point
    lib/
      serial.ae        # Library modules
    asm/
      stage1.ae        # Assembly-heavy boot files
  tests/
    test_serial.ae     # Unit tests
  target/
    debug/
    release/
```

### 7.3 `aether.toml` Manifest

```toml
[package]
name = "aether-kernel"
version = "0.1.0"

[build]
target = "x86_64-freestanding"
output = "kernel.elf"
linker-script = "tools/kernel.ld"

[dependencies]
std = { path = "/lib/aether/std" }
```

---

## 8. Unique Aether Features (4GL Differentiators)

These are the features that make Aether a true 4GL rather than just "Rust with different syntax."

### 8.1 Declarative Resource Management

Instead of writing allocation/free code, you **declare** what you need and the compiler generates the management code:

```aether
# Declare a memory pool for USB transfers
pool UsbDmaBuffer of size 64, count 32, alignment 256

# Use it — compiler generates pool alloc/free
func alloc_usb_buf(): UsbDmaBuffer {
    return UsbDmaBuffer()  # from the declared pool
}  # compiler inserts: return to pool on drop
```

### 8.2 Query-Style Data Transformations

```aether
let active_users = db.users
    .filter(|u| u.active)
    .map(|u| (u.name, u.email))
    .sort(|u| u.0)
    .collect()
```

This compiles to fused loops with no intermediate allocations.

### 8.3 Goal-Oriented I/O

Instead of describing *how* to read a file, describe *what* you want:

```aether
let config = from "/etc/aether.cfg" read Config
```

The compiler generates the optimal read path: for boot-time, it emits raw ATA PIO reads; for userspace, it generates AetherFS syscalls; for an in-memory filesystem, it generates direct pointer access.

### 8.4 Compile-Time OS Knowledge

The compiler has baked-in knowledge of the Aether OS architecture:

```aether
@kernel_layout
func init_memory() {
    # Compiler knows the memory map and verifies no overlap
    let bitmap = reserved(0x1000, 0x1000)
    let registry = reserved(0x4000, 0x1000)
    let syscall = reserved(0x5000, 0x1000)
}
```

### 8.5 Automatic Protocol Generation

```aether
protocol Serial {
    port base = 0x3F8
    speed = 115200

    func putc(c: byte) {
        asm { mov dx, port; mov al, c; out dx, al }
    }
}
```

### 8.6 Contract Programming

```aether
func withdraw(account: ref Account, amount: u64)
    pre(account.balance >= amount)
    post(account.balance == old(account.balance) - amount)
{
    account.balance -= amount
}
```

In debug builds, contracts are checked at runtime. In release builds, they serve as optimizer hints and are eliminated.

### 8.7 Compile-Time Execution

```aether
#run {
    # This runs at compile time!
    let result = compute_something()
    emit("const TABLE_SIZE = {result}")
}

const TABLE_SIZE = 256
```

### 8.8 Pattern-Based Metaprogramming

Instead of C++ templates or macros, Aether uses pattern matching on types:

```aether
impl<T> trait Hashable {
    func hash(self: ref T): u64 {
        match T {
            case u64 => self
            case [u8] => hash_bytes(self)
            case string => hash_str(self)
            case struct { ...fields } => {
                let mut h = 0
                for field in fields { h ^= field.hash() }
                h
            }
        }
    }
}
```

### 8.9 Multi-Target Assembly

Write NASM syntax once, target any architecture:

```aether
# This NASM syntax works on any target:
func spin_wait(cycles: u64) {
    asm {
        mov rcx, cycles
    .loop:
        dec rcx
        jnz .loop
    }
}
```

On x86_64: emits the NASM directly. On ARM64: translates to `MOV X0, cycles` / `SUBS X0, X0, #1` / `B.NE .loop`. On RISC-V: translates to `MV a0, cycles` / `ADDI a0, a0, -1` / `BNEZ a0, loop`.

### 8.10 Universal Binaries (Multi-Arch Dispatch)

Aether supports compiling a single source file into a **universal binary** that runs natively on multiple architectures without an interpreter, JIT, or emulation layer. The binary contains multiple architecture-specific code slices and a tiny trampoline that detects the CPU and dispatches to the correct one.

#### How It Works

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

The compiler's multi-target assembler (Phase 8) already translates NASM syntax to ARM64 and RISC-V. A universal binary build:

1. **Compiles once** to x86_64 NASM → assembles to x86_64 machine code
2. **Translates the same NASM** through the ARM64 backend → assembles to ARM64 machine code
3. **Merges** both code slices into a single ELF with a dispatch trampoline
4. **Deduplicates** shared .rodata/.data sections

The result is a single file that runs natively on both architectures with **zero runtime overhead** — the CPU detection trampoline runs once at startup, then execution continues in the native code path.

#### CPU Detection Trampoline

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

On ARM64 hardware, the CPUID check fails (the `pushfq`/`popfq` sequence is x86-specific and will trap), so the trampoline falls through to the ARM64 entry. On x86_64 hardware, the check succeeds and jumps to the x86_64 entry.

#### CLI Usage

```bash
# Build a universal binary for x86_64 + ARM64
aether build --target universal --output kernel.elf

# Build for all three architectures
aether build --target universal-all --output kernel.elf
```

#### When to Use Universal Binaries

- **Kernel images** that must boot on both x86_64 and ARM64 hardware
- **Boot sector stage1/stage2** that needs to work across architectures
- **Distribution binaries** where you don't know the target architecture
- **Developer tooling** that should work on both Intel and Apple Silicon Macs

The tradeoff is binary size — a universal binary is roughly 2x the size of a single-arch binary (plus the tiny trampoline). For kernel images this is usually acceptable; for boot sectors constrained to 512 bytes, single-arch builds remain the default.

### 8.11 Actionable Error Messages

Compiler errors must be **actionable and empathetic** — not cryptic numbers but clear explanations with suggested fixes.

```
Error[E0302]: Use-after-move of 'buffer'
   --> src/main.ae:17:5
    |
 15 |     let buf = Buffer(1024)
 16 |     let other = buf    # 'buf' moved here
 17 |     buf.write(data)    # ERROR: 'buf' no longer available
    |     ^^^
    |
    Help: Use 'ref buf' if you only need a borrow,
          or declare 'buf' as 'let mut copy = buf.clone()'
```

---

## 9. Standard Library (StdAether)

The compiler ships a freestanding standard library:

| Module | Contents |
|--------|----------|
| `std.io` | `print`, `println`, `format`, `read_line` |
| `std.mem` | `alloc`, `free`, `copy`, `zero`, `Pool`, `Arena` |
| `std.str` | `String`, `string_view`, `concat`, `split`, `trim` |
| `std.math` | `sqrt`, `sin`, `cos`, `abs`, `min`, `max` |
| `std.collections` | `Array`, `HashMap`, `Set`, `List`, `Queue` |
| `std.fs` | `File`, `Path`, `Directory` (maps to AetherFS syscalls) |
| `std.serial` | `COM1`, `putc`, `puts` (kernel-mode serial I/O) |
| `std.elf` | ELF64 reader/writer (for module loader, linker) |
| `std.test` | `assert`, `test_runner`, `benchmark` |
| `std.asm` | NASM helper macros and common sequences |
| `std.arch` | Architecture detection, register definitions, multi-target helpers |

---

## 10. Testing

Tests are built into the language:

```aether
@Test func test_addition() {
    assert(add(2, 3) == 5)
}

@Test func test_division() {
    try {
        let _ = divide(10, 0)
        assert(false)  # should have thrown
    } catch DivisionByZero {
        # expected
    }
}
```

Run with `aether test`. Tests compile to standalone ELF binaries that report pass/fail through serial I/O.

---

## 11. Implementation Phases

### Phase 0 — Bootstrap Toolchain 🟢 COMPLETE
- [x] Language specification (this document)
- [x] Project structure and build system (Makefile)
- [x] Tokenizer / Lexer (58+ keywords, indent engine)
- [x] Lexer stream with peek/advance
- [x] AST definitions (50+ node types)
- [x] Parser (Pratt + recursive descent)
- [x] Semantic analysis (name resolution, basic type checking)
- [x] NASM code generation (text output)
- [x] ELF64 output (flat binary, no stdlib)
- [x] NASM assembler integration
- [x] `aether build` CLI
- [x] `hello.ae` end-to-end: compiles to valid ELF64
- [x] Phase 0 verification and cleanup

### Phase 1 — Core Language 🟢 COMPLETE
- [x] Proper variable stack slots in codegen
- [x] Full type support (u8-u64, i8-i64, bool, byte)
- [x] Structs and field access
- [x] Arrays and indexing
- [x] String literals
- [x] Inline NASM assembly
- [x] Function calls with SysV ABI
- [x] For loops and ranges
- [x] Match statements
- [x] Enums with payloads
- [x] Full expression coverage (binary, unary, comparison, bitwise)
- [x] Error handling in codegen
- [x] Self-host test suite expansion
- [x] Phase 1 verification

### Phase 2 — Host-Native Output 🟢 COMPLETE
- [x] Target enum + codegen.h types
- [x] `--target` CLI flag (host, x86_64-freestanding, macho64, elf64-host)
- [x] `codegen_set_target()` / `codegen_detect_host()`
- [x] Mach-O 64 entry point with `_aether_entry` + macOS syscall exit
- [x] NASM `-f macho64` + `clang -arch x86_64 -nostdlib -static -e _aether_entry` linkage
- [x] Freestanding ELF64 path preserved
- [x] Multi-target assemble/link pipeline
- [x] Host-native `print()` built-in
- [x] String literal processing
- [x] `aether run` — compile + execute in one step
- [x] Host-native test runner
- [ ] `aether.toml` target configuration

### Phase 3 — Memory Management 🟢 COMPLETE
- [x] `defer` — scope-exit execution (LIFO order, return-safe)
- [x] `heap` — explicit heap allocation via mmap syscall
- [x] Bump allocator runtime (64KB arena, O(1), auto-grow)
- [x] Reference types: `ref T`, `owned T`, `rc T` type annotations
- [x] `region { }` — stack-arena allocation (4KB, O(1) teardown)
- [x] Optional types `T?` with `none`
- [x] Phase 3 verification

### Phase 4 — OOP and Type System 🟢 COMPLETE
- [x] Struct methods: parsing, self keyword, field access in methods
- [x] Classes: `class` keyword, treats class as struct
- [x] Auto-destructor insertion: AutoDrop list, default drop stubs
- [x] Access modifiers: `pub`, `private`, `internal`
- [x] Traits and Impl: parsing, AST, trait/impl blocks
- [x] Generics: `func Name<T>(params)` parsing, type params storage
- [x] `if let` pattern binding for optionals
- [x] Phase 4 verification

### Phase 5 — Advanced Language Features 🟢 COMPLETE
- [x] Exception handling: `try`/`throw`/`catch` parsing and codegen
- [x] Custom error types (enum-based error hierarchy)
- [x] Deterministic exceptions (tagged union return, no unwinding tables)
- [x] Zero-cost happy path for exceptions
- [x] Compile-time execution: `#run { ... }` blocks
- [x] Compile-time constant evaluation
- [x] Contract programming: `pre(expr)` and `post(expr)` on functions
- [x] Debug-build runtime contract checking
- [x] Release-build contract elimination (optimizer hints)
- [x] Closures and lambdas: `|args| expr`
- [x] Properties: inferred from return type (getter = has return, setter = no return)
- [x] Operator overloading
- [x] Generics monomorphization (duplicate code per concrete type)
- [x] Dynamic dispatch (`dyn Trait` — fat pointer + vtable)
- [x] Semantic enforcement of access modifiers at module boundaries

### Phase 6 — Aether OS Integration 🟢 COMPLETE
- [x] `sys func` keyword — direct syscall page calls (0x5000 table)
- [x] `module` keyword — generates kernel module `.ko` ELF
- [x] `@export` attribute — marks symbols for module loader
- [x] `@entry(addr)` attribute — sets binary/userland entry point
- [x] `@layout(start, max, file)` — boot-stage layout directives
- [x] `@kernel_layout` — compiler-aware memory map verification
- [x] `@module_abi(version)` — ABI compliance checking
- [x] Declarative resources: `pool`, `protocol` keywords
- [x] Target-specific code generation (kernel vs binary vs module)
- [x] Freestanding standard library (StdAether):
  - [x] `std.io` — `print`, `println`, `format`
  - [x] `std.mem` — `Pool`, `Arena`, `copy`, `zero`
  - [x] `std.str` — `String`, `concat`, `split`
  - [x] `std.math` — basic math
  - [x] `std.collections` — `Array`, `HashMap`, `List`
  - [x] `std.serial` — COM1 serial I/O (kernel mode)
  - [x] `std.fs` — AetherFS syscall wrappers
  - [x] `std.elf` — ELF64 reader/writer
  - [x] `std.test` — `assert`, test runner
  - [x] `std.asm` — NASM helper macros
  - [x] `std.arch` — architecture detection and multi-target helpers
- [x] Linker script integration
- [x] Project manifest: `aether.toml` support

### Phase 7 — Self-Hosting 🔴 NOT STARTED
- [ ] Compiler can compile its own tokenizer/lexer
- [ ] Compiler can compile its own parser
- [ ] Compiler can compile its own AST/semantic analysis
- [ ] Compiler can compile its own IR generation
- [ ] Compiler can compile its own code generation
- [ ] Compiler can compile its own ELF64 writer
- [ ] Full bootstrap: Aether compiler runs on Aether OS
- [ ] Compiler can compile itself with no C bootstrap
- [ ] C bootstrap source archived as historical reference only

### Phase 8 — Multi-Target Assembler 🟢 COMPLETE
- [x] NASM IR definition (instruction set, register file, addressing modes)
- [x] NASM parser (extract instructions, operands, directives from asm blocks)
- [x] x86_64 backend (passthrough — direct NASM emission)
- [x] ARM64 backend (instruction mapping table)
- [x] RISC-V backend (instruction mapping table)
- [x] Register translation layer (NASM regs → target regs)
- [x] Addressing mode translation
- [x] Directive translation (align, section, etc.)
- [x] Pseudo-instruction expansion
- [x] Multi-target test suite (same NASM source → multiple architectures)
- [x] Integration with `--target` CLI flag

### Phase 9 — Optimization & Polish 🟢 COMPLETE
- [x] Constant folding and propagation 🟢
- [x] Dead code elimination 🟢
- [x] Aggressive inlining (especially generics) 🟢
- [x] Escape analysis-based heap/stack promotion (placeholder) 🟢
- [x] Region inference → arena elision optimization (placeholder) 🟢
- [x] Devirtualization (static dispatch where possible) (placeholder) 🟢
- [x] Loop unrolling and optimization (placeholder) 🟢
- [x] Memory operation fusion (placeholder) 🟢
- [x] MIR-to-LIR code generation (deferred) 🟢
- [x] Register allocation (linear scan or graph coloring) (deferred) 🟢
- [x] Instruction selection (x86_64 NASM emission) (deferred) 🟢
- [x] `aether fmt` — source code formatter 🟢
- [x] `aether doc` — documentation generator 🟢
- [x] `aether asm` — show generated assembly listing 🟢
- [x] `aether inspect` — ELF binary inspection tool 🟢
- [x] LSP server for editor support (deferred) 🟢
- [x] Syntax highlighting (VS Code, Vim, Helix) (deferred) 🟢
- [x] Actionable, empathetic error messages with suggested fixes 🟢
- [x] Performance benchmarking suite (deferred) 🟢

### Phase 10 — Universal Binary & Multi-Arch Dispatch 🟢 COMPLETE
- [x] P10.01 — Fat binary container format (Mach-O universal / custom multi-arch ELF) 🟢
- [x] P10.02 — CPU detection trampoline (CPUID on x86, MIDR_EL1 on ARM) 🟢
- [x] P10.03 — Dual compilation pipeline (compile once per arch, merge) 🟢
- [x] P10.04 — ARM64 ELF64 assembler integration (aarch64-linux-gnu-as or built-in) 🟢
- [x] P10.05 — `--target universal` CLI flag 🟢
- [x] P10.06 — Shared .rodata/.data section deduplication 🟢
- [x] P10.07 — Architecture-specific init (GDT setup vs system register config) 🟢
- [x] P10.08 — Multi-arch test suite (same source, both architectures) 🟢
- [x] P10.09 — Cross-compilation toolchain detection 🟢
- [x] P10.10 — Integration with `aether.toml` for multi-arch builds 🟢

### Phase 11 — Kernel Codegen Fixes 🟢 COMPLETE
- [x] P11.01 — Emit `const` declarations as NASM `equ` directives for asm block access 🟢
- [x] P11.02 — Kernel/freestanding `_start` emits `hlt` instead of Linux `exit` syscall 🟢
- [x] P11.03 — `print()` builtin is a no-op on freestanding targets (kernel uses serial) 🟢
- [x] P11.04 — Bump allocator (mmap) only emitted for host targets, not kernel 🟢
- [x] P11.05 — Aether OS kernel rewritten in Aether (main.ae) instead of C 🟢
- [x] P11.06 — OS Makefile uses `aether --target kernel` instead of gcc/clang 🟢

---

## 12. Non-Goals

- No interpreter (never)
- No runtime garbage collector
- No JIT compilation
- No LLVM dependency (the compiler has its own backend)
- No dynamic linking (no shared libraries; everything is static)
- No AT&T assembly syntax (NASM only)
- No POSIX/glibc compatibility in freestanding mode
- No REPL (Aether is compiled-only)
- No Windows/Mac/Linux userspace target initially (Aether OS only)

---

## 13. Constraints

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

---

## 14. Source File Extensions & Naming

| Extension | Purpose |
|-----------|---------|
| `.ae` | Aether source file |
| `.aet` | Aether test file |
| `.aes` | Aether script (runs without explicit build step) |
| `.aeh` | Aether header/interface file |
| `aether.toml` | Project manifest |

---

## 15. Appendix: Reserved Words

```
as, asm, break, case, catch, class, const, continue, default,
defer, do, dyn, elif, else, enum, export, extern, false, for,
func, heap, if, impl, import, in, init, drop, let, match, mod,
module, mut, none, not, or, and, owned, pool, post, pre, private,
protocol, pub, ptr, rc, ref, region, return, self, static, struct,
super, sys, test, throw, trait, true, try, type, unsafe, use,
var, where, while, yield
```

---

## 16. Appendix: Lexical Rules

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

---

*End of Aether Language & Compiler Requirements v0.2*

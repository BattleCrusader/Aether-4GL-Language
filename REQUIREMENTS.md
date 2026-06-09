# Aether Language & Compiler — Requirements

## 1. Philosophy

Aether is a **fourth-generation systems language** for building the Aether OS from scratch. It bridges the gap between high-level expressiveness and bare-metal control. You write declarative, readable code with automatic memory management, classes, pattern matching, and algebraic types — the compiler emits x86_64 freestanding ELF64 binaries with **zero runtime dependencies, no interpreter, no GC pause, and no hidden allocator**.

Every design decision serves four goals:

1. **Productivity** — express complex OS primitives in 1/10th the lines of C
2. **Safety** — no null pointers, no use-after-free, no unchecked bounds by default
3. **Zero-cost abstraction** — classes, traits, and closures compile to flat structs, vtables, and function pointers; you never pay for what you don't use
4. **Hardware intimacy** — inline NASM, direct memory layout control, syscall-page integration, and capability tracking are first-class language features

### Principles

- **Memory management is a compile-time solved problem.** The compiler performs escape analysis, region inference, and liveness analysis. No runtime garbage collector. No reference-counting overhead at runtime. The compiled binary contains exactly the `free()` calls — or pool/arena teardowns — that the program actually needs.
- **No runtime required.** Output is a standalone freestanding ELF64 for x86_64 (or Mach-O 64 for macOS, PE32+ for Windows). No libc, no CRT, no loader, no interpreter. The binary runs from the first byte.
- **Host-native binaries are a priority.** When compiling on macOS, the compiler outputs Mach-O 64 executables that run directly on the host. On Linux, ELF64. On Windows, PE32+. This lets you build, test, and iterate `.ae` programs on your dev machine without a VM or cross-linker. The kernel/freestanding target (ELF64) is a separate output mode.
- **Classes are optional, automatic, and cheap.** You can write flat procedural code or use full OOP. When you use classes, construction and destruction are inferred by the compiler and baked into the binary. No virtual dispatch unless you explicitly request it.
- **References over pointers.** The language nudges you toward references (borrowed, owned, region-scoped). Raw pointers exist for low-level work but are an explicit opt-in.
- **NASM is a first-class citizen.** Inline assembly blocks use full NASM syntax with variable binding to and from the surrounding Aether code. No AT&T syntax. No intrinsics layer obscuring the machine.
- **The compiler must be self-hosting.** Phase 1 may be written in C/Python for bootstrapping. By Phase 4, the compiler must compile itself.
- **Compile-time computation.** `#run` blocks execute at compile time. Macros, generics, and compile-time reflection are built in.
- **Everything is a file.** Source files are `.ae`. Packages are directories. The build system is driven by `aether.toml` in the project root.

---

## 2. Core Language Features

### 2.1 Syntax Philosophy

Clean, minimal, whitespace-aware (indentation-based blocks). No semicolons. No unnecessary braces. Reads like pseudocode, compiles to machine code.

```
# This is Aether
func fib(n u64) u64 {
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

Functions return the last expression or an explicit `return`. Multiple return values via tuples.

```aether
func add(a int, b int) int -> a + b

func divmod(a int, b int) (int, int) {
    return (a / b, a % b)
}
```

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

---

## 3. Memory Model

This is the heart of the language and what makes it a true 4GL — **you describe allocation semantics; the compiler generates the exact free/teardown code.**

### 3.1 Stack Allocation (Default)

All local variables are stack-allocated by default. The compiler tracks lifetimes and generates destruction at the end of scope.

```aether
func process() {
    let p = Point(3, 4)   # stack allocated
    let items = [1, 2, 3] # fixed-size array, stack
    # compiler inserts p.~destructor() and implicit scope exit
}
```

### 3.2 Escape Analysis

When a reference to a stack variable could outlive its scope, the compiler **automatically promotes** it to heap allocation. No programmer annotation needed for simple cases.

```aether
func make_point(x int, y int) ref Point {
    let p = Point(x, y)
    # compiler detects: p's reference escapes this frame
    # auto-promotes p to heap, ref-counted
    return p
}
```

### 3.3 Explicit Heap (`heap` keyword)

When you want explicit heap allocation (for large objects, shared state, or when escape analysis might be conservative):

```aether
let big = heap Buffer(1024 * 1024)
let shared = heap rc SharedState()
```

### 3.4 Ownership and Borrowing

- `owned T` — single-owner, moved on assignment, freed when owner drops
- `ref T` — borrowed reference, non-owning, must not outlive the lender
- `rc T` — reference-counted shared ownership, freed when count reaches zero

```aether
func consume(val owned Buffer) {   # val will be freed at end
    process(val)
}  # val freed here

func observe(val ref Buffer) {     # borrow, no ownership
    print(val.size())
}  # nothing freed, borrow ends
```

### 3.5 Region-Based Allocation (Aether OS native)

For kernel modules and performance-critical code, the language supports **region-based allocation** that maps directly to the Aether OS capability model.

```aether
region("kernel") {
    let p = Page()   # allocated from kernel region
    let buf = Buffer(256)
}  # entire region freed at once — no individual frees needed
```

Regions compile to arena allocator setup/teardown. All allocations within a region are freed as a batch when the region exits — O(1) teardown regardless of allocation count.

### 3.6 No Null Pointers

The type system has no null. Use `T?` (optional) instead.

```aether
let x: int? = none
x = 42

if let val = x {
    print(val)  # val is int, not int?
}

# Or unwrap with default
let y = x or 0
```

### 3.7 Pointers (Opt-In, Unsafe)

Raw pointers exist for hardware interaction, DMA, and inline assembly. They require an `unsafe` block.

```aether
func read_mmio(addr ptr u64) u64 {
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
    fd int
    path string
    
    func init(self ref File, path string) throws {
        self.fd = sys_open(path)
        self.path = path
    }
    
    func drop(self ref File) {
        sys_close(self.fd)
    }
    
    func read(self ref File, buf ref [u8]) int {
        return sys_read(self.fd, buf)
    }
    
    func path(self ref File) string -> self.path
}
```

Classes can have:
- Fields (always private by default)
- Methods (`func name(self, ...)`)
- Static methods (`static func name(...)`)
- Constructors (`func init(...)`)
- Destructors (`func drop(...)`)
- Properties (syntactic sugar for getter/setter)
- Access modifiers: `pub`, `internal`, `private`

### 4.2 Automatic Class Destruction

The compiler generates destructor calls at every scope exit where a class instance exists. This includes:
- Normal scope exit
- Early return
- Exception unwinding
- Loop break/continue

```aether
func read_config() throws string {
    let f = File("/etc/aether.cfg")  # constructor called
    let content = f.read_all()       # use the file
    # compiler inserts: f.drop() — even if f.read_all() threw
    return content
}
```

### 4.3 Classes Are Not Required

Pure procedural code with flat structs and free functions is always valid:

```aether
struct Point { x int; y int }

func distance(p Point) float {
    return sqrt(p.x * p.x + p.y * p.y)
}
```

### 4.4 Traits (Interfaces)

Traits define shared behavior. They compile to vtable pointers only when used dynamically; static dispatch is the default.

```aether
trait Serializable {
    func serialize(self ref Self) [u8]
}

impl Serializable for Point {
    func serialize(self ref Point) [u8] {
        return to_bytes([self.x, self.y])
    }
}

# Static dispatch (zero-cost)
func save_static(s ref Serializable) {
    let bytes = s.serialize()
}

# Dynamic dispatch (vtable)
func save_dynamic(s ref dyn Serializable) {
    let bytes = s.serialize()
}
```

### 4.5 Algebraic Data Types

```aether
enum Result(T, E) {
    Ok(T),
    Err(E)
}

enum Tree(T) {
    Leaf(T),
    Node(ref Tree(T), ref Tree(T))
}
```

### 4.6 Generics

Generics are monomorphized (zero-cost). Type parameters use angle brackets.

```aether
func swap(T)(a ref T, b ref T) {
    let tmp = *a
    *a = *b
    *b = tmp
}

class Stack(T) {
    data [T]
    size int
    
    func push(self ref Stack(T), item T) {
        self.data[self.size] = item
        self.size += 1
    }
    
    func pop(self ref Stack(T)) T? {
        if self.size == 0 { return none }
        self.size -= 1
        return self.data[self.size]
    }
}
```

---

## 5. Exception Handling

### 5.1 Try/Throw/Catch

Exceptions are **deterministic** — no unwinding tables or personality routines. Exceptions are encoded as tagged unions returned through normal function return, optimized by the compiler.

```aether
func divide(a int, b int) throws int {
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

### 5.2 Custom Error Types

```aether
enum MyError {
    NotFound(string),
    PermissionDenied,
    Timeout(u64)
}

func risky() throws MyError {
    throw MyError.Timeout(30)
}
```

### 5.3 Compile-Time Cost

Exceptions compile to one of two strategies, chosen by the compiler:
- **Zero-cost path** — the happy path has no overhead; the error path returns a tagged union
- **Return-code path** — functions returning `throws T` compile to a `(T, ErrorCode)` pair; the compiler ensures errors are handled

There are **no runtime type lookups** or stack unwinding databases in the binary.

---

## 6. NASM Inline Assembly

### 6.1 Basic Inline

```aether
func outb(port u16, val u8) {
    asm {
        mov dx, port
        mov al, val
        out dx, al
    }
}
```

### 6.2 Extended Inline with Constraints

```aether
func rdtsc() u64 {
    let hi u32
    let lo u32
    asm -> (hi, lo) {
        rdtsc
        mov [hi], edx
        mov [lo], eax
    }
    return (u64(hi) << 32) | u64(lo)
}
```

### 6.3 Full NASM Syntax

Any valid NASM instruction, directive, or macro is accepted:

```aether
func setup_gdt() {
    asm {
        ; NASM comment
        lgdt [gdt_ptr]
        mov ax, 0x10
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax
        jmp 0x08:flush_cs
flush_cs:
    }
}
```

### 6.4 Assembly-Only Functions

```aether
asm func _start() {
    ; Kernel entry point — pure NASM
    mov esp, stack_top
    extern kernel_main
    call kernel_main
    hlt
}
```

### 6.5 Variable Binding

Variables from Aether code can be referenced by name in `asm` blocks. The compiler maps them to registers or memory as needed. Outputs use `->` binding.

```aether
func cpuid(leaf u32) (u32, u32, u32, u32) {
    let a u32; let b u32; let c u32; let d u32
    asm -> (a, b, c, d) {
        mov eax, leaf
        cpuid
        mov [a], eax
        mov [b], ebx
        mov [c], ecx
        mov [d], edx
    }
    return (a, b, c, d)
}
```

---

## 7. Aether OS Integration

### 7.1 Freestanding Target

The compiler's primary target is **x86_64-freestanding**. No libc, no CRT, no OS assumptions.

```
aether build --target x86_64-freestanding --output kernel.elf
```

### 7.2 Host-Native Target (Priority)

The compiler also outputs **host-native formats** so you can compile and run `.ae` programs directly on your development machine without a VM, emulator, or cross-linker.

| Host OS | Format | Linker | Notes |
|---------|--------|--------|-------|
| macOS   | Mach-O 64 (x86_64) | `ld` (system) or direct syscall emission | Uses `syscall` instruction; emits `_main` entry point |
| Linux   | ELF64 | `ld` (system) or direct syscall emission | Uses `syscall` instruction; emits `_start` entry point |
| Windows | PE32+ | `link.exe` or direct | Future target |

```
# On macOS — produces a native Mac executable
aether build hello.ae --output ./hello
./hello
# Hello, Aether!

# On Linux — ELF64 native binary
aether build hello.ae --output ./hello
./hello
# Hello, Aether!
```

**What this enables:**
- Rapid iteration on `.ae` programs from your dev machine
- Test suite runs natively instead of in QEMU
- `aether run` compiles and executes in one step
- Debugging with native tools (lldb, gdb, Instruments, perf)
- Gradual migration from C bootstrap to self-hosted: write compiler components in Aether, test them on macOS, then deploy to the OS target

**Technical approach:**
- The compiler uses a **multi-backend codegen architecture**: one NASM backend for the freestanding ELF64 target, one for host-native output
- Host-native output uses the host's syscall ABI directly (e.g. macOS `syscall` instruction via inline asm or an ABI shim)
- The host-native `print()`/`puts()` calls the host OS write syscall (macOS = 0x2000004, Linux = 1) rather than the Aether OS 0x5000 table
- A `aether.toml` setting controls the target: `target = "host"` (auto-detect) or `target = "x86_64-freestanding"`
- **Phase 2 outputs x86_64 Mach-O** (runs on Apple Silicon via Rosetta). Native aarch64 output will require a dedicated backend that emits arm64 Mach-O directly or uses `as` with GAS syntax, planned for a later phase since NASM has no ARM64 backend.

### 7.3 Syscall Page (0x5000)

The compiler knows the Aether syscall table and generates optimal call sequences:

```aether
# These compile to direct calls through the 0x5000 table
sys func putc(c byte)
sys func puts(s string)
sys func open(path string) int
sys func read(fd int, buf ref [u8]) int
sys func exit()
```

`sys func` is a language keyword that tells the compiler to emit a direct indirect call through the syscall page at offset `index * 8`.

### 7.4 Module Interface (0x4000)

For kernel module development:

```aether
module serial {
    @export func mod_init() int {
        reg_cmd("serial", cmd_serial)
        reg_hook(1, boolhook_handler)
        return 0
    }
    
    @export func mod_fini() {
        unreg_cmd("serial")
        unreg_hook(1)
    }
}
```

The `@export` attribute makes the symbol visible in the ELF symbol table for the module loader. The `module` keyword generates the correct entry points and ELF section layout for a `.ko` file.

### 7.5 Binary Target

Standalone `/bin/` executables:

```aether
# compiles to an ELF loaded at 0x2000000
@entry(0x2000000)
func main(args [][]byte) int {
    puts("Hello from Aether!\n")
    return 0
}
```

### 7.6 Memory Layout Directives

```aether
@layout(start=0x7C00, max=512, file="stage1.bin")
func stage1_entry() {
    asm { ... }  # 512-byte MBR
}

@layout(start=0x7E00, file="stage2.bin")
func stage2_entry() {
    asm { ... }  # stage2 loader
}
```

---

## 8. Compiler Architecture

### 8.1 Pipeline

```
Source (.ae) → Tokenizer → Parser → AST → Semantic Analysis → 
  IR Generation → Optimization → Code Generation → ELF64 → Binary
```

### 8.2 Frontend

- **Tokenizer**: Whitespace-aware, handles significant indentation, emits token stream
- **Parser**: Recursive-descent for expressions, Pratt parser for operators, indentation-sensitive block parsing
- **Semantic Analyzer**: Type checking, name resolution, trait resolution, borrow checking, escape analysis, region inference
- **AST to HIR**: High-level IR preserving all type information

### 8.3 Middle-end

- **HIR to MIR**: Mid-level IR with explicit control flow (CFG), memory operations annotated
- **Optimization passes**:
  - Constant folding and propagation
  - Dead code elimination
  - Inlining (aggressive for generics)
  - Escape analysis → heap/stack promotion decisions
  - Region inference → arena elision
  - Devirtualization (static dispatch where possible)
  - Loop unrolling and optimization
  - Memory operation fusion

### 8.4 Backend

- **MIR to LIR**: Low-level IR near machine code
- **Register allocation**: Linear scan or graph coloring
- **Instruction selection**: x86_64 NASM instruction emission
- **ELF64 writer**: Generates flat ELF binaries with:
  - Executable (`.text`), read-only data (`.rodata`), data (`.data`), BSS (`.bss`)
  - Custom sections for Aether metadata
  - Symbol table for module exports
  - No dynamic sections, no relocations (position-dependent, fixed-address)
  - No interpreter header

### 8.5 Integrated Assembler

The compiler contains a built-in NASM-syntax assembler. Inline `asm` blocks are parsed by the integrated assembler and emitted directly as machine code bytes into the ELF. No external assembler dependency.

### 8.6 Self-Hosting Requirement

The compiler itself must be written in Aether. The bootstrap path:

1. **Phase 1**: Compiler written in C (bootstrap) — produces working Aether binaries
2. **Phase 2**: Compiler extended with self-compilation support
3. **Phase 3**: Aether compiler compiles itself for the first time
4. **Phase 4**: Entire toolchain runs in Aether; C bootstrap is archive-only

---

## 9. Build System & Toolchain

### 9.1 `aether` CLI

```
aether new      <name>          # Create new project
aether build    [--target=...]   # Compile project
aether run      [--target=...]   # Build and run in QEMU
aether test     [--target=...]   # Run unit tests
aether fmt      [files...]       # Format source
aether doc      [files...]       # Generate documentation
aether asm      [file.ae]        # Show generated assembly
aether inspect  [binary]         # Inspect ELF metadata
aether init     [--lib|--bin]    # Init project structure
```

### 9.2 Project Structure

```
my-kernel/
  aether.toml          # Project manifest
  src/
    main.ae            # Entry point
    lib/
      serial.ae        # Library modules
      gdt.ae
    asm/
      stage1.ae        # Assembly-heavy boot files
  tests/
    test_serial.ae     # Unit tests
  target/
    debug/
    release/
```

### 9.3 `aether.toml` Manifest

```toml
[package]
name = "aether-kernel"
version = "0.1.0"

[build]
target = "x86_64-freestanding"
output = "kernel.elf"
linker-script = "tools/kernel.ld"

[dependencies]
# Aether standard library (built-in)
std = { path = "/lib/aether/std" }
```

---

## 10. Unique Aether Features (4GL Differentiators)

These are the features that make Aether a true 4GL rather than just "Rust with different syntax."

### 10.1 Declarative Resource Management

Instead of writing allocation/free code, you **declare** what you need and the compiler generates the management code:

```aether
# Declare a memory pool for USB transfers
pool UsbDmaBuffer of size 64, count 32, alignment 256

# Use it — compiler generates pool alloc/free
func alloc_usb_buf() -> UsbDmaBuffer {
    return UsbDmaBuffer()  # from the declared pool
}  # compiler inserts: return to pool on drop
```

### 10.2 Query-Style Data Transformations

```aether
let active_users = db.users
    .filter(u -> u.active)
    .map(u -> (u.name, u.email))
    .sort(u -> u.0)
    .collect()
```

This compiles to fused loops with no intermediate allocations. The compiler recognizes the pipeline and generates optimal code for the target (in-memory for userspace, or with AetherFS traversal for kernel).

### 10.3 Goal-Oriented I/O

Instead of describing *how* to read a file, describe *what* you want:

```aether
let config = from "/etc/aether.cfg" read Config
```

The compiler generates the optimal read path: for boot-time, it emits raw ATA PIO reads; for userspace, it generates AetherFS syscalls; for an in-memory filesystem, it generates direct pointer access. The same source line compiles differently depending on compilation target.

### 10.4 Compile-Time OS Knowledge

The compiler has baked-in knowledge of the Aether OS architecture:

```aether
# Compiler knows the memory map
@kernel_layout
func init_memory() {
    # Allocations at 0x1000 (bitmap), 0x4000 (registry), 0x5000 (syscall),
    # 0x1000000 (kernel), 0x2000000 (binaries), 0x2100000 (modules)
    let bitmap = reserved(0x1000, 0x1000)
}

# Compiler verifies module ABI compliance
@module_abi(version = 1)
func mod_init() int {
    # Compiler checks: return type, argument types, slot constraints
}
```

### 10.5 Automatic Protocol Generation

```aether
protocol Serial {
    # The compiler generates the register-level protocol
    # from these high-level declarations
    port base = 0x3F8
    speed = 115200
    
    func putc(c byte) {
        asm { mov dx, port; mov al, c; out dx, al }
    }
}
```

### 10.6 Contract Programming

```aether
func withdraw(account ref Account, amount u64)
    pre(account.balance >= amount)
    post(account.balance == old(account.balance) - amount)
{
    account.balance -= amount
}
```

In debug builds, contracts are checked at runtime with clear error messages. In release builds, they serve as optimizer hints and are eliminated.

### 10.7 Compile-Time Execution

```aether
#run {
    # This runs at compile time!
    let result = compute_something()
    emit("const TABLE_SIZE = {result}")
}

const TABLE_SIZE = 256
```

### 10.8 Pattern-Based Metaprogramming

Instead of C++ templates or macros, Aether uses pattern matching on types:

```aether
impl(T) trait Hashable {
    func hash(self ref T) u64 {
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

---

## 11. Error Messages

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

## 12. Source File Extensions & Naming

| Extension | Purpose |
|-----------|---------|
| `.ae` | Aether source file |
| `.aet` | Aether test file |
| `.aes` | Aether script (runs without explicit build step) |
| `.aeh` | Aether header/interface file |
| `aether.toml` | Project manifest |

---

## 13. Standard Library (StdAether)

The compiler ships a freestanding standard library:

| Module | Contents |
|--------|----------|
| `std.io` | `print`, `println`, `format`, `read_line` |
| `std.mem` | `alloc`, `free`, `copy`, `zero`, `Pool`, `Arena` |
| `std.str` | `String`, `string_view`, `concat`, `split`, `trim` |
| `std.math` | `sqrt`, `sin`, `cos`, `abs`, `min`, `max` |
| `std.collections` | `Array`, `HashMap`, `Set`, `List`, `Queue` |
| `std.fs` | `File`, `Path`, `Directory`, (maps to AetherFS syscalls) |
| `std.serial` | `COM1`, `putc`, `puts`, (kernel-mode serial I/O) |
| `std.elf` | ELF64 reader/writer (for module loader, linker) |
| `std.test` | `assert`, `test_runner`, `benchmark` |
| `std.asm` | NASM helper macros and common sequences |

---

## 14. Testing

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

## 15. Milestones & Phases

### Phase 0 — Bootstrap Toolchain (NOW)
- [ ] Create language specification (this document)
- [ ] Set up project structure
- [ ] Write tokenizer in C (bootstrap)
- [ ] Write parser in C (bootstrap)
- [ ] Write simple code generator producing NASM

### Phase 1 — Core Language (MINIMUM VIABLE COMPILER)
- [ ] Variables, functions, control flow
- [ ] Primitive types (u8-u64, i8-i64, bool)
- [ ] Structs and tuples
- [ ] Basic inline NASM assembly
- [ ] ELF64 output (flat binary, no stdlib)
- [ ] Compiles `hello.ae` → ELF that runs on Aether OS
- [ ] `aether build` CLI command

### Phase 2 — Memory Management
- [ ] Stack allocation and automatic destruction
- [ ] Escape analysis (auto-promotion to heap)
- [ ] `ref`, `owned`, `rc` reference types
- [ ] Region-based allocation (`region {}` blocks)
- [ ] `heap` keyword for explicit heap allocation
- [ ] No null (optional types with `?`)

### Phase 3 — OOP and Type System
- [ ] Classes with `init`/`drop`
- [ ] Automatic destructor insertion at scope exits
- [ ] Traits (interfaces) with static dispatch
- [ ] Generics with monomorphization
- [ ] Algebraic data types (enum with payloads)
- [ ] Pattern matching (`match`)

### Phase 4 — Advanced Language Features
- [ ] Exception handling (`try`/`throw`/`catch`)
- [ ] Compile-time execution (`#run`)
- [ ] Contract programming (`pre`/`post`)
- [ ] Dynamic dispatch (`dyn Trait`)
- [ ] Closures and lambdas
- [ ] Properties and operator overloading

### Phase 5 — Aether OS Integration
- [ ] `sys func` (syscall page at 0x5000)
- [ ] `module` keyword (kernel module output)
- [ ] `@entry`, `@layout`, `@export` attributes
- [ ] ABI compliance verification
- [ ] Linker script integration
- [ ] Freestanding standard library (StdAether)

### Phase 6 — Self-Hosting
- [ ] Compiler can compile its own frontend
- [ ] Compiler can compile its own middle-end
- [ ] Compiler can compile its own backend
- [ ] Full bootstrap: Aether compiler runs on Aether OS
- [ ] C bootstrap becomes historical reference only

### Phase 7 — Optimization & Polish
- [ ] Aggressive inlining and devirtualization
- [ ] Memory fusion and loop optimization
- [ ] Cross-module optimization
- [ ] `aether fmt` — formatter
- [ ] `aether doc` — documentation generator
- [ ] Editor support (LSP, syntax highlighting)
- [ ] Error message excellence

---

## 16. Non-Goals

- No interpreter (never)
- No runtime garbage collector
- No JIT compilation
- No LLVM dependency (the compiler has its own backend)
- No dynamic linking (no shared libraries; everything is static)
- No AT&T assembly syntax (NASM only)
- No POSIX/glibc compatibility in freestanding mode
- No Ada, COBOL, or Fortran compatibility
- No REPL (Aether is compiled-only)
- No Windows/Mac/Linux userspace target initially (Aether OS only)

---

## 17. Constraints

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

## Appendix A: EOF Convention

Aether source files must end with a single newline. No trailing whitespace. The parser expects Unix line endings (`\n`).

```aether
# good: ends with exactly one newline
```

---

## Appendix B: Reserved Words

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

## Appendix C: Lexical Rules

| Token | Rule |
|-------|------|
| Comment | `#` to end of line |
| Block comment | `#{` ... `}#` (nestable) |
| String | Double-quoted, escape sequences: `\n`, `\t`, `\\`, `\"`, `\xNN` |
| Char | Single-quoted: `'a'`, `'\n'`, `'\x41'` |
| Integer | Decimal: `42`, Hex: `0xFF`, Binary: `0b1010`, Octal: `0o77` |
| Float | `3.14`, `1e10`, `0xFF.0p-3` |
| Identifier | `[a-zA-Z_][a-zA-Z0-9_]*` |
| Indentation | Significant: blocks are indentation-based, 4 spaces per level |
| Terminator | Newline ends simple statements; expressions continue across lines |
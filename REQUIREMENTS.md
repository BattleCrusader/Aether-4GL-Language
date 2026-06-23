# Aether Language — Compiler Requirements

## 1. Philosophy

Aether is a compiled, object-oriented 4GL designed specifically for operating system development. It bridges the gap between high-level expressiveness and bare-metal control. Every design decision serves three goals: **zero-cost abstractions that compile to readable NASM**, **deterministic automatic memory management baked into the binary**, and **seamless integration with freestanding x86_64 environments**.

### Core Principles

- **Compiled only, no interpreter.** The compiler emits NASM assembly, assembles with NASM, and links with LD. No runtime, no VM, no JIT.
- **Automatic memory is the default.** Bump allocators, region-based allocation, and automatic destructor chains are emitted by the compiler. The programmer never calls `free` or `delete`.
- **Classes are optional.** The language is fully usable without OOP. Functions, structs, enums, and procedural code are first-class citizens.
- **References over pointers.** The compiler prefers `ref` semantics (borrowed, non-nullable, bounds-checked where possible). Raw pointers exist for hardware access and inline assembly.
- **Inline NASM assembly.** Any function can contain raw NASM blocks. The compiler passes them through verbatim with label resolution.
- **Deterministic exceptions.** `try`/`catch` with full stack unwinding, scope cleanup (destructors + defer), and no runtime type information. Compiled to explicit jump tables and cleanup code.
- **Freestanding by default.** No libc dependency. The compiler generates code that runs on bare metal, in kernel space, or hosted on the target OS.
- **Multi-target output.** The same source can compile to Mach-O (macOS host), ELF (Linux host), flat binary (boot sector), kernel ELF, or relocatable module.
- **Universal binary support.** The compiler can emit multi-architecture binaries containing x86_64, ARM64, and RISC-V code in a single file, with a thin dispatcher at the entry point.

---

## 2. Language Design

### 2.1 Syntax Philosophy

Aether syntax is brace-delimited, newline-sensitive for statement separation, and visually clean. It draws inspiration from Go (simplicity), Rust (safety concepts), and Python (readability), but is not a clone of any of them.

```
// Hello World
func main() {
    print("Hello, Aether!\n")
}
```

### 2.2 Key Syntax Rules

- **Braces `{}` for blocks.** Functions, if/else, loops, try/catch, classes, structs, enums all use braces.
- **Newlines separate statements.** Semicolons are optional and can be used to put multiple statements on one line.
- **`//` for line comments, `/* */` for block comments.**
- **Identifiers:** letters, digits, underscores. Must start with a letter or underscore.
- **Case-sensitive.** `foo` and `Foo` are different.
- **`_` is the blank identifier.** Discards values.

### 2.3 Types

| Type | Size | Description |
|------|------|-------------|
| `u8` | 1 byte | Unsigned 8-bit integer |
| `u16` | 2 bytes | Unsigned 16-bit integer |
| `u32` | 4 bytes | Unsigned 32-bit integer |
| `u64` | 8 bytes | Unsigned 64-bit integer |
| `i8` | 1 byte | Signed 8-bit integer |
| `i16` | 2 bytes | Signed 16-bit integer |
| `i32` | 4 bytes | Signed 32-bit integer |
| `i64` | 8 bytes | Signed 64-bit integer |
| `f32` | 4 bytes | IEEE 754 single-precision float |
| `f64` | 8 bytes | IEEE 754 double-precision float |
| `bool` | 1 byte | `true` or `false` |
| `char` | 1 byte | ASCII character |
| `str` | 16 bytes | String view: `{ptr: u64, len: u64}` |
| `void` | 0 bytes | No value (for functions) |
| `typeid` | 8 bytes | Runtime type identifier (opaque) |

### 2.4 Composite Types

| Type | Description |
|------|-------------|
| `[N]T` | Fixed-size array of N elements of type T |
| `[]T` | Dynamic slice: `{ptr: u64, len: u64}` |
| `ref T` | Immutable reference to T (non-nullable, borrowed) |
| `mut ref T` | Mutable reference to T |
| `*T` | Raw pointer to T (nullable, unchecked) |
| `?T` | Optional type: `{has_value: bool, value: T}` |
| `struct` | Named product type with fields |
| `enum` | Tagged union with optional payloads |
| `class` | OOP type with methods, fields, auto-destructor |
| `trait` | Interface definition (compile-time dispatch) |
| `dyn T` | Dynamic trait object (runtime dispatch via vtable) |

### 2.5 Operators

Standard C-style operators with these additions:

| Operator | Description |
|----------|-------------|
| `+` | Numeric add OR string concatenation (when either operand is a string) |
| `ref` | Take reference |
| `mut` | Take mutable reference |
| `owned` | Take ownership (move semantics) |
| `as` | Type cast |
| `is` | Type check (for enums with payloads) |
| `..` | Range (inclusive start, exclusive end) |
| `..=` | Range (inclusive both ends) |
| `??` | Nil-coalescing (unwrap optional or default) |
| `?.` | Optional chaining |
| `!` | Force-unwrap optional (panics on nil) |

### 2.6 String Interpolation

Strings support inline interpolation with `{expr}` syntax:

```
let name = "Aether"
let msg = "Hello, {name}! The answer is {6 * 7}.\n"
// msg = "Hello, Aether! The answer is 42.\n"
```

The compiler desugars interpolation into concatenation calls at compile time. Any expression that can be converted to a string (numbers, bools, types with a `format` method) is valid inside `{}`.

---

## 3. Memory Management

### 3.1 Philosophy

Aether has **no garbage collector** and **no manual free**. Memory management is entirely compile-time determined and baked into the binary. The compiler analyzes lifetimes and inserts allocation, deallocation, and destructor calls automatically.

### 3.2 Allocation Strategies

| Strategy | Description | Used For |
|----------|-------------|----------|
| **Bump allocation** | Linear scan through a pre-allocated arena. O(1) alloc, no free. | Temporary objects, per-function scratch |
| **Region allocation** | Arena with rollback. Allocations in a region are freed together. | Per-request, per-frame, per-transaction |
| **Stack allocation** | All variables live on the stack by default. No heap overhead. | Local variables, small structs |
| **Heap allocation** | Explicit `alloc` keyword. Paired with automatic destructor. | Long-lived objects, dynamic data |

### 3.3 Automatic Destructors

When a class instance or heap-allocated value goes out of scope, the compiler:

1. Calls the class destructor (if defined)
2. Calls destructors for all member fields (recursively)
3. Frees the memory (for heap allocations)

This is emitted as explicit code in the compiled binary — no runtime, no GC, no reference counting.

```
class Buffer {
    data: [100]u8
    len: u32

    destructor() {
        print("Buffer destroyed\n")
    }
}

func example() {
    let buf = Buffer{}    // stack-allocated
    // ... use buf ...
}   // compiler emits: buf.destructor() automatically
```

### 3.4 Defer

`defer` schedules cleanup code to run at scope exit, regardless of how the scope is exited (normal return, exception, break). Defers run in reverse order of declaration (LIFO).

```
func read_file(path: str) -> ?str {
    let f = open(path)
    defer close(f)       // runs when scope exits
    let data = read_all(f)
    return data
}   // close(f) called automatically even on early return
```

### 3.5 Regions

Regions provide scoped arena allocation. All allocations within a region are freed when the region ends.

```
region frame {
    let obj = alloc MyObject{}    // allocated in region
    // ... use obj ...
}   // all region allocations freed here
```

### 3.6 Ownership and Borrowing

Aether uses a simplified ownership model:

- **Owned values** (`owned` keyword): one owner at a time. Ownership can be transferred (moved).
- **References** (`ref`): borrowed, non-owning views. The compiler ensures the reference doesn't outlive the referent.
- **Mutable references** (`mut ref`): exclusive, mutable borrow. Only one mutable reference to a value at a time.

This is enforced at compile time without a borrow checker as complex as Rust's — Aether uses a simpler, more permissive model that catches use-after-free and double-free but allows more patterns.

---

## 4. Exception Handling

### 4.1 Try/Catch

Aether has deterministic exception handling with full stack unwinding. No exception objects, no RTTI, no heap allocation for exceptions.

```
func risky() throws {
    if condition {
        throw "something went wrong"
    }
}

func main() {
    try {
        risky()
    } catch {
        print("caught an error!\n")
    }
}
```

### 4.2 How It Works

The compiler emits:

1. **Before try body:** Save the stack frame and set up a jump buffer (sigsetjmp on host, custom IDT-based for kernel)
2. **Try body:** Execute normally
3. **On throw:** Walk the cleanup table (innermost scope first), call destructors and defers, then jump to the catch handler
4. **After try body:** Clear the jump buffer
5. **Catch handler:** Execute recovery code

### 4.3 Error Propagation

Functions that can throw are marked with `throws`. Callers must either handle the error with `try`/`catch` or propagate with `throws`.

```
func inner() throws {
    throw "fail"
}

func outer() throws {
    inner()    // propagates automatically
}

func main() {
    try {
        outer()
    } catch {
        print("outer failed\n")
    }
}
```

### 4.4 Segfault Handling (Host)

On host targets, segfaults in try blocks are caught via `sigsetjmp`/`siglongjmp`. The compiler emits a C helper that sets up signal handlers for `SIGSEGV` and `SIGBUS`, then restores the jump buffer on catch.

---

## 5. Object-Oriented Programming

### 5.1 Classes

Classes are the primary OOP construct. They support fields, methods, constructors, destructors, and inheritance.

```
class Animal {
    name: str
    age: u32

    constructor(name: str) {
        this.name = name
        this.age = 0
    }

    destructor() {
        print("{this.name} says goodbye\n")
    }

    func speak() {
        print("{this.name} makes a sound\n")
    }
}
```

### 5.2 Inheritance

Single inheritance with virtual dispatch via vtable.

```
class Dog : Animal {
    breed: str

    constructor(name: str, breed: str) : super(name) {
        this.breed = breed
    }

    override func speak() {
        print("{this.name} barks!\n")
    }
}
```

### 5.3 Automatic Destruction

Class instances are destroyed automatically when they go out of scope:

- **Stack-allocated:** destructor called at end of scope
- **Heap-allocated:** destructor called, then memory freed
- **Array of classes:** each element destroyed in reverse order
- **Inheritance chain:** destructors called from most-derived to base

### 5.4 Structs vs Classes

| Feature | Struct | Class |
|---------|--------|-------|
| Memory | Stack by default | Stack or heap |
| Copy semantics | Value copy (memcpy) | Reference (pointer) |
| Destructor | Not supported | Automatic |
| Inheritance | Not supported | Supported |
| Methods | Supported | Supported |
| Virtual dispatch | Static only | Static + virtual |
| When to use | Small data, no cleanup needed | Complex objects with lifecycle |

### 5.5 Traits

Traits define interfaces that types can implement. Dispatch is static (monomorphized) by default, with `dyn` for dynamic dispatch.

```
trait Drawable {
    func draw()
}

struct Circle {
    x: i32, y: i32, radius: u32
}

impl Drawable for Circle {
    func draw() {
        print("Circle at ({x}, {y}) r={radius}\n")
    }
}

func render(item: dyn Drawable) {
    item.draw()    // dynamic dispatch via vtable
}
```

---

## 6. Inline Assembly

### 6.1 NASM Blocks

Any function can contain raw NASM assembly blocks using the `asm` keyword:

```
func write_port(port: u16, value: u8) {
    asm {
        mov dx, [port]
        mov al, [value]
        out dx, al
    }
}
```

### 6.2 Register Constraints

Variables can be mapped to specific registers:

```
func cpuid() -> (u32, u32, u32, u32) {
    let eax: u32, ebx: u32, ecx: u32, edx: u32
    asm {
        mov eax, 1
        cpuid
        mov [eax], eax
        mov [ebx], ebx
        mov [ecx], ecx
        mov [edx], edx
    }
    return (eax, ebx, ecx, edx)
}
```

### 6.3 Label Resolution

NASM labels inside asm blocks are resolved by the assembler. Aether labels can be referenced from asm blocks using the `@` prefix:

```
func example() {
    asm {
        jmp @skip
        ; ... dead code ...
      @skip:
        nop
    }
}
```

### 6.4 Raw Binary Emission

For boot sectors and low-level code, raw bytes can be emitted:

```
asm {
    db 0x55, 0xAA    ; boot signature
    times 510-($-$$) db 0
}
```

---

## 7. Compile-Time Features

### 7.1 Compile-Time Evaluation

A subset of the language can be evaluated at compile time:

```
const MAX_SIZE = 4096
const TABLE_SIZE = MAX_SIZE / 16

const TABLE: [TABLE_SIZE]u32 = compute_table()    // computed at compile time

func compute_table() -> [TABLE_SIZE]u32 {
    let result: [TABLE_SIZE]u32
    for i in 0..TABLE_SIZE {
        result[i] = i * i
    }
    return result
}
```

### 7.2 Compile-Time Reflection

```
@target    // returns current target: "host", "kernel", "boot", "module", "binary"
@arch      // returns current architecture: "x86_64", "arm64", "riscv64"
@endian    // returns "little" or "big"
@sizeof(T) // returns size of type T at compile time
@alignof(T) // returns alignment of type T
```

### 7.3 Conditional Compilation

```
# if @target == "kernel"
    // kernel-specific code
# elif @target == "host"
    // host-specific code
# else
    // fallback
# end
```

---

## 8. Target System

### 8.1 Supported Targets

| Target | Output | Entry | Use Case |
|--------|--------|-------|----------|
| `host` | Mach-O (macOS) or ELF (Linux) | `_aether_entry` | Testing on host OS |
| `kernel` | ELF64 | `_start` | Aether OS kernel |
| `boot` | Flat binary (512B) | `0x7C00` | Boot sector |
| `binary` | ELF64 at 0x2000000 | `_start` | Aether OS /bin/ executables |
| `module` | ELF64 relocatable | `mod_init` | Aether OS kernel modules |
| `universal` | Multi-arch binary | Dispatcher | Cross-platform executables |

### 8.2 Target Annotations

Target-specific code can be annotated:

```
@target("kernel")
func kernel_init() {
    // only compiled for kernel target
}

@target("host")
func host_init() {
    // only compiled for host target
}
```

### 8.3 Memory Layout Directives

```
@entry(0x7C00)        // Set entry point address
@layout(512)          // Flat binary with max size
@org(0x2000000)       // Origin address for code
@section(".text")     // Place following code in specific section
```

---

## 9. Standard Library

### 9.1 Built-in Functions

| Function | Description |
|----------|-------------|
| `print(str)` | Print string to stdout/serial |
| `print_i64(i64)` | Print signed integer |
| `print_u64(u64)` | Print unsigned integer |
| `print_hex(u64)` | Print hex |
| `assert(bool)` | Runtime assertion |
| `panic(str)` | Fatal error with message |
| `alloc(T)` | Heap-allocate a value of type T |
| `alloc_array(T, n)` | Heap-allocate array of n elements |
| `len(slice)` | Length of slice or array |
| `copy(dst, src)` | Memory copy |
| `zero(ptr, len)` | Zero memory |
| `sizeof(T)` | Size of type (compile-time) |

### 9.2 OS-Specific Functions (Kernel/Binary Targets)

| Function | Description |
|----------|-------------|
| `syscall(n, ...)` | Call Aether OS syscall at 0x5000 |
| `writePortByte(port, value)` | Write byte to I/O port |
| `readPortByte(port) -> u8` | Read byte from I/O port |
| `disableInterrupts()` | Disable interrupts |
| `enableInterrupts()` | Enable interrupts |
| `haltCpu()` | Halt CPU |

---

## 10. Compiler Architecture

### 10.1 Pipeline

```
Source (.ae)
  → Tokenizer (character stream → tokens)
  → Lexer (tokens → token stream with whitespace handling)
  → Parser (token stream → AST)
  → Semantic Analyzer (type checking, name resolution, lifetime analysis)
  → Optimizer (constant folding, dead code elimination, inlining)
  → Code Generator (AST → NASM assembly)
  → Assembler (NASM → object file)
  → Linker (object file → executable)
```

### 10.2 Intermediate Representations

- **AST:** Full syntax tree with source locations
- **ASM IR:** Internal representation of assembly instructions (for multi-arch backends)
- **NASM:** Text output passed to NASM assembler

### 10.3 Backends

| Backend | Architecture | Status |
|---------|-------------|--------|
| x86_64 NASM | x86_64 | Primary |
| ARM64 ASM IR | ARM64 | In progress |
| RISC-V ASM IR | RISC-V | In progress |

---

## 11. Unique Features (Beyond Standard)

### 11.1 Automatic Bump Allocator

Every function gets a per-function bump arena for temporary allocations. The compiler emits code to initialize the arena on function entry and reset it on exit. This makes short-lived allocations (string concatenation, temporary buffers) essentially free.

### 11.2 String as a First-Class Type

Strings are `{ptr: u64, len: u64}` views. The `+` operator concatenates strings when either operand is a string. String interpolation is desugared at compile time. No null-termination required.

### 11.3 Pattern Matching

```
match value {
    0 => print("zero\n")
    1..10 => print("small\n")
    10..100 => print("medium\n")
    _ => print("large\n")
}
```

### 11.4 Optional Chaining

```
let name = user?.profile?.name ?? "Anonymous"
```

### 11.5 If-Let

```
if let val = optional_value {
    print("got {val}\n")
}
```

### 11.6 Contracts (Design by Contract)

```
func divide(a: i32, b: i32) -> i32
    require b != 0
    ensure result * b == a
{
    return a / b
}
```

Contracts are checked at runtime in debug builds, stripped in release builds.

### 11.7 Closures

```
let add = func(a: i32, b: i32) -> i32 { return a + b }
let result = add(3, 4)    // 7
```

Closures can capture variables from the enclosing scope. The compiler allocates a closure struct on the stack and passes it as a hidden argument.

### 11.8 Operator Overloading

```
struct Vector3 {
    x: f32, y: f32, z: f32
}

impl Vector3 {
    func op_add(other: Vector3) -> Vector3 {
        return Vector3{x + other.x, y + other.y, z + other.z}
    }
}

let a = Vector3{1.0, 2.0, 3.0}
let b = Vector3{4.0, 5.0, 6.0}
let c = a + b    // calls op_add
```

### 11.9 Generics

```
func max[T](a: T, b: T) -> T {
    if a > b { return a }
    return b
}

let m = max[i32](3, 7)    // explicit
let n = max(3.0, 7.0)     // inferred
```

Generics are monomorphized at compile time — each concrete instantiation generates separate code.

### 11.10 Compile-Time Function Execution

Functions marked `comptime` are executed at compile time. Their results are embedded as constants in the binary.

```
comptime func generate_lookup_table() -> [256]u32 {
    let table: [256]u32
    for i in 0..256 {
        table[i] = i * i
    }
    return table
}

const LOOKUP = generate_lookup_table()
```

### 11.11 Module System

```
// math.ae
pub func square(x: i32) -> i32 {
    return x * x
}

// main.ae
import "math.ae"

func main() {
    print_i64(square(7))    // 49
}
```

### 11.12 Universal Binary Support

The compiler can produce a single binary containing code for multiple architectures. The entry point is a thin dispatcher that detects the current architecture and jumps to the correct code section. This enables distributing a single binary that runs on x86_64, ARM64, and RISC-V without recompilation.

---

## 12. Compiler Requirements Summary

### 12.1 Must Have

- [ ] Compile Aether source to NASM assembly
- [ ] Assemble with NASM to object files
- [ ] Link with LD to executables
- [ ] Support all targets: host, kernel, boot, binary, module, universal
- [ ] Automatic bump allocator per function
- [ ] Automatic destructor chains for classes
- [ ] Region-based allocation
- [ ] `defer` statement
- [ ] `try`/`catch`/`throw` with stack unwinding
- [ ] Inline NASM assembly blocks
- [ ] Classes with inheritance, constructors, destructors
- [ ] Structs and enums
- [ ] Traits with static and dynamic dispatch
- [ ] Generics with monomorphization
- [ ] String interpolation
- [ ] Pattern matching
- [ ] Optional types with `?T`, `??`, `?.`, `!`
- [ ] If-let
- [ ] Closures
- [ ] Operator overloading
- [ ] Compile-time evaluation
- [ ] Conditional compilation
- [ ] Module/import system
- [ ] Contracts (require/ensure)
- [ ] Universal binary output
- [ ] Freestanding output (no libc dependency)
- [ ] Segfault handling in try blocks (host target)

### 12.2 Should Have

- [ ] Compile-time reflection (`@target`, `@arch`, etc.)
- [ ] Cross-module inlining
- [ ] Basic optimizer (constant folding, dead code elimination)
- [ ] Debug information output
- [ ] Error messages with source locations
- [ ] Multi-file compilation
- [ ] Linker script generation

### 12.3 Nice to Have

- [ ] IDE integration (LSP)
- [ ] Formatter
- [ ] Documentation generator
- [ ] Package manager
- [ ] Test framework
- [ ] Profiling support
- [ ] Address sanitizer (compile-time instrumentation)

---

## 13. Relationship to Aether OS

The Aether language is the primary development tool for Aether OS. The compiler must produce code that conforms to the OS ABI:

- **Kernel:** ELF64, entry `_start`, linked at 0x1000000, no libc, no red zone, no SSE
- **Binaries:** ELF64, entry `_start`, loaded at 0x2000000, syscall page at 0x5000
- **Modules:** ELF64 relocatable, entry `mod_init`, module registry at 0x4000
- **Boot:** Flat binary, entry 0x7C00, max 512 bytes (or configurable)

The compiler must also support host-native compilation for testing, producing Mach-O (macOS) or ELF (Linux) executables that link against the segfault helper for exception handling.

---

## 14. Non-Goals

- No garbage collector
- No interpreter or REPL (beyond compile-and-run)
- No JIT compilation
- No runtime type information (RTTI)
- No reflection at runtime
- No dynamic code loading (modules are loaded by the OS, not the language)
- No FFI to C (freestanding target has no C runtime)
- No async/await (cooperative multitasking is handled by the OS)
- No garbage-collected memory model

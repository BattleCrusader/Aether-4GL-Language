# Aether Language — Compiler Requirements

## 1. Philosophy

Aether is a compiled, object-oriented 4GL designed specifically for operating system development. It bridges the gap between high-level expressiveness and bare-metal control. Every design decision serves three goals: **zero-cost abstractions that compile to efficient native code**, **deterministic automatic memory management baked into the binary**, and **seamless integration with freestanding environments**.

### Core Principles

- **Compiled only, no interpreter.** The compiler emits LLVM IR, which is compiled to native code by LLVM backends. No runtime, no VM, no JIT.
- **Automatic memory is the default.** Bump allocators, region-based allocation, and automatic destructor chains are emitted by the compiler. The programmer never calls `free` or `delete`.
- **Classes are optional.** The language is fully usable without OOP. Functions, structs, enums, and procedural code are first-class citizens.
- **References over pointers.** The compiler prefers `ref` semantics (borrowed, non-nullable). Raw pointers exist for hardware access and inline assembly.
- **Inline assembly.** Any function can contain raw Intel-syntax assembly blocks via LLVM inline asm.
- **Deterministic exceptions.** `try`/`catch` with full stack unwinding, scope cleanup (destructors + defer), and no runtime type information. Compiled to explicit jump tables and cleanup code.
- **Freestanding by default.** No libc dependency. The compiler generates code that runs on bare metal, in kernel space, or hosted on the target OS.
- **Multi-target output.** The same source can compile to Mach-O (macOS host), ELF (Linux host), flat binary (boot sector), kernel ELF, or relocatable module.
- **LLVM backend.** Register allocation, optimization, and multi-arch support are handled by LLVM. The compiler generates LLVM IR, not assembly text.

---

## 2. Language Design

### 2.1 Syntax Philosophy

Aether syntax is indentation-based, newline-sensitive for statement separation, and visually clean. It draws inspiration from Go (simplicity), Rust (safety concepts), and Python (readability), but is not a clone of any of them.

```
// Hello World
func main() {
    print("Hello, Aether!\n")
}
```

### 2.2 Key Syntax Rules

- **Indentation for blocks.** Functions, if/else, loops, try/catch, classes, structs, enums all use indentation (4 spaces).
- **Newlines separate statements.** Semicolons are optional and can be used to put multiple statements on one line.
- **`//` for line comments, `/* */` for block comments, `///` for doc comments.**
- **Identifiers:** letters, digits, underscores. Must start with a letter or underscore.
- **Case-sensitive.** `foo` and `Foo` are different.
- **`_` is the blank identifier.** Discards values.

### 2.3 Types

| Type | Size | Description |
|------|------|-------------|
| `void` | 0 bytes | No value (for functions) |
| `bool` | 1 byte | `true` or `false` |
| `byte` | 1 byte | Raw byte (u8 alias) |
| `char` | 1 byte | Single character (byte alias) |
| `u8` | 1 byte | Unsigned 8-bit integer |
| `u16` | 2 bytes | Unsigned 16-bit integer |
| `u32` | 4 bytes | Unsigned 32-bit integer |
| `u64` | 8 bytes | Unsigned 64-bit integer |
| `i8` | 1 byte | Signed 8-bit integer |
| `i16` | 2 bytes | Signed 16-bit integer |
| `i32` | 4 bytes | Signed 32-bit integer |
| `i64` | 8 bytes | Signed 64-bit integer |
| `int` | 8 bytes | Alias for `i64` |
| `f32` | 4 bytes | IEEE 754 single-precision float |
| `float` | 4 bytes | Alias for `f32` |
| `f64` | 8 bytes | IEEE 754 double-precision float |
| `double` | 8 bytes | Alias for `f64` |
| `string` | 8 bytes | Pointer to null-terminated string with length header |

### 2.4 Composite Types

| Type | Description |
|------|-------------|
| `[T; N]` | Fixed-size array of N elements of type T |
| `[T]` | Dynamic slice: `{ptr: u64, len: u64}` |
| `ref T` | Immutable reference to T (non-nullable, borrowed) |
| `mut ref T` | Mutable reference to T |
| `owned T` | Unique ownership (move semantics) |
| `rc T` | Reference-counted shared ownership |
| `*T` | Raw pointer to T (nullable, unchecked) |
| `?T` | Optional type |
| `func(Args): Ret` | Function pointer type |
| `(T1, T2, ...)` | Tuple type |
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
| `++` `--` | Prefix/postfix increment and decrement |
| `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=` | Compound assignment |
| `#` | Array length (prefix operator) |
| `~` | Bitwise NOT |
| `and` `or` `not` | Keyword logical operators (synonyms for `&&` `||` `!`) |
| `..` | Range (inclusive start, exclusive end) |
| `..=` | Range (inclusive both ends) |
| `as` | Type cast |
| `::` | Scope resolution (enum variant construction) |
| `->` | Expression body (function arrow) |

### 2.6 String Interpolation

Strings support inline interpolation with `{expr}` syntax:

```
let name = "Aether"
let msg = "Hello, {name}! The answer is {6 * 7}."
// msg = "Hello, Aether! The answer is 42."
```

The compiler desugars interpolation into concatenation calls at compile time. Any expression that can be converted to a string (numbers, bools) is valid inside `{}`. Use `\{` and `\}` for literal braces.

---

## 3. Memory Management

### 3.1 Philosophy

Aether has **no garbage collector** and **no manual free**. Memory management is entirely compile-time determined and baked into the binary. The compiler analyzes lifetimes and inserts allocation, deallocation, and destructor calls automatically.

### 3.2 Allocation Strategies

| Strategy | Description | Used For |
|----------|-------------|----------|
| **Stack allocation** | All variables live on the stack by default. No heap overhead. | Local variables, small structs |
| **Bump allocation** | Linear scan through a pre-allocated arena. O(1) alloc, no free. | Temporary objects, per-function scratch |
| **Region allocation** | Arena with rollback. Allocations in a region are freed together. | Per-request, per-frame, per-transaction |
| **Heap allocation** | Explicit `heap` keyword. Paired with automatic destructor. | Long-lived objects, dynamic data |

### 3.3 Automatic Destructors

When a class instance or heap-allocated value goes out of scope, the compiler:

1. Calls the class destructor (`drop` method, if defined)
2. Calls destructors for all member fields (recursively)
3. Frees the memory (for heap allocations)

This is emitted as explicit code in the compiled binary — no runtime, no GC, no reference counting.

```
class Buffer {
    data: [u8; 100]
    len: u32

    func drop() {
        print("Buffer destroyed\n")
    }
}

func example() {
    let buf = Buffer { data: ..., len: 0 }    // stack-allocated
    // ... use buf ...
}   // compiler emits: buf.drop() automatically
```

### 3.4 Defer

`defer` schedules cleanup code to run at scope exit, regardless of how the scope is exited (normal return, exception, break). Defers run in reverse order of declaration (LIFO).

```
func readFile(path: string): string? {
    let f = File::open(path)
    defer { f.close() }       // runs when scope exits
    let data = f.readAll()
    return data
}   // f.close() called automatically even on early return
```

### 3.5 Regions

Regions provide scoped arena allocation. All allocations within a region are freed when the region ends.

```
region("frame") {
    let obj = heap MyObject {}    // allocated in region
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
    } catch _ {
        print("caught an error!\n")
    }
}
```

### 4.2 How It Works

The compiler emits:

1. **Before try body:** Clear error tag (`xor rdx, rdx`)
2. **Try body:** Execute normally with cleanup table tracking
3. **On throw:** Walk the cleanup table (innermost scope first), call destructors and defers, set error tag, jump to catch handler
4. **After try body:** Check error tag, branch to catch or finally
5. **Catch handler:** Match error discriminant against catch arms, execute recovery code

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
    } catch _ {
        print("outer failed\n")
    }
}
```

### 4.4 Segfault Handling (Host)

On host targets, segfaults in try blocks are caught via `sigsetjmp`/`siglongjmp`. The compiler emits a C helper that sets up signal handlers for `SIGSEGV` and `SIGBUS`, then restores the jump buffer on catch.

---

## 5. Object-Oriented Programming

### 5.1 Classes

Classes are the primary OOP construct. They support fields, methods, constructors, destructors.

```
class Animal {
    name: string
    age: u32

    func init(name: string) {
        self.name = name
        self.age = 0
    }

    func drop() {
        print("{self.name} says goodbye\n")
    }

    func speak() {
        print("{self.name} makes a sound\n")
    }
}
```

### 5.2 Implicit `self`

`self` is **never written in the parameter list**. The parser auto-injects `self: ref Type` as the first parameter for methods defined inside struct/class bodies.

### 5.3 Automatic Destruction

Class instances are destroyed automatically when they go out of scope:

- **Stack-allocated:** destructor called at end of scope
- **Heap-allocated:** destructor called, then memory freed
- **Array of classes:** each element destroyed in reverse order

### 5.4 Structs vs Classes

| Feature | Struct | Class |
|---------|--------|-------|
| Memory | Stack by default | Stack or heap |
| Copy semantics | Value copy (memcpy) | Reference (pointer) |
| Destructor | Not supported | Automatic via `drop()` |
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

func render(item: ref dyn Drawable) {
    item.draw()    // dynamic dispatch via vtable
}
```

---

## 6. Inline Assembly

### 6.1 Assembly Blocks

Any function can contain raw assembly blocks using the `asm` keyword:

```
func writePortByte(port: u16, value: byte) {
    asm {
        mov dx, rdi
        mov al, sil
        out dx, al
    }
}
```

Aether function parameters are **not** automatically substituted — use SysV ABI registers directly (rdi=arg1, rsi=arg2, rdx=arg3, rcx=arg4, r8=arg5, r9=arg6).

### 6.2 Output Bindings

Use `asm: (outputs) { body }` to bind assembly results to Aether variables:

```
func readTimestampCounter(): u64 {
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

### 6.3 Top-Level Assembly

At file level (outside any function), `asm { }` emits raw assembly text directly into the output without any function wrapping. Used for declaring BSS/data sections, global symbols, and other directives.

### 6.4 Extern Hoisting

`extern` declarations from asm blocks are automatically hoisted to file level. NASM requires `extern` at file level, not inside function bodies.

---

## 7. Compile-Time Features

### 7.1 Compile-Time Constants

```
const MAX_SIZE = 4096
const TABLE_SIZE = MAX_SIZE / 16
```

Constants are evaluated at compile time and can be used in type annotations, array sizes, and inline assembly.

### 7.2 `#run` Blocks

Code inside `#run { }` executes at compile time. This enables metaprogramming without a separate macro system.

```
#run {
    // This runs at compile time!
    let result = computeSomething()
    emit("const TABLE_SIZE = {result}")
}
```

### 7.3 Compile-Time Reflection

```
@target    // returns current target: "host", "kernel", "boot", "module", "binary"
@arch      // returns current architecture: "x86_64", "arm64", "riscv64"
@endian    // returns "little" or "big"
@sizeof(T) // returns size of type T at compile time
@alignof(T) // returns alignment of type T
```

### 7.4 Conditional Compilation

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
| `kernel` | ELF64 | `_start` at 0x1000000 | Aether OS kernel |
| `boot` | Flat binary (512B) | 0x7C00 | Boot sector |
| `binary` | ELF64 at 0x2000000 | `_start` | Aether OS /bin/ executables |
| `module` | ELF64 relocatable | `mod_init` | Aether OS kernel modules |
| `freestanding` | ELF64 | `_start` | Generic freestanding |
| `universal` | Multi-arch binary | CPU dispatcher | Cross-platform executables |
| `asm-x86_64` | Intel assembly | — | Assembly listing |
| `asm-arm64` | ARM64 assembly | — | ARM64 listing |
| `asm-riscv64` | RISC-V assembly | — | RISC-V listing |
| `wasm32` | WebAssembly | — | Web/embedded (future) |

### 8.2 Target Annotations

```
@entry(0x7C00)        // Set entry point address
@layout(start=0x7C00, max=512, file="stage1.bin")  // Flat binary layout
@kernel_layout        // Enable kernel memory map overlap verification
@export               // Export function for module loader
```

---

## 9. Standard Library

### 9.1 Built-in Functions

| Function | Description |
|----------|-------------|
| `print(...)` | Print values to stdout (variadic, auto-converts types) |
| `sizeof(T)` | Size of type (compile-time) |
| `alignof(T)` | Alignment of type (compile-time) |
| `offsetof(T, field)` | Byte offset of struct field (compile-time) |
| `typeName(T)` | String name of type (compile-time) |
| `panic(msg)` | Fatal error with message |

### 9.2 Standard Library Modules

| Module | Contents |
|--------|----------|
| `std.io` | `print`, `println`, `format`, `read_line` |
| `std.mem` | `alloc`, `free`, `copy`, `zero`, `Pool`, `Arena` |
| `std.str` | `String`, `concat`, `split`, `trim` |
| `std.math` | `sqrt`, `sin`, `cos`, `abs`, `min`, `max` |
| `std.collections` | `Array`, `HashMap`, `Set`, `List`, `Queue` |
| `std.fs` | `File`, `Path`, `Directory` (AetherFS syscalls) |
| `std.serial` | `COM1`, `putc`, `puts` (kernel-mode serial I/O) |
| `std.elf` | ELF64 reader/writer |
| `std.test` | `assert`, `test_runner`, `benchmark` |
| `std.asm` | NASM helper macros |
| `std.arch` | Architecture detection, multi-target helpers |

### 9.3 OS-Specific Functions (Kernel/Binary Targets)

| Function | Description |
|----------|-------------|
| `sys func` | Declare Aether OS syscall at 0x5000 |
| `writePortByte(port, value)` | Write byte to I/O port |
| `readPortByte(port) -> u8` | Read byte from I/O port |
| `disableInterrupts()` | Disable interrupts (CLI) |
| `enableInterrupts()` | Enable interrupts (STI) |
| `haltCpu()` | Halt CPU (HLT) |

---

## 10. Compiler Architecture

### 10.1 Pipeline

```
Source (.ae)
  → Tokenizer (character stream → tokens)
  → Lexer (tokens → token stream with whitespace handling)
  → Parser (token stream → AST)
  → Import Resolution (read, parse, merge imported files)
  → Semantic Analyzer (type checking, name resolution)
  → Optimizer (constant folding, dead code elimination, inlining)
  → LLVM Codegen (AST → LLVM IR)
  → LLVM Backend (LLVM IR → native code)
  → Binary Output (ELF64/Mach-O/PE32+/flat binary)
```

### 10.2 Intermediate Representations

- **AST:** Full syntax tree with source locations (50+ node types)
- **LLVM IR:** Standard LLVM intermediate representation (human-readable, debuggable)

### 10.3 Backends

| Backend | Architecture | Status |
|---------|-------------|--------|
| LLVM x86_64 | x86_64 | Primary (via LLVM) |
| LLVM ARM64 | ARM64 | Free (via LLVM) |
| LLVM RISC-V | RISC-V | Free (via LLVM) |
| LLVM WASM | WebAssembly | Free (via LLVM, future) |

---

## 11. Unique Features (Beyond Standard)

### 11.1 Automatic Bump Allocator

Every function gets a per-function bump arena for temporary allocations. The compiler emits code to initialize the arena on function entry and reset it on exit. This makes short-lived allocations (string concatenation, temporary buffers) essentially free.

### 11.2 String as a First-Class Type

Strings are null-terminated with a length header. The `+` operator concatenates strings when either operand is a string. String interpolation is desugared at compile time.

### 11.3 Pattern Matching

```
match value {
    case 0 -> print("zero\n")
    case 1..9 -> print("small\n")
    case _ -> print("large\n")
}
```

### 11.4 If-Let

```
if let val = optionalValue {
    print("got {val}\n")
}
```

### 11.5 Contracts (Design by Contract)

```
func divide(a: i64, b: i64): i64
    pre(b != 0)
    post(result * b == a)
{
    return a / b
}
```

Contracts are checked at runtime in debug builds, stripped in release builds.

### 11.6 Closures

```
let add = |a: i64, b: i64| -> a + b
let result = add(3, 4)    // 7
```

Non-capturing lambdas are compiled as standalone functions. Capturing closures are planned.

### 11.7 Operator Overloading

```
struct Vector3 {
    x: f32, y: f32, z: f32
}

impl Vector3 {
    func op_add(other: Vector3): Vector3 {
        return Vector3 { x: self.x + other.x, y: self.y + other.y, z: self.z + other.z }
    }
}

let a = Vector3 { x: 1.0, y: 2.0, z: 3.0 }
let b = Vector3 { x: 4.0, y: 5.0, z: 6.0 }
let c = a + b    // calls op_add
```

### 11.8 Generics

```
func max<T>(a: T, b: T): T {
    if a > b { return a }
    return b
}

let m = max(3, 7)     // inferred
```

Generics are monomorphized at compile time — each concrete instantiation generates separate code.

### 11.9 Module System

```
// math.ae
pub func square(x: i64): i64 {
    return x * x
}

// main.ae
import "math.ae"

func main() {
    print(square(7))    // 49
}
```

### 11.10 Universal Binary Support

The compiler can produce a single binary containing code for multiple architectures. The entry point is a thin dispatcher that detects the current architecture and jumps to the correct code section.

---

## 12. Compiler Requirements Summary

### 12.1 Must Have

- [x] Compile Aether source to native code
- [x] Support all targets: host, kernel, boot, binary, module, universal
- [x] Automatic bump allocator per function
- [x] Automatic destructor chains for classes
- [x] Region-based allocation
- [x] `defer` statement
- [x] `try`/`catch`/`throw` with stack unwinding
- [x] Inline assembly blocks
- [x] Classes with constructors, destructors
- [x] Structs and enums
- [x] Traits with static and dynamic dispatch
- [x] Generics with monomorphization
- [x] String interpolation
- [x] Pattern matching
- [x] Optional types with `?T`, `none`, `or`
- [x] If-let
- [x] Closures (non-capturing)
- [x] Operator overloading
- [x] Compile-time evaluation (`#run`)
- [x] Module/import system
- [x] Contracts (pre/post)
- [x] Universal binary output
- [x] Freestanding output (no libc dependency)
- [x] Segfault handling in try blocks (host target)

### 12.2 Should Have

- [x] Compile-time reflection (`@target`, `@arch`, etc.)
- [ ] Cross-module inlining
- [x] Basic optimizer (constant folding, dead code elimination)
- [ ] Debug information output (DWARF)
- [x] Error messages with source locations
- [x] Multi-file compilation
- [x] Linker script generation

### 12.3 Nice to Have

- [ ] IDE integration (LSP)
- [x] Formatter (`aether fmt`)
- [x] Documentation generator (`aether doc`)
- [ ] Package manager
- [x] Test framework (`std/test.ae`)
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

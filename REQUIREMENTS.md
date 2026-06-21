# Aether Compiler — Requirements Specification
## An OOP 4GL for Systems Programming and OS Development

---

## 1. Core Philosophy

Aether is a compiled, statically-typed, object-oriented 4GL designed for systems programming with the ultimate goal of building an operating system. It prioritizes readability, safety, and automatic resource management while providing full low-level control when needed.

**Key Design Principles:**
- **Readability first** — syntax should be self-documenting and easy to follow
- **Automatic memory management** — allocation and freeing baked into compiled code, no runtime GC
- **No interpreter** — everything compiles to native code (ELF64, flat binary)
- **Classes optional, not required** — procedural code is first-class
- **Assembly when you need it** — inline NASM syntax assembly blocks
- **References over pointers** — pointers available but references preferred
- **Think outside the box** — not another C, Rust, or Go clone

---

## 2. Language Features

### 2.1 Type System

**Primitive Types:**
- `byte` — 8-bit unsigned integer
- `u16` — 16-bit unsigned integer
- `u32` — 32-bit unsigned integer
- `u64` — 64-bit unsigned integer (default integer type)
- `i8` — 8-bit signed integer
- `i16` — 16-bit signed integer
- `i32` — 32-bit signed integer
- `i64` — 64-bit signed integer
- `bool` — boolean (true/false)
- `string` — null-terminated string pointer
- `void` — no return value

**Compound Types:**
- Arrays: `[T; N]` — fixed-size array of type T with N elements
- Slices: `[T]` — dynamic-length view into an array
- Pointers: `*T` — raw memory pointer to type T
- References: `&T` — safe reference to type T (preferred over pointers)
- Classes: `class Name { ... }` — user-defined types with methods

**Type Inference:**
- `let x = 42` infers type from initializer
- `let x: u64 = 42` explicit type annotation
- Function return types inferred when possible

### 2.2 Variables and Constants

```aether
// Immutable by default
let x: u64 = 42
let name = "hello"

// Mutable
var counter: u64 = 0
var buffer: [byte; 256]

// Constants (compile-time)
const MAX_SIZE = 4096
const COM1_DATA = 0x3F8
```

### 2.3 Control Flow

```aether
// If/else
if x > 10 {
    do_something()
} else if x > 5 {
    do_other()
} else {
    do_default()
}

// While loop
while true {
    process()
}

// For loop (range-based)
for i in 0..10 {
    serial_putdec(i)
}

// For loop (iterator)
for item in collection {
    process(item)
}

// Match (pattern matching)
match value {
    0 -> handle_zero()
    1..10 -> handle_range()
    _ -> handle_default()
}
```

### 2.4 Functions

```aether
// Simple function
func greet() {
    serial_puts("Hello!")
}

// Function with parameters and return type
func add(a: u64, b: u64): u64 {
    return a + b
}

// Function with multiple returns
func divide(a: u64, b: u64): (u64, u64) {
    return (a / b, a % b)
}

// Default parameters
func open(path: string, flags: u64 = 0): u64 {
    return sys_open(path, flags)
}

// Variadic functions
func print(fmt: string, args: ...) {
    // format and print
}
```

### 2.5 Classes (Optional OOP)

```aether
class File {
    // Fields
    var fd: u64
    var path: string
    var pos: u64

    // Constructor
    new(path: string) {
        self.fd = sys_open(path, 0)
        self.path = path
        self.pos = 0
    }

    // Methods
    func read(buf: &[byte], len: u64): u64 {
        let n = sys_read(self.fd, buf, len)
        self.pos += n
        return n
    }

    func write(data: &[byte]): u64 {
        return sys_write(self.fd, data)
    }

    func seek(offset: u64) {
        self.pos = offset
        sys_seek(self.fd, offset)
    }

    // Destructor (automatic — called when object goes out of scope)
    drop() {
        sys_close(self.fd)
    }
}

// Usage
func process_file() {
    let f = File.new("/etc/config")  // allocated on stack
    f.read(buf, 256)
    f.write(output)
    // f.drop() called automatically when scope exits
}
```

**Class Features:**
- Single inheritance with `extends`
- Interface/trait system with `impl`
- Virtual methods with `virtual` keyword
- Automatic constructor/destructor chaining
- No manual memory management for class instances

### 2.6 Automatic Memory Management

Aether uses **compile-time lifetime analysis** and **arena-based allocation** — no garbage collector, no reference counting at runtime.

**Stack allocation (default):**
```aether
func example() {
    let x: u64 = 42          // stack
    let buf: [byte; 256]     // stack (fixed size)
    let f = File.new()       // stack-allocated with inline storage
}
```

**Heap allocation (explicit):**
```aether
func example() {
    let buf = alloc(1024)    // heap, freed at scope exit
    let large = new BigStruct()  // heap, freed at scope exit
}
```

**Memory safety guarantees:**
- No use-after-free (lifetime analysis at compile time)
- No double-free (each allocation freed exactly once)
- No memory leaks (all allocations tracked and freed)
- Deterministic destruction (destructors called at scope exit)
- No dangling references (borrow checking)

### 2.7 References and Pointers

```aether
// References (preferred) — safe, checked at compile time
func process(data: &[byte]) {
    // data is a reference, cannot be null
    let first = data[0]
}

// Pointers — available when needed (hardware access, FFI)
func write_to_port(addr: *u64, val: u64) {
    // pointer arithmetic allowed
    addr[0] = val
}

// Nullable references
func find(name: string): ?&File {
    // returns either a reference or null
}
```

### 2.8 Inline Assembly (NASM Syntax)

```aether
func serial_putc(c: byte) {
    asm {
        mov dx, COM1_LSR
    .wait:
        in al, dx
        test al, LSR_THR_EMPTY
        jz .wait
        mov dx, COM1_DATA
        mov al, dil      ; SysV ABI: first arg in rdi/edi/dil
        out dx, al
    }
}
```

**Assembly Integration:**
- Full NASM syntax supported
- Access to function parameters via SysV ABI registers (rdi, rsi, rdx, rcx, r8, r9)
- Labels, sections, directives all work
- Can call other Aether functions from asm blocks
- Can access global symbols and constants
- Compiler handles function prologue/epilogue — asm blocks should NOT contain `ret`

### 2.9 Exception Handling

```aether
// Try-catch blocks
func read_file(path: string): [byte] {
    try {
        let f = File.open(path)
        let data = f.read_all()
        return data
    } catch FileError {
        serial_puts("File not found!")
        return []
    } catch IOError {
        serial_puts("IO error!")
        return []
    } finally {
        serial_puts("read_file completed")
    }
}

// Defer (execute on scope exit, like Go)
func process() {
    let f = File.open("data")
    defer f.close()     // runs when function exits
    
    // ... work with f ...
    // f.close() called automatically
}

// Error propagation
func risky(): u64 throws {
    let result = try_something()
    if result == 0 {
        throw Error("Something went wrong")
    }
    return result
}
```

### 2.10 Generics

```aether
// Generic function
func max<T>(a: T, b: T): T {
    if a > b { return a }
    return b
}

// Generic class
class Stack<T> {
    var data: [T]
    var count: u64
    
    new(capacity: u64) {
        self.data = alloc(capacity * sizeof(T))
        self.count = 0
    }
    
    func push(item: T) {
        self.data[self.count] = item
        self.count += 1
    }
    
    func pop(): T {
        self.count -= 1
        return self.data[self.count]
    }
}
```

### 2.11 Modules and Namespaces

```aether
// Module declaration
module fs

// Public/private visibility
pub func open(path: string): u64 { ... }
func internal_helper() { ... }  // private by default

pub const BLOCK_SIZE = 512

// Import
import fs
import io as stdio
import "path/to/module.ae"
```

### 2.12 Concurrency

```aether
// Spawn a task (lightweight, no OS thread)
func worker(id: u64) {
    serial_puts("Worker started")
}

spawn worker(1)
spawn worker(2)

// Synchronization primitives
var lock: Mutex
lock.acquire()
// critical section
lock.release()

// Channels
let ch: Chan<u64>
ch.send(42)
let val = ch.recv()
```

### 2.13 Compile-Time Features

```aether
// Compile-time evaluation
const TABLE_SIZE = 256
const LOOKUP_TABLE: [u64; TABLE_SIZE] = compute_table()

func compute_table(): [u64; TABLE_SIZE] {
    var result: [u64; TABLE_SIZE]
    for i in 0..TABLE_SIZE {
        result[i] = i * i
    }
    return result
}

// Conditional compilation
#[target = "kernel"]
func kernel_only() { ... }

#[target = "boot"]
func boot_only() { ... }

// Compile-time assertions
#assert(sizeof(u64) == 8)
#assert(MAX_SIZE > 0)
```

### 2.14 Attributes and Metadata

```aether
// Entry point
#[entry]
func _start() { ... }

// Interrupt handler
#[interrupt]
func timer_handler() { ... }

// Inline control
#[inline(always)]
func fast_path() { ... }

// Alignment
#[align(4096)]
var page_table: [byte; 4096]

// Section placement
#[section(".text.boot")]
func early_init() { ... }

// Packed struct
#[packed]
class Register {
    var data: u32
    var flags: u16
}
```

### 2.15 String Interpolation

```aether
let name = "World"
serial_puts("Hello, {name}!")     // "Hello, World!"
serial_puts("Value: {x + y}")     // "Value: 42"
serial_puts("Hex: {x:#x}")        // "Hex: 0x2a"
```

### 2.16 Operator Overloading

```aether
class Vector2 {
    var x: i64
    var y: i64
    
    func +(other: &Vector2): Vector2 {
        return Vector2(self.x + other.x, self.y + other.y)
    }
    
    func *(scalar: i64): Vector2 {
        return Vector2(self.x * scalar, self.y * scalar)
    }
}
```

### 2.17 Pattern Matching and Destructuring

```aether
// Destructure tuples
let (x, y) = get_coords()

// Destructure arrays
let [first, second, ..rest] = data

// Match with guards
match value {
    x if x > 100 -> handle_large(x)
    x if x < 0 -> handle_negative(x)
    _ -> handle_default()
}
```

### 2.18 Enum Types

```aether
enum Result {
    Success,
    Error(code: u64, message: string),
    Timeout,
}

func process(): Result {
    if ok {
        return Result.Success
    }
    return Result.Error(42, "something broke")
}

match result {
    Result.Success -> handle_ok()
    Result.Error(code, msg) -> handle_error(code, msg)
    Result.Timeout -> handle_timeout()
}
```

### 2.19 Bit Fields and Bit Operations

```aether
// Bit field access
var flags: u64
flags[0] = 1       // set bit 0
flags[3..7] = 0x1F // set bits 3-7

// Bit operations
let masked = value & 0xFF
let shifted = value << 4
let combined = flags | mask
```

### 2.20 Compiler Targets

```aether
// --target boot: flat binary, 16/32-bit, no ELF
// --target kernel: ELF64, linked at 0x1004000
// --target binary: ELF64 standalone binary
// --target host: ELF64 for the host OS (Linux/macOS)
```

---

## 3. Compiler Architecture

### 3.1 Pipeline

```
Source (.ae)
    → Tokenizer (lexer)
    → Parser (AST construction)
    → Semantic Analyzer (type checking, lifetime analysis)
    → Optimizer (constant folding, DCE, inlining)
    → Code Generator (NASM assembly emission)
    → Assembler (NASM → object file)
    → Linker (object → ELF64/flat binary)
```

### 3.2 Memory Management Strategy

**Stack allocation:**
- All local variables and objects with known size at compile time
- Objects destroyed in reverse order of construction (RAII)
- No runtime overhead for allocation/freeing

**Arena allocation:**
- Objects that escape their scope or are too large for stack
- Arena freed when all references are dropped
- Linear allocation, no fragmentation

**Compile-time analysis:**
- Lifetime tracking for all references
- Borrow checking (mutable XOR shared)
- Escape analysis to determine stack vs arena
- No runtime GC, no reference counting

### 3.3 Code Generation

- Direct NASM output (no intermediate representation)
- SysV ABI calling convention (x86-64)
- Function prologue: `push rbp; mov rbp, rsp; sub rsp, N`
- Function epilogue: `mov rsp, rbp; pop rbp; ret`
- All registers preserved across function calls (callee-saved)
- Assembly blocks emitted verbatim into function body

### 3.4 Optimization Passes

1. **Constant folding** — evaluate constant expressions at compile time
2. **Dead code elimination** — remove unreachable code and unused variables
3. **Constant propagation** — replace variables with known constants
4. **Inlining** — inline small functions
5. **Loop unrolling** — unroll loops with known iteration counts
6. **Tail call optimization** — convert tail recursion to jumps
7. **Strength reduction** — replace expensive operations with cheaper ones

---

## 4. OS Development Requirements

### 4.1 Boot Chain Support

- Stage 1 (MBR, 512 bytes, flat binary)
- Stage 2 (loader, 16KB, flat binary)
- Boot entry (long mode setup, 16KB padded)
- Kernel (ELF64, linked at 0x1004000)

### 4.2 Hardware Access

- Port I/O (in/out instructions)
- Memory-mapped I/O
- Interrupt handling (IDT, IVT)
- Page table management
- ATA PIO disk access
- PCI enumeration

### 4.3 Kernel Features

- Physical page allocator (bitmap-based)
- Virtual memory manager (paging)
- ELF64 loader
- Module/registry system
- Syscall interface
- Boot filesystem (read-only)
- Serial console I/O
- Process/task management
- Interrupt handling

---

## 5. Future Considerations

### 5.1 Language Features (Phase 2+)

- [ ] Generics (monomorphization)
- [ ] Closures and lambdas
- [ ] Coroutines/async
- [ ] Compile-time reflection
- [ ] Documentation generator
- [ ] Package manager
- [ ] Language server (LSP)
- [ ] Formatter
- [ ] Debug information (DWARF)
- [ ] Cross-compilation
- [ ] WASM backend
- [ ] ARM64 backend

### 5.2 Tooling

- [ ] Build system integration
- [ ] Test framework
- [ ] Benchmarking harness
- [ ] Profiling support
- [ ] Code coverage
- [ ] Documentation system

### 5.3 Standard Library

- [ ] Collections (vectors, maps, sets)
- [ ] String manipulation
- [ ] File I/O
- [ ] Networking
- [ ] Cryptography
- [ ] Compression
- [ ] Serialization
- [ ] Time/date
- [ ] Math library
- [ ] Testing utilities

---

## 6. Non-Goals

- Garbage collection at runtime
- Reference counting
- Interpreter mode
- Dynamic typing
- Reflection at runtime
- JIT compilation
- Automatic parallelization
- Managed runtime environment
- Bytecode/VM execution

---

## 7. Success Criteria

1. Aether compiler can compile itself (self-hosting)
2. Aether OS boots and runs a shell
3. Aether OS can load and execute ELF64 binaries
4. Aether OS has working file system
5. Aether OS has working process management
6. Aether OS has working networking
7. All memory management is automatic — no leaks, no crashes
8. Inline assembly works seamlessly for hardware access
9. Classes provide clean OOP without runtime overhead
10. The language is pleasant to read and write

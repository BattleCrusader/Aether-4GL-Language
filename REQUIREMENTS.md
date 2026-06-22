# Aether Compiler — Comprehensive Requirements Specification
## An OOP 4GL for Systems Programming and OS Development

**Version**: 1.0
**Status**: Living Document
**Last Updated**: 2026-06-22

---

## Table of Contents

1. [Core Philosophy](#1-core-philosophy)
2. [Language Design Goals](#2-language-design-goals)
3. [Type System](#3-type-system)
4. [Variables and Bindings](#4-variables-and-bindings)
5. [Control Flow](#5-control-flow)
6. [Functions](#6-functions)
7. [Memory Management](#7-memory-management)
8. [Object-Oriented Programming](#8-object-oriented-programming)
9. [Generics and Templates](#9-generics-and-templates)
10. [Error Handling](#10-error-handling)
11. [Inline Assembly](#11-inline-assembly)
12. [Compile-Time Execution](#12-compile-time-execution)
13. [Contract Programming](#13-contract-programming)
14. [Closures and Lambdas](#14-closures-and-lambdas)
15. [Properties and Operator Overloading](#15-properties-and-operator-overloading)
16. [Dynamic Dispatch](#16-dynamic-dispatch)
17. [Modules and Namespaces](#17-modules-and-namespaces)
18. [Concurrency and Fibers](#18-concurrency-and-fibers)
19. [Aether OS Integration](#19-aether-os-integration)
20. [Multi-Target Assembler](#20-multi-target-assembler)
21. [Universal Binaries](#21-universal-binaries)
22. [Standard Library](#22-standard-library)
23. [Build System and CLI](#23-build-system-and-cli)
24. [Compiler Targets](#24-compiler-targets)
25. [Optimization](#25-optimization)
26. [Self-Hosting](#26-self-hosting)
27. [Unique Innovations (Think Outside the Box)](#27-unique-innovations-think-outside-the-box)
28. [OS Pipeline Mapping](#28-os-pipeline-mapping)
29. [Implementation Phases](#29-implementation-phases)

---

## 1. Core Philosophy

Aether is a compiled, statically-typed, object-oriented 4GL designed for systems programming with the ultimate goal of building an operating system. It prioritizes readability, safety, and automatic resource management while providing full low-level control when needed.

### 1.1 Key Design Principles

- **Readability first** — syntax should be self-documenting and easy to follow. No sigils, no cryptic operators, no template metaprogramming.
- **Automatic memory management** — allocation and freeing baked into compiled code, no runtime GC, no reference counting overhead.
- **No interpreter** — everything compiles to native code (ELF64, flat binary, Mach-O). No VM, no JIT, no bytecode.
- **Classes optional, not required** — procedural code with flat structs and free functions is always first-class.
- **Assembly when you need it** — full NASM syntax inline assembly blocks, not GCC-style extended asm.
- **References over pointers** — pointers available for hardware access but references preferred for safety.
- **Think outside the box** — not another C, Rust, or Go clone. Unique features: compile-time OS knowledge, goal-oriented I/O, pattern-based metaprogramming, automatic protocol generation, capability-based security, compile-time unit checking.
- **OS-native from day one** — every feature exists because the Aether OS needs it: syscall tables, kernel modules, boot sectors, flat binaries.
- **Multi-architecture by default** — same source targets x86_64, ARM64, and RISC-V via the multi-target assembler.

### 1.2 What Makes Aether Unique

- **Compile-time `#run` blocks** execute code during compilation, enabling metaprogramming without a separate macro system
- **Contract programming** with `pre()` and `post()` conditions checked in debug builds, eliminated in release
- **Multi-target assembler** translates NASM assembly to ARM64 and RISC-V automatically
- **Universal binaries** contain native code for multiple architectures with a CPU detection trampoline
- **Deterministic exceptions** use tagged union returns — no unwinding tables, no personality functions
- **Region-based allocation** with `region { }` blocks for O(1) arena teardown
- **`sys func`** for direct syscall page invocation in kernel space
- **`self` is implicit** in methods — never write it in the parameter list
- **Properties inferred from return type** — getter has return type, setter has no return type
- **String interpolation** with `{expr}` in strings, auto-converts numerics via `__aether_itoa`
- **`+` does string concat** when either operand is a string (like Python/JS)
- **Import resolution** — `import "path.ae"` merges declarations at compile time
- **Compile-time OS knowledge** — compiler knows the Aether OS memory map, syscall table, boot chain
- **Goal-oriented I/O** — describe what you want, compiler generates the optimal path
- **Pattern-based metaprogramming** — pattern matching on types instead of templates
- **Query-style data transformations** — chain operations fused into single loop
- **Automatic protocol generation** — declare hardware protocols, compiler generates bit-banging
- **Compile-time hardware configuration** — detect hardware at compile time
- **Self-documenting binary format** — ELF note sections with metadata
- **Capability-based security** — functions declare what capabilities they need
- **Automatic boot chain generation** — describe boot chain declaratively
- **Zero-cost error context** — `?` operator with debug/release modes
- **Compile-time unit checking** — physical units tracked at compile time
- **Automatic interrupt handler generation** — declarative interrupt handlers

---

## 2. Language Design Goals

### 2.1 Syntax Goals

- **Python-like readability**: significant indentation (4 spaces), no braces for blocks, no semicolons required
- **Self-documenting**: type annotations, named parameters, clear error messages
- **Minimal keywords**: ~40 keywords, no sigils, no special characters for types
- **Consistent**: uniform syntax for functions, methods, closures, and generics

### 2.2 Safety Goals

- **Memory safety**: no use-after-free, no double-free, no memory leaks, no dangling references
- **Type safety**: strong static typing with type inference, no implicit conversions that lose data
- **Null safety**: no null pointers — use `T?` optionals with `if let` pattern matching
- **Exception safety**: deterministic exceptions with zero-cost happy path
- **Contract safety**: pre/post conditions checked in debug builds

### 2.3 Performance Goals

- **Zero-cost abstractions**: high-level features compile to the same assembly you'd write by hand
- **No hidden allocation**: all heap allocations are explicit or compiler-verified
- **Deterministic destruction**: destructors called at scope exit, no GC pauses
- **Optimization**: constant folding, DCE, inlining, escape analysis, loop optimization
- **Multi-target**: same source, optimal code for each architecture

### 2.4 OS Development Goals

- **Freestanding**: no libc, no CRT, no OS assumptions
- **Boot chain support**: stage1 (512B MBR), stage2 (16KB loader), long mode entry
- **Kernel support**: ELF64 kernel at 0x1000000 with linker script
- **Module support**: `.ko` ELF modules with ABI checking
- **Binary support**: userland ELF at 0x2000000 with syscall page
- **Syscall page**: direct calls to 0x5000 function table
- **Module registry**: interface to 0x4000 service table

---

## 3. Type System

### 3.1 Primitive Types

| Type | Size | Description |
|------|------|-------------|
| `byte` | 8 bits | Unsigned byte (alias for u8) |
| `u8` | 8 bits | Unsigned 8-bit integer |
| `u16` | 16 bits | Unsigned 16-bit integer |
| `u32` | 32 bits | Unsigned 32-bit integer |
| `u64` | 64 bits | Unsigned 64-bit integer (default integer type) |
| `i8` | 8 bits | Signed 8-bit integer |
| `i16` | 16 bits | Signed 16-bit integer |
| `i32` | 32 bits | Signed 32-bit integer |
| `i64` | 64 bits | Signed 64-bit integer |
| `f32` | 32 bits | IEEE 754 single-precision float |
| `f64` | 64 bits | IEEE 754 double-precision float |
| `bool` | 8 bits | Boolean (true/false) |
| `string` | 64 bits | Null-terminated string pointer |
| `void` | 0 bits | No return value |
| `type` | — | Type identifier (compile-time only) |

### 3.2 Compound Types

| Syntax | Description | Example |
|--------|-------------|---------|
| `[T; N]` | Fixed-size array | `[u64; 4]` |
| `[T]` | Dynamic slice | `[byte]` |
| `*T` | Raw pointer | `*u64` |
| `&T` | Reference | `&File` |
| `ref T` | Safe reference | `ref u64` |
| `owned T` | Owned reference | `owned Buffer` |
| `rc T` | Reference-counted | `rc SharedData` |
| `T?` | Optional | `u64?` |
| `func(T): U` | Function type | `func(u64): bool` |
| `(T, U)` | Tuple | `(u64, string)` |

### 3.3 Type Aliases

```aether
type Result = u64
type ErrorCode = i32
type Callback = func(u64): bool
```

### 3.4 Type Inference

```aether
let x = 42              // inferred as u64
let y: u64 = 42         // explicit type annotation
let z = "hello"         // inferred as string
let w = some_func()     // inferred from return type
```

### 3.5 Type Conversion

```aether
let x: u64 = 42
let y: u8 = u8(x)       // explicit cast
let z: u64 = u64(y)     // zero-extend
let a: i64 = i64(x)     // reinterpret as signed
```

---

## 4. Variables and Bindings

### 4.1 Let Bindings (Immutable by Default)

```aether
let x: u64 = 42          // explicit type
let y = 42               // type inferred
const MAX = 100          // compile-time constant
```

### 4.2 Var Bindings (Mutable)

```aether
var counter: u64 = 0      // mutable
var buffer: [byte; 256]  // mutable array
counter = 1              // ok
```

### 4.3 Scope and Shadowing

```aether
{
    let inner = 5
    // inner accessible here
}
// inner not accessible here

let x = 10
let x = x + 5   // shadows previous x, now 15
```

### 4.4 Destructuring

```aether
let (x, y) = get_coords()          // tuple destructuring
let [first, second, ..rest] = data  // array destructuring
let Point { x, y } = point         // struct destructuring
```

---

## 5. Control Flow

### 5.1 If / Elif / Else

```aether
if x > 10 {
    do_something()
} elif x > 5 {
    do_other()
} else {
    do_default()
}

// If as expression
let status = if x > 0 { "positive" } else { "non-positive" }
```

### 5.2 While Loops

```aether
while condition {
    do_work()
}

// Infinite loop
while true {
    process()
}
```

### 5.3 For Loops

```aether
// Range loop (exclusive)
for i in 0..10 {
    print(i)  // 0 through 9
}

// Inclusive range
for i in 0..=10 {
    print(i)  // 0 through 10
}

// Array iteration
let arr = [1, 2, 3, 4, 5]
for val in arr {
    print(val)
}

// With index
for i, val in arr {
    print("arr[{i}] = {val}")
}
```

### 5.4 Break and Continue

```aether
for i in 0..100 {
    if i == 50 { break }
    if i % 2 == 0 { continue }
    print(i)  // odd numbers 1..49
}
```

### 5.5 Match Statements

```aether
match value {
    0 -> handle_zero()
    1..9 -> handle_range()
    > 100 -> handle_big()
    string(s) -> handle_string(s)
    _ -> handle_default()
}

// Match as expression
let description = match value {
    0 -> "zero"
    1..9 -> "small"
    _ -> "large"
}

// Match with guards
match value {
    x if x > 100 -> handle_large(x)
    x if x < 0 -> handle_negative(x)
    _ -> handle_default()
}
```

### 5.6 Defer

```aether
func process_file(path: string) {
    let f = File.open(path)
    defer { f.close() }
    // f.close() is called when this scope exits,
    // regardless of how it exits (return, error, break)
    f.write(data)
}
```

---

## 6. Functions

### 6.1 Function Declaration

```aether
// Basic function
func add(a: u64, b: u64): u64 {
    return a + b
}

// Expression-bodied function
func add(a: u64, b: u64): u64 -> a + b

// Void return
func greet(name: string) {
    print("Hello, {name}")
}

// Multiple return values via tuple
func divide(a: u64, b: u64): (u64, u64) {
    return (a / b, a % b)
}
```

### 6.2 Function Attributes

```aether
// Inline hint
inline func fast_path(): u64 { return 42 }

// Force inline
@force_inline func always_inline(): u64 { return 42 }

// Prevent inlining
@no_inline func debug_trace() { ... }

// Export for module loader
@export func module_init() { ... }

// Entry point at specific address
@entry(0x100000) func kernel_main() { ... }

// Capability requirements
@requires(io, mem) func write_disk(sector: u64, data: ref [u8]) { ... }
```

### 6.3 Default Parameters

```aether
func create_window(title: string, width: u64 = 800, height: u64 = 600) {
    // ...
}

create_window("Hello")                    // uses defaults
create_window("Wide", 1024, 768)          // explicit
```

### 6.4 Variadic Functions

```aether
func sum(values: ...u64): u64 {
    let total = 0
    for v in values {
        total += v
    }
    return total
}

let s = sum(1, 2, 3, 4, 5)  // s = 15
```

### 6.5 Named and Optional Parameters

```aether
func configure(host: string, port: u64 = 8080, ssl: bool = false) {
    // ...
}

configure("localhost")                          // all defaults
configure("example.com", port: 443, ssl: true)  // named
```

### 6.6 Function Pointers

```aether
type Handler = func(u64): bool

func register_handler(h: Handler) {
    // ...
}

register_handler(my_callback)
```

---

## 7. Memory Management

This is the heart of the language and what makes it a true 4GL — **you describe allocation semantics; the compiler generates the exact free/teardown code.**

### 7.1 Stack Allocation (Default)

All local variables are stack-allocated by default. The compiler tracks lifetimes and generates destruction at the end of scope.

```aether
func process() {
    let p = Point(3, 4)   // stack allocated
    let items = [1, 2, 3] // fixed-size array, stack
    // compiler inserts destructor calls at scope exit
}
```

### 7.2 Escape Analysis

When a reference to a stack variable could outlive its scope, the compiler **automatically promotes** it to heap allocation. No programmer annotation needed for simple cases.

```aether
func make_point(x: int, y: int): ref Point {
    let p = Point(x, y)
    // compiler detects: p's reference escapes this frame
    // auto-promotes p to heap
    return p
}
```

### 7.3 Explicit Heap (`heap` keyword)

```aether
let big = heap Buffer(1024 * 1024)
let shared = heap rc SharedState()
```

### 7.4 Ownership and Borrowing

- `owned T` — single-owner, moved on assignment, freed when owner drops
- `ref T` — borrowed reference, non-owning, must not outlive the lender
- `rc T` — reference-counted shared ownership, freed when count reaches zero

```aether
func consume(val: owned Buffer) {   // val will be freed at end
    process(val)
}  // val freed here

func observe(val: ref Buffer) {     // borrow, no ownership
    print(val.size())
}  // nothing freed, borrow ends
```

### 7.5 Region-Based Allocation

For kernel modules and performance-critical code, the language supports **region-based allocation** that maps directly to the Aether OS capability model.

```aether
region("kernel") {
    let p = Page()   // allocated from kernel region
    let buf = Buffer(256)
}  // entire region freed at once — O(1) teardown
```

### 7.6 No Null Pointers

The type system has no null. Use `T?` (optional) instead.

```aether
let x: int? = none
x = 42

if let val = x {
    print(val)  // val is int, not int?
}

// Unwrap with default
let y = x or 0
```

### 7.7 Pointers (Opt-In, Unsafe)

Raw pointers exist for hardware interaction, DMA, and inline assembly. They require an `unsafe` block.

```aether
func read_mmio(addr: ptr u64): u64 {
    unsafe {
        return *addr
    }
}
```

### 7.8 Automatic Destructor Insertion

The compiler tracks every class instance and inserts destructor calls at every scope exit point — normal return, early return, exception propagation, loop break/continue.

```aether
func read_config(): throws string {
    let f = File("/etc/aether.cfg")  // constructor called
    let content = f.read_all()       // use the file
    // compiler inserts: f.drop() — even if f.read_all() threw
    return content
}
```

### 7.9 Bump Allocator Runtime

The compiler emits a built-in bump allocator for runtime allocations:

- 64KB arena, O(1) allocation, auto-grow
- Used by `__aether_concat`, `__aether_itoa`, and heap allocations
- No free list, no fragmentation — just bump a pointer
- Arena reset at scope exit for region-based allocations

---

## 8. Object-Oriented Programming

### 8.1 Structs

```aether
struct Point {
    x: u64
    y: u64
}

let p = Point { x: 10, y: 20 }
let x = p.x
```

### 8.2 Enums

```aether
enum Error {
    NotFound
    PermissionDenied
    InvalidInput(string)
    IoError(u64, string)
}

let err = Error.NotFound
let custom = Error.InvalidInput("bad data")
```

### 8.3 Classes

Classes are syntactic sugar over structs + associated functions. The compiler tracks constructors (`new`) and destructors (`drop`) and inserts calls automatically.

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

### 8.4 Implicit `self`

`self` is **never written in the parameter list**. The parser auto-injects `self: ref Type` as the first parameter for methods defined inside struct/class bodies.

```aether
class Point {
    x: int
    y: int

    // self is implicit — never write it in params
    func distance(other: ref Point): float {
        let dx = self.x - other.x
        let dy = self.y - other.y
        return sqrt(dx*dx + dy*dy)
    }
}
```

### 8.5 Classes Are Not Required

Pure procedural code with flat structs and free functions is always valid:

```aether
struct Point { x: int; y: int }

func distance(p: Point): float {
    return sqrt(p.x * p.x + p.y * p.y)
}
```

### 8.6 Traits (Interfaces)

Traits define shared behavior. They compile to vtable pointers only when used dynamically; static dispatch is the default.

```aether
trait Drawable {
    func draw()
    func area(): f64
}

impl Drawable for Circle {
    func draw(self) {
        print("Drawing circle")
    }

    func area(self): f64 {
        return 3.14159 * self.radius * self.radius
    }
}

// Static dispatch (zero-cost)
func render_static(items: [ref Drawable]) {
    for item in items {
        item.draw()
    }
}

// Dynamic dispatch (vtable)
func render_dynamic(items: [ref dyn Drawable]) {
    for item in items {
        item.draw()
    }
}
```

### 8.7 Access Modifiers

```aether
class BankAccount {
    pub balance: u64       // accessible anywhere
    private owner: string  // accessible only within this class
    internal pin: u64      // accessible within the same module

    pub func deposit(amount: u64) {
        self.balance += amount
    }

    private func validate_pin(input: u64): bool {
        return self.pin == input
    }
}
```

### 8.8 Inheritance

```aether
class Animal {
    name: string

    new(name: string) {
        self.name = name
    }

    func speak() {
        print("{self.name} makes a sound")
    }
}

class Dog extends Animal {
    breed: string

    new(name: string, breed: string) {
        super.new(name)
        self.breed = breed
    }

    func speak() {
        print("{self.name} barks!")
    }
}
```

### 8.9 Properties (Inferred from Return Type)

Properties are methods that look like fields. The compiler infers getter/setter from the return type:

- **Getter**: has a return type → `obj.prop` calls the getter
- **Setter**: has no return type → `obj.prop = value` calls the setter

```aether
class Temperature {
    celsius: f64

    // Getter — has return type
    prop fahrenheit(self): f64 {
        return self.celsius * 9.0 / 5.0 + 32.0
    }

    // Setter — no return type
    prop fahrenheit(self, value: f64) {
        self.celsius = (value - 32.0) * 5.0 / 9.0
    }
}

let t = Temperature { celsius: 100 }
print(t.fahrenheit)  // calls getter → 212.0
t.fahrenheit = 32    // calls setter → celsius = 0
```

### 8.10 Operator Overloading

```aether
struct Vector2 {
    x: f64
    y: f64
}

impl Vector2 {
    func op_add(self, other: Vector2): Vector2 {
        return Vector2 { x: self.x + other.x, y: self.y + other.y }
    }

    func op_sub(self, other: Vector2): Vector2 {
        return Vector2 { x: self.x - other.x, y: self.y - other.y }
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
let c = a + b          // calls op_add
let d = a * 2.0        // calls op_mul
```

---

## 9. Generics and Templates

### 9.1 Generic Functions

Generics are monomorphized (zero-cost). Type parameters use angle brackets.

```aether
func identity<T>(value: T): T {
    return value
}

func swap<T>(a: ref T, b: ref T) {
    let tmp = *a
    *a = *b
    *b = tmp
}

func max<T>(a: T, b: T): T {
    if a > b { return a }
    return b
}
```

### 9.2 Generic Classes

```aether
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

### 9.3 Generic Traits

```aether
trait Comparable<T> {
    func less_than(self: ref Self, other: ref T): bool
}

impl<T> Comparable<T> for u64 {
    func less_than(self: ref u64, other: ref u64): bool {
        return self < other
    }
}
```

### 9.4 Type Constraints

```aether
func min<T: Comparable>(a: T, b: T): T {
    if a.less_than(b) { return a }
    return b
}
```

---

## 10. Error Handling

### 10.1 Deterministic Exceptions with Full Unwinding

Exceptions are **deterministic** — no unwinding tables, no personality routines, no runtime type lookups. The compiler encodes exceptions as tagged union returns (rax=value/discriminant, rdx=error tag). The happy path has zero overhead.

Every function that can throw (`throws` annotation) returns a 2-register result:
- **rax**: return value (or error discriminant on error)
- **rdx**: error tag (0 = success, 1 = error)

The compiler generates **scope cleanup tables** for every try block. Each cleanup table entry records what objects need destructor calls, what `defer` blocks need execution, and the scope nesting level.

#### How `throw` Works

```
throw expr
  → evaluate expr into rax (error discriminant)
  → set rdx = 1 (error tag)
  → walk the current function's cleanup table:
      for each live scope (innermost first):
        call destructors for all live objects in that scope
        execute defer blocks in that scope
  → if inside a try block: jump to catch handler
  → if not inside a try block: return (rdx=1, rax=error)
```

#### How `try/catch/finally` Works

```aether
func divide(a: u64, b: u64): u64 throws {
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
        print("IO error: {e}")
    } catch _ {
        print("Unknown error caught")
    } finally {
        print("compute completed")
    }
}
```

Codegen for try/catch:
```
; Clear error tag
xor rdx, rdx

; Emit try body with cleanup table tracking
[try body code]

; Check error tag
test rdx, rdx
jnz .catch_label
jmp .finally_label

.catch_label:
; Walk cleanup table for this try block
[cleanup code — destructors + defers]

; Match error discriminant against catch arms
push rax  ; save error discriminant
[for each catch arm:
    if type specified: compare rax against variant discriminant
    if match: pop rax, bind error value to var, execute handler
    if no match: check next arm]
[catch-all (_): always matches]
[no match: re-throw (rdx=1, rax=discriminant, return)]

.finally_label:
[finally body]

.end:
```

### 10.2 Custom Error Types

```aether
enum MathError {
    DivisionByZero
    Overflow
    NegativeInput(i64)
}

func safe_sqrt(x: i64): i64 throws {
    if x < 0 {
        throw MathError.NegativeInput(x)
    }
    return sqrt(x)
}
```

### 10.3 Error Propagation

A `throws` function that doesn't catch an error:
1. Its own cleanup table is walked (destructors + defers for the function's scope)
2. Returns with rdx=1, rax=error discriminant
3. The caller checks rdx after the call:
   - rdx=0: success, use rax as return value
   - rdx=1: error, rax has discriminant, propagate or catch

```aether
func read_config(): throws Config {
    let data = read_file("/etc/aether.cfg")  // propagates errors
    return parse_config(data)
}

func caller(): u64 throws {
    let result = callee()  // after call, check rdx
    // if rdx=1, propagate (cleanup + return with rdx=1)
    return result
}
```

### 10.4 Catch-All

```aether
try {
    risky_operation()
} catch _ {
    print("Something went wrong, but we don't care what")
}
```

### 10.5 Nested Try/Catch

```aether
try {
    try {
        inner_operation()
    } catch InnerError {
        print("inner failed")
    }
} catch OuterError {
    print("outer failed")
}
```

### 10.6 Segfault Handling (Hardware Faults → Exceptions)

Aether can catch hardware faults (page faults, general protection faults) inside try blocks.

**For kernel/freestanding targets:**
The compiler generates IDT entries for page fault (interrupt 14) and general protection fault (interrupt 13). When a fault occurs:
1. The CPU pushes error code + return address onto the stack
2. The IDT handler saves all registers
3. Checks if the fault address is within a try block's range (using a runtime fault table)
4. If yes: sets rdx=1, rax=fault_type, unwinds to the catch handler
5. If no: calls the default fault handler (panic)

The **runtime fault table** is a simple array of [try_start, try_end, catch_addr] entries generated by the compiler for each try block. The fault handler walks this table to find the matching catch handler.

```aether
try {
    let ptr: *u64 = 0
    let val = *ptr  # page fault!
} catch Segfault {
    print("caught segfault - null pointer dereference")
}
```

**For host-native targets:**
Uses `sigaction` for SIGSEGV/SIGBUS + `sigsetjmp`/`siglongjmp` via a C helper:

The compiler emits a call to `aether_setJmpBuf()` before the try body. This C function calls `sigsetjmp(buf, 0)` (no signal mask save — combined with `SA_NODEFER` in sigaction to prevent re-entrancy). On first call, it returns 0 and the try body executes. If a hardware fault occurs, the signal handler prints a stacktrace and calls `siglongjmp()`, which makes `aether_setJmpBuf()` return non-zero. The generated assembly checks this return value and jumps to the catch handler.

```aether
try {
    let ptr: *u64 = 0
    let val = *ptr  # SIGSEGV!
} catch _ {
    print("caught segfault - null pointer dereference")
}
```

Generated codegen:
```asm
; sigsetjmp for hardware fault catch
lea rdi, [rel __aether_segfault_jmpbuf]
call aether_setJmpBuf
test eax, eax
jnz .catch_label    ; segfault caught, jump to catch

; Clear error tag
xor rdx, rdx

; Try body
[body code]

; Clear segfault jmpbuf
call aether_clearJmpBuf

; Check error tag (rdx = 0 success, 1 = error)
test rdx, rdx
jnz .catch_label
jmp .end

.catch_label:
[catch handler code]

.end:
```

The compiler also emits a **source map table** (`aether_source_map`) in `.rodata` that maps instruction addresses to source file/line/col. Before each statement in the try body, a label (`_aether_src_N`) is emitted in `.text`. At the end of codegen, a table in `.rodata` references these labels with their file name, line, and column. The segfault handler walks this table to report the exact crash location:

```
=== AETHER HARDWARE FAULT ===
Signal: 11, Fault address: 0x0
  Source: tests/fixtures/test_segfault.ae:8:12
```

The source map table format:
```asm
section .rodata align=8
aether_source_map:
  dq _aether_src_1     ; instruction address in .text
  dq Lfile_0           ; pointer to file name string
  dd 8, 12             ; line, col
  dq _aether_src_2
  dq Lfile_1
  dd 10, 5
  ...
  dq 0, 0              ; sentinel
  dd 0, 0
```

### 10.7 Zero-Cost Error Context (`?` operator)

```aether
func read_config(): throws Config {
    let f = File.open("/etc/aether.cfg") ? "Failed to open config"
    let data = f.read_all() ? "Failed to read config"
    return parse(data) ? "Failed to parse config"
}

// The `?` operator attaches context to errors.
// In debug builds: error messages are preserved.
// In release builds: only the error discriminant remains (zero-cost).
```

---

## 11. Inline Assembly

### 11.1 Basic Inline Assembly

Full NASM syntax is a first-class citizen. The asm block text is emitted verbatim into the generated assembly. Aether function parameters are **not** automatically substituted — use SysV ABI registers directly (rdi=arg1, rsi=arg2, rdx=arg3, rcx=arg4, r8=arg5, r9=arg6).

```aether
func outb(port: u16, value: byte) {
    asm {
        mov dx, rdi
        mov al, sil
        out dx, al
    }
}
```

### 11.2 Const Values in Assembly

`const` declarations are emitted as NASM `equ` directives, making them accessible from inline asm blocks:

```aether
const COM1_DATA = 0x3F8
const LSR_THR_EMPTY = 0x20

func serial_putc(c: byte) {
    asm {
        mov dx, COM1_DATA
        mov al, dil
        out dx, al
    }
}
```

This generates:
```nasm
COM1_DATA equ 0x3F8
LSR_THR_EMPTY equ 0x20
```

### 11.3 Assembly with Output Bindings

Use `asm: (outputs) { body }` to bind assembly results to Aether variables.

```aether
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

### 11.4 Labels and Jumps

```aether
func spin_wait(cycles: u64) {
    asm {
        mov rcx, cycles
    .loop:
        dec rcx
        jnz .loop
    }
}
```

### 11.5 Top-Level asm Blocks

At file level (outside any function), `asm { }` emits raw assembly text directly into the output without any function wrapping.

```aether
# Top-level asm block — emits section directives at file level
asm {
    section .bss
cmd_names:    resq 16
cmd_handlers: resq 16
cmd_count:    resq 1
    section .data
help_text:    db "Available commands: help, ls, echo", 0
    section .text
}
```

### 11.6 Extern Hoisting

The compiler automatically hoists `extern` declarations from asm blocks to the top of the output file. NASM requires `extern` at file level, not inside function bodies.

```aether
func main(): u64 {
    asm {
        extern __bss_start
        extern __bss_end
    }
    asm {
        mov rdi, __bss_start
        mov rcx, __bss_end
        sub rcx, rdi
        xor al, al
        rep stosb
    }
}
```

### 11.7 No `ret` in asm Blocks

The compiler handles function prologue/epilogue. asm blocks should NOT contain `ret` — the compiler emits the return instruction after the asm block. If an asm block contains `ret`, the compiler detects it and suppresses its own default return.

### 11.8 Multi-Architecture Assembly

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

---

## 12. Compile-Time Execution

### 12.1 `#run` Blocks

Code inside `#run { }` executes at compile time. This enables metaprogramming without a separate macro system.

```aether
#run {
    // This runs at compile time!
    let result = compute_something()
    emit("const TABLE_SIZE = {result}")
}

const TABLE_SIZE = 256
```

### 12.2 Compile-Time Constants

```aether
const MAX_CONNECTIONS = 1000
const BUFFER_SIZE = MAX_CONNECTIONS * 64
const GREETING = "Hello, World!"
```

### 12.3 Compile-Time Reflection

```aether
#run {
    // Iterate over all types in the current module
    for type in Module.types() {
        if type.has_trait(Serializable) {
            emit("impl Serialize for {type.name} {{ ... }}")
        }
    }
}
```

### 12.4 Compile-Time Assertions

```aether
#assert(sizeof(u64) == 8)
#assert(MAX_SIZE > 0)
```

### 12.5 Conditional Compilation

```aether
#[target = "kernel"]
func kernel_only() { ... }

#[target = "boot"]
func boot_only() { ... }
```

### 12.6 Compile-Time Hardware Configuration

```aether
#run {
    if target_arch() == "x86_64" {
        emit("func flush_tlb() { asm { mov cr3, cr3 } }")
    } elif target_arch() == "arm64" {
        emit("func flush_tlb() { asm { dsb ish; tlbi vmalle1is; dsb ish; isb } }")
    }
}
```

---

## 13. Contract Programming

### 13.1 Preconditions and Postconditions

```aether
func withdraw(account: ref Account, amount: u64)
    pre(account.balance >= amount)
    post(account.balance == old(account.balance) - amount)
{
    account.balance -= amount
}
```

### 13.2 Invariants

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

### 13.3 Debug vs Release

In debug builds, contracts are checked at runtime. In release builds, they serve as optimizer hints and are eliminated.

```aether
// Debug: checks pre/post at runtime
// Release: eliminated, used for optimization
func fast_path(x: u64): u64
    pre(x > 0)
    post(result > 0)
{
    return 100 / x
}
```

---

## 14. Closures and Lambdas

### 14.1 Lambda Syntax

```aether
let add = |a: int, b: int| -> a + b
let result = add(3, 4)  // result = 7

// Multi-line lambda
let process = |items: [int]| {
    let total = 0
    for item in items {
        total += item
    }
    return total
}
```

### 14.2 Closures with Captures

```aether
func make_adder(x: int): func(int): int {
    return |y| -> x + y  // captures x by reference
}

let add5 = make_adder(5)
let result = add5(3)  // result = 8
```

### 14.3 Higher-Order Functions

```aether
func map<T, U>(items: [T], f: func(T): U): [U] {
    let result: [U; items.len]
    for i, item in items {
        result[i] = f(item)
    }
    return result
}

let numbers = [1, 2, 3, 4, 5]
let doubled = map(numbers, |x| -> x * 2)
```

---

## 15. Properties and Operator Overloading

### 15.1 Properties (Inferred from Return Type)

Properties are methods that look like fields. The compiler infers getter/setter from the return type:

- **Getter**: has a return type → `obj.prop` calls the getter
- **Setter**: has no return type → `obj.prop = value` calls the setter

```aether
class Temperature {
    celsius: f64

    // Getter — has return type
    prop fahrenheit(self): f64 {
        return self.celsius * 9.0 / 5.0 + 32.0
    }

    // Setter — no return type
    prop fahrenheit(self, value: f64) {
        self.celsius = (value - 32.0) * 5.0 / 9.0
    }
}

let t = Temperature { celsius: 100 }
print(t.fahrenheit)  // calls getter → 212.0
t.fahrenheit = 32    // calls setter → celsius = 0
```

### 15.2 Operator Overloading

```aether
struct Vector2 {
    x: f64
    y: f64
}

impl Vector2 {
    func op_add(self, other: Vector2): Vector2 {
        return Vector2 { x: self.x + other.x, y: self.y + other.y }
    }

    func op_sub(self, other: Vector2): Vector2 {
        return Vector2 { x: self.x - other.x, y: self.y - other.y }
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
```

---

## 16. Dynamic Dispatch

### 16.1 `dyn Trait` Syntax

Dynamic dispatch uses fat pointers (vtable pointer + data pointer). Use `dyn Trait` to opt in.

```aether
trait Drawable {
    func draw(self)
    func area(self): f64
}

// Static dispatch (default) — monomorphized, zero-cost
func render_static(items: [ref Drawable]) {
    for item in items {
        item.draw()
    }
}

// Dynamic dispatch — vtable lookup
func render_dynamic(items: [ref dyn Drawable]) {
    for item in items {
        item.draw()
    }
}
```

### 16.2 When to Use Dynamic Dispatch

- **Static dispatch** (default): When the concrete type is known at compile time. Zero overhead.
- **Dynamic dispatch** (`dyn Trait`): When you need heterogeneous collections or runtime polymorphism. Adds one pointer dereference per call.

---

## 17. Modules and Namespaces

### 17.1 Module Declaration

```aether
module fs

// Public/private visibility
pub func open(path: string): u64 { ... }
func internal_helper() { ... }  // private by default

pub const BLOCK_SIZE = 512
```

### 17.2 File Imports

```aether
// Import a library file relative to the current source
import "libaether.ae"

// Now use functions from the imported file
func main(): u64 {
    puts("Hello from Aether!")
    exit_bin()
    return 0
}
```

**How imports work:**
1. The parser creates `NODE_IMPORT` AST nodes for each `import "path.ae"` statement
2. After parsing, the compiler scans for `NODE_IMPORT` nodes and resolves them:
   - Builds the full path relative to the importing file's directory
   - Reads the imported file from disk
   - Parses it using `parser_create_with_arena()` (shares the main parser's arena)
   - Merges the imported declarations into the main program's declaration list
3. The imported source buffer is kept alive because `StringView` fields point into it
4. Circular imports are detected and skipped (tracked by path)

### 17.3 Module Imports

```aether
import fs
import io as stdio
```

---

## 18. Concurrency and Fibers

### 18.1 Spawn

```aether
func worker(id: u64) {
    print("Worker {id} started")
}

spawn worker(1)
spawn worker(2)
```

### 18.2 Synchronization Primitives

```aether
var lock: Mutex
lock.acquire()
// critical section
lock.release()
```

### 18.3 Channels

```aether
let ch: Chan<u64>
ch.send(42)
let val = ch.recv()
```

### 18.4 Fiber Scheduler Support

The compiler generates code compatible with the Aether OS fiber scheduler:

- Cooperative multitasking with explicit yield
- No preemption, no timer interrupts needed
- Per-fiber stack allocation
- Yield points at blocking I/O, timer waits, and explicit `yield` calls

---

## 19. Aether OS Integration

### 19.1 Freestanding Target

The compiler's primary target is **x86_64-freestanding**. No libc, no CRT, no OS assumptions.

```bash
aether build --target x86_64-freestanding --output kernel.elf
```

### 19.2 Host-Native Target

The compiler also outputs **host-native formats** so you can compile and run `.ae` programs directly on your development machine.

| Host OS | Format | Linker |
|---------|--------|--------|
| macOS | Mach-O 64 (x86_64) | `ld` (system) or direct syscall emission |
| Linux | ELF64 | `ld` (system) or direct syscall emission |
| Windows | PE32+ | `link.exe` or direct |

### 19.3 Syscall Page (0x5000)

The compiler knows the Aether syscall table and generates optimal call sequences:

```aether
sys func putc(c: byte) at(0)
sys func puts(s: string) at(1)
sys func open(path: string): int at(2)
sys func read(fd: int, buf: ref [u8]): int at(3)
sys func readdir(ino: u32, buf: ref [u8]): int at(4)
sys func getcwd(): u32 at(5)
sys func chdir(ino: u32) at(6)
sys func exit() at(7)
sys func booleval(v: u64): u64 at(8)
```

### 19.4 Module Interface (0x4000)

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

### 19.5 Binary Entry Points

```aether
@entry(0x2000000)
func main(args: [][]byte): int {
    puts("Hello from Aether binary!\n")
    return 0
}
```

### 19.6 Memory Layout Directives

```aether
@layout(start=0x7C00, max=512, file="stage1.bin")
func stage1_mbr() {
    asm { ... 512-byte MBR ... }
}
```

### 19.7 Kernel Layout Verification

```aether
@kernel_layout
func init_memory() {
    // Compiler knows the memory map and verifies no overlap
    let bitmap = reserved(0x1000, 0x1000)
    let registry = reserved(0x4000, 0x1000)
    let syscall = reserved(0x5000, 0x1000)
}
```

### 19.8 Declarative Resources

```aether
// Declare a memory pool for USB transfers
pool UsbDmaBuffer of size 64, count 32, alignment 256

// Use it — compiler generates pool alloc/free
func alloc_usb_buf(): UsbDmaBuffer {
    return UsbDmaBuffer()  // from the declared pool
}  // compiler inserts: return to pool on drop
```

### 19.9 Protocol Generation

```aether
protocol Serial {
    port base = 0x3F8
    speed = 115200

    func putc(c: byte) {
        asm { mov dx, port; mov al, c; out dx, al }
    }
}
```

### 19.10 Automatic Boot Chain Generation

```aether
// Describe the boot chain declaratively
bootchain {
    stage1: @layout(start=0x7C00, max=512)
    stage2: @layout(start=0x7E00, max=16384)
    kernel: @layout(start=0x1000000)
}

// Compiler generates:
// 1. Stage1 MBR with correct INT 13h parameters
// 2. Stage2 loader with correct sector counts
// 3. Kernel entry point at correct address
// 4. Disk image with correct layout
```

### 19.11 Automatic Interrupt Handler Generation

```aether
// Declare interrupt handlers declaratively
interrupt timer at(0x20) {
    // Compiler generates:
    // 1. Correct interrupt frame save/restore
    // 2. EOI signaling
    // 3. Stack switching if needed
    tick_count += 1
}

interrupt keyboard at(0x21) {
    let scancode = inb(0x60)
    handle_key(scancode)
}
```

### 19.12 Self-Documenting Binary Format

```aether
@metadata {
    author = "Aether Team"
    description = "Kernel main binary"
    license = "MIT"
    required_abi = "1.0"
}
```

The compiler embeds this as an ELF note section. The OS can query it without loading the binary.

### 19.13 Capability-Based Security

```aether
// Functions declare what capabilities they need
@requires(io, mem)
func write_disk(sector: u64, data: ref [u8]) {
    // Compiler verifies caller has io and mem capabilities
}

// Capabilities are tracked at compile time
func safe_path() {
    // Error: write_disk requires io capability
    // write_disk(0, data)
}
```

### 19.14 Module ABI Compliance

```aether
@module_abi(version = "1.0")
module my_driver {
    @export func mod_init(): int { ... }
    @export func mod_fini() { ... }
}
```

---

## 20. Multi-Target Assembler

### 20.1 Architecture

The multi-target assembler translates NASM syntax to target architecture assembly:

```
NASM Source → NASM Parser → AsmIR (intermediate representation)
  → x86_64 Backend (passthrough)
  → ARM64 Backend (instruction mapping)
  → RISC-V Backend (instruction mapping)
```

### 20.2 Register Translation

| NASM | ARM64 | RISC-V |
|------|-------|--------|
| `rax` | `X0` | `a0` |
| `rbx` | `X19` | `s0` |
| `rcx` | `X1` | `a1` |
| `rdx` | `X2` | `a2` |
| `rsi` | `X3` | `a3` |
| `rdi` | `X4` | `a4` |
| `rsp` | `SP` | `sp` |
| `rbp` | `X29` | `s1` |

### 20.3 Instruction Translation Examples

| NASM | ARM64 | RISC-V |
|------|-------|--------|
| `mov rax, rbx` | `MOV X0, X19` | `MV a0, s0` |
| `add rax, 5` | `ADD X0, X0, #5` | `ADDI a0, a0, 5` |
| `jmp label` | `B label` | `J label` |
| `call func` | `BL func` | `JAL ra, func` |
| `ret` | `RET` | `JALR zero, ra, 0` |
| `push rax` | `STR X0, [SP, #-16]!` | `ADDI sp, sp, -16; SD a0, 0(sp)` |
| `pop rax` | `LDR X0, [SP], #16` | `LD a0, 0(sp); ADDI sp, sp, 16` |

### 20.4 Usage

```bash
# Show ARM64 translation of NASM assembly
aether --target asm-arm64 source.ae -o output.asm

# Show RISC-V translation
aether --target asm-riscv64 source.ae -o output.asm
```

---

## 21. Universal Binaries

### 21.1 Concept

Aether supports compiling a single source file into a **universal binary** that runs natively on multiple architectures without an interpreter, JIT, or emulation layer.

### 21.2 Binary Layout

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
// Generated automatically by --target universal
func _start() {
    asm {
        // x86_64 path: CPUID check
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

---

## 22. Standard Library

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

## 23. Build System and CLI

### 23.1 `aether` CLI

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

### 23.2 Project Structure

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

### 23.3 `aether.toml` Manifest

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

## 24. Compiler Targets

| Target | Format | Use Case |
|--------|--------|----------|
| `host` | Mach-O 64 / ELF64 / PE32+ | Development and testing on dev machine |
| `x86_64-freestanding` | ELF64 | Aether OS kernel |
| `kernel` | ELF64 | Kernel binary with memory map verification |
| `module` | ELF64 `.ko` | Loadable kernel module |
| `binary` | ELF64 | Userland binary at 0x2000000 |
| `boot` | Flat binary | Boot sector (stage1/stage2) |
| `asm-x86_64` | NASM text | x86_64 assembly listing |
| `asm-arm64` | ARM64 text | ARM64 assembly listing |
| `asm-riscv64` | RISC-V text | RISC-V assembly listing |
| `universal` | Multi-arch ELF | x86_64 + ARM64 combined |
| `universal-all` | Multi-arch ELF | x86_64 + ARM64 + RISC-V combined |

---

## 25. Optimization

### 25.1 Compiler Optimizations

- **Constant folding and propagation**: Evaluate constant expressions at compile time
- **Dead code elimination (DCE)**: Remove unused functions, variables, and code paths
- **Inlining**: Inline small functions with heuristic + `@force_inline` attribute
- **Escape analysis**: Promote stack allocations to heap when references escape
- **Region inference**: Automatically use arena allocation when possible
- **Devirtualization**: Convert dynamic dispatch to static when concrete type is known
- **Loop unrolling**: Unroll small loops for performance
- **Memory operation fusion**: Combine adjacent memory operations
- **Contract elimination**: Remove pre/post checks in release builds
- **Error context elimination**: Strip error messages in release builds

### 25.2 Optimization Levels

- `-O0`: No optimization, fastest compilation, best debugging
- `-O1`: Basic optimizations (constant folding, DCE)
- `-O2`: Full optimizations (inlining, escape analysis, loop unrolling)
- `-Oz`: Optimize for size

---

## 26. Self-Hosting

### 26.1 Goal

The Aether compiler must eventually compile itself. This means:

1. The compiler's tokenizer/lexer can be written in Aether
2. The compiler's parser can be written in Aether
3. The compiler's AST/semantic analysis can be written in Aether
4. The compiler's IR generation can be written in Aether
5. The compiler's code generation can be written in Aether
6. The compiler's ELF64 writer can be written in Aether
7. Full bootstrap: Aether compiler runs on Aether OS
8. Compiler can compile itself with no C bootstrap
9. C bootstrap source archived as historical reference only

### 26.2 Requirements for Self-Hosting

- Aether must support all language features used by the compiler itself
- The compiler must be able to produce correct output when compiled by itself
- The self-hosted compiler must match the C bootstrap compiler's output bit-for-bit
- Performance of the self-hosted compiler must be acceptable for development

---

## 27. Unique Innovations (Think Outside the Box)

This section describes the features that make Aether genuinely different from other systems languages.

### 27.1 Compile-Time OS Knowledge

The compiler has baked-in knowledge of the Aether OS architecture. It knows the memory map, the syscall table layout, the module registry structure, and the boot chain requirements.

```aether
// The compiler knows this is a boot sector and enforces the 512-byte limit
@layout(start=0x7C00, max=512, file="stage1.bin")
func stage1() {
    asm {
        // ... boot code ...
    }
    // Compiler error if this exceeds 512 bytes
}

// The compiler verifies memory map regions don't overlap
@kernel_layout
func verify_layout() {
    // Compiler checks: 0x1000-0x1FFF (bitmap) doesn't overlap
    // 0x4000-0x4FFF (module registry) doesn't overlap
    // 0x5000-0x5FFF (syscall page) doesn't overlap
    // 0x6000-0xAFFF (page tables) doesn't overlap
}
```

### 27.2 Goal-Oriented I/O

Instead of describing *how* to read a file, describe *what* you want:

```aether
// The compiler generates the optimal read path:
// - Boot-time: raw ATA PIO reads
// - Userspace: AetherFS syscalls
// - In-memory FS: direct pointer access
let config = from "/etc/aether.cfg" read Config
```

### 27.3 Pattern-Based Metaprogramming

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

### 27.4 Query-Style Data Transformations

Chain operations on collections — the compiler fuses them into a single loop with no intermediate allocations:

```aether
let active_users = db.users
    .filter(|u| u.active)
    .map(|u| (u.name, u.email))
    .sort(|u| u.0)
    .collect()
```

This compiles to a single fused loop. No temporary arrays, no allocation overhead.

### 27.5 Automatic Protocol Generation

Define hardware protocols declaratively:

```aether
protocol I2C {
    scl pin = 5
    sda pin = 6
    speed = 100000

    func start() {
        asm {
            // Compiler generates optimal bit-banging code
            // based on pin assignments and speed
        }
    }

    func write(byte data) {
        // Compiler generates the I2C protocol sequence
    }
}
```

### 27.6 Compile-Time Unit Checking

```aether
// The compiler tracks physical units at compile time
@units(meters)
func distance(v: f64, t: f64): f64 {
    return v * t  // m/s * s = m ✓
}

@units(meters_per_second)
func speed(d: f64, t: f64): f64 {
    return d / t  // m / s = m/s ✓
}

// Compile error: can't add meters to seconds
// let bad = distance(10, 5) + speed(10, 5)
```

### 27.7 String Interpolation with Auto-Conversion

```aether
let name = "World"
let msg = "Hello {name}!"       // "Hello World!"

let n: u64 = 42
let s = "value: {n}"            // "value: 42" (auto-converted via __aether_itoa)

let x: u64 = 40
let y: u64 = 2
let sum = "{x + y}"             // "42" (expressions work too)
```

### 27.8 `+` Does String Concat When Either Operand Is a String

```aether
let s = "hello " + "world"      // "hello world" (concat)
let s = "value: " + 42          // "value: 42" (auto-converted)
let s = 42 + " is the answer"   // "42 is the answer" (auto-converted)
let n = 42 + 1                  // 43 (numeric addition — both are numbers)
```

### 27.9 Import Resolution

```aether
// Import a library file relative to the current source
import "libaether.ae"

// Now use functions from the imported file
func main(): u64 {
    puts("Hello from Aether!")
    exit_bin()
    return 0
}
```

### 27.10 Zero-Cost Error Context

```aether
func read_config(): throws Config {
    let f = File.open("/etc/aether.cfg") ? "Failed to open config"
    let data = f.read_all() ? "Failed to read config"
    return parse(data) ? "Failed to parse config"
}

// The `?` operator attaches context to errors.
// In debug builds: error messages are preserved.
// In release builds: only the error discriminant remains (zero-cost).
```

### 27.11 Automatic Interrupt Handler Generation

```aether
// Declare interrupt handlers declaratively
interrupt timer at(0x20) {
    // Compiler generates:
    // 1. Correct interrupt frame save/restore
    // 2. EOI signaling
    // 3. Stack switching if needed
    tick_count += 1
}

interrupt keyboard at(0x21) {
    let scancode = inb(0x60)
    handle_key(scancode)
}
```

### 27.12 Self-Documenting Binary Format

```aether
@metadata {
    author = "Aether Team"
    description = "Kernel main binary"
    license = "MIT"
    required_abi = "1.0"
}
```

The compiler embeds this as an ELF note section. The OS can query it without loading the binary.

### 27.13 Capability-Based Security

```aether
// Functions declare what capabilities they need
@requires(io, mem)
func write_disk(sector: u64, data: ref [u8]) {
    // Compiler verifies caller has io and mem capabilities
}

// Capabilities are tracked at compile time
func safe_path() {
    // Error: write_disk requires io capability
    // write_disk(0, data)
}
```

### 27.14 Automatic Boot Chain Generation

```aether
// Describe the boot chain declaratively
bootchain {
    stage1: @layout(start=0x7C00, max=512)
    stage2: @layout(start=0x7E00, max=16384)
    kernel: @layout(start=0x1000000)
}

// Compiler generates:
// 1. Stage1 MBR with correct INT 13h parameters
// 2. Stage2 loader with correct sector counts
// 3. Kernel entry point at correct address
// 4. Disk image with correct layout
```

### 27.15 Compile-Time Hardware Configuration

```aether
#run {
    if target_arch() == "x86_64" {
        emit("func flush_tlb() { asm { mov cr3, cr3 } }")
    } elif target_arch() == "arm64" {
        emit("func flush_tlb() { asm { dsb ish; tlbi vmalle1is; dsb ish; isb } }")
    }
}
```

---

## 28. OS Pipeline Mapping

Every compiler feature maps to a specific Aether OS requirement:

| OS Requirement | Compiler Feature | Phase |
|---------------|-----------------|-------|
| Boot chain (stage1/stage2) | `@layout` attribute, flat binary output | 12 |
| Long mode entry (boot.ae) | `@layout(bits=64)`, inline NASM | 12 |
| Kernel at 0x1000000 | `--target kernel`, ELF64, linker script | 11 |
| Module loading (.ko) | `module` keyword, `@export`, `.ko` ELF | 6 |
| Binary execution (0x2000000) | `@entry`, `--target binary` | 6 |
| Syscall page (0x5000) | `sys func` keyword | 6 |
| Module registry (0x4000) | Module interface | 6 |
| AetherFS syscalls | `sys func` wrappers | 6 |
| Universal binaries | `--target universal` | 10 |
| Multi-architecture | Multi-target assembler | 8 |
| Memory management | Region-based allocation, `heap`, `defer` | 3 |
| Capability model | `@requires`, `pool`, `protocol` | 6 |
| Qubit module | No special compiler support | — |
| GUI compositor | No special compiler support | — |
| Fiber scheduler | `spawn`, `Chan`, `Mutex` | 16 |
| Self-hosting | Full language feature parity | 7 |
| Compile-time metaprogramming | `#run` blocks | 5 |
| Error handling | `try`/`throw`/`catch` | 5 |
| Contract programming | `pre()`/`post()` | 5 |
| String interpolation | `{expr}` in strings | 15 |
| File imports | `import "path.ae"` | 15 |

---

## 29. Implementation Phases

### Phase 0 — Bootstrap Toolchain 🟢 COMPLETE
- Project structure, build system, tokenizer, parser, AST, semantic analysis, NASM codegen, ELF64 output, CLI

### Phase 1 — Core Language 🟢 COMPLETE
- Variables, types, control flow, functions, structs, arrays, strings, inline NASM, for loops, match, enums

### Phase 2 — Host-Native Output 🟢 COMPLETE
- Multi-target codegen, Mach-O/ELF64 host output, `aether run`, host test runner

### Phase 3 — Memory Management 🟢 COMPLETE
- `defer`, `heap`, bump allocator, `ref`/`owned`/`rc`, `region`, `T?` optionals

### Phase 4 — OOP and Type System 🟢 COMPLETE
- Struct methods, classes, auto-destructors, access modifiers, traits, generics, `if let`

### Phase 5 — Advanced Language Features 🟢 COMPLETE
- Exceptions, compile-time `#run`, contracts, closures, properties, operator overloading, monomorphization, dynamic dispatch

### Phase 6 — Aether OS Integration 🟢 COMPLETE
- `sys func`, `module`, `@export`, `@entry`, `@layout`, `@kernel_layout`, `@module_abi`, `pool`, `protocol`, stdlib, linker script, `aether.toml`

### Phase 7 — Self-Hosting 🔴 NOT STARTED
- Compiler compiles its own tokenizer, parser, AST, IR, codegen, ELF writer; full bootstrap

### Phase 8 — Multi-Target Assembler 🟢 COMPLETE
- NASM IR, x86_64/ARM64/RISC-V backends, register translation, addressing mode translation

### Phase 9 — Optimization & Polish 🟢 COMPLETE
- Constant folding, DCE, inlining, escape analysis, region inference, devirtualization, loop unrolling, `aether fmt`, `aether asm`, `aether inspect`

### Phase 10 — Universal Binary & Multi-Arch Dispatch 🟢 COMPLETE
- Fat binary format, CPU detection trampoline, dual compilation pipeline, `--target universal`

### Phase 11 — Kernel Codegen Fixes 🟢 COMPLETE
- `const`→`equ`, no Linux syscalls in freestanding, `print()` no-op on kernel, bump allocator only for host, Aether kernel

### Phase 12 — @layout Auto-Injection 🟢 COMPLETE
- `bits`/`org`/padding from attributes, redundant directives removed from asm blocks

### Phase 13 — Language Specification & Requirements 🟢 COMPLETE
- REQUIREMENTS.md, SPECIFICATION.md, STATUS.md

### Phase 14 — OS Boot & Shell Stabilization 🟢 COMPLETE
- Triple fault fix, serial I/O, shell commands, binary loading, ATA PIO, fs_read/fs_readdir

### Phase 15 — String Interpolation & Imports 🟢 COMPLETE
- `{expr}` in strings, BIN_CONCAT, `__aether_concat`, `__aether_itoa`, `+` concat, `import "path.ae"`

### Phase 16 — OS Memory & Process Management 🔴 NOT STARTED
- Virtual memory manager, process/task management, interrupt handling, syscall interface, module loading, user mode switching

### Phase 17 — Concurrency & Fibers 🔴 NOT STARTED
- `spawn`, `Chan`, `Mutex`, fiber scheduler, cooperative multitasking, yield points

### Phase 18 — Advanced OS Integration 🔴 NOT STARTED
- `bootchain` declarative generation, `interrupt` handler generation, `@metadata` self-documenting binaries, `@requires` capability tracking, `@units` compile-time checking

### Phase 19 — Goal-Oriented I/O & Query Fusion 🔴 NOT STARTED
- `from path read Type` syntax, query fusion for collection operations, pattern-based metaprogramming

### Phase 20 — Protocol Generation & Hardware Configuration 🔴 NOT STARTED
- `protocol` declarative hardware interfaces, `#run` compile-time hardware detection, automatic bit-banging code generation

---

## Appendix A: Lexical Rules

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

## Appendix C: Compiler Pipeline

```
Source (.ae)
  → Tokenizer (whitespace-aware, indent engine)
    → Parser (Pratt + recursive descent)
      → AST (50+ node types)
        → Import Resolution (read, parse, merge imported files)
          → Semantic Analysis (type checking, name resolution, borrow checking)
            → HIR → MIR (CFG with memory annotations)
              → Optimization (constant folding, DCE, inlining, escape analysis)
                → LIR (register allocation, instruction selection)
                  → Code Generation (NASM text output)
                    → Multi-Target Assembler (x86_64/ARM64/RISC-V)
                      → Binary Output (ELF64/Mach-O/PE32+/flat binary)
```

## Appendix D: Known Technical Decisions

- **Bootstrap language**: C11 freestanding (matches Aether OS kernel)
- **Output**: ELF64 flat binary for freestanding; Mach-O 64 (macOS) or native ELF64 (Linux) for host-native
- **Assembly**: NASM syntax only, integrated assembler in compiler
- **Multi-target assembler**: NASM syntax parsed into an intermediate IR, then translated to target architecture
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

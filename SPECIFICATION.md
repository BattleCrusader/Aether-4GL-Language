# Aether Language Specification

**Version**: 0.4 (Phase 10 — Universal Binary)
**Status**: Living Document

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Design Philosophy](#2-design-philosophy)
3. [Lexical Structure](#3-lexical-structure)
4. [Types](#4-types)
5. [Variables and Bindings](#5-variables-and-bindings)
6. [Functions](#6-functions)
7. [Control Flow](#7-control-flow)
8. [Memory Management](#8-memory-management)
9. [Object-Oriented Programming](#9-object-oriented-programming)
10. [Generics](#10-generics)
11. [Error Handling](#11-error-handling)
12. [Compile-Time Execution](#12-compile-time-execution)
13. [Contract Programming](#13-contract-programming)
14. [Closures and Lambdas](#14-closures-and-lambdas)
15. [Properties and Operator Overloading](#15-properties-and-operator-overloading)
16. [Dynamic Dispatch](#16-dynamic-dispatch)
17. [Inline Assembly](#17-inline-assembly)
18. [Aether OS Integration](#18-aether-os-integration)
19. [Multi-Target Assembler](#19-multi-target-assembler)
20. [Universal Binaries](#20-universal-binaries)
21. [Standard Library](#21-standard-library)
22. [Build System](#22-build-system)
23. [Compiler Targets](#23-compiler-targets)

---

## 1. Introduction

Aether is a systems programming language designed for building operating systems, kernels, bootloaders, and high-performance userland applications. It compiles directly to native code through NASM assembly — no LLVM, no interpreter, no runtime GC.

The language combines the readability of Python with the control of C and the safety of Rust, while adding unique features like compile-time execution, contract programming, and a multi-target assembler that can translate the same source to x86_64, ARM64, and RISC-V.

### 1.1 Hello, World

```aether
func main(): u64 {
    print("Hello, Aether!")
    return 0
}
```

### 1.2 Compilation

```bash
# Compile for the host system
aether hello.ae -o hello

# Compile for Aether OS kernel
aether --target kernel kernel.ae -o kernel.elf

# Compile as a universal binary (x86_64 + ARM64)
aether --target universal hello.ae -o hello.ub

# Compile and run in one step
aether run hello.ae
```

---

## 2. Design Philosophy

### 2.1 Core Principles

1. **Readability first** — Syntax should be immediately understandable. No sigils, no cryptic operators, no template metaprogramming.

2. **Zero-cost abstractions** — High-level features (classes, generics, exceptions) compile down to the same assembly you'd write by hand. You never pay for what you don't use.

3. **Automatic memory management, no GC** — Memory is managed at compile time through escape analysis, region inference, and deterministic scope-based deallocation. No garbage collector, no reference counting overhead.

4. **No interpreter** — Aether compiles directly to native code. There is no VM, no JIT, no bytecode. The compiler itself is written in C (bootstrap) and will eventually be self-hosting.

5. **OS-native from day one** — The language is designed alongside the Aether OS. Every feature exists because the OS needs it: syscall tables, kernel modules, boot sectors, flat binaries.

6. **Multi-architecture by default** — The same source code can target x86_64, ARM64, and RISC-V. The multi-target assembler translates NASM instructions between architectures automatically.

### 2.2 What Makes Aether Unique

- **Compile-time `#run` blocks** execute code during compilation, enabling metaprogramming without a separate macro system
- **Contract programming** with `pre()` and `post()` conditions checked in debug builds, eliminated in release
- **Multi-target assembler** translates NASM assembly to ARM64 and RISC-V automatically
- **Universal binaries** contain native code for multiple architectures with a CPU detection trampoline
- **Deterministic exceptions** use tagged union returns — no unwinding tables, no personality functions
- **Region-based allocation** with `region { }` blocks for O(1) arena teardown
- **`sys func`** for direct syscall page invocation in kernel space

---

## 3. Lexical Structure

### 3.1 Comments

```aether
// Line comment

/* Block comment */

/* Nested /* block */ comments */
```

### 3.2 Identifiers

Identifiers start with a letter or underscore, followed by letters, digits, or underscores:

```aether
my_variable    // valid
_private_val   // valid
camelCase      // valid
PascalCase     // valid
2bad           // invalid
```

### 3.3 Keywords

```
and         as          bool        break       catch
class       const       continue    defer       dyn
elif        else        enum        export      extern
false       for         func        get         heap
if          impl        import      in          inline
internal    let         module      mut         none
not         or          owned       pool        post
pre         private     protocol    pub         rc
ref         region      return      self        set
struct      sys         throw       trait       true
try         type        u8          u16         u32
u64         u128        while
```

### 3.4 Operators

```aether
Arithmetic:     +   -   *   /   %   **
Comparison:     ==  !=  <   >   <=  >=
Logical:        &&  ||  !
Bitwise:        &   |   ^   <<  >>
Assignment:     =   +=  -=  *=  /=  %=  &=  |=  ^=  <<=  >>=
Member:         .   ->
Scope:          ::
Arrow:          =>
Range:          ..
Pattern:        |   ..=
```

### 3.5 Literals

```aether
// Integer literals
42              // decimal
0xFF            // hexadecimal
0o77            // octal
0b1010          // binary

// Float literals
3.14
1.0e-5

// String literals
"hello, world"
"escaped: \n \t \\ \" \x41"

// Multi-line strings
"""
This is a
multi-line string
"""

// Character literals
'a'
'\n'

// Boolean literals
true
false

// None literal
none
```

### 3.6 Indentation

Aether uses significant indentation (4 spaces) for block structure, similar to Python:

```aether
func example(): u64 {
    if condition {
        do_something()
    } else {
        do_other()
    }
}
```

---

## 4. Types

### 4.1 Primitive Types

```aether
let a: u8 = 255          // unsigned 8-bit
let b: u16 = 65535       // unsigned 16-bit
let c: u32 = 4294967295  // unsigned 32-bit
let d: u64 = 18446744073709551615  // unsigned 64-bit
let e: bool = true       // boolean
```

### 4.2 Compound Types

```aether
// Arrays
let arr: [u64; 4] = [1, 2, 3, 4]
let dyn_arr: [u64]      // dynamic array (slice)

// Pointers
let ptr: *u64 = &value
let null_ptr: *u64 = none

// References (preferred over pointers)
let r: ref u64 = &value
let mut_r: mut ref u64 = &mut value

// Owned references
let o: owned String = String::new()

// Reference-counted
let rc: rc SharedData = rc_new(data)

// Optionals
let opt: u64? = some(42)
let none_val: u64? = none
```

### 4.3 Type Aliases

```aether
type Result = u64
type ErrorCode = i32
type Callback = func(u64): bool
```

### 4.4 Structs

```aether
struct Point {
    x: u64
    y: u64
}

let p = Point { x: 10, y: 20 }
let x = p.x
```

### 4.5 Enums

```aether
enum Error {
    NotFound
    PermissionDenied
    InvalidInput(string)
    IoError(u64, string)
}

let err = Error::NotFound
let custom = Error::InvalidInput("bad data")
```

### 4.6 Classes

```aether
class Counter {
    value: u64

    func new(): Counter {
        return Counter { value: 0 }
    }

    func increment() {
        self.value += 1
    }

    func get(): u64 {
        return self.value
    }
}
```

### 4.7 Traits

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
```

### 4.8 Optional Types

```aether
let x: u64? = some(42)
let y: u64? = none

// Pattern matching with if let
if let val = x {
    print("Got: {val}")
}

// Unwrap with default
let val = x or 0
```

---

## 5. Variables and Bindings

### 5.1 Let Bindings

```aether
let x: u64 = 42          // explicit type
let y = 42               // type inferred
let mut z = 10           // mutable
z = 20                   // ok

const MAX = 100          // compile-time constant
```

### 5.2 Scope

```aether
{
    let inner = 5
    // inner accessible here
}
// inner not accessible here
```

### 5.3 Shadowing

```aether
let x = 10
let x = x + 5   // shadows previous x, now 15
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

// Multiple return values via struct
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

---

## 7. Control Flow

### 7.1 If / Elif / Else

```aether
if x > 0 {
    print("positive")
} elif x < 0 {
    print("negative")
} else {
    print("zero")
}

// If as expression
let status = if x > 0 { "positive" } else { "non-positive" }
```

### 7.2 While Loops

```aether
while condition {
    do_work()
}

// Infinite loop
while true {
    process()
}
```

### 7.3 For Loops

```aether
// Range loop
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

### 7.4 Break and Continue

```aether
for i in 0..100 {
    if i == 50 { break }
    if i % 2 == 0 { continue }
    print(i)  // odd numbers 1..49
}
```

### 7.5 Match Statements

```aether
match value {
    0 => print("zero")
    1..10 => print("small")
    10..100 => print("medium")
    _ => print("large")
}

// Match on enums
match error {
    Error::NotFound => handle_not_found()
    Error::PermissionDenied => handle_permission()
    Error::InvalidInput(msg) => print("Invalid: {msg}")
    _ => print("Unknown error")
}
```

### 7.6 If Let

```aether
if let val = optional_value {
    print("Got: {val}")
} else {
    print("No value")
}
```

---

## 8. Memory Management

Aether uses compile-time memory management with no garbage collector. Memory is managed through a combination of escape analysis, region inference, and deterministic scope-based deallocation.

### 8.1 Stack Allocation (Default)

All local variables are stack-allocated by default:

```aether
func example() {
    let x: u64 = 42       // on stack
    let p = Point { ... } // on stack
    // automatically freed when function returns
}
```

### 8.2 Heap Allocation

```aether
func example() {
    let data = heap [u64; 1000]  // heap-allocated array
    // use data...
    // freed when data goes out of scope
}
```

### 8.3 Defer

`defer` schedules code to run at scope exit, in LIFO order:

```aether
func process_file(path: string) {
    let file = open(path)
    defer { close(file) }

    let buf = heap [u8; 4096]
    defer { free(buf) }

    // use file and buf...
    // defers run in reverse order when function returns
}
```

### 8.4 Regions

Regions provide O(1) arena allocation and teardown:

```aether
region("request") {
    let request_data = heap [u8; 1024]
    let response_data = heap [u8; 4096]

    // process request...

    // all region allocations freed at once when block exits
}
```

### 8.5 Reference Types

```aether
// Immutable reference
let r: ref u64 = &value

// Mutable reference
let r: mut ref u64 = &mut value

// Owned reference (moved, not copied)
let o: owned String = String::new()

// Reference-counted
let shared: rc Data = rc_new(data)
let clone = shared  // increments ref count
// decremented when clone goes out of scope
```

### 8.6 Escape Analysis

The compiler determines at compile time whether a value can be stack-allocated or needs the heap:

```aether
func create_point(): ref Point {
    let p = Point { x: 10, y: 20 }
    return &p  // compiler promotes p to heap if it escapes
}
```

### 8.7 Class Destruction

Classes are automatically destroyed when they go out of scope. The compiler inserts destructor calls:

```aether
class File {
    handle: u64

    func new(path: string): File {
        return File { handle: open(path) }
    }

    func drop(self) {
        close(self.handle)
    }
}

func read_config() {
    let f = File::new("/etc/config")
    // use f...
    // f.drop() called automatically at scope exit
}
```

---

## 9. Object-Oriented Programming

### 9.1 Struct Methods

Methods defined inside a struct or class body automatically receive `self` as the first parameter. You never write `self` in the parameter list — it's always implied.

```aether
struct Point {
    x: u64
    y: u64

    func distance(other: Point): f64 {
        let dx = self.x - other.x
        let dy = self.y - other.y
        return sqrt(dx*dx + dy*dy)
    }
}

let p1 = Point { x: 0, y: 0 }
let p2 = Point { x: 3, y: 5 }
let d = p1.distance(p2)  // 5.0
```

The compiler injects `self: ref StructName` as the first parameter automatically. Inside the method body, `self` refers to the instance the method was called on.

### 9.2 Classes

Classes are like structs but with automatic destructor calls:

```aether
class Buffer {
    data: *u8
    size: u64

    func new(capacity: u64): Buffer {
        return Buffer {
            data: heap_alloc(capacity),
            size: capacity
        }
    }

    func drop() {
        heap_free(self.data)
    }

    func write(data: *u8, len: u64) {
        memcpy(self.data, data, len)
    }
}
```

### 9.3 Access Modifiers

```aether
pub func public_api() { ... }       // accessible everywhere
private func internal_impl() { ... } // accessible only in current module
internal func module_only() { ... } // accessible within the same module

class Encapsulated {
    pub value: u64       // public field
    private secret: u64  // private field
}
```

### 9.4 Inheritance

```aether
class Animal {
    name: string

    func speak(self) {
        print("...")
    }
}

class Dog : Animal {
    func speak(self) {
        print("Woof!")
    }
}
```

### 9.5 Traits and Impl

```aether
trait Serializable {
    func serialize(): [u8]
    func deserialize(data: [u8]): Self
}

impl Serializable for Config {
    func serialize(): [u8] {
        // serialize fields...
    }

    func deserialize(data: [u8]): Config {
        // deserialize and return...
    }
}
```

---

## 10. Generics

### 10.1 Generic Functions

```aether
func identity<T>(value: T): T {
    return value
}

func swap<T>(a: mut ref T, b: mut ref T) {
    let tmp = a
    a = b
    b = tmp
}

let x = identity<u64>(42)
let y = identity<string>("hello")
```

### 10.2 Generic Structs

```aether
struct Pair<T, U> {
    first: T
    second: U
}

let p = Pair<u64, string> { first: 1, second: "one" }
```

### 10.3 Generic Classes

```aether
class Stack<T> {
    data: [T]
    count: u64

    func push(self, value: T) {
        self.data[self.count] = value
        self.count += 1
    }

    func pop(self): T? {
        if self.count == 0 { return none }
        self.count -= 1
        return some(self.data[self.count])
    }
}
```

### 10.4 Generic Constraints

```aether
trait Comparable {
    func less(self, other: Self): bool
}

func max<T: Comparable>(a: T, b: T): T {
    if a.less(b) { return b }
    return a
}
```

### 10.5 Monomorphization

Generics are monomorphized at compile time — each concrete type gets its own specialized code. This means zero runtime overhead compared to hand-written code.

```aether
// Both compile to separate, optimized functions
let a = max<u64>(10, 20)
let b = max<string>("apple", "banana")
```

---

## 11. Error Handling

Aether uses deterministic exceptions — errors are returned as tagged unions through the standard register convention, with no unwinding tables or personality functions.

### 11.1 Try / Catch

```aether
try {
    let result = risky_operation()
    print("Success: {result}")
} catch IoError(code, msg) {
    print("IO error {code}: {msg}")
} catch {
    print("Unknown error")
}
```

### 11.2 Throw

```aether
func divide(a: u64, b: u64): u64 {
    if b == 0 {
        throw Error::DivisionByZero
    }
    return a / b
}
```

### 11.3 Error Types

```aether
enum MathError {
    DivisionByZero
    Overflow
    Underflow
}

func safe_sqrt(value: f64): f64 {
    if value < 0 {
        throw MathError::Underflow
    }
    return sqrt(value)
}
```

### 11.4 Zero-Cost Happy Path

When no exception is thrown, the error checking path is never executed. The happy path compiles to straight-line code:

```aether
// Happy path: just a function call and return
// No branch to error handling unless an error actually occurs
let result = try { divide(10, 2) }
```

---

## 12. Compile-Time Execution

### 12.1 #run Blocks

`#run` blocks execute during compilation, enabling metaprogramming without a separate macro system:

```aether
const TABLE_SIZE = #run {
    // This code runs at compile time
    let size = 256
    print("Generating table with {size} entries")
    size
}

// Generate lookup tables at compile time
const SINE_TABLE = #run {
    let table: [f64; 360]
    for i in 0..360 {
        table[i] = sin(i * 3.14159 / 180.0)
    }
    table
}
```

### 12.2 Compile-Time Constants

```aether
const MAX_CONNECTIONS = 1000
const BUFFER_SIZE = MAX_CONNECTIONS * 64
const VERSION = "1.0.0"
```

### 12.3 Compile-Time Evaluation

Arithmetic on constants is evaluated at compile time:

```aether
const BASE = 100
const MULTIPLIER = 2
const RESULT = BASE * MULTIPLIER  // evaluated at compile time
```

---

## 13. Contract Programming

### 13.1 Preconditions

```aether
func divide(a: u64, b: u64): u64
    pre(b != 0, "Cannot divide by zero")
{
    return a / b
}
```

### 13.2 Postconditions

```aether
func factorial(n: u64): u64
    pre(n <= 20, "n too large")
    post(result > 0, "Result must be positive")
{
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
```

### 13.3 Debug vs Release

Contracts are checked in debug builds and eliminated in release builds:

```bash
# Debug build — contracts checked
aether build --debug

# Release build — contracts eliminated
aether build --release
```

---

## 14. Closures and Lambdas

### 14.1 Lambda Syntax

```aether
// Simple lambda
let double = |x: u64| x * 2

// Lambda with block body
let process = |x: u64, y: u64| {
    let sum = x + y
    sum * 2
}

// Calling a lambda
let result = double(21)  // 42
```

### 14.2 Closures

```aether
func make_adder(x: u64): func(u64): u64 {
    return |y| x + y  // captures x
}

let add5 = make_adder(5)
let result = add5(10)  // 15
```

### 14.3 Higher-Order Functions

```aether
func map<T, U>(arr: [T], f: func(T): U): [U] {
    let result: [U; arr.len]
    for i in 0..arr.len {
        result[i] = f(arr[i])
    }
    return result
}

let numbers = [1, 2, 3, 4, 5]
let doubled = map(numbers, |x| x * 2)
```

---

## 15. Properties and Operator Overloading

### 15.1 Properties

Properties are methods that look like field access. The compiler infers getter/setter from the signature:

- **Getter**: a method with a return type and no extra parameters beyond `self`
- **Setter**: a method with no return type and one value parameter

```aether
class Temperature {
    celsius: f64

    // Getter: has return type, no extra params
    func fahrenheit(self): f64 {
        return self.celsius * 9.0 / 5.0 + 32.0
    }

    // Setter: no return type, takes a value param
    func fahrenheit(self, value: f64) {
        self.celsius = (value - 32.0) * 5.0 / 9.0
    }
}

let t = Temperature { celsius: 100 }
print(t.fahrenheit)  // 212.0 — calls getter
t.fahrenheit = 32    // calls setter
print(t.celsius)      // 0.0
```

No `get`/`set` keywords needed — the return type tells the compiler everything.

### 15.2 Operator Overloading

```aether
struct Vector3 {
    x: f64
    y: f64
    z: f64
}

func op_add(self: Vector3, other: Vector3): Vector3 {
    return Vector3 {
        x: self.x + other.x,
        y: self.y + other.y,
        z: self.z + other.z
    }
}

func op_mul(self: Vector3, scalar: f64): Vector3 {
    return Vector3 {
        x: self.x * scalar,
        y: self.y * scalar,
        z: self.z * scalar
    }
}

let v1 = Vector3 { x: 1, y: 2, z: 3 }
let v2 = Vector3 { x: 4, y: 5, z: 6 }
let v3 = v1 + v2  // calls op_add
let v4 = v1 * 2.0  // calls op_mul
```

Overloadable operators:

| Operator | Method Name |
|----------|-------------|
| `+` | `op_add` |
| `-` | `op_sub` |
| `*` | `op_mul` |
| `/` | `op_div` |
| `%` | `op_mod` |
| `==` | `op_eq` |
| `!=` | `op_ne` |
| `<` | `op_lt` |
| `>` | `op_gt` |
| `[]` | `op_index` |

---

## 16. Dynamic Dispatch

### 16.1 Dyn Trait

```aether
trait Drawable {
    func draw(self)
}

func render(shapes: [dyn Drawable]) {
    for shape in shapes {
        shape.draw()  // dynamic dispatch via vtable
    }
}
```

### 16.2 Vtable Layout

The compiler generates a vtable for each trait implementation. The vtable is a struct of function pointers, and `dyn Trait` is a fat pointer (data pointer + vtable pointer).

---

## 17. Inline Assembly

### 17.1 Basic Inline Assembly

```aether
func read_cr0(): u64 {
    asm {
        mov rax, cr0
    }
}

func write_cr0(value: u64) {
    asm {
        mov cr0, [rbp + 16]
    }
}
```

### 17.2 Multi-Architecture Assembly

```aether
func halt() {
    asm {
        // x86_64
        hlt
    }
    asm {
        // ARM64
        wfi
    }
    asm {
        // RISC-V
        wfi
    }
}
```

### 17.3 Assembly with Output Bindings

```aether
func cpuid(): (u64, u64, u64, u64) {
    asm -> (eax, ebx, ecx, edx) {
        mov rax, 0
        cpuid
    }
}
```

---

## 18. Aether OS Integration

### 18.1 Syscall Functions

```aether
// Declare a syscall at position 0 in the syscall table
sys func write(fd: u64, buf: *u8, count: u64): u64 at(0)

// Declare a syscall at position 1
sys func read(fd: u64, buf: *u8, count: u64): u64 at(1)

// Use syscalls
func print_hello() {
    let msg = "Hello from Aether OS!\n"
    write(1, &msg, 20)
}
```

### 18.2 Kernel Modules

```aether
module my_driver

@export func init(): u64 {
    // Called when module is loaded
    print("Driver initialized")
    return 0
}

@export func cleanup() {
    // Called when module is unloaded
    print("Driver cleaned up")
}
```

### 18.3 Entry Points

```aether
// Kernel entry at 0x100000
@entry(0x100000)
func kernel_main() {
    // Kernel initialization
}

// Userland binary entry
@entry(0x200000)
func main(): u64 {
    return 0
}
```

### 18.4 Layout Directives

```aether
// Boot sector layout: starts at 0x7C00, max 512 bytes
@layout(start = 0x7C00, max = 512, file = "boot.bin")
func boot_main() {
    // Boot code here
    // Compiler ensures binary fits in 512 bytes
    // and places 0xAA55 signature at offset 510
}
```

### 18.5 Module ABI

```aether
@module_abi(version = 1)
module network_driver

// ABI version checked by module loader
// Incompatible versions rejected at load time
```

### 18.6 Declarative Resources

```aether
// Memory pool declaration
pool DMA_BUFFER {
    size: 4096
    alignment: 64
    count: 16
}

// Protocol declaration
protocol NetworkDriver {
    func send(packet: *u8, len: u64): u64
    func recv(buf: *u8, max_len: u64): u64
}
```

---

## 19. Multi-Target Assembler

The multi-target assembler translates NASM syntax assembly between architectures. It parses NASM into an intermediate representation (IR), then emits through pluggable backends.

### 19.1 Architecture Targets

```bash
# Emit x86_64 NASM (passthrough)
aether --target asm-x86_64 source.ae -o output.asm

# Translate to ARM64
aether --target asm-arm64 source.ae -o output_arm64.asm

# Translate to RISC-V
aether --target asm-riscv64 source.ae -o output_riscv.asm
```

### 19.2 Instruction Translation

The assembler maps instructions between architectures:

| x86_64 | ARM64 | RISC-V |
|--------|-------|--------|
| `mov rax, rbx` | `mov x0, x19` | `mv a0, s1` |
| `push rax` | `stp x0, [sp, #-16]!` | `addi sp, sp, -8; sd a0, 0(sp)` |
| `pop rax` | `ldp x0, [sp], #16` | `ld a0, 0(sp); addi sp, sp, 8` |
| `call func` | `bl func` | `jal ra, func` |
| `jz label` | `b.eq label` | `beqz a0, label` |
| `int 0x80` | `svc #0` | `ecall` |

### 19.3 Register Mapping

| x86_64 | ARM64 | RISC-V | Purpose |
|--------|-------|--------|---------|
| `rax` | `x0` | `a0` | Return value / arg1 |
| `rbx` | `x19` | `s1` | Callee-saved |
| `rcx` | `x1` | `a1` | Arg2 |
| `rdx` | `x2` | `a2` | Arg3 / error flag |
| `rsi` | `x3` | `a3` | Arg4 |
| `rdi` | `x4` | `a4` | Arg5 |
| `r8` | `x5` | `a5` | Arg6 |
| `rsp` | `sp` | `sp` | Stack pointer |
| `rbp` | `x29` | `s0` | Frame pointer |

---

## 20. Universal Binaries

A universal binary contains native code for multiple architectures in a single ELF file. A tiny CPU detection trampoline at the entry point detects the running CPU and dispatches to the correct architecture's code.

### 20.1 Building Universal Binaries

```bash
# Build for x86_64 + ARM64
aether --target universal source.ae -o output.ub

# Build for all architectures
aether --target universal-all source.ae -o output.ub

# Build with explicit arch list
aether --target universal-x86_64-arm64-riscv64 source.ae -o output.ub
```

### 20.2 Binary Layout

```
ELF Header (entry → CPU detection trampoline)
├── .text.trampoline  — CPUID detection + dispatch
├── .text.x86_64      — x86_64 native code
├── .text.arm64       — ARM64 native code
├── .text.riscv64     — RISC-V native code
├── .rodata           — Shared read-only data
├── .data             — Shared writable data
└── .bss              — Shared zero-initialized data
```

### 20.3 CPU Detection

The trampoline uses the EFLAGS.ID bit (bit 21) to detect x86_64 CPUs:

```aether
// Pseudocode for the trampoline
_start:
    if can_toggle_eflags_id_bit() {
        jmp _aether_entry  // x86_64
    } else {
        jmp _start_arm64   // ARM64
    }
```

### 20.4 Architecture-Specific Code

```aether
// Function only included in x86_64 slice
@arch(x86_64)
func setup_gdt() {
    asm {
        lgdt [gdt_ptr]
    }
}

// Function only included in ARM64 slice
@arch(arm64)
func setup_page_tables() {
    asm {
        msr ttbr0_el1, x0
    }
}

// Function included in all slices (default)
@arch_shared
func shared_init() {
    print("Common initialization")
}
```

---

## 21. Standard Library

### 21.1 std.io

```aether
import std.io

print("Hello")           // print string
println("Hello")         // print with newline
let s = format("x={x}", x=42)  // format string
```

### 21.2 std.mem

```aether
import std.mem

let pool = Pool::new(1024, 64)  // object pool
let arena = Arena::new(4096)    // arena allocator
memcpy(dest, src, 100)          // memory copy
memzero(buf, 256)               // zero memory
```

### 21.3 std.str

```aether
import std.str

let s = String::new("hello")
let t = s.concat(" world")
let parts = s.split(",")
```

### 21.4 std.math

```aether
import std.math

let x = abs(-5)
let y = min(10, 20)
let z = max(10, 20)
```

### 21.5 std.collections

```aether
import std.collections

let arr = Array::new<u64>()
arr.push(10)
arr.push(20)
let v = arr.pop()

let map = HashMap::new<string, u64>()
map.insert("key", 42)
let v = map.get("key")
```

### 21.6 std.serial

```aether
import std.serial

serial_write(COM1, "Hello from kernel!\n")
let c = serial_read(COM1)
```

### 21.7 std.test

```aether
import std.test

func test_addition() {
    assert(add(2, 2) == 4, "2+2 should equal 4")
    assert(add(-1, 1) == 0, "-1+1 should equal 0")
}
```

---

## 22. Build System

### 22.1 aether.toml

```toml
[project]
name = "my_os"
version = "0.1.0"

[build]
target = "kernel"
debug = true

[build.targets]
x86_64 = true
arm64 = true
universal = true

[toolchain]
x86_64_assembler = "nasm"
arm64_assembler = "aarch64-linux-gnu-as"
riscv64_assembler = "riscv64-linux-gnu-as"
linker = "x86_64-elf-ld"
```

### 22.2 Build Commands

```bash
# Build project
aether build

# Build with specific target
aether build --target kernel

# Build universal binary
aether build --target universal

# Run tests
aether test

# Format source
aether fmt

# Generate documentation
aether doc

# Show generated assembly
aether asm source.ae

# Inspect ELF binary
aether inspect output.elf
```

---

## 23. Compiler Targets

| Target | Description | Output |
|--------|-------------|--------|
| `host` | Auto-detect host format | Mach-O 64 or ELF64 |
| `macho64` | macOS native | Mach-O 64 |
| `elf64-host` | Linux native | ELF64 |
| `x86_64-freestanding` | Aether OS ELF64 | ELF64 |
| `kernel` | Kernel at 0x1000000 | ELF64 |
| `module` | Aether OS .ko module | ELF64 |
| `binary` | Userland at 0x2000000 | ELF64 |
| `boot` | Flat binary boot sector | 512-byte binary |
| `asm-x86_64` | x86_64 NASM listing | .asm file |
| `asm-arm64` | ARM64 assembly listing | .asm file |
| `asm-riscv64` | RISC-V assembly listing | .asm file |
| `universal` | x86_64 + ARM64 | Multi-arch ELF64 |
| `universal-all` | All architectures | Multi-arch ELF64 |

---

## Appendix A: Compiler Architecture

```
Source (.ae)
    │
    ▼
┌─────────────┐
│  Tokenizer   │  → Tokens
└─────────────┘
    │
    ▼
┌─────────────┐
│   Parser     │  → AST (50+ node types)
└─────────────┘
    │
    ▼
┌─────────────┐
│  Semantic    │  → Type checking, name resolution
│  Analysis    │  → Escape analysis, region inference
└─────────────┘
    │
    ▼
┌─────────────┐
│  Codegen     │  → NASM assembly text
└─────────────┘
    │
    ▼
┌─────────────┐     ┌──────────────┐
│  Multi-Arch  │────▶│  x86_64      │
│  Assembler   │────▶│  ARM64       │
│  (AsmIR)     │────▶│  RISC-V      │
└─────────────┘     └──────────────┘
    │
    ▼
┌─────────────┐
│  Assemble    │  → nasm -f elf64
│  + Link      │  → x86_64-elf-ld
└─────────────┘
    │
    ▼
┌─────────────┐
│  Universal   │  → Multi-arch ELF
│  Builder     │  → CPU detection trampoline
└─────────────┘
    │
    ▼
  Output (.elf / .o / .asm / .bin)
```

## Appendix B: Error Convention

Aether uses a deterministic error convention based on register state:

- **`rdx = 0`**: Success — `rax` holds the return value
- **`rdx = 1`**: Error — `rax` holds the error discriminant

This convention means:
- No unwinding tables needed
- No personality functions
- Zero-cost happy path (no branches when no error occurs)
- Deterministic, predictable error handling

## Appendix C: Memory Model

```
┌─────────────────────────────────────┐
│              Stack                   │  ← Local variables, function frames
│  (Automatic, O(1) alloc/free)       │
├─────────────────────────────────────┤
│              Heap                    │  ← Explicit `heap` allocations
│  (Bump allocator, 64KB arenas)      │
├─────────────────────────────────────┤
│              Regions                 │  ← `region { }` blocks
│  (Stack-arena, O(1) teardown)       │
├─────────────────────────────────────┤
│              .bss                    │  ← Zero-initialized globals
│  (Heap start/cur/end, region state) │
└─────────────────────────────────────┘
```

## Appendix D: Phase Roadmap

| Phase | Status | Description |
|-------|--------|-------------|
| 0 | ✅ | Bootstrap toolchain (C compiler, tokenizer, parser, codegen) |
| 1 | ✅ | Core language (variables, functions, control flow, structs, enums) |
| 2 | ✅ | Host-native output (Mach-O, ELF64, `aether run`) |
| 3 | ✅ | Memory management (defer, heap, regions, optionals) |
| 4 | ✅ | OOP (classes, traits, generics, access modifiers) |
| 5 | ✅ | Advanced features (exceptions, compile-time, contracts, closures) |
| 6 | ✅ | Aether OS integration (sys func, modules, attributes, stdlib) |
| 7 | 🔴 | Self-hosting (compiler compiles itself) |
| 8 | ✅ | Multi-target assembler (NASM → ARM64/RISC-V) |
| 9 | 🔴 | Optimization & polish (fmt, doc, LSP, benchmarks) |
| 10 | ✅ | Universal binary & multi-arch dispatch |

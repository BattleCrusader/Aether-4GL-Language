# Aether Language Specification

**Version**: 1.0 (Comprehensive)
**Status**: Living Document — Last updated: 2026-06-24 — Comprehensive audit complete. See annotations for implementation status.

> **AUDIT NOTE (2026-06-24)**: This specification has been audited against the compiler source code. Discrepancies between spec and implementation are annotated with `⚠️` (partially implemented), `❌` (not implemented), or `🔧` (spec mismatch — implementation differs from spec). See individual sections for details.

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
24. [Think Outside the Box: Unique Aether Innovations](#24-think-outside-the-box-unique-aether-innovations)
25. [Concurrency and Fibers](#25-concurrency-and-fibers)
26. [Advanced OS Integration](#26-advanced-os-integration)
27. [Goal-Oriented I/O and Query Fusion](#27-goal-oriented-io-and-query-fusion)
28. [Protocol Generation and Hardware Configuration](#28-protocol-generation-and-hardware-configuration)
29. [Module System and Imports](#29-module-system-and-imports)

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
- **`self` is implicit** in methods — never write it in the parameter list
- **Properties inferred from return type** — getter has return type, setter has no return type

---

## 3. Lexical Structure

### 3.1 Comments

> **🔧 SPEC MISMATCH**: The spec documents `//` and `/* */` comment syntax. The actual implementation uses `#` for line comments and `#{ }#` for nestable block comments. This section has been corrected to match the implementation.

```aether
# Line comment

#{ Block comment }#

#{ Nested #{ block }# comments }#
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
false       for         func        heap        if
if          impl        import      in          inline
internal    let         module      mut         none
not         or          owned       pool        post
pre         private     protocol    pub         rc
ref         region      return      self        struct
sys         throw       trait       true        try
type        u8          u16         u32         u64
u128        while
```

### 3.4 Operators

```aether
Arithmetic:     +   -   *   /   %
Comparison:     ==  !=  <   >   <=  >=
Logical:        &&  ||  !   and or not
Bitwise:        &   |   ^   <<  >>
Assignment:     =   +=  -=  *=  /=  %=  &=  |=  ^=  <<=  >>=
Member:         .   ->
Scope:          ::
Arrow:          ->
Range:          ..  ..=
Pattern:        |   ..=
```

> **🔧 SPEC MISMATCH**: The spec previously listed `**` (power operator) and `=>` (arrow). The `**` operator is not implemented in the tokenizer. The arrow syntax is `->` (TOKEN_ARROW), not `=>`. The `and`, `or`, `not` keyword operators are also supported alongside `&&`, `||`, `!`.

**String Concatenation with `+`**: The `+` operator is overloaded at the language level. When either operand is a string, `+` performs string concatenation instead of numeric addition. This is detected at codegen time by the `is_string_expr()` helper.

```aether
let s = "hello " + "world"      // "hello world" (concat)
let s = "value: " + 42          // "value: 42" (auto-converted via __aether_itoa)
let s = 42 + " is the answer"   // "42 is the answer" (auto-converted)
let n = 42 + 1                  // 43 (numeric addition — both operands are numbers)
```

The `is_string_expr()` helper detects:
- String literals (`NODE_LITERAL_STRING`)
- `BIN_CONCAT` chains (interpolation results)
- Typed identifiers with `PRIM_STRING` type
- Untyped identifiers whose initializer is a string expression

When string concat is detected, the codegen:
1. Checks if either operand is numeric and auto-converts via `__aether_itoa`
2. Pushes both operands and calls `__aether_concat`
3. Returns the new string pointer in `rax`

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

// String interpolation — expressions in {} are evaluated and converted to strings
let name = "World"
let msg = "Hello {name}!"       // "Hello World!"
let n: u64 = 42
let s = "count = {n}"           // "count = 42" (auto-converted via __aether_itoa)
let sum = "total: {x + y}"      // expressions work too

// Multi-line strings
// ❌ NOT YET IMPLEMENTED — Triple-quoted string syntax is not supported.
// Use string concatenation or interpolation instead.
// """ ... """ syntax is reserved for future implementation.

// Character literals
'a'
'\n'

// Boolean literals
true
false

// None literal
none
```

### 3.6 String Interpolation

String interpolation is a first-class language feature, not a library function. When the parser encounters `{expr}` inside a string literal, it:

1. Splits the string at the `{` boundary
2. Parses the expression between `{` and `}` as a sub-expression
3. Builds a left-associative chain of `BIN_CONCAT` operations
4. At codegen time, each `BIN_CONCAT` emits a `call __aether_concat`

**Numeric-to-string auto-conversion**: If an interpolated expression evaluates to a numeric type (u8-u64, i8-i64, bool, byte), the codegen automatically inserts a `call __aether_itoa` before the concat. This is detected by the `is_numeric_expr()` helper which checks:
- Literal types (`NODE_LITERAL_INT`, `NODE_LITERAL_FLOAT`, `NODE_LITERAL_BOOL`, `NODE_LITERAL_CHAR`)
- Binary ops that produce numeric results (add, sub, mul, div, bitwise, shifts)
- Unary ops that produce numeric results (neg, bit_not, inc, dec)
- Typed identifiers (resolved to declarations with primitive numeric types)

```aether
let name = "World"
let msg = "Hello {name}!"       # BIN_CONCAT("Hello ", BIN_CONCAT(name, "!"))

let n: u64 = 42
let s = "value: {n}"            # BIN_CONCAT("value: ", __aether_itoa(n))

let x: u64 = 40
let y: u64 = 2
let sum = "{x + y}"             # BIN_CONCAT("", __aether_itoa(x + y))
```

**Runtime**: The `__aether_concat(left: ptr, right: ptr) -> rax: ptr` function:
1. Computes strlen of both strings
2. Allocates a buffer via `__aether_alloc` (bump allocator)
3. Copies both strings into the buffer
4. Returns the new string pointer

The `__aether_itoa(rdi: u64) -> rax: ptr` function:
1. Allocates 21 bytes (max 20 digits + null)
2. Handles zero case (returns "0")
3. Writes digits from end backwards using repeated div by 10
4. Shifts the result to the start of the buffer
5. Returns the string pointer

**Note**: `__aether_itoa` clobbers `rcx` (uses `div rcx`), so callers must save `rcx` before calling it.

### 3.7 Indentation

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

> **❌ NOT YET IMPLEMENTED** — The `type` keyword is tokenized but not handled in the parser. `NODE_TYPE_ALIAS` exists in the AST but no parser code creates these nodes. Type aliases are reserved for future implementation.

```aether
// ❌ NOT YET IMPLEMENTED
// type Result = u64
// type ErrorCode = i32
// type Callback = func(u64): bool
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
    func draw() {
        print("Drawing circle")
    }

    func area(): f64 {
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
// ❌ NOT YET IMPLEMENTED — The `or` operator for optional unwrap is not supported.
// let val = x or 0
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

> **Implementation status**: ⚠️ Partially implemented — basic functions work; default parameters and variadic functions are not yet implemented.

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

> **❌ NOT YET IMPLEMENTED** — Parser does not support default parameter values.

```aether
func create_window(title: string, width: u64 = 800, height: u64 = 600) {
    // ...
}

create_window("Hello")                    // uses defaults
create_window("Wide", 1024, 768)          // explicit
```

### 6.4 Variadic Functions

> **❌ NOT YET IMPLEMENTED** — Parser does not support variadic parameter syntax.

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

> **Implementation status**: ✅ Fully implemented — if/elif/else, while, for, break/continue, match, defer all work.

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
// ❌ NOT YET IMPLEMENTED — Index+value iteration syntax is not supported.
// for i, val in arr {
//     print("arr[{i}] = {val}")
// }
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
    case 0 -> print("zero")
    // ❌ Range patterns (case 1..9) not yet implemented
    // ❌ Guard patterns (case > 100) not yet implemented
    // ❌ Enum destructuring (case string(s)) not yet implemented
    case _ -> print("default")
}

// Match as expression
let description = match value {
    case 0 -> "zero"
    // ❌ Range patterns not yet implemented
    case _ -> "large"
}
```

> **🔧 SPEC MISMATCH**: The spec previously used `=>` for match arms. The actual syntax uses `->` (TOKEN_ARROW). Range patterns (`case 1..9`), guard patterns (`case > 100`), and enum destructuring (`case string(s)`) are not yet implemented — only literal int and wildcard (`_`) patterns work.

### 7.6 Defer

```aether
func process_file(path: string) {
    let f = File::open(path)
    defer { f.close() }
    // f.close() is called when this scope exits,
    // regardless of how it exits (return, error, break)
    f.write(data)
}
```

---

## 8. Memory Management

> **Implementation status**: ⚠️ Partially implemented — stack allocation, explicit heap, region blocks, and destructor insertion work; escape analysis and ownership/borrowing are not yet implemented.

This is the heart of the language and what makes it a true 4GL — **you describe allocation semantics; the compiler generates the exact free/teardown code.**

### 8.1 Stack Allocation (Default)

All local variables are stack-allocated by default. The compiler tracks lifetimes and generates destruction at the end of scope.

```aether
func process() {
    let p = Point(3, 4)   // stack allocated
    let items = [1, 2, 3] // fixed-size array, stack
    // compiler inserts destructor calls at scope exit
}
```

### 8.2 Escape Analysis

> **❌ NOT YET IMPLEMENTED** — No escape analysis pass exists. All variables remain stack-allocated; no automatic heap promotion.

When a reference to a stack variable could outlive its scope, the compiler **automatically promotes** it to heap allocation. No programmer annotation needed for simple cases.

```aether
func make_point(x: int, y: int): ref Point {
    let p = Point(x, y)
    // compiler detects: p's reference escapes this frame
    // auto-promotes p to heap
    return p
}
```

### 8.3 Explicit Heap (`heap` keyword)

```aether
let big = heap Buffer(1024 * 1024)
let shared = heap rc SharedState()
```

### 8.4 Ownership and Borrowing

> **❌ NOT YET IMPLEMENTED** — `owned`, `ref`, and `rc` type annotations are parsed but no borrow checker or ownership tracking exists. All types are effectively treated as plain values.

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

### 8.5 Region-Based Allocation

For kernel modules and performance-critical code, the language supports **region-based allocation** that maps directly to the Aether OS capability model.

```aether
region("kernel") {
    let p = Page()   // allocated from kernel region
    let buf = Buffer(256)
}  // entire region freed at once — O(1) teardown
```

### 8.6 No Null Pointers

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

### 8.7 Pointers (Opt-In, Unsafe)

> **⚠️ PARSED BUT NO-OP CODEGEN** — `unsafe` blocks are parsed into `NODE_UNSAFE` AST nodes. The codegen emits the body without any special handling — no memory protection changes, no bounds-check suppression. Full unsafe semantics are reserved for future implementation.

```aether
func read_mmio(addr: ptr u64): u64 {
    unsafe {
        return *addr
    }
}
```

### 8.8 Automatic Destructor Insertion

The compiler tracks every class instance and inserts destructor calls at every scope exit point — normal return, early return, exception propagation, loop break/continue.

```aether
func read_config(): throws string {
    let f = File("/etc/aether.cfg")  // constructor called
    let content = f.read_all()       // use the file
    // compiler inserts: f.drop() — even if f.read_all() threw
    return content
}
```

---

## 9. Object-Oriented Programming

> **Implementation status**: ⚠️ Partially implemented — classes, structs, enums, implicit self, access modifiers work; traits (§9.4), properties (§9.6), and operator overloading (§9.7) are parsed but not codegen'd.

### 9.1 Classes

Classes are syntactic sugar over structs + associated functions. The compiler tracks constructors (`init`) and destructors (`drop`) and inserts calls automatically.

```aether
class File {
    fd: int
    path: string

    func init( path: string) throws {
        self.fd = sys_open(path)
        self.path = path
    }

    func drop() {
        sys_close(self.fd)
    }

    pub func read( buf: ref [u8]): int {
        return sys_read(self.fd, buf)
    }

    pub prop path(): string {
        return self.path
    }
}
```

### 9.2 Implicit `self`

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

### 9.3 Classes Are Not Required

Pure procedural code with flat structs and free functions is always valid:

```aether
struct Point { x: int; y: int }

func distance(p: Point): float {
    return sqrt(p.x * p.x + p.y * p.y)
}
```

### 9.4 Traits (Interfaces)

> **⚠️ PARSED BUT NOT CODEGEN'D** — Trait declarations and impl blocks are parsed into AST nodes but no vtable or dispatch code is generated. Static dispatch via traits does not yet work.

Traits define shared behavior. They compile to vtable pointers only when used dynamically; static dispatch is the default.

```aether
trait Serializable {
    func serialize(): [u8]
}

impl Serializable for Point {
    func serialize(): [u8] {
        return to_bytes([self.x, self.y])
    }
}

// Static dispatch (zero-cost)
func save_static(value: ref Serializable) {
    let bytes = value.serialize()
}

// Dynamic dispatch (vtable)
func save_dynamic(value: ref dyn Serializable) {
    let bytes = value.serialize()
}
```

### 9.5 Access Modifiers

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

### 9.6 Properties (Inferred from Return Type)

> **⚠️ PARSED BUT NOT CODEGEN'D** — `prop` keyword is parsed into `NODE_PROPERTY` AST nodes but no getter/setter dispatch code is generated.

Properties are methods that look like fields. The compiler infers getter/setter from the return type:

- **Getter**: has a return type → `obj.prop` calls the getter
- **Setter**: has no return type → `obj.prop = value` calls the setter

```aether
class Temperature {
    celsius: f64

    // Getter — has return type
    prop fahrenheit(): f64 {
        return self.celsius * 9.0 / 5.0 + 32.0
    }

    // Setter — no return type
    prop fahrenheit(value: f64) {
        self.celsius = (value - 32.0) * 5.0 / 9.0
    }
}

let t = Temperature { celsius: 100 }
print(t.fahrenheit)  // calls getter → 212.0
t.fahrenheit = 32    // calls setter → celsius = 0
```

### 9.7 Operator Overloading

> **⚠️ PARSED BUT NOT CODEGEN'D** — `op_add`, `op_sub`, etc. method names are parsed as regular methods but no operator dispatch code is generated. The `+` operator is not overloadable for user types.

```aether
struct Vector2 {
    x: f64
    y: f64
}

impl Vector2 {
    func op_add( other: Vector2): Vector2 {
        return Vector2 { x: self.x + other.x, y: self.y + other.y }
    }

    func op_mul( scalar: f64): Vector2 {
        return Vector2 { x: self.x * scalar, y: self.y * scalar }
    }
}

let a = Vector2 { x: 1, y: 2 }
let b = Vector2 { x: 3, y: 4 }
let c = a + b          // calls op_add
let d = a * 2.0        // calls op_mul
```

---

## 10. Generics

> **Implementation status**: ⚠️ Partially implemented — generic type parameters are parsed and monomorphization is attempted in semantic analysis, but codegen for generic functions is not fully working. Test fixture exists (`test_generic.ae`, `test_monomorph.ae`).

### 10.1 Generic Functions

> **⚠️ PARSED BUT NOT CODEGEN'D** — Generic type parameters are parsed and monomorphization is attempted in semantic analysis, but the codegen path for generic functions is incomplete.

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
```

### 10.2 Generic Classes

```aether
class Stack<T> {
    data: [T]
    size: int

    pub func push( item: T) {
        self.data[self.size] = item
        self.size += 1
    }

    pub func pop(): T? {
        if self.size == 0 { return none }
        self.size -= 1
        return self.data[self.size]
    }
}
```

### 10.3 Generic Traits

```aether
trait Comparable<T> {
    func less_than( other: ref T): bool
}

impl<T> Comparable<T> for u64 {
    func less_than( other: ref u64): bool {
        return self < other
    }
}
```

### 10.4 Type Constraints

```aether
func min<T: Comparable>(a: T, b: T): T {
    if a.less_than(b) { return a }
    return b
}
```

---

## 11. Error Handling

> **Implementation status**: ✅ Fully implemented — deterministic exceptions with try/catch/finally, throw, error propagation, nested try/catch, and segfault handling all work. Test fixtures exist (`test_trycatch.ae`, `test_trycatch_nested.ae`, `test_trycatch_finally.ae`, `test_trycatch_catch_var.ae`, `test_throw.ae`, `test_segfault.ae`, `test_catch_all.ae`).

### 11.1 Deterministic Exceptions with Full Unwinding

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

### 11.2 Custom Error Types

```aether
enum MathError {
    DivisionByZero
    Overflow
    NegativeInput(i64)
}

func safe_sqrt(x: i64): i64 throws {
    if x < 0 {
        throw MathError::NegativeInput(x)
    }
    return sqrt(x)
}
```

### 11.3 Error Propagation

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

### 11.4 Catch-All

```aether
try {
    risky_operation()
} catch _ {
    print("Something went wrong, but we don't care what")
}
```

### 11.5 Nested Try/Catch

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

### 11.6 Segfault Handling (Hardware Faults → Exceptions)

Aether transparently catches hardware faults (segmentation faults, bus errors, page faults) inside try blocks. No special syntax, no signal handlers to write — it just works.

```aether
try {
    let ptr: *u64 = 0
    let val = *ptr  # null pointer dereference — caught!
} catch _ {
    print("caught a segfault")
}
```

If a hardware fault occurs **outside** a try block, or if no catch arm matches, the program prints a stack trace with the exact crash location and exits:

```
=== AETHER HARDWARE FAULT ===
Signal: 11, Fault address: 0x0
  Source: tests/fixtures/test_segfault.ae:8:12
```

The output shows:
- **Signal** — the type of fault (11 = SIGSEGV, 10 = SIGBUS)
- **Fault address** — the memory address that caused the fault (`0x0` = null pointer)
- **Source** — the exact file, line, and column where the crash happened

This works on all host targets (macOS, Linux) without any debug symbols or external tools. The compiler embeds a source map table directly into the binary.

**For kernel/freestanding targets:**
The compiler generates IDT entries for page fault (interrupt 14) and general protection fault (interrupt 13). When a fault occurs inside a try block, the IDT handler unwinds to the catch handler. If no try block is active, the default fault handler prints a stack trace and halts.

---

## 12. Compile-Time Execution

> **Implementation status**: ⚠️ Partially implemented — `#run` blocks are parsed and visited in semantic analysis, but no actual compile-time evaluation occurs. The codegen emits a comment stub. Test fixture exists (`test_comptime.ae`).

### 12.1 `#run` Blocks

> **⚠️ PARSED BUT NOT CODEGEN'D** — `#run` blocks are parsed into `NODE_RUN_BLOCK` AST nodes and visited during semantic analysis, but no compile-time evaluation engine exists. The codegen emits a comment and produces no runtime code.

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

---

## 13. Contract Programming

> **Implementation status**: ⚠️ Partially implemented — pre/post conditions are parsed and codegen'd with runtime checks; class invariants (§13.2) are not yet implemented. Test fixture exists (`test_contract.ae`).

### 13.1 Preconditions and Postconditions

> **✅ Implemented** — Pre/post conditions are parsed, codegen'd with runtime checks in debug builds, and eliminated in release builds. Test fixture: `test_contract.ae`.

```aether
func withdraw(account: ref Account, amount: u64)
    pre(account.balance >= amount)
    post(account.balance == old(account.balance) - amount)
{
    account.balance -= amount
}
```

### 13.2 Invariants

> **❌ NOT YET IMPLEMENTED** — The `inv()` syntax for class invariants is not parsed or codegen'd.

```aether
class Queue<T> {
    data: [T]
    head: u64
    tail: u64

    inv(self.size <= self.data.len)

    func size(): u64 {
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

> **Implementation status**: ⚠️ Partially implemented — non-capturing lambdas are parsed and codegen'd as standalone functions; closures with captures (§14.2) are not yet implemented. Test fixture exists (`test_closure.ae`).

### 14.1 Lambda Syntax

> **✅ Implemented** — Non-capturing lambdas are parsed and codegen'd as unique standalone functions. Test fixture: `test_closure.ae`.

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

> **❌ NOT YET IMPLEMENTED** — Capturing lambdas (closures that reference variables from the enclosing scope) are not yet supported. The codegen has a placeholder comment noting this is deferred.

```aether
func make_adder(x: int): func(int): int {
    return |y| -> x + y  // captures x by reference
}

let add5 = make_adder(5)
let result = add5(3)  // result = 8
```

### 14.3 Higher-Order Functions

> **⚠️ PARSED BUT NOT CODEGEN'D** — Higher-order functions (passing lambdas as arguments) are parsed but the function pointer dispatch is not fully codegen'd.

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

> **Implementation status**: ⚠️ Partially implemented — both properties and operator overloading are parsed but not codegen'd. See §9.6 and §9.7 for details.

### 15.1 Properties (Inferred from Return Type)

> **⚠️ PARSED BUT NOT CODEGEN'D** — See §9.6 annotation.

Properties are methods that look like fields. The compiler infers getter/setter from the return type:

- **Getter**: has a return type → `obj.prop` calls the getter
- **Setter**: has no return type → `obj.prop = value` calls the setter

```aether
class Temperature {
    celsius: f64

    // Getter — has return type
    prop fahrenheit(): f64 {
        return self.celsius * 9.0 / 5.0 + 32.0
    }

    // Setter — no return type
    prop fahrenheit(value: f64) {
        self.celsius = (value - 32.0) * 5.0 / 9.0
    }
}

let t = Temperature { celsius: 100 }
print(t.fahrenheit)  // calls getter → 212.0
t.fahrenheit = 32    // calls setter → celsius = 0
```

### 15.2 Operator Overloading

> **⚠️ PARSED BUT NOT CODEGEN'D** — See §9.7 annotation.

```aether
struct Vector2 {
    x: f64
    y: f64
}

impl Vector2 {
    func op_add( other: Vector2): Vector2 {
        return Vector2 { x: self.x + other.x, y: self.y + other.y }
    }

    func op_sub( other: Vector2): Vector2 {
        return Vector2 { x: self.x - other.x, y: self.y - other.y }
    }

    func op_mul( scalar: f64): Vector2 {
        return Vector2 { x: self.x * scalar, y: self.y * scalar }
    }

    func op_neg(): Vector2 {
        return Vector2 { x: -self.x, y: -self.y }
    }

    func op_eq( other: Vector2): bool {
        return self.x == other.x and self.y == other.y
    }
}
```

---

## 16. Dynamic Dispatch

> **Implementation status**: ❌ Not yet implemented — `dyn Trait` syntax is parsed but no vtable or fat pointer codegen exists. Test fixture exists (`test_dyn.ae`).

### 16.1 `dyn Trait` Syntax

> **⚠️ PARSED BUT NOT CODEGEN'D** — `dyn` keyword and trait impl blocks are parsed but no vtable dispatch code is generated. The test fixture (`test_dyn.ae`) only tests parsing.

Dynamic dispatch uses fat pointers (vtable pointer + data pointer). Use `dyn Trait` to opt in.

```aether
trait Drawable {
    func draw()
    func area(): f64
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

## 17. Inline Assembly

> **Implementation status**: ✅ Fully implemented — inline asm blocks, const values in asm, output bindings, labels/jumps, top-level asm blocks, extern hoisting, and rax preservation all work.

### 17.1 Basic Inline Assembly

Full NASM syntax is a first-class citizen. The asm block text is emitted verbatim into the generated assembly. Aether function parameters are **not** automatically substituted — use SysV ABI registers directly (rdi=arg1, rsi=arg2, rdx=arg3, rcx=arg4, r8=arg5, r9=arg6).

```aether
func writePortByte(port: u16, value: byte) {
    asm {
        mov dx, rdi
        mov al, sil
        out dx, al
    }
}
```

### 17.2 Const Values in Assembly

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

### 17.4 Assembly with Output Bindings

Use `asm: (outputs) { body }` to bind assembly results to Aether variables.

```aether
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

### 17.3 Labels and Jumps

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

### 17.4 Multi-Architecture Assembly

The same NASM syntax works on all targets via the multi-target assembler:

```aether
// Write once in NASM syntax:
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

### 17.5 Top-Level asm Blocks

At file level (outside any function), `asm { }` emits raw assembly text directly into the output without any function wrapping. This is used for declaring BSS/data sections, global symbols, and other NASM directives that must appear at file scope.

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

Top-level asm blocks are emitted in declaration order, before any function code. They are useful for:
- Declaring BSS variables with `resb`/`resq`/`resw`/`resd`
- Declaring data tables with `db`/`dw`/`dd`/`dq`
- Defining global symbols that functions reference
- Emitting NASM directives like `align`, `section`, `global`, `extern`

### 17.6 Extern Hoisting

> **✅ Implemented** — Extern declarations from asm blocks are automatically hoisted to file level.

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

The compiler emits:
```nasm
; Extern declarations (hoisted from asm blocks)
        extern __bss_start
        extern __bss_end

section .text
...
main:
        mov rdi, __bss_start
        mov rcx, __bss_end
        sub rcx, rdi
        xor al, al
        rep stosb
```

### 17.7 Asm Block RAX Preservation

> **✅ Implemented** — The compiler no longer zeros `rax` after asm blocks. When a function body is (or ends with) an asm block, the compiler skips the default `xor rax, rax` return-value zeroing, trusting that the asm block already set `rax` to the intended return value. This is detected by checking if the function body is `NODE_ASM_BLOCK` or if the last statement in a `NODE_BLOCK` is an asm block.

```aether
func get_forty_two(): u64 {
    asm {
        mov rax, 42
    }
    // No "xor rax, rax" emitted here — rax preserved from asm block
}
```

---

## 18. Aether OS Integration

> **Implementation status**: ⚠️ Partially implemented — freestanding and host-native targets work; syscall page, module interface, file imports, binary entry points, and memory layout directives are implemented. `pool` and `protocol` declarations are parsed but codegen is a comment stub.

### 18.1 Freestanding Target

The compiler's primary target is **x86_64-freestanding**. No libc, no CRT, no OS assumptions.

```bash
aether build --target x86_64-freestanding --output kernel.elf
```

### 18.2 Host-Native Target

The compiler also outputs **host-native formats** so you can compile and run `.ae` programs directly on your development machine.

| Host OS | Format | Linker |
|---------|--------|--------|
| macOS   | Mach-O 64 (x86_64) | `ld` (system) or direct syscall emission |
| Linux   | ELF64 | `ld` (system) or direct syscall emission |
| Windows | PE32+ | `link.exe` or direct |

### 18.3 Syscall Page (0x5000)

> **✅ Implemented** — The compiler knows the Aether syscall table and generates optimal call sequences. On freestanding targets, `print()` uses slot 1 of the syscall page at `0x5008`.

The compiler knows the Aether syscall table and generates optimal call sequences:

```aether
sys func putc(c: byte) at(0)
sys func puts(s: string) at(1)
sys func open(path: string): int at(2)
sys func read(fd: int, buf: ref [u8]): int at(3)
sys func exit() at(7)
```

### 18.4 Module Interface (0x4000)

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

### 18.5 File Imports

Aether supports file-level imports via the `import` keyword. Imports are resolved at compile time — the compiler reads the imported file, parses it, and merges its declarations into the current compilation unit.

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
   - Parses it using `parser_create_with_arena()` (shares the main parser's arena so AST nodes persist)
   - Merges the imported declarations into the main program's declaration list
   - Removes the `NODE_IMPORT` node from the list
3. The imported source buffer is kept alive because `StringView` fields in the AST point into it
4. Circular imports are detected and skipped (tracked by path)

**Semantic analysis** uses a two-pass approach to handle forward references across files:
1. First pass: declares all top-level names (functions, consts, structs, enums, traits, classes)
2. Second pass: visits function bodies and resolves identifiers

**Dead Code Elimination (DCE)** also uses a two-pass approach:
1. First pass: adds all top-level functions to the symbol table
2. Second pass: collects references from all function bodies
3. Removal pass: only removes functions that are neither entry points nor referenced

**Limitations:**
- Only file-level imports are supported (no module-level or selective imports)
- Imported files must use relative paths (resolved from the importing file's directory)
- The `import` keyword currently requires a string literal path (not a bare module name)

### 18.6 Binary Entry Points

```aether
@entry(0x2000000)
func main(args: [][]byte): int {
    puts("Hello from Aether binary!\n")
    return 0
}
```

### 18.6 Memory Layout Directives

```aether
@layout(start=0x7C00, max=512, file="stage1.bin")
func stage1_mbr() {
    asm { ... 512-byte MBR ... }
}
```

### 18.7 Kernel Layout Verification

```aether
@kernel_layout
func init_memory() {
    // Compiler knows the memory map and verifies no overlap
    let bitmap = reserved(0x1000, 0x1000)
    let registry = reserved(0x4000, 0x1000)
    let syscall = reserved(0x5000, 0x1000)
}
```

### 18.8 Declarative Resources

> **⚠️ PARSED BUT NOT CODEGEN'D** — `pool` declarations are parsed into `NODE_POOL_DECL` AST nodes but codegen emits only a comment stub. No pool alloc/free code is generated.

```aether
// Declare a memory pool for USB transfers
pool UsbDmaBuffer of size 64, count 32, alignment 256

// Use it — compiler generates pool alloc/free
func alloc_usb_buf(): UsbDmaBuffer {
    return UsbDmaBuffer()  // from the declared pool
}  // compiler inserts: return to pool on drop
```

**Syntax**: `pool <Name> of size <N>, count <M>, alignment <A>`

The parser accepts `pool`, `of`, `size`, `count`, and `alignment` as keywords. The AST node stores `name`, `size`, `count`, and `alignment` fields. Codegen currently emits `; pool declaration (reserved)` and produces no runtime code.

### 18.9 Protocol Generation

> **⚠️ PARSED BUT NOT CODEGEN'D** — `protocol` declarations are parsed into `NODE_PROTOCOL_DECL` AST nodes but codegen emits only a comment stub. No protocol dispatch or bit-banging code is generated.

```aether
protocol Serial {
    port base = 0x3F8
    speed = 115200

    func putc(c: byte) {
        asm { mov dx, port; mov al, c; out dx, al }
    }
}
```

**Syntax**: `protocol <Name> { <field_decls> <method_decls> }`

The `protocol` keyword is parsed at the top level. The AST node (`NODE_PROTOCOL_DECL`) stores a `name` and a list of method declarations. Codegen currently emits `; protocol declaration (interface)` and produces no runtime code. This is distinct from the `protocol` block in §28.1 (Automatic Protocol Generation for hardware), which is a separate feature.

---

## 19. Multi-Target Assembler

> **Implementation status**: ✅ Fully implemented — NASM parser, AsmIR, and backends for x86_64 (passthrough), ARM64, and RISC-V all work. Register translation and instruction mapping tables are complete.

### 19.1 Architecture

The multi-target assembler translates NASM syntax to target architecture assembly:

```
NASM Source → NASM Parser → AsmIR (intermediate representation)
  → x86_64 Backend (passthrough)
  → ARM64 Backend (instruction mapping)
  → RISC-V Backend (instruction mapping)
```

### 19.2 Register Translation

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

### 19.3 Instruction Translation Examples

| NASM | ARM64 | RISC-V |
|------|-------|--------|
| `mov rax, rbx` | `MOV X0, X19` | `MV a0, s0` |
| `add rax, 5` | `ADD X0, X0, #5` | `ADDI a0, a0, 5` |
| `jmp label` | `B label` | `J label` |
| `call func` | `BL func` | `JAL ra, func` |
| `ret` | `RET` | `JALR zero, ra, 0` |
| `push rax` | `STR X0, [SP, #-16]!` | `ADDI sp, sp, -16; SD a0, 0(sp)` |
| `pop rax` | `LDR X0, [SP], #16` | `LD a0, 0(sp); ADDI sp, sp, 16` |

### 19.4 Usage

```bash
# Show ARM64 translation of NASM assembly
aether --target asm-arm64 source.ae -o output.asm

# Show RISC-V translation
aether --target asm-riscv64 source.ae -o output.asm
```

---

## 20. Universal Binaries

> **Implementation status**: ⚠️ Partially implemented — universal binary concept and CPU detection trampoline are designed; the `--target universal` flag exists but the multi-arch ELF packaging is not fully integrated.

### 20.1 Concept

Aether supports compiling a single source file into a **universal binary** that runs natively on multiple architectures without an interpreter, JIT, or emulation layer.

### 20.2 Binary Layout

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

### 20.3 CPU Detection Trampoline

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

### 20.4 CLI Usage

```bash
# Build a universal binary for x86_64 + ARM64
aether build --target universal --output kernel.elf

# Build for all three architectures
aether build --target universal-all --output kernel.elf
```

---

## 21. Standard Library

> **Implementation status**: ⚠️ Partially implemented — `std.io` (print, println), `std.mem` (alloc, free), `std.serial` (COM1, putc, puts), `std.test` (assert, test_runner) are implemented. Other modules are planned.

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

## 22. Build System

> **Implementation status**: ⚠️ Partially implemented — `aether build`, `aether run`, and `aether test` CLI commands work. Other commands (`fmt`, `doc`, `asm`, `inspect`, `init`) are planned.

### 22.1 `aether` CLI

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

### 22.2 Project Structure

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

### 22.3 `aether.toml` Manifest

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

## 23. Compiler Targets

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

## 24. Think Outside the Box: Unique Aether Innovations

> **Implementation status**: ⚠️ Mostly aspirational — compile-time OS knowledge (§24.1) and zero-cost error context (§24.10) are partially implemented. Most other features in this section (§24.2–§24.12) are aspirational/planned.

This section describes the features that make Aether genuinely different from other systems languages.

### 24.1 Compile-Time OS Knowledge

The compiler has baked-in knowledge of the Aether OS architecture. It knows the memory map, the syscall table layout, the module registry structure, and the boot chain requirements. This means:

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

### 24.2 Goal-Oriented I/O

Instead of describing *how* to read a file, describe *what* you want:

```aether
// The compiler generates the optimal read path:
// - Boot-time: raw ATA PIO reads
// - Userspace: AetherFS syscalls
// - In-memory FS: direct pointer access
let config = from "/etc/aether.cfg" read Config
```

### 24.3 Pattern-Based Metaprogramming

Instead of C++ templates or macros, Aether uses pattern matching on types:

```aether
impl<T> trait Hashable {
    func hash(): u64 {
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

### 24.4 Query-Style Data Transformations

Chain operations on collections — the compiler fuses them into a single loop with no intermediate allocations:

```aether
let active_users = db.users
    .filter(|u| u.active)
    .map(|u| (u.name, u.email))
    .sort(|u| u.0)
    .collect()
```

This compiles to a single fused loop. No temporary arrays, no allocation overhead.

### 24.5 Automatic Protocol Generation

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

### 24.6 Compile-Time Hardware Configuration

```aether
// Detect hardware at compile time and generate optimized code
#run {
    if target_arch() == "x86_64" {
        emit("func flush_tlb() { asm { mov cr3, cr3 } }")
    } elif target_arch() == "arm64" {
        emit("func flush_tlb() { asm { dsb ish; tlbi vmalle1is; dsb ish; isb } }")
    }
}
```

### 24.7 Self-Documenting Binary Format

Aether binaries contain embedded metadata that the OS can read:

```aether
@metadata {
    author = "Aether Team"
    description = "Kernel main binary"
    license = "MIT"
    required_abi = "1.0"
}
```

The compiler embeds this as an ELF note section. The OS can query it without loading the binary.

### 24.8 Capability-Based Security

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

### 24.9 Automatic Boot Chain Generation

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

### 24.10 Zero-Cost Error Context

```aether
func read_config(): throws Config {
    let f = File::open("/etc/aether.cfg") ? "Failed to open config"
    let data = f.read_all() ? "Failed to read config"
    return parse(data) ? "Failed to parse config"
}

// The `?` operator attaches context to errors.
// In debug builds: error messages are preserved.
// In release builds: only the error discriminant remains (zero-cost).
```

### 24.11 Compile-Time Unit Checking

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

### 24.12 Automatic Interrupt Handler Generation

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
    let scancode = readPortByte(0x60)
    handle_key(scancode)
}
```

---

---

## 25. Concurrency and Fibers

### 25.1 Spawn

```aether
func worker(id: u64) {
    print("Worker {id} started")
}

spawn worker(1)
spawn worker(2)
```

### 25.2 Synchronization Primitives

```aether
var lock: Mutex
lock.acquire()
// critical section
lock.release()
```

### 25.3 Channels

```aether
let ch: Chan<u64>
ch.send(42)
let val = ch.recv()
```

### 25.4 Fiber Scheduler Support

The compiler generates code compatible with the Aether OS fiber scheduler:

- Cooperative multitasking with explicit yield
- No preemption, no timer interrupts needed
- Per-fiber stack allocation
- Yield points at blocking I/O, timer waits, and explicit `yield` calls

---

## 26. Advanced OS Integration

### 26.1 Automatic Boot Chain Generation

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

### 26.2 Automatic Interrupt Handler Generation

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
    let scancode = readPortByte(0x60)
    handle_key(scancode)
}
```

### 26.3 Self-Documenting Binary Format

```aether
@metadata {
    author = "Aether Team"
    description = "Kernel main binary"
    license = "MIT"
    required_abi = "1.0"
}
```

The compiler embeds this as an ELF note section. The OS can query it without loading the binary.

### 26.4 Capability-Based Security

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

### 26.5 Compile-Time Unit Checking

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

---

## 27. Goal-Oriented I/O and Query Fusion

### 27.1 Goal-Oriented I/O

Instead of describing *how* to read a file, describe *what* you want:

```aether
// The compiler generates the optimal read path:
// - Boot-time: raw ATA PIO reads
// - Userspace: AetherFS syscalls
// - In-memory FS: direct pointer access
let config = from "/etc/aether.cfg" read Config
```

### 27.2 Query-Style Data Transformations

Chain operations on collections — the compiler fuses them into a single loop with no intermediate allocations:

```aether
let active_users = db.users
    .filter(|u| u.active)
    .map(|u| (u.name, u.email))
    .sort(|u| u.0)
    .collect()
```

This compiles to a single fused loop. No temporary arrays, no allocation overhead.

### 27.3 Pattern-Based Metaprogramming

Instead of C++ templates or macros, Aether uses pattern matching on types:

```aether
impl<T> trait Hashable {
    func hash(): u64 {
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

## 28. Protocol Generation and Hardware Configuration

### 28.1 Automatic Protocol Generation

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

### 28.2 Compile-Time Hardware Configuration

```aether
// Detect hardware at compile time and generate optimized code
#run {
    if target_arch() == "x86_64" {
        emit("func flush_tlb() { asm { mov cr3, cr3 } }")
    } elif target_arch() == "arm64" {
        emit("func flush_tlb() { asm { dsb ish; tlbi vmalle1is; dsb ish; isb } }")
    }
}
```

---

## 29. Module System and Imports

### 29.1 File as Module

Each `.ae` file is a module. The filename (without extension) is the module name.

### 29.2 Source Imports (`.ae` files)

```
import "math.ae"
import "io/file.ae"
```

Imports are resolved at compile time: the compiler reads the file, parses it with a shared arena, and merges declarations. Circular imports are detected. Two-pass semantic analysis (declare all names first, then visit bodies) handles forward references across files.

### 29.3 Library Imports (`.aelib` files) — Phase 19 (planned)

For closed-source third-party library distribution, Aether uses `.aelib` — an archive format containing compiled code (`.o` files) plus a metadata section with type signatures, class layouts, and the export table. The metadata enables code completion and compile-time validation while the archive format prevents trivial reverse engineering with `objdump`.

**File format (version 1):**
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

**Metadata section format:**
- Header: magic `"AEMETA\0"`, version `0x0001`
- Symbol table: name, kind (function/struct/class/global/const/enum), flags (public bit), namespace, type data offset/size
- Type data: function signatures (return type, param count, params with name/type/mutability), struct/class layouts (field offsets/sizes), enum variants
- String table: null-terminated strings concatenated

### 29.4 Import Syntax

```
# All public decls from a library
import "libtest"

# Only public decls from a specific class/namespace
import "libtest" : Foo

# From multiple classes
import "libtest" : Foo, Bar

# Explicit .aelib file
import "libtest.aelib" : Foo

# Standard library (auto-resolved by compiler)
import "std/io"
```

### 29.5 Resolution Order

For `import "foo"` (no extension), the compiler tries in order:
1. `foo.ae` — source file (for code you own)
2. `foo.aelib` — pre-compiled library
3. `foo/lib.ae` or `foo/lib.aelib` — package directory
4. Standard library paths: `$AETHER_LIB`, `~/.aether/lib`, `/usr/local/lib/aether`

If none found, the compiler reports an error listing what was tried.

### 29.6 Public, Private, Internal Visibility

```
pub func exported() { }          // visible to importers
private func internal_only() { } // only within this file
internal func package_only() { }  // within the same package
func default_public() { }        // public by default (for simplicity)
```

For `.aelib` libraries, visibility is enforced via metadata gating — `private` and `internal` declarations are omitted from the metadata entirely, so they literally cannot be called from outside the library.

### 29.7 Build Command

```
aether build --target lib libtest.ae -o libtest.aelib
```

The new `--target lib` target produces code + metadata in one `.aelib` file.

### 29.8 Linking Modes

When importing a `.aelib`, the consumer has two options:

- **Static linking** (default for `--target kernel`, `--target binary`): extract `.o` files from the archive, link them into the final binary. Everything in one executable — no runtime dependencies.
- **Dynamic linking** (for `--target host`): reference the `.aelib` as a shared library, runtime resolution via the OS dynamic linker. Smaller binaries, can update libraries without recompiling.

### 29.9 Multi-Arch Libraries

A `.aelib` can contain code for multiple architectures (x86_64, ARM64, RISC-V). The metadata is arch-agnostic — it references symbols by name. The linker picks the right arch at build time, like macOS fat dylibs.

### 29.10 Why Class/Namespace Imports, Not Function-Level

Function-level imports create ambiguity problems (multiple overloads with different signatures — which do you import?). Class-level is unambiguous: import the whole class, use `Foo.sayHello()` to call methods. This is how Python modules, Java classes, and C# namespaces work. The class is the natural unit of grouping.

### 29.11 Why `.aelib`, Not `.aeb`?

An ELF file with a metadata section (`.aeb`) is trivially reverse-engineerable with `objdump -d` — anyone can disassemble the code sections. An archive format (`.aelib`) requires writing a custom disassembler to extract the code sections AND understanding the archive layout. This matches how dylibs work on every platform (macOS, Linux, Windows) and is the natural choice for distributing compiled libraries without exposing source.

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

## Appendix C: Compiler Pipeline

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

---

*End of Aether Language Specification v1.0*

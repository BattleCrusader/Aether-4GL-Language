# Aether Language Specification v0.1

*A fourth-generation systems language for the Aether Operating System*

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Lexical Structure](#2-lexical-structure)
3. [Types](#3-types)
4. [Variables and Constants](#4-variables-and-constants)
5. [Functions](#5-functions)
6. [Control Flow](#6-control-flow)
7. [Memory Model](#7-memory-model)
8. [Reference System](#8-reference-system)
9. [Structs](#9-structs)
10. [Enums and Pattern Matching](#10-enums-and-pattern-matching)
11. [Classes](#11-classes)
12. [Traits](#12-traits)
13. [Generics](#13-generics)
14. [Exception Handling](#14-exception-handling)
15. [NASM Inline Assembly](#15-nasm-inline-assembly)
16. [Aether OS Integration](#16-aether-os-integration)
17. [Compile-Time Execution](#17-compile-time-execution)
18. [Contract Programming](#18-contract-programming)
19. [Declarative Resources](#19-declarative-resources)
20. [Build System](#20-build-system)
21. [Testing](#21-testing)
22. [Standard Library](#22-standard-library)
23. [Appendix: Grammar](#23-appendix-grammar)

---

## 1. Introduction

Aether is a compiled, statically-typed, fourth-generation language for systems programming on the Aether OS. It combines the readability of Python, the safety of Rust, and the bare-metal control of C with NASM — all without a runtime, garbage collector, or interpreter.

### 1.1 Design Goals

| Goal | How Aether achieves it |
|------|----------------------|
| Productivity | Indentation-based syntax, powerful type inference, pattern matching, query-style pipelines |
| Safety | No null, ownership system, automatic memory management, contract programming |
| Zero-cost | Stack-first allocation, monomorphized generics, static dispatch by default, escape analysis |
| Hardware intimacy | NASM inline assembly, memory layout directives, syscall-page integration |
| Self-hosting | Compiler written in Aether by Phase 6, bootstrap via C |
| OS-native | `sys func`, `module`, `@entry`, `@layout` — the compiler knows the Aether OS architecture |

### 1.2 Hello, Aether

```aether
# hello.ae — your first Aether program
func main() {
    print("Hello, Aether!\n")
}
```

Compile and run:

```
aether build --target x86_64-freestanding --output hello.elf
# or directly:
aether run hello.ae
```

### 1.3 Hello, Kernel Module

```aether
module hello_kernel {
    @export func mod_init() int {
        print("Hello from kernel module!\n")
        return 0  # success
    }
    
    @export func mod_fini() {
        print("Goodbye from kernel module!\n")
    }
}
```

### 1.4 Hello, Boot Sector (512 bytes)

```aether
@layout(start=0x7C00, max=512, file="stage1.bin")
func stage1() {
    asm {
        [org 0x7C00]
        mov ax, 0
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7C00
        
        mov si, msg
        call print_string
        jmp $
        
print_string:
        lodsb
        or al, al
        jz .done
        mov ah, 0x0E
        int 0x10
        jmp print_string
.done:
        ret
        
msg: db "Aether boot!", 0
        
        times 510-($-$$) db 0
        dw 0xAA55
    }
}
```

---

## 2. Lexical Structure

### 2.1 Comments

```aether
# Line comment — starts with # and goes to end of line

#{ Block comment — nestable
    #{ Nested block comment }#
    Still inside block comment
}#
```

### 2.2 Identifiers

```aether
x                  # valid
camelCase          # conventional for variables
PascalCase         # conventional for types, classes, traits
SCREAMING_CASE     # conventional for constants
_private           # underscore prefix = module-private
_special           # special/compiler-intrinsic
```

Valid identifiers: `[a-zA-Z_][a-zA-Z0-9_]*`

### 2.3 Literals

```aether
# Integer literals
42                  # decimal
0xFF                # hexadecimal
0b1010              # binary
0o77                # octal
1_000_000           # underscores for readability
0xFFFF_FFFF         # groups of 4 in hex
'Z'                 # character (u8 ascii value)
'\n'                # escape sequences in chars

# Float literals
3.14                # float
1e10                # scientific
0xFF.0p-3           # hex float

# String literals
"hello"             # UTF-8 string
"line one\nline two"  # escape sequences
"path: {filename}"  # interpolation with {expr}

# Boolean literals
true
false

# Special literals
none                # the absence of a value (for optionals)
```

### 2.4 Escape Sequences

| Escape | Meaning |
|--------|---------|
| `\n` | Newline (0x0A) |
| `\r` | Carriage return (0x0D) |
| `\t` | Tab (0x09) |
| `\\` | Backslash |
| `\"` | Double quote |
| `\xNN` | Hex byte |
| `\u{NNNN}` | Unicode code point |

### 2.5 Indentation

Aether uses significant indentation. Blocks are defined by indentation levels. One level = 4 spaces. Tabs are not allowed.

```aether
func example() {
    print("indented one level\n")
    if true {
        print("indented two levels\n")
    }
    print("back to one level\n")
}
```

### 2.6 Statement Termination

Statements end at newlines unless the expression clearly continues:

```aether
# Multi-line expression (continues because line ends with operator)
let result = 1 + 2 + 3 +
             4 + 5 + 6

# Multi-line function call
print("this is a very long string that ",
      "continues on the next line")

# Explicit continuation with backslash
print("backslash continues \",
    "to the next line")
```

---

## 3. Types

### 3.1 Primitive Types

| Type | Size | Description |
|------|------|-------------|
| `byte` | 8 bits | Raw byte (unsigned), distinct from `u8` in intent |
| `u8` | 8 bits | Unsigned 8-bit integer |
| `u16` | 16 bits | Unsigned 16-bit integer |
| `u32` | 32 bits | Unsigned 32-bit integer |
| `u64` | 64 bits | Unsigned 64-bit integer |
| `i8` | 8 bits | Signed 8-bit integer |
| `i16` | 16 bits | Signed 16-bit integer |
| `i32` | 32 bits | Signed 32-bit integer |
| `i64` | 64 bits | Signed 64-bit integer |
| `f32` | 32 bits | IEEE 754 single-precision float |
| `f64` | 64 bits | IEEE 754 double-precision float |
| `bool` | 8 bits | Boolean (`true` or `false`) — stored as byte |
| `void` | 0 bits | Unit type, for functions returning nothing |

### 3.2 Compound Types

```aether
# Arrays (fixed size, stack by default)
let arr: [u8; 256]           # 256 bytes on stack
let arr2 = [1, 2, 3, 4, 5]  # inferred: [int; 5]

# Slices (dynamically sized view)
let slice: [u8]              # slice of bytes (fat pointer: ptr + len)
let view = arr[10..20]       # slice from array

# Tuples
let t: (int, string, bool)   # heterogeneous tuple
let t2 = (42, "hello", true) # inferred
let (a, b, _) = t2           # destructuring

# String (a slice of bytes guaranteed to be UTF-8)
let s: string                # shorthand for [byte] that is valid UTF-8
let s2 = "hello"             # string literal
```

### 3.3 Optional Types

Aether has no null pointer. Instead, any type can be wrapped in an optional with the `?` suffix. An `int?` is either `some` (holding an int) or `none`. The compiler stores optionals as a tag byte + value, so `u64?` costs 9 bytes at runtime.

```aether
# Declaring an optional — starts as none by default
let age: int? = none

# Assigning a value wraps it automatically
age = 30                      # now holds 30, tag is 'some'

# The type system prevents you from using the value without checking
# let years = age + 1         # ERROR: can't add int? + int

# if-let pattern binding: unwraps safely, only runs the block if some
if let actual = age {
    print("age is {actual}")   # actual is int, not int?
    # actual only exists inside this block
}

# Unwrap with a default value
let display = age or 0        # 30 if some, 0 if none

# Force-unwrap (crashes if none — use sparingly, like a safety valve)
let risky = age!              # runtime panic if age is none

# Optionals compose with control flow
for i in 0..100 {
    let maybe: int? = lookup(i)
    if let val = maybe {
        process(val)
    }
}
```

### 3.4 Type Aliases

```aether
type PageNumber = u64
type Buffer = [u8; 4096]
type Result<T> = Ok(T) | Err(string)  # anonymous enum

type GenericList<T> = struct {
    data ptr T
    count u64
    capacity u64
}
```

### 3.5 Type Inference

The compiler infers types from context. Explicit annotations are optional except for function signatures and struct fields.

```aether
let x = 42                    # inferred: int
let y: u64 = 42               # explicit: u64
let z = compute()             # inferred from return type of compute()

# In function signatures, all parameters and return types must be annotated
func add(a: int, b: int): int {
    return a + b
}

# But local variables inside the body are inferred
func example() {
    let count = get_count()   # inferred
    let doubled = count * 2   # inferred (u64 from context)
}
```

---

## 4. Variables and Constants

### 4.1 Immutable Variables (Default)

```aether
let x = 42          # immutable
# x = 43            # ERROR: cannot assign to immutable variable x
```

### 4.2 Mutable Variables

```aether
let mut counter = 0
counter += 1        # OK: counter is mutable

let mut buf = Buffer()
buf.fill(0)
```

### 4.3 Constants

```aether
const MAX_BUFFER_SIZE = 4096
const PI: f64 = 3.14159265359
const PAGE_SIZE: u64 = 0x1000
```

Constants are compile-time evaluated and inlined at every use site. No memory is allocated for them.

### 4.4 Shadowing

Variable shadowing is allowed:

```aether
let x = 10
print(x)           # 10
let x = x * 2      # shadows previous x
print(x)           # 20
```

---

## 5. Functions

### 5.1 Basic Functions

```aether
func greet() {
    print("Hello!\n")
}

# With parameters (name: type)
func greet_name(name: string) {
    print("Hello, {name}!\n")
}

# With return value (: type)
func add(a: int, b: int): int {
    return a + b
}

# Implicit return (last expression)
func multiply(a: int, b: int): int -> a * b
```

> **Syntax note:** Parameters use `name: type` notation (required). Return types use `: type` after the parameter list (required when a return type is present).

### 5.2 Syntax Summary

```aether
# Function declaration syntax:
#   func name(param: type, ...): return_type { body }
# Parameters use colon syntax: name: type
# Return type uses colon syntax: : type (after the closing paren)
```

### 5.3 Multiple Return Values

```aether
func divmod(a: int, b: int): (int, int) {
    return (a / b, a % b)
}

let (q, r) = divmod(17, 5)
let result = divmod(17, 5)
```

### 5.4 Named Returns

```aether
func divide(a: int, b: int): (quotient: int, remainder: int) {
    quotient = a / b
    remainder = a % b
    return
}
```

### 5.5 Default Parameters

```aether
func create_buffer(size: int = 4096): [byte] {
    return heap [byte; size]
}

let default_buf = create_buffer()      # 4096
let custom_buf = create_buffer(1024)   # 1024
```

### 5.6 Varargs

```aether
func sum(args: ...int): int {
    let mut total = 0
    for arg in args {
        total += arg
    }
    return total
}

let result = sum(1, 2, 3, 4, 5)  # 15
```

### 5.7 Recursion

```aether
func factorial(n: u64): u64 {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}

# Tail-recursive version (compiler may optimize to loop)
func factorial_tr(n: u64, acc: u64 = 1): u64 {
    if n <= 1 { return acc }
    return factorial_tr(n - 1, acc * n)
}
```

### 5.8 Closures / Lambdas

```aether
let increment = |x| x + 1
let result = increment(41)  # 42

let add = |a, b| a + b

# Multi-line closures use { }
let process = |items: [int]| {
    let mut sum = 0
    for i in items { sum += i }
    return sum
}
```

### 5.9 Higher-Order Functions

```aether
func apply(T)(value: T, f: func(T) T): T -> f(value)

let double = |x: int| x * 2
let result = apply(21, double)  # 42
```

---

## 6. Control Flow

### 6.1 If / Elif / Else

All branches are **expressions** — they can produce values.

```aether
let status = if x > 0 {
    "positive"
} elif x < 0 {
    "negative"
} else {
    "zero"
}

# Statement form (no value needed)
if x > 0 {
    print("positive\n")
}
```

### 6.2 While Loops

```aether
let mut i = 0
while i < 10 {
    print("{i}\n")
    i += 1
}

# Infinite loop (kernel entry point pattern)
while true {
    handle_interrupts()
}
```

### 6.3 For Loops

```aether
# Range loop
for i in 0..10 {
    print(i)   # 0 1 2 3 4 5 6 7 8 9
}

for i in 0..=10 {
    print(i)   # 0 1 2 3 4 5 6 7 8 9 10
}

# Step
for i in (0..100).step(2) {
    print(i)   # 0 2 4 6 ...
}

# Over collections
for item in array {
    print(item)
}

# With index
for i, item in enumerate(array) {
    print("index {i}: {item}\n")
}

# Reverse
for i in (0..10).reverse() {
    print(i)   # 9 8 7 ... 0
}
```

### 6.4 Loop Control

```aether
for i in 0..100 {
    if i % 2 == 0 { continue }   # skip evens
    if i > 50 { break }           # stop at 50
    print(i)
}
```

### 6.5 Match

Match is Aether's primary branching construct for algebraic types. Unlike if/elif chains, match **exhaustively checks all possibilities** at compile time. The compiler can warn when you miss a case.

```aether
# Basic integer matching — each case is checked in order
match value {
    case 0 => print("zero")
    case 1 => print("one")
    case 2..9 => print("small")
    case 10..=100 => print("medium")
    case > 100 => print("large")
    case _ => print("default")      # wildcard catches everything
}

# Match on enum variants (algebraic data types)
enum Result {
    Ok(int)
    Err(string)
}

match result {
    case Ok(val) => print("success: {val}")
    case Err(msg) => print("error: {msg}")
}
# Compiler checks: Ok and Err both handled — no wildcard needed

# Match on struct content — destructuring with field patterns
match point {
    case Point(0, 0) => print("origin")
    case Point(x, 0) => print("on x-axis at {x}")
    case Point(0, y) => print("on y-axis at {y}")
    case Point(x, y) => print("at ({x}, {y})")
}

# Guard clauses — additional conditions on a pattern
match x {
    case n if n > 0 => "positive"
    case n if n < 0 => "negative"
    case _ => "zero"
}

# Match as an expression — all arms must return the same type
let description = match x {
    case 0 => "none"
    case 1 => "one"
    case _ => "many"
}
```

### 6.6 Defer

`defer` schedules code to run when the current scope exits — whether by normal completion, early return, or error. Think of it as a guaranteed cleanup hook, like Python's `contextlib` or Golang's `defer`. Defers execute in last-in-first-out order (stack discipline).

```aether
# Basic defer — runs when the function returns
func read_config(): string {
    let fd = open("/etc/aether.cfg")
    defer { close(fd) }        # guaranteed to run when scope exits
    let content = read_all(fd)
    return content
    # close(fd) called automatically right here, before the actual return
}

# Multiple defers run in reverse order (LIFO — stack discipline)
func transfer_data() {
    let source = open_source()
    defer { close_source(source) }    # runs second

    let dest = open_dest()
    defer { close_dest(dest) }        # runs first (LIFO)

    copy(source, dest)
    # close_dest called, then close_source
}

# Defers work in any scope, not just functions
func process_chunk() {
    {
        let buf = allocate(1024)
        defer { free(buf) }
        fill(buf)
    }  # buf freed here
    print("buffer was already freed")
}

# Defers with class instances are automatic — see §11.3
```

---

## 7. Memory Model

### 7.1 Stack Allocation (Default)

All local variables live on the stack by default. The compiler tracks lifetimes and inserts destructor calls at scope exit.

```aether
func process() {
    let p = Point(3, 4)      # struct lives on stack
    let arr = [1, 2, 3]      # array on stack
    # compiler generates any necessary destructor calls
    # when scope exits
}
```

### 7.2 Escape Analysis

When a reference to a stack variable could outlive the stack frame, the compiler **automatically promotes** the allocation to the heap.

```aether
func make_pair(a int, b int) ref Pair {
    let p = Pair(a, b)
    # Compiler detects: p's reference escapes this function
    # Auto-promotes to heap allocation
    return p
}
```

The compiler analyzes:
- Return references
- Assignment to outer-scope variables
- Storage in longer-lived data structures
- Closure captures

### 7.3 Explicit Heap Allocation

```aether
let big = heap Buffer(1024 * 1024)      # explicit heap allocation
let shared = heap rc SharedState()       # heap + ref-counted
```

### 7.4 Region-Based Allocation

Regions group allocations into arenas that are freed as a batch:

```aether
region("network") {
    let pkt = Packet(header, payload)   # allocated from region
    let buf = Buffer(2048)
    process(pkt)
}  # all region allocations freed at once — O(1) teardown
```

Regions compile to:
1. Arena allocator setup at region entry
2. Normal allocation within the region
3. Arena teardown (batch free) at region exit

This is ideal for OS kernel code — interrupt handlers, packet processing, syscall handlers — where you want O(1) cleanup regardless of allocation complexity.

### 7.5 `pool` — Declarative Object Pools

```aether
# Declare a pool of fixed-size objects
pool UsbDescriptor of size 64, count 128, alignment 16

# Using the pool
func alloc_descriptor() -> ref UsbDescriptor {
    return UsbDescriptor()  # allocated from pool
}  # returned to pool on drop

# Pool statistics at runtime
print("Free: {UsbDescriptor.free_count()}")
print("Used: {UsbDescriptor.used_count()}")
```

The compiler generates:
- A fixed-size array of `count` × `size` bytes in BSS
- A free-list or bitmap allocator
- O(1) allocation and deallocation
- No fragmentation
- Deterministic runtime

### 7.6 The `drop` / Destructor Protocol

Any type can define a destructor:

```aether
struct Buffer {
    data ptr u8
    size u64
    
    func drop(self ref Buffer) {
        # compiler guarantees this is called automatically
        free(self.data)
    }
}
```

The compiler inserts `drop` calls:
- At every scope exit (normal, early return, exception unwind)
- In the reverse order of construction
- For fields within structs/classes (field destruction order = reverse declaration order)
- For elements in arrays

---

## 8. Reference System

### 8.1 Overview

| Reference Kind | Syntax | Ownership | Cost |
|---------------|--------|-----------|------|
| Borrowed ref | `ref T` | None | Zero (just a pointer at runtime) |
| Unique owner | `owned T` | Single | Move semantics; free on drop |
| Shared count | `rc T` | Multiple | Atomic ref-count increment/decrement |
| Raw pointer | `ptr T` | None (unsafe) | Zero (requires `unsafe` block) |

### 8.2 Borrowed References (`ref`)

```aether
func read_data(buf ref Buffer) {
    print(buf.size())  # borrowing, not owning
}  # nothing freed — borrow expires

let buf = Buffer(256)
read_data(ref buf)     # explicit borrow
read_data(buf)         # auto-borrowed (inferred)
```

### 8.3 Move Semantics (`owned`)

```aether
func take_ownership(buf owned Buffer) {
    process(buf)
}  # buf freed here

let buf = Buffer(256)
take_ownership(buf)    # moved — 'buf' is no longer valid here
# buf.read()          # ERROR: use-after-move
```

### 8.4 Shared Ownership (`rc`)

```aether
let shared = heap rc Data()
let copy = shared.clone()
process(shared)        # ref count = 2
process(copy)          # ref count = 2 (same allocation)
# ref count drops to 1, then 0, Data freed
```

### 8.5 Raw Pointers (`ptr`, unsafe)

```aether
func dma_transfer(addr ptr u64, value u64) {
    unsafe {
        *addr = value
    }
}

# Casting
let addr: ptr u64 = unsafe { as_ptr(&buffer) }
```

---

## 9. Structs

### 9.1 Struct Definition

```aether
struct Point {
    x int
    y int
}

struct Person {
    name string
    age int
    active bool
}
```

### 9.2 Struct Construction

```aether
let p = Point(3, 4)               # positional
let p2 = Point { x: 3, y: 4 }     # named fields
let p3 = Point(y: 4)              # defaults: x = 0
let p4 = Point { .x }             # shorthand: use local variable x

# Struct update syntax (copy with changes)
let origin = Point(0, 0)
let moved = Point { .x = 5; ..origin }  # x=5, y=0
```

### 9.3 Struct Methods

```aether
struct Point {
    x int
    y int
    
    func distance(self ref Point, other ref Point) float {
        let dx = self.x - other.x
        let dy = self.y - other.y
        return sqrt(f32(dx * dx + dy * dy))
    }
    
    func to_tuple(self ref Point) -> (self.x, self.y)
    
    static func origin() Point -> Point(0, 0)
}

let p = Point(3, 4)
print(p.distance(Point.origin()))
```

### 9.4 Struct Layout Control

```aether
# Default: packed (no padding between fields)
struct SerialPort {
    @offset(0x00) data byte
    @offset(0x01) interrupt byte
    @offset(0x02) fifo_control byte
    @offset(0x03) line_control byte
}

# C-compatible layout (with padding)
@repr(C)
struct ElfHeader {
    magic [byte; 4]
    class byte
    data byte
    version byte
    osabi byte
    ...
}
```

---

## 10. Enums and Pattern Matching

### 10.1 Simple Enums

```aether
enum Color {
    Red
    Green
    Blue
    Rgb(u8, u8, u8)
}

let c = Color::Red
match c {
    case Red => print("red\n")
    case Green => print("green\n")
    case Rgb(r, g, b) => print("#{r:02X}{g:02X}{b:02X}\n")
}
```

### 10.2 Enums with Payloads (Algebraic Data Types)

```aether
enum Optional(T) {
    Some(T)
    None
}

enum Result<T, E> {
    Ok(T)
    Err(E)
}

# Usage
let r: Result<int, string> = Result::Ok(42)
match r {
    case Ok(val) => print("got {val}")
    case Err(msg) => print("error: {msg}")
}
```

### 10.3 Recursive Enums

```aether
enum Tree<T> {
    Leaf(T)
    Node(ref Tree<T>, ref Tree<T>)
}
```

### 10.4 Pattern Matching Power

```aether
# Range patterns
match x {
    case 0..10 => "small"
    case 10..100 => "medium"
    case _ => "large"
}

# Or patterns
match x {
    case 0 | 1 | 2 => "few"
    case _ => "many"
}

# Destructuring nested
match complex {
    case Ok(Point(x, y)) if x > 0 => "positive x"
    case _ => "other"
}

# Binding
match data {
    case [first, ..rest] => print("first: {first}, rest: {rest}")
    case [] => print("empty")
}
```

---

## 11. Classes

### 11.1 Class Definition

Classes are like structs with automatic constructor/destructor management, private fields, and inheritance. The key difference from structs: when a class-typed variable goes out of scope, the compiler **automatically calls `drop()`** — you never need to manually free a class instance. The `init()` constructor is called when the variable is declared.

```aether
class File {
    # Private fields (default)
    fd int
    path string
    
    # Constructor — called automatically when a File is created
    func init(self ref File, path string) throws {
        self.fd = sys_open(path)
        if self.fd < 0 {
            throw IOError("Could not open {path}")
        }
        self.path = path
    }
    
    # Destructor — called automatically when the File goes out of scope
    func drop(self ref File) {
        sys_close(self.fd)
    }
    
    # Public method
    pub func read(self ref File, buf ref [u8]) int {
        return sys_read(self.fd, buf)
    }
    
    pub func write(self ref File, data [u8]) int {
        return sys_write(self.fd, data)
    }
    
    # Property (accessor syntax)
    pub prop path(self) string -> self.path
    
    # Static method
    pub static func temp() File {
        return File("/tmp/temp.dat")
    }
}
```

### 11.2 Using Classes

```aether
func read_config() throws string {
    let f = File("/etc/aether.cfg")  # calls init()
    # compiler inserts: if anything throws, call f.drop()
    let content = f.read_all()
    # compiler inserts: f.drop() on all exit paths
    return content
}

func read_config_autoclose() {
    # Using defer for explicit control
    let f = File("/etc/aether.cfg")
    defer { f.drop() }        # explicit: drop when scope exits
    let content = f.read_all()
}
```

### 11.3 Automatic Destructor Insertion

This is the key feature that separates classes from structs at runtime. When you declare a class-typed variable, the compiler automatically:

```aether
# When you write this:
class File {
    fd int
    path string
    func init(self, path: string) { ... }
    func drop(self) { sys_close(self.fd) }
}

func process_files() throws {
    let a = File("a.txt")       # init() called here
    let b = File("b.txt")       # init() called here

    if some_condition {
        return  # compiler inserts: b.drop(), then a.drop()
    }

    throw Error()  # compiler inserts: b.drop(), then a.drop()

    # normal exit: compiler inserts: b.drop(), then a.drop()
    # destructors run in REVERSE declaration order
}
```

The compiler inserts destructor calls at every scope exit path:

1. **Normal scope exit**: End of block or function
2. **Early return**: Any `return` statement
3. **Exception**: Any `throw` that unwinds past the scope
4. **Loop exit**: `break` or `continue` leaving a block containing class instances
5. **Move**: When ownership transfers, the source is invalidated but not destructed
6. **Field**: When a struct/class is destructed, all fields are destructed in reverse order

### 11.4 Inheritance

```aether
class Base {
    pub func speak(self) string -> "base"
}

class Derived : Base {
    pub func speak(self) string -> "derived"
}

# Virtual dispatch (opt-in)
virtual class Renderer {
    pub func draw(self ref dyn Renderer)
}
```

---

## 12. Traits

Traits define shared behavior across types. They're similar to Rust traits or Go interfaces — a contract that types can implement. The compiler uses **static dispatch by default** (zero-cost — no vtable lookups) unless you explicitly opt into dynamic dispatch with `dyn`.

### 12.1 Defining Traits

A trait declares method signatures without implementations. The `Self` type refers to whichever type implements the trait.

```aether
trait Hashable {
    func hash(self ref Self) u64
    func eq(self ref Self, other ref Self) bool
}

trait Serializable {
    func serialize(self ref Self) [u8]
    func deserialize(data [u8]) throws Self
}

trait Default {
    func default() Self  # static — no self parameter
}
```

### 12.2 Implementing Traits

```aether
impl Hashable for Point {
    func hash(self ref Point) u64 {
        return u64(self.x) ^ (u64(self.y) << 32)
    }
    
    func eq(self ref Point, other ref Point) bool {
        return self.x == other.x and self.y == other.y
    }
}
```

### 12.3 Static Dispatch (Default)

```aether
# Static dispatch — compiler knows the concrete type
func print_hash<T where T: Hashable>(value T) {
    print(value.hash())
}

# Called with
print_hash(Point(3, 4))
# Compiler generates: call directly to Point::hash
```

### 12.4 Dynamic Dispatch

```aether
# Dynamic dispatch — vtable
func print_hash_dyn(value ref dyn Hashable) {
    print(value.hash())  # indirect call through vtable
}
```

### 12.5 Trait Objects

```aether
# A collection of different types implementing the same trait
let items: [dyn Serializable] = [
    Point(3, 4) as dyn Serializable,
    User("alice") as dyn Serializable,
]

for item in items {
    let data = item.serialize()  # dynamic dispatch
}
```

### 12.6 Trait Bounds

```aether
func sorted(T where T: Hashable + Eq)(items [T]) [T] {
    # T must implement both Hashable and Eq
}
```

---

## 13. Generics

Generics let you write functions, structs, and classes that work with any type. All generics are **monomorphized** — the compiler generates a separate copy of the code for each concrete type used. This means zero runtime overhead, at the cost of larger binaries.

### 13.1 Generic Functions

Type parameters are declared with angle brackets `T, U` after the function name and before the regular parameter list:

```aether
# identity works for any type T
func identity<T>(value: T): T {
    return value
}

# Usage — type is inferred from the argument:
let x = identity(42)   # T = int
let s = identity("hi") # T = string

# Generic function with multiple type params
func min<T, U>(a: T, b: U) { ... }
```

### 13.2 Generic Structs and Classes

```aether
class Stack<T> {
    data [T]
    count int
    capacity int
    
    pub func init(self, capacity int = 16) {
        self.capacity = capacity
        self.data = heap [T; capacity]
    }
    
    pub func push(self ref Stack<T>, item T) {
        if self.count >= self.capacity {
            self.grow()
        }
        self.data[self.count] = item
        self.count += 1
    }
    
    pub func pop(self ref Stack<T>) T? {
        if self.count == 0 { return none }
        self.count -= 1
        return self.data[self.count]
    }
}

let stack = Stack<int>(16)
stack.push(42)
let val = stack.pop()  # Optional::Some(42)
```

### 13.3 Monomorphization

All generics are monomorphized — the compiler generates a separate copy of the function/class for each concrete type used. Zero runtime overhead.

```aether
# These call two different compiled versions:
let max_i = max(10, 20)       # max(int, int) — generated
let max_f = max(3.14, 2.71)   # max(float, float) — generated
```

### 13.4 Where Clauses

```aether
func serialize_all<T where T: Serializable>(items [T]) [byte] {
    let mut result = [byte]()
    for item in items {
        result.append(item.serialize())
    }
    return result
}
```

---

## 14. Exception Handling

### 14.1 Deterministic Exceptions

Aether's exceptions are deterministic — no unwinding tables, no personality routines. The compiler encodes exceptions as tagged union returns.

```aether
func divide(a int, b int) throws int {
    if b == 0 {
        throw DivisionByZero()
    }
    return a / b
}
```

The compiler transforms this to something equivalent to:

```aether
func __divide(a int, b int) (int, Error?) {
    if b == 0 {
        return (0, some Error::DivisionByZero)
    }
    return (a / b, none)
}
```

### 14.2 Try / Catch

```aether
func compute() {
    try {
        let result = divide(10, 2)  # 5
        print(result)
        
        let bad = divide(10, 0)     # throws DivisionByZero
        # print("never reached")    # skipped
    } catch DivisionByZero {
        print("Can't divide by zero!\n")
    } catch e IOError {
        print("IO error: {e.message()}\n")
    }
}
```

### 14.3 Custom Error Types

```aether
enum NetworkError {
    Timeout
    ConnectionRefused
    DnsFailure(string)
    Unknown(u64)
}

func connect(host string, port u16) throws NetworkError {
    if port == 0 {
        throw NetworkError::Timeout
    }
}

# Pattern match on errors
try {
    let conn = connect("localhost", 8080)
} catch NetworkError::Timeout {
    retry()
} catch NetworkError::ConnectionRefused {
    print("server down\n")
} catch e NetworkError::DnsFailure(host) {
    print("DNS failed for {host}: {e}\n")
}
```

### 14.4 Try Expression

```aether
let result = try? divide(10, 0) or 0  # if throws, use 0

# Propogate errors with postfix ?
let value = divide(10, 2)?  # if throws, function returns early with the error
```

---

## 15. NASM Inline Assembly

### 15.1 Basic Inline

```aether
func outb(port u16, value byte) {
    asm {
        mov dx, port
        mov al, value
        out dx, al
    }
}
```

### 15.2 Input/Output Binding

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

### 15.3 Full NASM Directives

```aether
func setup_gdt() {
    asm {
        ; Full NASM syntax supported
        ; Use any directive, pseudo-instruction, or macro
        
        gdt_start:
            dq 0x0000000000000000  ; null descriptor
        gdt_code:
            dw 0xFFFF              ; limit low
            dw 0x0000              ; base low
            db 0x00                ; base middle
            db 10011010b           ; access
            db 11001111b           ; granularity
            db 0x00                ; base high
        gdt_data:
            dw 0xFFFF
            dw 0x0000
            db 0x00
            db 10010010b
            db 11001111b
            db 0x00
        gdt_end:
        
        gdt_ptr:
            dw gdt_end - gdt_start - 1
            dq gdt_start
        
        lgdt [gdt_ptr]
        mov ax, 0x10
        mov ds, ax
        mov es, ax
        mov ss, ax
        jmp 0x08:flush_cs
    flush_cs:
    }
}
```

### 15.4 Assembly-Only Functions

```aether
asm func _start() {
    ; Kernel entry point
    mov esp, stack_top
    extern kernel_main
    call kernel_main
    .halt:
        hlt
        jmp .halt
}

asm func _idt_handler() {
    ; Interrupt handler with error code
    push rax
    push rcx
    push rdx
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    
    mov rdi, cr2
    extern handle_page_fault
    call handle_page_fault
    
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rax
    iretq
}
```

### 15.5 Mixing Aether and Assembly

```aether
func setup_interrupts() {
    let idt_ptr IDTDescriptor
    
    asm {
        ; Use variables from Aether scope
        lidt [idt_ptr]
        sti
    }
    
    print("Interrupts enabled\n")
}

func read_cr0() u64 {
    let value u64
    asm -> (value) {
        mov [value], cr0
    }
    return value
}

func write_cr3(value u64) {
    asm {
        mov cr3, value
    }
}
```

---

## 16. Aether OS Integration

### 16.1 Syscall Functions (`sys func`)

```aether
# These compile to direct calls through the 0x5000 syscall page

sys func putc(c byte) at(0) {
    # offset 0 in syscall table: 0x5000
}

sys func puts(s string) at(1) {
    # offset 1: 0x5008
}

sys func open(path string) int at(2)
sys func read(fd int, buf ref [u8]) int at(3)
sys func write(fd int, data [u8]) int at(4)
sys func getcwd() int at(5)
sys func chdir(ino int) at(6)
sys func exit() at(7)
sys func booleval(v u64) u64 at(8)
```

The compiler emits:
```nasm
mov rdi, <arg1>
mov rsi, <arg2>
; etc.
mov rax, <index * 8>    ; 0, 8, 16, ...
add rax, 0x5000
call [rax]               ; indirect call through syscall page
```

### 16.2 Kernel Modules (`module`)

```aether
module serial_driver {
    # Module initialization — called when .ko is loaded
    @export
    func mod_init() int {
        reg_cmd("serial", cmd_serial)
        reg_hook(1, boolhook_handler)
        return 0  # success
    }
    
    # Module cleanup — called when .ko is unloaded
    @export
    func mod_fini() {
        unreg_cmd("serial")
        unreg_hook(1)
    }
    
    # Internal helper
    func cmd_serial(args [string]) int {
        print("serial: {args}\n")
        return 0
    }
    
    # Hook handler for quantum boolean override
    func boolhook_handler(value u64) u64 {
        # quantum logic
        return value
    }
}
```

The module keyword tells the compiler:
- Generate ELF with `mod_init` and `mod_fini` as exported symbols
- Reserve space for the module table entry
- Generate ELF `.ko` file, not a standalone `.elf`

### 16.3 Binary Entry Points

```aether
# /bin/ executable — loaded at 0x2000000
@entry(0x2000000)
func main(args [][]byte) int {
    puts("Hello from Aether binary!\n")
    return 0
}
```

### 16.4 Boot Stage Layout

```aether
# Stage 1: 512-byte MBR boot sector
@layout(start=0x7C00, max=512, file="stage1.bin")
func stage1_mbr() {
    asm { ... boot code ... }
}

# Stage 2: ATA PIO loader
@layout(start=0x7E00, file="stage2.bin")
func stage2_loader() {
    asm { ... load kernel from disk ... }
}

# Kernel: loaded at 0x1000000
@layout(start=0x1000000, file="kernel.elf")
func kernel_main() {
    init_serial()
    init_memory()
    mount_fs()
    load_modules()
    shell_loop()
}
```

### 16.5 Memory Layout Verification

```aether
@kernel_layout
func verify_layout() {
    # Compiler knows the Aether memory map
    # and verifies no overlap:
    #   0x1000: page allocator bitmap
    #   0x4000: module registry (5 pointers)
    #   0x5000: syscall page (9 pointers)
    #   0x6000-0x1000: Page tables / GDT
    #   0x7C00: Stage1 MBR
    #   0x7E00: Stage2 loader
    #   0x1000000: kernel base
    #   0x2000000: binary exec space
    #   0x2100000: module slots
    
    let bitmap = reserved(0x1000, 0x1000)      # compiler verifies
    let registry = reserved(0x4000, 0x1000)
    let syscall = reserved(0x5000, 0x1000)
}
```

### 16.6 ABI Compliance

```aether
@module_abi(version = 1)
func mod_init() int {
    # Compiler checks:
    # - Return type must be int (not void, not u64)
    # - No parameters beyond what ABI allows
    # - Must be exported (visible in symbol table)
    # - Module slot constraints (max 64KB code)
}
```

---

## 17. Compile-Time Execution

### 17.1 `#run` Blocks

Code inside `#run { }` executes at compile time:

```aether
const TABLE_SIZE = 256

#run {
    # This runs during compilation, not at runtime
    let mut values = [int; TABLE_SIZE]
    for i in 0..TABLE_SIZE {
        values[i] = compute_sine(i, TABLE_SIZE)
    }
    emit_sine_table(values)
}

# The generated table is embedded in the binary
const SINE_TABLE = #include_sine_table
```

### 17.2 Compile-Time Constants

```aether
const MAX_PORTS = 64
const PORT_MASK = (1 << MAX_PORTS) - 1   # computed at compile time

func initialize_ports() {
    # MAX_PORTS is a compile-time constant — no load needed
    for i in 0..MAX_PORTS {
        setup_port(i)
    }
}
```

### 17.3 Compile-Time Reflection

```aether
#run {
    let fields = reflect(T).fields()
    for field in fields {
        print("  {field.name}: {field.type}\n")
    }
}
```

### 17.4 Compile-Time Assertions

```aether
#run {
    assert(sizeof(int) == 4)
    assert(sizeof(Pointer) == 8)
    assert(MAX_PORTS > 0)
    assert(TABLE_SIZE % 32 == 0)  # must be aligned
}
```

---

## 18. Contract Programming

### 18.1 Preconditions and Postconditions

```aether
func withdraw(account ref Account, amount u64) u64
    pre(account.balance >= amount, "insufficient funds")
    post(account.balance == old(account.balance) - amount)
    post(result == amount)
{
    account.balance -= amount
    return amount
}
```

### 18.2 Invariants

```aether
class SafeBuffer {
    data ptr u8
    size u64
    position u64
    
    # Class invariant — checked at scope entry and exit
    @invariant(self.position <= self.size)
    
    pub func write(self ref SafeBuffer, data [byte]) {
        # Invariant checked: position <= size before
        self.position += data.len()
        # Invariant checked: position <= size after
    }
}
```

### 18.3 Contract Behavior

In **debug builds**:
- Preconditions are checked at runtime
- Postconditions verify the return value
- Invariants are checked at every scope boundary
- Clear error messages: `Contract violation: insufficient funds`

In **release builds**:
- Contracts are eliminated (zero runtime cost)
- The compiler uses contracts as optimization hints
- Postconditions enable the compiler to elide redundant checks

---

## 19. Declarative Resources

These features go beyond traditional 4GL scope — they let you **declare what you need** and the compiler generates the implementation.

### 19.1 Protocol Declaration

```aether
protocol SerialPort {
    # Protocol-level constants
    base = 0x3F8     # COM1
    speed = 115200
    data_bits = 8
    stop_bits = 1
    parity = none
    
    # Protocol operations — compiler generates register sequences
    func init() {
        asm {
            mov dx, base + 1     ; interrupt enable
            mov al, 0x00
            out dx, al
            
            mov dx, base + 3     ; line control
            mov al, 0x80         ; DLAB enable
            out dx, al
            
            mov dx, base          ; divisor low (115200 → 1)
            mov al, 1
            out dx, al
            
            mov dx, base + 1     ; divisor high
            mov al, 0
            out dx, al
            
            mov dx, base + 3
            mov al, 0x03         ; 8N1
            out dx, al
        }
    }
    
    func putc(c byte) {
        asm {
            mov dx, base + 5     ; line status
        wait:
            in al, dx
            test al, 0x20        ; transmitter holding register empty?
            jz wait
            
            mov dx, base
            mov al, c
            out dx, al
        }
    }
}

# Usage — compiler resolves to protocol-specific operations
let com1 = SerialPort()
com1.putc('A')
```

### 19.2 Query-Style Pipelines

```aether
# Declarative data processing — compiles to fused loops

let result = data
    .filter(|x| x > 0)
    .map(|x| x * 2)
    .sort()
    .collect()

# With compile-time knowledge:
let configs = filesystem
    .scan("/etc/")
    .filter(|f| f.extension == "cfg")
    .map(|f| load_config(f))
    .collect()
```

The compiler fuses these operations into a single loop with no intermediate allocations.

### 19.3 Goal-Oriented I/O

```aether
# Describe *what*, not *how*
let config = from "/etc/aether.cfg" read Config

# The compiler generates the optimal implementation:
# - Boot time (no AetherFS): raw ATA PIO read from fixed sectors
# - Kernel mode (AetherFS loaded): fs_open + fs_read syscalls
# - Userspace: syscall-page open/read

# Simple resource declaration
let bitmap = memory(64KB, alignment=4096, type=pageable)
let dma_buf = memory(256, alignment=16, type=dma)
```

---

## 20. Build System

### 20.1 Semicolons Are Not Required

Aether uses **newlines and indentation** as statement terminators, not semicolons. Semicolons (`;`) are optional and may be used between statements on the same line for readability but are never required.

```aether
func main() {
    let x = 42          # newline ends the statement — no semicolon needed
    let y = 10; let z = 20  # semicolon separates two statements on one line
    return x + y + z    # no semicolon
}
```

Inside braces `{ }`, indentation tokens are ignored — the braces themselves delimit the block. This means you can write:

```aether
# Braces override indentation — all of these are equivalent:
func a() {
    return 1
}

func b() { return 1 }           # single statement in one line

func c() { let x = 1; let y = 2; return x + y }  # semicolons separate inline stmts
```

### 20.2 Project Structure

```
my-project/
  src/
    main.ae                # entry point
    lib/
      utils.ae             # library modules
    asm/
      boot.ae              # assembly-heavy files
  tests/
    test_utils.ae          # test files
  target/
    debug/
      kernel.elf
    release/
      kernel.elf
```

### 20.3 `aether.toml`

```toml
[package]
name = "aether-os-kernel"
version = "0.1.0"
authors = ["Aether Team"]

[build]
target = "x86_64-freestanding"
output = "aether-kernel.elf"
entry = "src/main.ae"

[build.options]
optimization = "size"      # size, speed, debug
linker-script = "tools/kernel.ld"
strip = true

[dependencies]
std = { path = "/lib/aether/std" }

[[stages]]
file = "stage1.bin"
layout-start = "0x7C00"
layout-max = 512
source = "src/asm/stage1.ae"

[[stages]]
file = "stage2.bin"
layout-start = "0x7E00"
source = "src/asm/stage2.ae"
```

### 20.4 CLI Commands

```aether
aether new my-kernel       # Create new project
  ├── aether.toml
  ├── src/
  │   ├── main.ae
  │   └── lib/
  └── tests/

aether build               # Compile (debug by default)
aether build --release     # Compile with optimizations
aether run                 # Build + run in QEMU
aether test                # Run test suite
aether fmt                 # Format source code
aether asm main.ae         # Show generated NASM assembly
aether inspect kernel.elf  # Inspect ELF binary
aether clean               # Remove build artifacts
```

---

## 21. Testing

### 21.1 Test Functions

```aether
@Test
func test_addition() {
    assert(add(2, 3) == 5)
    assert(add(-1, 1) == 0)
}

@Test
func test_exceptions() {
    try {
        let _ = divide(10, 0)
        assert(false)  # should have thrown
    } catch DivisionByZero {
        # expected — test passes
    }
}
```

### 21.2 Test Groups

```aether
test group "memory" {
    @Test func test_alloc() {
        let buf = heap Buffer(1024)
        assert(buf.size() == 1024)
    }
    
    @Test func test_region() {
        region("test") {
            let a = allocate()
            let b = allocate()
            assert(a != none and b != none)
        }
    }
}
```

### 21.3 Running Tests

```
aether test                    # Run all tests
aether test --filter memory    # Run only memory tests
aether test --verbose          # Verbose output
```

Tests compile to standalone ELF binaries that report results through serial output.

### 21.4 Test Infrastructure

```aether
# Built-in assertion functions
assert(condition)
assert_eq(a, b)
assert_ne(a, b)
assert_throws(func, ErrorType)

# Test lifecycle
@Before func setup() { ... }
@After func teardown() { ... }
@BeforeClass func init_suite() { ... }
@AfterClass func cleanup_suite() { ... }
```

---

## 22. Standard Library

### 22.1 `std.io`

```aether
import std.io

std.io.print("Hello, Aether!\n")
std.io.println("Hello")  # auto-adds newline
std.io.format("value: {x}, name: {name}")  # string formatting
std.io.read_line(buf)    # read a line from serial input
```

### 22.2 `std.mem`

```aether
import std.mem

let buf = std.mem.alloc(1024)
std.mem.copy(dest, src, 512)
std.mem.zero(buf)
std.mem.free(buf)

# Sized allocators
let pool = std.mem.Pool(64, 128)       # pool of 128 blocks of 64 bytes
let arena = std.mem.Arena(65536)       # arena allocator, 64KB
```

### 22.3 `std.str`

```aether
import std.str

let s = std.str.String("hello")
let s2 = s.concat(" world")
let parts = s2.split(" ")
let trimmed = s2.trim()
let upper = s2.upper()
let has_h = s2.contains("h")
```

### 22.4 `std.collections`

```aether
import std.collections

let arr = std.collections.Array(int)()
arr.push(1)
arr.push(2)
let x = arr.pop()

let map = std.collections.HashMap(string, int)()
map["key"] = 42
let val = map["key"] or 0

let set = std.collections.Set(int)()
set.insert(1)
assert(set.contains(1))
```

### 22.5 `std.serial`

```aether
import std.serial

std.serial.init(115200)
std.serial.putc('A')
std.serial.puts("hello\n")
let c = std.serial.getc()   # blocking read
```

### 22.6 `std.elf`

```aether
import std.elf

let reader = std.elf.Reader(data)
let entry = reader.entry_point()
for section in reader.sections() {
    print("{section.name} at {section.offset}\n")
}
```

### 22.7 `std.test`

```aether
import std.test as test

test.assert(condition)
test.assert_eq(a, b)
test.assert_ne(a, b)
test.benchmark(func, iterations=1000)
```

### 22.8 `std.math`

```aether
import std.math

let x = std.math.sqrt(144.0)     # 12.0
let s = std.math.sin(3.14159)    # ~0.0
let c = std.math.cos(0.0)        # 1.0
let a = std.math.abs(-42)        # 42
let m = std.math.min(3, 7)       # 3
let mx = std.math.max(3, 7)      # 7
```

---

## 23. Appendix: Grammar

This is a simplified grammar. The full grammar is defined in the compiler's parser source.

### 23.1 File Structure

```
file         = { decl }
decl         = func_decl | class_decl | struct_decl | enum_decl
             | trait_decl | impl_decl | const_decl | type_alias
             | module_decl | protocol_decl | pool_decl
             | import_decl | # run_block
```

### 23.2 Functions

```
func_decl    = "func" ident [ generic_params ] param_list [ ret_type ] ( block | "->" expr )
param_list   = "(" [ param { "," param } ] ")"
param        = ident [ "..." ] type_annotation
ret_type     = type_annotation
type_annotation = ":" type_expr
```

### 23.3 Types

```
type_expr    = primitive | ident | array_type | slice_type
             | tuple_type | optional_type | ref_type
             | fn_type | ptr_type
             | generic_application | where_clause

primitive    = "u8" | "u16" | "u32" | "u64"
             | "i8" | "i16" | "i32" | "i64"
             | "f32" | "f64" | "bool" | "byte" | "void"
             | "string"
array_type   = "[" type_expr ";" expr "]"
slice_type   = "[" type_expr "]"
tuple_type   = "(" type_expr { "," type_expr } ")"
optional_type = type_expr "?"
ref_type     = ("ref" | "owned" | "rc") type_expr
ptr_type     = "ptr" type_expr
fn_type      = "func" param_list ret_type
```

### 23.4 Expressions

```
expr         = literal | ident | unary_op expr
             | expr binary_op expr
             | expr "(" args ")"        # function call
             | expr "." ident           # member access
             | expr "[" expr "]"        # indexing
             | expr "[" expr ".." expr "]"  # slice
             | "if" expr block [ "elif" expr block ] [ "else" block ]
             | "match" expr "{" match_arms "}"
             | "try" expr
             | "throw" expr
             | block
             | asm_block
             | "|" params "|" expr      # lambda

literal      = int_literal | float_literal | string_literal
             | char_literal | bool_literal | "none"
```

### 23.5 Statements

```
stmt         = let_decl | assignment | expr
             | if_stmt | while_stmt | for_stmt
             | return_stmt | break_stmt | continue_stmt
             | defer_stmt | region_block | contract_stmt

let_decl     = "let" ["mut"] ident [type_annotation] "=" expr
assignment   = lvalue "=" expr
return_stmt  = "return" [expr]
```

---

*End of Aether Language Specification v0.1*

*"Write what you mean. The compiler handles the rest."*
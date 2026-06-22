# Aether Language Specification v1.0

> **Aether** is a compiled, object-oriented 4GL for operating system development. It compiles to NASM assembly, assembles with NASM, and links with LD — producing bare-metal executables with no runtime, no interpreter, and no garbage collector.

---

## Table of Contents

1. [Lexical Structure](#1-lexical-structure)
2. [Types](#2-types)
3. [Variables and Declarations](#3-variables-and-declarations)
4. [Expressions](#4-expressions)
5. [Statements](#5-statements)
6. [Functions](#6-functions)
7. [Structs](#7-structs)
8. [Enums](#8-enums)
9. [Classes](#9-classes)
10. [Traits](#10-traits)
11. [Generics](#11-generics)
12. [Memory Management](#12-memory-management)
13. [Exception Handling](#13-exception-handling)
14. [Inline Assembly](#14-inline-assembly)
15. [Compile-Time Features](#15-compile-time-features)
16. [Module System](#16-module-system)
17. [Target System](#17-target-system)
18. [Standard Library](#18-standard-library)
19. [Compiler Directives](#19-compiler-directives)
20. [Full Examples](#20-full-examples)

---

## 1. Lexical Structure

### 1.1 Character Set

Aether source files are UTF-8 encoded. Identifiers and keywords use ASCII letters, digits, and underscores.

### 1.2 Comments

```
// Line comment — extends to end of line

/* Block comment — can span
   multiple lines */
```

Block comments are nestable:

```
/* outer /* inner */ still outer */
```

### 1.3 Identifiers

```
identifier ::= (letter | '_') (letter | digit | '_')*
letter     ::= 'a'..'z' | 'A'..'Z'
digit      ::= '0'..'9'
```

Identifiers are case-sensitive. `foo`, `Foo`, and `FOO` are distinct.

### 1.4 Keywords

```
alloc     and       as        asm       bool      break
case      catch     class     comptime  const     constructor
continue  defer     destructor dyn       else      enum
ensure    false     for       func      if        impl
import    in        is        let       match     mod
mut       not       or        override  owned     pub
ref       region    require   return    struct    super
throw     throws    trait     true      try       type
unsafe    use       var       while
```

### 1.5 Operators and Delimiters

```
+    -    *    /    %    &    |    ^    ~    <<   >>
+=   -=   *=   /=   %=   &=   |=   ^=   <<=  >>=
++   --
==   !=   <    >    <=   >=   <=>
&&   ||   !
.    ..   ..=  ->   =>
[    ]    (    )    {    }
,    ;    :    ::   #
@    $    ?    ??   ?.   !
```

### 1.6 Literals

**Integer literals:**

```
42          // decimal
0xFF        // hexadecimal
0o77        // octal
0b1010      // binary
1_000_000   // underscores for readability
```

**Float literals:**

```
3.14
1.0e-5
```

**String literals:**

```
"hello, world"
"line 1\nline 2"
"tab\there"
"hex \x41 byte"
```

**Character literals:**

```
'A'
'\n'
'\x41'
```

**Boolean literals:**

```
true
false
```

---

## 2. Types

### 2.1 Primitive Types

| Type | Size | Alignment | Description |
|------|------|-----------|-------------|
| `u8` | 1 | 1 | Unsigned 8-bit |
| `u16` | 2 | 2 | Unsigned 16-bit |
| `u32` | 4 | 4 | Unsigned 32-bit |
| `u64` | 8 | 8 | Unsigned 64-bit |
| `i8` | 1 | 1 | Signed 8-bit |
| `i16` | 2 | 2 | Signed 16-bit |
| `i32` | 4 | 4 | Signed 32-bit |
| `i64` | 8 | 8 | Signed 64-bit |
| `f32` | 4 | 4 | IEEE 754 single |
| `f64` | 8 | 8 | IEEE 754 double |
| `bool` | 1 | 1 | `true` or `false` |
| `char` | 1 | 1 | ASCII character |
| `str` | 16 | 8 | String view `{ptr, len}` |
| `void` | 0 | 1 | No value |
| `typeid` | 8 | 8 | Opaque type identifier |

### 2.2 Composite Types

**Arrays:**

```
let arr: [10]u32          // fixed-size array of 10 u32s
let matrix: [3][3]f32     // 3x3 matrix
```

**Slices:**

```
let slice: []u32          // dynamic slice {ptr, len}
```

**References:**

```
let r: ref u32            // immutable reference
let mr: mut ref u32       // mutable reference
```

**Pointers:**

```
let p: *u32               // raw pointer (nullable, unchecked)
```

**Optionals:**

```
let opt: ?u32             // optional u32 {has_value, value}
```

### 2.3 Named Types

```
struct Point { x: i32, y: i32 }
enum Result { Ok(u32), Err(str) }
class Buffer { data: [256]u8 }
```

### 2.4 Type Aliases

```
type Byte = u8
type Coord = i32
type Handler = func(u32) -> bool
```

### 2.5 Type Inference

The compiler infers types when possible:

```
let x = 42              // x: i32 (default integer)
let y = 3.14            // y: f64 (default float)
let z: u64 = 42         // explicit type
let name = "hello"      // name: str
```

---

## 3. Variables and Declarations

### 3.1 `let` — Immutable Binding

```
let x = 42
let name: str = "Aether"
```

### 3.2 `var` — Mutable Binding

```
var count = 0
count += 1
```

### 3.3 `const` — Compile-Time Constant

```
const MAX_SIZE = 4096
const TABLE: [4]u32 = [1, 2, 3, 4]
```

Constants are evaluated at compile time and inlined at all use sites.

### 3.4 Scope

Variables are block-scoped. A new scope is created by `{}`:

```
{
    let x = 1
    {
        let x = 2    // shadows outer x
        print("{x}\n")  // 2
    }
    print("{x}\n")      // 1
}
```

### 3.5 The Blank Identifier

`_` discards values:

```
let _, err = risky_function()
if err != 0 { panic("failed") }
```

---

## 4. Expressions

### 4.1 Literal Expressions

```
42
3.14
"hello"
true
'A'
```

### 4.2 Arithmetic

```
a + b    // add (or string concat)
a - b    // subtract
a * b    // multiply
a / b    // divide
a % b    // modulo
```

### 4.3 Comparison

```
a == b    // equal
a != b    // not equal
a < b     // less than
a > b     // greater than
a <= b    // less or equal
a >= b    // greater or equal
```

### 4.4 Logical

```
a and b    // logical AND
a or b     // logical OR
not a      // logical NOT
```

### 4.5 Bitwise

```
a & b      // bitwise AND
a | b      // bitwise OR
a ^ b      // bitwise XOR
~a         // bitwise NOT
a << b     // left shift
a >> b     // right shift
```

### 4.6 Assignment

```
a = b
a += b
a -= b
a *= b
a /= b
a %= b
a &= b
a |= b
a ^= b
a <<= b
a >>= b
```

### 4.7 Increment/Decrement

```
a++    // post-increment
a--    // post-decrement
```

### 4.8 String Concatenation

The `+` operator performs string concatenation when either operand is a string:

```
let greeting = "Hello, " + "world!"    // "Hello, world!"
let answer = "The answer is " + 42     // "The answer is 42"
let mixed = 7 * 6 + " is 42"           // "42 is 42"
```

### 4.9 String Interpolation

```
let name = "Aether"
let msg = "Hello, {name}!"            // "Hello, Aether!"
let calc = "6 * 7 = {6 * 7}"          // "6 * 7 = 42"
```

Any expression that can be converted to a string is valid inside `{}`. Numbers, booleans, and types with a `format` method are automatically convertible.

### 4.10 Indexing

```
arr[0]          // array/slice access
matrix[i][j]    // multi-dimensional
str[3]          // character access (returns char)
```

### 4.11 Field Access

```
point.x
person.name
buffer.data[0]
```

### 4.12 Method Call

```
obj.method(args)
```

### 4.13 Function Call

```
func_name(arg1, arg2)
```

### 4.14 Reference and Dereference

```
ref x           // take reference
*ptr            // dereference pointer
mut ref x       // take mutable reference
```

### 4.15 Optional Operators

```
opt ?? default      // nil-coalescing: unwrap or use default
opt?.field          // optional chaining: access field if present
opt!                // force-unwrap: panic if nil
```

### 4.16 Type Operators

```
value as u64        // type cast
value is MyEnum     // type check (for tagged enums)
```

### 4.17 Range Expressions

```
0..10       // 0 to 9 (exclusive end)
0..=10      // 0 to 10 (inclusive end)
```

### 4.18 Ternary

Aether has no ternary operator. Use `if` expressions:

```
let max = if a > b { a } else { b }
```

### 4.19 Precedence (highest to lowest)

| Level | Operators |
|-------|-----------|
| 1 | `()`, `[]`, `.`, `?.`, `!` |
| 2 | `ref`, `mut`, `*`, `&`, `-`, `~`, `not`, `owned` |
| 3 | `as`, `is` |
| 4 | `*`, `/`, `%` |
| 5 | `+`, `-` |
| 6 | `<<`, `>>` |
| 7 | `&` |
| 8 | `^` |
| 9 | `|` |
| 10 | `==`, `!=`, `<`, `>`, `<=`, `>=`, `<=>` |
| 11 | `and` |
| 12 | `or` |
| 13 | `..`, `..=` |
| 14 | `??` |
| 15 | `=`, `+=`, `-=`, etc. |

---

## 5. Statements

### 5.1 Expression Statement

```
42
foo()
a + b
```

Any expression followed by a newline or semicolon is a statement.

### 5.2 Assignment Statement

```
x = 42
x += 1
```

### 5.3 Variable Declaration

```
let x = 42
var count = 0
let z: u64 = 100
```

### 5.4 Block

```
{
    let x = 1
    let y = 2
    print("{x + y}\n")
}
```

### 5.5 If/Else

```
if condition {
    // true branch
}

if condition {
    // true
} else {
    // false
}

if a {
    // a
} else if b {
    // b
} else {
    // neither
}
```

### 5.6 While Loop

```
while condition {
    // loop body
}
```

### 5.7 For Loop

```
for i in 0..10 {
    print("{i}\n")
}

for i in 0..=10 {
    print("{i}\n")
}

for item in array {
    print("{item}\n")
}
```

### 5.8 Break and Continue

```
while true {
    if done { break }
    if skip { continue }
}
```

### 5.9 Return

```
func add(a: i32, b: i32) -> i32 {
    return a + b
}

func greet(name: str) {
    print("Hello, {name}!\n")
    return    // optional for void functions
}
```

### 5.10 Defer

```
func read_file(path: str) -> ?str {
    let f = open(path)
    defer close(f)       // runs when scope exits
    let data = read_all(f)
    return data
}
```

Defers run in LIFO order:

```
defer print("first\n")    // runs second
defer print("second\n")   // runs first
```

### 5.11 Match

```
match value {
    0 => print("zero\n")
    1..10 => print("small\n")
    10..100 => print("medium\n")
    _ => print("large\n")
}
```

Match with enums:

```
match result {
    Result.Ok(val) => print("got {val}\n")
    Result.Err(msg) => print("error: {msg}\n")
}
```

### 5.12 If-Let

```
if let val = optional_value {
    print("got {val}\n")
}
```

### 5.13 Try/Catch/Throw

```
func risky() throws {
    if error_condition {
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

### 5.14 Region

```
region frame {
    let obj = alloc MyObject{}
    // obj is allocated in the region
    // all region allocations freed at end
}
```

### 5.15 Unsafe Block

```
unsafe {
    let ptr = *u32(0xB8000)    // direct memory access
    ptr[0] = 0x41              // write to VGA memory
}
```

---

## 6. Functions

### 6.1 Function Declaration

```
func name(param1: Type1, param2: Type2) -> ReturnType {
    // body
}
```

### 6.2 Void Return

```
func greet(name: str) {
    print("Hello, {name}!\n")
}
```

### 6.3 Multiple Return Values

```
func divide(a: i32, b: i32) -> (i32, i32) {
    return (a / b, a % b)
}

let quot, rem = divide(10, 3)
```

### 6.4 Default Parameters

```
func increment(value: i32, by: i32 = 1) -> i32 {
    return value + by
}

let a = increment(5)        // 6
let b = increment(5, 3)     // 8
```

### 6.5 Named Parameters

```
func create_point(x: i32, y: i32) -> Point {
    return Point{x, y}
}

let p = create_point(x: 10, y: 20)
```

### 6.6 Throws Functions

```
func risky() throws {
    throw "error"
}
```

### 6.7 Public Functions

```
pub func exported_func() {
    // accessible from other modules
}
```

### 6.8 Entry Point

```
func main() {
    // program entry point
}
```

For kernel targets, the entry is `_start`:

```
@entry("_start")
func kmain() {
    // kernel entry
}
```

### 6.9 Closures

```
let add = func(a: i32, b: i32) -> i32 { return a + b }
let result = add(3, 4)    // 7
```

Closures capture variables:

```
var sum = 0
let items = [1, 2, 3, 4, 5]
for item in items {
    let add_to_sum = func() { sum += item }
    add_to_sum()
}
print("{sum}\n")    // 15
```

### 6.10 Function Types

```
type BinaryOp = func(i32, i32) -> i32

func apply(a: i32, b: i32, op: BinaryOp) -> i32 {
    return op(a, b)
}

let result = apply(3, 4, func(x, y) { return x + y })
```

---

## 7. Structs

### 7.1 Struct Declaration

```
struct Point {
    x: i32
    y: i32
}
```

### 7.2 Struct Instantiation

```
let p = Point{x: 10, y: 20}
let q = Point{10, 20}    // positional if order matches declaration
```

### 7.3 Field Access

```
print("{p.x}, {p.y}\n")
```

### 7.4 Struct Methods

```
struct Point {
    x: i32
    y: i32
}

impl Point {
    func distance(self) -> f64 {
        return sqrt((self.x * self.x + self.y * self.y) as f64)
    }

    func translate(mut ref self, dx: i32, dy: i32) {
        self.x += dx
        self.y += dy
    }
}

let p = Point{3, 4}
print("{p.distance()}\n")    // 5.0
```

### 7.5 Struct Update

```
let p = Point{1, 2}
let q = Point{...p, x: 10}    // copy p, override x
```

### 7.6 Packed Structs

```
struct #[packed] Header {
    signature: u16
    version: u8
    flags: u8
    size: u32
}
```

Packed structs have no padding between fields, matching the on-disk layout.

---

## 8. Enums

### 8.1 Simple Enum

```
enum Color {
    Red
    Green
    Blue
}

let c = Color.Red
```

### 8.2 Enum with Payloads

```
enum Result {
    Ok(u32)
    Err(str)
    None
}

let success = Result.Ok(42)
let failure = Result.Err("failed")
```

### 8.3 Enum Methods

```
impl Result {
    func is_ok(self) -> bool {
        return self is Result.Ok
    }

    func unwrap(self) -> u32 {
        if let val = self as Result.Ok {
            return val
        }
        panic("unwrap on non-Ok value")
    }
}
```

### 8.4 Enum Matching

```
match result {
    Result.Ok(val) => print("got {val}\n")
    Result.Err(msg) => print("error: {msg}\n")
    Result.None => print("nothing\n")
}
```

---

## 9. Classes

### 9.1 Class Declaration

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

    func speak(self) {
        print("{this.name} makes a sound\n")
    }
}
```

### 9.2 Instantiation

```
let animal = Animal("Generic")    // stack-allocated
let heap_animal = alloc Animal("Heap")  // heap-allocated
```

### 9.3 Inheritance

```
class Dog : Animal {
    breed: str

    constructor(name: str, breed: str) : super(name) {
        this.breed = breed
    }

    override func speak(self) {
        print("{this.name} barks!\n")
    }
}
```

### 9.4 Virtual Methods

Methods are virtual by default. The compiler generates a vtable for each class with virtual methods.

```
class Shape {
    func area(self) -> f64 { return 0.0 }
}

class Circle : Shape {
    radius: f64

    override func area(self) -> f64 {
        return 3.14159 * self.radius * self.radius
    }
}

class Square : Shape {
    side: f64

    override func area(self) -> f64 {
        return self.side * self.side
    }
}
```

### 9.5 Automatic Destruction

Class instances are destroyed automatically:

```
func example() {
    let dog = Dog("Rex", "German Shepherd")
    // ... use dog ...
}   // compiler emits: dog.destructor() automatically
```

The destructor chain runs from most-derived to base class.

### 9.6 Class Fields

```
class Buffer {
    data: [256]u8
    len: u32
    pos: u32
}
```

Fields are private by default. Use `pub` to make them accessible:

```
class Counter {
    pub count: u32
    secret: u32    // private
}
```

### 9.7 Static Methods

```
class Math {
    static func square(x: i32) -> i32 {
        return x * x
    }
}

let s = Math.square(5)    // 25
```

---

## 10. Traits

### 10.1 Trait Declaration

```
trait Drawable {
    func draw(self)
    func get_bounds(self) -> (i32, i32, i32, i32)
}
```

### 10.2 Trait Implementation

```
struct Circle {
    x: i32, y: i32, radius: u32
}

impl Drawable for Circle {
    func draw(self) {
        print("Circle at ({x}, {y}) r={radius}\n")
    }

    func get_bounds(self) -> (i32, i32, i32, i32) {
        return (x - radius, y - radius, x + radius, y + radius)
    }
}
```

### 10.3 Static Dispatch (Default)

```
func render(item: Drawable) {
    item.draw()    // monomorphized at compile time
}
```

### 10.4 Dynamic Dispatch

```
func render(item: dyn Drawable) {
    item.draw()    // runtime dispatch via vtable
}
```

### 10.5 Default Methods

```
trait Stringify {
    func to_string(self) -> str {
        return "<unknown>"
    }
}

impl Stringify for Point {
    // uses default to_string
}
```

---

## 11. Generics

### 11.1 Generic Functions

```
func max[T](a: T, b: T) -> T {
    if a > b { return a }
    return b
}

let m = max[i32](3, 7)    // explicit type parameter
let n = max(3.0, 7.0)     // type inference
```

### 11.2 Generic Structs

```
struct Pair[T, U] {
    first: T
    second: U
}

let p = Pair[i32, str]{42, "hello"}
```

### 11.3 Generic Classes

```
class Box[T] {
    value: T

    constructor(value: T) {
        this.value = value
    }

    func get(self) -> T {
        return this.value
    }
}
```

### 11.4 Generic Constraints

```
trait Comparable {
    func op_lt(self, other: Self) -> bool
}

func sort[T: Comparable](items: mut ref [T]) {
    // T must implement Comparable
}
```

### 11.5 Monomorphization

Generics are monomorphized at compile time. Each concrete instantiation generates separate, optimized code. No runtime overhead.

---

## 12. Memory Management

### 12.1 Stack Allocation (Default)

All local variables are stack-allocated by default:

```
func example() {
    let x = 42           // on stack
    let p = Point{1, 2}  // on stack
    let arr: [10]u32     // on stack
}
```

### 12.2 Heap Allocation

Use `alloc` for heap allocation:

```
let obj = alloc MyObject{}
let arr = alloc_array[100]u32
```

### 12.3 Automatic Bump Allocator

Every function gets a per-function bump arena. The compiler emits:

```
func example() {
    // Compiler emits: save current bump pointer
    // ... function body ...
    // Compiler emits: restore bump pointer (frees all temp allocations)
}
```

This makes short-lived allocations (string concatenation, temporary buffers) essentially free.

### 12.4 Region Allocation

```
region frame {
    let obj1 = alloc Object1{}
    let obj2 = alloc Object2{}
    // both freed when region ends
}
```

Regions can be nested:

```
region outer {
    let a = alloc A{}
    region inner {
        let b = alloc B{}
    }   // b freed here
}   // a freed here
```

### 12.5 Automatic Destructors

When a class instance goes out of scope, the compiler emits:

1. Call the class destructor
2. Call destructors for all member fields (recursively)
3. Free the memory (for heap allocations)

```
class File {
    handle: u64

    destructor() {
        close(this.handle)
    }
}

func process() {
    let f = File{open("data.txt")}
    // ... use f ...
}   // compiler emits: f.destructor() → close(f.handle)
```

### 12.6 Defer

`defer` schedules cleanup code at scope exit:

```
func process() {
    let buf = alloc_array[1024]u8
    defer free(buf)

    let f = open("file.txt")
    defer close(f)

    // ... use buf and f ...
    // close(f) runs first, then free(buf)
}
```

### 12.7 Ownership

```
func consume(owned value: LargeObject) {
    // takes ownership, value destroyed at end
}

func borrow(value: ref LargeObject) {
    // borrows, value not destroyed
}

func main() {
    let obj = alloc LargeObject{}
    consume(obj)    // ownership transferred
    // obj no longer valid here
}
```

### 12.8 Memory Safety Rules

- References (`ref`) must not outlive the referenced value
- Only one mutable reference (`mut ref`) to a value at a time
- Multiple immutable references (`ref`) are allowed
- Raw pointers (`*T`) have no safety guarantees
- Use `unsafe` blocks for raw pointer operations

---

## 13. Exception Handling

### 13.1 Throw

```
throw "error message"
throw 42    // integer error code
```

### 13.2 Try/Catch

```
try {
    risky_operation()
} catch {
    print("operation failed\n")
}
```

### 13.3 Throws Functions

```
func read_config(path: str) -> Config throws {
    let f = open(path)
    if !f { throw "cannot open config" }
    return parse_config(f)
}
```

### 13.4 Error Propagation

```
func outer() throws {
    inner()    // propagates automatically if inner throws
}
```

### 13.5 Nested Try/Catch

```
try {
    try {
        deep_risky()
    } catch {
        print("inner catch\n")
        throw    // re-throw
    }
} catch {
    print("outer catch\n")
}
```

### 13.6 How It Works (Host Target)

The compiler emits:

```
; Before try block:
lea rdi, [rel __aether_segfault_jmpbuf]
call aether_setJmpBuf
test eax, eax
jnz .catch_label

; Try body:
call risky_operation

; After try body:
call aether_clearJmpBuf
jmp .end

; Catch handler:
.catch_label:
call aether_clearJmpBuf
print("caught!\n")

.end:
```

### 13.7 How It Works (Kernel Target)

In kernel mode, exceptions use an IDT-based fault table:

```
; Before try block:
;   Save current fault handler state
;   Install try-range in fault table

; On throw/fault:
;   Walk cleanup table (innermost scope first)
;   Call destructors and defers
;   Jump to catch handler

; After try block:
;   Restore previous fault handler state
```

### 13.8 Cleanup Table

The compiler builds a cleanup table for each try block:

```
CleanupEntry:
  .try_start:   address of try block start
  .try_end:     address of try block end
  .catch_addr:  address of catch handler
  .cleanup:     address of cleanup code (destructors + defers)
```

On throw, the runtime walks the table from innermost to outermost, executing cleanup code for each matching entry.

---

## 14. Inline Assembly

### 14.1 Basic Inline Assembly

```
asm {
    mov rax, 1
    cpuid
}
```

### 14.2 Accessing Aether Variables

```
let result: u32
asm {
    mov eax, 1
    cpuid
    mov [result], eax
}
```

### 14.3 Register Constraints

```
let eax_val: u32, ebx_val: u32
asm {
    mov eax, 1
    cpuid
    mov [eax_val], eax
    mov [ebx_val], ebx
}
```

### 14.4 Labels in Assembly

```
asm {
    jmp @skip
    ; dead code
  @skip:
    nop
}
```

### 14.5 Raw Data Emission

```
asm {
    db 0x55, 0xAA    ; boot signature
    times 510-($-$$) db 0
}
```

### 14.6 Calling Aether Functions from Assembly

```
asm {
    ; Call Aether function by label
    call @my_function
}
```

### 14.7 Full Example: Port I/O

```
func outb(port: u16, value: u8) {
    asm {
        mov dx, [port]
        mov al, [value]
        out dx, al
    }
}

func inb(port: u16) -> u8 {
    let result: u8
    asm {
        mov dx, [port]
        in al, dx
        mov [result], al
    }
    return result
}
```

---

## 15. Compile-Time Features

### 15.1 Compile-Time Constants

```
const MAX_BUFFER = 4096
const PI = 3.14159
```

### 15.2 Compile-Time Functions

```
comptime func factorial(n: u32) -> u32 {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}

const FACT_10 = factorial(10)    // computed at compile time
```

### 15.3 Compile-Time Reflection

```
@target        // "host", "kernel", "boot", "module", "binary", "universal"
@arch          // "x86_64", "arm64", "riscv64"
@endian        // "little" or "big"
@sizeof(T)     // size of type T
@alignof(T)    // alignment of type T
@line          // current line number
@file          // current file name
@func          // current function name
```

### 15.4 Conditional Compilation

```
# if @target == "kernel"
    func kernel_init() {
        // kernel-specific init
    }
# elif @target == "host"
    func host_init() {
        // host-specific init
    }
# else
    func default_init() {
        // fallback
    }
# end
```

### 15.5 Compile-Time Assertions

```
# assert @sizeof(u32) == 4
# assert @alignof(u64) == 8
```

### 15.6 Compile-Time Loops

```
comptime func generate_table() -> [256]u32 {
    let table: [256]u32
    for i in 0..256 {
        table[i] = i * i
    }
    return table
}

const SQUARE_TABLE = generate_table()
```

---

## 16. Module System

### 16.1 File as Module

Each `.ae` file is a module. The filename (without extension) is the module name.

### 16.2 Import

```
import "math.ae"
import "io/file.ae"
```

### 16.3 Public Items

```
pub func exported_func() { }
pub struct PublicStruct { }
pub const PUBLIC_CONST = 42
```

Items without `pub` are private to the module.

### 16.4 Module Paths

```
import "std/io.ae"        // relative to library path
import "../utils.ae"      // relative path
import "aether:math"      // standard library module
```

### 16.5 Re-exports

```
pub import "internal/helper.ae"    // re-export all public items
```

---

## 17. Target System

### 17.1 Target Selection

```
aether --target host source.ae        # macOS/Linux executable
aether --target kernel source.ae      # Aether OS kernel
aether --target boot source.ae        # 512-byte boot sector
aether --target binary source.ae      # Aether OS /bin/ executable
aether --target module source.ae      # Aether OS kernel module
aether --target universal source.ae   # Multi-arch binary
```

### 17.2 Target Annotations

```
@target("kernel")
func only_in_kernel() { }

@target("host")
func only_on_host() { }
```

### 17.3 Memory Layout Directives

```
@entry(0x7C00)        // Set entry point address
@layout(512)          // Flat binary with max size
@org(0x2000000)       // Origin address for code
@section(".text")     // Place following code in specific section
```

### 17.4 Target-Specific Code

```
# if @target == "kernel"
    func write_serial(c: char) {
        asm {
            mov dx, 0x3F8
            mov al, [c]
            out dx, al
        }
    }
# elif @target == "host"
    func write_serial(c: char) {
        print("{c}")
    }
# end
```

---

## 18. Standard Library

### 18.1 Built-in Functions (Always Available)

```
print(value)              // Print value to stdout/serial
print_i64(value)          // Print signed integer
print_u64(value)          // Print unsigned integer
print_hex(value)          // Print hex representation
assert(condition)         // Runtime assertion
panic(message)            // Fatal error
alloc[T](value)           // Heap-allocate T
alloc_array[T](count)     // Heap-allocate array
len(slice)                // Length of slice/array
copy(dst, src)            // Memory copy
zero(ptr, len)            // Zero memory
sizeof(T)                 // Compile-time size of T
```

### 18.2 OS Primitives (Kernel/Binary Targets)

```
syscall(n, ...)           // Call Aether OS syscall
outb(port, value)         // Write byte to I/O port
inb(port) -> u8           // Read byte from I/O port
cli()                     // Disable interrupts
sti()                     // Enable interrupts
hlt()                     // Halt CPU
```

### 18.3 Standard Library Modules

```
import "aether:io"        // I/O functions
import "aether:math"      // Math functions
import "aether:mem"       // Memory operations
import "aether:str"       // String utilities
import "aether:conv"      // Type conversion
```

---

## 19. Compiler Directives

### 19.1 Entry Point

```
@entry("_start")
@entry(0x7C00)
```

### 19.2 Layout

```
@layout(512)              // Max binary size
@layout(4096)
```

### 19.3 Origin

```
@org(0x7C00)              // Code origin address
@org(0x2000000)
```

### 19.4 Section

```
@section(".text")
@section(".rodata")
```

### 19.5 Target

```
@target("host")
@target("kernel")
```

### 19.6 Packed

```
struct #[packed] Header { ... }
```

### 19.7 Align

```
struct #[align(16)] CacheLine { ... }
```

### 19.8 Export

```
@export
func my_function() { }    // Force export even if unused
```

---

## 20. Full Examples

### 20.1 Hello World

```
func main() {
    print("Hello, Aether!\n")
}
```

Compile: `aether --target host hello.ae && ./hello`

### 20.2 FizzBuzz

```
func main() {
    for i in 1..=100 {
        if i % 15 == 0 {
            print("FizzBuzz\n")
        } else if i % 3 == 0 {
            print("Fizz\n")
        } else if i % 5 == 0 {
            print("Buzz\n")
        } else {
            print_i64(i)
            print("\n")
        }
    }
}
```

### 20.3 Linked List

```
class Node[T] {
    value: T
    next: ?*Node[T]

    constructor(value: T) {
        this.value = value
        this.next = none
    }

    destructor() {
        if let n = this.next {
            free(n)    // recursive destruction
        }
    }
}

class List[T] {
    head: ?*Node[T]

    func push(self, value: T) {
        let node = alloc Node[T](value)
        node.next = this.head
        this.head = node
    }

    func pop(self) -> ?T {
        if let h = this.head {
            let value = h.value
            this.head = h.next
            free(h)
            return value
        }
        return none
    }
}

func main() {
    let list = List[i32]{}
    list.push(1)
    list.push(2)
    list.push(3)

    while let val = list.pop() {
        print_i64(val)
        print("\n")
    }
}
```

### 20.4 Boot Sector

```
@entry(0x7C00)
@layout(512)
@org(0x7C00)

func _start() {
    asm {
        ; Set up segments
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7C00

        ; Print message
        mov si, msg
      .loop:
        lodsb
        or al, al
        jz .done
        mov ah, 0x0E
        int 0x10
        jmp .loop
      .done:
        hlt
        jmp .done

      msg: db "Aether Boot Loader", 0
    }
}
```

### 20.5 Kernel Module

```
@target("module")
@entry("mod_init")

struct ModuleInfo {
    name: *char
    version: u32
    api_version: u32
}

func mod_init(info: *ModuleInfo) -> u32 {
    info.name = "hello"
    info.version = 1
    info.api_version = 1
    return 0    // success
}
```

### 20.6 Exception Handling with Cleanup

```
class Resource {
    id: u32

    constructor(id: u32) {
        this.id = id
        print("Resource {id} acquired\n")
    }

    destructor() {
        print("Resource {this.id} released\n")
    }
}

func use_resources() throws {
    let r1 = Resource(1)
    defer print("cleaning up r1\n")

    let r2 = Resource(2)
    defer print("cleaning up r2\n")

    if random() > 0.5 {
        throw "random failure"
    }

    print("all operations successful\n")
}

func main() {
    for i in 0..5 {
        try {
            use_resources()
        } catch {
            print("caught exception in iteration {i}\n")
        }
    }
}
```

### 20.7 Universal Binary

```
@target("universal")

func main() {
    # if @arch == "x86_64"
        print("Running on x86_64\n")
    # elif @arch == "arm64"
        print("Running on ARM64\n")
    # elif @arch == "riscv64"
        print("Running on RISC-V\n")
    # end
}
```

### 20.8 Compile-Time Table Generation

```
comptime func generate_sine_table() -> [256]f64 {
    let table: [256]f64
    for i in 0..256 {
        table[i] = sin((i as f64) * 3.14159 / 128.0)
    }
    return table
}

const SINE_TABLE = generate_sine_table()

func lookup_sine(index: u32) -> f64 {
    return SINE_TABLE[index & 0xFF]
}
```

### 20.9 Trait-Based Rendering

```
trait Renderable {
    func render(self, buffer: mut ref [u32])
    func z_order(self) -> u32
}

struct Sprite {
    x: u32, y: u32
    pixels: [16][16]u32
}

impl Renderable for Sprite {
    func render(self, buffer: mut ref [u32]) {
        for dy in 0..16 {
            for dx in 0..16 {
                let px = self.x + dx
                let py = self.y + dy
                buffer[py * 320 + px] = self.pixels[dy][dx]
            }
        }
    }

    func z_order(self) -> u32 {
        return self.y    // sort by y for painter's algorithm
    }
}

func render_all(items: []dyn Renderable, buffer: mut ref [u32]) {
    // Sort by z-order
    // ... (sorting logic)

    for item in items {
        item.render(buffer)
    }
}
```

### 20.10 Contract-Based Math

```
func sqrt(value: f64) -> f64
    require value >= 0.0
    ensure result * result >= value - 0.001
    ensure result * result <= value + 0.001
{
    if value == 0.0 { return 0.0 }

    var guess = value
    var better = (guess + value / guess) / 2.0

    while abs(guess - better) > 0.0001 {
        guess = better
        better = (guess + value / guess) / 2.0
    }

    return better
}
```

---

## Appendix A: Compiler Pipeline

```
Source (.ae)
  │
  ▼
Tokenizer ─── character stream → token stream
  │
  ▼
Lexer ─────── token stream → structured tokens
  │
  ▼
Parser ────── tokens → Abstract Syntax Tree (AST)
  │
  ▼
Semantic ──── type checking, name resolution, lifetime analysis
  │
  ▼
Optimizer ─── constant folding, dead code elimination, inlining
  │
  ▼
Codegen ───── AST → NASM assembly text
  │
  ▼
Assembler ─── NASM → object file (.o)
  │
  ▼
Linker ─────── object file → executable
  │
  ▼
Output (Mach-O, ELF, flat binary, or universal)
```

## Appendix B: Reserved Words

```
alloc       and         as          asm         bool
break       case        catch       class       comptime
const       constructor continue    defer       destructor
dyn         else        enum        ensure      false
for         func        if          impl        import
in           is         let         match       mod
mut         not         or          override    owned
pub         ref         region      require     return
self        static      struct      super       throw
throws      trait       true        try         type
unsafe      use         var         while
```

## Appendix C: Operator Precedence

| Precedence | Operators | Associativity |
|-----------|-----------|---------------|
| 1 (highest) | `()`, `[]`, `.`, `?.`, `!` | Left |
| 2 | `ref`, `mut`, `*`, `&`, `-`, `~`, `not`, `owned` | Right |
| 3 | `as`, `is` | Left |
| 4 | `*`, `/`, `%` | Left |
| 5 | `+`, `-` | Left |
| 6 | `<<`, `>>` | Left |
| 7 | `&` | Left |
| 8 | `^` | Left |
| 9 | `|` | Left |
| 10 | `==`, `!=`, `<`, `>`, `<=`, `>=`, `<=>` | Left |
| 11 | `and` | Left |
| 12 | `or` | Left |
| 13 | `..`, `..=` | Right |
| 14 | `??` | Right |
| 15 (lowest) | `=`, `+=`, `-=`, etc. | Right |

## Appendix D: ABI for Aether OS

### Kernel ABI

- **Format:** ELF64
- **Entry:** `_start`
- **Load address:** 0x1000000
- **Stack:** 16KB at 0x1000000 - 0x4000
- **No libc, no red zone, no SSE**
- **Calling convention:** System V AMD64 (with modifications for freestanding)

### Binary ABI

- **Format:** ELF64
- **Entry:** `_start`
- **Load address:** 0x2000000
- **Syscall page:** 0x5000 (function pointer table)
- **Stack:** 12KB at 0x2000000 + 0x10000 + 4096

### Module ABI

- **Format:** ELF64 relocatable
- **Entry:** `mod_init`
- **Module registry:** 0x4000
- **Module slots:** 8 × 64KB at 0x2100000

### Boot Sector ABI

- **Format:** Flat binary
- **Entry:** 0x7C00
- **Max size:** 512 bytes (or configurable via `@layout`)
- **Segments:** CS=0, DS=0, ES=0, SS=0, SP=0x7C00

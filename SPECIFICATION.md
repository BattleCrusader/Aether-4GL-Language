# Aether Language Specification

**Version**: 1.0 (Comprehensive)
**Status**: Living Document — Last updated: 2026-06-29

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
24. [Future & Aspirational Features](#24-future--aspirational-features)
25. [Concurrency and Fibers](#25-concurrency-and-fibers)
26. [Module System and Imports](#26-module-system-and-imports)

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

```aether
// Line comment

/* Block comment */

/* Nested /* block */ comments */

/// Documentation comment — attached to the next declaration
/// These are used by the `aether doc` tool to generate API docs
```

**Documentation comments** (`///`) are attached to the declaration that follows them. They are collected by the documentation generator (`aether doc`). Unlike regular comments, doc comments are associated with specific declarations (functions, types, constants, modules) and can include markdown formatting.

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
and         as          bool        break       byte
catch       char        class       const       continue
defer       double      dyn         elif        else
enum        export      extern      false       float
for         func        heap        i8          i16
i32         i64         if          impl        import
in          init        inline      int         internal
let         module      mut         none        not
or          owned       pool        post        pre
private     protocol    public      ptr         rc
ref         region      return      self        static
string      struct      super       sys         throw
trait       true        try         type        u8
u16         u32         u64         u128        unsafe
use         var         void        while       yield
```

> **Note:** `int`, `float`, `double`, and `char` are type aliases (see §4.3). `void` is a zero-sized type used for functions with no return value. `none` is the null value for optional types. `true` and `false` are boolean literals. `self` is the implicit method receiver. `init` and `drop` are special method names for constructors and destructors.

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
Array length:   #   (prefix operator, e.g. #arrayName)
Pattern:        |   ..=
```

> **Array length `#`**: The `#` prefix operator returns the length of an array (the first 8 bytes of the array's header). At codegen time, it reads `[array_ptr]` to get the stored count. Supported for array literals, array-typed variables, and dynamic arrays.

**Short-circuit evaluation**: The logical operators `&&`, `||`, `and`, and `or` use short-circuit evaluation. If the left operand determines the result, the right operand is not evaluated:

```aether
let a = false and expensive()   // expensive() never called
let b = true or expensive()     // expensive() never called
```

This is guaranteed behavior — the compiler never evaluates the right side when the left side is sufficient. This applies to both symbolic (`&&`, `||`) and keyword (`and`, `or`) forms.

**Compound assignment operators** (`+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`): These operators modify a variable in place. The codegen evaluates the left operand to get the current value, applies the operation with the right operand, and stores the result back. Supported for all numeric and bitwise operation combinations.

**Prefix/postfix increment and decrement** (`++x`, `x++`, `--x`, `x--`): Both prefix and postfix forms are supported. The parser creates `UNARY_INC` and `UNARY_DEC` nodes, and codegen emits `inc`/`dec` instructions. Prefix form returns the incremented value; postfix form returns the original value.

```aether
let mut x: u64 = 5
x += 3           // x = 8  (compound add)
x *= 2           // x = 16  (compound mul)
++x              // x = 17  (prefix increment)
let y = x++      // y = 17, x = 18  (postfix increment)
```

**Bitwise operators** (`&`, `|`, `^`, `~`, `<<`, `>>`): Full bitwise operations are supported in the parser and codegen. The `~` prefix operator creates `UNARY_BIT_NOT` nodes. All operations are evaluated at compile time when operands are constants.

```aether
let flags: u64 = 0b1010
let mask: u64 = 0b1100
let and = flags & mask   // 0b1000
let or  = flags | mask   // 0b1110
let xor = flags ^ mask   // 0b0110
let not = ~flags         // bitwise NOT
let shl = flags << 2     // 0b101000
let shr = flags >> 1     // 0b0101
```

**Logical keyword operators** (`and`, `or`, `not`): These keyword operators are synonyms for `&&`, `||`, and `!`. They are treated identically in the parser and generated the same code. The tokenizer emits `TOKEN_KW_AND`, `TOKEN_KW_OR`, `TOKEN_KW_NOT` which the parser maps to `BIN_AND`, `BIN_OR`, and `UNARY_NOT`.

```aether
let a = true and false   // false (same as &&)
let b = true or false    // true  (same as ||)
let c = not true         // false (same as !)
```

**Range expressions** (`..` and `..=`): Range expressions are used in `for` loops and match range patterns. `a..b` creates a half-open range `[a..b)`, while `a..=b` creates an inclusive range `[a..b]`. The parser creates `BIN_RANGE` and `BIN_RANGE_INCLUSIVE` nodes.

```aether
for i in 0..10 { }       // 0 through 9
for i in 0..=10 { }      // 0 through 10
match x { case 1..9 -> ... }  // range pattern
```

### 3.5 Operator Precedence

Operators are evaluated in the following order, from highest to lowest precedence:

| Level | Operators | Associativity | Description |
|-------|-----------|---------------|-------------|
| 1 (highest) | `()` `[]` `.` `::` `->` | Left-to-right | Call, index, member access, scope, arrow |
| 2 | `#` `~` `!` `not` `-` (unary) `++` `--` (prefix) | Right-to-left | Prefix operators |
| 3 | `*` `/` `%` | Left-to-right | Multiplicative |
| 4 | `+` `-` | Left-to-right | Additive |
| 5 | `<<` `>>` | Left-to-right | Bitwise shift |
| 6 | `&` | Left-to-right | Bitwise AND |
| 7 | `^` | Left-to-right | Bitwise XOR |
| 8 | `\|` | Left-to-right | Bitwise OR |
| 9 | `..` `..=` | Left-to-right | Range |
| 10 | `==` `!=` `<` `>` `<=` `>=` | Left-to-right | Comparison |
| 11 | `&&` `and` | Left-to-right | Logical AND |
| 12 | `\|\|` `or` | Left-to-right | Logical OR |
| 13 | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` | Right-to-left | Assignment |
| 14 (lowest) | `->` (arrow) | Right-to-left | Expression body |

**Notes:**
- Postfix `++` and `--` have the same precedence as level 1 (evaluated before prefix operators).
- The `not` keyword operator has the same precedence as `!`.
- The `and`/`or` keyword operators have the same precedence as `&&`/`||`.
- Assignment operators are right-associative: `a = b = c` parses as `a = (b = c)`.

**String indexing**: Strings can be indexed with `[]` to access individual bytes. When `s[i]` is used on a string-typed expression, the codegen returns a single byte (u8) at the given position.

```aether
let s = \"hello\"
let b = s[0]   // 'h' (byte 0x68)
```

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

### 3.6 Literals

```aether
// Integer literals
42              // decimal
0xFF            // hexadecimal
0o77            // octal
0b1010          // binary
1_000_000       // digit separators (underscores ignored)

// Float literals
3.14
1.0e-5
1_000.5         // digit separators in floats

// String literals
"hello, world"
"escaped: \n \t \\ \" \x41"

// String interpolation — expressions in {} are evaluated and converted to strings
let name = "World"
let msg = "Hello {name}!"       // "Hello World!"
let n: u64 = 42
let s = "count = {n}"           // "count = 42" (auto-converted via __aether_itoa)
let sum = "total: {x + y}"      // expressions work too

// Escaping braces in string interpolation
let s = "literal brace: \{not_interpolated\}"  // \{ and \} produce literal { and }

// Multi-line strings
// Use string concatenation or interpolation instead.
// """ ... """ syntax is reserved for future implementation.

// Character literals
'a'
'\n'
'\x41'          // hex escape in char

// Boolean literals
true
false

// None literal
none
```

> **Digit separators**: Underscores (`_`) in numeric literals are ignored by the compiler. They exist only for readability: `1_000_000`, `0xFF_FF_FF_FF`, `0b1010_0101`. Multiple underscores are allowed but not between the prefix (`0x`, `0b`, `0o`) and the first digit.

### 3.7 String Interpolation

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

### 3.8 Indentation

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

Aether provides a full set of primitive types. All type names are lowercase keywords:

| Type | Description | Size |
|------|-------------|------|
| `void` | Zero-sized type (no value) | 0 bytes |
| `bool` | Boolean (true/false) | 1 byte |
| `byte` | Raw byte (u8 alias) | 1 byte |
| `u8` | Unsigned 8-bit integer | 1 byte |
| `u16` | Unsigned 16-bit integer | 2 bytes |
| `u32` | Unsigned 32-bit integer | 4 bytes |
| `u64` | Unsigned 64-bit integer | 8 bytes |
| `i8` | Signed 8-bit integer | 1 byte |
| `i16` | Signed 16-bit integer | 2 bytes |
| `i32` | Signed 32-bit integer | 4 bytes |
| `i64` | Signed 64-bit integer | 8 bytes |
| `f32` | IEEE 754 32-bit float (always signed) | 4 bytes |
| `f64` | IEEE 754 64-bit double (always signed) | 8 bytes |
| `string` | Null-terminated string (pointer + length header) | 8 bytes (pointer) |

**Notes:**
- All integer types (u8-u64, i8-i64) support arithmetic, bitwise, and comparison operations.
- Signed integers (i8-i64) use two's complement representation.
- Floating point types (f32, f64) follow IEEE 754. There are no "unsigned" float variants — all floats have a sign bit.
- `bool` values are 0 (false) or 1 (true).
- `byte` is an alias for `u8`.
- There is no implicit `int` type — all types must be explicit in declarations.

**Integer overflow semantics:**

| Operation | Debug mode | Release mode |
|-----------|------------|--------------|
| `+` `-` `*` (signed) | ⚠️ Wraps (two's complement) | ⚠️ Wraps (two's complement) |
| `+` `-` `*` (unsigned) | ⚠️ Wraps (modulo 2^N) | ⚠️ Wraps (modulo 2^N) |
| `/` `%` (division by zero) | 💥 Runtime panic | 💥 Runtime panic |
| `<<` `>>` (shift >= bit width) | ⚠️ Wraps (shift masked) | ⚠️ Wraps (shift masked) |

> **Note:** Aether uses wrapping arithmetic by default (like C). There is no implicit overflow check. If you need checked arithmetic, use the standard library functions (`checked_add`, `checked_mul`, etc.) or implement your own. Division by zero always panics regardless of build mode.

```aether
let a: u8 = 255          // unsigned 8-bit
let b: u16 = 65535       // unsigned 16-bit
let c: u32 = 4294967295  // unsigned 32-bit
let d: u64 = 18446744073709551615  // unsigned 64-bit
let e: bool = true       // boolean
let f: byte = 0xFF       // raw byte
let g: i8 = -128         // signed 8-bit
let h: i16 = -32768      // signed 16-bit
let i: i32 = -2147483648 // signed 32-bit
let j: i64 = -9223372036854775808 // signed 64-bit
let k: f32 = 3.14        // 32-bit float
let l: f64 = 3.141592653589793 // 64-bit float
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

// Function types
let handler: func(u64, string): bool   // function pointer type
let callback: func(): void             // void-returning function

// Tuple types
let pair: (u64, string)                // anonymous tuple
let triple: (i32, f64, bool)           // 3-element tuple
```

**Function type syntax:** `func(ParamType1, ParamType2, ...): ReturnType`. The parameter list is comma-separated. The return type follows the colon. For void functions, the return type is omitted or `void`.

**Tuple type syntax:** `(Type1, Type2, ...)`. Tuples are anonymous structs. They can be destructured:

```aether
let pair = (42, "hello")
let (num, str) = pair   // destructuring
```

**Type casting with `as`:** The `as` keyword performs explicit type conversion between compatible types:

```aether
let x: u64 = 42
let y: u8 = x as u8       // truncates to 8 bits
let z: f64 = 3.14
let w: f32 = z as f32     // float truncation
let b: byte = 0x41
let c: char = b as char   // byte to char (same type)
```

> **Note:** `as` casts are unchecked — they truncate or reinterpret bits without runtime checks. For checked conversions, use the constructor pattern: `u8(x)`, `f32(z)`, etc. which may perform bounds checking in debug mode.

### 4.3 Type Aliases

Aether provides common type aliases for convenience. These are defined in the standard library and are just shorthand for the underlying explicit types:

| Alias | Underlying Type | Description |
|-------|----------------|-------------|
| `int` | `i64` | Default signed integer |
| `float` | `f32` | Default floating-point |
| `double` | `f64` | Double-precision float |
| `char` | `byte` | Single character (byte) |

> **Note:** These are aliases, not distinct types. `int` and `i64` are the same type. Use the explicit types (`u64`, `i32`, `f64`, etc.) when you need a specific size — aliases are for convenience in general-purpose code.

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

Enumerations are **tagged unions** — a type that can hold one of several named variants. Each variant can optionally carry payload data. Enum construction uses the `::` (scope resolution) syntax.

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

**Enum construction with `::`**: The `EnumName::Variant(args)` syntax is handled by the parser. When the codegen encounters a `::` expression where the left side is an enum type and the right side is a variant name, it creates the enum discriminant (data pointer or integer tag) in `rax`. For variants with payload, the payload is stored inline in the struct layout.

**Key rules:**
- Variants without payload are unit-like (like `NotFound`, `PermissionDenied`)
- Variants with payload carry data inline (`InvalidInput(string)`, `IoError(u64, string)`)
- Match expressions can destructure enum variants and bind payload fields
- Enums are stored as a discriminant + payload (sum type representation)

**Pattern matching on enums:**
```aether
match err {
    Error::NotFound -> print("not found")
    Error::PermissionDenied -> print("denied")
    Error::InvalidInput(msg) -> print("invalid: {msg}")
    Error::IoError(code, msg) -> print("io error {code}: {msg}")
}
```

### 4.6 Classes

Classes are **syntactic sugar over structs + associated functions**. The compiler tracks constructors (`init`) and destructors (`drop`) and inserts calls automatically. Classes differ from structs in that they support `init`/`drop` and automatic memory management.

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

**Key rules:**
- `init` is the constructor name — called automatically on variable declaration
- `drop` is the destructor name — called automatically at scope exit
- Methods defined inside the class body get implicit `self: ref Type` as first parameter
- Fields are initialized in declaration order
- Classes are emitted as C structs with companion functions

**Constructors:** You can define multiple `init` methods with different parameters. The compiler selects the matching one based on call-site arguments.

**Destructors:** The `drop` method is called in LIFO order at scope exit. You don't need to write explicit cleanup code.

### 4.7 Traits

Traits define **shared behavior** — a contract that types can implement. They are Aether's primary mechanism for polymorphism and code organization.

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

**Key characteristics of traits:**
- **Behavior-only**: Traits define method signatures, never fields or data
- **Static dispatch by default**: `ref Trait` compiles to monomorphized code — zero overhead
- **Dynamic dispatch opt-in**: `ref dyn Trait` uses fat pointers (vtable + data pointer) for runtime polymorphism
- **Generic type params**: Traits can be parameterized (`trait Comparable<T>`) for type-safe generic algorithms
- **Type constraints**: Traits can constrain generic type params (`func min<T: Comparable>(a: T, b: T): T`)
- **No data**: Traits describe *what a type can do*, not *what a type is*

**When to use traits:**
- Defining interfaces that multiple types can implement
- Writing generic algorithms that work with any type satisfying a contract
- Organizing code into logical capability groups (Serializable, Comparable, Iterable)
- Enabling polymorphism with zero-cost static dispatch
- Constraining generic type parameters

### 4.8 Protocols

Protocols define **hardware-level I/O interfaces** — they describe how to communicate with physical devices, ports, and buses. Unlike traits, protocols carry configuration data (pin assignments, port addresses, baud rates) and their methods contain inline assembly for direct hardware interaction.

```aether
protocol Serial {
    port base = 0x3F8
    speed = 115200

    func putc(c: byte) {
        asm { mov dx, port; mov al, c; out dx, al }
    }
}
```

**Key characteristics of protocols:**
- **Data + behavior**: Protocols declare fields (pin numbers, port addresses, speeds) and methods
- **Hardware-oriented**: Methods contain inline assembly for register-level I/O
- **Compile-time configuration**: Field values are compile-time constants, enabling the compiler to generate optimized bit-banging sequences
- **No polymorphism**: Protocols are not implemented by types — they are standalone hardware interface descriptions
- **Code generation target**: The compiler can generate complete driver code from a protocol declaration

**When to use protocols:**
- Defining serial bus protocols (I2C, SPI, UART)
- Configuring hardware pin assignments and port addresses
- Writing low-level device drivers with inline assembly
- Describing memory-mapped I/O regions
- Generating optimized bit-banging code from declarative specs

### 4.9 Trait vs Protocol — Comparison

| Aspect | Trait | Protocol |
|--------|-------|----------|
| **Purpose** | Define shared behavior/interfaces | Define hardware I/O interfaces |
| **Data** | No fields — method signatures only | Fields (pins, ports, speeds, addresses) |
| **Implementation** | `impl Trait for Type { ... }` | Standalone declaration with inline asm |
| **Polymorphism** | Static dispatch (default), dynamic (`dyn`) | None — single hardware target |
| **Generics** | Generic type params supported | Not applicable |
| **Codegen** | Monomorphization or vtable dispatch | Compiler generates driver code |
| **Use case** | Application-level polymorphism | Hardware-level device drivers |
| **Runtime cost** | Zero (static) or one deref (dynamic) | Zero — all code is compile-time generated |
| **Example** | `trait Drawable { func draw() }` | `protocol I2C { scl pin = 5; sda pin = 6 }` |

**Rule of thumb:** If you're describing *what a type can do*, use a trait. If you're describing *how to talk to hardware*, use a protocol.

### 4.10 Optional Types

**`T?`** (optional type) represents a value that may be present or absent. It's syntactic sugar for a tagged union: `some(T) | none`. Optionals are the primary way to handle "no value" cases without null pointers.

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

**Key rules:**
- `T?` is a type alias for the optional sum type
- `some(expr)` wraps a value, `none` represents absence
- `if let pattern = optional { }` binds the inner value if present
- `or` operator provides a default: `optional or default_value`
- Under the hood, optionals use a discriminant byte (0 = none, 1 = some) + payload

**Matching on optionals:**
```aether
match x {
    some(val) -> print("has value: {val}")
    none -> print("empty")
}
```

---

## 5. Variables and Bindings

### 5.1 Let Bindings

**`let`** declares an immutable binding. **`let mut`** declares a mutable binding. Types can be explicit or inferred from the initializer.

```aether
let x: u64 = 42          // explicit type
let y = 42               // type inferred (u64)
let mut z = 10           // mutable
z = 20                   // ok

const MAX = 100          // compile-time constant (must be a literal or const expression)
```

**Key rules:**
- `let` bindings are immutable by default — use `mut` to allow reassignment
- Type inference works for all expressions including function calls, arithmetic, and struct literals
- `const` is evaluated at compile time and substituted inline — no runtime storage
- Shadowing is allowed: `let x = 10; let x = x + 5;` creates a new binding
- Destructuring is supported: `let (a, b) = (1, 2);`

**Destructuring example:**
```aether
func point() -> (u64, u64) { return (1, 2) }
let (x, y) = point()   // x = 1, y = 2
```

### 5.2 Scope

```aether
{
    let inner = 5
    // inner accessible here
}
// inner not accessible here
```

Variables are scoped to the block (`{ ... }`) they are declared in. Nested blocks can see outer variables; outer blocks cannot see inner variables.

### 5.3 Shadowing

```aether
let x = 10
let x = x + 5   // shadows previous x, now 15
```

Shadowing creates a new binding with the same name in the same or inner scope. The previous binding is hidden, not modified. This is different from mutation (`mut`) — shadowing changes the binding, mutation changes the value.

### 5.4 Global Variables

File-scope `let` declarations create global variables. They are initialized at program startup and live for the entire program lifetime:

```aether
let GLOBAL_CONFIG: string = "default"
let mut counter: u64 = 0
```

Global variables are stored in the `.bss` (zero-initialized) or `.data` (initialized) section. Mutable globals require `unsafe` to access from multiple functions, as the compiler cannot prove thread safety.

### 5.5 Recursion

Aether supports recursive functions. The compiler does not perform tail-call optimization (TCO) yet:

```aether
func factorial(n: u64): u64 {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
```

Recursion depth is limited only by the available stack space. The default stack size is 2 MB on host targets and configurable on freestanding targets.

### 5.6 Loop Labels

Loops can be labeled to allow `break` and `continue` to target an outer loop:

```aether
outer: for i in 0..10 {
    for j in 0..10 {
        if i * j > 50 { break outer }   // breaks out of both loops
        if j == 5 { continue outer }    // continues the outer loop
    }
}
```

Labels are identifiers followed by a colon (`:`). They can be applied to `for`, `while`, and `loop` constructs.

---

## 6. Functions
### 6.1 Function Declaration

Functions are declared with the `func` keyword. Parameters are written as `name: type` pairs. The return type follows the parameter list after `:`. The body is a block or an expression (for expression-bodied functions).

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

**Key rules:**
- Parameters are passed by value (copy) unless `ref` or pointer types are used
- The last expression in a block is implicitly returned if no explicit `return`
- `return` is optional in expression-bodied functions (`-> expr`)
- Void functions implicitly return `0` at the end (via `xor rax, rax` in assembly)

### 6.2 Function Attributes

Attributes modify function behavior at compile time. They are written as `@name` or `@name(args)` before the `func` keyword.

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

// Test fixture attribute — marks a function as a test with expected exit code
@test(expect=0) func test_addition(): u64 {
    assert(1 + 1 == 2)
    return 0
}

// Module ABI version declaration
@module_abi(version=1) module my_module {
    // module body
}
```

**Standard attributes:**

| Attribute | Description |
|-----------|-------------|
| `@force_inline` | Force the compiler to inline this function |
| `@no_inline` | Prevent inlining of this function |
| `@export` | Export function for module loader visibility |
| `@entry(addr)` | Set binary entry point at specific address |
| `@test(expect=N)` | Mark function as test fixture; binary exits with N on success |
| `@module_abi(version=N)` | Declare ABI version for module declarations |
| `@layout(start=N, max=M, file="name")` | Flat binary layout: start address, max size, output file |
| `@kernel_layout` | Enable kernel memory map overlap verification |

### 6.3 Default Parameters

```aether
func create_window(title: string, width: u64 = 800, height: u64 = 600) {
    // ...
}

create_window("Hello")                    // uses defaults
create_window("Wide", 1024, 768)          // explicit
```

Default parameters are evaluated at the call site. They must be trailing parameters (all parameters after a default must also have defaults).

### 6.4 Variadic Functions

Variadic parameters accept zero or more arguments of the given type. The variadic parameter must be the last parameter in the function signature.

```aether
func sum(values: ...u64): u64 {
    let total = 0
    for v in values {
        total += v
    }
    return total
}

let s = sum(1, 2, 3, 4, 5)  // s = 15
let empty = sum()            // empty call — returns 0
```

Inside the function, the variadic parameter behaves like an array. Iterating over it with `for v in values` works as expected. Calling with no arguments produces an empty array (length 0), so the loop body never executes.

### 6.5 Extern Functions (FFI)

```aether
extern func puts(s: string): int
extern func malloc(size: u64): ptr void
extern func free(ptr: ptr void)
```

Extern functions are declared without a body. They tell the compiler the function exists in another object file or shared library. The compiler emits an `extern` NASM directive and generates a call instruction with the correct ABI (SysV for x86_64).

**Calling convention:** Aether uses the System V AMD64 ABI for extern calls:
- Integer/pointer arguments: `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`
- Floating-point arguments: `xmm0`–`xmm7`
- Return value: `rax` (integer/pointer), `xmm0` (float/double)
- Stack alignment: 16 bytes before `call` instruction

```aether
extern func printf(fmt: string, ...): int

func main(): u64 {
    printf("Hello from C! %d\n", 42)
    return 0
}
```

> **Note:** Extern functions are unsafe by nature — the compiler cannot verify the types or memory safety of external code. Use `unsafe` blocks when calling extern functions that involve pointer manipulation.

---

## 7. Control Flow

### 7.1 If / Elif / Else

The `if` statement evaluates a condition and executes the corresponding block. It can also be used as an expression.

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

**Key rules:**
- `elif` and `else` are optional
- As an expression, all branches must evaluate to the same type
- The condition must be a boolean expression
- Blocks are required — no single-line `if x > 0: print("x")` syntax

**If-let pattern matching:**
```aether
if let some(val) = optional_value {
    // val is bound here
    print("Got: {val}")
}
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

The `while` loop evaluates the condition before each iteration. If the condition is false initially, the body never executes.

### 7.3 For Loops

Aether supports three forms of `for` loops:

**Range loops:**
```aether
// Range loop (half-open: 0..10 means 0 through 9)
for i in 0..10 {
    print(i)
}

// Inclusive range (0..=10 means 0 through 10)
for i in 0..=10 {
    print(i)
}
```

**Array iteration:**
```aether
let arr = [1, 2, 3, 4, 5]
for val in arr {
    print(val)
}
```

**With index:**
```aether
for i, val in arr {
    print("arr[{i}] = {val}")
}
```

**Key rules:**
- Range expressions `a..b` and `a..=b` are lazy — they don't allocate an array
- Iterating over an array yields elements by value (copy)
- `for i, val` yields the index (u64) and the element value
- The loop variable is scoped to the loop body

### 7.4 Break and Continue

```aether
for i in 0..100 {
    if i == 50 { break }
    if i % 2 == 0 { continue }
    print(i)  // odd numbers 1..49
}
```

- `break` exits the innermost loop
- `continue` skips to the next iteration of the innermost loop
- Labels can target outer loops: `break outer`, `continue outer`

### 7.5 Match Statements

`match` is both a statement and an expression. It compares a value against a series of patterns and executes the first matching arm.

```aether
match value {
    case 0 -> print("zero")
    case _ -> print("default")
}

// Match as expression
let description = match value {
    case 0 -> "zero"
    case 1..9 -> "small"
    case _ -> "large"
}
```

**Pattern types:**
- Literal patterns: `case 0`, `case "hello"`
- Range patterns: `case 1..9`, `case 1..=9`
- Wildcard: `case _` (matches anything, must be last)
- Enum destructuring: `case Error::InvalidInput(msg)`

**Key rules:**
- Patterns are evaluated top-to-bottom; first match wins
- The wildcard `_` must be the last arm
- As an expression, all arms must evaluate to the same type
- Match expressions can be used in assignments, returns, etc.

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

`defer` schedules a block to run when the current scope exits — whether by normal return, error throw, `break`, or `continue`. This is the primary mechanism for resource cleanup.

**Defer ordering:** Deferred calls are executed in **last-in-first-out (LIFO)** order — the most recently deferred block runs first:

```aether
func example() {
    defer { print("first defer") }   // runs second
    defer { print("second defer") }  // runs first
}
// Output: "second defer" then "first defer"
```

**Defer in try/catch:** Deferred blocks inside a try body execute before the catch handler runs. This ensures resources are cleaned up before error handling:

```aether
try {
    let f = File::open("/etc/config")
    defer { f.close() }
    // If an error occurs, f.close() runs before the catch block
} catch _ {
    print("error handled after cleanup")
}
```

**Defer in loops:** Deferred blocks inside a loop body execute at the end of each iteration, not when the loop exits:

```aether
for i in 0..3 {
    defer { print("cleaning up iteration") }
    // defer runs at end of each iteration
}
// defer runs 3 times, once per iteration
```

---

## 8. Memory Management

This is the heart of the language and what makes it a true 4GL — **you describe allocation semantics; the compiler generates the exact free/teardown code.**

## 8. Memory Management

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

**Key rules:**
- All `let` bindings without `heap`, `region`, or `rc` are stack-allocated
- The compiler tracks the lifetime of each variable and emits cleanup at scope exit
- For types with `drop` methods (classes), the destructor is called in LIFO order
- Stack allocation is zero-cost — just a stack pointer bump

### 8.2 Escape Analysis

When a reference to a stack variable could outlive its scope, the compiler **automatically promotes** it to heap allocation. No programmer annotation needed for simple cases.

```aether
func make_point(x: int, y: int): ref Point {
    let p = Point(x, y)
    // compiler detects: p's reference escapes this frame
    // auto-promotes p to heap
    return p
}
```

Escape analysis is conservative — if the compiler cannot prove a reference doesn't escape, it promotes to heap. This is sound but may over-allocate.

### 8.3 Explicit Heap (`heap` keyword)

```aether
let big = heap Buffer(1024 * 1024)
let shared = heap rc SharedState()
```

The `heap` keyword forces heap allocation even if escape analysis would stack-allocate. Use it for:
- Large objects that would overflow the stack
- Objects that need to outlive the current function
- When you need explicit control over lifetime

### 8.4 Ownership and Borrowing

Aether's ownership model is inspired by Rust but with different syntax and semantics:

- **`owned T`** — single-owner, moved on assignment, freed when owner drops
- **`ref T`** — borrowed reference, non-owning, must not outlive the lender
- **`rc T`** — reference-counted shared ownership with atomic reference counting, freed when count reaches zero

```aether
func consume(val: owned Buffer) {   // val will be freed at end
    process(val)
}  // val freed here

func observe(val: ref Buffer) {     // borrow, no ownership
    print(val.size())
}  // nothing freed, borrow ends

func share(val: rc Buffer) {        // shared ownership
    let clone = val  // increments refcount
}  // decrements refcount, frees if zero
```

**Key rules:**
- `owned` values are moved on assignment — the original binding becomes invalid
- `ref` bindings are non-owning — they must not outlive the value they reference
- `rc` types use atomic reference counting — thread-safe but with overhead
- The compiler enforces ownership at compile time where possible

### 8.5 Region-Based Allocation

For kernel modules and performance-critical code, the language supports **region-based allocation** that maps directly to the Aether OS capability model.

```aether
region("kernel") {
    let p = Page()   // allocated from kernel region
    let buf = Buffer(256)
}  // entire region freed at once — O(1) teardown
```

Regions are named allocation arenas. All allocations inside a `region { }` block come from that region's arena. When the block exits, the entire arena is freed in one operation — O(1) regardless of how many objects were allocated.

**Use cases:**
- Kernel memory pools
- Request-scoped allocations in servers
- Temporary working sets that can be bulk-freed

### 8.6 No Null Pointers

The type system has no null. Use `T?` (optional) instead.

```aether
let x: u64? = some(42)
let y: u64? = none
```

**Why no null:**
- Null pointers are the "billion dollar mistake" — they require runtime checks
- `T?` makes absence explicit in the type system
- The compiler can prove optional handling is complete (exhaustive match)
- No silent crashes from dereferencing null

**Optional patterns:**
```aether
// If-let
if let val = optional { ... }

// Match
match opt {
    some(v) -> ...
    none -> ...
}

// Default with `or`
let val = opt or default_value
```
```

### 8.7 Pointers (Opt-In, Unsafe)

```aether
func read_mmio(addr: ptr u64): u64 {
    unsafe {
        return *addr
    }
}
```

**What is allowed inside `unsafe` blocks:**
- Dereferencing raw pointers (`*ptr`)
- Calling functions marked `unsafe`
- Inline assembly (`asm { }`)
- Direct memory manipulation (pointer arithmetic, type punning)
- Accessing or modifying mutable statics

**What is NOT allowed inside `unsafe` blocks:**
- Bypassing the type system (you cannot cast `ptr u64` to `string` without an explicit `as` cast)
- Creating references with invalid lifetimes (the borrow checker still applies to `ref` types)
- Calling non-`unsafe` functions with invalid arguments (type checking still applies)

> **Safety note:** `unsafe` does not disable the compiler. Type checking, borrow checking, and lifetime analysis still apply to `ref` types. `unsafe` only enables operations that the compiler cannot prove safe: pointer dereference, inline assembly, and FFI calls.

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

    public func read( buf: ref [u8]): int {
        return sys_read(self.fd, buf)
    }

    public prop path(): string {
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
    public balance: u64       // accessible anywhere
    private owner: string  // accessible only within this class
    internal pin: u64      // accessible within the same module

    public func deposit(amount: u64) {
        self.balance += amount
    }

    private func validate_pin(input: u64): bool {
        return self.pin == input
    }
}
```

### 9.6 Properties (Inferred from Return Type)

Properties are methods that look like fields. The compiler infers getter/setter from the return type:

- **Getter**: has a return type → `obj.prop()` calls the getter
- **Setter**: has no return type → `obj.prop(value)` calls the setter

Properties are defined as regular `func` methods inside struct/class bodies. The `self` parameter is auto-injected by the compiler — you never write it yourself.

```aether
struct Temperature {
    celsius: f64

    // Getter — has return type
    func fahrenheit(): f64 {
        return self.celsius * 9.0 / 5.0 + 32.0
    }

    // Setter — no return type
    func fahrenheit(value: f64) {
        self.celsius = (value - 32.0) * 5.0 / 9.0
    }
}

let t: Temperature
t.celsius = 100
print(t.fahrenheit())  // calls getter → 212.0
t.fahrenheit(32)    // calls setter → celsius = 0
```

> **Note:** `self` is a reserved keyword referencing the enclosing object. It is never passed as a parameter — the compiler injects it automatically. You only need `self` when a local variable or parameter name shadows a field name.

### 9.7 Operator Overloading

Operators can be overloaded by defining a function named `op_` followed by the operator symbol. The `op_` prefix is reserved — it can only be used for operator overloading. The symbol can be any sequence of characters (including unicode), making custom operators possible.

Operator overloads use explicit left/right parameters (C++ style):

```aether
struct Vector2 {
    x: f64
    y: f64
}

func op_+(a: Vector2, b: Vector2): Vector2 {
    let result: Vector2
    result.x = a.x + b.x
    result.y = a.y + b.y
    return result
}

func op_*(a: Vector2, scalar: f64): Vector2 {
    let result: Vector2
    result.x = a.x * scalar
    result.y = a.y * scalar
    return result
}

let a: Vector2
a.x = 1
a.y = 2
let b: Vector2
b.x = 3
b.y = 4
let c: Vector2 = a + b  // calls op_+(a, b)
let d: Vector2 = a * 2.0  // calls op_*(a, 2.0)
```

**Supported operator overloads:**

| Operator | Function name | Type |
|----------|--------------|------|
| `+` | `op_+` | Binary |
| `-` | `op_-` | Binary |
| `*` | `op_*` | Binary |
| `/` | `op_/` | Binary |
| `%` | `op_%` | Binary |
| `==` | `op_==` | Binary |
| `!=` | `op_!=` | Binary |
| `<` | `op_<` | Binary |
| `>` | `op_>` | Binary |
| `<=` | `op_<=` | Binary |
| `>=` | `op_>=` | Binary |
| `&` | `op_&` | Binary |
| `\|` | `op_\|` | Binary |
| `^` | `op_^` | Binary |
| `<<` | `op_<<` | Binary |
| `>>` | `op_>>` | Binary |
| `-` (unary) | `op_-` | Unary |
| `!` (unary) | `op_!` | Unary |
| `~` (unary) | `op_~` | Unary |

**Custom operators:** Any unicode symbol can be used as a custom operator with infix syntax:

```aether
func op_⌛(a: int, b: int): int {
    return a + b
}

let result = 1 ⌛ 2  // calls op_⌛(1, 2)
```

---

## 10. Generics

### 10.1 Generic Functions

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

    public func push( item: T) {
        self.data[self.size] = item
        self.size += 1
    }

    public func pop(): T? {
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

---

## 13. Contract Programming

Contract programming allows you to specify **preconditions**, **postconditions**, and **struct/class contracts** that the compiler checks at runtime (debug builds) or uses as optimization hints (release builds). Contracts are written inline between the function signature and the body.

### 13.1 Preconditions and Postconditions

**`pre(expr)`** declares a condition that must be true **before** a function executes. **`post(expr)`** declares a condition that must be true **after** a function returns. The `old()` function inside a `post()` condition refers to the value of an expression before the function executed.

```aether
func withdraw(account: ref Account, amount: u64)
    pre(account.balance >= amount)
    post(account.balance == old(account.balance) - amount)
{
    account.balance -= amount
}
```

**Key rules:**
- `pre()` and `post()` are placed between the function signature and the opening `{`
- Multiple conditions are allowed — each gets its own `pre()` or `post()` line
- `old(expr)` is only valid inside `post()` conditions — it captures the value of `expr` at function entry
- In debug builds, violations trigger `assert()` failures
- In release builds, conditions are eliminated and used as optimizer hints

### 13.2 Struct/Class Contracts

**`contract(expr)`** declares a condition that must hold **before and after every method call** on a struct or class instance. Unlike `pre()`/`post()` which are per-function, contracts are per-type — they define the type's internal consistency contract.

```aether
class Queue<T> {
    data: [T]
    head: u64
    tail: u64

    contract(self.size <= self.data.len)

    func size(): u64 {
        return (self.tail - self.head) % self.data.len
    }
}
```

**Key rules:**
- `contract()` is declared inside a struct/class body, alongside fields and methods
- The condition is checked at every method entry and exit (in debug builds)
- Contracts are structural — they describe what makes an instance *valid*
- The compiler emits a `_check_contracts()` function that calls `assert()` for each condition

**`contract` vs `assert` — comparison:**

| Aspect | `assert(expr)` | `contract(expr)` |
|--------|---------------|------------------|
| **Scope** | Inside a function body | Inside a struct/class definition |
| **When checked** | At that exact line of code | Before/after every method call on the type |
| **Who guarantees** | The function author | The type itself (all methods collectively) |
| **Granularity** | Point-in-time, ad-hoc | Structural, cross-method |
| **Removed in release** | Yes (NDEBUG) | Yes, but could be kept for safety |
| **Use case** | "This should never happen here" | "This type is always in a valid state" |

**Rule of thumb:** Use `assert()` when you want to check a condition at a specific point in a function. Use `contract()` when you want to define what "valid" means for a type, and have every method automatically verify it.

### 13.3 Debug vs Release

In debug builds, all contracts (`pre`, `post`, `inv`) are checked at runtime via `assert()`. In release builds, they serve as optimizer hints and are eliminated — the compiler can use the information to prove properties about the code without generating runtime checks.

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

**Build modes:**
- **Debug** (`-O0` or no optimization flag): All contracts checked at runtime. Violations print a message and abort.
- **Release** (`-O2`, `-O3`): Contracts are parsed and stored in the AST but no runtime checks are emitted. The optimizer may use contract information for dead code elimination and range analysis.

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

- **Getter**: has a return type → `obj.prop()` calls the getter
- **Setter**: has no return type → `obj.prop(value)` calls the setter

Properties are defined as regular `func` methods inside struct/class bodies. The `self` parameter is auto-injected by the compiler — you never write it yourself.

```aether
struct Temperature {
    celsius: f64

    // Getter — has return type
    func fahrenheit(): f64 {
        return self.celsius * 9.0 / 5.0 + 32.0
    }

    // Setter — no return type
    func fahrenheit(value: f64) {
        self.celsius = (value - 32.0) * 5.0 / 9.0
    }
}

let t: Temperature
t.celsius = 100
print(t.fahrenheit())  // calls getter → 212.0
t.fahrenheit(32)    // calls setter → celsius = 0
```

> **Note:** `self` is a reserved keyword referencing the enclosing object. It is never passed as a parameter — the compiler injects it automatically. You only need `self` when a local variable or parameter name shadows a field name.

### 15.2 Operator Overloading

Operator overloads use explicit left/right parameters (C++ style). The `op_` prefix is reserved — it can only be used for operator overloading.

```aether
struct Vector2 {
    x: f64
    y: f64
}

func op_+(a: Vector2, b: Vector2): Vector2 {
    let result: Vector2
    result.x = a.x + b.x
    result.y = a.y + b.y
    return result
}

func op_-(a: Vector2, b: Vector2): Vector2 {
    let result: Vector2
    result.x = a.x - b.x
    result.y = a.y - b.y
    return result
}

func op_*(a: Vector2, scalar: f64): Vector2 {
    let result: Vector2
    result.x = a.x * scalar
    result.y = a.y * scalar
    return result
}

func op_==(a: Vector2, b: Vector2): bool {
    return a.x == b.x and a.y == b.y
}
```

---

## 16. Dynamic Dispatch

### 16.1 `dyn Trait` Syntax

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

### 17.3 Assembly with Output Bindings

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

### 17.4 Labels and Jumps

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

### 17.5 Multi-Architecture Assembly

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

### 17.6 Top-Level asm Blocks

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

### 18.7 Memory Layout Directives

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

### 21.1 Built-in Functions

The following functions are available without any import. They are emitted directly by the compiler:

| Function | Signature | Description |
|----------|-----------|-------------|
| `print` | `print(...)` | Print values to stdout (variadic, auto-converts types) |
| `sizeof` | `sizeof(T) -> u64` | Returns the size of type `T` in bytes |
| `alignof` | `alignof(T) -> u64` | Returns the alignment of type `T` in bytes |
| `offsetof` | `offsetof(T, field) -> u64` | Returns the byte offset of a struct field |
| `typeName` | `typeName(T) -> string` | Returns the string name of type `T` at compile time |
| `panic` | `panic(msg: string)` | Prints a message and aborts the program |

```aether
let size = sizeof(u64)           // 8
let align = alignof(u64)         // 8
let offset = offsetof(Point, y)  // byte offset of y field
let name = typeName(u64)         // "u64"
panic("unreachable code reached") // abort with message
```

> **Note:** `sizeof`, `alignof`, `offsetof`, and `typeName` are compile-time evaluable — they produce constant values that can be used in `const` declarations and `#run` blocks.

---

## 22. Build System

### 22.1 `aether` CLI

```
aether build    [--target=...]   # Compile source file
aether run      [--target=...]   # Build and run
aether asm      [--target=...]   # Show generated assembly
aether inspect  [binary]         # Inspect ELF metadata
aether init|new <name>           # Scaffold new project (planned)
aether fmt      [files...]       # Format source (planned)
aether doc      [files...]       # Generate documentation (planned)
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

## 24. Future & Aspirational Features

This section describes features that make Aether genuinely different from other systems languages. Many are aspirational — they represent the long-term vision.

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

### 24.9 Zero-Cost Error Context

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

### 24.10 Compile-Time Unit Checking

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

### 24.11 Automatic Interrupt Handler Generation

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

### 24.12 Automatic Boot Chain Generation

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

## 26. Module System and Imports

### 26.1 File as Module

Each `.ae` file is a module. The filename (without extension) is the module name.

### 26.2 Source Imports (`.ae` files)

```
import "math.ae"
import "io/file.ae"
```

Imports are resolved at compile time: the compiler reads the file, parses it with a shared arena, and merges declarations. Circular imports are detected. Two-pass semantic analysis (declare all names first, then visit bodies) handles forward references across files.

### 26.3 Library Imports (`.aelib` files)

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

### 26.4 Import Syntax

```
# All public decls from a library
import "test"

# Only public decls from a specific class/namespace
import "test" : Foo

# From multiple classes
import "test" : Foo, Bar

# Explicit .aelib file
import "test.aelib" : Foo

# Standard library (auto-resolved by compiler)
import "std/io"
```

### 26.5 Resolution Order

For `import "foo"` (no extension), the compiler tries in order:
1. `foo.ae` — source file (for code you own)
2. `foo.aelib` — pre-compiled library
3. `foo/lib.ae` or `foo/lib.aelib` — package directory
4. Standard library paths: `$AETHER_LIB`, `~/.aether/lib`, `/usr/local/lib/aether`

If none found, the compiler reports an error listing what was tried.

### 26.6 Public, Private, Internal Visibility

```
public func exported() { }          // visible to importers
private func internal_only() { } // only within this file
internal func package_only() { }  // within the same package
func default_public() { }        // public by default (for simplicity)
```

For `.aelib` libraries, visibility is enforced via metadata gating — `private` and `internal` declarations are omitted from the metadata entirely, so they literally cannot be called from outside the library.

### 26.7 Build Command

```
aether build --target lib test.ae -o test.aelib
```

The new `--target lib` target produces code + metadata in one `.aelib` file.

### 26.8 Linking Modes

When importing a `.aelib`, the consumer has two options:

- **Static linking** (default for `--target kernel`, `--target binary`): extract `.o` files from the archive, link them into the final binary. Everything in one executable — no runtime dependencies.
- **Dynamic linking** (for `--target host`): reference the `.aelib` as a shared library, runtime resolution via the OS dynamic linker. Smaller binaries, can update libraries without recompiling.

### 26.9 Multi-Arch Libraries

A `.aelib` can contain code for multiple architectures (x86_64, ARM64, RISC-V). The metadata is arch-agnostic — it references symbols by name. The linker picks the right arch at build time, like macOS fat dylibs.

### 26.10 Why Class/Namespace Imports, Not Function-Level

Function-level imports create ambiguity problems (multiple overloads with different signatures — which do you import?). Class-level is unambiguous: import the whole class, use `Foo.sayHello()` to call methods. This is how Python modules, Java classes, and C# namespaces work. The class is the natural unit of grouping.

### 26.11 Why `.aelib`, Not `.aeb`?

An ELF file with a metadata section (`.aeb`) is trivially reverse-engineerable with `objdump -d` — anyone can disassemble the code sections. An archive format (`.aelib`) requires writing a custom disassembler to extract the code sections AND understanding the archive layout. This matches how dylibs work on every platform (macOS, Linux, Windows) and is the natural choice for distributing compiled libraries without exposing source.

---

## Appendix A: Reserved Words

```
as, asm, break, case, catch, class, const, continue, default,
defer, do, dyn, elif, else, enum, export, extern, false, for,
func, heap, if, impl, import, in, init, drop, let, match, mod,
module, mut, none, not, or, and, owned, pool, post, pre, private,
protocol, public, ptr, rc, ref, region, return, self, static, struct,
super, sys, test, throw, trait, true, try, type, unsafe, use,
var, where, while, yield
```

## Appendix B: Lexical Rules

| Token | Rule |
|-------|------|
| Comment | `//` to end of line |
| Block comment | `/*` ... `*/` (nestable) |
| String | Double-quoted, escape sequences: `\n`, `\t`, `\\`, `\"`, `\xNN` |
| Char | Single-quoted: `'a'`, `'\n'`, `'\x41'` |
| Integer | Decimal: `42`, Hex: `0xFF`, Binary: `0b1010`, Octal: `0o77` |
| Float | `3.14`, `1e10`, `0xFF.0p-3` |
| Identifier | `[a-zA-Z_][a-zA-Z0-9_]*` |
| Indentation | Significant: blocks are indentation-based, 4 spaces per level |
| Terminator | Newline ends simple statements; expressions continue across lines |
| Semicolons | Optional — may separate statements on the same line |

## Appendix C: Build Profiles

Aether supports two build profiles that affect optimization, contract checking, and error handling:

| Feature | Debug (`-O0`) | Release (`-O2`/`-O3`) |
|---------|---------------|----------------------|
| Optimization | None (fast compile) | Full (constant folding, DCE, inlining) |
| Contract checks | `pre()`/`post()` checked at runtime | Eliminated (optimizer hints only) |
| Error context | Error messages preserved | Error discriminants only |
| Stack traces | Full source map | Minimal |
| Debug symbols | Included | Stripped |
| Build time | Fast | Slower (optimization passes) |

```bash
# Debug build (default)
aether build source.ae -o output

# Release build
aether build -O2 source.ae -o output

# Maximum optimization
aether build -O3 source.ae -o output
```

## Appendix D: Undefined Behavior

The following operations are **undefined behavior** — the compiler may produce any result, including crashes, incorrect output, or silent data corruption:

1. **Data races** — Concurrent read and write to the same memory location without synchronization
2. **Use-after-free** — Dereferencing a pointer after the memory has been freed
3. **Double free** — Calling `free()` on the same pointer twice
4. **Null pointer dereference** — Reading from address `0x0` (outside a try block)
5. **Uninitialized variable access** — Reading a `let` variable before it has been assigned
6. **Invalid pointer arithmetic** — Moving a pointer beyond the bounds of its allocation
7. **Dangling reference** — Returning a `ref` to a stack-local variable
8. **Signed integer overflow** — `i8`, `i16`, `i32`, `i64` arithmetic that exceeds the representable range (wraps silently)
9. **Shift overflow** — Shifting by an amount >= the bit width of the type
10. **Type punning through `as`** — Casting between incompatible types and reading the result
11. **Calling variadic function with wrong types** — Passing arguments that don't match the expected types
12. **Stack overflow** — Exceeding the available stack space (e.g., infinite recursion)

> **Note:** Aether does not have Rust-level safety guarantees. The compiler trusts the programmer. `unsafe` blocks explicitly opt into operations 1-7. Operations 8-9 are always wrapping (not UB in Aether, but the result may be unexpected). Use the standard library's checked arithmetic functions when you need overflow detection.

## Appendix E: Compiler Pipeline

The Aether compiler pipeline:

```
Source (.ae)
  → Tokenizer (whitespace-aware, indent engine)
    → Parser (Pratt + recursive descent)
      → AST (50+ node types)
        → Import Resolution (read, parse, merge imported files)
          → Semantic Analysis (type checking, name resolution)
            → Code Generation (NASM text output)
              → Multi-Target Assembler (x86_64/ARM64/RISC-V)
                → Binary Output (ELF64/Mach-O/PE32+/flat binary)
```

**Planned future pipeline phases** (not yet implemented):

```
            → HIR (High-level IR — type-aware, for semantic passes)
              → MIR (Mid-level IR — CFG with memory annotations)
                → Optimization (constant folding, DCE, inlining)
                  → LIR (Low-level IR — register allocation)
                    → Code Generation (from LIR, not AST)
```

**Comparison with other languages:**

| Phase | Aether | Swift | Zig |
|-------|--------|-------|-----|
| Tokenizer | Direct | Lexer | Tokenizer |
| Parser | Recursive descent | Recursive descent | Recursive descent |
| AST | 50+ node types | AST | AST |
| Import resolution | File-level | Module system | `@import` |
| Semantic analysis | Two-pass | Sema | ZIR → AIR |
| High-level IR | Planned | SIL (Swift IR) | ZIR (Zig IR) |
| Mid-level IR | Planned | SIL Optimizer | AIR (Analyzed IR) |
| Optimization | Partial | SIL + LLVM passes | LLVM passes |
| Low-level IR | Planned | LLVM IR | LLVM IR |
| Code generation | NASM text | LLVM IRGen | LLVM / native |
| Assembly | Multi-target | LLVM MC | LLVM MC |
| Binary output | ELF/Mach-O/PE | LLVM | LLVM / custom |

> **Note:** Aether's current pipeline skips intermediate IRs and goes directly from AST to NASM assembly text. This is by design for the bootstrap phase — it keeps the compiler simple and debuggable. As the language matures, HIR/MIR/LIR passes will be added for better optimization and code quality.

---

*End of Aether Language Specification v1.0*

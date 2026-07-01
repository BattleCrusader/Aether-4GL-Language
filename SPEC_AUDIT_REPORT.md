# Aether Compiler: SPECIFICATION.md Audit Report

**Date**: 2026-07-01
**Method**: Systematic comparison of SPECIFICATION.md sections against parser.c, c_transpiler/*.c, tests/fixtures/*.ae, and Makefile TEST_FIXTURES list.

**Legend**:
- ✅ **PARSED** — AST node or parsing logic exists in parser.c
- ✅ **C_TRANSPILER** — C codegen handles this in c_transpiler/*.c
- ✅ **TEST_EXISTS** — A test fixture .ae file exists in tests/fixtures/
- ✅ **IN_MAKEFILE** — The fixture is listed in Makefile TEST_FIXTURES (lines 124-174)
- ❌ **NOT_IMPLEMENTED** — No evidence of implementation
- ⚠️ **PARTIAL** — Partially implemented (details noted)

---

## Section 3: Lexical Structure

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 3.1 | Comments (//, /* */, ///) | ✅ | ✅ | ✅ | ✅ | test_asm_comment.ae |
| 3.2 | Identifiers | ✅ | ✅ | ✅ | ❌ | test_spec_03_identifiers.ae not in Makefile |
| 3.3 | Keywords | ✅ | ✅ | ✅ | ❌ | test_spec_03_keywords.ae not in Makefile |
| 3.4 | Operators (all) | ✅ | ✅ | ✅ | ❌ | 8 test_spec_03_operators_*.ae files, none in Makefile |
| 3.5 | Operator precedence | ✅ | ✅ | ✅ | ❌ | test_spec_03_operators_precedence.ae not in Makefile |
| 3.6 | Literals (int, float, string, char, bool, none) | ✅ | ✅ | ✅ | ❌ | test_spec_03_literals_*.ae not in Makefile |
| 3.7 | String interpolation | ✅ | ✅ | ✅ | ✅ | test_interp_*.ae (9 fixtures, all in Makefile) |
| 3.8 | Indentation | ✅ | ✅ | ✅ | ❌ | test_spec_03_indentation.ae not in Makefile |

---

## Section 4: Types

### 4.1 Primitive Types

| Type | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|
| void, bool, byte, u8-u64, i8-i64, f32, f64, string | ✅ | ✅ | ✅ | ✅ | test_types.ae, test_spec_04_primitives.ae |
| u128 | ✅ | ✅ | ✅ | ❌ | test_spec_04_primitives.ae not in Makefile |
| int, float, double, char (aliases) | ❌ | ❌ | ✅ | ❌ | **NOT IMPLEMENTED** — parser has no `int`/`float`/`double`/`char` keyword tokens. Spec says "defined in standard library" but no alias resolution exists. test_spec_04_type_aliases.ae exists but not in Makefile. |
| Integer overflow semantics | ✅ | ✅ | ✅ | ❌ | test_spec_04_overflow.ae not in Makefile |

### 4.2 Compound Types

| Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|
| Arrays `[T; N]` | ✅ | ✅ | ✅ | ✅ | test_types.ae |
| Dynamic arrays `[T]` | ✅ | ✅ | ✅ | ✅ | test_types.ae |
| Pointers `*T` | ✅ | ✅ | ✅ | ✅ | test_types.ae |
| References `ref T` | ✅ | ✅ | ✅ | ✅ | test_types.ae |
| Owned `owned T` | ✅ | ❌ | ❌ | ❌ | **PARSED** (NODE_TYPE_OWNED) but **NOT in C transpiler** |
| RC `rc T` | ✅ | ❌ | ❌ | ❌ | **PARSED** (NODE_TYPE_RC) but **NOT in C transpiler** |
| Optionals `T?` | ✅ | ✅ | ✅ | ✅ | test_optional.ae, test_spec_04_optionals.ae |
| Function types `func(T): R` | ✅ | ✅ | ✅ | ✅ | test_type_fn.ae in Makefile |
| Tuple types `(T1, T2)` | ✅ | ✅ | ✅ | ✅ | test_type_tuple.ae in Makefile |
| Type casting `as` | ✅ | ✅ | ✅ | ❌ | test_spec_04_type_casting.ae not in Makefile |
| Destructuring tuples | ✅ | ✅ | ❌ | ❌ | No dedicated test |

### 4.3 Type Aliases

| Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|
| `type Name = Type` | ✅ | ✅ | ✅ | ✅ | test_type_alias.ae in Makefile |
| `int` → `i64` alias | ❌ | ❌ | ✅ | ❌ | **NOT IMPLEMENTED** — no keyword or resolution |
| `float` → `f32` alias | ❌ | ❌ | ✅ | ❌ | **NOT IMPLEMENTED** |
| `double` → `f64` alias | ❌ | ❌ | ✅ | ❌ | **NOT IMPLEMENTED** |
| `char` → `byte` alias | ❌ | ❌ | ✅ | ❌ | **NOT IMPLEMENTED** |

### 4.4-4.8 Structs, Enums, Classes, Traits, Optionals

| Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|
| Structs | ✅ | ✅ | ✅ | ✅ | test_struct.ae, test_spec_04_structs.ae |
| Enums (tagged unions) | ✅ | ✅ | ✅ | ✅ | test_enum.ae, test_spec_04_enums.ae |
| Classes | ✅ | ✅ (as structs) | ✅ | ❌ | test_spec_04_classes.ae, test_oop.ae not in Makefile |
| Traits | ✅ | ✅ (as comments) | ✅ | ✅ | test_trait.ae, test_spec_04_traits.ae |
| Impl blocks | ✅ | ✅ (as comments) | ✅ | ✅ | test_trait.ae |
| Optionals | ✅ | ✅ | ✅ | ✅ | test_optional.ae, test_spec_04_optionals.ae |

---

## Section 5: Variables and Bindings

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 5.1 | Let bindings (explicit type) | ✅ | ✅ | ✅ | ✅ | test_types.ae |
| 5.1 | Type inference | ✅ | ✅ | ✅ | ✅ | test_type_infer.ae in Makefile |
| 5.1 | Mutability (`mut`) | ✅ | ✅ | ✅ | ✅ | test_variables.ae not in Makefile |
| 5.1 | Const declarations | ✅ | ✅ | ✅ | ✅ | test_const.ae in Makefile |
| 5.2 | Scope | ✅ | ✅ | ✅ | ❌ | Implicit in all tests |
| 5.3 | Shadowing | ✅ | ✅ | ❌ | ❌ | No dedicated test |
| 5.4 | Global variables | ✅ | ✅ | ✅ | ❌ | test_variables.ae not in Makefile |
| 5.5 | Recursion | ✅ | ✅ | ✅ | ❌ | test_functions.ae not in Makefile |
| **5.6** | **Loop labels** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** — no NODE_LABEL, no break/continue label support anywhere |

---

## Section 6: Functions

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 6.1 | Basic function decl | ✅ | ✅ | ✅ | ✅ | hello.ae, test_params.ae |
| 6.1 | Expression-bodied `->` | ✅ | ❌ | ❌ | ❌ | **PARSED** (line 594-602) but **NOT in C transpiler** — no NODE_EXPR_BODY case |
| 6.1 | Void return | ✅ | ✅ | ✅ | ✅ | hello.ae |
| 6.1 | Multiple returns via tuple | ✅ | ✅ | ✅ | ✅ | test_type_tuple.ae |
| 6.2 | Function attributes (@export, @entry, etc.) | ✅ | ❌ | ✅ | ✅ | test_export.ae, test_entry.ae in Makefile; C transpiler skips attributes |
| **6.3** | **Default parameters** | **✅** | **❌** | **❌** | **❌** | **PARSED** (line 646-655) but **NOT in C transpiler** — no default_value handling in c_func.c |
| 6.4 | Variadic functions | ✅ | ✅ | ✅ | ✅ | test_variadic.ae in Makefile |
| **6.5** | **Extern functions (FFI)** | **✅** | **❌** | **❌** | **❌** | **PARSED** (no body = extern) but **NOT in C transpiler** — no NODE_EXTERN case |

---

## Section 7: Control Flow

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 7.1 | If/elif/else | ✅ | ✅ | ✅ | ✅ | test_types.ae |
| 7.1 | If as expression | ✅ | ✅ | ✅ | ❌ | test_spec_* not in Makefile |
| 7.1 | If-let | ✅ | ✅ | ✅ | ✅ | test_iflet.ae in Makefile |
| 7.2 | While loops | ✅ | ✅ | ✅ | ✅ | test_math.ae |
| 7.3 | For loops (range) | ✅ | ✅ | ✅ | ✅ | test_math.ae |
| 7.3 | For loops (array) | ✅ | ✅ | ✅ | ❌ | test_for_index.ae not in Makefile |
| 7.3 | For with index | ✅ | ✅ | ✅ | ❌ | test_for_index.ae not in Makefile |
| 7.4 | Break/continue | ✅ | ✅ | ✅ | ✅ | test_math.ae |
| 7.5 | Match statement | ✅ | ✅ | ✅ | ✅ | test_match.ae in Makefile |
| 7.5 | Match as expression | ✅ | ✅ | ✅ | ❌ | test_match.ae covers this |
| 7.5 | Match range patterns | ✅ | ✅ | ✅ | ❌ | test_match_range.ae not in Makefile |
| 7.6 | Defer | ✅ | ✅ | ✅ | ✅ | test_defer.ae in Makefile |

---

## Section 8: Memory Management

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 8.1 | Stack allocation (default) | ✅ | ✅ | ✅ | ✅ | Implicit |
| **8.2** | **Escape analysis** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** — aspirational |
| 8.3 | Heap keyword | ✅ | ✅ | ✅ | ❌ | test_memory_management.ae not in Makefile |
| **8.4** | **Ownership (`owned T`)** | **✅** | **❌** | **❌** | **❌** | **PARSED** but **NOT in C transpiler** |
| **8.4** | **Borrowing (`ref T`)** | **✅** | **⚠️** | **✅** | **❌** | C transpiler maps ref to pointer but no borrow checking |
| **8.4** | **Reference-counted (`rc T`)** | **✅** | **❌** | **❌** | **❌** | **PARSED** but **NOT in C transpiler** |
| 8.5 | Region blocks | ✅ | ✅ | ✅ | ✅ | test_region.ae in Makefile |
| 8.6 | No null pointers (optionals) | ✅ | ✅ | ✅ | ✅ | test_optional.ae |
| 8.7 | Pointers/unsafe blocks | ✅ | ✅ | ✅ | ❌ | test_unsafe.ae not in Makefile |
| **8.8** | **Auto destructor insertion** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** — no drop tracking in C transpiler |

---

## Section 9: Object-Oriented Programming

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 9.1 | Classes | ✅ | ✅ (as structs) | ✅ | ❌ | test_oop.ae, test_spec_04_classes.ae not in Makefile |
| 9.2 | Implicit self | ✅ | ✅ | ✅ | ❌ | Auto-injected by parser |
| 9.3 | Classes not required | ✅ | ✅ | ✅ | ✅ | N/A — structs work |
| 9.4 | Traits (interfaces) | ✅ | ✅ (as comments) | ✅ | ✅ | test_trait.ae in Makefile |
| 9.5 | Access modifiers (pub/private/internal) | ✅ | ❌ | ❌ | ❌ | Parsed but C transpiler ignores visibility |
| **9.6** | **Properties** | **✅** | **⚠️** | **✅** | **❌** | **PARSED** (NODE_PROPERTY) but C transpiler emits `// property` comment only. test_properties_full.ae not in Makefile |
| **9.7** | **Operator overloading** | **✅** | **❌** | **✅** | **✅** | **PARSED** (op_* methods) but **NOT in C transpiler** — no special handling. test_op_overload.ae in Makefile but likely fails |

---

## Section 10: Generics

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 10.1 | Generic functions `<T>` | ✅ | ❌ | ✅ | ✅ | **PARSED** (type_params list) but **NOT in C transpiler** — no monomorphization. test_generic.ae, test_monomorph.ae in Makefile |
| 10.2 | Generic classes | ✅ | ❌ | ✅ | ❌ | test_generics.ae not in Makefile |
| 10.3 | Generic traits | ✅ | ❌ | ❌ | ❌ | Parsed but not transpiled |
| **10.4** | **Type constraints** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** |

---

## Section 11: Error Handling

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 11.1 | try/catch | ✅ | ✅ (setjmp/longjmp) | ✅ | ✅ | test_trycatch.ae in Makefile |
| 11.1 | throw | ✅ | ✅ | ✅ | ✅ | test_throw.ae in Makefile |
| 11.1 | throws annotation | ✅ | ❌ | ✅ | ✅ | test_errors.ae in Makefile |
| 11.2 | Custom error types (enums) | ✅ | ✅ | ✅ | ❌ | test_error_handling.ae not in Makefile |
| 11.3 | Error propagation | ✅ | ❌ | ✅ | ❌ | test_trycatch_nested.ae not in Makefile |
| 11.4 | Catch-all (`catch _`) | ✅ | ✅ | ✅ | ❌ | test_trycatch_catch_var.ae not in Makefile |
| 11.5 | Nested try/catch | ✅ | ✅ | ✅ | ❌ | test_trycatch_nested.ae not in Makefile |
| **11.1** | **finally blocks** | **✅** | **❌** | **✅** | **❌** | **PARSED** (line 1139-1146) but **NOT in C transpiler** — c_error.c has no finally handling. test_trycatch_finally.ae, test_trycatch_finally_throw.ae not in Makefile |
| 11.6 | Segfault handling | ✅ | ✅ | ✅ | ✅ | test_segfault.ae in Makefile |

---

## Section 12: Compile-Time Execution

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 12.1 | `#run` blocks | ✅ | ❌ | ✅ | ✅ | **PARSED** (NODE_RUN_BLOCK) but C transpiler skips it. test_comptime.ae in Makefile |
| 12.2 | Compile-time constants | ✅ | ✅ | ✅ | ✅ | test_const.ae in Makefile |
| **12.3** | **Compile-time reflection** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** |

---

## Section 13: Contract Programming

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 13.1 | Preconditions `pre()` | ✅ | ✅ | ✅ | ✅ | test_contract.ae in Makefile. c_contract.c handles both |
| 13.1 | Postconditions `post()` | ✅ | ✅ | ✅ | ✅ | test_contract.ae in Makefile |
| **13.2** | **Invariants `inv()`** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** |
| **13.3** | **Debug vs release elimination** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** — contracts always emitted |

---

## Section 14: Closures and Lambdas

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 14.1 | Lambda syntax `\|x\| -> expr` | ✅ | ⚠️ | ✅ | ✅ | **PARSED** (NODE_LAMBDA). C transpiler emits body expression only — no closure struct. test_closure.ae in Makefile |
| **14.2** | **Closures with captures** | **✅** | **❌** | **✅** | **✅** | **PARSED** but **NOT in C transpiler** — no capture environment. test_closure.ae in Makefile |
| **14.3** | **Higher-order functions** | **✅** | **❌** | **❌** | **❌** | **PARSED** but **NOT in C transpiler** |

---

## Section 15: Properties and Operator Overloading

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 15.1 | Properties (getter/setter) | ✅ | ⚠️ | ✅ | ❌ | **PARSED** (NODE_PROPERTY) but C transpiler emits `// property` comment. test_properties_full.ae not in Makefile |
| 15.2 | Operator overloading (op_add, etc.) | ✅ | ❌ | ✅ | ✅ | **PARSED** (as regular methods) but **NOT in C transpiler** — no op_* dispatch. test_op_overload.ae in Makefile |

---

## Section 16: Dynamic Dispatch

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 16.1 | `dyn Trait` syntax | ✅ | ❌ | ✅ | ✅ | **PARSED** (NODE_TYPE_REF with dyn) but **NOT in C transpiler** — no vtable generation. test_dyn.ae in Makefile |
| 16.2 | Static vs dynamic dispatch | ✅ | ❌ | ✅ | ✅ | test_dyn.ae in Makefile |

---

## Section 17: Inline Assembly

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 17.1-17.7 | asm blocks | ✅ | ❌ | ✅ | ✅ | **PARSED** (NODE_ASM_BLOCK) but C transpiler skips as NASM-specific. test_asm_comment.ae, test_top_level_asm.ae in Makefile |

---

## Section 18: Aether OS Integration

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 18.3 | `sys func` (syscall page) | ✅ | ❌ | ✅ | ✅ | **PARSED** (is_sys flag) but **NOT in C transpiler**. test_sysfunc.ae in Makefile |
| 18.4 | Module declarations | ✅ | ✅ (as comments) | ✅ | ✅ | test_module_abi.ae in Makefile |
| 18.5 | File imports | ✅ | ❌ | ✅ | ✅ | **PARSED** (NODE_IMPORT) and resolved at compile time, but **NOT in C transpiler**. test_import.ae, test_aelib_import.ae in Makefile |
| 18.6 | Binary entry points | ✅ | ❌ | ✅ | ✅ | test_entry.ae in Makefile |
| 18.7 | Memory layout directives | ✅ | ❌ | ✅ | ❌ | test_layout.ae in LAYOUT_FIXTURES |
| 18.8 | Pool declarations | ✅ | ✅ (as comments) | ❌ | ❌ | Parsed but C transpiler emits `; pool declaration (reserved)` |
| 18.9 | Protocol declarations | ✅ | ✅ (as comments) | ❌ | ❌ | Parsed but C transpiler emits `; protocol declaration (interface)` |

---

## Section 19: Multi-Target Assembler

**Skipped per instructions** — NASM-specific feature. The asm_backend_*.c files exist for x86_64, ARM64, and RISC-V.

---

## Section 20: Universal Binaries

**Skipped per instructions** — NASM-specific feature. universal.c exists.

---

## Section 21: Standard Library

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 21.1 | `print` | ✅ | ✅ | ✅ | ✅ | Built-in via runtime |
| **21.1** | **`sizeof(T)`** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** — no special handling in parser or C transpiler |
| **21.1** | **`alignof(T)`** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** |
| **21.1** | **`offsetof(T, field)`** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** |
| **21.1** | **`typeName(T)`** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** |
| **21.1** | **`panic(msg)`** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** |
| 21.1 | std lib modules (std/io, std/mem, etc.) | ✅ | ✅ | ✅ | ✅ | LIBAETHER_SRCS in Makefile, test_std_test.ae in Makefile |

---

## Section 22: Build System

**Skipped** — CLI/config, not a language feature.

---

## Section 23: Compiler Targets

**Skipped** — CLI/config, not a language feature.

---

## Section 24: Future & Aspirational Features

**Skipped per instructions**.

---

## Section 25: Concurrency and Fibers

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| **25.1** | **spawn** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** |
| **25.2** | **Mutex** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** |
| **25.3** | **Channels** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** |
| **25.4** | **Fiber scheduler** | **❌** | **❌** | **❌** | **❌** | **NOT IMPLEMENTED** |

---

## Section 26: Module System and Imports

| Subsection | Feature | PARSED | C_TRANSPILER | TEST_EXISTS | IN_MAKEFILE | Notes |
|---|---|---|---|---|---|---|
| 26.1-26.2 | Source imports (.ae) | ✅ | ❌ | ✅ | ✅ | **PARSED** (NODE_IMPORT) and resolved at compile time. test_import.ae in Makefile |
| 26.3-26.11 | Library imports (.aelib) | ✅ | ❌ | ✅ | ✅ | aelib.c handles archive format. test_aelib_import.ae in Makefile |
| 26.6 | pub/private/internal visibility | ✅ | ❌ | ❌ | ❌ | Parsed but not enforced in C transpiler |

---

## Summary of NOT IMPLEMENTED Features

### Critical gaps (specified but no implementation found):

| # | Feature | Spec Section | Impact |
|---|---|---|---|
| 1 | **Loop labels** (`break outer`, `continue outer`) | §5.6 | Control flow limitation |
| 2 | **Expression-bodied functions** (`->`) in C transpiler | §6.1 | Parsed but not transpiled |
| 3 | **Default parameters** in C transpiler | §6.3 | Parsed but not transpiled |
| 4 | **Extern functions (FFI)** in C transpiler | §6.5 | Parsed but not transpiled |
| 5 | **`int`/`float`/`double`/`char` type aliases** | §4.3 | No keyword tokens or resolution |
| 6 | **finally blocks** in C transpiler | §11.1 | Parsed but not transpiled |
| 7 | **Operator overloading** (op_add dispatch) in C transpiler | §9.7, §15.2 | Parsed but not transpiled |
| 8 | **Properties** (getter/setter dispatch) in C transpiler | §9.6, §15.1 | Parsed but emits comment only |
| 9 | **Generics monomorphization** in C transpiler | §10 | Parsed but not transpiled |
| 10 | **Closure captures** in C transpiler | §14.2 | Parsed but not transpiled |
| 11 | **Dynamic dispatch** (`dyn Trait` vtable) in C transpiler | §16 | Parsed but not transpiled |
| 12 | **Ownership/borrowing/RC** in C transpiler | §8.4 | Parsed but not transpiled |
| 13 | **Auto destructor insertion** | §8.8 | Not implemented |
| 14 | **Built-in functions** (sizeof, alignof, offsetof, typeName, panic) | §21.1 | Not implemented |
| 15 | **Escape analysis** | §8.2 | Aspirational |
| 16 | **Type constraints** on generics | §10.4 | Not implemented |
| 17 | **Invariants** (`inv()`) | §13.2 | Not implemented |
| 18 | **Debug vs release contract elimination** | §13.3 | Not implemented |
| 19 | **Compile-time reflection** | §12.3 | Not implemented |
| 20 | **Concurrency** (spawn, mutex, channels, fibers) | §25 | Not implemented |

### Test fixtures NOT in Makefile TEST_FIXTURES (exist but not run by `make test-host`):

All ~30 `test_spec_*.ae` files, plus: test_variables.ae, test_unsafe.ae, test_trycatch_nested.ae, test_trycatch_finally.ae, test_trycatch_finally_throw.ae, test_trycatch_catch_var.ae, test_top_level_asm.ae, test_str.ae, test_serial.ae, test_properties_full.ae, test_or_else.ae, test_oop.ae, test_monomorph.ae, test_memory_management.ae, test_mem.ae, test_match_range.ae, test_lib.ae, test_io.ae, test_generics.ae, test_functions.ae, test_fs.ae, test_for_index.ae, test_fixtures.ae, test_error_handling.ae

---

## Key Findings

1. **Parser is comprehensive**: ~50+ AST node types, covering most of the spec. The parser handles nearly everything described.

2. **C transpiler has significant gaps**: While it handles basic constructs (let, if, while, for, match, try/throw, structs, enums, basic lambdas), it lacks support for: generics, closures with captures, operator overloading, properties, dynamic dispatch, extern functions, default parameters, expression-bodied functions, finally blocks, ownership types, and built-in functions.

3. **Test coverage is uneven**: 50 .ae fixtures exist but only ~50 are in the Makefile TEST_FIXTURES list. The `test_spec_*` files (~30) are comprehensive but not wired into the test runner.

4. **The "Missing Pieces" doc is outdated**: It lists features as "COMPLETED" that the C transpiler doesn't actually handle (operator overloading, generics, closures with captures, dynamic dispatch, properties).

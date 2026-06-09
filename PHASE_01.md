# Phase 01 — Core Language (Minimum Viable Compiler)

**Goal**: Transform the Phase 0 bootstrap compiler into a working compiler that can produce correct, functional binaries for programs using variables, control flow, functions, structs, enums, arrays, strings, inline NASM, and the type system. The output must be a verifiably correct ELF64 binary.

**Strategy**: Each milestone produces a working test case. Extend the compiler pipeline (parser → codegen) for each feature, keeping tests green.

---

## Task Breakdown

### P01.01 — Codegen: Proper Variable Stack Slots (`P01.01`) 🟢
- [x] Track variable stack offsets in codegen (compute_frame collects let decls)
- [x] Function prologue: calculate total stack frame size from let declarations
- [x] Stack frame layout: account for all local vars with proper offsets
- [x] Variable access codegen: read/write from correct `[rbp - offset]`
- [x] `let` without initializer: zero-fill the stack slot
- [x] `let` with initializer: evaluate expression, store to stack slot
- [x] Mutable variable assignment: `x = expr` generates correct store ***(BIN_ASSIGN implemented in P01.11)***
- [x] **MILESTONE**: `let x = 42; let y = 10; return x + y` produces correct math

### P01.02 — Codegen: Full Type Support (`P01.02`) 🟢
- [x] u8/u16/u32/u64 sizes in stack allocation (actual_size tracked per VarSlot)
- [x] i8/i16/i32/i64 signed operations (extend correctly via cg_load_var/cg_store_var)
- [x] bool as `byte` (0/1)
- [ ] f32/f64 support (basic mov/movss/movsd) ***(deferred to Phase 7)***
- [x] `byte` type (alias for u8)
- [x] Type size calculation utility function (type_size)
- [x] **MILESTONE**: Variables of all primitive types allocate correct stack sizes

### P01.03 — Codegen: Structs and Field Access (`P01.03`) 🟢
- [x] Struct declaration → emit struct as aligned stack allocation
- [x] Field access codegen: `s.x` → `[rbp - offset + field_offset]`
- [ ] Nested struct field access ***(deferred)***
- [ ] Struct literal construction: `Point(3, 4)` → store each field ***(deferred)***
- [ ] Struct parameter passing (via stack, aligned) ***(deferred)***
- [x] **MILESTONE**: `struct Point { x: int; y: int }` works with field access

### P01.04 — Codegen: Arrays and Indexing (`P01.04`) 🟢
- [x] Fixed-size array allocation: `[T; N]` → N * sizeof(T) on stack
- [x] Array indexing: `a[i]` → compute offset, load from `[rax + rcx*8]`
- [ ] Array literal: `[1, 2, 3]` → initialize each element ***(stub returns 0)***
- [ ] Slice access: `a[i..j]` → fat pointer (ptr + len) ***(stub returns 0)***
- [ ] Bounds checking (optional, debug builds) ***(deferred)***
- [x] **MILESTONE**: Array indexing codegen added

### P01.05 — Codegen: String Literals (`P01.05`) 🟢
- [x] String table in `.rodata` section with unique labels
- [x] String literal references: `"hello"` → lea to `.rodata` label
- [x] Escape sequence processing in string output (NASM db format)
- [ ] String concatenation (basic `"a" + "b"`) ***(deferred)***
- [x] **MILESTONE**: `print("hello")` generates LEA to .rodata label

### P01.06 — Codegen: Inline NASM (`P01.06`) 🟢
- [x] `asm { ... }` block → skip tokens counting brace depth, emit NODE_ASM_BLOCK
- [ ] Variable binding in asm: `asm -> (out_var) { ... }`  ***(deferred)***
- [ ] Input binding: use variable names in asm expressions ***(deferred)***
- [x] **MILESTONE**: asm blocks are parsed and produce AST nodes

### P01.07 — Codegen: Function Calls with Arguments (`P01.07`) 🟢
- [x] Parameter passing: SysV ABI (rdi, rsi, rdx, rcx, r8, r9 for first 6 args)
- [x] Return value handling (rax)
- [x] Stack cleanup after call
- [x] Recursive function calls (stack frames nest correctly — verified with test_params.ae)
- [x] **MILESTONE**: `func add(a: i64, b: i64): i64 { return a + b }` compiles correctly

### P01.08 — Codegen: For Loops and Ranges (`P01.08`) 🟢
- [x] `for i in 0..10` → counter init (rcx), loop check (cmp/jge), increment (inc)
- [ ] `for i in expr..expr` → dynamic range bounds ***(deferred)***
- [ ] `for item in array` → iteration over array elements ***(deferred)***
- [x] **MILESTONE**: `for i in 0..10 { s = s + i }` produces correct loop

### P01.09 — Codegen: Match Statements (`P01.09`) 🟢
- [x] Integer literal pattern matching: chained cmp/jne per arm
- [ ] String pattern matching ***(deferred)***
- [ ] Range patterns: `case 0..10` ***(deferred)***
- [x] Wildcard: `case _` → default (last arm without comparison)
- [ ] Multi-condition: `case 0 | 1 | 2` ***(deferred)***
- [x] **MILESTONE**: `match x { case 0 -> 10 case _ -> 0 }` works

### P01.10 — Codegen: Enums with Payloads (`P01.10`) 🟢
- [x] Enum as tagged union: discriminant (u64) + max variant payload
- [x] Enum size tracking: `total_size = 8 + max_payload`
- [x] Variant-to-discriminant mapping (VariantEntry list)
- [x] `::` token (TOKEN_COLON_COLON) in tokenizer
- [x] Qualified name parsing: `EnumName::Variant` in parser infix handler
- [x] Semantic analysis: enum/struct names registered in scope
- [x] Enum construction codegen: discriminant emitted to stack slot
- [ ] Pattern matching with payload extraction ***(deferred)***
- [ ] Full payload storage during construction ***(deferred — currently only discriminant)***
- [x] **MILESTONE**: `let x: Option = Option::Some(42)` compiles

### P01.11 — Full Expression Coverage (`P01.11`) 🟢
- [x] Unary negation: `-x`
- [x] Logical not: `!x`
- [x] Bitwise ops: `&`, `|`, `^`, `~`, `<<`, `>>` (and/or/xor/shl/shr)
- [x] Assignment: `x = expr` stores to variable's stack slot
- [x] Compound assignment: `+=`, `-=`, `*=`, `/=`
- [x] Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- [x] Short-circuit: `&&` and `||` (emulated via conditional set)
- [x] **MILESTONE**: All expression types produce correct assembly

### P01.12 — Error Handling in Codegen (`P01.12`) 🟢
- [x] Report warning for unsupported constructs with file:line:col via cg_warn()
- [x] Graceful recovery: emit zero value and continue to next stmt
- [x] cg_error() for hard errors (unused, available for future use)
- [x] **MILESTONE**: Unsupported constructs produce clear warning messages

### P01.13 — Self-Host Test Suite Expansion (`P01.13`) 🟢
- [x] 7 `.ae` test programs compiling with `aether build`:
  - `hello.ae` — basic function + return
  - `test_math.ae` — variables + arithmetic
  - `test_types.ae` — typed vars (u8/u16/u32/u64)
  - `test_params.ae` — function calls with SysV ABI
  - `test_struct.ae` — struct declaration + allocation
  - `test_enum.ae` — enum declaration + construction via `::`
  - `test_match.ae` — match expression with integer patterns + wildcard
- [x] Each test compiles to valid ELF64
- [x] **MILESTONE**: 7+ .ae test programs compiling correctly

### P01.14 — Phase 1 Verification (`P01.14`) 🟢
- [x] Full `make clean && make test` from clean checkout — **28/28 tests passing**
- [x] All Phase 0 tests still pass (14/14 tokenizer + 14/14 parser)
- [x] All Phase 1 .ae fixtures compile to valid ELF64
- [x] `aether build` produces x86_64 ELF64 binary with correct entry point
- [x] 7 end-to-end .ae → .elf compilations verified
- [x] Update STATUS.md — lock Phase 1 as complete
- [x] PHASE_02.md created

---

## Legend

| Status | Meaning |
|--------|---------|
| 🟢 COMPLETE | Done and verified |
| 🔵 IN PROGRESS | Currently being worked on |
| 🟡 HOLD | Blocked, waiting on something else |
| 🔴 NOT STARTED | Planned but not started |
| ⚪ CANCELLED | No longer planned |

## Known Gaps (deferred from Phase 1)

| Feature | Reason |
|---------|--------|
| f32/f64 codegen | Register moves differ (movss/movsd); low priority |
| Nested struct fields | Recursive struct layout not implemented |
| Struct literal construction | `Point(3, 4)` syntax not wired to codegen |
| Array literal codegen | `[1, 2, 3]` returns 0 at runtime |
| String concatenation | `"a" + "b"` not implemented |
| Inline asm variable binding | `asm -> (out) { ... }` not implemented |
| Enum payload storage | Construction sets discriminant only, not payload bytes |
| Match payload extraction | Pattern `Some(x)` can't yet bind payload |
| Bounds checking | Deferred to optimization phase |
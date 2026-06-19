# Phase 05 — Advanced Language Features

**Goal:** Add advanced language features to Aether — exception handling, compile-time execution, contract programming, closures, properties, operator overloading, generics monomorphization, dynamic dispatch, and access modifier enforcement.

**Strategy:** Each milestone adds one feature, building from the most impactful (exceptions) through compile-time metaprogramming to closures and dispatch.

---

## Task Breakdown

### P05.01 — Exception Handling: `try`/`throw`/`catch` Parsing 🟢
- [x] Tokenizer: ensure `try`, `throw`, `catch`, `throws` keywords are recognized 🟢
- [x] Parser: `try { body } catch Type { handler }` block parsing 🟢
- [x] Parser: `throw expr` statement parsing 🟢
- [x] Parser: `throws` return type annotation on function declarations 🟢
- [x] AST: `NODE_TRY`, `NODE_THROW`, `NODE_CATCH` node types 🟢
- [x] AST: `TryNode` with body, catch arms; `ThrowNode` with value 🟢
- [x] Semantic: validate throw values are error types 🟢
- [x] Codegen: deterministic exception convention (rdx=0 success, rdx=1 error) 🟢
- [x] Codegen: `throw` sets error tag, emits defers, returns 🟢
- [x] Codegen: `try` clears rdx, runs body, checks rdx, branches to catch 🟢
- [x] Codegen: `throws` functions clear rdx on normal return 🟢
- [x] Test fixture: `test_trycatch.ae` — basic try/catch compiles and returns 42 🟢
- [x] Test fixture: `test_throw.ae` — throw and catch, verify error propagation (returns 42) 🟢
- [x] Fixed assignment codegen bug (left evaluation overwriting right side) 🟢

### P05.02 — Custom Error Types 🟢
- [x] Parser: error type declarations (enum-based, `error` keyword or convention) 🟢
- [x] Semantic: error type hierarchy (sub-errors, cause chaining) 🟢
- [x] Codegen: error type layout (tagged union with discriminant + payload) 🟢
- [x] Codegen: discriminant matching in catch arms (type-specific catch) 🟢
- [x] Test fixture: `test_errors.ae` — custom error types with enum variants, discriminant matching returns 42 🟢

### P05.03 — Deterministic Exception Codegen 🟢
- [x] Codegen: `throws T` functions return `(T, ErrorCode?)` tagged pair (rdx=0/1) 🟢
- [x] Codegen: `throw` sets error tag, returns early 🟢
- [x] Codegen: `try` checks error tag, branches to catch or continues 🟢
- [x] Codegen: no personality routines, no unwinding tables 🟢
- [x] Codegen: destructor calls on exception unwind (integrate with AutoDrop) 🟢
- [x] Test fixture: `test_throw.ae` — throw and catch, verify error propagation 🟢

### P05.04 — Zero-Cost Happy Path 🟢
- [x] Codegen: happy path has no branch or tag check overhead (just `xor rdx, rdx` at function entry) 🟢
- [x] Codegen: error path uses separate basic block 🟢
- [x] Optimization: inline error check only when catch is present 🟢
- [x] Test: verify no extra instructions on happy path (try without throw has no overhead) 🟢

### P05.05 — Compile-Time Execution: `#run` Blocks
- [ ] Tokenizer: `#run` directive token
- [ ] Parser: `#run { body }` block parsing
- [ ] AST: `NODE_RUN_BLOCK` node type
- [ ] Semantic: validate `#run` body contains only compile-time evaluable expressions
- [ ] Interpreter: simple compile-time expression evaluator (const folding, basic ops)
- [ ] Codegen: `#run` results embedded as data in the binary
- [ ] Test fixture: `test_comptime.ae` — `#run` with constant computation

### P05.06 — Compile-Time Constant Evaluation
- [ ] Semantic: constant folding for arithmetic, string concat, type sizes
- [ ] Semantic: `const` expressions evaluated at compile time
- [ ] Semantic: compile-time `sizeof()`, `alignof()`, `offsetof()`
- [ ] Test fixture: `test_const.ae` — compile-time constants used in array sizes

### P05.07 — Contract Programming: `pre`/`post`
- [ ] Tokenizer: `pre`, `post` keywords
- [ ] Parser: `pre(expr)` and `post(expr)` annotations on function declarations
- [ ] AST: `ContractNode` with pre/post expression lists
- [ ] Semantic: validate contract expressions are boolean
- [ ] Codegen (debug): emit contract checks at function entry/exit
- [ ] Test fixture: `test_contract.ae` — pre/post conditions compile and check

### P05.08 — Debug-Build Runtime Contract Checking
- [ ] Codegen: `--debug` flag enables contract assertions
- [ ] Codegen: contract violation calls panic with source location
- [ ] Runtime: `__aether_panic` helper for contract failures
- [ ] Test: verify contract violation panics in debug mode

### P05.09 — Release-Build Contract Elimination
- [ ] Codegen: `--release` flag eliminates all contract checks
- [ ] Codegen: contracts become optimizer hints (const propagation, bounds elision)
- [ ] Test: verify zero-cost in release mode

### P05.10 — Closures and Lambdas: `|args| expr`
- [ ] Tokenizer: `|` as lambda delimiter (TOKEN_PIPE_LAMBDA)
- [ ] Parser: `|params| expr` and `|params| { body }` lambda parsing
- [ ] AST: `NODE_LAMBDA` with captures list
- [ ] Semantic: capture analysis (by value, by ref, by move)
- [ ] Codegen: closure struct (captured vars + function pointer)
- [ ] Codegen: non-capturing lambda → plain function pointer
- [ ] Codegen: capturing lambda → closure struct on stack or heap
- [ ] Test fixture: `test_closure.ae` — map/filter with lambdas

### P05.11 — Properties: `get`/`set` Sugar
- [ ] Parser: `prop name(self): type { get { } set { } }` inside struct/class
- [ ] AST: `NODE_PROPERTY` node type
- [ ] Semantic: property access desugars to getter/setter calls
- [ ] Codegen: `obj.prop` → `obj.prop_get()`, `obj.prop = val` → `obj.prop_set(val)`
- [ ] Test fixture: `test_property.ae` — property with validation in setter

### P05.12 — Operator Overloading
- [ ] Parser: `func op_add(self, other): type { }` inside struct/class
- [ ] Tokenizer: operator method names (`op_add`, `op_sub`, `op_mul`, etc.)
- [ ] Semantic: `a + b` on class types → `a.op_add(b)` call
- [ ] Codegen: operator calls desugar to method calls
- [ ] Supported operators: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&`, `|`, `^`, `<<`, `>>`, `[]`, `!`, `~`
- [ ] Test fixture: `test_op_overload.ae` — custom vector math with operators

### P05.13 — Generics Monomorphization
- [ ] Semantic: collect all concrete type instantiations of generic functions/structs
- [ ] Semantic: duplicate AST per concrete type with type params substituted
- [ ] Codegen: emit separate function/struct per monomorphized instance
- [ ] Codegen: name mangling for monomorphized symbols (`identity__int`, `identity__float`)
- [ ] Optimization: deduplicate identical monomorphizations
- [ ] Test fixture: `test_monomorph.ae` — generic used with 3+ types

### P05.14 — Dynamic Dispatch (`dyn Trait`)
- [ ] Semantic: `dyn Trait` type → fat pointer `{ *data, *vtable }`
- [ ] Codegen: vtable generation per `impl Trait for Type`
- [ ] Codegen: vtable layout (function pointers in trait declaration order)
- [ ] Codegen: method call through vtable index
- [ ] Codegen: `as dyn Trait` coercion (produce fat pointer)
- [ ] Test fixture: `test_dyn.ae` — heterogeneous collection via `dyn`

### P05.15 — Access Modifier Enforcement
- [ ] Semantic: `pub` symbols accessible across modules
- [ ] Semantic: `private` symbols accessible only within current module
- [ ] Semantic: `internal` symbols accessible within package
- [ ] Semantic: error on access violation
- [ ] Test fixture: `test_access_enforce.ae` — verify private access rejected

### P05.16 — Phase 5 Verification
- [ ] `make clean && make test` — all unit tests passing
- [ ] `make test-host` — all host-native fixtures passing
- [ ] Freestanding ELF64 still works
- [ ] Updated STATUS.md — Phase 5 complete
- [ ] Updated PHASE_05.md with final status
- [ ] **MILESTONE**: Phase 5 verified

---

## Technical Notes

### Exception strategy
- `throws T` compiles to `(T, ErrorCode?)` — a tagged union return
- The happy path returns the value with a `0` tag (no error)
- The error path sets the tag to `1` and returns the error payload
- `try` checks the tag; if `1`, branches to the matching `catch` arm
- Destructors for in-scope class instances are called before the catch handler
- No stack unwinding metadata, no personality routines, no runtime type lookups

### Compile-time execution
- `#run` blocks execute during semantic analysis
- A simple interpreter evaluates constant expressions
- Results are embedded as data in the binary (`.rodata` section)
- `#run` can emit declarations via `emit()` function
- Compile-time reflection: `reflect(T).fields()`, `sizeof(T)`, `alignof(T)`

### Closure layout
```
Non-capturing: function pointer only (8 bytes)
Capturing:     { function_ptr: *u8, env: [captured vars...] }
               (function pointer + captured data, passed as first arg)
```

### Vtable layout (dynamic dispatch)
```
dyn Trait = { data: *u8, vtable: *u8 }
vtable = [method0_ptr, method1_ptr, ...]  (in trait declaration order)
Method call: vtable[index](data, args...)
```

### Monomorphization
- Generic function `identity<T>` with calls `identity(42)` and `identity(3.14)` generates:
  - `identity__int` (T → int)
  - `identity__float` (T → float)
- Each is a separate AST copy with type params substituted
- No runtime type information, no type erasure
- Binary size grows with number of unique instantiations

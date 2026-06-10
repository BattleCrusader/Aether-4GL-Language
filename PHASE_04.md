# Phase 04 — OOP and Type System

**Goal:** Add object-oriented programming to Aether — classes with constructors/destructors, methods, inheritance, traits/interfaces, generics, closures, and pattern binding.

**Strategy:** Each milestone adds one OOP concept, building from simplest syntactic sugar (methods on structs) through full class semantics to generics and traits.

---

## Task Breakdown

### P04.01 — Methods on Structs (`P04.01`) 🟢
- [x] Parser: `func method_name(self, ...)` inside `struct { }` block via TOKEN_KW_FUNC detection
- [x] Parser: `self` keyword → ident expression (TOKEN_KW_SELF in parse_prefix)
- [x] Parser: `self` accepted as parameter name
- [x] Methods stored in `StructDecl.methods` list
- [x] Test fixture: `test_method.ae` — struct Point with sum(self) method, self.x + self.y

### P04.02 — Classes with `init` and `drop` (`P04.02`) 🟢
- [x] `class` keyword parses as struct-like declaration → `NODE_CLASS_DECL`
- [x] Flagged identically to struct in semantic, codegen, and dump
- [x] Test: `test_class.ae` — class Buffer with fields, compiles

### P04.03 — Automatic Destructor Insertion (`P04.03`) 🟢
- [x] AutoDrop linked list on Codegen struct tracks class-typed variables
- [x] Default `%s_drop` stubs (ret-only) emitted at compile time for all classes
- [x] Auto-drop calls emitted alongside defers at scope exit (both return paths)
- [x] Runtime helpers moved before user functions to fix NASM forward-ref offset shifts
- [x] Allocator labels renamed to LA_ prefix to avoid NASM local-label conflicts
- [x] Test: `test_class.ae` — class Buffer with auto-drop, compiles and runs

### P04.04 — Access Modifiers (`P04.04`) 🟢 (parsing only — enforcement deferred to Phase 5)
- [x] Parser: `pub`, `private`, `internal` before declarations → `AccessLevel` enum
- [x] Tokenizer: `TOKEN_KW_PRIVATE`, `TOKEN_KW_INTERNAL` added
- [x] FuncDecl stores `AccessLevel access` field
- [x] Test: `test_access.ae` — private/pub/internal functions parse and compile
- [ ] Semantic enforcement (module boundary checking deferred — Phase 5)

### P04.05 — Traits and Impl (`P04.05`) 🟢
- [x] Parser: `trait Name { method signatures }` → `TRAIT_DECL`
- [x] Parser: `impl Trait for Type { method bodies }` → `IMPL_BLOCK`
- [x] AST: `TraitDecl` and `ImplBlock` structs in tagged union
- [x] Dump: type name strings for both node types
- [x] Test: `test_trait.ae` — trait Hashable + impl for Point, compiles and runs

### P04.06 — Generics (`P04.06`) 🟢
- [x] Parser: `func Name<T, U>(params)` angle-bracket syntax for type parameters
- [x] AST: `FuncDecl.type_params` stores type param identifiers
- [x] Test: `test_generic.ae` — `identity<T>` parses, compiles, runs
- [ ] Monomorphization: duplicate code per concrete type (Phase 5)

### P04.07 — `if let` Pattern Binding (`P04.07`) 🟢
- [x] Parser: `if let pattern = expr { body }` statement
- [x] AST: `IfNode.is_if_let` flag + `if_let_pattern` field
- [x] Semantic: register pattern variable in body scope
- [x] Codegen: check optional tag byte, skip body if none
- [x] Test: `test_iflet.ae` — `if let val = optional { return val }` with none → fallback 42

### P04.08 — Phase 4 Verification (`P04.08`) 🟢
- [x] `make clean && make test` — 16/16 + 14/14 passing
- [x] `make test-host` — 13/13 host-native fixtures passing
- [x] Freestanding ELF64 still works (hello.ae → valid ELF)
- [x] Updated STATUS.md — Phase 4 complete
- [x] Updated PHASE_04.md with final status
- [x] **MILESTONE**: Phase 4 verified

---

## Technical Notes

### Method calling convention
- `obj.method(args)` desugars to `ClassName::method(ref obj, args)`
- `self` is always the first parameter, passed as a `ref` in rdi
- `static func` methods have no `self` parameter

### Class storage
- Classes ARE structs internally — no separate storage format in Phase 4
- `init` is called after the struct is allocated (stack or heap)
- `drop` is called before the struct memory is released
- Both are identified by name: `func init(self, ...)` / `func drop(self)`

### Generics strategy
- Monomorphization (zero-cost): each `T` → concrete type generates a fresh AST copy
- No runtime type information, no type erasure
- Bounds (`where T: Trait`) checked at monomorphization time

### Dynamic dispatch
- `dyn Trait` is a fat pointer: `{ *data, *vtable }` (16 bytes)
- Vtable is generated once per `impl Trait for Type`
- Method calls go through `vtable[index]`
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

### P04.03 — Automatic Destructor Insertion (`P04.03`)
- [ ] Track class-typed variables in frame collector
- [ ] Emit `drop` calls at all scope exits (normal, return, break/continue)
- [ ] Emit in reverse declaration order
- [ ] Test fixture: `test_drop.ae` — multiple class vars, correct drop order
- [ ] **MILESTONE**: Compiler-generated destructor chains at scope exit

### P04.04 — Access Modifiers (`P04.04`)
- [ ] Parser: `pub`, `internal`, `private` on fields and methods
- [ ] Semantic: enforce access rules per scope
- [ ] Test fixture: `test_access.ae`
- [ ] **MILESTONE**: Access control works

### P04.05 — Traits and Impl (`P04.05`)
- [ ] Parser: `trait Name { ... }` declaration with method signatures
- [ ] Parser: `impl Trait for Type { ... }` block
- [ ] Semantic: check trait methods are implemented
- [ ] Static dispatch: direct call to type's method (zero-cost)
- [ ] Dynamic dispatch: `dyn Trait` vtable pointer
- [ ] Test fixture: `test_trait.ae`
- [ ] **MILESTONE**: Traits with static dispatch compile

### P04.06 — Generics (`P04.06`)
- [ ] Parser: `func Name(T)(params)` generic parameter syntax
- [ ] Parser: `class Name(T)` generic class syntax
- [ ] Semantic: monomorphization — duplicate code per concrete type
- [ ] Codegen: `where T: Trait` bounds checking at monomorphization time
- [ ] Test fixture: `test_generic.ae` — generic identity function, generic stack
- [ ] **MILESTONE**: `func identity(T)(value: T): T -> value` works

### P04.07 — `if let` Pattern Binding (`P04.07`)
- [ ] Parser: `if let x = expr { }` / `if let Some(x) = opt { }`
- [ ] Codegen: check optional tag, if some → bind value to x, run body
- [ ] Test fixture: `test_iflet.ae`
- [ ] **MILESTONE**: `if let val = optional { print(val) }` works

### P04.08 — Phase 4 Verification (`P04.08`)
- [ ] `make clean && make test` — 14/14 passing
- [ ] `make test-host` — all fixturs passing
- [ ] Freestanding ELF64 still works
- [ ] `test_method.ae`, `test_class.ae`, `test_drop.ae`, `test_trait.ae`, `test_generic.ae`, `test_iflet.ae` all pass
- [ ] Update STATUS.md and PHASE_04.md
- [ ] **MILESTONE**: Phase 4 verified

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
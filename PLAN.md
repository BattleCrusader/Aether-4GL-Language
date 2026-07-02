# Aether Compiler — Development Plan

> **Current branch:** `feature/P34.00-wire-spec-tests`
> **Last updated:** 2026-07-01
> **Status:** All 34 phases complete. C transpiler is the default backend. 54/54 host-native tests pass. 37 spec test fixtures wired.

---

## Completed Phases (0–34)

| Phase | Description | Status |
|-------|-------------|--------|
| 0–6 | Bootstrap, core language, host-native, memory mgmt, OOP, advanced features, OS integration | ✅ Complete |
| 8–11 | Multi-target assembler, optimization, universal binaries, kernel codegen, @layout | ✅ Complete |
| 12 | Language specification & requirements | ✅ Complete |
| 17 | `.aelib` library format | ✅ Complete |
| 18 | Standard library in pure Aether | ✅ Complete |
| 20–34 | C transpiler backend — all parsed features transpiled, tested, documented | ✅ Complete |

---

## Next Priority Queue

1. **Fix spec test failures**: 18/37 `test_spec_*.ae` fixtures fail — parser/semantic gaps
2. **Phase 9 remaining**: Escape analysis, devirtualization, loop unrolling, actionable errors
3. **Phase 13**: Concurrency & fibers (spawn, channels, mutex, scheduler)
4. **Phase 14**: Advanced OS language features (bootchain, interrupt handlers, metadata)
5. **Phase 15**: Goal-oriented I/O & query fusion
6. **Phase 16**: Protocol generation & hardware configuration
7. **Phase 19**: LLVM backend migration (deferred — C transpiler is sufficient)
8. **Phase 20**: Self-hosting — compiler compiles itself

---

## Architecture

```
Source (.ae)
  → Tokenizer (existing)          ✅
    → Parser (existing)            ✅
      → AST (existing)             ✅
        → Import Resolution        ✅
          → Semantic Analysis      ✅
            → C Codegen            ✅
              → C Compiler (gcc/clang) → native binary
```

The C transpiler (`src/c_transpiler/`, 12 files, ~1900 lines) is the default backend. The NASM codegen (`src/codegen.c`) is used only for `--target asm-*` targets.

---

## Module Map

```
src/c_transpiler/
├── c_transpiler.h          # Main header: CCodegen state, public API
├── c_transpiler.c           # Entry point, walks AST, dispatches
├── c_types.c                # Aether → C type mapping (owned, rc, ref)
├── c_expr.c                 # Expression codegen (literals, idents, ops, calls)
├── c_stmt.c                 # Statement codegen (let, if, while, for, return, defer, contracts)
├── c_func.c                 # Function codegen (decl, params, return types, throws)
├── c_string.c               # String operations (concat, interpolation, itoa)
├── c_asm.c                  # NASM→GCC asm converter
├── c_error.c                # Error handling (try/catch/throw, throws propagation)
├── c_contract.c             # Pre/post conditions
├── c_runtime.c              # Runtime helpers (alloc, concat, itoa, rc retain/release)
└── c_target.c               # Target-specific emission (host, freestanding, kernel, boot)
```

---

## Key Decisions

- **C transpiler is the default backend** — no flag needed. NASM codegen for asm targets only.
- **`pub` → `public`** keyword (Phase 32.00)
- **`inv` → `contract`** keyword for struct/class contracts (Phase 33.00)
- **Ownership**: `owned T` → `T*` with auto-free, `rc T` → `void*` with retain/release
- **Throws**: return type struct with `ThrowResult_FuncName` typedef
- **Generics**: monomorphization with `T: Constraint` syntax
- **All 37 spec test fixtures** wired into Makefile (Phase 34.00)

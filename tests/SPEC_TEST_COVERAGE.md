# Spec Test Coverage Status

This document tracks which SPECIFICATION.md sections have test coverage
and the current status of those tests.

## Test Conventions

- **Positive tests** (`tests/fixtures/test_spec_*.ae`): valid Aether code that
  exercises a spec feature. Must COMPILE when the feature is fully implemented.
- **Negative tests** (`tests/fixtures/negative/*.ae`): malformed programs that
  the compiler MUST reject. They verify the parser/semantic analyzer correctly
  catches errors.
- **Test pattern**: each `@test func main(): u64 { ... }` returns a failure
  count (non-zero = fail) — see existing fixtures for examples.

## How to Run

```bash
make test-host       # compile all positive fixtures
make test-negative   # verify all negative fixtures are rejected
make test-spec       # full test suite (positive + negative)
```

## Coverage Matrix

| Section | Title                          | Fixture                                 | Status |
|--------:|--------------------------------|-----------------------------------------|--------|
|       1 | Introduction                   | (n/a — Hello World only)                | n/a    |
|       2 | Design Philosophy              | (no test — descriptive)                 | n/a    |
|       3 | Lexical Structure              | test_spec_03_* (multiple)               | Mixed  |
|       4 | Types                          | test_spec_04_* (multiple)               | Mixed  |
|       5 | Variables and Bindings         | test_spec_05_variables.ae              | OK     |
|       5 | (alt full coverage)            | test_spec_05_variables_bindings_full.ae | OK     |
|       6 | Functions                      | test_spec_06_functions.ae              | OK     |
|       7 | Control Flow                   | test_spec_07_control_flow.ae           | OK     |
|       8 | Memory Management              | test_spec_08_memory.ae                 | FAIL   |
|       9 | Object-Oriented Programming    | test_spec_09_oop.ae                    | FAIL   |
|      10 | Generics                       | test_spec_10_generics.ae               | FAIL   |
|      11 | Error Handling                 | test_spec_11_error_handling.ae         | FAIL   |
|      12 | Compile-Time Execution         | test_spec_12_comptime.ae               | FAIL   |
|      13 | Contract Programming           | test_spec_13_contracts.ae              | FAIL   |
|      14 | Closures and Lambdas            | test_spec_14_closures.ae               | FAIL   |
|      15 | Properties and Operator Overloading | test_spec_15_properties.ae        | FAIL   |
|      16 | Dynamic Dispatch               | test_spec_16_dynamic_dispatch.ae       | FAIL   |
|      17 | Inline Assembly                | test_spec_17_inline_assembly.ae        | FAIL   |
|      18 | Aether OS Integration          | test_spec_18_os_integration.ae         | FAIL   |
|      18 | (full coverage)                | test_spec_18_os_integration_full.ae    | FAIL   |
|      19 | Multi-Target Assembler         | test_spec_19_multi_target.ae           | FAIL   |
|      20 | Universal Binaries             | test_spec_20_universal_binaries.ae    | FAIL   |
|      21 | Standard Library               | test_spec_21_stdlib.ae                 | FAIL   |
|      22 | Build System                   | test_spec_22_build_system.ae           | OK     |
|      23 | Compiler Targets               | test_spec_23_compiler_targets.ae       | FAIL   |
|      24 | Future & Aspirational Features | test_spec_24_future_features.ae        | FAIL   |
|      25 | Concurrency and Fibers          | test_spec_25_concurrency_fibers.ae     | FAIL   |
|      26 | Module System and Imports      | test_spec_26_module_system.ae          | FAIL   |

**Status meanings:**
- **OK**     — compiles successfully (bootstrap implements enough of the feature)
- **FAIL**   — fails to compile because the feature isn't fully implemented yet
- **Mixed**  — some sub-fixtures pass, others fail (depends on which sub-feature)

## Why Each Spec File Fails (or Doesn't)

The v1 Go bootstrap compiler is missing significant portions of the spec.
Every failing `test_spec_*.ae` fixture exposes a real gap. When that
feature is implemented, the test will start compiling and (if the
assertions are valid) passing.

Specific known gaps (current failures):
- **Multi-field struct parsing** — the parser rejects structs with >1 field
  per declaration block. This affects sections 4, 8, 12, etc.
- **Multi-variant enum parsing** — the parser rejects enums with
  multi-line variant lists. Affects section 11.
- **Generic type parameters** — `<T>` syntax not implemented. Affects section 10.
- **Lambda syntax** — `|x| -> x + 1` not implemented. Affects section 14.
- **Pre/post contracts** — `pre(x > 0)` clauses not parsed. Affects section 13.
- **Trait static dispatch** — partial; see existing `test_trait.ae`.
- **Inline asm** — partial; some constructs fail (e.g. nested asm blocks).
- **External declarations** — `extern func` not in v1 yet.

## Negative Test Coverage

22 negative test fixtures verify the compiler REJECTS invalid programs:
- Lexer-level errors (malformed literals, unterminated strings)
- Parser-level errors (unexpected tokens, missing syntax)
- Semantic-level errors (type mismatches, undeclared variables,
  wrong arg counts, duplicate declarations)

**Currently 11/22 negative tests pass** (compiler correctly rejects the
malformed program). The remaining 11 expose gaps where the compiler
INCORRECTLY ACCEPTS invalid programs — these need to be addressed in
the bootstrap.

## What This Means for TDD

- Every `test_spec_*.ae` is a specification of what the compiler
  SHOULD do, written as a test. As the compiler implements each
  feature, the corresponding test will start passing.
- Every `negative/*.ae` is a specification of what the compiler
  MUST reject. As the type checker/semantic analyzer improves,
  these tests will start rejecting correctly.

Run `make test-host` and `make test-negative` regularly during compiler
development to track progress. Each passing test marks a feature or
safety guarantee implemented.
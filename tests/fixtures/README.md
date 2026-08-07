# Aether spec test fixtures

This directory contains .ae test fixtures for the Aether compiler.

## Layout

* `test_spec_##_..._full.ae` — comprehensive coverage of one or more
  SPECIFICATION.md sections. Self-contained, no imports. Each file
  ends with a `@test func main()` that returns 0 on full pass, or the
  label of the first failing assertion (1..N).

* `neg/test_neg_*.ae` — negative-test catalog. Each fixture contains a
  syntactically or semantically invalid construct that the compiler
  MUST reject. Run via `./build/aether neg/test_neg_*.ae`; exit code
  non-zero means the fixture was correctly rejected.

* `test_spec_##_*.ae` (without `_full`) — older fixtures from
  earlier TDD sessions. Kept for history.

## Running

```
# Compile every positive fixture (compile-check only)
make test-host

# Compile + run a specific fixture
./build/aether tests/fixtures/test_spec_07_control_flow_full.ae -o /tmp/t
( /tmp/t < /dev/null ); echo $?
```

## Coverage map

| Fixture | Spec sections |
|---------|---------------|
| test_spec_05_variables_bindings_full.ae   | §5 Variables and Bindings |
| test_spec_06_functions_full.ae            | §6 Functions |
| test_spec_07_control_flow_full.ae         | §7 Control Flow |
| test_spec_08_memory_full.ae              | §8 Memory Management |
| test_spec_09_oop_full.ae                  | §9 OOP |
| test_spec_10_generics_full.ae            | §10 Generics |
| test_spec_11_error_handling_full.ae      | §11 Error Handling |
| test_spec_12_compile_time_full.ae        | §12 Compile-Time Execution |
| test_spec_13_contracts_full.ae           | §13 Contract Programming |
| test_spec_14_15_closures_properties_full.ae | §14 Closures, §15 Properties |
| test_spec_16_17_dispatch_asm_full.ae     | §16 Dynamic Dispatch, §17 Inline Assembly |
| test_spec_18_os_integration_full.ae      | §18 Aether OS Integration |
| test_spec_19_20_multi_target_universal_full.ae | §19 Multi-Target Assembler, §20 Universal Binaries |
| test_spec_21_22_stdlib_build_full.ae     | §21 Standard Library, §22 Build System |
| test_spec_23_25_targets_concurrency_full.ae | §23 Compiler Targets, §25 Concurrency |
| test_spec_24_future_features_full.ae     | §24 Future & Aspirational Features |
| test_spec_26_modules_full.ae             | §26 Module System and Imports |
| test_spec_appendix_lexical_full.ae       | Appendix A, B |
| test_spec_appendix_build_undefined_full.ae | Appendix C, D |
| test_spec_appendix_pipeline_full.ae     | Appendix E |
| test_spec_negative_cases.ae              | negative-test index |

## Bootstrap status (known unimplemented)

Features the bootstrap parser does NOT yet support. Each is documented
as TODO in the relevant `_full` fixture:

* `pre()`, `post()` contracts
* `asm { ... }` inline blocks
* `extern func` declarations
* `heap`, `unsafe`, `pool` keywords
* `region("name") { }` blocks
* `dyn Trait` vtable dispatch
* `class` keyword (struct methods work; methods inside class body don't)
* `op_+`, `op_==`, etc. operator overloading
* `trait`, `impl` blocks
* `for i in 0..10` hangs the binary — use `while i < 10`
* Integer literals >= 2^63 are rejected
* Multi-field structs (`struct P { x: u64; y: u64 }`) fail; use one field
* `-> expr` expression-bodied function
* `...T` variadic parameters
* Labeled `break outer` / `continue outer`
* `throws` annotation on return types
* `sys func ... at(N)` declarations

Until these are fixed, the `_full` fixtures test the SPEC surface and
document each limitation inline.
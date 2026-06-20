# Phase 06 — Aether OS Integration 🔵 IN PROGRESS

**Goal:** Add Aether OS-specific features to the compiler — syscall page declarations, module support, entry points, layout directives, kernel-aware codegen, and a freestanding standard library. The compiler becomes Aether-OS-aware.

**Current branch:** `feature/P06.00-os-integration` (single branch for this phase)

---

## Task Breakdown

### P06.01 — `sys func` Keyword 🟢
- [x] Tokenizer: add `sys` keyword
- [x] Parser: `sys func name(params) at(N)` parsing
- [x] AST: `is_sys` flag on FuncDecl, `sys_index` field
- [x] Codegen: indirect call through 0x5000 table (`call [rax]` with rax = table_base + N*8)
- [x] Test fixture: `test_sysfunc.ae` — basic syscall page declaration
- [x] Phase doc update

### P06.02 — `module` Keyword 🟢
- [x] Tokenizer: `module` keyword
- [x] Parser: `module name { decls }` block parsing
- [x] AST: `NODE_MODULE_DECL` node type
- [x] Codegen: generates kernel module ELF structure
- [x] Test fixture: `test_module.ae` — module declaration
- [x] Phase doc update

### P06.03 — `@export` Attribute 🟢
- [x] Tokenizer: `export` as keyword (part of `@export` attribute)
- [x] Parser: `@export` attribute on function declarations
- [x] AST: `is_exported` flag on FuncDecl
- [x] Codegen: module exports symbols correctly
- [x] Semantic: exported functions are visible to module loader
- [x] Test fixture: `test_export.ae` — exported function compiles and runs
- [x] Phase doc update

### P06.04 — `@entry(addr)` Attribute 🟢
- [x] Parser: parse integer argument from `@entry(0x2000000)` syntax 🟢
- [x] AST: `entry_addr` field on FuncDecl (stores load address, -1 = not set) 🟢
- [x] AST: `AttrData` struct with name and int_value for @entry payload 🟢
- [x] Codegen: when func has `@entry(addr)`: 🟢
  - Freestanding: function IS the entry point, linker script uses ENTRY(func_name) and `. = addr`
  - Host targets: wrapper calls @entry function instead of `main`, then exits
- [x] Codegen: linker script uses `@entry` address for `. = ` origin directive 🟢
- [x] Test fixture: `test_entry.ae` — `@entry(0x2000000) func main(): int` returns 42 🟢
- [x] All 26 host-native tests passing (16/16 tokenizer, 14/14 parser) 🟢

### P06.05 — `@layout(start, max, file)` Attribute 🟢
- [x] Parser: `@layout(start=N, max=M, file="name")` key=value syntax 🟢
- [x] AST: layout fields on FuncDecl (has_layout, layout_start, layout_max, layout_file) 🟢
- [x] Codegen: `@layout` functions emit `bits 64` + `[org N]` for flat binary mode 🟢
- [x] Codegen: `@layout` functions skip prologue/epilogue (no push rbp / pop rbp / ret) 🟢
- [x] Codegen: padding via `times max-($-$$) db 0` to exactly fill max bytes 🟢
- [x] Codegen: flat binary assembly via `nasm -f bin`, bypassing ELF linker 🟢
- [x] Codegen: size verification — error if binary exceeds layout_max 🟢
- [x] Codegen: skip entry wrapper, runtime helpers, and .rodata for layout functions 🟢
- [x] Test fixture: `test_layout.ae` — compiles to 512-byte flat binary 🟢
- [x] `make test-layout` target added — verifies flat binary output size 🟢

### P06.06 — `@kernel_layout` — Memory Map Verification
- [ ] Parser: `@kernel_layout` attribute on functions
- [ ] Semantic: verify declared memory regions don't overlap known OS regions
- [ ] Semantic: check reserved ranges against the Aether OS memory map
- [ ] Codegen: emit layout verification as comments (debug) or assertions (test mode)
- [ ] Test fixture: overlapping region detection test

### P06.07 — `@module_abi(version)` — ABI Compliance
- [ ] Parser: `@module_abi(version=N)` on module declarations
- [ ] Semantic: verify module exports conform to ABI version requirements
- [ ] Codegen: emit ABI version marker in module ELF
- [ ] Test: ABI version mismatch detection

### P06.08 — Declarative Resources: `pool`, `protocol`
- [ ] Tokenizer: `pool`, `protocol` keywords
- [ ] Parser: `pool Name of size N, count M, alignment A` syntax
- [ ] Parser: `protocol Name { ... }` syntax
- [ ] AST: pool and protocol node types
- [ ] Codegen: pool generates static allocation array + alloc/free helpers
- [ ] Codegen: protocol generates init/transfer/status helpers
- [ ] Test fixture: pool allocation and deallocation

### P06.09 — Target-Specific Code Generation
- [ ] Add target enum entries: `TARGET_KERNEL`, `TARGET_MODULE`, `TARGET_BINARY`, `TARGET_BOOT`
- [ ] Codegen: kernel target → 0x1000000 load, ring 0 syscall conventions
- [ ] Codegen: module target → .ko ELF, 0x4000 registry calls
- [ ] Codegen: binary target → 0x2000000 load, 0x5000 syscall table
- [ ] Codegen: boot target → flat binary, no ELF header
- [ ] CLI: `--target kernel|module|binary|boot`
- [ ] Linker scripts per target
- [ ] Test: each target produces correct output format

### P06.10 — Freestanding Standard Library (StdAether)
- [ ] `std.io` — `print`, `println`, `format`, `write` syscall wrappers
- [ ] `std.mem` — `Pool`, `Arena`, `copy`, `zero`, `alloc`, `free`
- [ ] `std.str` — `String`, `string_view`, `concat`, `split`, `trim`
- [ ] `std.math` — basic math (abs, min, max, clamp)
- [ ] `std.collections` — `Array`, `HashMap`, `List` (simple versions)
- [ ] `std.serial` — COM1 serial I/O (kernel mode)
- [ ] `std.fs` — AetherFS syscall wrappers (open, read, readdir)
- [ ] `std.elf` — ELF64 reader/writer
- [ ] `std.test` — `assert`, test runner
- [ ] `std.asm` — NASM helper macros
- [ ] `std.arch` — architecture detection helpers

### P06.11 — Linker Script Integration
- [ ] Support custom linker scripts via `--linker-script` CLI flag
- [ ] Support `aether.toml` linker script configuration
- [ ] Default linker scripts for each target baked into compiler
- [ ] Allow `@layout` to override linker script sections
- [ ] Test: custom linker script produces correct binary layout

### P06.12 — Project Manifest: `aether.toml`
- [ ] Parser for `aether.toml` (TOML subset)
- [ ] CLI: `aether init` creates project structure
- [ ] Support `[package]`, `[build]`, `[dependencies]`, `[profile.*]` sections
- [ ] Target configuration in manifest
- [ ] Dependency resolution (path-based)
- [ ] Test: `aether init` + `aether build` from manifest

---

## Technical Notes

### Entry point strategy
- `@entry(addr)` sets the load address. The function itself is the entry point.
- For freestanding: the linker script uses `ENTRY(func_name)` and `. = addr`
- For host targets: still uses the native entry mechanism but address is advisory
- For boot sectors: `@layout` combined with `@entry` at 0x7C00

### Memory map (compiler-known)
- 0x7C00: Stage1 MBR
- 0x7E00: Stage2 loader
- 0x6000-0x1000: Page tables / GDT
- 0x4000: Module registry
- 0x5000: Syscall page
- 0x1000000: Kernel base
- 0x2000000: Binary exec space (64KB max)
- 0x2100000: Module slots (8 × 64KB)
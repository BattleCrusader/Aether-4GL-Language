# Phase 06 — Aether OS Integration 🟢 COMPLETE

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

### P06.02 — `module` Keyword 🟢
- [x] Tokenizer: `module` keyword
- [x] Parser: `module name { decls }` block parsing
- [x] AST: `NODE_MODULE_DECL` node type with `ModuleDecl` struct
- [x] Codegen: module ABI version emitted as assembly comment
- [x] Test fixture: `test_module.ae` — module declaration compiles

### P06.03 — `@export` Attribute 🟢
- [x] Parser: `@export` attribute on function declarations
- [x] AST: `is_exported` flag on FuncDecl
- [x] Codegen: exported function compiles correctly
- [x] Test fixture: `test_export.ae`

### P06.04 — `@entry(addr)` Attribute 🟢
- [x] Parser: `@entry(0xNNN)` integer payload extraction into `AttrData.int_value`
- [x] AST: `entry_addr` field on FuncDecl, `AttrData` struct in node union
- [x] Codegen: @entry function IS the entry point for freestanding; host wrapper calls it
- [x] Linker script: uses `@entry` address for `. = origin` and `ENTRY(func_name)`
- [x] Test fixture: `test_entry.ae` — `@entry(0x2000000) func main(): int` returns 42

### P06.05 — `@layout(start, max, file)` Attribute 🟢
- [x] Parser: `@layout(key=value)` key=value syntax with comma separators
- [x] AST: layout fields on FuncDecl + AttrData; Codegen struct fields
- [x] Codegen: flat binary via `nasm -f bin`, `[org N]`, `bits 64`
- [x] Codegen: skips prologue/epilogue for layout functions (raw code)
- [x] Codegen: `times max-($-$$) db 0` padding to exact size
- [x] Codegen: size verification — error if binary exceeds layout_max
- [x] Codegen: skips entry wrapper, runtime helpers, .rodata for layout
- [x] **Asm block passthrough**: raw assembly captured from source (tokenizer pos tracking) and emitted verbatim
- [x] Test: `test_bootsector.ae` → valid 512-byte bootable flat binary with 0xAA55 signature
- [x] `make test-layout` target for flat binary size verification

### P06.06 — `@kernel_layout` — Memory Map Verification 🟢
- [x] Parser: `@kernel_layout` attribute on functions
- [x] AST: `is_kernel_layout` flag on FuncDecl
- [x] Codegen: `MemoryRegion` table with all 9 Aether OS known regions (0x1000-0x10000000)
- [x] Codegen: `find_overlapping_region()` overlap check
- [x] Codegen: `cg_verify_kernel_layout()` emits memory map as assembly comments
- [x] Test fixture: `test_kernellayout.ae`

### P06.07 — `@module_abi(version)` — ABI Compliance 🟢
- [x] Parser: `@module_abi(version=N)` key=value on module declarations
- [x] AST: `module_abi_version` field on ModuleDecl + AttrData
- [x] Codegen: emits `; Module ABI version: N` as assembly comment
- [x] Test fixture: `test_module_abi.ae` — compiles and returns 42

### P06.08 — Declarative Resources: `pool`, `protocol` 🟢
- [x] Tokenizer: `pool`, `protocol` keywords (already in keyword table)
- [x] AST: `NODE_POOL_DECL`, `NODE_PROTOCOL_DECL` node types with `PoolDecl` and `ProtocolDecl` structs
- [x] Parser: `pool Name of size N, count M, alignment A` syntax
- [x] Parser: `protocol Name { func_decls }` syntax
- [x] Codegen: passes through compilation (no runtime codegen yet)
- [x] Test fixtures + parser unit tests (4 new parser tests)

### P06.09 — Target-Specific Code Generation 🟢
- [x] Target enum entries: `TARGET_KERNEL`, `TARGET_MODULE`, `TARGET_BINARY`, `TARGET_BOOT`
- [x] Codegen: kernel target → ELF64 at 0x1000000 layout
- [x] Codegen: module target → .ko-style ELF64
- [x] Codegen: binary target → /bin/ ELF64 at 0x2000000 layout
- [x] Codegen: boot target → flat binary via `nasm -f bin`
- [x] CLI: `--target kernel|module|binary|boot` parsing
- [x] Test: each new target compiles successfully

### P06.10 — Freestanding Standard Library (StdAether) 🟢
- [x] `std/io.ae` — `print`, `println`, `format` stubs
- [x] `std/mem.ae` — `copy`, `zero`, `alloc`, `free` stubs
- [x] `std/str.ae` — `concat`, `split`, `trim`, `to_upper`, `to_lower` stubs
- [x] `std/math.ae` — `abs_val`, `min_val`, `max_val`, `clamp_val` stubs
- [x] `std/collections.ae` — `Array`, `List` structs + constructors
- [x] `std/serial.ae` — COM1 serial I/O stubs
- [x] `std/test.ae` — `assert`, `assert_eq`, `test_run` stubs
- [ ] `std/fs.ae` — AetherFS syscall wrappers (not started)
- [ ] `std/elf.ae` — ELF64 reader/writer (not started)
- [ ] `std/asm.ae` — NASM helper macros (not started)
- [ ] `std/arch.ae` — architecture detection (not started)

### P06.11 — Linker Script Integration 🟢
- [x] `--linker-script` / `-L` CLI flag for custom linker script path
- [x] Codegen: `linker_script` field on Codegen struct
- [x] Freestanding target uses custom linker script when specified
- [x] `aether.toml` supports `[build] linker-script = "path"` key
- [x] Test: custom linker script path accepted and used

### P06.12 — Project Manifest: `aether.toml` 🟢
- [x] `aether init <name>` creates project structure: `aether.toml`, `src/main.ae`, `tests/`
- [x] `aether new <name>` alias
- [x] TOML subset parser: `[build]` section with `target`, `output`, `linker-script`
- [x] Auto-discovery: when no `.ae` file specified, reads `aether.toml` from cwd
- [x] Test: init creates valid structure, build from manifest works

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

### Asm block implementation
- Raw assembly text captured from source using tokenizer position tracking
- `p->lexer->tok->pos` records start after `{`, extracts bytes before `}`
- Trimmed of leading/trailing whitespace and closing brace
- Codegen writes lines verbatim at column 0 (no extra indentation)
- Supports `asm: (outputs) { ... }` syntax with output variable bindings
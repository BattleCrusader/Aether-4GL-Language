# Phase 17 — `.aelib` Library Format

**Status**: ✅ COMPLETE
**Branch**: `feature/P17.00-aelib-library-format`
**Goal**: Closed-source library distribution via `.aelib` binary format — compiled code + metadata for type-safe linking without source.

---

## Overview

Current `import "path.ae"` requires source. For third-party closed-source libraries, we need a binary format that contains compiled code, exposes type signatures and class layouts, hides source code, and enforces access modifiers.

The `.aelib` format is an archive containing:
- **Code section**: archive of `.o` object files (one per compiled source)
- **Metadata section**: binary blob with symbol table, type data, string table

---

## P17.01 — Add `TARGET_LIB` to Target enum and CLI

- [x] P17.01.01 — Add `TARGET_LIB` to `Target` enum in `include/aether/codegen.h`
- [x] P17.01.02 — Add `"lib"` to `target_name()` in `src/codegen.c`
- [x] P17.01.03 — Add `"lib"` to `parse_target_name()` in `src/aether.c`
- [x] P17.01.04 — Add `lib` to usage() help text in `src/aether.c`
- [x] P17.01.05 — Add `lib` to Makefile test targets (compile-check only, no native run)

## P17.02 — AelibWriter: Write `.aelib` binary format

- [x] P17.02.01 — Create `include/aether/aelib.h` with:
  - Header struct: magic "AELIB\0", version 0x0001, flags, ABI version
  - Code section descriptor (offset, size)
  - Metadata section descriptor (offset, size)
  - Metadata sub-header: magic "AEMETA\0", version 0x0001
  - Symbol table entry struct: name_offset, kind (func/struct/class/global/const/enum), flags (public bit), namespace_offset, type_data_offset, type_data_size
  - Type data: function signature, struct/class layout, enum layout
  - String table: null-terminated strings concatenated
- [x] P17.02.02 — Create `src/aelib.c` with:
  - `aelib_create()` — initialize writer
  - `aelib_add_symbol()` — add a symbol entry
  - `aelib_add_type_data()` — add type data blob
  - `aelib_add_string()` — add string to string table
  - `aelib_write()` — serialize everything to file
  - `aelib_destroy()` — cleanup
- [x] P17.02.03 — Add `src/aelib.c` to `CORE_SRCS` in Makefile
- [x] P17.02.04 — Add `include/aether/aelib.h` to install targets in Makefile

## P17.03 — Metadata extraction from AST

- [x] P17.03.01 — Walk the AST after semantic analysis to extract:
  - Function declarations: name, return type, param names/types/mutability, visibility
  - Struct declarations: name, field names/types/offsets/sizes, visibility
  - Class declarations: name, fields, methods, visibility
  - Enum declarations: name, variants with discriminants and payload types, visibility
  - Const declarations: name, type, visibility
- [x] P17.03.02 — Filter by visibility: only `pub` decls go into metadata
- [x] P17.03.03 — Serialize extracted metadata into AelibWriter

## P17.04 — Codegen for `--target lib`

- [x] P17.04.01 — In `codegen_generate()`, handle `TARGET_LIB` by:
  - Generating NASM as normal (same as host target for the code)
  - Assembling to `.o` file
  - Extracting metadata from AST
  - Writing `.aelib` archive containing `.o` + metadata
- [x] P17.04.02 — In `codegen_assemble()`, handle `TARGET_LIB`:
  - Assemble .asm → .o via NASM
  - Call aelib_write() to produce .aelib
  - Return 0 on success

## P17.05 — Import resolution for `.aelib` files

- [x] P17.05.01 — In `aether.c` import resolution, check for `.aelib` files:
  - Try `path.ae` first (existing behavior)
  - If not found, try `path.aelib`
  - If not found, try `path/lib.ae` or `path/lib.aelib`
  - If not found, search standard library paths
- [x] P17.05.02 — Create `src/aelib_reader.c` with:
  - `aelib_read()` — open and validate .aelib file
  - `aelib_read_metadata()` — read and parse metadata section
  - `aelib_get_symbols()` — return symbol table
  - `aelib_get_type_data()` — return type data for a symbol
  - `aelib_get_string()` — resolve string from string table
- [x] P17.05.03 — Create `include/aether/aelib_reader.h`
- [x] P17.05.04 — Add `src/aelib_reader.c` to `CORE_SRCS` in Makefile
- [x] P17.05.05 — Add `include/aether/aelib_reader.h` to install targets

## P17.06 — Synthetic AST nodes from metadata

- [x] P17.06.01 — Create function to build synthetic `NODE_FUNC_DECL` from metadata:
  - Name, params, return type, body=NULL (extern)
  - Mark as `is_pub` if public
- [x] P17.06.02 — Create function to build synthetic `NODE_STRUCT_DECL` from metadata:
  - Name, fields with types
- [x] P17.06.03 — Create function to build synthetic `NODE_CLASS_DECL` from metadata:
  - Name, fields, methods
- [x] P17.06.04 — Create function to build synthetic `NODE_ENUM_DECL` from metadata:
  - Name, variants with payload types
- [x] P17.06.05 — Create function to build synthetic `NODE_CONST_DECL` from metadata
- [x] P17.06.06 — Wire synthetic node creation into import resolution: when importing `.aelib`, create synthetic AST nodes and merge them into the program

## P17.07 — Linking `.o` from `.aelib` archives

- [x] P17.07.01 — In `codegen_assemble()`, when linking for host/kernel/binary targets:
  - Collect all imported `.aelib` paths
  - Extract `.o` files from each archive
  - Include them in the linker command
- [x] P17.07.02 — Track imported `.aelib` paths during import resolution
- [x] P17.07.03 — Pass `.o` file paths to `codegen_assemble()` via Codegen struct

## P17.08 — Test fixture: build and use a `.aelib` library

- [x] P17.08.01 — Create `tests/fixtures/lib_math.ae` — a simple library with pub functions
- [x] P17.08.02 — Create `tests/fixtures/test_aelib_import.ae` — imports the .aelib and calls functions
- [x] P17.08.03 — Add Makefile target to build the .aelib before running test-host
- [x] P17.08.04 — Add test to test-host runner
- [x] P17.08.05 — Run `make test && make test-host` to verify no regressions

## P17.09 — Documentation and cleanup

- [x] P17.09.01 — Update `STATUS.md` with Phase 17 completion
- [x] P17.09.02 — Update `AGENTS.md` with new source files
- [x] P17.09.03 — Update `PHASE_17.md` with completion status
- [x] P17.09.04 — Update Obsidian knowledge base
- [x] P17.09.05 — Commit and push

---

## File Format Reference

```
+----------------------------------+
|  Magic: "AELIB\0" (8 bytes)     |
|  Version: 0x0001 (2 bytes)     |
|  Flags:  (2 bytes)             |
|  ABI version: (2 bytes)         |
+----------------------------------+
|  Code section offset (8 bytes)  |
|  Code section size   (8 bytes)  |
+----------------------------------+
|  Metadata section offset (8)    |
|  Metadata section size   (8)   |
+----------------------------------+
|  Code section (archive of .o)  |
+----------------------------------+
|  Metadata section (binary blob) |
+----------------------------------+
```

### Metadata Section

```
Header: magic "AEMETA\0" (8 bytes), version 0x0001 (2 bytes)
Symbol table:
  count (u32)
  for each symbol:
    name_offset (u32) — into string table
    kind (u8) — 0=function, 1=struct, 2=class, 3=global, 4=const, 5=enum
    flags (u8) — bit 0 = public
    namespace_offset (u32) — class name if member, 0 if top-level
    type_data_offset (u32)
    type_data_size (u32)

Type data:
  Function: return_type_id (u16), param_count (u8), params[] with name/type/mutability
  Struct/class: field_count (u16), fields[] with name/type/offset/size
  Enum: variant_count (u8), variants[] with name/discriminant/payload_type

String table: null-terminated strings concatenated
```

### Import Syntax
```aether
import "libtest"                    # all public decls
import "libtest" : Foo              # all public from Foo class only
import "libtest" : Foo, Bar         # from multiple classes
import "libtest.aelib" : Foo        # explicit .aelib file
import "std/io"                     # standard library
```

### Resolution Order
1. `foo.ae` — source file (existing behavior)
2. `foo.aelib` — pre-compiled library (new)
3. `foo/lib.ae` or `foo/lib.aelib` — package directory
4. Standard library paths: `$AETHER_LIB`, `~/.aether/lib`, `/usr/local/lib/aether`

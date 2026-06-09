# Phase 02 — Host-Native Output (PRIORITY)

**Goal:** Compile `.ae` programs into native Mach-O 64 executables that run directly on macOS (Intel natively, ARM via Rosetta). Produce `aether build hello.ae && ./hello` as a working loop on the dev machine.

**Strategy:** Add a target parameter to the codegen/aether CLI that controls output format. The existing freestanding ELF64 path stays intact. Add a parallel Mach-O 64 path.

---

## Task Breakdown

### P02.01 — Target enum + CLI flag (`P02.01`)
- [ ] Add `Target` enum to codegen.h: `TARGET_FREESTANDING`, `TARGET_MACHO64`, `TARGET_HOST`
- [ ] Add `--target` CLI flag to `aether.c`: `--target host` (auto-detect), `--target x86_64-freestanding` (default)
- [ ] Pass target through to codegen and assembler/linker stages
- [ ] `TARGET_HOST` detects macOS → `TARGET_MACHO64`, Linux → `TARGET_ELF64_HOST` (future)
- [ ] **MILESTONE**: `aether build hello.ae --target host` parses the flag

### P02.02 — Mach-O 64 entry point + syscall ABI (`P02.02`)
- [ ] Codegen emits `global _main` (not `_start`) when target is Mach-O
- [ ] Codegen emits `_main:` (not `_start:`) as entry point
- [ ] Mach-O exit syscall: `mov rax, 0x2000001; xor rdi, rdi; syscall` (instead of Linux `rax=60`)
- [ ] Mach-O write syscall: `mov rax, 0x2000004; mov rdi, 1; lea rsi, [rel msg]; mov rdx, len; syscall`
- [ ] Handle NASM Mach-O underscore prefix: symbols are `_main`, `_fnname` in .rodata labels etc.
- [ ] **MILESTONE**: `hello.ae` produces a runnable Mach-O binary

### P02.03 — Assembler + linker integration (`P02.03`)
- [ ] `nasm -f macho64` for Mach-O target (instead of `-f elf64`)
- [ ] Link with `clang -arch x86_64 -nostdlib -static -e _main` (instead of `x86_64-elf-ld`)
- [ ] Remove hardcoded `-DLD='...'` from Makefile; let aether.c choose the linker
- [ ] Handle temp file naming: `.o` vs `.obj` cleanly per target
- [ ] **MILESTONE**: Full `aether build hello.ae --target host` pipeline: compile → nasm → ld → runnable binary

### P02.04 — Multi-backend codegen architecture (`P02.04`)
- [ ] Add `target` field to `Codegen` struct
- [ ] `codegen_generate()` branches on target for top-level directives:
  - Freestanding: `bits 64`, `default rel`, `section .text`, `global _start`, `_start:` with Linux exit
  - Mach-O: `default rel` (no `bits 64`), `global _main`, `_main:` with macOS exit
- [ ] `cg_func()` handles Mach-O label prefix (`_fnname` instead of `fnname`)
- [ ] String table label prefix: `.Lstr%d` stays (local labels don't need underscore)
- [ ] **MILESTONE**: Both targets produce correct assembly from the same codegen

### P02.05 — Host-native print() / putchar() (`P02.05`)
- [ ] Add a `__aether_host_putchar` or `__aether_print` function emitted inline for host target
- [ ] Uses macOS write syscall (0x2000004) to stdout
- [ ] Aether `print("...")` calls this
- [ ] **MILESTONE**: `print("Hello\n")` works in native binary

### P02.06 — `aether run` command (`P02.06`)
- [ ] `aether run hello.ae` → compiles + executes in one step
- [ ] Returns exit code from the compiled program
- [ ] Works with `--target host` (implicit default for `run`)
- [ ] Captures program stdout/stderr
- [ ] **MILESTONE**: `aether run hello.ae` shows "Hello" and exits

### P02.07 — Host-native test runner (`P02.07`)
- [ ] Tests compile with `--target host` and run natively
- [ ] Update test fixtures to work as host-native binaries
- [ ] `make test-host` target in Makefile
- [ ] **MILESTONE**: 7 test fixtures compile and run on macOS

### P02.08 — Phase 2 Verification (`P02.08`)
- [ ] `make clean && make test` still passes 28/28
- [ ] 7 `.ae` fixtures compile with `--target host` and run on macOS
- [ ] `aether run hello.ae` works end-to-end
- [ ] Freestanding ELF64 output still works unchanged
- [ ] Update STATUS.md — Phase 2 complete
- [ ] Update PHASE_02.md with final status
- [ ] **MILESTONE**: Phase 2 verified and documented

---

## Technical Notes

### NASM Mach-O specifics
- `nasm -f macho64` auto-prepends `_` to global symbols
- So `global _main` in source becomes `_main` in symbol table
- Local labels `.Lxxx` do NOT get underscore prepended
- No `bits 64` directive needed — the format implies it
- `default rel` is still valid

### Syscall number differences
| Operation | Linux (ELF) | macOS (Mach-O) |
|-----------|-------------|-----------------|
| exit      | 60 (0x3C)   | 0x2000001       |
| write     | 1           | 0x2000004       |
| read      | 0           | 0x2000003       |
| mmap      | 9           | 0x20000C5       |

macOS syscalls have the format: `0x2000000 + syscall_number` for POSIX syscalls.

### Underscore handling
- Mach-O: `global main` is wrong — needs `global _main`
- ELF: `global _start` is correct — no leading underscore
- Solution: make `cg_func()` emit `global _%s` for Mach-O, `global %s` for ELF
- For `_start` / `_main`: store in a target-dependent variable

### arm64 host — x86_64 only for now, aarch64 native planned
- The compiler itself compiles/runs as arm64 (on Apple Silicon)
- It emits x86_64 NASM assembly → assembled with `nasm` → linked with `ld` → produces x86_64 Mach-O
- Binary runs via Rosetta 2 (automatic on Apple Silicon)
- **NASM has no ARM64 backend** — `macho64` format is x86_64 only
- **Future: aarch64 native** will require either:
  - Direct Mach-O object file emission (writing the binary format ourselves, no assembler)
  - Using the system assembler (`as`) with aarch64 GAS syntax (not NASM syntax)
  - A custom backend that emits `arm64` Mach-O relocatable objects
  - This is a separate phase after Phase 2 is stable with x86_64
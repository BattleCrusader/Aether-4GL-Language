# Phase 8 — Multi-Target Assembler

**Goal**: Parse NASM syntax assembly blocks into an intermediate representation (IR), then translate to multiple target architectures (x86_64 passthrough, ARM64, RISC-V). The compiler's `asm { }` blocks currently emit raw NASM text — Phase 8 makes them architecture-aware.

**Branch**: `feature/P08.00-multi-target-asm`

---

## P08.01 — NASM IR Definition 🟡 IN PROGRESS

Define the intermediate representation that NASM instructions are parsed into.

- [x] Define `AsmIR` struct types in `include/aether/asm_ir.h`:
  - `AsmOperand` — register, immediate, memory, label
  - `AsmInstruction` — mnemonic + operands
  - `AsmDirective` — section, align, global, extern, etc.
  - `AsmBlock` — list of instructions + directives
- [x] Register enum: all x86_64 GPRs (rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8-r15), SIMD (xmm0-xmm15), segment regs
- [x] Addressing mode enum: direct, indirect, base+disp, base+index*scale+disp
- [x] Size specifiers: byte, word, dword, qword, oword
- [ ] Instruction metadata: opcode, operand count, operand types, side effects (read/write flags, memory)
- [ ] Unit tests for IR construction

## P08.02 — NASM Parser (asm block → IR)

Parse the raw assembly text inside `asm { }` blocks into the AsmIR.

- [ ] Tokenizer for NASM syntax (mnemonics, registers, numbers, labels, directives)
- [ ] Instruction parser: mnemonic + operand list
- [ ] Operand parser: register, immediate, memory (various addressing modes)
- [ ] Directive parser: global, extern, section, align, times, etc.
- [ ] Label parser: `label:` and `.local_label:`
- [ ] Comment handling (; comments)
- [ ] Error recovery for malformed assembly
- [ ] Integration with existing `asm { }` block parsing in `src/parser.c`
- [ ] Unit tests for NASM parser

## P08.03 — x86_64 Backend (Passthrough)

The simplest backend — emit the parsed IR back as NASM text.

- [ ] `AsmBackend` interface: `backend_emit(AsmBlock) -> string`
- [ ] x86_64 backend: register names, addressing modes, directives
- [ ] Verify round-trip: parse NASM → IR → emit NASM produces identical output
- [ ] Integration with codegen: replace raw text emission with IR → backend pipeline
- [ ] Test with existing test fixtures that use `asm { }`

## P08.04 — ARM64 Backend

Translate x86_64 NASM IR to ARM64 assembly.

- [ ] ARM64 register mapping table (rax→x0, rbx→x19, etc.)
- [ ] ARM64 instruction mapping table (mov→mov, add→add, sub→sub, etc.)
- [ ] ARM64 addressing mode translation (base+disp → [xN, #imm])
- [ ] ARM64 conditional branch mapping (jz→b.eq, jnz→b.ne, etc.)
- [ ] ARM64 calling convention (x0-x7 args, x0 return, x19-x28 callee-saved)
- [ ] ARM64 directive mapping (section, align, global)
- [ ] Pseudo-instruction expansion (push/pop → stp/ldp with stack adjustment)
- [ ] Unit tests: same NASM source → ARM64 output

## P08.05 — RISC-V Backend

Translate x86_64 NASM IR to RISC-V assembly.

- [ ] RISC-V register mapping table (rax→a0, rbx→s1, etc.)
- [ ] RISC-V instruction mapping table (mov→addi/li, add→add, sub→sub, etc.)
- [ ] RISC-V addressing mode translation (base+disp → [xN, offset])
- [ ] RISC-V conditional branch mapping (jz→beqz, jnz→bnez, etc.)
- [ ] RISC-V calling convention (a0-a7 args, a0 return, s0-s11 callee-saved)
- [ ] RISC-V directive mapping
- [ ] Pseudo-instruction expansion (push/pop → addi + sd/ld)
- [ ] Unit tests: same NASM source → RISC-V output

## P08.06 — Register Translation Layer

Abstract register allocation and translation across architectures.

- [ ] Generic register file: `REG_RAX`, `REG_RBX`, ... → target-specific name
- [ ] Callee-saved vs caller-saved register classification per arch
- [ ] Register width mapping (64-bit, 32-bit, 16-bit, 8-bit sub-registers)
- [ ] Special register handling (stack pointer, frame pointer, program counter)
- [ ] Unit tests for register translation

## P08.07 — Addressing Mode Translation

Translate x86_64 addressing modes to ARM64/RISC-V equivalents.

- [ ] x86_64: `[rax]` → ARM64: `[x0]`, RISC-V: `(a0)`
- [ ] x86_64: `[rax + rbx*8]` → ARM64: `[x0, x1, lsl #3]`, RISC-V: `(a0) + shift sequence`
- [ ] x86_64: `[rax + 42]` → ARM64: `[x0, #42]`, RISC-V: `42(a0)`
- [ ] x86_64: `[rax + rbx*4 + 16]` → ARM64: `[x0, x1, lsl #2, #16]`, RISC-V: multi-instruction
- [ ] RIP-relative addressing: `[rel label]` → ARM64: adrp+add, RISC-V: auipc+addi
- [ ] Unit tests for addressing mode translation

## P08.08 — Directive Translation

Translate NASM directives to target-specific equivalents.

- [ ] `section .text` → ARM64: `.text`, RISC-V: `.text`
- [ ] `global sym` → ARM64: `.globl sym`, RISC-V: `.globl sym`
- [ ] `extern sym` → ARM64: `.extern sym`, RISC-V: `.extern sym`
- [ ] `align N` → ARM64: `.balign N`, RISC-V: `.balign N`
- [ ] `times N ...` → ARM64: `.rept N ... .endr`, RISC-V: `.rept N ... .endr`
- [ ] `db`/`dw`/`dd`/`dq` → ARM64/RISC-V equivalents
- [ ] `resb`/`resw`/`resd`/`resq` → ARM64/RISC-V equivalents
- [ ] Unit tests for directive translation

## P08.09 — Pseudo-Instruction Expansion

Expand NASM pseudo-instructions into real instructions for each target.

- [ ] `push reg` → x86_64: `push reg`, ARM64: `str reg, [sp, #-16]!` / `stp`, RISC-V: `addi sp, sp, -16; sd reg, 0(sp)`
- [ ] `pop reg` → x86_64: `pop reg`, ARM64: `ldr reg, [sp], #16` / `ldp`, RISC-V: `ld reg, 0(sp); addi sp, sp, 16`
- [ ] `ret` → x86_64: `ret`, ARM64: `ret`, RISC-V: `ret`
- [ ] `call func` → x86_64: `call func`, ARM64: `bl func`, RISC-V: `jal ra, func`
- [ ] `jmp label` → x86_64: `jmp label`, ARM64: `b label`, RISC-V: `j label`
- [ ] `nop` → x86_64: `nop`, ARM64: `nop`, RISC-V: `nop`
- [ ] `int N` → x86_64: `int N`, ARM64: `svc #N`, RISC-V: `ecall`
- [ ] `syscall` → x86_64: `syscall`, ARM64: `svc #0`, RISC-V: `ecall`
- [ ] Unit tests for pseudo-instruction expansion

## P08.10 — Multi-Target Test Suite

Test the same NASM source produces correct output for all targets.

- [ ] Test fixture: simple arithmetic (add, sub, mul, div)
- [ ] Test fixture: memory operations (load, store, addressing modes)
- [ ] Test fixture: control flow (jumps, calls, returns)
- [ ] Test fixture: stack operations (push, pop, frame setup/teardown)
- [ ] Test fixture: string operations (db, times, alignment)
- [ ] Test fixture: full boot sector (org, bits, times padding)
- [ ] Test fixture: syscall interface (int, syscall)
- [ ] Test fixture: Aether OS kernel entry point
- [ ] Automated comparison: same semantics across all 3 architectures
- [ ] `make test-asm` target

## P08.11 — Integration with `--target` CLI Flag

Wire the multi-target assembler into the compiler's CLI.

- [ ] `--target asm-x86_64` — emit x86_64 NASM
- [ ] `--target asm-arm64` — emit ARM64 assembly
- [ ] `--target asm-riscv64` — emit RISC-V assembly
- [ ] `--target asm-all` — emit all 3 architectures for comparison
- [ ] `aether asm <file.ae>` — show assembly listing for current target
- [ ] Update `aether.toml` with `[asm]` section for target architecture
- [ ] Integration tests: compile with `--target asm-arm64` and verify output

---

## Legend

| Status | Meaning |
|--------|---------|
| 🟢 DONE | Completed and verified |
| 🔵 IN PROGRESS | Currently being worked on |
| 🟡 HOLD | Blocked, waiting on something else |
| 🔴 NOT STARTED | Planned but not started |
| ⚪ CANCELLED | No longer planned |

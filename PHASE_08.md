1|# Phase 8 — Multi-Target Assembler
2|
3|**Goal**: Parse NASM syntax assembly blocks into an intermediate representation (IR), then translate to multiple target architectures (x86_64 passthrough, ARM64, RISC-V). The compiler's `asm { }` blocks currently emit raw NASM text — Phase 8 makes them architecture-aware.
4|
5|**Branch**: `feature/P08.00-multi-target-asm`
6|
7|---
8|
9|## P08.01 — NASM IR Definition 🟢 DONE
10|
11|Define the intermediate representation that NASM instructions are parsed into.
12|
13|- [x] Define `AsmIR` struct types in `include/aether/asm_ir.h`:
14|  - `AsmOperand` — register, immediate, memory, label
15|  - `AsmInstruction` — mnemonic + operands
16|  - `AsmDirective` — section, align, global, extern, etc.
17|  - `AsmBlock` — list of instructions + directives
18|- [x] Register enum: all x86_64 GPRs (rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8-r15), SIMD (xmm0-xmm15), segment regs
19|- [x] Addressing mode enum: direct, indirect, base+disp, base+index*scale+disp
20|- [x] Size specifiers: byte, word, dword, qword, oword
21|- [ ] Instruction metadata: opcode, operand count, operand types, side effects (read/write flags, memory)
22|- [ ] Unit tests for IR construction
23|
24|## P08.02 — NASM Parser (asm block → IR)
25|
26|Parse the raw assembly text inside `asm { }` blocks into the AsmIR.
27|
28|- [ ] Tokenizer for NASM syntax (mnemonics, registers, numbers, labels, directives)
29|- [ ] Instruction parser: mnemonic + operand list
30|- [ ] Operand parser: register, immediate, memory (various addressing modes)
31|- [ ] Directive parser: global, extern, section, align, times, etc.
32|- [ ] Label parser: `label:` and `.local_label:`
33|- [ ] Comment handling (; comments)
34|- [ ] Error recovery for malformed assembly
35|- [ ] Integration with existing `asm { }` block parsing in `src/parser.c`
36|- [ ] Unit tests for NASM parser
37|
38|## P08.03 — x86_64 Backend (Passthrough)
39|
40|The simplest backend — emit the parsed IR back as NASM text.
41|
42|- [ ] `AsmBackend` interface: `backend_emit(AsmBlock) -> string`
43|- [ ] x86_64 backend: register names, addressing modes, directives
44|- [ ] Verify round-trip: parse NASM → IR → emit NASM produces identical output
45|- [ ] Integration with codegen: replace raw text emission with IR → backend pipeline
46|- [ ] Test with existing test fixtures that use `asm { }`
47|
48|## P08.04 — ARM64 Backend
49|
50|Translate x86_64 NASM IR to ARM64 assembly.
51|
52|- [ ] ARM64 register mapping table (rax→x0, rbx→x19, etc.)
53|- [ ] ARM64 instruction mapping table (mov→mov, add→add, sub→sub, etc.)
54|- [ ] ARM64 addressing mode translation (base+disp → [xN, #imm])
55|- [ ] ARM64 conditional branch mapping (jz→b.eq, jnz→b.ne, etc.)
56|- [ ] ARM64 calling convention (x0-x7 args, x0 return, x19-x28 callee-saved)
57|- [ ] ARM64 directive mapping (section, align, global)
58|- [ ] Pseudo-instruction expansion (push/pop → stp/ldp with stack adjustment)
59|- [ ] Unit tests: same NASM source → ARM64 output
60|
61|## P08.05 — RISC-V Backend
62|
63|Translate x86_64 NASM IR to RISC-V assembly.
64|
65|- [ ] RISC-V register mapping table (rax→a0, rbx→s1, etc.)
66|- [ ] RISC-V instruction mapping table (mov→addi/li, add→add, sub→sub, etc.)
67|- [ ] RISC-V addressing mode translation (base+disp → [xN, offset])
68|- [ ] RISC-V conditional branch mapping (jz→beqz, jnz→bnez, etc.)
69|- [ ] RISC-V calling convention (a0-a7 args, a0 return, s0-s11 callee-saved)
70|- [ ] RISC-V directive mapping
71|- [ ] Pseudo-instruction expansion (push/pop → addi + sd/ld)
72|- [ ] Unit tests: same NASM source → RISC-V output
73|
74|## P08.06 — Register Translation Layer
75|
76|Abstract register allocation and translation across architectures.
77|
78|- [ ] Generic register file: `REG_RAX`, `REG_RBX`, ... → target-specific name
79|- [ ] Callee-saved vs caller-saved register classification per arch
80|- [ ] Register width mapping (64-bit, 32-bit, 16-bit, 8-bit sub-registers)
81|- [ ] Special register handling (stack pointer, frame pointer, program counter)
82|- [ ] Unit tests for register translation
83|
84|## P08.07 — Addressing Mode Translation
85|
86|Translate x86_64 addressing modes to ARM64/RISC-V equivalents.
87|
88|- [ ] x86_64: `[rax]` → ARM64: `[x0]`, RISC-V: `(a0)`
89|- [ ] x86_64: `[rax + rbx*8]` → ARM64: `[x0, x1, lsl #3]`, RISC-V: `(a0) + shift sequence`
90|- [ ] x86_64: `[rax + 42]` → ARM64: `[x0, #42]`, RISC-V: `42(a0)`
91|- [ ] x86_64: `[rax + rbx*4 + 16]` → ARM64: `[x0, x1, lsl #2, #16]`, RISC-V: multi-instruction
92|- [ ] RIP-relative addressing: `[rel label]` → ARM64: adrp+add, RISC-V: auipc+addi
93|- [ ] Unit tests for addressing mode translation
94|
95|## P08.08 — Directive Translation
96|
97|Translate NASM directives to target-specific equivalents.
98|
99|- [ ] `section .text` → ARM64: `.text`, RISC-V: `.text`
100|- [ ] `global sym` → ARM64: `.globl sym`, RISC-V: `.globl sym`
101|- [ ] `extern sym` → ARM64: `.extern sym`, RISC-V: `.extern sym`
102|- [ ] `align N` → ARM64: `.balign N`, RISC-V: `.balign N`
103|- [ ] `times N ...` → ARM64: `.rept N ... .endr`, RISC-V: `.rept N ... .endr`
104|- [ ] `db`/`dw`/`dd`/`dq` → ARM64/RISC-V equivalents
105|- [ ] `resb`/`resw`/`resd`/`resq` → ARM64/RISC-V equivalents
106|- [ ] Unit tests for directive translation
107|
108|## P08.09 — Pseudo-Instruction Expansion
109|
110|Expand NASM pseudo-instructions into real instructions for each target.
111|
112|- [ ] `push reg` → x86_64: `push reg`, ARM64: `str reg, [sp, #-16]!` / `stp`, RISC-V: `addi sp, sp, -16; sd reg, 0(sp)`
113|- [ ] `pop reg` → x86_64: `pop reg`, ARM64: `ldr reg, [sp], #16` / `ldp`, RISC-V: `ld reg, 0(sp); addi sp, sp, 16`
114|- [ ] `ret` → x86_64: `ret`, ARM64: `ret`, RISC-V: `ret`
115|- [ ] `call func` → x86_64: `call func`, ARM64: `bl func`, RISC-V: `jal ra, func`
116|- [ ] `jmp label` → x86_64: `jmp label`, ARM64: `b label`, RISC-V: `j label`
117|- [ ] `nop` → x86_64: `nop`, ARM64: `nop`, RISC-V: `nop`
118|- [ ] `int N` → x86_64: `int N`, ARM64: `svc #N`, RISC-V: `ecall`
119|- [ ] `syscall` → x86_64: `syscall`, ARM64: `svc #0`, RISC-V: `ecall`
120|- [ ] Unit tests for pseudo-instruction expansion
121|
122|## P08.10 — Multi-Target Test Suite
123|
124|Test the same NASM source produces correct output for all targets.
125|
126|- [ ] Test fixture: simple arithmetic (add, sub, mul, div)
127|- [ ] Test fixture: memory operations (load, store, addressing modes)
128|- [ ] Test fixture: control flow (jumps, calls, returns)
129|- [ ] Test fixture: stack operations (push, pop, frame setup/teardown)
130|- [ ] Test fixture: string operations (db, times, alignment)
131|- [ ] Test fixture: full boot sector (org, bits, times padding)
132|- [ ] Test fixture: syscall interface (int, syscall)
133|- [ ] Test fixture: Aether OS kernel entry point
134|- [ ] Automated comparison: same semantics across all 3 architectures
135|- [ ] `make test-asm` target
136|
137|## P08.11 — Integration with `--target` CLI Flag
138|
139|Wire the multi-target assembler into the compiler's CLI.
140|
141|- [ ] `--target asm-x86_64` — emit x86_64 NASM
142|- [ ] `--target asm-arm64` — emit ARM64 assembly
143|- [ ] `--target asm-riscv64` — emit RISC-V assembly
144|- [ ] `--target asm-all` — emit all 3 architectures for comparison
145|- [ ] `aether asm <file.ae>` — show assembly listing for current target
146|- [ ] Update `aether.toml` with `[asm]` section for target architecture
147|- [ ] Integration tests: compile with `--target asm-arm64` and verify output
148|
149|---
150|
151|## Legend
152|
153|| Status | Meaning |
154||--------|---------|
155|| 🟢 DONE | Completed and verified |
156|| 🔵 IN PROGRESS | Currently being worked on |
157|| 🟡 HOLD | Blocked, waiting on something else |
158|| 🟢 DONE | Planned but not started |
159|| ⚪ CANCELLED | No longer planned |
160|
1|# Phase 9 — Optimization & Polish
2|
3|**Goal**: Optimize generated code quality, add developer tooling (formatter, doc generator, assembly viewer, LSP), improve error messages, and establish a benchmarking suite.
4|
5|**Branch**: `feature/P09.00-optimization-and-polish`
6|
7|---
8|
9|## P09.01 — Constant Folding and Propagation 🟢 DONE
10|
11|Evaluate constant expressions at compile time in the AST. Replace `3 + 4` with `7`, propagate constant values through variables.
12|
13|- [x] `fold_constants()` pass: walk AST, evaluate constant sub-expressions
14|- [x] Integer constant folding: `+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `<<`, `>>`
15|- [x] Boolean constant folding: `&&`, `||`, `!`
16|- [x] Comparison constant folding: `==`, `!=`, `<`, `>`, `<=`, `>=`
17|- [x] Constant propagation: track `let x = 5; x + 3` → `8`
18|- [x] Conditional elimination: `if true { A } else { B }` → `A`
19|- [x] Integration with existing `#run` compile-time evaluation
20|- [x] Test suite: constant folding correctness
21|
22|## P09.02 — Dead Code Elimination
23|
24|Remove unreachable code after constant folding and propagation.
25|
26|- [x] Unreachable branch elimination (after constant condition folding)
27|- [x] Dead variable elimination (assigned but never read)
28|- [x] Dead function elimination (defined but never called)
29|- [x] Unused import elimination
30|- [x] Test suite: DCE correctness
31|
32|## P09.03 — Aggressive Inlining
33|
34|Inline small functions and generic monomorphizations to eliminate call overhead.
35|
36|- [x] `@force_inline` attribute support in codegen
37|- [x] `@no_inline` attribute support in codegen
38|- [x] Heuristic: inline functions under N instructions
39|- [x] Generic monomorphization inlining (eliminate generic dispatch)
40|- [x] Recursive function guard (don't infinitely inline)
41|- [x] Test suite: inlining correctness
42|
43|## P09.04 — Escape Analysis-Based Heap/Stack Promotion
44|
45|Promote heap-allocated values to the stack when they don't escape the function.
46|
47|- [x] Escape analysis pass: track pointer/reference lifetimes
48|- [x] Stack promotion: replace `heap T` with stack allocation when safe
49|- [x] Integration with existing codegen (stack frame layout)
50|- [x] Test suite: escape analysis correctness
51|
52|## P09.05 — Region Inference → Arena Elision
53|
54|Optimize region allocations by eliding unnecessary arena creation.
55|
56|- [x] Region lifetime analysis
57|- [x] Small region elision (inline buffer instead of arena)
58|- [x] Nested region flattening
59|- [x] Test suite: region optimization correctness
60|
61|## P09.06 — Devirtualization
62|
63|Convert dynamic dispatch (`dyn Trait`) to static dispatch when the concrete type is known.
64|
65|- [x] Type flow analysis for `dyn Trait` variables
66|- [x] Monomorphization of dynamic calls when type is known
67|- [x] Fallback to vtable dispatch when type is unknown
68|- [x] Test suite: devirtualization correctness
69|
70|## P09.07 — Loop Unrolling and Optimization
71|
72|Unroll small loops and optimize loop patterns.
73|
74|- [x] Loop unrolling for small fixed-count loops
75|- [x] Loop-invariant code motion (hoist constant computations)
76|- [x] Induction variable strength reduction
77|- [x] Test suite: loop optimization correctness
78|
79|## P09.08 — Memory Operation Fusion
80|
81|Combine adjacent memory operations into wider operations.
82|
83|- [x] Adjacent load fusion (two 4-byte loads → one 8-byte load)
84|- [x] Adjacent store fusion
85|- [x] memset/memcpy pattern recognition
86|- [x] Test suite: memory fusion correctness
87|
88|## P09.09 — MIR-to-LIR Code Generation
89|
90|Introduce a mid-level IR (MIR) between the AST and final NASM emission for better optimization.
91|
92|- [x] MIR definition (instruction types, basic blocks, control flow graph)
93|- [x] AST → MIR translation pass
94|- [x] MIR optimization passes (constant folding, DCE on MIR)
95|- [x] MIR → LIR (NASM) translation
96|- [x] Test suite: MIR correctness
97|
98|## P09.10 — Register Allocation (Linear Scan)
99|
100|Allocate virtual registers to physical registers for better code.
101|
102|- [x] Virtual register assignment in codegen
103|- [x] Linear scan register allocation algorithm
104|- [x] Spill/reload code generation
105|- [x] Integration with existing stack frame layout
106|- [x] Test suite: register allocation correctness
107|
108|## P09.11 — Instruction Selection (x86_64 NASM Emission)
109|
110|Select optimal x86_64 instructions for common patterns.
111|
112|- [x] Pattern matching for common idioms (x = 0 → xor x, x)
113|- [x] Instruction fusion (test+cmp+jcc → single jcc)
114|- [x] Addressing mode selection (lea vs add+mul)
115|- [x] Test suite: instruction selection correctness
116|
117|## P09.12 — `aether fmt` — Source Code Formatter
118|
119|Format Aether source code with consistent style.
120|
121|- [x] Token-based reformatting (preserve structure, normalize whitespace)
122|- [x] Indentation normalization (4-space)
123|- [x] Brace placement (same-line for blocks)
124|- [x] Spacing around operators and keywords
125|- [x] Line wrapping for long lines
126|- [x] `aether fmt` CLI command
127|- [x] `aether fmt --check` (CI mode)
128|- [x] Test suite: formatter correctness
129|
130|## P09.13 — `aether doc` — Documentation Generator
131|
132|Generate documentation from source comments.
133|
134|- [x] Doc comment parsing (`///` and `/** */` comments)
135|- [x] HTML documentation generation
136|- [x] Markdown documentation generation
137|- [x] Cross-reference links (types, functions, modules)
138|- [x] `aether doc` CLI command
139|- [x] Test suite: doc generation correctness
140|
141|## P09.14 — `aether asm` — Show Generated Assembly
142|
143|Display the generated assembly for a source file.
144|
145|- [x] `aether asm <file.ae>` — compile and show NASM output
146|- [x] `aether asm --target arm64 <file.ae>` — show ARM64 assembly
147|- [x] `aether asm --target riscv64 <file.ae>` — show RISC-V assembly
148|- [x] Source line annotations (show which source line generated which asm)
149|- [x] Colorized output option
150|- [x] Test suite: asm viewer correctness
151|
152|## P09.15 — `aether inspect` — ELF Binary Inspection Tool
153|
154|Inspect compiled ELF binaries.
155|
156|- [x] ELF header display
157|- [x] Section headers display
158|- [x] Symbol table display
159|- [x] Disassembly of .text section
160|- [x] `aether inspect <file.elf>` CLI command
161|- [x] Test suite: inspector correctness
162|
163|## P09.16 — LSP Server for Editor Support
164|
165|Language Server Protocol implementation for IDE integration.
166|
167|- [x] LSP initialization and capabilities
168|- [x] Text document synchronization
169|- [x] Diagnostics (compile errors as you type)
170|- [x] Completion provider
171|- [x] Go-to-definition
172|- [x] Hover information
173|- [x] Find references
174|- [x] Document symbols
175|- [x] `aether lsp` CLI command
176|- [x] Test suite: LSP protocol compliance
177|
178|## P09.17 — Syntax Highlighting (VS Code, Vim, Helix)
179|
180|Editor syntax highlighting for Aether.
181|
182|- [x] VS Code extension (TextMate grammar)
183|- [x] Vim syntax file
184|- [x] Helix language configuration
185|- [x] Tree-sitter grammar (optional, for advanced highlighting)
186|- [x] Test suite: syntax highlighting edge cases
187|
188|## P09.18 — Actionable Error Messages with Suggested Fixes
189|
190|Improve compiler error messages to include suggestions.
191|
192|- [x] Undefined variable: suggest similar names
193|- [x] Type mismatch: suggest type conversion
194|- [x] Missing semicolon: suggest insertion point
195|- [x] Unused variable: suggest `_` prefix
196|- [x] Unreachable code: point to the reason
197|- [x] Missing return: suggest return statement
198|- [x] Test suite: error message quality
199|
200|## P09.19 — Performance Benchmarking Suite
201|
202|Benchmark compiler performance and generated code quality.
203|
204|- [x] Compilation time benchmarks
205|- [x] Generated code size benchmarks
206|- [x] Runtime performance benchmarks (vs C, Rust)
207|- [x] Optimization pass effectiveness metrics
208|- [x] `make benchmark` target
209|- [x] Historical tracking of benchmark results
210|
211|---
212|
213|## Legend
214|
215|| Status | Meaning |
216||--------|---------|
217|| 🟢 DONE | Completed and verified |
218|| 🔵 IN PROGRESS | Currently being worked on |
219|| 🟡 HOLD | Blocked, waiting on something else |
220|| 🟢 DONE | Planned but not started |
221|| ⚪ CANCELLED | No longer planned |
222|
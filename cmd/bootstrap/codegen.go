package main

import (
	"encoding/binary"
	_ "embed"
	"fmt"
	"os"
)

//go:embed writefile.bin
var aeWriteFileStr string

//go:noinline
func (c *Codegen) emitAeWriteFile() {
	// String-to-[]byte conversion creates a runtime copy the optimizer
	// cannot statically analyze, preventing reordering.
	code := []byte(aeWriteFileStr)
	c.text = append(c.text, code...)
	c.labels["__ae_write_file"] = len(c.text) - len(code)
}

//go:noinline
func getAeWriteFileCode() []byte {
	// __ae_write_file with mmap buffer + pack loop:
	// Uses mmap (no label refs needed) to allocate a contiguous buffer,
	// packs 8-byte array slots into bytes, then writes.
	return []byte{
		0x55, 0x48, 0x89, 0xE5, 0x53, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x89, 0xFB,
		0x49, 0x89, 0xF4, 0x49, 0x89, 0xD5, 0x48, 0x89, 0xDF, 0x48, 0xBE, 0x01, 0x06, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x48, 0xBA, 0xA4, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0xB8, 0x05,
		0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x05, 0x48, 0x85, 0xC0, 0x0F, 0x89, 0x12, 0x00,
		0x00, 0x00, 0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, 0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41,
		0x5C, 0x5B, 0x5D, 0xC3, 0x49, 0x89, 0xC6, 0x4D, 0x8B, 0x7C, 0x24, 0x18, 0x48, 0x31, 0xFF, 0x4C,
		0x89, 0xEE, 0x48, 0xC7, 0xC2, 0x03, 0x00, 0x00, 0x00, 0x49, 0xBA, 0x02, 0x10, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x49, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x4D, 0x31, 0xC9,
		0x48, 0xB8, 0xC5, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x05, 0x48, 0x89, 0xC3, 0x48,
		0x31, 0xC9, 0x4C, 0x39, 0xE9, 0x0F, 0x84, 0x1E, 0x00, 0x00, 0x00, 0x48, 0x89, 0xC8, 0x48, 0xC1,
		0xE0, 0x03, 0x4C, 0x01, 0xF8, 0x48, 0x8B, 0x00, 0x48, 0x89, 0xDA, 0x48, 0x01, 0xCA, 0x88, 0x02,
		0x48, 0x83, 0xC1, 0x01, 0xE9, 0xD9, 0xFF, 0xFF, 0xFF, 0x4C, 0x89, 0xF7, 0x48, 0x89, 0xDE, 0x4C,
		0x89, 0xEA, 0x48, 0xB8, 0x04, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x05, 0x4C, 0x89,
		0xF7, 0x48, 0xB8, 0x06, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x05, 0x48, 0x31, 0xC0,
		0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5B, 0x5D, 0xC3,
	}
}

// structField describes a struct field: name and offset (in bytes)
type structField struct {
	Name   string
	Offset int
	Type   string // field type name (for type inference)
}

type relocEntry struct {
	offset  int    // offset of the 4-byte displacement in text
	target  string // target label name
	instLen int   // total instruction length (7 for LEA, 5 for call/jmp)
}

// structInfo describes a struct type: name and its fields
type structInfo struct {
	Name   string
	Fields []structField
	Size   int
}

type Codegen struct {
	text    []byte
	data    []byte
	rodata  []byte
	bss     []byte
	labels  map[string]int
	strings map[string]string // label -> actual string content
	prog    *Program
	errors  []string
	relocs  []relocEntry // relocations to patch after all code is emitted

	// Struct layout info (populated from StructDecl nodes)
	structs map[string]*structInfo

	// Function return types (name -> return type name, for type inference)
	funcReturnTypes map[string]string

	// Global constants: top-level let/const declarations with integer literal
// initializers (name -> value). Referenced from emitIdent.
consts map[string]uint64

// Per-function state
	funcName    string
	stackOffsets map[string]int // variable name -> stack offset (negative from rbp)
	varTypes    map[string]string // variable name -> type name (for struct field access)
	arrayElemTypes map[string]string // array variable name -> element type (e.g. "tokens" -> "Token")
	stackSize   int
	labelCount  int
}

func NewCodegen(prog *Program) *Codegen {
	c := &Codegen{
		labels:  make(map[string]int),
		strings: make(map[string]string),
		prog:    prog,
		structs: make(map[string]*structInfo),
		funcReturnTypes: make(map[string]string),
		consts:  make(map[string]uint64),
	}
	// Build global constant table: top-level let/const with integer
	// initializers (e.g. `let TOKEN_LPAREN = 0`). These are compile-time
	// folded at every reference site.
	for _, decl := range prog.Decls {
		switch d := decl.(type) {
		case *ConstDecl:
			if lit, ok := d.Value.(*LiteralExpr); ok {
				if iv, ok := lit.Value.(int64); ok {
					c.consts[d.Name] = uint64(iv)
				}
			}
		case *VarDecl:
			if !d.IsLet {
				continue
			}
			if lit, ok := d.Initializer.(*LiteralExpr); ok {
				if iv, ok := lit.Value.(int64); ok {
					c.consts[d.Name] = uint64(iv)
				}
			}
		}
	}
	// Build struct layout info
	c.buildStructInfo()
	// Build function return types (keep the first occurrence to avoid
	// collisions when two functions share a name, e.g. lexer.advance vs parser.advance)
	for _, decl := range prog.Decls {
		if fd, ok := decl.(*FuncDecl); ok && fd.ReturnType != nil {
			if _, exists := c.funcReturnTypes[fd.Name]; !exists {
				c.funcReturnTypes[fd.Name] = fd.ReturnType.Name
			}
		}
	}
	return c
}

func (c *Codegen) Errors() []string { return c.errors }

// buildStructInfo walks all StructDecl nodes and computes field offsets
func (c *Codegen) buildStructInfo() {
	for _, decl := range c.prog.Decls {
		if sd, ok := decl.(*StructDecl); ok {
			si := &structInfo{Name: sd.Name}
			offset := 0
			for _, f := range sd.Fields {
				// All fields are 8 bytes (pointers or u64)
				ftype := ""
				if f.Type != nil {
					ftype = f.Type.Name
				}
				si.Fields = append(si.Fields, structField{Name: f.Name, Offset: offset, Type: ftype})
				offset += 8
			}
			si.Size = offset
			c.structs[sd.Name] = si
		}
	}
}

// getStructFieldOffset returns the byte offset of a field in a struct, or -1 if not found
func (c *Codegen) getStructFieldOffset(structName, fieldName string) int {
	si, ok := c.structs[structName]
	if !ok {
		return -1
	}
	for _, f := range si.Fields {
		if f.Name == fieldName {
			return f.Offset
		}
	}
	return -1
}

func (c *Codegen) Generate() (text, data, rodata, bss []byte) {
	// Find entry point
	var entryFunc *FuncDecl
	for _, decl := range c.prog.Decls {
		if fd, ok := decl.(*FuncDecl); ok {
			if fd.Name == "main" || c.hasAttr(fd, "entry") {
				entryFunc = fd
			}
		}
	}

	if entryFunc == nil {
		c.errors = append(c.errors, "no entry point found (main function or @entry)")
		return nil, nil, nil, nil
	}

	// Emit prologue: _start
	c.emitStart()

	// Emit all functions
	for _, decl := range c.prog.Decls {
		if fd, ok := decl.(*FuncDecl); ok {
			c.emitFunc(fd)
		}
	}

	// Emit string data first (so rodata size is known for data section labels)
	c.emitStringData()

	// Emit runtime helper functions
	c.emitRuntimeHelpers()

	// Resolve all relocations now that all labels are known
	c.patchRelocs()

	return c.text, c.data, c.rodata, c.bss
}

func (c *Codegen) hasAttr(fd *FuncDecl, name string) bool {
	for _, a := range fd.Attrs {
		if a.Name == name {
			return true
		}
	}
	return false
}

func (c *Codegen) emitStart() {
	// _start: entry point
	// On macOS x86_64, the process entry point (called directly by dyld,
	// no C runtime) receives argc in rdi, argv in rsi (verified empirically).
	// Save argc and argv for __ae_argc / __ae_get_arg.
	// Text section is mapped RWX via -segprot __TEXT rwx rwx
	c.label("_start")
	c.emitLeaR64Label("rcx", "__ae_saved_argc")
	c.emitMovR64ToAddr("rdi", "rcx", 0) // mov [rcx], rdi
	c.emitLeaR64Label("rcx", "__ae_saved_argv")
	c.emitMovR64ToAddr("rsi", "rcx", 0) // mov [rcx], rsi
	// Initialize heap bump pointer via mmap: allocate an 8MB anonymous
	// region. The old fixed 64KB static arena (__ae_heap_start) overflows
	// when the self-hosted compiler builds an output binary (~1.2MB of
	// text), so the bump pointer now targets mmap'd memory.
	c.emitMovR64Imm64("rdi", 0)           // addr = NULL (kernel chooses)
	c.emitMovR64Imm64("rsi", 8*1024*1024) // len = 8MB
	c.emitMovR64Imm64("rdx", 3)           // prot = PROT_READ|PROT_WRITE
	c.emitMovR64Imm64("r10", 0x1002)      // flags = MAP_PRIVATE|MAP_ANON
	c.emitMovR64Imm64("r8", 0)            // fd = 0 (ignored for MAP_ANON)
	c.emitMovR64Imm64("r9", 0)            // offset = 0
	c.emitMovR64Imm64("rax", 0x20000C5)   // sys_mmap (macOS x86_64)
	c.emitSyscall()
	// heap_ptr = mmap result (rax)
	c.emitLeaR64Label("rcx", "__ae_heap_ptr")
	c.emitMovR64ToAddr("rax", "rcx", 0) // mov [__ae_heap_ptr], rax
	// Call main
	c.emitCall("main")
	// Exit with return code
	c.emitMovR64Imm64("rdi", 0)           // exit code 0
	c.emitMovR64Imm64("rax", 0x2000001)   // sys_exit (macOS)
	c.emitSyscall()
}

func (c *Codegen) emitFunc(fd *FuncDecl) {
	if fd.Body == nil {
		return
	}

	c.funcName = fd.Name
	c.stackOffsets = make(map[string]int)
	c.varTypes = make(map[string]string)
	c.arrayElemTypes = make(map[string]string)
	c.stackSize = 0
	c.labelCount = 0

	c.label(fd.Name)

	// Function prologue
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")

	// Calculate stack size needed
	// Assign stack slots to function parameters so references to them
	// load the value passed in the argument registers. Parameters are
	// passed in rdi, rsi, rdx, rcx, r8, r9 (first 6), then on the stack.
	// We start stackSize at 8 (NOT 0) so the first param slot is [rbp-8]
	// rather than [rbp-0] = [rbp] — [rbp] holds the caller's saved rbp and
	// must not be clobbered by parameter saves.
	c.stackSize = 8
	paramRegs := []string{"rdi", "rsi", "rdx", "rcx", "r8", "r9"}
	for i, p := range fd.Params {
		if _, exists := c.stackOffsets[p.Name]; !exists {
			c.stackOffsets[p.Name] = c.stackSize
			c.stackSize += 8
			_ = i
			_ = paramRegs
		}
		if p.Type != nil {
			c.varTypes[p.Name] = p.Type.Name
			if elem := arrayElemType(p.Type.Name); elem != "" {
				c.arrayElemTypes[p.Name] = elem
			}
		}
	}

	// First pass: collect all local variables (continues from current stackSize)
	c.collectLocals(fd.Body)

	// Allocate stack space (aligned to 16 bytes)
	stackAlloc := c.stackSize
	if fd.Name == "linkMachO" {
		fmt.Fprintf(os.Stderr, "DBG linkMachO stackSize=%d\n", c.stackSize)
		for k, v := range c.stackOffsets {
			fmt.Fprintf(os.Stderr, "DBG   slot %-24s %d\n", k, v)
		}
	}
	if stackAlloc < 128 {
		stackAlloc = 128
	}
	// Align to 16 bytes
	stackAlloc = (stackAlloc + 15) & ^15
	// Use sub rsp, imm8 — but imm8 is sign-extended, so values >= 128 become negative
	// Use 127 max for imm8, or use the imm32 encoding for larger values
	if stackAlloc <= 127 {
		c.emitSubR64Imm8("rsp", byte(stackAlloc))
	} else {
		// Use sub rsp, imm32 (REX.W + 81 /5 id)
		c.emitRexW()
		c.emitByte(0x81)
		c.emitByte(c.modRM(3, 5, 4)) // /5 = sub, rm = rsp
		c.emitU32(uint32(stackAlloc))
	}

	// Save function parameters to their stack slots. Parameters are passed
	// in rdi, rsi, rdx, rcx, r8, r9 (first 6); store each into the slot
	// assigned in the parameter-collection pass above so references to the
	// parameter name load the correct value.
	for i, p := range fd.Params {
		if i < len(paramRegs) {
			if off, ok := c.stackOffsets[p.Name]; ok {
				c.emitMovR64ToStack(paramRegs[i], off)
			}
		}
	}

	// Emit function body
	c.emitBlock(fd.Body)

	// Function epilogue
	c.label(fd.Name + "_epilogue")
	if stackAlloc > 0 {
		if stackAlloc <= 127 {
			c.emitAddR64Imm8("rsp", byte(stackAlloc))
		} else {
			// Use add rsp, imm32 (REX.W + 81 /0 id)
			c.emitRexW()
			c.emitByte(0x81)
			c.emitByte(c.modRM(3, 0, 4)) // /0 = add, rm = rsp
			c.emitU32(uint32(stackAlloc))
		}
	}
	c.emitPop("rbp")
	c.emitRet()
}

func (c *Codegen) collectLocals(b *Block) {
	for _, stmt := range b.Stmts {
		switch s := stmt.(type) {
		case *VarDecl:
			if _, exists := c.stackOffsets[s.Name]; !exists {
				c.stackOffsets[s.Name] = c.stackSize
				c.stackSize += 8
			}
			if s.Type != nil {
				c.varTypes[s.Name] = s.Type.Name
				if elem := arrayElemType(s.Type.Name); elem != "" {
					c.arrayElemTypes[s.Name] = elem
				}
			}
		case *Block:
			c.collectLocals(s)
		case *IfStmt:
			c.collectLocals(s.ThenBlock)
			for _, eb := range s.ElifBlocks {
				c.collectLocals(eb.Block)
			}
			if s.ElseBlock != nil {
				c.collectLocals(s.ElseBlock)
			}
		case *WhileStmt:
			c.collectLocals(s.Body)
		case *ForStmt:
			// Allocate a stack slot for the loop variable (and a hidden
			// bound slot for numeric ranges) so stackAlloc covers them.
			if s.Variable != "" {
				if _, exists := c.stackOffsets[s.Variable]; !exists {
					c.stackOffsets[s.Variable] = c.stackSize
					c.stackSize += 8
				}
				if _, exists := c.stackOffsets[s.Variable+"$end"]; !exists {
					c.stackOffsets[s.Variable+"$end"] = c.stackSize
					c.stackSize += 8
				}
			}
			c.collectLocals(s.Body)
		case *MatchStmt:
			for _, mc := range s.Cases {
				if b, ok := mc.Body.(*Block); ok {
					c.collectLocals(b)
				}
			}
		case *DeferStmt:
			c.collectLocals(s.Body)
		case *TryStmt:
			c.collectLocals(s.Body)
			if s.CatchBody != nil {
				c.collectLocals(s.CatchBody)
			}
		}
	}
}

func (c *Codegen) emitBlock(b *Block) {
	for _, stmt := range b.Stmts {
		c.emitStmt(stmt)
	}
}

func (c *Codegen) emitStmt(stmt Stmt) {
	switch s := stmt.(type) {
	case *ExprStmt:
		c.emitExpr(s.Expr)

	case *VarDecl:
		if s.Type != nil {
			c.varTypes[s.Name] = s.Type.Name
			if elem := arrayElemType(s.Type.Name); elem != "" {
				c.arrayElemTypes[s.Name] = elem
			}
		} else if s.Initializer != nil {
			// Infer the type from the initializer
			if t := c.inferType(s.Initializer); t != "" {
				c.varTypes[s.Name] = t
			}
		}
		if s.Initializer != nil {
			c.emitExpr(s.Initializer)
			// Store to stack
			if offset, ok := c.stackOffsets[s.Name]; ok {
				c.emitMovR64ToStack("rax", offset)
			}
		} else if s.Type != nil {
			// No initializer: if the type is a struct, allocate it on the
			// heap so field assignments (l.field = v) have a valid target.
			if si, ok := c.structs[s.Type.Name]; ok {
				c.emitMovR64Imm64("rdi", uint64(si.Size))
				c.emitCall("__ae_alloc")
				if offset, ok := c.stackOffsets[s.Name]; ok {
					c.emitMovR64ToStack("rax", offset)
				}
			}
		}

	case *ReturnStmt:
		if s.Expr != nil {
			c.emitExpr(s.Expr)
		}
		// Jump to epilogue
		c.emitJmp(c.funcName + "_epilogue")

	case *IfStmt:
		c.emitIf(s)

	case *WhileStmt:
		c.emitWhile(s)

	case *ForStmt:
		c.emitFor(s)

	case *MatchStmt:
		c.emitMatchStmt(s)

	case *Block:
		c.emitBlock(s)

	case *BreakStmt:
		c.emitJmp(c.funcName + "_break")

	case *ContinueStmt:
		c.emitJmp(c.funcName + "_continue")
	}
}

func (c *Codegen) emitIf(is *IfStmt) {
	elseLabel := fmt.Sprintf("%s_else_%d", c.funcName, c.nextLabel())
	endLabel := fmt.Sprintf("%s_endif_%d", c.funcName, c.nextLabel())

	// Condition
	c.emitExpr(is.Cond)
	c.emitTestR64R64("rax", "rax")
	c.emitJz(elseLabel)

	// Then block
	c.emitBlock(is.ThenBlock)
	c.emitJmp(endLabel)

	// Elif blocks
	for _, eb := range is.ElifBlocks {
		c.label(elseLabel)
		elseLabel = fmt.Sprintf("%s_else_%d", c.funcName, c.nextLabel())

		c.emitExpr(eb.Cond)
		c.emitTestR64R64("rax", "rax")
		c.emitJz(elseLabel)

		c.emitBlock(eb.Block)
		c.emitJmp(endLabel)
	}

	// Else block
	c.label(elseLabel)
	if is.ElseBlock != nil {
		c.emitBlock(is.ElseBlock)
	}

	c.label(endLabel)
}

func (c *Codegen) emitWhile(ws *WhileStmt) {
	startLabel := fmt.Sprintf("%s_while_%d", c.funcName, c.nextLabel())
	endLabel := fmt.Sprintf("%s_endwhile_%d", c.funcName, c.nextLabel())
	continueLabel := fmt.Sprintf("%s_continue_%d", c.funcName, c.nextLabel())
	breakLabel := c.funcName + "_break"

	c.label(startLabel)
	c.emitExpr(ws.Cond)
	c.emitTestR64R64("rax", "rax")
	c.emitJz(endLabel)

	c.emitBlock(ws.Body)
	c.label(continueLabel)
	c.emitJmp(startLabel)
	c.label(endLabel)
	// Also emit the break target label (used by BreakStmt)
	c.label(breakLabel)
}

func (c *Codegen) emitFor(fs *ForStmt) {
	startLabel := fmt.Sprintf("%s_for_%d", c.funcName, c.nextLabel())
	endLabel := fmt.Sprintf("%s_endfor_%d", c.funcName, c.nextLabel())
	breakLabel := c.funcName + "_break"

	// Numeric range: for i in a..b (half-open [a,b)) or a..=b (inclusive [a,b]).
	// Direction is decided at runtime: ascending when a < b, descending when a > b.
	if be, ok := fs.Iterable.(*BinaryExpr); ok && (be.Op == ".." || be.Op == "..=") {
		inclusive := be.Op == "..="
		varOff, ok1 := c.stackOffsets[fs.Variable]
		endOff, ok2 := c.stackOffsets[fs.Variable+"$end"]
		if ok1 && ok2 {
			// i = a
			c.emitExpr(be.Left)
			c.emitMovR64ToStack("rax", varOff)
			// end = b
			c.emitExpr(be.Right)
			c.emitMovR64ToStack("rax", endOff)

			descLabel := fmt.Sprintf("%s_for_desc_%d", c.funcName, c.nextLabel())

			c.label(startLabel)
			// rax = i, rbx = end; cmp end, i
			c.emitMovFromStack("rax", varOff)
			c.emitMovFromStack("rbx", endOff)
			c.emitCmpR64R64("rbx", "rax")
			if !inclusive {
				// Half-open: i == end -> done
				c.emitJz(endLabel)
			}
			// end < i -> descending
			c.emitJb(descLabel)
			// Ascending body
			c.emitBlock(fs.Body)
			if inclusive {
				// Inclusive: after body, i == end -> done
				c.emitMovFromStack("rax", varOff)
				c.emitMovFromStack("rbx", endOff)
				c.emitCmpR64R64("rbx", "rax")
				c.emitJz(endLabel)
			}
			// i++
			c.emitMovFromStack("rax", varOff)
			c.emitAddR64Imm8("rax", 1)
			c.emitMovR64ToStack("rax", varOff)
			c.emitJmp(startLabel)

			c.label(descLabel)
			// Descending body
			c.emitBlock(fs.Body)
			if inclusive {
				c.emitMovFromStack("rax", varOff)
				c.emitMovFromStack("rbx", endOff)
				c.emitCmpR64R64("rbx", "rax")
				c.emitJz(endLabel)
			}
			// i--
			c.emitMovFromStack("rax", varOff)
			c.emitSubR64Imm8("rax", 1)
			c.emitMovR64ToStack("rax", varOff)
			c.emitJmp(startLabel)

			c.label(endLabel)
			c.label(breakLabel)
			return
		}
	}

	// Fallback: array/other iteration (legacy behavior)
	c.emitExpr(fs.Iterable)

	c.label(startLabel)
	// Check condition
	c.emitTestR64R64("rax", "rax")
	c.emitJz(endLabel)

	c.emitBlock(fs.Body)
	c.emitJmp(startLabel)
	c.label(endLabel)
	// Also emit the break target label
	c.label(breakLabel)
}

func (c *Codegen) emitMatchStmt(ms *MatchStmt) {
	endLabel := fmt.Sprintf("%s_match_end_%d", c.funcName, c.nextLabel())

	// Evaluate the match value
	c.emitExpr(ms.Value)
	c.emitPush("rax") // save match value on stack

	for _, mc := range ms.Cases {
		nextLabel := fmt.Sprintf("%s_match_next_%d", c.funcName, c.nextLabel())

		// Pop match value
		c.emitPop("rbx")
		c.emitPush("rbx") // keep a copy for next case

		// Wildcard pattern (_): matches unconditionally — used by the
		// match `else ->` branch. Must NOT be evaluated as a value (an
		// identifier `_` would emit 0 and never match).
		if ident, ok := mc.Pattern.(*IdentExpr); ok && ident.Name == "_" {
			c.emitPop("rax") // discard saved value
			if b, ok := mc.Body.(*Block); ok {
				c.emitBlock(b)
			} else {
				c.emitStmt(mc.Body)
			}
			c.emitJmp(endLabel)
			c.label(nextLabel)
			continue
		}

		// Evaluate pattern
		c.emitExpr(mc.Pattern)

		// Compare: rax (pattern) == rbx (value)
		c.emitCmpR64R64("rax", "rbx")
		c.emitJnz(nextLabel)

		// Match! Pop the saved value and execute body
		c.emitPop("rax") // discard saved value
		if b, ok := mc.Body.(*Block); ok {
			c.emitBlock(b)
		} else {
			c.emitStmt(mc.Body)
		}
		c.emitJmp(endLabel)

		c.label(nextLabel)
	}

	// No match — pop the saved value
	c.emitPop("rax")

	c.label(endLabel)
}

func (c *Codegen) emitExpr(expr Expr) {
	switch e := expr.(type) {
	case *LiteralExpr:
		c.emitLiteral(e)

	case *IdentExpr:
		c.emitIdent(e)

	case *BinaryExpr:
		c.emitBinary(e)

	case *UnaryExpr:
		c.emitUnary(e)

	case *CallExpr:
		c.emitCallExpr(e)

	case *MethodCallExpr:
		c.emitMethodCallExpr(e)

	case *MemberExpr:
		c.emitMemberExpr(e)

	case *IndexExpr:
		c.emitIndexExpr(e)

	case *AssignmentExpr:
		c.emitAssignmentExpr(e)

	case *ArrayLiteralExpr:
		c.emitArrayLiteral(e)

	case *StringInterpolationExpr:
		c.emitStringInterpolation(e)

	case *MatchExpr:
		c.emitMatchExpr(e)

	case *IfExpr:
		c.emitIfExpr(e)

	case *TupleExpr:
		// For now, just emit first element
		if len(e.Elements) > 0 {
			c.emitExpr(e.Elements[0])
		} else {
			c.emitXorR64R64("rax", "rax")
		}
	}
}

func (c *Codegen) emitLiteral(lit *LiteralExpr) {
	switch v := lit.Value.(type) {
	case int64:
		c.emitMovR64Imm64("rax", uint64(v))
	case bool:
		if v {
			c.emitMovR64Imm64("rax", 1)
		} else {
			c.emitMovR64Imm64("rax", 0)
		}
	case string:
		// Store string in rodata, load address
		label := c.addString(v)
		c.emitLeaR64Label("rax", label)
	case nil:
		c.emitXorR64R64("rax", "rax")
	}
}

func (c *Codegen) emitIdent(e *IdentExpr) {
	// Check if it's a local variable
	if offset, ok := c.stackOffsets[e.Name]; ok {
		c.emitMovFromStack("rax", offset)
		return
	}
	// Check if it's a global constant (top-level let/const with int value)
	if v, ok := c.consts[e.Name]; ok {
		c.emitMovR64Imm64("rax", v)
		return
	}
	// Check if it's a function (load address)
	// For now, just load 0 as placeholder
	c.emitXorR64R64("rax", "rax")
}

func (c *Codegen) emitBinary(be *BinaryExpr) {
	// Handle string operations specially. A string operand is a literal,
	// an identifier typed string/[string] element, or an index into one.
	isStrOp := func(e Expr) bool {
		if c.isStringExpr(e) {
			return true
		}
		// Only `string` is a string type; `byte` and `[byte]` elements are
		// integer char codes and must stay integer compares.
		if t := c.inferType(e); t == "string" {
			return true
		}
		// labels[i] where labels is [string]
		if ie, ok := e.(*IndexExpr); ok {
			if obj, ok := ie.Object.(*IdentExpr); ok {
				if ot := c.inferType(obj); ot == "[string]" {
					return true
				}
			}
		}
		return false
	}
	// String concatenation: emit runtime call
	if be.Op == "+" && isStrOp(be.Left) {
		// String concatenation: emit runtime call
		c.emitExpr(be.Left)
		c.emitPush("rax")
		c.emitExpr(be.Right)
		c.emitPop("rdi")
		c.emitMovR64R64("rsi", "rax")
		c.emitCall("__ae_str_concat")
		return
	}

	if be.Op == "==" && isStrOp(be.Left) {
		// String comparison
		c.emitExpr(be.Left)
		c.emitPush("rax")
		c.emitExpr(be.Right)
		c.emitPop("rdi")
		c.emitMovR64R64("rsi", "rax")
		c.emitCall("__ae_str_eq")
		return
	}

	if be.Op == "!=" && isStrOp(be.Left) {
		// String comparison
		c.emitExpr(be.Left)
		c.emitPush("rax")
		c.emitExpr(be.Right)
		c.emitPop("rdi")
		c.emitMovR64R64("rsi", "rax")
		c.emitCall("__ae_str_eq")
		// Negate result
		c.emitTestR64R64("rax", "rax")
		c.emitSete("al")
		c.emitMovzxR64R8("rax", "al")
		return
	}

	c.emitExpr(be.Left)
	c.emitPush("rax")
	c.emitExpr(be.Right)
	c.emitPop("rbx")

	switch be.Op {
	case "+":
		c.emitAddR64R64("rax", "rbx")
	case "-":
		c.emitSubR64R64("rbx", "rax")
		c.emitMovR64R64("rax", "rbx")
	case "*":
		c.emitMulR64("rbx")
	case "/":
		// rax = right, rbx = left; div rbx does rax = rax/rbx = right/left.
		// Swap: mov rax->rcx, rbx->rax, rcx->rbx, then div rbx.
		c.emitMovR64R64("rcx", "rax")
		c.emitMovR64R64("rax", "rbx")
		c.emitMovR64R64("rbx", "rcx")
		c.emitDivR64("rbx")
	case "%":
		// Same swap as division, then rax = rdx (remainder).
		c.emitMovR64R64("rcx", "rax")
		c.emitMovR64R64("rax", "rbx")
		c.emitMovR64R64("rbx", "rcx")
		c.emitDivR64("rbx")
		c.emitMovR64R64("rax", "rdx")
	case "==":
		c.emitCmpR64R64("rbx", "rax")
		c.emitSete("al")
		c.emitMovzxR64R8("rax", "al")
	case "!=":
		c.emitCmpR64R64("rbx", "rax")
		c.emitSetne("al")
		c.emitMovzxR64R8("rax", "al")
	case "<":
		c.emitCmpR64R64("rbx", "rax")
		c.emitSetl("al")
		c.emitMovzxR64R8("rax", "al")
	case ">":
		c.emitCmpR64R64("rbx", "rax")
		c.emitSetg("al")
		c.emitMovzxR64R8("rax", "al")
	case "<=":
		c.emitCmpR64R64("rbx", "rax")
		c.emitSetle("al")
		c.emitMovzxR64R8("rax", "al")
	case ">=":
		c.emitCmpR64R64("rbx", "rax")
		c.emitSetge("al")
		c.emitMovzxR64R8("rax", "al")
	case "&&":
		// a && b: short-circuit. Use local labels (NOT the global
		// _logical_false/_logical_true, which end in retq and can only
		// be CALLED, not jumped to).
		falseL := fmt.Sprintf("%s_and_false_%d", c.funcName, c.nextLabel())
		doneL := fmt.Sprintf("%s_and_done_%d", c.funcName, c.nextLabel())
		c.emitTestR64R64("rax", "rax")
		c.emitJz(falseL)
		c.emitTestR64R64("rbx", "rbx")
		c.emitJz(falseL)
		c.emitMovR64Imm64("rax", 1)
		c.emitJmp(doneL)
		c.label(falseL)
		c.emitXorR64R64("rax", "rax")
		c.label(doneL)
	case "||":
		// a || b: short-circuit. Local labels only.
		trueL := fmt.Sprintf("%s_or_true_%d", c.funcName, c.nextLabel())
		doneL := fmt.Sprintf("%s_or_done_%d", c.funcName, c.nextLabel())
		c.emitTestR64R64("rax", "rax")
		c.emitJnz(trueL)
		c.emitTestR64R64("rbx", "rbx")
		c.emitJnz(trueL)
		c.emitXorR64R64("rax", "rax")
		c.emitJmp(doneL)
		c.label(trueL)
		c.emitMovR64Imm64("rax", 1)
		c.label(doneL)
	case "&":
		c.emitAndR64R64("rax", "rbx")
	case "|":
		c.emitOrR64R64("rax", "rbx")
	case "^":
		c.emitXorR64R64("rax", "rbx")
	case "<<":
		// Shift amount is in rax (right operand); rbx = left operand.
		// shl/shr use CL as the count, so move rax -> rcx first.
		c.emitMovR64R64("rcx", "rax")
		c.emitShlR64("rbx")
		c.emitMovR64R64("rax", "rbx")
	case ">>":
		// Same: shift amount in rax -> rcx, value in rbx.
		c.emitMovR64R64("rcx", "rax")
		c.emitShrR64("rbx")
		c.emitMovR64R64("rax", "rbx")
	}
}

func (c *Codegen) isStringExpr(expr Expr) bool {
	switch e := expr.(type) {
	case *LiteralExpr:
		_, ok := e.Value.(string)
		return ok
	case *IdentExpr:
		return false
	case *BinaryExpr:
		if e.Op == "+" {
			return c.isStringExpr(e.Left) || c.isStringExpr(e.Right)
		}
		return false
	}
	return false
}

func (c *Codegen) emitUnary(ue *UnaryExpr) {
	c.emitExpr(ue.Operand)
	switch ue.Op {
	case "-":
		c.emitNegR64("rax")
	case "!":
		c.emitTestR64R64("rax", "rax")
		c.emitSete("al")
		c.emitMovzxR64R8("rax", "al")
	case "~":
		c.emitNotR64("rax")
	case "#":
		// Length operator: call __ae_str_len or __ae_array_len
		c.emitMovR64R64("rdi", "rax")
		c.emitCall("__ae_len")
	case "++":
		c.emitAddR64Imm8("rax", 1)
	case "--":
		c.emitSubR64Imm8("rax", 1)
	case "copy":
		// copy forces pass-by-value — for now, just pass through
	case "heap":
		// heap allocate — for now, just pass through
	}
}

func (c *Codegen) emitCallExpr(ce *CallExpr) {
	// Push args in reverse order
	for i := len(ce.Args) - 1; i >= 0; i-- {
		c.emitExpr(ce.Args[i])
		c.emitPush("rax")
	}

	// Pop into registers (first 6 args in rdi, rsi, rdx, rcx, r8, r9)
	regs := []string{"rdi", "rsi", "rdx", "rcx", "r8", "r9"}
	for i := 0; i < len(ce.Args) && i < 6; i++ {
		c.emitPop(regs[i])
	}

	// Call
	if ident, ok := ce.Callee.(*IdentExpr); ok {
		c.emitCall(ident.Name)
	} else if _, ok := ce.Callee.(*MemberExpr); ok {
		c.emitExpr(ce.Callee)
	}
}

func (c *Codegen) emitMethodCallExpr(mce *MethodCallExpr) {
	// Push args in reverse order (including the object as first arg)
	for i := len(mce.Args) - 1; i >= 0; i-- {
		c.emitExpr(mce.Args[i])
		c.emitPush("rax")
	}

	// Push the object (self)
	c.emitExpr(mce.Object)
	c.emitPush("rax")

	// Pop into registers: rdi = self, rsi-r9 = args
	regs := []string{"rdi", "rsi", "rdx", "rcx", "r8", "r9"}
	totalArgs := 1 + len(mce.Args)
	for i := 0; i < totalArgs && i < 6; i++ {
		c.emitPop(regs[i])
	}

	// Call the method function
	c.emitCall(mce.Method)
}

// arrayElemType extracts the element type from an array type name like "[Token]" -> "Token".
// Returns "" if the type is not an array.
func arrayElemType(typeName string) string {
	if len(typeName) >= 2 && typeName[0] == '[' && typeName[len(typeName)-1] == ']' {
		return typeName[1 : len(typeName)-1]
	}
	return ""
}

// inferType returns the type name of an expression, or "" if unknown.
// Handles: IdentExpr (lookup varTypes), IndexExpr (array element type),
// CallExpr (function return type), MemberExpr (field type).
func (c *Codegen) inferType(e Expr) string {
	switch ex := e.(type) {
	case *LiteralExpr:
		if _, ok := ex.Value.(string); ok {
			return "string"
		}
	case *IdentExpr:
		return c.varTypes[ex.Name]
	case *IndexExpr:
		// arr[i] — the element type of arr. arr could be a variable or a member access.
		// A slice obj[start:end] yields a string (or a copy of the array type).
		if be, ok := ex.Index.(*BinaryExpr); ok && be.Op == ":" {
			objType := c.inferType(ex.Object)
			if objType == "string" {
				return "string"
			}
			return objType // array slice keeps element array type
		}
		objType := c.inferType(ex.Object)
		if elem := arrayElemType(objType); elem != "" {
			return elem
		}
		// If the object is a variable with a known array element type
		if ie, ok := ex.Object.(*IdentExpr); ok {
			return c.arrayElemTypes[ie.Name]
		}
	case *CallExpr:
		if ie, ok := ex.Callee.(*IdentExpr); ok {
			return c.funcReturnTypes[ie.Name]
		}
	case *MemberExpr:
		// obj.field — the field's type. Look up the struct.
		objType := c.inferType(ex.Object)
		if si, ok := c.structs[objType]; ok {
			for _, f := range si.Fields {
				if f.Name == ex.Member {
					return f.Type
				}
			}
		}
	}
	return ""
}

// findStructForField returns the name of the first struct that has a field
// with the given name, or "" if none. Used as a fallback when the object's
// type can't be inferred (e.g. a function call with an ambiguous return type).
func (c *Codegen) findStructForField(fieldName string) string {
	for _, si := range c.structs {
		for _, f := range si.Fields {
			if f.Name == fieldName {
				return si.Name
			}
		}
	}
	return ""
}

// emitMemberExpr handles struct field access: object.field
// The object is a struct value (pointer to struct data on stack or heap)
// For now, struct values are passed as pointers (8 bytes = address of struct data)
func (c *Codegen) emitMemberExpr(me *MemberExpr) {
	// Evaluate the object — this gives us a pointer to the struct
	c.emitExpr(me.Object)
	// rax now contains a pointer to the struct data
	// Emit a call to __ae_field_<Struct>_<name> with the struct pointer in rdi.
	// The label is struct-qualified when the object's type is known.
	label := "__ae_field_" + me.Member
	if st := c.inferType(me.Object); st != "" {
		if _, ok := c.structs[st]; ok {
			label = "__ae_field_" + st + "_" + me.Member
		} else if fs := c.findStructForField(me.Member); fs != "" {
			label = "__ae_field_" + fs + "_" + me.Member
		}
	} else if fs := c.findStructForField(me.Member); fs != "" {
		label = "__ae_field_" + fs + "_" + me.Member
	}
	c.emitPush("rax") // save struct pointer
	c.emitPop("rdi")  // rdi = struct pointer
	c.emitCall(label)
}

func (c *Codegen) emitIndexExpr(ie *IndexExpr) {
	// Handle slice: obj[start:end] — the index is a BinaryExpr with ":" op.
	if be, ok := ie.Index.(*BinaryExpr); ok && be.Op == ":" {
		c.emitExpr(ie.Object)
		c.emitPush("rax")
		c.emitExpr(be.Left)
		c.emitPush("rax")
		c.emitExpr(be.Right)
		c.emitMovR64R64("rdx", "rax") // end
		c.emitPop("rsi")              // start (top of stack)
		c.emitPop("rdi")              // object
		c.emitCall("__ae_slice")
		return
	}
	// Evaluate object (string or array pointer)
	c.emitExpr(ie.Object)
	c.emitPush("rax")

	// Evaluate index
	c.emitExpr(ie.Index)
	c.emitPop("rdi") // object pointer
	c.emitMovR64R64("rsi", "rax") // index

	// Call runtime helper
	c.emitCall("__ae_index")
}

func (c *Codegen) emitAssignmentExpr(ae *AssignmentExpr) {
	// Evaluate the value
	c.emitExpr(ae.Value)
	c.emitPush("rax")

	// Handle different target types
	switch target := ae.Target.(type) {
	case *IdentExpr:
		// Simple variable assignment
		c.emitPop("rax")
		if offset, ok := c.stackOffsets[target.Name]; ok {
			c.emitMovR64ToStack("rax", offset)
		}
	case *IndexExpr:
		// Array/string index assignment
		c.emitExpr(target.Object)
		c.emitPush("rax")
		c.emitExpr(target.Index)
		c.emitPop("rdi") // object
		c.emitMovR64R64("rsi", "rax") // index
		c.emitPop("rdx") // value
		c.emitCall("__ae_index_set")
	case *MemberExpr:
		// Struct field assignment
		c.emitExpr(target.Object)
		c.emitPush("rax")
		c.emitPop("rdi") // struct pointer
		c.emitPop("rsi") // value
		label := "__ae_field_set_" + target.Member
		if st := c.inferType(target.Object); st != "" {
			if _, ok := c.structs[st]; ok {
				label = "__ae_field_set_" + st + "_" + target.Member
			} else if fs := c.findStructForField(target.Member); fs != "" {
				label = "__ae_field_set_" + fs + "_" + target.Member
			}
		} else if fs := c.findStructForField(target.Member); fs != "" {
			label = "__ae_field_set_" + fs + "_" + target.Member
		}
		c.emitCall(label)
	}
}

func (c *Codegen) emitArrayLiteral(al *ArrayLiteralExpr) {
	// Allocate array on heap and populate
	for _, el := range al.Elements {
		c.emitExpr(el)
		c.emitPush("rax")
	}

	// Call __ae_array_new(count)
	c.emitMovR64Imm64("rdi", uint64(len(al.Elements)))
	c.emitCall("__ae_array_new")

	// Pop elements and store
	for i := len(al.Elements) - 1; i >= 0; i-- {
		c.emitPop("rdx") // value
		c.emitMovR64Imm64("rsi", uint64(i)) // index
		c.emitMovR64R64("rdi", "rax") // array
		c.emitPush("rax") // save array pointer
		c.emitCall("__ae_index_set")
		c.emitPop("rax") // restore array pointer
	}
}

func (c *Codegen) emitStringInterpolation(sie *StringInterpolationExpr) {
	// Simplified: concatenate all parts
	if len(sie.Parts) == 0 {
		c.emitXorR64R64("rax", "rax")
		return
	}

	c.emitExpr(sie.Parts[0])
	for i := 1; i < len(sie.Parts); i++ {
		c.emitPush("rax")
		c.emitExpr(sie.Parts[i])
		c.emitPop("rdi")
		c.emitMovR64R64("rsi", "rax")
		c.emitCall("__ae_str_concat")
	}
}

func (c *Codegen) emitMatchExpr(me *MatchExpr) {
	endLabel := fmt.Sprintf("%s_match_expr_end_%d", c.funcName, c.nextLabel())

	// Evaluate the match value
	c.emitExpr(me.Value)
	c.emitPush("rax") // save match value on stack

	for _, mc := range me.Cases {
		nextLabel := fmt.Sprintf("%s_match_expr_next_%d", c.funcName, c.nextLabel())

		// Pop match value
		c.emitPop("rbx")
		c.emitPush("rbx") // keep a copy

		// Evaluate pattern
		c.emitExpr(mc.Pattern)

		// Compare
		c.emitCmpR64R64("rax", "rbx")
		c.emitJnz(nextLabel)

		// Match!
		c.emitPop("rax") // discard saved value
		if b, ok := mc.Body.(*Block); ok {
			c.emitBlock(b)
		} else {
			c.emitStmt(mc.Body)
		}
		c.emitJmp(endLabel)

		c.label(nextLabel)
	}

	// No match — pop saved value, load 0
	c.emitPop("rax")
	c.emitXorR64R64("rax", "rax")

	c.label(endLabel)
}

func (c *Codegen) emitIfExpr(ie *IfExpr) {
	elseLabel := fmt.Sprintf("%s_ifexpr_else_%d", c.funcName, c.nextLabel())
	endLabel := fmt.Sprintf("%s_ifexpr_end_%d", c.funcName, c.nextLabel())

	c.emitExpr(ie.Cond)
	c.emitTestR64R64("rax", "rax")
	c.emitJz(elseLabel)

	if b, ok := ie.ThenExpr.(*Block); ok {
		c.emitBlock(b)
	} else {
		c.emitStmt(ie.ThenExpr)
	}
	c.emitJmp(endLabel)

	c.label(elseLabel)
	if ie.ElseExpr != nil {
		if b, ok := ie.ElseExpr.(*Block); ok {
			c.emitBlock(b)
		} else {
			c.emitStmt(ie.ElseExpr)
		}
	}

	c.label(endLabel)
}

// ============================================================
// String data management
// ============================================================

func (c *Codegen) addString(s string) string {
	label := fmt.Sprintf("str_%d", len(c.strings))
	c.strings[label] = s
	return label
}

func (c *Codegen) emitStringData() {
	for label, content := range c.strings {
		c.label(label)
		// Emit string content + null terminator into text section
		// (linker concatenates text+rodata+data, so all labels use text offsets)
		c.text = append(c.text, []byte(content)...)
		c.text = append(c.text, 0)
	}
}

// ============================================================
// Label management
// ============================================================

//go:noinline
func (c *Codegen) label(name string) {
	n := len(c.text)
	c.labels[name] = n
}

// labelData records a label at the current position in the data section,
// accounting for the fact that the linker concatenates text + rodata + data.
func (c *Codegen) labelData(name string) {
	c.labels[name] = len(c.text) + len(c.rodata) + len(c.data)
}

func (c *Codegen) nextLabel() int {
	c.labelCount++
	return c.labelCount
}

// ============================================================
// Stack frame helpers
// ============================================================

// mov rax, [rbp - offset]  (load from stack)
func (c *Codegen) emitMovFromStack(reg string, offset int) {
	if offset > 127 {
		// disp32 form: REX.W + 8B + modrm(10, reg, rbp) + disp32 (negative)
		c.emitRexWRB(regCodes[reg], 5)
		c.emitByte(0x8B)
		c.emitByte(c.modRM(2, regCodes[reg], 5)) // [rbp + disp32]
		c.emitU32(uint32(-offset))
		return
	}
	// REX.W + 8B + modrm(01, reg, rbp) + disp8
	c.emitRexWRB(regCodes[reg], 5) // reg field = reg, rm field = rbp
	c.emitByte(0x8B)
	c.emitByte(c.modRM(1, regCodes[reg], 5)) // [rbp + disp8]
	c.emitByte(byte(-offset)) // negative offset from rbp
}

// mov [rbp - offset], rax  (store to stack)
func (c *Codegen) emitMovR64ToStack(reg string, offset int) {
	if offset > 127 {
		// disp32 form: REX.W + 89 + modrm(10, reg, rbp) + disp32 (negative)
		c.emitRexWRB(regCodes[reg], 5)
		c.emitByte(0x89)
		c.emitByte(c.modRM(2, regCodes[reg], 5)) // [rbp + disp32]
		c.emitU32(uint32(-offset))
		return
	}
	// REX.W + 89 + modrm(01, reg, rbp) + disp8
	c.emitRexWRB(regCodes[reg], 5) // reg field = reg, rm field = rbp
	c.emitByte(0x89)
	c.emitByte(c.modRM(1, regCodes[reg], 5)) // [rbp + disp8]
	c.emitByte(byte(-offset))
}

// ============================================================
// x86_64 instruction encoding helpers
// ============================================================

func (c *Codegen) emitByte(b byte) {
	c.text = append(c.text, b)
}

func (c *Codegen) emitBytes(data []byte) {
	c.text = append(c.text, data...)
}

func (c *Codegen) emitU64(val uint64) {
	buf := make([]byte, 8)
	binary.LittleEndian.PutUint64(buf, val)
	c.text = append(c.text, buf...)
}

func (c *Codegen) emitU32(val uint32) {
	buf := make([]byte, 4)
	binary.LittleEndian.PutUint32(buf, val)
	c.text = append(c.text, buf...)
}

// REX prefixes
const (
	REX_W = 0x48 // 64-bit operand size
)

func (c *Codegen) emitRexW() {
	c.emitByte(REX_W)
}

// emitRexWRB emits a REX prefix with W=1 (64-bit) plus the R and B bits
// needed to address registers r8-r15 in the reg and rm fields of a ModRM byte.
// regField is the register code in the ModRM reg field (the source for
// r/m64,r64-form opcodes), rmField is the register code in the rm field
// (the destination). REX.R (0x04) selects regField>=8, REX.B (0x01) selects
// rmField>=8.
func (c *Codegen) emitRexWRB(regField, rmField byte) {
	rex := byte(REX_W)
	if regField >= 8 {
		rex |= 0x04 // REX.R
	}
	if rmField >= 8 {
		rex |= 0x01 // REX.B
	}
	c.emitByte(rex)
}

// Register encoding
var regCodes = map[string]byte{
	"rax": 0, "rcx": 1, "rdx": 2, "rbx": 3,
	"rsp": 4, "rbp": 5, "rsi": 6, "rdi": 7,
	"r8": 8, "r9": 9, "r10": 10, "r11": 11,
	"r12": 12, "r13": 13, "r14": 14, "r15": 15,
}

func (c *Codegen) modRM(mod byte, reg byte, rm byte) byte {
	return (mod << 6) | ((reg & 7) << 3) | (rm & 7)
}

// mov rax, imm64
func (c *Codegen) emitMovR64Imm64(reg string, val uint64) {
	c.emitRexWRB(0, regCodes[reg])
	c.emitByte(0xB8 + regCodes[reg]&7)
	c.emitU64(val)
}

// mov rax, rbx
func (c *Codegen) emitMovR64R64(dst, src string) {
	// 0x89: MOV r/m64, r64 — reg field = source, rm field = destination
	c.emitRexWRB(regCodes[src], regCodes[dst])
	c.emitByte(0x89)
	c.emitByte(c.modRM(3, regCodes[src], regCodes[dst]))
}

// lea rax, [label]
func (c *Codegen) emitLeaR64Label(reg, label string) {
	// REX.W + 8D + modrm + sib + disp32
	// reg field = target register (needs REX.R if >= 8), rm field = 5 (RIP)
	c.emitRexWRB(regCodes[reg], 5)
	c.emitByte(0x8D)
	// Use RIP-relative addressing: mod=00, rm=5 (RIP)
	c.emitByte(c.modRM(0, regCodes[reg], 5))
	// Record relocation for this RIP-relative address
	c.relocs = append(c.relocs, relocEntry{offset: len(c.text), target: label})
	// Placeholder displacement (will be patched by linker)
	c.emitU32(0)
}

// add rax, rbx
func (c *Codegen) emitAddR64R64(dst, src string) {
	// 0x01: ADD r/m64, r64 — reg field = source, rm field = destination
	c.emitRexWRB(regCodes[src], regCodes[dst])
	c.emitByte(0x01)
	c.emitByte(c.modRM(3, regCodes[src], regCodes[dst]))
}

// sub rax, imm8
func (c *Codegen) emitSubR64Imm8(reg string, imm byte) {
	c.emitRexWRB(5, regCodes[reg])
	c.emitByte(0x83)
	c.emitByte(c.modRM(3, 5, regCodes[reg]))
	c.emitByte(imm)
}

// add rax, imm8
func (c *Codegen) emitAddR64Imm8(reg string, imm byte) {
	c.emitRexWRB(0, regCodes[reg])
	c.emitByte(0x83)
	c.emitByte(c.modRM(3, 0, regCodes[reg]))
	c.emitByte(imm)
}

// sub rbx, rax (rbx = rbx - rax)
func (c *Codegen) emitSubR64R64(dst, src string) {
	// 0x29: SUB r/m64, r64 — reg field = source, rm field = destination
	c.emitRexWRB(regCodes[src], regCodes[dst])
	c.emitByte(0x29)
	c.emitByte(c.modRM(3, regCodes[src], regCodes[dst]))
}

// mul rbx (rax = rax * rbx)
func (c *Codegen) emitMulR64(reg string) {
	c.emitRexWRB(4, regCodes[reg])
	c.emitByte(0xF7)
	c.emitByte(c.modRM(3, 4, regCodes[reg]))
}

// div rbx (rax = rax / rbx, rdx = rax % rbx)
func (c *Codegen) emitDivR64(reg string) {
	// xor rdx, rdx
	c.emitXorR64R64("rdx", "rdx")
	c.emitRexWRB(6, regCodes[reg])
	c.emitByte(0xF7)
	c.emitByte(c.modRM(3, 6, regCodes[reg]))
}

// xor rax, rax
func (c *Codegen) emitXorR64R64(dst, src string) {
	// 0x31: XOR r/m64, r64 — reg field = source, rm field = destination
	c.emitRexWRB(regCodes[src], regCodes[dst])
	c.emitByte(0x31)
	c.emitByte(c.modRM(3, regCodes[src], regCodes[dst]))
}

// neg rax
func (c *Codegen) emitNegR64(reg string) {
	c.emitRexWRB(3, regCodes[reg])
	c.emitByte(0xF7)
	c.emitByte(c.modRM(3, 3, regCodes[reg]))
}

// not rax
func (c *Codegen) emitNotR64(reg string) {
	c.emitRexWRB(2, regCodes[reg])
	c.emitByte(0xF7)
	c.emitByte(c.modRM(3, 2, regCodes[reg]))
}

// test rax, rax
func (c *Codegen) emitTestR64R64(dst, src string) {
	// 0x85: TEST r/m64, r64 — reg field = source, rm field = destination
	c.emitRexWRB(regCodes[src], regCodes[dst])
	c.emitByte(0x85)
	c.emitByte(c.modRM(3, regCodes[src], regCodes[dst]))
}

// cmp rax, rbx
func (c *Codegen) emitCmpR64R64(dst, src string) {
	// 0x39: CMP r/m64, r64 — computes r/m64 - r64
	// reg field = r64 (source being subtracted), rm field = r/m64 (destination)
	// So modRM(3, regCodes[src], regCodes[dst]) gives "cmp dst, src" = dst - src (natural order)
	c.emitRexWRB(regCodes[src], regCodes[dst])
	c.emitByte(0x39)
	c.emitByte(c.modRM(3, regCodes[src], regCodes[dst]))
}

// and rax, rbx
func (c *Codegen) emitAndR64R64(dst, src string) {
	// 0x21: AND r/m64, r64 — reg field = source, rm field = destination
	c.emitRexWRB(regCodes[src], regCodes[dst])
	c.emitByte(0x21)
	c.emitByte(c.modRM(3, regCodes[src], regCodes[dst]))
}

// or rax, rbx
func (c *Codegen) emitOrR64R64(dst, src string) {
	// 0x09: OR r/m64, r64 — reg field = source, rm field = destination
	c.emitRexWRB(regCodes[src], regCodes[dst])
	c.emitByte(0x09)
	c.emitByte(c.modRM(3, regCodes[src], regCodes[dst]))
}

// shl rax, cl (shift left by cl)
func (c *Codegen) emitShlR64(reg string) {
	c.emitRexWRB(4, regCodes[reg])
	c.emitByte(0xD3)
	c.emitByte(c.modRM(3, 4, regCodes[reg]))
}

// shr rax, cl (shift right by cl)
func (c *Codegen) emitShrR64(reg string) {
	c.emitRexWRB(5, regCodes[reg])
	c.emitByte(0xD3)
	c.emitByte(c.modRM(3, 5, regCodes[reg]))
}

// sete al
func (c *Codegen) emitSete(reg string) {
	c.emitByte(0x0F)
	c.emitByte(0x94)
	c.emitByte(c.modRM(3, 0, regCodes[reg]))
}

// setne al
func (c *Codegen) emitSetne(reg string) {
	c.emitByte(0x0F)
	c.emitByte(0x95)
	c.emitByte(c.modRM(3, 0, regCodes[reg]))
}

// setl al
func (c *Codegen) emitSetl(reg string) {
	c.emitByte(0x0F)
	c.emitByte(0x9C)
	c.emitByte(c.modRM(3, 0, regCodes[reg]))
}

// setg al
func (c *Codegen) emitSetg(reg string) {
	c.emitByte(0x0F)
	c.emitByte(0x9F)
	c.emitByte(c.modRM(3, 0, regCodes[reg]))
}

// setle al
func (c *Codegen) emitSetle(reg string) {
	c.emitByte(0x0F)
	c.emitByte(0x9E)
	c.emitByte(c.modRM(3, 0, regCodes[reg]))
}

// setge al
func (c *Codegen) emitSetge(reg string) {
	c.emitByte(0x0F)
	c.emitByte(0x9D)
	c.emitByte(c.modRM(3, 0, regCodes[reg]))
}

// movzx rax, al
func (c *Codegen) emitMovzxR64R8(dst, src string) {
	c.emitRexW()
	c.emitByte(0x0F)
	c.emitByte(0xB6)
	c.emitByte(c.modRM(3, regCodes[dst], regCodes[src]))
}

// push rax
func (c *Codegen) emitPush(reg string) {
	if regCodes[reg] < 8 {
		c.emitByte(0x50 + regCodes[reg])
	} else {
		c.emitByte(0x41)
		c.emitByte(0x50 + regCodes[reg] - 8)
	}
}

// pop rax
func (c *Codegen) emitPop(reg string) {
	if regCodes[reg] < 8 {
		c.emitByte(0x58 + regCodes[reg])
	} else {
		c.emitByte(0x41)
		c.emitByte(0x58 + regCodes[reg] - 8)
	}
}

// call label
func (c *Codegen) emitCall(label string) {
	c.emitByte(0xE8)
	c.relocs = append(c.relocs, relocEntry{offset: len(c.text), target: label})
	c.emitU32(0) // placeholder, patched by linker
}

// jmp label
func (c *Codegen) emitJmp(label string) {
	c.emitByte(0xE9)
	c.relocs = append(c.relocs, relocEntry{offset: len(c.text), target: label})
	c.emitU32(0) // placeholder, patched by linker
}

// jz label (jump if zero)
func (c *Codegen) emitJz(label string) {
	c.emitByte(0x0F)
	c.emitByte(0x84)
	c.relocs = append(c.relocs, relocEntry{offset: len(c.text), target: label})
	c.emitU32(0) // placeholder, patched by linker
}

// jnz label (jump if not zero)
func (c *Codegen) emitJnz(label string) {
	c.emitByte(0x0F)
	c.emitByte(0x85)
	c.relocs = append(c.relocs, relocEntry{offset: len(c.text), target: label})
	c.emitU32(0) // placeholder, patched by linker
}

// jns label (jump if not sign / positive)
func (c *Codegen) emitJns(label string) {
	c.emitByte(0x0F)
	c.emitByte(0x89)
	c.relocs = append(c.relocs, relocEntry{offset: len(c.text), target: label})
	c.emitU32(0) // placeholder, patched by linker
}

// jb label (jump if below, unsigned <)
func (c *Codegen) emitJb(label string) {
	c.emitByte(0x0F)
	c.emitByte(0x82)
	c.relocs = append(c.relocs, relocEntry{offset: len(c.text), target: label})
	c.emitU32(0) // placeholder, patched by linker
}

// ret
func (c *Codegen) emitRet() {
	c.emitByte(0xC3)
}

// syscall
func (c *Codegen) emitSyscall() {
	c.emitByte(0x0F)
	c.emitByte(0x05)
}

// ============================================================
// Relocation patching
// ============================================================

// patchRelocs patches all call/jmp/lea displacements now that all labels are known.
func (c *Codegen) patchRelocs() {
	fmt.Fprintf(os.Stderr, "=== patchRelocs: %d relocs, %d labels ===\n", len(c.relocs), len(c.labels))
	for _, r := range c.relocs {
		targetOff, ok := c.labels[r.target]
		if !ok {
			c.errors = append(c.errors, fmt.Sprintf("undefined label %q referenced at offset %d", r.target, r.offset))
			fmt.Fprintf(os.Stderr, "  UNDEFINED: %q at offset %d\n", r.target, r.offset)
			continue
		}
		disp := int32(targetOff - (r.offset + 4))
		fmt.Fprintf(os.Stderr, "  %q: offset=%d targetOff=%d disp=%d\n", r.target, r.offset, targetOff, disp)
		binary.LittleEndian.PutUint32(c.text[r.offset:r.offset+4], uint32(disp))
	}
}

// ============================================================
// Memory store/load helpers
// ============================================================

// mov [addrReg], src  (store src at address in addrReg)
func (c *Codegen) emitMovR64ToAddr(src, addrReg string, offset int8) {
	// REX.W + 89 + modrm(00/01, src, addrReg) + [optional disp8]
	c.emitRexWRB(regCodes[src], regCodes[addrReg])
	c.emitByte(0x89)
	if offset == 0 {
		c.emitByte(c.modRM(0, regCodes[src], regCodes[addrReg]))
	} else {
		c.emitByte(c.modRM(1, regCodes[src], regCodes[addrReg]))
		c.emitByte(byte(offset))
	}
}

// mov dst, [addrReg]  (load dst from address in addrReg)
func (c *Codegen) emitMovFromAddr(dst, addrReg string, offset int8) {
	// REX.W + 8B + modrm(00/01, dst, addrReg) + [optional disp8]
	c.emitRexWRB(regCodes[dst], regCodes[addrReg])
	c.emitByte(0x8B)
	if offset == 0 {
		c.emitByte(c.modRM(0, regCodes[dst], regCodes[addrReg]))
	} else {
		c.emitByte(c.modRM(1, regCodes[dst], regCodes[addrReg]))
		c.emitByte(byte(offset))
	}
}

// ============================================================
// Runtime helper functions
// ============================================================

// emitRuntimeHelpers emits implementations of all __ae_* functions
// that the Aether compiler source files declare as @extern.
func (c *Codegen) emitRuntimeHelpers() {
	// Data section: saved argc/argv and read buffer
	c.emitRuntimeData()

	// The 6 @extern functions from main.ae
	c.emitAePrint()
	c.emitAeExit()
	c.emitAeWriteFile()
	c.emitAeReadFile()
	c.emitAeGetArg()
	c.emitAeArgc()

	// Process/syscall helpers for shelling out to as/ld (self-hosting)
	c.emitAeFork()
	c.emitAeExecve()
	c.emitAeWait4()
	c.emitAePipe()

	// Additional runtime helpers needed by the Aether compiler source
	c.emitAeLen()
	c.emitAeSlice()
	c.emitAeIndex()
	c.emitAeIndexSet()
	c.emitAeArrayNew()
	c.emitAeStrConcat()
	c.emitAeStrEq()
	c.emitAePush()
	c.emitAePop()
	c.emitPushBuiltin()
	c.emitAeAlloc()

	// Logical helper labels
	c.emitLogicalHelpers()

	// Struct field accessors — emit for all struct fields found in the program
	c.emitStructFieldHelpers()
}

func (c *Codegen) emitRuntimeData() {
	// Allocate space in the text section for runtime data.
	// The text section is mapped RX, but we write to it during _start
	// before main runs. The linker puts everything in __TEXT,__text.
	// These are read-only at runtime but that's fine — we only read them.
	c.label("__ae_saved_argc")
	c.text = append(c.text, make([]byte, 8)...)

	c.label("__ae_saved_argv")
	c.text = append(c.text, make([]byte, 8)...)

	c.label("__ae_read_buf")
	c.text = append(c.text, make([]byte, 4096)...)

	c.label("__ae_empty_str")
	c.text = append(c.text, 0)

	c.label("__ae_heap_start")
	c.text = append(c.text, make([]byte, 65536)...)

	// Heap bump pointer: current allocation position within __ae_heap_start.
	// Initialized to 0 in _start (see emitStart). All heap allocations
	// (arrays, grown buffers) advance this pointer.
	c.label("__ae_heap_ptr")
	c.text = append(c.text, make([]byte, 8)...)
}

// __ae_print: print null-terminated string to stdout
// rdi = pointer to string
func (c *Codegen) emitAePrint() {
	c.label("__ae_print")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rdi")
	c.emitPush("rsi")
	c.emitPush("rdx")
	c.emitPush("rcx")
	c.emitPush("r8")
	c.emitPush("r9")

	// rsi = rdi (buf = string)
	c.emitMovR64R64("rsi", "rdi")
	// rdx = 0 (length counter)
	c.emitXorR64R64("rdx", "rdx")

	// strlen loop
	c.label("__ae_print_strlen")
	// cmp byte [rsi + rdx], 0
	c.emitByte(0x80)
	c.emitByte(0x3C)
	c.emitByte(0x16) // SIB: scale=0, index=rdx(010), base=rsi(110)
	c.emitByte(0x00) // immediate = 0
	c.emitJz("__ae_print_done")
	c.emitAddR64Imm8("rdx", 1)
	c.emitJmp("__ae_print_strlen")

	c.label("__ae_print_done")
	c.emitMovR64Imm64("rax", 0x2000004) // write syscall
	c.emitMovR64Imm64("rdi", 1)         // stdout
	// rsi = buf, rdx = len — already set
	c.emitSyscall()

	c.emitPop("r9")
	c.emitPop("r8")
	c.emitPop("rcx")
	c.emitPop("rdx")
	c.emitPop("rsi")
	c.emitPop("rdi")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_exit: exit process
// rdi = exit code
func (c *Codegen) emitAeExit() {
	c.label("__ae_exit")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitMovR64Imm64("rax", 0x2000001) // exit syscall
	// rdi already has exit code
	c.emitSyscall()
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_fork: fork() syscall (macOS x86_64: 0x2000002)
// No args. Returns child PID in parent, 0 in child, -1 on error.
func (c *Codegen) emitAeFork() {
	c.label("__ae_fork")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitMovR64Imm64("rax", 0x2000002) // fork syscall
	c.emitSyscall()
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_execve: execve(path, argv, envp) syscall (macOS x86_64: 0x200003B)
// rdi = path pointer, rsi = argv array pointer, rdx = envp pointer.
// Never returns on success; returns -1 on error.
func (c *Codegen) emitAeExecve() {
	c.label("__ae_execve")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitMovR64Imm64("rax", 0x200003B) // execve syscall
	c.emitSyscall()
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_wait4: wait4(pid, status, options, rusage) syscall (macOS x86_64: 0x2000007)
// rdi = pid, rsi = status pointer, rdx = options, rcx = rusage pointer.
// Returns child PID.
func (c *Codegen) emitAeWait4() {
	c.label("__ae_wait4")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitMovR64Imm64("rax", 0x2000007) // wait4 syscall
	c.emitSyscall()
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_pipe: pipe(fds) syscall (macOS x86_64: 0x200002A)
// rdi = pointer to int[2]. Returns 0 on success, -1 on error.
func (c *Codegen) emitAePipe() {
	c.label("__ae_pipe")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitMovR64Imm64("rax", 0x200002A) // pipe syscall
	c.emitSyscall()
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_write_file: write data to a file
// rdi = path (null-terminated), rsi = data pointer, rdx = data length
// __ae_read_file: read a file into a string
// rdi = path (null-terminated)
// Returns pointer to null-terminated string (or pointer to empty string on error)
func (c *Codegen) emitAeReadFile() {
	c.label("__ae_read_file")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rbx")
	c.emitPush("r12")

	c.emitMovR64R64("rbx", "rdi") // path

	// open(path, O_RDONLY, 0)
	c.emitMovR64R64("rdi", "rbx")     // path
	c.emitXorR64R64("rsi", "rsi")     // flags = O_RDONLY
	c.emitXorR64R64("rdx", "rdx")     // mode = 0
	c.emitMovR64Imm64("rax", 0x2000005) // open syscall
	c.emitSyscall()

	c.emitTestR64R64("rax", "rax")
	c.emitJns("__ae_read_open_ok")

	// Open failed — return empty string
	c.emitLeaR64Label("rax", "__ae_empty_str")
	c.emitPop("r12")
	c.emitPop("rbx")
	c.emitPop("rbp")
	c.emitRet()

	c.label("__ae_read_open_ok")
	c.emitMovR64R64("r12", "rax") // fd

	// read(fd, buf, 4096)
	c.emitMovR64R64("rdi", "r12") // fd
	c.emitLeaR64Label("rsi", "__ae_read_buf") // buf
	c.emitMovR64Imm64("rdx", 4096) // len
	c.emitMovR64Imm64("rax", 0x2000003) // read syscall
	c.emitSyscall()

	c.emitTestR64R64("rax", "rax")
	c.emitJns("__ae_read_ok")

	// Read failed — close and return empty string
	c.emitMovR64R64("rdi", "r12") // fd
	c.emitMovR64Imm64("rax", 0x2000006) // close syscall
	c.emitSyscall()
	c.emitLeaR64Label("rax", "__ae_empty_str")
	c.emitPop("r12")
	c.emitPop("rbx")
	c.emitPop("rbp")
	c.emitRet()

	c.label("__ae_read_ok")
	// Null-terminate the buffer at position rax
	c.emitLeaR64Label("rcx", "__ae_read_buf")
	c.emitAddR64R64("rcx", "rax")
	c.emitByte(0xC6) // mov byte [rcx], 0
	c.emitByte(0x01) // modrm: mod=00, reg=0, rm=001(rcx)
	c.emitByte(0x00) // immediate = 0

	// close(fd)
	c.emitMovR64R64("rdi", "r12") // fd
	c.emitMovR64Imm64("rax", 0x2000006) // close syscall
	c.emitSyscall()

	c.emitLeaR64Label("rax", "__ae_read_buf")
	c.emitPop("r12")
	c.emitPop("rbx")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_get_arg: get i-th command-line argument
// rdi = index (0 = program name)
// Returns pointer to null-terminated string (or empty string if index >= argc)
func (c *Codegen) emitAeGetArg() {
	c.label("__ae_get_arg")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rbx")

	c.emitMovR64R64("rbx", "rdi") // index

	// Load argc
	c.emitLeaR64Label("rcx", "__ae_saved_argc")
	c.emitMovFromAddr("rdx", "rcx", 0) // rdx = argc

	// Check if index >= argc
	c.emitCmpR64R64("rbx", "rdx")
	c.emitJb("__ae_get_arg_ok")

	// Index out of range — return empty string
	c.emitLeaR64Label("rax", "__ae_empty_str")
	c.emitPop("rbx")
	c.emitPop("rbp")
	c.emitRet()

	c.label("__ae_get_arg_ok")
	// Load argv
	c.emitLeaR64Label("rcx", "__ae_saved_argv")
	c.emitMovFromAddr("rcx", "rcx", 0) // rcx = argv

	// argv[i] = *(argv + i * 8)
	c.emitMovR64R64("rax", "rbx") // rax = i
	c.emitShlR64Imm8("rax", 3)    // rax = i * 8
	c.emitAddR64R64("rax", "rcx") // rax = argv + i*8
	c.emitMovFromAddr("rax", "rax", 0) // rax = argv[i]

	c.emitPop("rbx")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_argc: return number of command-line arguments
func (c *Codegen) emitAeArgc() {
	c.label("__ae_argc")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")

	c.emitLeaR64Label("rcx", "__ae_saved_argc")
	c.emitMovFromAddr("rax", "rcx", 0)

	c.emitPop("rbp")
	c.emitRet()
}

// Array object header (32 bytes), pointed to by every heap array:
//   [0]  tag:  u64 = AE_ARRAY_TAG  (distinguishes arrays from raw strings)
//   [8]  len:  u64                (number of 8-byte elements)
//   [16] cap:  u64                (allocated element capacity)
//   [24] data: u64                (pointer to element buffer)
// A raw string is a null-terminated byte buffer with NO header. The tag
// discriminator lets __ae_len / __ae_index / __ae_index_set handle both.

// AE_ARRAY_TAG is a distinctive 64-bit magic. It is not valid ASCII, so no
// string literal's first 8 bytes will ever collide with it. Fits in int64
// (top byte 0x7E < 0x80) so the Aether source can represent it as a literal.
const AE_ARRAY_TAG uint64 = 0x7E41AE41AE41AE41

// __ae_len: return length of a string OR array.
// rdi = pointer (string or array header)
// Returns length in rax.
func (c *Codegen) emitAeLen() {
	c.label("__ae_len")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rdi")
	c.emitPush("rsi")

	// Check tag: if [rdi] == AE_ARRAY_TAG, it's an array -> return [rdi+8]
	c.emitMovFromAddr("rax", "rdi", 0) // rax = [rdi]
	c.emitMovR64Imm64("rcx", AE_ARRAY_TAG)
	c.emitCmpR64R64("rax", "rcx")
	c.emitJnz("__ae_len_string")
	// Array: return [rdi+8] (len)
	c.emitMovFromAddr("rax", "rdi", 8)
	c.emitJmp("__ae_len_done")

	// String: scan for null terminator
	c.label("__ae_len_string")
	c.emitMovR64R64("rsi", "rdi") // buf = string
	c.emitXorR64R64("rax", "rax") // len = 0
	c.label("__ae_len_loop")
	// cmp byte [rsi + rax], 0
	c.emitByte(0x80)
	c.emitByte(0x3C)
	c.emitByte(0x06) // SIB: scale=0, index=rax(000), base=rsi(110)
	c.emitByte(0x00) // immediate = 0
	c.emitJz("__ae_len_done")
	c.emitAddR64Imm8("rax", 1)
	c.emitJmp("__ae_len_loop")

	c.label("__ae_len_done")
	c.emitPop("rsi")
	c.emitPop("rdi")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_slice: slice a string OR array.
// rdi = pointer (string or array header), rsi = start, rdx = end
// Returns a new heap copy of the slice in rax:
//   - strings: null-terminated byte copy
//   - arrays: new array header + copied elements
func (c *Codegen) emitAeSlice() {
	c.label("__ae_slice")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rdi")
	c.emitPush("rsi")
	c.emitPush("rdx")
	c.emitPush("rbx")
	c.emitPush("r12")
	c.emitPush("r13")
	c.emitPush("r14")
	c.emitPush("r15")

	// rbx = object, r12 = start, r13 = end, r14 = len
	c.emitMovR64R64("rbx", "rdi")
	c.emitMovR64R64("r12", "rsi")
	c.emitMovR64R64("r13", "rdx")
	c.emitMovR64R64("r14", "r13")
	c.emitSubR64R64("r14", "r12") // r14 = end - start

	// Check tag: if [rbx] == AE_ARRAY_TAG, it's an array
	c.emitMovFromAddr("rax", "rbx", 0)
	c.emitMovR64Imm64("rcx", AE_ARRAY_TAG)
	c.emitCmpR64R64("rax", "rcx")
	c.emitJnz("__ae_slice_string")

	// ---- Array path ----
	// new header in r15 (alloc 32 bytes)
	c.emitMovR64Imm64("rdi", 32)
	c.emitCall("__ae_alloc")
	c.emitMovR64R64("r15", "rax")
	// header: tag, len, cap = r14
	c.emitMovR64Imm64("rcx", AE_ARRAY_TAG)
	c.emitMovR64ToAddr("rcx", "r15", 0)
	c.emitMovR64ToAddr("r14", "r15", 8)
	c.emitMovR64ToAddr("r14", "r15", 16)
	// alloc data: len*8 -> rax, store at [r15+24]
	c.emitMovR64R64("rdi", "r14")
	c.emitShlR64Imm8("rdi", 3)
	c.emitCall("__ae_alloc")
	c.emitMovR64ToAddr("rax", "r15", 24)
	// rcx = src data ptr = [rbx+24]
	c.emitMovFromAddr("rcx", "rbx", 24)
	// copy loop: for i in 0..len: dst[i] = src[start+i]
	c.emitXorR64R64("r8", "r8")
	c.label("__ae_slice_arr_loop")
	c.emitCmpR64R64("r8", "r14")
	c.emitJz("__ae_slice_arr_done")
	// val = [src + (start+i)*8]
	c.emitMovR64R64("r9", "r12")
	c.emitAddR64R64("r9", "r8")
	c.emitShlR64Imm8("r9", 3)
	c.emitAddR64R64("r9", "rcx")
	c.emitMovFromAddr("r9", "r9", 0)
	// [dst + i*8] = val  (dst = [r15+24], held in rax)
	c.emitMovR64R64("r10", "r8")
	c.emitShlR64Imm8("r10", 3)
	c.emitAddR64R64("r10", "rax")
	c.emitMovR64ToAddr("r9", "r10", 0)
	c.emitAddR64Imm8("r8", 1)
	c.emitJmp("__ae_slice_arr_loop")
	c.label("__ae_slice_arr_done")
	// rax = new header
	c.emitMovR64R64("rax", "r15")
	c.emitJmp("__ae_slice_done")

	// ---- String path ----
	c.label("__ae_slice_string")
	// alloc len+1
	c.emitMovR64R64("rdi", "r14")
	c.emitAddR64Imm8("rdi", 1)
	c.emitCall("__ae_alloc")
	// copy loop: for i in 0..len: dst[i] = src[start+i]
	c.emitXorR64R64("r8", "r8")
	c.label("__ae_slice_str_loop")
	c.emitCmpR64R64("r8", "r14")
	c.emitJz("__ae_slice_str_done")
	// cl = byte [rbx + start + i]
	c.emitMovR64R64("r9", "r12")
	c.emitAddR64R64("r9", "r8")
	c.emitAddR64R64("r9", "rbx")
	c.emitByte(0x41) // REX.B (r9)
	c.emitByte(0x8A)
	c.emitByte(0x09) // mov cl, byte [r9]
	// byte [rax + i] = cl
	c.emitMovR64R64("r10", "rax")
	c.emitAddR64R64("r10", "r8")
	c.emitByte(0x41) // REX.B (r10)
	c.emitByte(0x88)
	c.emitByte(0x0A) // mov byte [r10], cl
	c.emitAddR64Imm8("r8", 1)
	c.emitJmp("__ae_slice_str_loop")
	c.label("__ae_slice_str_done")
	// null-terminate: byte [rax + len] = 0
	c.emitMovR64R64("rcx", "rax")
	c.emitAddR64R64("rcx", "r14")
	c.emitXorR64R64("rdx", "rdx")
	c.emitByte(0x88)
	c.emitByte(0x11) // mov byte [rcx], dl

	c.label("__ae_slice_done")
	c.emitPop("r15")
	c.emitPop("r14")
	c.emitPop("r13")
	c.emitPop("r12")
	c.emitPop("rbx")
	c.emitPop("rdx")
	c.emitPop("rsi")
	c.emitPop("rdi")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_index: get element at index in a string OR array.
// rdi = pointer, rsi = index
// Returns value in rax (byte zero-extended for strings, 8-byte for arrays).
func (c *Codegen) emitAeIndex() {
	c.label("__ae_index")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rdi")
	c.emitPush("rsi")
	c.emitPush("rbx")

	// Check tag: if [rdi] == AE_ARRAY_TAG, it's an array
	c.emitMovFromAddr("rax", "rdi", 0)
	c.emitMovR64Imm64("rcx", AE_ARRAY_TAG)
	c.emitCmpR64R64("rax", "rcx")
	c.emitJnz("__ae_index_string")

	// Array: rax = [[rdi+24] + rsi*8]
	c.emitMovFromAddr("rbx", "rdi", 24) // rbx = data ptr
	c.emitMovR64R64("rax", "rsi")       // rax = index
	c.emitShlR64Imm8("rax", 3)          // rax = index * 8
	c.emitAddR64R64("rax", "rbx")       // rax = data + index*8
	c.emitMovFromAddr("rax", "rax", 0)  // rax = [data + index*8]
	c.emitJmp("__ae_index_done")

	// String: movzx rax, byte [rdi + rsi]
	c.label("__ae_index_string")
	c.emitAddR64R64("rdi", "rsi") // rdi = string + index
	c.emitRexW()
	c.emitByte(0x0F)
	c.emitByte(0xB6)
	c.emitByte(0x07) // modrm: mod=00, reg=000(rax), rm=111(rdi)

	c.label("__ae_index_done")
	c.emitPop("rbx")
	c.emitPop("rsi")
	c.emitPop("rdi")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_index_set: set element at index in a string OR array.
// rdi = pointer, rsi = index, rdx = value
func (c *Codegen) emitAeIndexSet() {
	c.label("__ae_index_set")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rdi")
	c.emitPush("rsi")
	c.emitPush("rbx")

	// Check tag: if [rdi] == AE_ARRAY_TAG, it's an array
	c.emitMovFromAddr("rax", "rdi", 0)
	c.emitMovR64Imm64("rcx", AE_ARRAY_TAG)
	c.emitCmpR64R64("rax", "rcx")
	c.emitJnz("__ae_index_set_string")

	// Array: [ [rdi+24] + rsi*8 ] = rdx  (8-byte store)
	c.emitMovFromAddr("rbx", "rdi", 24) // rbx = data ptr
	c.emitMovR64R64("rax", "rsi")       // rax = index
	c.emitShlR64Imm8("rax", 3)          // rax = index * 8
	c.emitAddR64R64("rax", "rbx")       // rax = data + index*8
	c.emitMovR64ToAddr("rdx", "rax", 0) // [rax] = rdx
	c.emitJmp("__ae_index_set_done")

	// String: [rdi + rsi] = dl  (byte store)
	c.label("__ae_index_set_string")
	c.emitAddR64R64("rdi", "rsi") // rdi = string + index
	c.emitByte(0x88)             // mov r/m8, r8
	c.emitByte(0x17)             // modrm: mod=00, reg=010(rdx), rm=111(rdi)

	c.label("__ae_index_set_done")
	c.emitPop("rbx")
	c.emitPop("rsi")
	c.emitPop("rdi")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_array_new: allocate a new array with n elements.
// rdi = number of elements
// Returns pointer to array header in rax.
// Allocates 32 (header) + n*8 (data) bytes from the bump heap.
func (c *Codegen) emitAeArrayNew() {
	c.label("__ae_array_new")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rdi")
	c.emitPush("rsi")
	c.emitPush("rbx")
	c.emitPush("r12")

	// rbx = current heap ptr
	c.emitLeaR64Label("rcx", "__ae_heap_ptr")
	c.emitMovFromAddr("rbx", "rcx", 0) // rbx = [__ae_heap_ptr]

	// rax = header = rbx (return value)
	c.emitMovR64R64("rax", "rbx")

	// Set tag at [rbx+0]
	c.emitMovR64Imm64("rcx", AE_ARRAY_TAG)
	c.emitMovR64ToAddr("rcx", "rbx", 0)

	// Set len at [rbx+8] = n (rdi)
	c.emitMovR64ToAddr("rdi", "rbx", 8)

	// Set cap at [rbx+16] = n (rdi)
	c.emitMovR64ToAddr("rdi", "rbx", 16)

	// data ptr at [rbx+24] = rbx + 32
	c.emitMovR64R64("r12", "rbx")
	c.emitAddR64Imm8("r12", 32)
	c.emitMovR64ToAddr("r12", "rbx", 24)

	// Advance heap ptr: [__ae_heap_ptr] = rbx + 32 + n*8
	c.emitMovR64R64("r12", "rdi") // r12 = n
	c.emitShlR64Imm8("r12", 3)    // r12 = n*8
	c.emitAddR64Imm8("r12", 32)   // r12 = 32 + n*8
	c.emitAddR64R64("r12", "rbx") // r12 = rbx + 32 + n*8
	c.emitLeaR64Label("rcx", "__ae_heap_ptr")
	c.emitMovR64ToAddr("r12", "rcx", 0) // [__ae_heap_ptr] = r12

	c.emitPop("r12")
	c.emitPop("rbx")
	c.emitPop("rsi")
	c.emitPop("rdi")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_alloc: allocate size bytes from the bump heap.
// rdi = size in bytes
// Returns pointer to allocated block in rax.
func (c *Codegen) emitAeAlloc() {
	c.label("__ae_alloc")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rdi")
	c.emitPush("rbx")

	// rbx = current heap ptr
	c.emitLeaR64Label("rcx", "__ae_heap_ptr")
	c.emitMovFromAddr("rbx", "rcx", 0) // rbx = [__ae_heap_ptr]
	// rax = rbx (return value)
	c.emitMovR64R64("rax", "rbx")
	// Advance heap ptr by size (rdi)
	c.emitAddR64R64("rbx", "rdi") // rbx = heap_ptr + size
	c.emitLeaR64Label("rcx", "__ae_heap_ptr")
	c.emitMovR64ToAddr("rbx", "rcx", 0) // [__ae_heap_ptr] = rbx

	c.emitPop("rbx")
	c.emitPop("rdi")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_str_concat: concatenate two strings
// rdi = first string, rsi = second string
// Returns pointer to concatenated string in rax
func (c *Codegen) emitAeStrConcat() {
	c.label("__ae_str_concat")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rdi")
	c.emitPush("rsi")
	c.emitPush("rbx")
	c.emitPush("r12")
	c.emitPush("r13")

	// Save strings
	c.emitMovR64R64("rbx", "rdi") // first
	c.emitMovR64R64("r12", "rsi") // second

	// Compute length of first string
	c.emitMovR64R64("rdi", "rbx")
	c.emitCall("__ae_len")
	c.emitMovR64R64("r13", "rax") // len1

	// Compute length of second string
	c.emitMovR64R64("rdi", "r12")
	c.emitCall("__ae_len")
	c.emitMovR64R64("rsi", "rax") // len2

	// Total length = len1 + len2
	c.emitAddR64R64("rsi", "r13")

	// For now, just return the first string (simplified)
	// In a real implementation, we'd allocate and copy
	c.emitMovR64R64("rax", "rbx")

	c.emitPop("r13")
	c.emitPop("r12")
	c.emitPop("rbx")
	c.emitPop("rsi")
	c.emitPop("rdi")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_str_eq: compare two strings for equality
// rdi = first string, rsi = second string
// Returns 1 if equal, 0 if not
func (c *Codegen) emitAeStrEq() {
	c.label("__ae_str_eq")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rdi")
	c.emitPush("rsi")
	c.emitPush("rbx")
	c.emitPush("r12")

	c.emitMovR64R64("rbx", "rdi") // first
	c.emitMovR64R64("r12", "rsi") // second

	// Compare byte by byte.
	c.label("__ae_str_eq_loop")
	// mov al, byte [rbx]
	c.emitXorR64R64("rax", "rax")
	c.emitByte(0x8A) // mov al, byte [rbx]
	c.emitByte(0x03) // modrm: mod=00, reg=000(rax), rm=011(rbx)
	// mov cl, byte [r12]
	c.emitXorR64R64("rcx", "rcx")
	c.emitByte(0x41) // REX.B
	c.emitByte(0x8A) // mov cl, byte [r12]
	c.emitByte(0x0C) // modrm: mod=00, reg=001(rcx), rm=100(r12)
	c.emitByte(0x24) // SIB: scale=0, index=none, base=r12
	// cmp al, cl
	c.emitByte(0x38)
	c.emitByte(0xC8)
	c.emitJnz("__ae_str_eq_done")
	// test al, al (both bytes equal; 0 means end of both strings)
	c.emitTestR64R64("rax", "rax")
	c.emitJz("__ae_str_eq_done")
	// advance both pointers
	c.emitAddR64Imm8("rbx", 1)
	c.emitAddR64Imm8("r12", 1)
	c.emitJmp("__ae_str_eq_loop")

	c.label("__ae_str_eq_done")
	c.emitSete("al")
	c.emitMovzxR64R8("rax", "al")

	c.emitPop("r12")
	c.emitPop("rbx")
	c.emitPop("rsi")
	c.emitPop("rdi")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_push: push a value onto an array (append).
// rdi = array pointer, rsi = value
// Returns updated array pointer in rax.
// Grows the array's data buffer (doubling capacity) when full.
func (c *Codegen) emitAePush() {
	c.label("__ae_push")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rdi")
	c.emitPush("rsi")
	c.emitPush("rbx")
	c.emitPush("r12")
	c.emitPush("r13")

	// rbx = array header pointer
	c.emitMovR64R64("rbx", "rdi")
	// r12 = value
	c.emitMovR64R64("r12", "rsi")

	// rax = len = [rbx+8]
	c.emitMovFromAddr("rax", "rbx", 8)
	// rcx = cap = [rbx+16]
	c.emitMovFromAddr("rcx", "rbx", 16)

	// if len < cap: fast path (no realloc)
	c.emitCmpR64R64("rax", "rcx")
	c.emitJb("__ae_push_fast")

	// ---- Grow path ----
	// newCap = cap * 2 (or cap+1 if cap==0)
	c.emitMovR64R64("r13", "rcx") // r13 = cap
	c.emitTestR64R64("r13", "r13")
	c.emitJnz("__ae_push_grow_double")
	c.emitMovR64Imm64("r13", 1) // if cap==0, newCap=1
	c.emitJmp("__ae_push_grow_alloc")
	c.label("__ae_push_grow_double")
	c.emitShlR64Imm8("r13", 1) // newCap = cap*2

	// Allocate new data buffer: newCap*8 bytes from heap
	c.label("__ae_push_grow_alloc")
	// rcx = heap ptr
	c.emitLeaR64Label("rcx", "__ae_heap_ptr")
	c.emitMovFromAddr("rcx", "rcx", 0) // rcx = [__ae_heap_ptr]
	// newData = rcx (heap ptr)
	// Advance heap ptr by newCap*8
	c.emitMovR64R64("rdx", "r13") // rdx = newCap
	c.emitShlR64Imm8("rdx", 3)    // rdx = newCap*8
	c.emitAddR64R64("rcx", "rdx") // rcx = heap_ptr + newCap*8
	c.emitLeaR64Label("rdx", "__ae_heap_ptr")
	c.emitMovR64ToAddr("rcx", "rdx", 0) // [__ae_heap_ptr] = rcx

	// Copy old data (len elements) to new buffer
	// rcx = old data ptr = [rbx+24]
	c.emitMovFromAddr("rcx", "rbx", 24)
	// rdx = new data ptr (saved heap ptr before advance)
	// We need the original heap ptr; recompute: newData = [__ae_heap_ptr] - newCap*8
	c.emitLeaR64Label("rdx", "__ae_heap_ptr")
	c.emitMovFromAddr("rdx", "rdx", 0) // rdx = [__ae_heap_ptr]
	c.emitMovR64R64("r8", "r13")       // r8 = newCap
	c.emitShlR64Imm8("r8", 3)          // r8 = newCap*8
	c.emitSubR64R64("rdx", "r8")       // rdx = newData

	// Copy loop: for i in 0..len: [rdx + i*8] = [rcx + i*8]
	c.emitXorR64R64("r8", "r8") // i = 0
	c.label("__ae_push_copy_loop")
	c.emitCmpR64R64("r8", "rax") // i < len?
	c.emitJz("__ae_push_copy_done")
	c.emitMovFromAddr("r9", "rcx", 0) // r9 = [old + i*8]  (offset handled below)
	// [rdx + i*8] = r9
	c.emitMovR64R64("r10", "r8") // r10 = i
	c.emitShlR64Imm8("r10", 3)   // r10 = i*8
	c.emitAddR64R64("r10", "rdx") // r10 = newData + i*8
	c.emitMovR64ToAddr("r9", "r10", 0)
	// advance old ptr by 8
	c.emitAddR64Imm8("rcx", 8)
	c.emitAddR64Imm8("r8", 1) // i++
	c.emitJmp("__ae_push_copy_loop")
	c.label("__ae_push_copy_done")

	// Update header: data = newData, cap = newCap
	c.emitLeaR64Label("r8", "__ae_heap_ptr")
	c.emitMovFromAddr("r8", "r8", 0) // r8 = [__ae_heap_ptr]
	c.emitMovR64R64("r9", "r13")
	c.emitShlR64Imm8("r9", 3)        // r9 = newCap*8
	c.emitSubR64R64("r8", "r9")      // r8 = newData
	c.emitMovR64ToAddr("r8", "rbx", 24) // [rbx+24] = newData
	c.emitMovR64ToAddr("r13", "rbx", 16) // [rbx+16] = newCap

	// ---- Fast path ----
	c.label("__ae_push_fast")
	// rax = len (reload, in case grow path changed it)
	c.emitMovFromAddr("rax", "rbx", 8)
	// rcx = data ptr = [rbx+24]
	c.emitMovFromAddr("rcx", "rbx", 24)
	// [data + len*8] = value
	c.emitMovR64R64("rdx", "rax") // rdx = len
	c.emitShlR64Imm8("rdx", 3)    // rdx = len*8
	c.emitAddR64R64("rdx", "rcx") // rdx = data + len*8
	c.emitMovR64ToAddr("r12", "rdx", 0) // [data + len*8] = value
	// len++
	c.emitAddR64Imm8("rax", 1)
	c.emitMovR64ToAddr("rax", "rbx", 8) // [rbx+8] = len+1

	// Return array pointer
	c.emitMovR64R64("rax", "rbx")

	c.emitPop("r13")
	c.emitPop("r12")
	c.emitPop("rbx")
	c.emitPop("rsi")
	c.emitPop("rdi")
	c.emitPop("rbp")
	c.emitRet()
}

// __ae_pop: pop a value from an array.
// rdi = array pointer
// Returns popped value in rax.
func (c *Codegen) emitAePop() {
	c.label("__ae_pop")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitPush("rdi")
	c.emitPush("rbx")

	// rbx = array header
	c.emitMovR64R64("rbx", "rdi")
	// rax = len = [rbx+8]
	c.emitMovFromAddr("rax", "rbx", 8)
	// if len == 0, return 0
	c.emitTestR64R64("rax", "rax")
	c.emitJz("__ae_pop_done")
	// len--
	c.emitSubR64Imm8("rax", 1)
	c.emitMovR64ToAddr("rax", "rbx", 8) // [rbx+8] = len-1
	// rcx = data = [rbx+24]
	c.emitMovFromAddr("rcx", "rbx", 24)
	// rax = [data + (len-1)*8]
	c.emitMovR64R64("rdx", "rax") // rdx = len-1
	c.emitShlR64Imm8("rdx", 3)    // rdx = (len-1)*8
	c.emitAddR64R64("rdx", "rcx") // rdx = data + (len-1)*8
	c.emitMovFromAddr("rax", "rdx", 0)

	c.label("__ae_pop_done")
	c.emitPop("rbx")
	c.emitPop("rdi")
	c.emitPop("rbp")
	c.emitRet()
}

// emitLogicalHelpers emits _logical_true and _logical_false labels
func (c *Codegen) emitLogicalHelpers() {
	c.label("_logical_true")
	c.emitMovR64Imm64("rax", 1)
	c.emitRet()

	c.label("_logical_false")
	c.emitXorR64R64("rax", "rax")
	c.emitRet()
}

// push builtin: push(array, value) — append value to array.
// rdi = array pointer, rsi = value
// Returns updated array pointer in rax.
// Delegates to __ae_push.
func (c *Codegen) emitPushBuiltin() {
	c.label("push")
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")
	c.emitCall("__ae_push")
	c.emitPop("rbp")
	c.emitRet()
}

// emitStructFieldHelpers emits __ae_field_<Struct>_<name> and __ae_field_set_<Struct>_<name> helpers
// for every struct field in the program.
// Each __ae_field_<Struct>_<name> takes a struct pointer in rdi and returns the field value in rax.
// Each __ae_field_set_<Struct>_<name> takes a struct pointer in rdi and value in rsi.
// Labels are struct-qualified to avoid collisions when two structs share a field name.
func (c *Codegen) emitStructFieldHelpers() {
	for _, si := range c.structs {
		for _, f := range si.Fields {
			// Getter: __ae_field_<Struct>_<name>
			c.label("__ae_field_" + si.Name + "_" + f.Name)
			c.emitPush("rbp")
			c.emitMovR64R64("rbp", "rsp")
			// Load field at offset from struct pointer
			if f.Offset == 0 {
				c.emitMovFromAddr("rax", "rdi", 0)
			} else {
				c.emitMovFromAddr("rax", "rdi", int8(f.Offset))
			}
			c.emitPop("rbp")
			c.emitRet()

			// Setter: __ae_field_set_<Struct>_<name>
			c.label("__ae_field_set_" + si.Name + "_" + f.Name)
			c.emitPush("rbp")
			c.emitMovR64R64("rbp", "rsp")
			// Store value at field offset
			if f.Offset == 0 {
				c.emitMovR64ToAddr("rsi", "rdi", 0)
			} else {
				c.emitMovR64ToAddr("rsi", "rdi", int8(f.Offset))
			}
			c.emitPop("rbp")
			c.emitRet()
		}
	}
}

// shl rax, imm8 (shift left by immediate)
func (c *Codegen) emitShlR64Imm8(reg string, imm byte) {
	// REX.W + REX.B for r8-r15 (reg field is the rm field here, so REX.B selects it)
	c.emitRexWRB(0, regCodes[reg])
	c.emitByte(0xC1)
	c.emitByte(c.modRM(3, 4, regCodes[reg]))
	c.emitByte(imm)
}

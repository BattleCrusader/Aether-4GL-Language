package main

import (
	"encoding/binary"
	"fmt"
)

type Codegen struct {
	text    []byte
	data    []byte
	rodata  []byte
	bss     []byte
	labels  map[string]int
	strings map[string]int
	prog    *Program
	errors  []string
}

func NewCodegen(prog *Program) *Codegen {
	return &Codegen{
		labels:  make(map[string]int),
		strings: make(map[string]int),
		prog:    prog,
	}
}

func (c *Codegen) Errors() []string { return c.errors }

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

	// Emit string data
	c.emitStringData()

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
	// _start: ELF entry point
	c.label("_start")
	// Call main
	c.emitCall("main")
	// Exit with return code
	c.emitMovR64Imm64("rdi", 0) // exit code 0
	c.emitMovR64Imm64("rax", 60) // sys_exit
	c.emitSyscall()
}

func (c *Codegen) emitFunc(fd *FuncDecl) {
	if fd.Body == nil {
		return
	}

	c.label(fd.Name)

	// Function prologue
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")

	// Allocate stack space (simplified: 128 bytes)
	c.emitSubR64Imm8("rsp", 128)

	// Emit function body
	c.emitBlock(fd.Body, fd.Name)

	// Function epilogue
	c.label(fd.Name + "_epilogue")
	c.emitAddR64Imm8("rsp", 128)
	c.emitPop("rbp")
	c.emitRet()
}

func (c *Codegen) emitBlock(b *Block, funcName string) {
	for _, stmt := range b.Stmts {
		c.emitStmt(stmt, funcName)
	}
}

func (c *Codegen) emitStmt(stmt Stmt, funcName string) {
	switch s := stmt.(type) {
	case *ExprStmt:
		c.emitExpr(s.Expr)

	case *VarDecl:
		if s.Initializer != nil {
			c.emitExpr(s.Initializer)
		}

	case *ReturnStmt:
		if s.Expr != nil {
			c.emitExpr(s.Expr)
		}
		// Jump to epilogue
		c.emitJmp(funcName + "_epilogue")

	case *IfStmt:
		c.emitIf(s, funcName)

	case *WhileStmt:
		c.emitWhile(s, funcName)

	case *ForStmt:
		c.emitFor(s, funcName)

	case *Block:
		c.emitBlock(s, funcName)
	}
}

func (c *Codegen) emitIf(is *IfStmt, funcName string) {
	elseLabel := fmt.Sprintf("else_%d", len(c.labels))
	endLabel := fmt.Sprintf("endif_%d", len(c.labels))

	// Condition
	c.emitExpr(is.Cond)
	c.emitTestR64R64("rax", "rax")
	c.emitJz(elseLabel)

	// Then block
	c.emitBlock(is.ThenBlock, funcName)
	c.emitJmp(endLabel)

	// Else block
	c.label(elseLabel)
	if is.ElseBlock != nil {
		c.emitBlock(is.ElseBlock, funcName)
	}

	c.label(endLabel)
}

func (c *Codegen) emitWhile(ws *WhileStmt, funcName string) {
	startLabel := fmt.Sprintf("while_%d", len(c.labels))
	endLabel := fmt.Sprintf("endwhile_%d", len(c.labels))

	c.label(startLabel)
	c.emitExpr(ws.Cond)
	c.emitTestR64R64("rax", "rax")
	c.emitJz(endLabel)

	c.emitBlock(ws.Body, funcName)
	c.emitJmp(startLabel)
	c.label(endLabel)
}

func (c *Codegen) emitFor(fs *ForStmt, funcName string) {
	startLabel := fmt.Sprintf("for_%d", len(c.labels))
	endLabel := fmt.Sprintf("endfor_%d", len(c.labels))

	// Initialize loop variable
	c.emitExpr(fs.Iterable)

	c.label(startLabel)
	// Check condition
	c.emitTestR64R64("rax", "rax")
	c.emitJz(endLabel)

	c.emitBlock(fs.Body, funcName)
	c.emitJmp(startLabel)
	c.label(endLabel)
}

func (c *Codegen) emitExpr(expr Expr) {
	switch e := expr.(type) {
	case *LiteralExpr:
		c.emitLiteral(e)

	case *IdentExpr:
		// For now, just load 0 (placeholder)
		c.emitXorR64R64("rax", "rax")

	case *BinaryExpr:
		c.emitBinary(e)

	case *UnaryExpr:
		c.emitUnary(e)

	case *CallExpr:
		c.emitCallExpr(e)

	case *StringInterpolationExpr:
		c.emitStringInterpolation(e)
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
	}
}

func (c *Codegen) emitBinary(be *BinaryExpr) {
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
		c.emitDivR64("rbx")
	case "==":
		c.emitCmpR64R64("rax", "rbx")
		c.emitSete("al")
		c.emitMovzxR64R8("rax", "al")
	case "!=":
		c.emitCmpR64R64("rax", "rbx")
		c.emitSetne("al")
		c.emitMovzxR64R8("rax", "al")
	case "<":
		c.emitCmpR64R64("rax", "rbx")
		c.emitSetl("al")
		c.emitMovzxR64R8("rax", "al")
	case ">":
		c.emitCmpR64R64("rax", "rbx")
		c.emitSetg("al")
		c.emitMovzxR64R8("rax", "al")
	case "<=":
		c.emitCmpR64R64("rax", "rbx")
		c.emitSetle("al")
		c.emitMovzxR64R8("rax", "al")
	case ">=":
		c.emitCmpR64R64("rax", "rbx")
		c.emitSetge("al")
		c.emitMovzxR64R8("rax", "al")
	}
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
	}
}

func (c *Codegen) emitCallExpr(ce *CallExpr) {
	// Push args in reverse order
	for i := len(ce.Args) - 1; i >= 0; i-- {
		c.emitExpr(ce.Args[i])
		c.emitPush("rax")
	}

	// Pop into registers (simplified: first 6 args in rdi, rsi, rdx, rcx, r8, r9)
	regs := []string{"rdi", "rsi", "rdx", "rcx", "r8", "r9"}
	for i := 0; i < len(ce.Args) && i < 6; i++ {
		c.emitPop(regs[i])
	}

	// Call
	if ident, ok := ce.Callee.(*IdentExpr); ok {
		c.emitCall(ident.Name)
	}
}

func (c *Codegen) emitStringInterpolation(sie *StringInterpolationExpr) {
	// Simplified: just emit first part
	if len(sie.Parts) > 0 {
		c.emitExpr(sie.Parts[0])
	}
}

// ============================================================
// String data management
// ============================================================

func (c *Codegen) addString(s string) string {
	label := fmt.Sprintf("str_%d", len(c.strings))
	c.strings[label] = len(s) + 1
	return label
}

func (c *Codegen) emitStringData() {
	for label, _ := range c.strings {
		c.label(label)
		// Placeholder: emit "hello" string
		c.rodata = append(c.rodata, []byte("hello\000")...)
	}
}

// ============================================================
// Label management
// ============================================================

func (c *Codegen) label(name string) {
	c.labels[name] = len(c.text)
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
	c.emitRexW()
	c.emitByte(0xB8 + regCodes[reg]&7)
	c.emitU64(val)
}

// mov rax, rbx
func (c *Codegen) emitMovR64R64(dst, src string) {
	if regCodes[dst] >= 8 || regCodes[src] >= 8 {
		c.emitRexW()
	}
	c.emitByte(0x89)
	c.emitByte(c.modRM(3, regCodes[dst], regCodes[src]))
}

// lea rax, [label]
func (c *Codegen) emitLeaR64Label(reg, label string) {
	// REX.W + 8D + modrm + sib + disp32
	c.emitRexW()
	c.emitByte(0x8D)
	// Use RIP-relative addressing: mod=00, rm=5 (RIP)
	c.emitByte(c.modRM(0, regCodes[reg], 5))
	// Placeholder displacement (will be patched by linker)
	c.emitU32(0)
}

// add rax, rbx
func (c *Codegen) emitAddR64R64(dst, src string) {
	c.emitRexW()
	c.emitByte(0x01)
	c.emitByte(c.modRM(3, regCodes[dst], regCodes[src]))
}

// sub rax, imm8
func (c *Codegen) emitSubR64Imm8(reg string, imm byte) {
	c.emitRexW()
	c.emitByte(0x83)
	c.emitByte(c.modRM(3, 5, regCodes[reg]))
	c.emitByte(imm)
}

// add rax, imm8
func (c *Codegen) emitAddR64Imm8(reg string, imm byte) {
	c.emitRexW()
	c.emitByte(0x83)
	c.emitByte(c.modRM(3, 0, regCodes[reg]))
	c.emitByte(imm)
}

// sub rbx, rax (rbx = rbx - rax)
func (c *Codegen) emitSubR64R64(dst, src string) {
	c.emitRexW()
	c.emitByte(0x29)
	c.emitByte(c.modRM(3, regCodes[dst], regCodes[src]))
}

// mul rbx (rax = rax * rbx)
func (c *Codegen) emitMulR64(reg string) {
	c.emitRexW()
	c.emitByte(0xF7)
	c.emitByte(c.modRM(3, 4, regCodes[reg]))
}

// div rbx (rax = rax / rbx, rdx = rax % rbx)
func (c *Codegen) emitDivR64(reg string) {
	// xor rdx, rdx
	c.emitXorR64R64("rdx", "rdx")
	c.emitRexW()
	c.emitByte(0xF7)
	c.emitByte(c.modRM(3, 6, regCodes[reg]))
}

// xor rax, rax
func (c *Codegen) emitXorR64R64(dst, src string) {
	c.emitRexW()
	c.emitByte(0x31)
	c.emitByte(c.modRM(3, regCodes[dst], regCodes[src]))
}

// neg rax
func (c *Codegen) emitNegR64(reg string) {
	c.emitRexW()
	c.emitByte(0xF7)
	c.emitByte(c.modRM(3, 3, regCodes[reg]))
}

// not rax
func (c *Codegen) emitNotR64(reg string) {
	c.emitRexW()
	c.emitByte(0xF7)
	c.emitByte(c.modRM(3, 2, regCodes[reg]))
}

// test rax, rax
func (c *Codegen) emitTestR64R64(dst, src string) {
	c.emitRexW()
	c.emitByte(0x85)
	c.emitByte(c.modRM(3, regCodes[dst], regCodes[src]))
}

// cmp rax, rbx
func (c *Codegen) emitCmpR64R64(dst, src string) {
	c.emitRexW()
	c.emitByte(0x39)
	c.emitByte(c.modRM(3, regCodes[dst], regCodes[src]))
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
	c.emitU32(0) // placeholder, patched by linker
}

// jmp label
func (c *Codegen) emitJmp(label string) {
	c.emitByte(0xE9)
	c.emitU32(0) // placeholder, patched by linker
}

// jz label (jump if zero)
func (c *Codegen) emitJz(label string) {
	c.emitByte(0x0F)
	c.emitByte(0x84)
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

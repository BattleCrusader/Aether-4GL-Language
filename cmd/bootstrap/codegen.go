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
	strings map[string]string // label -> actual string content
	prog    *Program
	errors  []string

	// Per-function state
	funcName    string
	stackOffsets map[string]int // variable name -> stack offset (negative from rbp)
	stackSize   int
	labelCount  int
}

func NewCodegen(prog *Program) *Codegen {
	return &Codegen{
		labels:  make(map[string]int),
		strings: make(map[string]string),
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

	c.funcName = fd.Name
	c.stackOffsets = make(map[string]int)
	c.stackSize = 0
	c.labelCount = 0

	c.label(fd.Name)

	// Function prologue
	c.emitPush("rbp")
	c.emitMovR64R64("rbp", "rsp")

	// Calculate stack size needed
	// First pass: collect all local variables
	c.collectLocals(fd.Body)

	// Allocate stack space (aligned to 16 bytes)
	stackAlloc := c.stackSize
	if stackAlloc < 128 {
		stackAlloc = 128
	}
	// Align to 16 bytes
	stackAlloc = (stackAlloc + 15) & ^15

	if stackAlloc > 0 {
		c.emitSubR64Imm8("rsp", byte(stackAlloc))
	}

	// Emit function body
	c.emitBlock(fd.Body)

	// Function epilogue
	c.label(fd.Name + "_epilogue")
	if stackAlloc > 0 {
		c.emitAddR64Imm8("rsp", byte(stackAlloc))
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
		if s.Initializer != nil {
			c.emitExpr(s.Initializer)
			// Store to stack
			if offset, ok := c.stackOffsets[s.Name]; ok {
				c.emitMovR64Stack("rax", offset)
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
		// For now, just jump to end of current loop
		c.emitJmp(c.funcName + "_break")

	case *ContinueStmt:
		c.emitJmp(c.funcName + "_continue")
	}
}

func (c *Codegen) emitIf(is *IfStmt) {
	elseLabel := fmt.Sprintf("else_%d", c.nextLabel())
	endLabel := fmt.Sprintf("endif_%d", c.nextLabel())

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
		elseLabel = fmt.Sprintf("else_%d", c.nextLabel())

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
	startLabel := fmt.Sprintf("while_%d", c.nextLabel())
	endLabel := fmt.Sprintf("endwhile_%d", c.nextLabel())
	continueLabel := fmt.Sprintf("continue_%d", c.nextLabel())

	c.label(startLabel)
	c.emitExpr(ws.Cond)
	c.emitTestR64R64("rax", "rax")
	c.emitJz(endLabel)

	c.emitBlock(ws.Body)
	c.label(continueLabel)
	c.emitJmp(startLabel)
	c.label(endLabel)
}

func (c *Codegen) emitFor(fs *ForStmt) {
	startLabel := fmt.Sprintf("for_%d", c.nextLabel())
	endLabel := fmt.Sprintf("endfor_%d", c.nextLabel())

	// Initialize loop variable
	c.emitExpr(fs.Iterable)

	c.label(startLabel)
	// Check condition
	c.emitTestR64R64("rax", "rax")
	c.emitJz(endLabel)

	c.emitBlock(fs.Body)
	c.emitJmp(startLabel)
	c.label(endLabel)
}

func (c *Codegen) emitMatchStmt(ms *MatchStmt) {
	endLabel := fmt.Sprintf("match_end_%d", c.nextLabel())

	// Evaluate the match value
	c.emitExpr(ms.Value)
	c.emitPush("rax") // save match value on stack

	for _, mc := range ms.Cases {
		nextLabel := fmt.Sprintf("match_next_%d", c.nextLabel())

		// Pop match value
		c.emitPop("rbx")
		c.emitPush("rbx") // keep a copy for next case

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
		c.emitMovStackR64("rax", offset)
		return
	}
	// Check if it's a function (load address)
	// For now, just load 0 as placeholder
	c.emitXorR64R64("rax", "rax")
}

func (c *Codegen) emitBinary(be *BinaryExpr) {
	// Handle string operations specially
	if be.Op == "+" && c.isStringExpr(be.Left) {
		// String concatenation: emit runtime call
		c.emitExpr(be.Left)
		c.emitPush("rax")
		c.emitExpr(be.Right)
		c.emitPop("rdi")
		c.emitMovR64R64("rsi", "rax")
		c.emitCall("__ae_str_concat")
		return
	}

	if be.Op == "==" && c.isStringExpr(be.Left) {
		// String comparison
		c.emitExpr(be.Left)
		c.emitPush("rax")
		c.emitExpr(be.Right)
		c.emitPop("rdi")
		c.emitMovR64R64("rsi", "rax")
		c.emitCall("__ae_str_eq")
		return
	}

	if be.Op == "!=" && c.isStringExpr(be.Left) {
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
		c.emitDivR64("rbx")
	case "%":
		// div puts remainder in rdx
		c.emitDivR64("rbx")
		c.emitMovR64R64("rax", "rdx")
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
	case "&&":
		c.emitTestR64R64("rax", "rax")
		c.emitJz("_logical_false")
		c.emitTestR64R64("rbx", "rbx")
		c.emitSete("al")
		c.emitMovzxR64R8("rax", "al")
	case "||":
		c.emitTestR64R64("rax", "rax")
		c.emitJnz("_logical_true")
		c.emitTestR64R64("rbx", "rbx")
		c.emitSete("al")
		c.emitMovzxR64R8("rax", "al")
	case "&":
		c.emitAndR64R64("rax", "rbx")
	case "|":
		c.emitOrR64R64("rax", "rbx")
	case "^":
		c.emitXorR64R64("rax", "rbx")
	case "<<":
		c.emitShlR64("rbx")
	case ">>":
		c.emitShrR64("rbx")
	}
}

func (c *Codegen) isStringExpr(expr Expr) bool {
	switch e := expr.(type) {
	case *LiteralExpr:
		_, ok := e.Value.(string)
		return ok
	case *IdentExpr:
		// Conservative: assume strings are common
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
		// Postfix or prefix increment
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
		// Method call via member expression — handled by emitMemberExpr
		// This shouldn't happen since MethodCallExpr handles it
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

	// Call the method function (mangled name: Type_method)
	// For now, just call the method name directly
	c.emitCall(mce.Method)
}

func (c *Codegen) emitMemberExpr(me *MemberExpr) {
	// Evaluate the object
	c.emitExpr(me.Object)
	// For struct field access, we need the struct pointer in rax
	// and then load the field offset
	// For now, just push rax and call a runtime helper
	c.emitPush("rax")
	c.emitMovR64Imm64("rdi", 0) // placeholder: struct pointer
	c.emitCall("__ae_field_" + me.Member)
}

func (c *Codegen) emitIndexExpr(ie *IndexExpr) {
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
			c.emitMovR64Stack("rax", offset)
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
		c.emitCall("__ae_field_set_" + target.Member)
	}
}

func (c *Codegen) emitArrayLiteral(al *ArrayLiteralExpr) {
	// Allocate array on heap and populate
	// For now, just push elements and call runtime
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
	endLabel := fmt.Sprintf("match_expr_end_%d", c.nextLabel())

	// Evaluate the match value
	c.emitExpr(me.Value)
	c.emitPush("rax") // save match value on stack

	for _, mc := range me.Cases {
		nextLabel := fmt.Sprintf("match_expr_next_%d", c.nextLabel())

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
	elseLabel := fmt.Sprintf("ifexpr_else_%d", c.nextLabel())
	endLabel := fmt.Sprintf("ifexpr_end_%d", c.nextLabel())

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
		// Emit string content + null terminator
		c.rodata = append(c.rodata, []byte(content)...)
		c.rodata = append(c.rodata, 0)
	}
}

// ============================================================
// Label management
// ============================================================

func (c *Codegen) label(name string) {
	c.labels[name] = len(c.text)
}

func (c *Codegen) nextLabel() int {
	c.labelCount++
	return c.labelCount
}

// ============================================================
// Stack frame helpers
// ============================================================

// mov rax, [rbp - offset]
func (c *Codegen) emitMovStackR64(reg string, offset int) {
	// REX.W + 8B + modrm(01, reg, rbp) + disp8
	c.emitRexW()
	c.emitByte(0x8B)
	c.emitByte(c.modRM(1, regCodes[reg], 5)) // [rbp + disp8]
	c.emitByte(byte(-offset)) // negative offset from rbp
}

// mov [rbp - offset], rax
func (c *Codegen) emitMovR64Stack(reg string, offset int) {
	// REX.W + 89 + modrm(01, reg, rbp) + disp8
	c.emitRexW()
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

// and rax, rbx
func (c *Codegen) emitAndR64R64(dst, src string) {
	c.emitRexW()
	c.emitByte(0x21)
	c.emitByte(c.modRM(3, regCodes[dst], regCodes[src]))
}

// or rax, rbx
func (c *Codegen) emitOrR64R64(dst, src string) {
	c.emitRexW()
	c.emitByte(0x09)
	c.emitByte(c.modRM(3, regCodes[dst], regCodes[src]))
}

// shl rax, cl (shift left by cl)
func (c *Codegen) emitShlR64(reg string) {
	c.emitRexW()
	c.emitByte(0xD3)
	c.emitByte(c.modRM(3, 4, regCodes[reg]))
}

// shr rax, cl (shift right by cl)
func (c *Codegen) emitShrR64(reg string) {
	c.emitRexW()
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

// jnz label (jump if not zero)
func (c *Codegen) emitJnz(label string) {
	c.emitByte(0x0F)
	c.emitByte(0x85)
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

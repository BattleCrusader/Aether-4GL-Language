// ast.go - AST node definitions for the Aether bootstrap compiler
// Phase 1: Core language features
package main

import "fmt"

// Pos represents a source position (line, column)
type Pos struct {
	Line int
	Col  int
}

func (p Pos) String() string {
	return fmt.Sprintf("%d:%d", p.Line, p.Col)
}

// Node is the interface all AST nodes implement
type Node interface {
	Pos() Pos
	String() string
	nodeType()
}

// =============================================================================
// Program
// =============================================================================

type Program struct {
	pos      Pos
	Decls    []Decl
	Filepath string
}

func (p *Program) Pos() Pos { return p.pos }
func (p *Program) String() string { return fmt.Sprintf("Program{Decls:%d}", len(p.Decls)) }
func (p *Program) nodeType() {}

// =============================================================================
// Declarations
// =============================================================================

type Decl interface {
	Node
	declNode()
}

type FuncDecl struct {
	pos        Pos
	Name       string
	Params     []*Param
	ReturnType *TypeAnnotation
	Body       *Block
	Attrs      []*Attribute
	IsExtern   bool
	IsVariadic bool
}

func (f *FuncDecl) Pos() Pos { return f.pos }
func (f *FuncDecl) String() string { return fmt.Sprintf("FuncDecl{%s}", f.Name) }
func (f *FuncDecl) nodeType() {}
func (f *FuncDecl) declNode() {}

type Param struct {
	pos          Pos
	Name         string
	Type         *TypeAnnotation
	DefaultValue Expr
}

func (p *Param) Pos() Pos { return p.pos }
func (p *Param) String() string { return fmt.Sprintf("Param{%s}", p.Name) }
func (p *Param) nodeType() {}

type VarDecl struct {
	pos         Pos
	Name        string
	Type        *TypeAnnotation
	Initializer Expr
	IsLet       bool
}

func (v *VarDecl) Pos() Pos { return v.pos }
func (v *VarDecl) String() string {
	kind := "var"
	if v.IsLet {
		kind = "let"
	}
	return fmt.Sprintf("%s{%s}", kind, v.Name)
}
func (v *VarDecl) nodeType() {}
func (v *VarDecl) declNode() {}
func (v *VarDecl) stmtNode() {}

type ClassDecl struct {
	pos        Pos
	Name       string
	SuperClass string
	Body       *Block
}

func (c *ClassDecl) Pos() Pos { return c.pos }
func (c *ClassDecl) String() string { return fmt.Sprintf("ClassDecl{%s}", c.Name) }
func (c *ClassDecl) nodeType() {}
func (c *ClassDecl) declNode() {}

type StructDecl struct {
	pos    Pos
	Name   string
	Fields []*StructField
}

func (s *StructDecl) Pos() Pos { return s.pos }
func (s *StructDecl) String() string { return fmt.Sprintf("StructDecl{%s}", s.Name) }
func (s *StructDecl) nodeType() {}
func (s *StructDecl) declNode() {}

type StructField struct {
	pos  Pos
	Name string
	Type *TypeAnnotation
}

func (s *StructField) Pos() Pos { return s.pos }
func (s *StructField) String() string { return fmt.Sprintf("StructField{%s}", s.Name) }
func (s *StructField) nodeType() {}

type EnumDecl struct {
	pos      Pos
	Name     string
	Variants []*EnumVariant
}

func (e *EnumDecl) Pos() Pos { return e.pos }
func (e *EnumDecl) String() string { return fmt.Sprintf("EnumDecl{%s}", e.Name) }
func (e *EnumDecl) nodeType() {}
func (e *EnumDecl) declNode() {}

type EnumVariant struct {
	pos     Pos
	Name    string
	Payload []*TypeAnnotation
}

func (e *EnumVariant) Pos() Pos { return e.pos }
func (e *EnumVariant) String() string { return fmt.Sprintf("EnumVariant{%s}", e.Name) }
func (e *EnumVariant) nodeType() {}

type ImportDecl struct {
	pos  Pos
	Path string
}

func (i *ImportDecl) Pos() Pos { return i.pos }
func (i *ImportDecl) String() string { return fmt.Sprintf("ImportDecl{%s}", i.Path) }
func (i *ImportDecl) nodeType() {}
func (i *ImportDecl) declNode() {}

type ConstDecl struct {
	pos   Pos
	Name  string
	Type  *TypeAnnotation
	Value Expr
}

func (c *ConstDecl) Pos() Pos { return c.pos }
func (c *ConstDecl) String() string { return fmt.Sprintf("ConstDecl{%s}", c.Name) }
func (c *ConstDecl) nodeType() {}
func (c *ConstDecl) declNode() {}

// =============================================================================
// Statements
// =============================================================================

type Stmt interface {
	Node
	stmtNode()
}

type Block struct {
	pos   Pos
	Stmts []Stmt
}

func (b *Block) Pos() Pos { return b.pos }
func (b *Block) String() string { return fmt.Sprintf("Block{Stmts:%d}", len(b.Stmts)) }
func (b *Block) nodeType() {}
func (b *Block) stmtNode() {}

type ReturnStmt struct {
	pos  Pos
	Expr Expr
}

func (r *ReturnStmt) Pos() Pos { return r.pos }
func (r *ReturnStmt) String() string { return "ReturnStmt" }
func (r *ReturnStmt) nodeType() {}
func (r *ReturnStmt) stmtNode() {}

type BreakStmt struct {
	pos   Pos
	Label string
}

func (b *BreakStmt) Pos() Pos { return b.pos }
func (b *BreakStmt) String() string {
	if b.Label != "" {
		return fmt.Sprintf("BreakStmt{%s}", b.Label)
	}
	return "BreakStmt"
}
func (b *BreakStmt) nodeType() {}
func (b *BreakStmt) stmtNode() {}

type ContinueStmt struct {
	pos   Pos
	Label string
}

func (c *ContinueStmt) Pos() Pos { return c.pos }
func (c *ContinueStmt) String() string {
	if c.Label != "" {
		return fmt.Sprintf("ContinueStmt{%s}", c.Label)
	}
	return "ContinueStmt"
}
func (c *ContinueStmt) nodeType() {}
func (c *ContinueStmt) stmtNode() {}

type ExprStmt struct {
	pos  Pos
	Expr Expr
}

func (e *ExprStmt) Pos() Pos { return e.pos }
func (e *ExprStmt) String() string { return "ExprStmt" }
func (e *ExprStmt) nodeType() {}
func (e *ExprStmt) stmtNode() {}

type IfStmt struct {
	pos        Pos
	Cond       Expr
	ThenBlock  *Block
	ElifBlocks []*ElifBlock
	ElseBlock  *Block
}

type ElifBlock struct {
	Cond  Expr
	Block *Block
}

func (i *IfStmt) Pos() Pos { return i.pos }
func (i *IfStmt) String() string { return "IfStmt" }
func (i *IfStmt) nodeType() {}
func (i *IfStmt) stmtNode() {}

type WhileStmt struct {
	pos   Pos
	Cond  Expr
	Body  *Block
	Label string
}

func (w *WhileStmt) Pos() Pos { return w.pos }
func (w *WhileStmt) String() string { return "WhileStmt" }
func (w *WhileStmt) nodeType() {}
func (w *WhileStmt) stmtNode() {}

type ForStmt struct {
	pos       Pos
	Variable  string
	IndexVar  string
	Iterable  Expr
	Body      *Block
	Label     string
}

func (f *ForStmt) Pos() Pos { return f.pos }
func (f *ForStmt) String() string { return "ForStmt" }
func (f *ForStmt) nodeType() {}
func (f *ForStmt) stmtNode() {}

type MatchStmt struct {
	pos   Pos
	Value Expr
	Cases []*MatchCase
}

func (m *MatchStmt) Pos() Pos { return m.pos }
func (m *MatchStmt) String() string { return "MatchStmt" }
func (m *MatchStmt) nodeType() {}
func (m *MatchStmt) stmtNode() {}

type MatchCase struct {
	pos     Pos
	Pattern Expr
	Body    Stmt
}

func (m *MatchCase) Pos() Pos { return m.pos }
func (m *MatchCase) String() string { return "MatchCase" }
func (m *MatchCase) nodeType() {}

type DeferStmt struct {
	pos  Pos
	Body *Block
}

func (d *DeferStmt) Pos() Pos { return d.pos }
func (d *DeferStmt) String() string { return "DeferStmt" }
func (d *DeferStmt) nodeType() {}
func (d *DeferStmt) stmtNode() {}

type TryStmt struct {
	pos       Pos
	Body      *Block
	CatchVar  string
	CatchBody *Block
}

func (t *TryStmt) Pos() Pos { return t.pos }
func (t *TryStmt) String() string { return "TryStmt" }
func (t *TryStmt) nodeType() {}
func (t *TryStmt) stmtNode() {}

type ThrowStmt struct {
	pos  Pos
	Expr Expr
}

func (t *ThrowStmt) Pos() Pos { return t.pos }
func (t *ThrowStmt) String() string { return "ThrowStmt" }
func (t *ThrowStmt) nodeType() {}
func (t *ThrowStmt) stmtNode() {}

// =============================================================================
// Expressions
// =============================================================================

type Expr interface {
	Node
	exprNode()
}

type CallExpr struct {
	pos    Pos
	Callee Expr
	Args   []Expr
}

func (c *CallExpr) Pos() Pos { return c.pos }
func (c *CallExpr) String() string { return "CallExpr" }
func (c *CallExpr) nodeType() {}
func (c *CallExpr) exprNode() {}

type MethodCallExpr struct {
	pos    Pos
	Object Expr
	Method string
	Args   []Expr
}

func (m *MethodCallExpr) Pos() Pos { return m.pos }
func (m *MethodCallExpr) String() string { return fmt.Sprintf("MethodCallExpr{.%s}", m.Method) }
func (m *MethodCallExpr) nodeType() {}
func (m *MethodCallExpr) exprNode() {}

type BinaryExpr struct {
	pos   Pos
	Op    string
	Left  Expr
	Right Expr
}

func (b *BinaryExpr) Pos() Pos { return b.pos }
func (b *BinaryExpr) String() string { return fmt.Sprintf("BinaryExpr{%s}", b.Op) }
func (b *BinaryExpr) nodeType() {}
func (b *BinaryExpr) exprNode() {}

type UnaryExpr struct {
	pos       Pos
	Op        string
	Operand   Expr
	IsPostfix bool
}

func (u *UnaryExpr) Pos() Pos { return u.pos }
func (u *UnaryExpr) String() string { return fmt.Sprintf("UnaryExpr{%s}", u.Op) }
func (u *UnaryExpr) nodeType() {}
func (u *UnaryExpr) exprNode() {}

type IdentExpr struct {
	pos  Pos
	Name string
}

func (i *IdentExpr) Pos() Pos { return i.pos }
func (i *IdentExpr) String() string { return fmt.Sprintf("IdentExpr{%s}", i.Name) }
func (i *IdentExpr) nodeType() {}
func (i *IdentExpr) exprNode() {}

type LiteralExpr struct {
	pos   Pos
	Value interface{}
}

func (l *LiteralExpr) Pos() Pos { return l.pos }
func (l *LiteralExpr) String() string {
	switch v := l.Value.(type) {
	case int64:
		return fmt.Sprintf("LiteralExpr{%d}", v)
	case float64:
		return fmt.Sprintf("LiteralExpr{%f}", v)
	case string:
		return fmt.Sprintf("LiteralExpr{\"%s\"}", v)
	case bool:
		return fmt.Sprintf("LiteralExpr{%t}", v)
	case rune:
		return fmt.Sprintf("LiteralExpr{'%c'}", v)
	default:
		return "LiteralExpr{?}"
	}
}
func (l *LiteralExpr) nodeType() {}
func (l *LiteralExpr) exprNode() {}

type ArrayLiteralExpr struct {
	pos      Pos
	Elements []Expr
}

func (a *ArrayLiteralExpr) Pos() Pos { return a.pos }
func (a *ArrayLiteralExpr) String() string { return fmt.Sprintf("ArrayLiteralExpr{Elems:%d}", len(a.Elements)) }
func (a *ArrayLiteralExpr) nodeType() {}
func (a *ArrayLiteralExpr) exprNode() {}

type TupleExpr struct {
	pos      Pos
	Elements []Expr
}

func (t *TupleExpr) Pos() Pos { return t.pos }
func (t *TupleExpr) String() string { return fmt.Sprintf("TupleExpr{Elems:%d}", len(t.Elements)) }
func (t *TupleExpr) nodeType() {}
func (t *TupleExpr) exprNode() {}

type StringInterpolationExpr struct {
	pos   Pos
	Parts []Expr
}

func (s *StringInterpolationExpr) Pos() Pos { return s.pos }
func (s *StringInterpolationExpr) String() string { return "StringInterpolationExpr" }
func (s *StringInterpolationExpr) nodeType() {}
func (s *StringInterpolationExpr) exprNode() {}

type MemberExpr struct {
	pos    Pos
	Object Expr
	Member string
}

func (m *MemberExpr) Pos() Pos { return m.pos }
func (m *MemberExpr) String() string { return fmt.Sprintf("MemberExpr{.%s}", m.Member) }
func (m *MemberExpr) nodeType() {}
func (m *MemberExpr) exprNode() {}

type IndexExpr struct {
	pos   Pos
	Object Expr
	Index  Expr
}

func (i *IndexExpr) Pos() Pos { return i.pos }
func (i *IndexExpr) String() string { return "IndexExpr" }
func (i *IndexExpr) nodeType() {}
func (i *IndexExpr) exprNode() {}

type AssignmentExpr struct {
	pos    Pos
	Target Expr
	Op     string
	Value  Expr
}

func (a *AssignmentExpr) Pos() Pos { return a.pos }
func (a *AssignmentExpr) String() string { return fmt.Sprintf("AssignmentExpr{%s}", a.Op) }
func (a *AssignmentExpr) nodeType() {}
func (a *AssignmentExpr) exprNode() {}

type ScopeExpr struct {
	pos   Pos
	Block *Block
}

func (s *ScopeExpr) Pos() Pos { return s.pos }
func (s *ScopeExpr) String() string { return "ScopeExpr" }
func (s *ScopeExpr) nodeType() {}
func (s *ScopeExpr) exprNode() {}

type IfExpr struct {
	pos        Pos
	Cond       Expr
	ThenExpr   Stmt
	ElifBlocks []*ElifBlock
	ElseExpr   Stmt
}

func (i *IfExpr) Pos() Pos { return i.pos }
func (i *IfExpr) String() string { return "IfExpr" }
func (i *IfExpr) nodeType() {}
func (i *IfExpr) exprNode() {}

type MatchExpr struct {
	pos   Pos
	Value Expr
	Cases []*MatchCase
}

func (m *MatchExpr) Pos() Pos { return m.pos }
func (m *MatchExpr) String() string { return "MatchExpr" }
func (m *MatchExpr) nodeType() {}
func (m *MatchExpr) exprNode() {}

// =============================================================================
// Types
// =============================================================================

type TypeAnnotation struct {
	pos  Pos
	Name string
}

func (t *TypeAnnotation) Pos() Pos { return t.pos }
func (t *TypeAnnotation) String() string { return fmt.Sprintf("TypeAnnotation{%s}", t.Name) }
func (t *TypeAnnotation) nodeType() {}

// =============================================================================
// Attributes
// =============================================================================

type Attribute struct {
	pos  Pos
	Name string
	Args []Expr
}

func (a *Attribute) Pos() Pos { return a.pos }
func (a *Attribute) String() string { return fmt.Sprintf("Attribute{@%s}", a.Name) }
func (a *Attribute) nodeType() {}

// =============================================================================
// Helper methods for debugging
// =============================================================================

func (p *Program) PrettyPrint() string {
	var result string
	for _, decl := range p.Decls {
		result += decl.String() + "\n"
	}
	return result
}

func PrintTree(n interface{}, indent int) string {
	var result string
	for i := 0; i < indent; i++ {
		result += "  "
	}
	if node, ok := n.(Node); ok {
		result += node.String() + "\n"
	} else {
		result += fmt.Sprintf("%v\n", n)
	}

	switch node := n.(type) {
	case *Program:
		for _, d := range node.Decls {
			result += PrintTree(d, indent+1)
		}
	case *FuncDecl:
		if node.Body != nil {
			result += PrintTree(node.Body, indent+1)
		}
	case *Block:
		for _, s := range node.Stmts {
			result += PrintTree(s, indent+1)
		}
	case *IfStmt:
		result += PrintTree(node.ThenBlock, indent+1)
		for _, e := range node.ElifBlocks {
			result += PrintTree(e.Block, indent+1)
		}
		if node.ElseBlock != nil {
			result += PrintTree(node.ElseBlock, indent+1)
		}
	case *WhileStmt:
		result += PrintTree(node.Body, indent+1)
	case *ForStmt:
		result += PrintTree(node.Body, indent+1)
	case *MatchStmt:
		for _, c := range node.Cases {
			result += PrintTree(c.Body, indent+1)
		}
	case *ExprStmt:
		result += PrintTree(node.Expr, indent+1)
	case *CallExpr:
		for _, a := range node.Args {
			result += PrintTree(a, indent+1)
		}
	case *BinaryExpr:
		result += PrintTree(node.Left, indent+1)
		result += PrintTree(node.Right, indent+1)
	case *UnaryExpr:
		result += PrintTree(node.Operand, indent+1)
	}

	return result
}

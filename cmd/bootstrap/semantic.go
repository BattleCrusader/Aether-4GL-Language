package main

import "fmt"

type SemanticAnalyzer struct {
	scopes  []map[string]SymbolInfo
	errors  []string
	funcs   map[string]*FuncDecl
	globals map[string]SymbolInfo
}

type SymbolKind int

const (
	SYM_VAR SymbolKind = iota
	SYM_FUNC
	SYM_TYPE
	SYM_CONST
)

type SymbolInfo struct {
	Kind  SymbolKind
	Type  string
	IsLet bool
}

func NewSemanticAnalyzer() *SemanticAnalyzer {
	return &SemanticAnalyzer{
		scopes:  []map[string]SymbolInfo{{}},
		errors:  []string{},
		funcs:   make(map[string]*FuncDecl),
		globals: make(map[string]SymbolInfo),
	}
}

func (s *SemanticAnalyzer) Errors() []string { return s.errors }

func (s *SemanticAnalyzer) Analyze(prog *Program) {
	// First pass: collect all function declarations
	for _, decl := range prog.Decls {
		switch d := decl.(type) {
		case *FuncDecl:
			s.funcs[d.Name] = d
			s.globals[d.Name] = SymbolInfo{Kind: SYM_FUNC, Type: s.typeName(d.ReturnType)}
		case *VarDecl:
			s.globals[d.Name] = SymbolInfo{Kind: SYM_VAR, Type: s.typeName(d.Type), IsLet: d.IsLet}
		case *ConstDecl:
			s.globals[d.Name] = SymbolInfo{Kind: SYM_CONST, Type: s.typeName(d.Type)}
		}
	}

	// Second pass: check function bodies
	for _, decl := range prog.Decls {
		switch d := decl.(type) {
		case *FuncDecl:
			s.checkFunc(d)
		}
	}
}

func (s *SemanticAnalyzer) checkFunc(fd *FuncDecl) {
	s.enterScope()
	defer s.exitScope()

	// Add parameters to scope
	for _, p := range fd.Params {
		s.declare(p.Name, SymbolInfo{Kind: SYM_VAR, Type: s.typeName(p.Type)})
	}

	if fd.Body != nil {
		s.checkBlock(fd.Body)
	}
}

func (s *SemanticAnalyzer) checkBlock(b *Block) {
	s.enterScope()
	defer s.exitScope()

	for _, stmt := range b.Stmts {
		s.checkStmt(stmt)
	}
}

func (s *SemanticAnalyzer) checkStmt(stmt Stmt) {
	switch st := stmt.(type) {
	case *VarDecl:
		if st.IsLet && st.Initializer == nil {
			s.error(st.Pos(), "let declarations must have an initializer")
		}
		if st.Initializer != nil {
			s.checkExpr(st.Initializer)
		}
		s.declare(st.Name, SymbolInfo{Kind: SYM_VAR, Type: s.typeName(st.Type), IsLet: st.IsLet})

	case *ReturnStmt:
		if st.Expr != nil {
			s.checkExpr(st.Expr)
		}

	case *BreakStmt, *ContinueStmt:
		// Validated during codegen

	case *ExprStmt:
		if st.Expr != nil {
			s.checkExpr(st.Expr)
		}

	case *IfStmt:
		s.checkExpr(st.Cond)
		s.checkBlock(st.ThenBlock)
		for _, eb := range st.ElifBlocks {
			s.checkExpr(eb.Cond)
			s.checkBlock(eb.Block)
		}
		if st.ElseBlock != nil {
			s.checkBlock(st.ElseBlock)
		}

	case *WhileStmt:
		s.checkExpr(st.Cond)
		s.checkBlock(st.Body)

	case *ForStmt:
		s.enterScope()
		s.declare(st.Variable, SymbolInfo{Kind: SYM_VAR, Type: "auto"})
		if st.IndexVar != "" {
			s.declare(st.IndexVar, SymbolInfo{Kind: SYM_VAR, Type: "u64"})
		}
		s.checkExpr(st.Iterable)
		s.checkBlock(st.Body)
		s.exitScope()

	case *MatchStmt:
		s.checkExpr(st.Value)
		for _, mc := range st.Cases {
			s.checkExpr(mc.Pattern)
			s.checkStmt(mc.Body)
		}

	case *DeferStmt:
		s.checkBlock(st.Body)

	case *TryStmt:
		s.checkBlock(st.Body)
		if st.CatchBody != nil {
			s.enterScope()
			s.declare(st.CatchVar, SymbolInfo{Kind: SYM_VAR, Type: "string"})
			s.checkBlock(st.CatchBody)
			s.exitScope()
		}

	case *ThrowStmt:
		s.checkExpr(st.Expr)

	case *Block:
		s.checkBlock(st)
	}
}

func (s *SemanticAnalyzer) checkExpr(expr Expr) string {
	if expr == nil {
		return "void"
	}

	switch e := expr.(type) {
	case *LiteralExpr:
		switch e.Value.(type) {
		case int64:
			return "i64"
		case float64:
			return "f64"
		case string:
			return "string"
		case bool:
			return "bool"
		case nil:
			return "none"
		default:
			return "unknown"
		}

	case *IdentExpr:
		info := s.lookup(e.Name)
		if info == nil {
			s.error(e.Pos(), fmt.Sprintf("undefined symbol: %s", e.Name))
			return "unknown"
		}
		return info.Type

	case *BinaryExpr:
		leftType := s.checkExpr(e.Left)
		rightType := s.checkExpr(e.Right)
		_ = leftType
		_ = rightType
		switch e.Op {
		case "+", "-", "*", "/", "%":
			return "i64"
		case "==", "!=", "<", ">", "<=", ">=":
			return "bool"
		case "&&", "||":
			return "bool"
		case "&", "|", "^", "<<", ">>":
			return "i64"
		case "..", "..=":
			return "range"
		default:
			return "unknown"
		}

	case *UnaryExpr:
		s.checkExpr(e.Operand)
		switch e.Op {
		case "!", "~":
			return "bool"
		case "++", "--", "-":
			return "i64"
		case "#":
			return "u64"
		case "copy", "heap":
			return "unknown"
		default:
			return "unknown"
		}

	case *CallExpr:
		s.checkExpr(e.Callee)
		for _, arg := range e.Args {
			s.checkExpr(arg)
		}
		if ident, ok := e.Callee.(*IdentExpr); ok {
			if fd, ok := s.funcs[ident.Name]; ok {
				return s.typeName(fd.ReturnType)
			}
		}
		return "unknown"

	case *MethodCallExpr:
		s.checkExpr(e.Object)
		for _, arg := range e.Args {
			s.checkExpr(arg)
		}
		return "unknown"

	case *MemberExpr:
		s.checkExpr(e.Object)
		return "unknown"

	case *IndexExpr:
		s.checkExpr(e.Object)
		s.checkExpr(e.Index)
		return "unknown"

	case *AssignmentExpr:
		s.checkExpr(e.Target)
		s.checkExpr(e.Value)
		return "unknown"

	case *ArrayLiteralExpr:
		for _, el := range e.Elements {
			s.checkExpr(el)
		}
		return "array"

	case *TupleExpr:
		for _, el := range e.Elements {
			s.checkExpr(el)
		}
		return "tuple"

	case *IfExpr:
		s.checkExpr(e.Cond)
		s.checkStmt(e.ThenExpr)
		for _, eb := range e.ElifBlocks {
			s.checkExpr(eb.Cond)
			s.checkStmt(eb.Block)
		}
		if e.ElseExpr != nil {
			s.checkStmt(e.ElseExpr)
		}
		return "unknown"

	case *MatchExpr:
		s.checkExpr(e.Value)
		for _, mc := range e.Cases {
			s.checkExpr(mc.Pattern)
			s.checkStmt(mc.Body)
		}
		return "unknown"

	default:
		return "unknown"
	}
}

func (s *SemanticAnalyzer) enterScope() {
	s.scopes = append(s.scopes, make(map[string]SymbolInfo))
}

func (s *SemanticAnalyzer) exitScope() {
	s.scopes = s.scopes[:len(s.scopes)-1]
}

func (s *SemanticAnalyzer) declare(name string, info SymbolInfo) {
	if name == "" {
		return
	}
	s.scopes[len(s.scopes)-1][name] = info
}

func (s *SemanticAnalyzer) lookup(name string) *SymbolInfo {
	for i := len(s.scopes) - 1; i >= 0; i-- {
		if info, ok := s.scopes[i][name]; ok {
			return &info
		}
	}
	if info, ok := s.globals[name]; ok {
		return &info
	}
	return nil
}

func (s *SemanticAnalyzer) typeName(ta *TypeAnnotation) string {
	if ta == nil {
		return "void"
	}
	return ta.Name
}

func (s *SemanticAnalyzer) error(pos Pos, msg string) {
	s.errors = append(s.errors, fmt.Sprintf("line %d:%d: %s", pos.Line, pos.Col, msg))
}

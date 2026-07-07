package main

import (
	"fmt"
	"strconv"
	"strings"
)

// Precedence levels
const (
	PREC_NONE       = iota
	PREC_ASSIGNMENT // = += -= *= /= %= &= |= ^= <<= >>=
	PREC_OR         // or
	PREC_AND        // and
	PREC_EQUALITY   // == !=
	PREC_COMPARISON // < > <= >=
	PREC_TERM       // + -
	PREC_FACTOR     // * / %
	PREC_SHIFT      // << >>
	PREC_BITAND     // &
	PREC_BITXOR     // ^
	PREC_BITOR      // |
	PREC_UNARY      // ! ~ ++ -- -
	PREC_CALL       // () [] . ::
)

type Parser struct {
	tokens []Token
	current int
	errors  []string
}

func NewParser(tokens []Token) *Parser {
	return &Parser{tokens: tokens, current: 0}
}

func (p *Parser) Errors() []string { return p.errors }

func (p *Parser) Parse() *Program {
	prog := &Program{pos: Pos{Line: 1, Col: 1}}
	for !p.isAtEnd() {
		decl := p.parseDecl()
		if decl != nil {
			prog.Decls = append(prog.Decls, decl)
		}
	}
	return prog
}

// ============================================================
// Token helpers
// ============================================================

func (p *Parser) isAtEnd() bool {
	return p.peek().Type == TOKEN_EOF
}

func (p *Parser) peek() Token {
	return p.tokens[p.current]
}

func (p *Parser) previous() Token {
	return p.tokens[p.current-1]
}

func (p *Parser) advance() Token {
	if !p.isAtEnd() {
		p.current++
	}
	return p.previous()
}

func (p *Parser) check(typ TokenType) bool {
	if p.isAtEnd() {
		return false
	}
	return p.peek().Type == typ
}

func (p *Parser) match(types ...TokenType) bool {
	for _, typ := range types {
		if p.check(typ) {
			p.advance()
			return true
		}
	}
	return false
}

func (p *Parser) expect(typ TokenType, msg string) Token {
	if p.check(typ) {
		return p.advance()
	}
	p.errorAt(p.peek(), msg)
	return Token{Type: typ, Lexeme: "", Line: p.peek().Line, Column: p.peek().Column}
}

func (p *Parser) errorAt(tok Token, msg string) {
	p.errors = append(p.errors, fmt.Sprintf("line %d:%d: %s", tok.Line, tok.Column, msg))
}

func (p *Parser) synchronize() {
	for !p.isAtEnd() {
		switch p.peek().Type {
		case TOKEN_KW_FUNC, TOKEN_KW_CLASS, TOKEN_KW_STRUCT, TOKEN_KW_ENUM,
			TOKEN_KW_IMPORT, TOKEN_KW_LET, TOKEN_KW_VAR, TOKEN_KW_CONST,
			TOKEN_KW_PUBLIC, TOKEN_KW_PRIVATE, TOKEN_KW_EXPORT:
			return
		}
		p.advance()
	}
}

// ============================================================
// Declarations
// ============================================================

func (p *Parser) parseDecl() Decl {
	if p.match(TOKEN_KW_PUBLIC) || p.match(TOKEN_KW_PRIVATE) || p.match(TOKEN_KW_EXPORT) {
		// visibility modifier — peek at what follows
	}
	// Collect leading attributes
	var attrs []*Attribute
	for p.check(TOKEN_AT) {
		p.advance()
		attr := &Attribute{pos: p.previous().Pos()}
		if p.check(TOKEN_IDENT) {
			attr.Name = p.advance().Lexeme
		}
		attrs = append(attrs, attr)
	}
	if p.check(TOKEN_KW_FUNC) {
		fd := p.parseFuncDecl()
		fd.Attrs = append(attrs, fd.Attrs...)
		return fd
	}
	if p.check(TOKEN_KW_CLASS) {
		return p.parseClassDecl()
	}
	if p.check(TOKEN_KW_STRUCT) {
		return p.parseStructDecl()
	}
	if p.check(TOKEN_KW_ENUM) {
		return p.parseEnumDecl()
	}
	if p.check(TOKEN_KW_IMPORT) {
		return p.parseImportDecl()
	}
	if p.check(TOKEN_KW_CONST) {
		return p.parseConstDecl()
	}
	if p.check(TOKEN_KW_LET) || p.check(TOKEN_KW_VAR) {
		return p.parseVarDecl()
	}
	// Skip unknown tokens
	p.errorAt(p.peek(), "unexpected token in declaration")
	p.advance()
	return nil
}

// funcDecl: func name(params) [: returnType] { body }
func (p *Parser) parseFuncDecl() *FuncDecl {
	fd := &FuncDecl{pos: p.peek().Pos()}
	p.advance() // consume 'func'

	// Parse attributes before name
	for p.check(TOKEN_AT) {
		p.advance()
		attr := &Attribute{pos: p.previous().Pos()}
		if p.check(TOKEN_IDENT) {
			attr.Name = p.advance().Lexeme
		}
		fd.Attrs = append(fd.Attrs, attr)
	}

	// Function name
	if p.check(TOKEN_IDENT) {
		fd.Name = p.advance().Lexeme
	} else {
		p.errorAt(p.peek(), "expected function name")
		return fd
	}

	// Parameters
	p.expect(TOKEN_LPAREN, "expected '(' after function name")
	if !p.check(TOKEN_RPAREN) {
		fd.Params = p.parseParamList()
	}
	p.expect(TOKEN_RPAREN, "expected ')' after parameters")

	// Return type (optional)
	if p.match(TOKEN_COLON) {
		fd.ReturnType = p.parseType()
	}

	// Body
	if p.check(TOKEN_LBRACE) {
		fd.Body = p.parseBlock()
	} else if p.match(TOKEN_SEMICOLON) {
		// Forward declaration
	} else {
		p.errorAt(p.peek(), "expected '{' or ';' after function signature")
	}

	return fd
}

func (p *Parser) parseParamList() []*Param {
	var params []*Param
	for {
		if p.check(TOKEN_RPAREN) {
			break
		}
		param := &Param{pos: p.peek().Pos()}
		if p.check(TOKEN_IDENT) {
			param.Name = p.advance().Lexeme
		} else {
			p.errorAt(p.peek(), "expected parameter name")
			break
		}
		if p.match(TOKEN_COLON) {
			param.Type = p.parseType()
		}
		// Default value
		if p.match(TOKEN_EQUAL) {
			param.DefaultValue = p.parseExpression()
		}
		params = append(params, param)
		if !p.match(TOKEN_COMMA) {
			break
		}
	}
	return params
}

func (p *Parser) parseClassDecl() Decl {
	p.advance() // consume 'class'
	cd := &ClassDecl{pos: p.previous().Pos()}
	if p.check(TOKEN_IDENT) {
		cd.Name = p.advance().Lexeme
	}
	if p.match(TOKEN_COLON) {
		// Superclass
		if p.check(TOKEN_IDENT) {
			cd.SuperClass = p.advance().Lexeme
		}
	}
	if p.check(TOKEN_LBRACE) {
		cd.Body = p.parseBlock()
	}
	return cd
}

func (p *Parser) parseStructDecl() Decl {
	p.advance() // consume 'struct'
	sd := &StructDecl{pos: p.previous().Pos()}
	if p.check(TOKEN_IDENT) {
		sd.Name = p.advance().Lexeme
	}
	if p.check(TOKEN_LBRACE) {
		sd.Fields = p.parseStructFields()
	}
	return sd
}

func (p *Parser) parseStructFields() []*StructField {
	var fields []*StructField
	p.advance() // consume '{'
	for !p.check(TOKEN_RBRACE) && !p.isAtEnd() {
		sf := &StructField{pos: p.peek().Pos()}
		if p.check(TOKEN_IDENT) {
			sf.Name = p.advance().Lexeme
		}
		if p.match(TOKEN_COLON) {
			sf.Type = p.parseType()
		}
		fields = append(fields, sf)
		if !p.match(TOKEN_COMMA) && !p.check(TOKEN_RBRACE) {
			p.errorAt(p.peek(), "expected ',' or '}' in struct fields")
			break
		}
	}
	p.expect(TOKEN_RBRACE, "expected '}' after struct fields")
	return fields
}

func (p *Parser) parseEnumDecl() Decl {
	p.advance() // consume 'enum'
	ed := &EnumDecl{pos: p.previous().Pos()}
	if p.check(TOKEN_IDENT) {
		ed.Name = p.advance().Lexeme
	}
	if p.check(TOKEN_LBRACE) {
		ed.Variants = p.parseEnumVariants()
	}
	return ed
}

func (p *Parser) parseEnumVariants() []*EnumVariant {
	var variants []*EnumVariant
	p.advance() // consume '{'
	for !p.check(TOKEN_RBRACE) && !p.isAtEnd() {
		ev := &EnumVariant{pos: p.peek().Pos()}
		if p.check(TOKEN_IDENT) {
			ev.Name = p.advance().Lexeme
		}
		// Optional tuple payload
		if p.check(TOKEN_LPAREN) {
			p.advance()
			ev.Payload = p.parseTypeList()
			p.expect(TOKEN_RPAREN, "expected ')' after enum variant payload")
		}
		variants = append(variants, ev)
		if !p.match(TOKEN_COMMA) && !p.check(TOKEN_RBRACE) {
			p.errorAt(p.peek(), "expected ',' or '}' in enum variants")
			break
		}
	}
	p.expect(TOKEN_RBRACE, "expected '}' after enum variants")
	return variants
}

func (p *Parser) parseImportDecl() Decl {
	p.advance() // consume 'import'
	imp := &ImportDecl{pos: p.previous().Pos()}
	if p.check(TOKEN_STRING) {
		imp.Path = p.advance().Literal.(string)
	} else if p.check(TOKEN_IDENT) {
		imp.Path = p.advance().Lexeme
	}
	p.match(TOKEN_SEMICOLON) // optional semicolon
	return imp
}

func (p *Parser) parseConstDecl() Decl {
	p.advance() // consume 'const'
	cd := &ConstDecl{pos: p.previous().Pos()}
	if p.check(TOKEN_IDENT) {
		cd.Name = p.advance().Lexeme
	}
	if p.match(TOKEN_COLON) {
		cd.Type = p.parseType()
	}
	if p.match(TOKEN_EQUAL) {
		cd.Value = p.parseExpression()
	}
	p.match(TOKEN_SEMICOLON) // optional semicolon
	return cd
}

func (p *Parser) parseVarDecl() Decl {
	isLet := p.check(TOKEN_KW_LET)
	p.advance()
	vd := &VarDecl{pos: p.previous().Pos(), IsLet: isLet}
	if p.check(TOKEN_IDENT) {
		vd.Name = p.advance().Lexeme
	}
	if p.match(TOKEN_COLON) {
		vd.Type = p.parseType()
	}
	if p.match(TOKEN_EQUAL) {
		vd.Initializer = p.parseExpression()
	}
	p.match(TOKEN_SEMICOLON) // optional semicolon
	return vd
}

// ============================================================
// Statements
// ============================================================

func (p *Parser) parseStatement() Stmt {
	if p.check(TOKEN_RBRACE) || p.check(TOKEN_EOF) {
		return nil
	}
	if p.match(TOKEN_KW_IF) {
		return p.parseIfStmt()
	}
	if p.match(TOKEN_KW_WHILE) {
		return p.parseWhileStmt()
	}
	if p.match(TOKEN_KW_FOR) {
		return p.parseForStmt()
	}
	if p.match(TOKEN_KW_MATCH) {
		return p.parseMatchStmt()
	}
	if p.match(TOKEN_KW_DEFER) {
		return p.parseDeferStmt()
	}
	if p.match(TOKEN_KW_TRY) {
		return p.parseTryStmt()
	}
	if p.match(TOKEN_KW_THROW) {
		return p.parseThrowStmt()
	}
	if p.match(TOKEN_KW_RETURN) {
		return p.parseReturnStmt()
	}
	if p.match(TOKEN_KW_BREAK) {
		bs := &BreakStmt{pos: p.previous().Pos()}
		if p.check(TOKEN_IDENT) {
			bs.Label = p.advance().Lexeme
		}
		p.match(TOKEN_SEMICOLON) // optional semicolon
		return bs
	}
	if p.match(TOKEN_KW_CONTINUE) {
		cs := &ContinueStmt{pos: p.previous().Pos()}
		if p.check(TOKEN_IDENT) {
			cs.Label = p.advance().Lexeme
		}
		p.match(TOKEN_SEMICOLON) // optional semicolon
		return cs
	}
	if p.match(TOKEN_KW_LET) || p.match(TOKEN_KW_VAR) {
		isLet := p.previous().Type == TOKEN_KW_LET
		vd := &VarDecl{pos: p.previous().Pos(), IsLet: isLet}
		if p.check(TOKEN_IDENT) {
			vd.Name = p.advance().Lexeme
		}
		if p.match(TOKEN_COLON) {
			vd.Type = p.parseType()
		}
		if p.match(TOKEN_EQUAL) {
			vd.Initializer = p.parseExpression()
		}
		p.match(TOKEN_SEMICOLON) // optional semicolon
		return vd
	}
	if p.check(TOKEN_LBRACE) {
		return p.parseBlock()
	}
	// Expression statement
	expr := p.parseExpression()
	if expr == nil {
		return nil
	}
	p.match(TOKEN_SEMICOLON) // optional semicolon
	return &ExprStmt{pos: expr.Pos(), Expr: expr}
}

func (p *Parser) parseBlock() *Block {
	b := &Block{pos: p.peek().Pos()}
	p.expect(TOKEN_LBRACE, "expected '{'")
	for !p.check(TOKEN_RBRACE) && !p.isAtEnd() {
		stmt := p.parseStatement()
		if stmt != nil {
			b.Stmts = append(b.Stmts, stmt)
		} else if p.check(TOKEN_RBRACE) || p.isAtEnd() {
			break
		} else {
			// Advance to avoid infinite loop on parse failure
			p.advance()
		}
	}
	p.expect(TOKEN_RBRACE, "expected '}'")
	return b
}

func (p *Parser) parseIfStmt() Stmt {
	is := &IfStmt{pos: p.previous().Pos()}
	is.Cond = p.parseExpression()
	is.ThenBlock = p.parseBlock()
	for p.match(TOKEN_KW_ELIF) {
		eb := &ElifBlock{Cond: p.parseExpression(), Block: p.parseBlock()}
		is.ElifBlocks = append(is.ElifBlocks, eb)
	}
	if p.match(TOKEN_KW_ELSE) {
		is.ElseBlock = p.parseBlock()
	}
	return is
}

func (p *Parser) parseWhileStmt() Stmt {
	ws := &WhileStmt{pos: p.previous().Pos()}
	ws.Cond = p.parseExpression()
	ws.Body = p.parseBlock()
	return ws
}

func (p *Parser) parseForStmt() Stmt {
	fs := &ForStmt{pos: p.previous().Pos()}
	if p.check(TOKEN_IDENT) {
		fs.Variable = p.advance().Lexeme
	}
	if p.match(TOKEN_COMMA) {
		// for i, val in ...
		if p.check(TOKEN_IDENT) {
			fs.IndexVar = fs.Variable
			fs.Variable = p.advance().Lexeme
		}
	}
	p.expect(TOKEN_KW_IN, "expected 'in' in for loop")
	fs.Iterable = p.parseExpression()
	fs.Body = p.parseBlock()
	return fs
}

func (p *Parser) parseMatchStmt() Stmt {
	ms := &MatchStmt{pos: p.previous().Pos()}
	ms.Value = p.parseExpression()
	p.expect(TOKEN_LBRACE, "expected '{' after match value")
	for !p.check(TOKEN_RBRACE) && !p.isAtEnd() {
		if p.match(TOKEN_KW_CASE) {
			mc := &MatchCase{pos: p.previous().Pos()}
			mc.Pattern = p.parseExpression()
			if p.match(TOKEN_ARROW) {
				mc.Body = p.parseStatement()
			} else {
				mc.Body = p.parseBlock()
			}
			ms.Cases = append(ms.Cases, mc)
		} else if p.match(TOKEN_KW_ELSE) {
			mc := &MatchCase{pos: p.previous().Pos(), Pattern: &IdentExpr{Name: "_"}}
			if p.match(TOKEN_ARROW) {
				mc.Body = p.parseStatement()
			} else {
				mc.Body = p.parseBlock()
			}
			ms.Cases = append(ms.Cases, mc)
		} else {
			p.errorAt(p.peek(), "expected 'case' or 'else' in match")
			p.advance()
		}
	}
	p.expect(TOKEN_RBRACE, "expected '}' after match cases")
	return ms
}

func (p *Parser) parseDeferStmt() Stmt {
	ds := &DeferStmt{pos: p.previous().Pos()}
	ds.Body = p.parseBlock()
	return ds
}

func (p *Parser) parseTryStmt() Stmt {
	ts := &TryStmt{pos: p.previous().Pos()}
	ts.Body = p.parseBlock()
	if p.match(TOKEN_KW_CATCH) {
		if p.check(TOKEN_IDENT) {
			ts.CatchVar = p.advance().Lexeme
		}
		ts.CatchBody = p.parseBlock()
	}
	return ts
}

func (p *Parser) parseThrowStmt() Stmt {
	ts := &ThrowStmt{pos: p.previous().Pos()}
	ts.Expr = p.parseExpression()
	p.match(TOKEN_SEMICOLON) // optional semicolon
	return ts
}

func (p *Parser) parseReturnStmt() Stmt {
	rs := &ReturnStmt{pos: p.previous().Pos()}
	if !p.check(TOKEN_SEMICOLON) && !p.check(TOKEN_RBRACE) && !p.check(TOKEN_EOF) {
		rs.Expr = p.parseExpression()
	}
	p.match(TOKEN_SEMICOLON) // optional semicolon
	return rs
}

// ============================================================
// Expression parser (Pratt)
// ============================================================

func (p *Parser) parseExpression() Expr {
	return p.parsePrecedence(PREC_NONE)
}

func (p *Parser) parsePrecedence(prec int) Expr {
	prefix := p.prefixFn()
	if prefix == nil {
		p.errorAt(p.peek(), "expected expression")
		return nil
	}
	left := prefix()

	for prec < p.getPrecedence() {
		infix := p.infixFn()
		if infix == nil {
			return left
		}
		left = infix(left)
	}

	// Handle postfix ++ and --
	if p.check(TOKEN_PLUS_PLUS) || p.check(TOKEN_MINUS_MINUS) {
		if prec < PREC_UNARY {
			op := p.advance().Lexeme
			left = &UnaryExpr{pos: left.Pos(), Op: op, Operand: left, IsPostfix: true}
		}
	}

	return left
}

func (p *Parser) getPrecedence() int {
	switch p.peek().Type {
	case TOKEN_EQUAL, TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL, TOKEN_STAR_EQUAL,
		TOKEN_SLASH_EQUAL, TOKEN_PERCENT_EQUAL, TOKEN_AMPERSAND_EQUAL,
		TOKEN_PIPE_EQUAL, TOKEN_CARET_EQUAL, TOKEN_LESS_LESS_EQUAL,
		TOKEN_GREATER_GREATER_EQUAL:
		return PREC_ASSIGNMENT
	case TOKEN_KW_AS:
		return PREC_ASSIGNMENT + 1
	case TOKEN_DOT_DOT, TOKEN_DOT_DOT_EQUAL:
		return PREC_ASSIGNMENT + 2
	case TOKEN_KW_OR:
		return PREC_OR
	case TOKEN_PIPE_PIPE:
		return PREC_OR
	case TOKEN_KW_AND:
		return PREC_AND
	case TOKEN_AMPERSAND_AMPERSAND:
		return PREC_AND
	case TOKEN_EQUAL_EQUAL, TOKEN_EXCLAIM_EQUAL:
		return PREC_EQUALITY
	case TOKEN_LESS, TOKEN_GREATER, TOKEN_LESS_EQUAL, TOKEN_GREATER_EQUAL:
		return PREC_COMPARISON
	case TOKEN_PLUS, TOKEN_MINUS:
		return PREC_TERM
	case TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT:
		return PREC_FACTOR
	case TOKEN_LESS_LESS, TOKEN_GREATER_GREATER:
		return PREC_SHIFT
	case TOKEN_AMPERSAND:
		return PREC_BITAND
	case TOKEN_CARET:
		return PREC_BITXOR
	case TOKEN_PIPE:
		return PREC_BITOR
	case TOKEN_LPAREN, TOKEN_DOT, TOKEN_LBRACKET, TOKEN_COLON_COLON:
		return PREC_CALL
	default:
		return PREC_NONE
	}
}

func (p *Parser) prefixFn() func() Expr {
	switch p.peek().Type {
	case TOKEN_IDENT:
		return p.parseIdentPrefix
	case TOKEN_KW_BOOL, TOKEN_KW_BYTE, TOKEN_KW_CHAR, TOKEN_KW_STRING, TOKEN_KW_VOID,
		TOKEN_KW_INT, TOKEN_KW_FLOAT, TOKEN_KW_DOUBLE,
		TOKEN_KW_U8, TOKEN_KW_U16, TOKEN_KW_U32, TOKEN_KW_U64, TOKEN_KW_U128,
		TOKEN_KW_I8, TOKEN_KW_I16, TOKEN_KW_I32, TOKEN_KW_I64:
		return p.parseTypeKeywordPrefix
	case TOKEN_INTEGER, TOKEN_FLOAT, TOKEN_STRING, TOKEN_CHAR, TOKEN_KW_TRUE, TOKEN_KW_FALSE, TOKEN_KW_NONE:
		return p.parseLiteral
	case TOKEN_LPAREN:
		return p.parseGroupingOrTuple
	case TOKEN_LBRACKET:
		return p.parseArrayLiteral
	case TOKEN_MINUS, TOKEN_EXCLAIM, TOKEN_TILDE:
		return p.parseUnaryPrefix
	case TOKEN_PLUS_PLUS, TOKEN_MINUS_MINUS:
		return p.parsePrefixIncDec
	case TOKEN_HASH:
		return p.parseLengthPrefix
	case TOKEN_KW_IF:
		return p.parseIfExpr
	case TOKEN_KW_MATCH:
		return p.parseMatchExpr
	case TOKEN_KW_SELF:
		return p.parseSelfPrefix
	case TOKEN_KW_COPY:
		return p.parseCopyPrefix
	case TOKEN_KW_HEAP:
		return p.parseHeapPrefix
	case TOKEN_KW_NOT:
		return p.parseNotPrefix
	default:
		return nil
	}
}

func (p *Parser) parseNotPrefix() Expr {
	tok := p.advance()
	operand := p.parsePrecedence(PREC_UNARY)
	return &UnaryExpr{pos: tok.Pos(), Op: "!", Operand: operand}
}

func (p *Parser) infixFn() func(Expr) Expr {
	switch p.peek().Type {
	case TOKEN_LPAREN:
		return p.parseCallInfix
	case TOKEN_DOT:
		return p.parseMemberInfix
	case TOKEN_LBRACKET:
		return p.parseIndexInfix
	case TOKEN_COLON_COLON:
		return p.parseScopeInfix
	case TOKEN_EQUAL, TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL, TOKEN_STAR_EQUAL,
		TOKEN_SLASH_EQUAL, TOKEN_PERCENT_EQUAL, TOKEN_AMPERSAND_EQUAL,
		TOKEN_PIPE_EQUAL, TOKEN_CARET_EQUAL, TOKEN_LESS_LESS_EQUAL,
		TOKEN_GREATER_GREATER_EQUAL:
		return p.parseAssignmentInfix
	case TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
		TOKEN_EQUAL_EQUAL, TOKEN_EXCLAIM_EQUAL, TOKEN_LESS, TOKEN_GREATER,
		TOKEN_LESS_EQUAL, TOKEN_GREATER_EQUAL, TOKEN_AMPERSAND, TOKEN_PIPE,
		TOKEN_CARET, TOKEN_LESS_LESS, TOKEN_GREATER_GREATER,
		TOKEN_AMPERSAND_AMPERSAND, TOKEN_PIPE_PIPE,
		TOKEN_DOT_DOT, TOKEN_DOT_DOT_EQUAL:
		return p.parseBinaryInfix
	case TOKEN_KW_AND:
		return p.parseAndInfix
	case TOKEN_KW_OR:
		return p.parseOrInfix
	case TOKEN_KW_AS:
		return p.parseAsInfix
	case TOKEN_KW_IN:
		return p.parseInInfix
	default:
		return nil
	}
}

// Prefix parse functions

func (p *Parser) parseIdentPrefix() Expr {
	tok := p.advance()
	return &IdentExpr{pos: tok.Pos(), Name: tok.Lexeme}
}

func (p *Parser) parseTypeKeywordPrefix() Expr {
	tok := p.advance()
	return &IdentExpr{pos: tok.Pos(), Name: tok.Lexeme}
}

func (p *Parser) parseLiteral() Expr {
	tok := p.advance()
	lit := &LiteralExpr{pos: tok.Pos()}
	switch tok.Type {
	case TOKEN_INTEGER:
		val, err := strconv.ParseInt(tok.Lexeme, 0, 64)
		if err != nil {
			// Try hex/octal/binary
			cleaned := strings.ReplaceAll(tok.Lexeme, "_", "")
			val, err = strconv.ParseInt(cleaned, 0, 64)
			if err != nil {
				p.errorAt(tok, fmt.Sprintf("invalid integer: %s", tok.Lexeme))
				val = 0
			}
		}
		lit.Value = val
	case TOKEN_FLOAT:
		val, err := strconv.ParseFloat(tok.Lexeme, 64)
		if err != nil {
			p.errorAt(tok, fmt.Sprintf("invalid float: %s", tok.Lexeme))
			val = 0.0
		}
		lit.Value = val
	case TOKEN_STRING:
		lit.Value = tok.Lexeme
	case TOKEN_CHAR:
		lit.Value = tok.Lexeme
	case TOKEN_KW_TRUE:
		lit.Value = true
	case TOKEN_KW_FALSE:
		lit.Value = false
	case TOKEN_KW_NONE:
		lit.Value = nil
	}
	return lit
}

func (p *Parser) parseGroupingOrTuple() Expr {
	p.advance() // consume '('
	if p.check(TOKEN_RPAREN) {
		p.advance()
		return &LiteralExpr{pos: p.previous().Pos(), Value: nil}
	}
	expr := p.parseExpression()
	if p.check(TOKEN_COMMA) {
		// Tuple
		elems := []Expr{expr}
		for p.match(TOKEN_COMMA) {
			elems = append(elems, p.parseExpression())
		}
		p.expect(TOKEN_RPAREN, "expected ')' after tuple")
		return &TupleExpr{pos: expr.Pos(), Elements: elems}
	}
	p.expect(TOKEN_RPAREN, "expected ')' after expression")
	return expr
}

func (p *Parser) parseArrayLiteral() Expr {
	p.advance() // consume '['
	al := &ArrayLiteralExpr{pos: p.previous().Pos()}
	if !p.check(TOKEN_RBRACKET) {
		al.Elements = append(al.Elements, p.parseExpression())
		for p.match(TOKEN_COMMA) {
			al.Elements = append(al.Elements, p.parseExpression())
		}
	}
	p.expect(TOKEN_RBRACKET, "expected ']' after array literal")
	return al
}

func (p *Parser) parseUnaryPrefix() Expr {
	tok := p.advance()
	op := tok.Lexeme
	operand := p.parsePrecedence(PREC_UNARY)
	return &UnaryExpr{pos: tok.Pos(), Op: op, Operand: operand}
}

func (p *Parser) parsePrefixIncDec() Expr {
	tok := p.advance()
	op := tok.Lexeme
	operand := p.parsePrecedence(PREC_UNARY)
	return &UnaryExpr{pos: tok.Pos(), Op: op, Operand: operand, IsPostfix: false}
}

func (p *Parser) parseLengthPrefix() Expr {
	tok := p.advance()
	operand := p.parsePrecedence(PREC_UNARY)
	return &UnaryExpr{pos: tok.Pos(), Op: "#", Operand: operand}
}

func (p *Parser) parseIfExpr() Expr {
	p.advance() // consume 'if'
	ie := &IfExpr{pos: p.previous().Pos()}
	ie.Cond = p.parseExpression()
	ie.ThenExpr = p.parseBlock()
	for p.match(TOKEN_KW_ELIF) {
		ee := &ElifBlock{Cond: p.parseExpression(), Block: p.parseBlock()}
		ie.ElifBlocks = append(ie.ElifBlocks, ee)
	}
	if p.match(TOKEN_KW_ELSE) {
		ie.ElseExpr = p.parseBlock()
	}
	return ie
}

func (p *Parser) parseMatchExpr() Expr {
	p.advance() // consume 'match'
	me := &MatchExpr{pos: p.previous().Pos()}
	me.Value = p.parseExpression()
	p.expect(TOKEN_LBRACE, "expected '{' after match value")
	for !p.check(TOKEN_RBRACE) && !p.isAtEnd() {
		if p.match(TOKEN_KW_CASE) {
			mc := &MatchCase{pos: p.previous().Pos()}
			mc.Pattern = p.parseExpression()
			if p.match(TOKEN_ARROW) {
				mc.Body = p.parseStatement()
			} else {
				mc.Body = p.parseBlock()
			}
			me.Cases = append(me.Cases, mc)
		} else if p.match(TOKEN_KW_ELSE) {
			mc := &MatchCase{pos: p.previous().Pos(), Pattern: &IdentExpr{Name: "_"}}
			if p.match(TOKEN_ARROW) {
				mc.Body = p.parseStatement()
			} else {
				mc.Body = p.parseBlock()
			}
			me.Cases = append(me.Cases, mc)
		} else {
			p.errorAt(p.peek(), "expected 'case' or 'else' in match")
			p.advance()
		}
	}
	p.expect(TOKEN_RBRACE, "expected '}' after match cases")
	return me
}

func (p *Parser) parseSelfPrefix() Expr {
	tok := p.advance()
	return &IdentExpr{pos: tok.Pos(), Name: "self"}
}

func (p *Parser) parseCopyPrefix() Expr {
	tok := p.advance()
	operand := p.parsePrecedence(PREC_UNARY)
	return &UnaryExpr{pos: tok.Pos(), Op: "copy", Operand: operand}
}

func (p *Parser) parseHeapPrefix() Expr {
	tok := p.advance()
	operand := p.parsePrecedence(PREC_UNARY)
	return &UnaryExpr{pos: tok.Pos(), Op: "heap", Operand: operand}
}

// Infix parse functions

func (p *Parser) parseCallInfix(left Expr) Expr {
	ce := &CallExpr{pos: left.Pos(), Callee: left}
	p.advance() // consume '('
	if !p.check(TOKEN_RPAREN) {
		ce.Args = append(ce.Args, p.parseExpression())
		for p.match(TOKEN_COMMA) {
			ce.Args = append(ce.Args, p.parseExpression())
		}
	}
	p.expect(TOKEN_RPAREN, "expected ')' after arguments")
	return ce
}

func (p *Parser) parseMemberInfix(left Expr) Expr {
	p.advance() // consume '.'
	me := &MemberExpr{pos: left.Pos(), Object: left}
	if p.check(TOKEN_IDENT) {
		me.Member = p.advance().Lexeme
	} else {
		p.errorAt(p.peek(), "expected member name after '.'")
	}
	return me
}

func (p *Parser) parseIndexInfix(left Expr) Expr {
	p.advance() // consume '['
	ie := &IndexExpr{pos: left.Pos(), Object: left}
	ie.Index = p.parseExpression()
	p.expect(TOKEN_RBRACKET, "expected ']' after index")
	return ie
}

func (p *Parser) parseScopeInfix(left Expr) Expr {
	p.advance() // consume '::'
	se := &MemberExpr{pos: left.Pos(), Object: left}
	if p.check(TOKEN_IDENT) {
		se.Member = p.advance().Lexeme
	} else {
		p.errorAt(p.peek(), "expected name after '::'")
	}
	return se
}

func (p *Parser) parseAssignmentInfix(left Expr) Expr {
	tok := p.advance()
	ae := &AssignmentExpr{pos: left.Pos(), Target: left, Op: tok.Lexeme}
	ae.Value = p.parsePrecedence(PREC_ASSIGNMENT)
	return ae
}

func (p *Parser) parseBinaryInfix(left Expr) Expr {
	tok := p.advance()
	prec := p.getPrecedence()
	be := &BinaryExpr{pos: left.Pos(), Op: tok.Lexeme, Left: left}
	be.Right = p.parsePrecedence(prec)
	return be
}

func (p *Parser) parseAndInfix(left Expr) Expr {
	p.advance()
	be := &BinaryExpr{pos: left.Pos(), Op: "&&", Left: left}
	be.Right = p.parsePrecedence(PREC_AND)
	return be
}

func (p *Parser) parseOrInfix(left Expr) Expr {
	p.advance()
	be := &BinaryExpr{pos: left.Pos(), Op: "||", Left: left}
	be.Right = p.parsePrecedence(PREC_OR)
	return be
}

func (p *Parser) parseAsInfix(left Expr) Expr {
	p.advance()
	be := &BinaryExpr{pos: left.Pos(), Op: "as", Left: left}
	be.Right = p.parsePrecedence(PREC_ASSIGNMENT)
	return be
}

func (p *Parser) parseInInfix(left Expr) Expr {
	p.advance()
	be := &BinaryExpr{pos: left.Pos(), Op: "in", Left: left}
	be.Right = p.parsePrecedence(PREC_COMPARISON)
	return be
}

// ============================================================
// Type parsing
// ============================================================

func (p *Parser) parseType() *TypeAnnotation {
	ta := &TypeAnnotation{pos: p.peek().Pos()}
	if p.check(TOKEN_DOT_DOT_DOT) {
		// Variadic type: ...type
		p.advance()
		ta.Name = "..."
		if p.check(TOKEN_IDENT) || p.check(TOKEN_KW_BOOL) || p.check(TOKEN_KW_BYTE) ||
			p.check(TOKEN_KW_CHAR) || p.check(TOKEN_KW_STRING) || p.check(TOKEN_KW_VOID) ||
			p.check(TOKEN_KW_INT) || p.check(TOKEN_KW_FLOAT) || p.check(TOKEN_KW_DOUBLE) ||
			p.check(TOKEN_KW_U8) || p.check(TOKEN_KW_U16) || p.check(TOKEN_KW_U32) ||
			p.check(TOKEN_KW_U64) || p.check(TOKEN_KW_U128) ||
			p.check(TOKEN_KW_I8) || p.check(TOKEN_KW_I16) || p.check(TOKEN_KW_I32) || p.check(TOKEN_KW_I64) {
			ta.Name += p.advance().Lexeme
		}
		return ta
	}
	if p.check(TOKEN_IDENT) || p.check(TOKEN_KW_BOOL) || p.check(TOKEN_KW_BYTE) ||
		p.check(TOKEN_KW_CHAR) || p.check(TOKEN_KW_STRING) || p.check(TOKEN_KW_VOID) ||
		p.check(TOKEN_KW_INT) || p.check(TOKEN_KW_FLOAT) || p.check(TOKEN_KW_DOUBLE) ||
		p.check(TOKEN_KW_U8) || p.check(TOKEN_KW_U16) || p.check(TOKEN_KW_U32) ||
		p.check(TOKEN_KW_U64) || p.check(TOKEN_KW_U128) ||
		p.check(TOKEN_KW_I8) || p.check(TOKEN_KW_I16) || p.check(TOKEN_KW_I32) || p.check(TOKEN_KW_I64) {
		ta.Name = p.advance().Lexeme
	} else if p.check(TOKEN_LBRACKET) {
		// Array type: [type] or [type; size]
		p.advance()
		ta.Name = "[" + p.parseType().Name
		if p.match(TOKEN_SEMICOLON) {
			size := p.parseExpression()
			ta.Name += "; " + fmt.Sprintf("%v", size)
		}
		p.expect(TOKEN_RBRACKET, "expected ']' in array type")
		ta.Name += "]"
	} else if p.check(TOKEN_LPAREN) {
		// Tuple type: (type1, type2)
		p.advance()
		ta.Name = "("
		ta.Name += p.parseType().Name
		for p.match(TOKEN_COMMA) {
			ta.Name += ", " + p.parseType().Name
		}
		p.expect(TOKEN_RPAREN, "expected ')' in tuple type")
		ta.Name += ")"
	} else if p.check(TOKEN_KW_FUNC) {
		// Function type: func(param: type): returnType
		p.advance()
		ta.Name = "func("
		p.expect(TOKEN_LPAREN, "expected '(' in function type")
		if !p.check(TOKEN_RPAREN) {
			ta.Name += p.parseType().Name
			for p.match(TOKEN_COMMA) {
				ta.Name += ", " + p.parseType().Name
			}
		}
		p.expect(TOKEN_RPAREN, "expected ')' in function type")
		ta.Name += ")"
		if p.match(TOKEN_COLON) {
			ta.Name += ": " + p.parseType().Name
		}
	} else {
		p.errorAt(p.peek(), "expected type")
		ta.Name = "unknown"
	}
	// Optional type: type?
	if p.match(TOKEN_QUESTION) {
		ta.Name += "?"
	}
	return ta
}

func (p *Parser) parseTypeList() []*TypeAnnotation {
	var types []*TypeAnnotation
	types = append(types, p.parseType())
	for p.match(TOKEN_COMMA) {
		types = append(types, p.parseType())
	}
	return types
}

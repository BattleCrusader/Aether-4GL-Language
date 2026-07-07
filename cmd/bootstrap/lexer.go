package main

import (
	"fmt"
	"strings"
	"unicode"
)

// Token types
type TokenType int

const (
	// Single-character tokens
	TOKEN_LPAREN TokenType = iota
	TOKEN_RPAREN
	TOKEN_LBRACE
	TOKEN_RBRACE
	TOKEN_LBRACKET
	TOKEN_RBRACKET
	TOKEN_COMMA
	TOKEN_SEMICOLON
	TOKEN_COLON
	TOKEN_DOT
	TOKEN_QUESTION

	// Operators
	TOKEN_PLUS
	TOKEN_MINUS
	TOKEN_STAR
	TOKEN_SLASH
	TOKEN_PERCENT
	TOKEN_AMPERSAND
	TOKEN_PIPE
	TOKEN_CARET
	TOKEN_TILDE
	TOKEN_EXCLAIM
	TOKEN_LESS
	TOKEN_GREATER
	TOKEN_EQUAL
	TOKEN_AMPERSAND_AMPERSAND
	TOKEN_PIPE_PIPE
	TOKEN_EQUAL_EQUAL
	TOKEN_EXCLAIM_EQUAL
	TOKEN_LESS_EQUAL
	TOKEN_GREATER_EQUAL
	TOKEN_LESS_LESS
	TOKEN_GREATER_GREATER
	TOKEN_PLUS_PLUS
	TOKEN_PLUS_EQUAL
	TOKEN_MINUS_MINUS
	TOKEN_MINUS_EQUAL
	TOKEN_STAR_EQUAL
	TOKEN_SLASH_EQUAL
	TOKEN_PERCENT_EQUAL
	TOKEN_AMPERSAND_EQUAL
	TOKEN_PIPE_EQUAL
	TOKEN_CARET_EQUAL
	TOKEN_LESS_LESS_EQUAL
	TOKEN_GREATER_GREATER_EQUAL
	TOKEN_DOT_DOT
	TOKEN_DOT_DOT_EQUAL
	TOKEN_DOT_DOT_DOT
	TOKEN_ARROW
	TOKEN_COLON_COLON
	TOKEN_HASH

	// Literals
	TOKEN_INTEGER
	TOKEN_FLOAT
	TOKEN_STRING
	TOKEN_CHAR
	TOKEN_IDENT

	// Keywords
	TOKEN_KW_AND
	TOKEN_KW_AS
	TOKEN_KW_BOOL
	TOKEN_KW_BREAK
	TOKEN_KW_BYTE
	TOKEN_KW_CATCH
	TOKEN_KW_CHAR
	TOKEN_KW_CLASS
	TOKEN_KW_CONST
	TOKEN_KW_CONTINUE
	TOKEN_KW_COPY
	TOKEN_KW_DEFER
	TOKEN_KW_DOUBLE
	TOKEN_KW_DYN
	TOKEN_KW_ELIF
	TOKEN_KW_ELSE
	TOKEN_KW_ENUM
	TOKEN_KW_EXPORT
	TOKEN_KW_EXTERN
	TOKEN_KW_FALSE
	TOKEN_KW_FLOAT
	TOKEN_KW_FOR
	TOKEN_KW_FUNC
	TOKEN_KW_HEAP
	TOKEN_KW_I8
	TOKEN_KW_I16
	TOKEN_KW_I32
	TOKEN_KW_I64
	TOKEN_KW_IF
	TOKEN_KW_IMPL
	TOKEN_KW_IMPORT
	TOKEN_KW_IN
	TOKEN_KW_INIT
	TOKEN_KW_INT
	TOKEN_KW_INTERNAL
	TOKEN_KW_LET
	TOKEN_KW_MODULE
	TOKEN_KW_NONE
	TOKEN_KW_NOT
	TOKEN_KW_OR
	TOKEN_KW_POOL
	TOKEN_KW_POST
	TOKEN_KW_PRE
	TOKEN_KW_PRIVATE
	TOKEN_KW_PROTOCOL
	TOKEN_KW_PUBLIC
	TOKEN_KW_REGION
	TOKEN_KW_RETURN
	TOKEN_KW_SELF
	TOKEN_KW_STATIC
	TOKEN_KW_STRING
	TOKEN_KW_STRUCT
	TOKEN_KW_SUPER
	TOKEN_KW_SYS
	TOKEN_KW_THROW
	TOKEN_KW_TRAIT
	TOKEN_KW_TRUE
	TOKEN_KW_TRY
	TOKEN_KW_TYPE
	TOKEN_KW_U8
	TOKEN_KW_U16
	TOKEN_KW_U32
	TOKEN_KW_U64
	TOKEN_KW_U128
	TOKEN_KW_UNSAFE
	TOKEN_KW_VAR
	TOKEN_KW_VOID
	TOKEN_KW_WHILE
	TOKEN_KW_YIELD
	TOKEN_KW_MATCH
	TOKEN_KW_CASE
	TOKEN_AT

	// Special
	TOKEN_EOF
	TOKEN_ERROR
)

// Keyword map
var keywords = map[string]TokenType{
	"and":      TOKEN_KW_AND,
	"as":       TOKEN_KW_AS,
	"bool":     TOKEN_KW_BOOL,
	"break":    TOKEN_KW_BREAK,
	"byte":     TOKEN_KW_BYTE,
	"catch":    TOKEN_KW_CATCH,
	"char":     TOKEN_KW_CHAR,
	"class":    TOKEN_KW_CLASS,
	"const":    TOKEN_KW_CONST,
	"continue": TOKEN_KW_CONTINUE,
	"copy":     TOKEN_KW_COPY,
	"defer":    TOKEN_KW_DEFER,
	"double":   TOKEN_KW_DOUBLE,
	"dyn":      TOKEN_KW_DYN,
	"elif":     TOKEN_KW_ELIF,
	"else":     TOKEN_KW_ELSE,
	"enum":     TOKEN_KW_ENUM,
	"export":   TOKEN_KW_EXPORT,
	"extern":   TOKEN_KW_EXTERN,
	"false":    TOKEN_KW_FALSE,
	"float":    TOKEN_KW_FLOAT,
	"for":      TOKEN_KW_FOR,
	"func":     TOKEN_KW_FUNC,
	"heap":     TOKEN_KW_HEAP,
	"i8":       TOKEN_KW_I8,
	"i16":      TOKEN_KW_I16,
	"i32":      TOKEN_KW_I32,
	"i64":      TOKEN_KW_I64,
	"if":       TOKEN_KW_IF,
	"impl":     TOKEN_KW_IMPL,
	"import":   TOKEN_KW_IMPORT,
	"in":       TOKEN_KW_IN,
	"init":     TOKEN_KW_INIT,
	"int":      TOKEN_KW_INT,
	"internal": TOKEN_KW_INTERNAL,
	"let":      TOKEN_KW_LET,
	"module":   TOKEN_KW_MODULE,
	"none":     TOKEN_KW_NONE,
	"not":      TOKEN_KW_NOT,
	"or":       TOKEN_KW_OR,
	"pool":     TOKEN_KW_POOL,
	"post":     TOKEN_KW_POST,
	"pre":      TOKEN_KW_PRE,
	"private":  TOKEN_KW_PRIVATE,
	"protocol": TOKEN_KW_PROTOCOL,
	"public":   TOKEN_KW_PUBLIC,
	"region":   TOKEN_KW_REGION,
	"return":   TOKEN_KW_RETURN,
	"self":     TOKEN_KW_SELF,
	"static":   TOKEN_KW_STATIC,
	"string":   TOKEN_KW_STRING,
	"struct":   TOKEN_KW_STRUCT,
	"super":    TOKEN_KW_SUPER,
	"sys":      TOKEN_KW_SYS,
	"throw":    TOKEN_KW_THROW,
	"trait":    TOKEN_KW_TRAIT,
	"true":     TOKEN_KW_TRUE,
	"try":      TOKEN_KW_TRY,
	"type":     TOKEN_KW_TYPE,
	"u8":       TOKEN_KW_U8,
	"u16":      TOKEN_KW_U16,
	"u32":      TOKEN_KW_U32,
	"u64":      TOKEN_KW_U64,
	"u128":     TOKEN_KW_U128,
	"unsafe":   TOKEN_KW_UNSAFE,
	"var":      TOKEN_KW_VAR,
	"void":     TOKEN_KW_VOID,
	"while":    TOKEN_KW_WHILE,
	"yield":    TOKEN_KW_YIELD,
	"match":    TOKEN_KW_MATCH,
	"case":     TOKEN_KW_CASE,
}

// Token represents a lexical token
type Token struct {
	Type    TokenType
	Lexeme  string
	Line    int
	Column  int
	Literal interface{}
}

func (t Token) Pos() Pos {
	return Pos{Line: t.Line, Col: t.Column}
}

// tokenName returns a human-readable name for a token type
func tokenName(t TokenType) string {
	names := map[TokenType]string{
		TOKEN_LPAREN:                   "LPAREN",
		TOKEN_RPAREN:                   "RPAREN",
		TOKEN_LBRACE:                   "LBRACE",
		TOKEN_RBRACE:                   "RBRACE",
		TOKEN_LBRACKET:                 "LBRACKET",
		TOKEN_RBRACKET:                 "RBRACKET",
		TOKEN_COMMA:                    "COMMA",
		TOKEN_SEMICOLON:                "SEMICOLON",
		TOKEN_COLON:                    "COLON",
		TOKEN_DOT:                      "DOT",
		TOKEN_QUESTION:                 "QUESTION",
		TOKEN_PLUS:                     "PLUS",
		TOKEN_MINUS:                    "MINUS",
		TOKEN_STAR:                     "STAR",
		TOKEN_SLASH:                    "SLASH",
		TOKEN_PERCENT:                  "PERCENT",
		TOKEN_AMPERSAND:                "AMPERSAND",
		TOKEN_PIPE:                     "PIPE",
		TOKEN_CARET:                    "CARET",
		TOKEN_TILDE:                    "TILDE",
		TOKEN_EXCLAIM:                  "EXCLAIM",
		TOKEN_LESS:                     "LESS",
		TOKEN_GREATER:                  "GREATER",
		TOKEN_EQUAL:                    "EQUAL",
		TOKEN_AMPERSAND_AMPERSAND:      "AMPERSAND_AMPERSAND",
		TOKEN_PIPE_PIPE:                "PIPE_PIPE",
		TOKEN_EQUAL_EQUAL:              "EQUAL_EQUAL",
		TOKEN_EXCLAIM_EQUAL:            "EXCLAIM_EQUAL",
		TOKEN_LESS_EQUAL:               "LESS_EQUAL",
		TOKEN_GREATER_EQUAL:            "GREATER_EQUAL",
		TOKEN_LESS_LESS:                "LESS_LESS",
		TOKEN_GREATER_GREATER:          "GREATER_GREATER",
		TOKEN_PLUS_EQUAL:               "PLUS_EQUAL",
		TOKEN_MINUS_EQUAL:              "MINUS_EQUAL",
		TOKEN_STAR_EQUAL:               "STAR_EQUAL",
		TOKEN_SLASH_EQUAL:              "SLASH_EQUAL",
		TOKEN_PERCENT_EQUAL:            "PERCENT_EQUAL",
		TOKEN_AMPERSAND_EQUAL:          "AMPERSAND_EQUAL",
		TOKEN_PIPE_EQUAL:               "PIPE_EQUAL",
		TOKEN_CARET_EQUAL:              "CARET_EQUAL",
		TOKEN_LESS_LESS_EQUAL:          "LESS_LESS_EQUAL",
		TOKEN_GREATER_GREATER_EQUAL:    "GREATER_GREATER_EQUAL",
		TOKEN_DOT_DOT:                  "DOT_DOT",
		TOKEN_DOT_DOT_EQUAL:            "DOT_DOT_EQUAL",
		TOKEN_ARROW:                    "ARROW",
		TOKEN_COLON_COLON:              "COLON_COLON",
		TOKEN_HASH:                     "HASH",
		TOKEN_INTEGER:                  "INTEGER",
		TOKEN_FLOAT:                    "FLOAT",
		TOKEN_STRING:                   "STRING",
		TOKEN_CHAR:                     "CHAR",
		TOKEN_IDENT:                    "IDENT",
		TOKEN_EOF:                      "EOF",
		TOKEN_ERROR:                     "ERROR",
	}
	if name, ok := names[t]; ok {
		return name
	}
	if t >= TOKEN_KW_AND && t <= TOKEN_KW_YIELD {
		return "KEYWORD"
	}
	return "UNKNOWN"
}

// String returns a string representation of the token
func (t Token) String() string {
	lit := ""
	if t.Literal != nil {
		lit = fmt.Sprintf(" => %v", t.Literal)
	}
	return fmt.Sprintf("%3d:%-3d %-20s %q%s", t.Line, t.Column, tokenName(t.Type), t.Lexeme, lit)
}

// Lexer tokenizes Aether source code
type Lexer struct {
	source  string
	srcPath string
	start   int
	current int
	line    int
	column  int
	tokens  []Token
	err     error
}

// New creates a new Lexer for the given source and source path
func New(source, srcPath string) *Lexer {
	return &Lexer{
		source:  source,
		srcPath: srcPath,
		start:   0,
		current: 0,
		line:    1,
		column:  1,
		tokens:  []Token{},
	}
}

// Tokenize lexes the entire source and returns all tokens
func (l *Lexer) Tokenize() ([]Token, error) {
	for !l.isAtEnd() && l.err == nil {
		l.start = l.current
		l.scanToken()
	}

	if l.err != nil {
		return l.tokens, l.err
	}

	l.tokens = append(l.tokens, Token{
		Type:   TOKEN_EOF,
		Lexeme: "",
		Line:   l.line,
		Column: l.column,
	})
	return l.tokens, nil
}

// scanToken lexes a single token
func (l *Lexer) scanToken() {
	c := l.advance()

	switch c {
	case '(':
		l.addToken(TOKEN_LPAREN)
	case ')':
		l.addToken(TOKEN_RPAREN)
	case '{':
		l.addToken(TOKEN_LBRACE)
	case '}':
		l.addToken(TOKEN_RBRACE)
	case '[':
		l.addToken(TOKEN_LBRACKET)
	case ']':
		l.addToken(TOKEN_RBRACKET)
	case ',':
		l.addToken(TOKEN_COMMA)
	case ';':
		l.addToken(TOKEN_SEMICOLON)
	case ':':
		if l.match('=') {
			l.addToken(TOKEN_COLON_COLON)
		} else {
			l.addToken(TOKEN_COLON)
		}
	case '.':
		if l.match('.') {
			if l.match('.') {
				l.addToken(TOKEN_DOT_DOT_DOT)
			} else if l.match('=') {
				l.addToken(TOKEN_DOT_DOT_EQUAL)
			} else {
				l.addToken(TOKEN_DOT_DOT)
			}
		} else {
			l.addToken(TOKEN_DOT)
		}
	case '?':
		l.addToken(TOKEN_QUESTION)

	case '+':
		if l.match('+') {
			l.addToken(TOKEN_PLUS_PLUS)
		} else if l.match('=') {
			l.addToken(TOKEN_PLUS_EQUAL)
		} else {
			l.addToken(TOKEN_PLUS)
		}
	case '-':
		if l.match('-') {
			l.addToken(TOKEN_MINUS_MINUS)
		} else if l.match('=') {
			l.addToken(TOKEN_MINUS_EQUAL)
		} else if l.match('>') {
			l.addToken(TOKEN_ARROW)
		} else {
			l.addToken(TOKEN_MINUS)
		}
	case '*':
		if l.match('=') {
			l.addToken(TOKEN_STAR_EQUAL)
		} else {
			l.addToken(TOKEN_STAR)
		}
	case '/':
		if l.match('=') {
			l.addToken(TOKEN_SLASH_EQUAL)
		} else if l.match('/') {
			// Line comment
			for l.peek() != '\n' && !l.isAtEnd() {
				l.advance()
			}
		} else if l.match('*') {
			// Block comment (possibly nested)
			l.nestedComment()
		} else {
			l.addToken(TOKEN_SLASH)
		}
	case '%':
		if l.match('=') {
			l.addToken(TOKEN_PERCENT_EQUAL)
		} else {
			l.addToken(TOKEN_PERCENT)
		}
	case '&':
		if l.match('=') {
			l.addToken(TOKEN_AMPERSAND_EQUAL)
		} else if l.match('&') {
			l.addToken(TOKEN_AMPERSAND_AMPERSAND)
		} else {
			l.addToken(TOKEN_AMPERSAND)
		}
	case '|':
		if l.match('=') {
			l.addToken(TOKEN_PIPE_EQUAL)
		} else if l.match('|') {
			l.addToken(TOKEN_PIPE_PIPE)
		} else {
			l.addToken(TOKEN_PIPE)
		}
	case '^':
		if l.match('=') {
			l.addToken(TOKEN_CARET_EQUAL)
		} else {
			l.addToken(TOKEN_CARET)
		}
	case '~':
		l.addToken(TOKEN_TILDE)
	case '!':
		if l.match('=') {
			l.addToken(TOKEN_EXCLAIM_EQUAL)
		} else {
			l.addToken(TOKEN_EXCLAIM)
		}
	case '<':
		if l.match('=') {
			l.addToken(TOKEN_LESS_EQUAL)
		} else if l.match('<') {
			if l.match('=') {
				l.addToken(TOKEN_LESS_LESS_EQUAL)
			} else {
				l.addToken(TOKEN_LESS_LESS)
			}
		} else {
			l.addToken(TOKEN_LESS)
		}
	case '>':
		if l.match('=') {
			l.addToken(TOKEN_GREATER_EQUAL)
		} else if l.match('>') {
			if l.match('=') {
				l.addToken(TOKEN_GREATER_GREATER_EQUAL)
			} else {
				l.addToken(TOKEN_GREATER_GREATER)
			}
		} else {
			l.addToken(TOKEN_GREATER)
		}
	case '=':
		if l.match('=') {
			l.addToken(TOKEN_EQUAL_EQUAL)
		} else {
			l.addToken(TOKEN_EQUAL)
		}
	case '#':
		l.addToken(TOKEN_HASH)

	case '@':
		l.addToken(TOKEN_AT)

	case ' ', '\r', '\t':
		// Ignore whitespace
	case '\n':
		l.line++
		l.column = 1

	case '"':
		l.string()

	case '\'':
		l.char()

	default:
		if unicode.IsDigit(rune(c)) {
			l.number()
		} else if unicode.IsLetter(rune(c)) || c == '_' {
			l.identifier()
		} else {
			l.err = fmt.Errorf("unexpected character: %c at line %d, column %d", c, l.line, l.column)
		}
	}
}

// nestedComment handles nested /* */ block comments
func (l *Lexer) nestedComment() {
	depth := 1
	for depth > 0 && !l.isAtEnd() {
		if l.peek() == '/' && l.peekNext() == '*' {
			l.advance()
			l.advance()
			depth++
		} else if l.peek() == '*' && l.peekNext() == '/' {
			l.advance()
			l.advance()
			depth--
		} else {
			if l.peek() == '\n' {
				l.line++
				l.column = 0
			}
			l.advance()
		}
	}
	if depth > 0 {
		l.err = fmt.Errorf("unterminated block comment at line %d", l.line)
	}
}

// string lexes a string literal
func (l *Lexer) string() {
	startLine := l.line

	for l.peek() != '"' && !l.isAtEnd() {
		if l.peek() == '\n' {
			l.line++
			l.column = 0
		}
		if l.peek() == '\\' && l.peekNext() == '"' {
			l.advance() // consume backslash
			l.advance() // consume quote
		} else if l.peek() == '\\' && l.peekNext() == '{' {
			// String interpolation: \{
			l.advance() // consume backslash
			l.advance() // consume {
		} else {
			l.advance()
		}
	}

	if l.isAtEnd() {
		l.err = fmt.Errorf("unterminated string at line %d", startLine)
		return
	}

	// The closing quote
	l.advance()

	// Get the lexeme (we started at the opening quote)
	lexeme := l.source[l.start:l.current]
	l.addTokenWithLiteral(TOKEN_STRING, lexeme[1:len(lexeme)-1]) // Strip quotes
}

// char lexes a character literal
func (l *Lexer) char() {
	startLine := l.line

	// Skip characters until closing quote
	if l.peek() == '\\' {
		l.advance() // backslash
		l.advance() // escape char
	} else {
		l.advance()
	}

	if l.peek() != '\'' {
		l.err = fmt.Errorf("unterminated char literal at line %d", startLine)
		return
	}
	l.advance() // closing quote

	lexeme := l.source[l.start:l.current]
	l.addTokenWithLiteral(TOKEN_CHAR, lexeme)
}

// number lexes an integer or float literal
func (l *Lexer) number() {
	// Read integer part
	for unicode.IsDigit(rune(l.peek())) {
		l.advance()
	}

	// Check for float (but not range operator ..)
	if l.peek() == '.' && l.peekNext() != '.' && unicode.IsDigit(rune(l.peekNext())) {
		l.advance() // consume '.'
		for unicode.IsDigit(rune(l.peek())) {
			l.advance()
		}
		// Check for float exponent
		if l.peek() == 'e' || l.peek() == 'E' {
			l.advance()
			if l.peek() == '+' || l.peek() == '-' {
				l.advance()
			}
			for unicode.IsDigit(rune(l.peek())) {
				l.advance()
			}
		}
		lexeme := l.source[l.start:l.current]
		l.addTokenWithLiteral(TOKEN_FLOAT, lexeme)
	} else {
		// Check for digit separators (underscores)
		lexeme := l.source[l.start:l.current]
		// Remove underscores for actual value
		cleaned := strings.ReplaceAll(lexeme, "_", "")
		l.addTokenWithLiteral(TOKEN_INTEGER, cleaned)
	}
}

// identifier lexes an identifier or keyword
func (l *Lexer) identifier() {
	for unicode.IsLetter(rune(l.peek())) || unicode.IsDigit(rune(l.peek())) || l.peek() == '_' {
		l.advance()
	}

	lexeme := l.source[l.start:l.current]
	if kwType, ok := keywords[lexeme]; ok {
		l.addToken(kwType)
	} else {
		l.addToken(TOKEN_IDENT)
	}
}

// Helper methods

func (l *Lexer) isAtEnd() bool {
	return l.current >= len(l.source)
}

func (l *Lexer) advance() byte {
	l.current++
	l.column++
	return l.source[l.current-1]
}

func (l *Lexer) peek() byte {
	if l.isAtEnd() {
		return 0
	}
	return l.source[l.current]
}

func (l *Lexer) peekNext() byte {
	if l.current+1 >= len(l.source) {
		return 0
	}
	return l.source[l.current+1]
}

func (l *Lexer) match(expected byte) bool {
	if l.isAtEnd() {
		return false
	}
	if l.source[l.current] != expected {
		return false
	}
	l.current++
	l.column++
	return true
}

func (l *Lexer) addToken(t TokenType) {
	l.tokens = append(l.tokens, Token{
		Type:   t,
		Lexeme: l.source[l.start:l.current],
		Line:   l.line,
		Column: l.column - (l.current - l.start),
	})
}

func (l *Lexer) addTokenWithLiteral(t TokenType, literal interface{}) {
	l.tokens = append(l.tokens, Token{
		Type:    t,
		Lexeme:  l.source[l.start:l.current],
		Line:    l.line,
		Column:  l.column - (l.current - l.start),
		Literal: literal,
	})
}

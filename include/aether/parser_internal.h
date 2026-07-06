#ifndef AETHER_PARSER_INTERNAL_H
#define AETHER_PARSER_INTERNAL_H

#include "aether/parser.h"

/* ── Cross-module function declarations ─────────────────────── */

/* parser_decl.c */
void parse_declaration(Parser *p, AstNodeList *decls);

/* parser_func.c */
AstNode *parse_func_decl(Parser *p);
AstNodeList parse_params(Parser *p);

/* parser_type_decl.c */
AstNode *parse_struct_decl(Parser *p);
AstNode *parse_enum_decl(Parser *p);

/* parser_stmt.c */
AstNode *parse_statement(Parser *p);

/* parser_block.c */
AstNode *parse_block(Parser *p);
AstNode *parse_block_braced(Parser *p);

/* parser_match.c */
AstNode *parse_match_arm(Parser *p);
AstNode *parse_pattern(Parser *p);

/* parser_type.c */
AstNode *parse_type_annotation(Parser *p);
AstNode *parse_type(Parser *p);
AstNode *parse_type_base(Parser *p);
AstNode *parse_type_postfix(Parser *p, AstNode *base);

/* parser_attr.c */
AstNode *parse_attribute(Parser *p);

/* parser_expr.c */
AstNode *parse_expr(Parser *p);
AstNode *parse_expr_prec(Parser *p, Precedence min_prec);
Precedence token_precedence(TokenType type);
AstNode *parse_prefix(Parser *p);
AstNode *parse_infix(Parser *p, AstNode *left, Precedence left_prec);

#endif /* AETHER_PARSER_INTERNAL_H */

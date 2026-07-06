#include "aether/parser.h"
#include "aether/parser_internal.h"
#include "aether/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Expression parsing (Pratt parser with precedence climbing)
 * ================================================================ */

/* Precedence of a token (returns PREC_MIN if not an operator) */
Precedence token_precedence(TokenType type) {
    switch (type) {
        case TOKEN_LPAREN: case TOKEN_LBRACKET: case TOKEN_DOT: case TOKEN_COLON_COLON:
            return PREC_CALL;
        case TOKEN_EQ: case TOKEN_PLUS_EQ: case TOKEN_MINUS_EQ:
        case TOKEN_STAR_EQ: case TOKEN_SLASH_EQ:
            return PREC_ASSIGNMENT;
        case TOKEN_PIPE_PIPE: case TOKEN_KW_OR: return PREC_LOGICAL_OR;
        case TOKEN_AND_AND: case TOKEN_KW_AND: return PREC_LOGICAL_AND;
        case TOKEN_EQ_EQ: case TOKEN_BANG_EQ: case TOKEN_LT:
        case TOKEN_GT: case TOKEN_LT_EQ: case TOKEN_GT_EQ:
            return PREC_COMPARISON;
        case TOKEN_KW_AS: return PREC_CAST;
        case TOKEN_PIPE: return PREC_BIT_OR;
        case TOKEN_CARET: return PREC_BIT_XOR;
        case TOKEN_AMPERSAND: return PREC_BIT_AND;
        case TOKEN_LT_LT: case TOKEN_GT_GT: return PREC_SHIFT;
        case TOKEN_DOT_DOT: case TOKEN_DOT_DOT_EQ: return PREC_RANGE;
        case TOKEN_PLUS: case TOKEN_MINUS: return PREC_TERM;
        case TOKEN_STAR: case TOKEN_SLASH: case TOKEN_PERCENT: case TOKEN_STAR_STAR: return PREC_FACTOR;
        case TOKEN_QUESTION: return PREC_TERNARY;
        case TOKEN_UNICODE_OP: return PREC_TERM;
        default: return PREC_MIN;
    }
}

/* Parsing a prefix expression */
AstNode *parse_prefix(Parser *p) {
    Token token = p->current;
    Location loc = token.loc;

    switch (token.type) {
        case TOKEN_INT_LITERAL:
            parser_advance(p);
            return node_int_literal(p->arena, loc, token.val.int_value);

        case TOKEN_FLOAT_LITERAL:
            parser_advance(p);
            return node_float_literal(p->arena, loc, token.val.float_value);

        case TOKEN_STRING_LITERAL: {
            parser_advance(p);
            StringView raw = token.text;
            const char *content = raw.data;
            size_t content_len = raw.len;
            if (content_len >= 2 && content[0] == '"' && content[content_len-1] == '"') {
                content += 1;
                content_len -= 2;
            }
            const char *p_start = content;
            const char *p_end = content + content_len;
            const char *cur = content;
            AstNode *result = NULL;
            bool found_interp = false;
            while (cur < p_end) {
                if (*cur == '{' && (cur == content || *(cur - 1) != '\\')) {
                    found_interp = true;
                    size_t lit_len = (size_t)(cur - p_start);
                    if (lit_len > 0) {
                        char *unesc = (char *)arena_alloc(p->arena, lit_len + 1);
                        size_t u = 0;
                        for (size_t s = 0; s < lit_len; s++) {
                            if (p_start[s] == '\\' && s + 1 < lit_len &&
                                (p_start[s + 1] == '{' || p_start[s + 1] == '}')) {
                                unesc[u++] = p_start[s + 1];
                                s++;
                            } else {
                                unesc[u++] = p_start[s];
                            }
                        }
                        unesc[u] = '\0';
                        char *quoted = (char *)arena_alloc(p->arena, u + 3);
                        quoted[0] = '"';
                        memcpy(quoted + 1, unesc, u);
                        quoted[u + 1] = '"';
                        quoted[u + 2] = '\0';
                        StringView lit_sv = sv_from_parts(quoted, u + 2);
                        AstNode *lit_node = node_string_literal(p->arena, loc, lit_sv);
                        if (result) {
                            result = node_binary(p->arena, loc, BIN_CONCAT, result, lit_node);
                        } else {
                            result = lit_node;
                        }
                    }
                    cur++;
                    const char *expr_start = cur;
                    int depth = 1;
                    while (cur < p_end && depth > 0) {
                        if (*cur == '\\' && cur + 1 < p_end && (*(cur + 1) == '{' || *(cur + 1) == '}')) {
                            cur += 2;
                        } else {
                            if (*cur == '{') depth++;
                            else if (*cur == '}') depth--;
                            if (depth > 0) cur++;
                        }
                    }
                    if (depth == 0) {
                        size_t expr_len = (size_t)(cur - expr_start);
                        if (expr_len > 0) {
                            Parser *sub_p = (Parser *)calloc(1, sizeof(Parser));
                            if (sub_p) {
                                sub_p->lexer = lexer_create(expr_start, expr_len, loc.file);
                                sub_p->arena = p->arena;
                                sub_p->has_current = false;
                                sub_p->error_count = 0;
                                sub_p->panic_mode = false;
                                sub_p->current_scope = NULL;
                                AstNode *expr_node = parse_expr(sub_p);
                                if (expr_node) {
                                    if (result) {
                                        result = node_binary(p->arena, loc, BIN_CONCAT, result, expr_node);
                                    } else {
                                        result = expr_node;
                                    }
                                }
                                lexer_destroy(sub_p->lexer);
                                free(sub_p);
                            }
                        }
                        cur++;
                        p_start = cur;
                    } else {
                        cur = expr_start;
                        p_start = cur;
                    }
                } else {
                    cur++;
                }
            }
            if (found_interp) {
                size_t lit_len = (size_t)(p_end - p_start);
                if (lit_len > 0) {
                    char *unesc = (char *)arena_alloc(p->arena, lit_len + 1);
                    size_t u = 0;
                    for (size_t s = 0; s < lit_len; s++) {
                        if (p_start[s] == '\\' && s + 1 < lit_len &&
                            (p_start[s + 1] == '{' || p_start[s + 1] == '}')) {
                            unesc[u++] = p_start[s + 1];
                            s++;
                        } else {
                            unesc[u++] = p_start[s];
                        }
                    }
                    unesc[u] = '\0';
                    char *quoted = (char *)arena_alloc(p->arena, u + 3);
                    quoted[0] = '"';
                    memcpy(quoted + 1, unesc, u);
                    quoted[u + 1] = '"';
                    quoted[u + 2] = '\0';
                    StringView lit_sv = sv_from_parts(quoted, u + 2);
                    AstNode *lit_node = node_string_literal(p->arena, loc, lit_sv);
                    if (result) {
                        result = node_binary(p->arena, loc, BIN_CONCAT, result, lit_node);
                    } else {
                        result = lit_node;
                    }
                }
                if (!result) {
                    result = node_string_literal(p->arena, loc, SV("\"\""));
                }
                return result;
            }
            return node_string_literal(p->arena, loc, token.text);
        }

        case TOKEN_CHAR_LITERAL:
            parser_advance(p);
            return node_char_literal(p->arena, loc, (uint32_t)token.val.int_value);

        case TOKEN_KW_TRUE:
            parser_advance(p);
            return node_bool_literal(p->arena, loc, true);

        case TOKEN_KW_FALSE:
            parser_advance(p);
            return node_bool_literal(p->arena, loc, false);

        case TOKEN_KW_NONE:
            parser_advance(p);
            return node_none_literal(p->arena, loc);

        case TOKEN_KW_SELF:
            parser_advance(p);
            return node_ident(p->arena, loc, SV("self"));

        case TOKEN_KW_MATCH: {
            parser_advance(p);
            AstNode *value = parse_expr(p);
            AstNode *match_node = node_match(p->arena, p->previous.loc, value);

            if (parser_match(p, TOKEN_LBRACE)) {
                while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
                    if (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON)) continue;
                    AstNode *arm = parse_match_arm(p);
                    node_list_append(&match_node->data.match_node.arms, arm);
                }
                parser_expect(p, TOKEN_RBRACE, "match body");
            }

            return match_node;
        }

        case TOKEN_IDENT:
            parser_advance(p);
            return node_ident(p->arena, loc, token.text);

        case TOKEN_LPAREN: {
            parser_advance(p);
            AstNode *expr = parse_expr(p);
            parser_expect(p, TOKEN_RPAREN, "parenthesized expression");
            return expr;
        }

        /* Prefix ++ and -- */
        case TOKEN_PLUS_PLUS:
        case TOKEN_MINUS_MINUS: {
            parser_advance(p);
            UnaryOp op = (token.type == TOKEN_PLUS_PLUS) ? UNARY_INC : UNARY_DEC;
            AstNode *operand = parse_prefix(p);
            if (!operand) return NULL;
            AstNode *node = node_create(p->arena, NODE_UNARY_OP, loc);
            node->data.unary.op = op;
            node->data.unary.operand = operand;
            return node;
        }

        case TOKEN_LBRACKET: {
            parser_advance(p);
            AstNode *arr = node_create(p->arena, NODE_ARRAY_LIT, loc);
            while (!parser_check(p, TOKEN_RBRACKET) && !parser_check(p, TOKEN_EOF)) {
                AstNode *elem = parse_expr(p);
                node_list_append(&arr->data.array_lit.elements, elem);
                if (!parser_match(p, TOKEN_COMMA)) break;
            }
            parser_expect(p, TOKEN_RBRACKET, "array literal");
            return arr;
        }

        /* Unary operators */
        case TOKEN_MINUS:
            parser_advance(p);
            return node_unary(p->arena, loc, UNARY_NEG, parse_expr_prec(p, PREC_UNARY));

        case TOKEN_BANG:
            parser_advance(p);
            return node_unary(p->arena, loc, UNARY_NOT, parse_expr_prec(p, PREC_UNARY));

        case TOKEN_KW_NOT:
            parser_advance(p);
            return node_unary(p->arena, loc, UNARY_NOT, parse_expr_prec(p, PREC_UNARY));

        case TOKEN_TILDE:
            parser_advance(p);
            return node_unary(p->arena, loc, UNARY_BIT_NOT, parse_expr_prec(p, PREC_UNARY));

        case TOKEN_AMPERSAND:
            parser_advance(p);
            return node_unary(p->arena, loc, UNARY_ADDR, parse_expr_prec(p, PREC_UNARY));

        case TOKEN_STAR:
            parser_advance(p);
            return node_unary(p->arena, loc, UNARY_DEREF, parse_expr_prec(p, PREC_UNARY));

        case TOKEN_PIPE: {
            /* Lambda: |params| expr or |params| { body } */
            parser_advance(p);
            AstNode *lambda = node_create(p->arena, NODE_LAMBDA, loc);
            while (!parser_check(p, TOKEN_PIPE) && !parser_check(p, TOKEN_EOF)) {
                if (parser_check(p, TOKEN_IDENT)) {
                    Token pt = p->current; parser_advance(p);
                    AstNode *type = NULL;
                    if (parser_match(p, TOKEN_COLON)) {
                        type = parse_type(p);
                    }
                    AstNode *param = node_param(p->arena, pt.loc,
                        node_ident(p->arena, pt.loc, pt.text), type, false, false);
                    node_list_append(&lambda->data.lambda.params, param);
                } else { parser_advance(p); }
                if (!parser_match(p, TOKEN_COMMA)) break;
            }
            parser_expect(p, TOKEN_PIPE, "lambda parameter list");
            parser_match(p, TOKEN_ARROW);
            if (parser_match(p, TOKEN_LBRACE)) {
                lambda->data.lambda.body = parse_block_braced(p);
            } else {
                lambda->data.lambda.body = parse_expr(p);
            }
            return lambda;
        }

        case TOKEN_KW_REF:
            parser_advance(p);
            return node_unary(p->arena, loc, UNARY_REF, parse_expr_prec(p, PREC_UNARY));

        case TOKEN_KW_OWNED:
            parser_advance(p);
            return node_unary(p->arena, loc, UNARY_OWNED, parse_expr_prec(p, PREC_UNARY));

        case TOKEN_KW_HEAP:
            parser_advance(p);
            return node_unary(p->arena, loc, UNARY_HEAP, parse_expr_prec(p, PREC_UNARY));

        case TOKEN_HASH:
            parser_advance(p);
            return node_unary(p->arena, loc, UNARY_ARRAY_LEN, parse_expr_prec(p, PREC_UNARY));

        default:
            parser_error(p, token, "expected expression");
            parser_advance(p);
            return NULL;
    }
}

/* Parsing an infix (binary) expression */
AstNode *parse_infix(Parser *p, AstNode *left, Precedence left_prec) {
    Token token = p->current;
    Location loc = token.loc;

    /* Function call: left(args) */
    if (token.type == TOKEN_LPAREN) {
        parser_advance(p);
        AstNode *call = node_call(p->arena, loc, left);
        while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
            while (parser_match(p, TOKEN_NEWLINE));
            if (parser_check(p, TOKEN_RPAREN)) break;
            if (parser_check(p, TOKEN_COMMA)) {
                AstNode *none = node_int_literal(p->arena, loc, 0);
                none->type = NODE_LITERAL_NONE;
                node_list_append(&call->data.call.args, none);
            } else {
                AstNode *arg = parse_expr(p);
                node_list_append(&call->data.call.args, arg);
            }
            if (!parser_match(p, TOKEN_COMMA)) break;
        }
        parser_expect(p, TOKEN_RPAREN, "function call");
        return call;
    }

    /* Index: left[expr] */
    if (token.type == TOKEN_LBRACKET) {
        parser_advance(p);
        AstNode *index = parse_expr(p);
        if (parser_match(p, TOKEN_DOT_DOT)) {
            AstNode *end = parse_expr(p);
            parser_expect(p, TOKEN_RBRACKET, "slice");
            AstNode *slice = node_create(p->arena, NODE_SLICE, loc);
            slice->data.slice.target = left;
            slice->data.slice.start = index;
            slice->data.slice.end = end;
            return slice;
        }
        parser_expect(p, TOKEN_RBRACKET, "index");
        return node_index(p->arena, loc, left, index);
    }

    /* Field access: left.field */
    if (token.type == TOKEN_DOT) {
        parser_advance(p);
        if (!parser_check(p, TOKEN_IDENT)) {
            parser_error(p, p->current, "expected field name");
            return left;
        }
        Token field = p->current; parser_advance(p);
        return node_field_access(p->arena, loc, left, node_ident(p->arena, field.loc, field.text));
    }

    /* Qualified name: EnumName::Variant */
    if (token.type == TOKEN_COLON_COLON) {
        parser_advance(p);
        if (!parser_check(p, TOKEN_IDENT)) {
            parser_error(p, p->current, "expected variant name after ::");
            return left;
        }
        Token variant = p->current; parser_advance(p);
        return node_field_access(p->arena, loc, left, node_ident(p->arena, variant.loc, variant.text));
    }

    /* Lambda pipe: left |params| body */
    if (token.type == TOKEN_PIPE) {
        return left;
    }

    /* Cast: left as Type */
    if (token.type == TOKEN_KW_AS) {
        parser_advance(p);
        AstNode *type_node = parse_type(p);
        if (!type_node) {
            parser_error(p, p->current, "expected type after 'as'");
            return left;
        }
        AstNode *cast_node = node_create(p->arena, NODE_CAST, loc);
        cast_node->data.binary.left = left;
        cast_node->data.binary.right = type_node;
        return cast_node;
    }

    /* Error context: throws_call ? "message" */
    if (token.type == TOKEN_QUESTION) {
        Token peek = lexer_peek_next(p->lexer);
        if (peek.type == TOKEN_STRING_LITERAL) {
            parser_advance(p);
            AstNode *right = parse_expr_prec(p, PREC_TERNARY);
            AstNode *node = node_binary(p->arena, loc, BIN_ERROR_CONTEXT, left, right);
            return node;
        }
    }

    /* Ternary: left ? middle : right */
    if (token.type == TOKEN_QUESTION) {
        parser_advance(p);
        AstNode *middle = parse_expr_prec(p, PREC_TERNARY);
        parser_expect(p, TOKEN_COLON, "ternary ':'");
        AstNode *right = parse_expr_prec(p, PREC_TERNARY - 1);
        AstNode *ternary = node_create(p->arena, NODE_TERNARY, loc);
        node_list_append(&ternary->data.list, left);
        node_list_append(&ternary->data.list, middle);
        node_list_append(&ternary->data.list, right);
        return ternary;
    }

    /* Binary operators */
    BinOp op;
    StringView custom_op = {0};
    switch (token.type) {
        case TOKEN_PLUS: op = BIN_ADD; break;
        case TOKEN_MINUS: op = BIN_SUB; break;
        case TOKEN_STAR: op = BIN_MUL; break;
        case TOKEN_STAR_STAR: op = BIN_POWER; break;
        case TOKEN_SLASH: op = BIN_DIV; break;
        case TOKEN_PERCENT: op = BIN_MOD; break;
        case TOKEN_EQ_EQ: op = BIN_EQ; break;
        case TOKEN_BANG_EQ: op = BIN_NEQ; break;
        case TOKEN_LT: op = BIN_LT; break;
        case TOKEN_GT: op = BIN_GT; break;
        case TOKEN_LT_EQ: op = BIN_LE; break;
        case TOKEN_GT_EQ: op = BIN_GE; break;
        case TOKEN_AND_AND: case TOKEN_KW_AND: op = BIN_AND; break;
        case TOKEN_PIPE_PIPE: op = BIN_OR; break;
        case TOKEN_KW_OR: op = BIN_OR; break;
        case TOKEN_AMPERSAND: op = BIN_BIT_AND; break;
        case TOKEN_PIPE: op = BIN_BIT_OR; break;
        case TOKEN_CARET: op = BIN_BIT_XOR; break;
        case TOKEN_LT_LT: op = BIN_SHL; break;
        case TOKEN_GT_GT: op = BIN_SHR; break;
        case TOKEN_DOT_DOT: op = BIN_RANGE; break;
        case TOKEN_DOT_DOT_EQ: op = BIN_RANGE_INCLUSIVE; break;
        case TOKEN_EQ: op = BIN_ASSIGN; break;
        case TOKEN_PLUS_EQ: op = BIN_ADD_ASSIGN; break;
        case TOKEN_MINUS_EQ: op = BIN_SUB_ASSIGN; break;
        case TOKEN_STAR_EQ: op = BIN_MUL_ASSIGN; break;
        case TOKEN_SLASH_EQ: op = BIN_DIV_ASSIGN; break;
        case TOKEN_UNICODE_OP:
            op = BIN_CUSTOM;
            custom_op = token.text;
            break;
        default:
            return left;
    }

    Precedence op_prec = token_precedence(token.type);
    parser_advance(p);
    AstNode *right = parse_expr_prec(p, op_prec);
    AstNode *result = node_binary(p->arena, loc, op, left, right);
    if (op == BIN_CUSTOM) {
        result->data.binary.custom_op = custom_op;
    }
    return result;
}

AstNode *parse_expr(Parser *p) {
    return parse_expr_prec(p, PREC_MIN);
}

AstNode *parse_expr_prec(Parser *p, Precedence min_prec) {
    if (!p->has_current) parser_advance(p);
    if (parser_check(p, TOKEN_EOF)) return NULL;

    AstNode *left = parse_prefix(p);
    if (!left) return NULL;

    /* Parse postfix operators (++, --) */
    while (parser_check(p, TOKEN_PLUS_PLUS) || parser_check(p, TOKEN_MINUS_MINUS)) {
        UnaryOp op = parser_match(p, TOKEN_PLUS_PLUS) ? UNARY_INC : UNARY_DEC;
        AstNode *postfix = node_create(p->arena, NODE_UNARY_OP, p->previous.loc);
        postfix->data.unary.op = op;
        postfix->data.unary.operand = left;
        left = postfix;
    }

    /* Parse infix operators while they have sufficient precedence */
    bool had_infix = false;
    while (true) {
        if (had_infix) {
            while (parser_match(p, TOKEN_NEWLINE));
        }
        if (parser_check(p, TOKEN_EOF) || parser_check(p, TOKEN_NEWLINE) ||
            parser_check(p, TOKEN_RPAREN) || parser_check(p, TOKEN_RBRACKET) ||
            parser_check(p, TOKEN_RBRACE) || parser_check(p, TOKEN_COMMA) ||
            parser_check(p, TOKEN_COLON) || parser_check(p, TOKEN_SEMICOLON) ||
            parser_check(p, TOKEN_EOF)) {
            break;
        }

        Precedence prec = token_precedence(p->current.type);
        if (prec < min_prec) break;
        if (prec == PREC_MIN) break;

        left = parse_infix(p, left, prec);
        if (!left) break;
        had_infix = true;
    }

    return left;
}

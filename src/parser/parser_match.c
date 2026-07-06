#include "aether/parser.h"
#include "aether/parser_internal.h"
#include "aether/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Pattern matching
 * ================================================================ */

/* Parse a single match arm: [case] pattern [, pattern]* -> body
 * Returns a NODE_MATCH_ARM with pattern set and patterns list populated. */
AstNode *parse_match_arm(Parser *p) {
    /* Skip optional 'case' keyword */
    parser_match(p, TOKEN_KW_CASE);
    /* Skip newlines after case */
    while (parser_match(p, TOKEN_NEWLINE));

    AstNode *pattern = NULL;
    AstNodeList extra_patterns = {0};

    /* If current token is ->, this is a bare arrow (no pattern) — wildcard */
    if (p->has_current && p->current.type != TOKEN_ARROW) {
        pattern = parse_pattern(p);
        /* Handle comma-separated patterns */
        while (p->has_current && parser_match(p, TOKEN_COMMA)) {
            /* Skip newlines after comma */
            while (parser_match(p, TOKEN_NEWLINE));
            AstNode *extra = parse_pattern(p);
            if (extra) {
                node_list_append(&extra_patterns, extra);
            }
        }
    }

    parser_expect(p, TOKEN_ARROW, "match arm arrow (->)");
    AstNode *body = parse_expr(p);

    AstNode *arm = node_match_arm(p->arena,
        pattern ? pattern->loc : p->previous.loc, pattern, body);
    arm->data.match_arm.patterns = extra_patterns;
    return arm;
}

AstNode *parse_pattern(Parser *p) {
    /* Simple patterns for now: literals, identifiers, wildcards */
    if (parser_match(p, TOKEN_KW_TRUE)) return node_bool_literal(p->arena, p->previous.loc, true);
    if (parser_match(p, TOKEN_KW_FALSE)) return node_bool_literal(p->arena, p->previous.loc, false);
    if (parser_match(p, TOKEN_KW_NONE)) return node_none_literal(p->arena, p->previous.loc);

    if (parser_check(p, TOKEN_INT_LITERAL)) {
        Token t = p->current; parser_advance(p);
        /* Check for range pattern: 1..9 or 1..=9 */
        if (parser_check(p, TOKEN_DOT_DOT) || parser_check(p, TOKEN_DOT_DOT_EQ)) {
            bool inclusive = parser_match(p, TOKEN_DOT_DOT_EQ);
            if (!inclusive) parser_match(p, TOKEN_DOT_DOT); /* consume .. */
            if (parser_check(p, TOKEN_INT_LITERAL)) {
                Token end_t = p->current; parser_advance(p);
                AstNode *start = node_int_literal(p->arena, t.loc, t.val.int_value);
                AstNode *end = node_int_literal(p->arena, end_t.loc, end_t.val.int_value);
                return node_binary(p->arena, t.loc, inclusive ? BIN_RANGE_INCLUSIVE : BIN_RANGE, start, end);
            }
            parser_error(p, p->current, "expected range end value");
            return NULL;
        }
        return node_int_literal(p->arena, t.loc, t.val.int_value);
    }

    if (parser_check(p, TOKEN_STRING_LITERAL)) {
        Token t = p->current; parser_advance(p);
        return node_string_literal(p->arena, t.loc, t.text);
    }

    if (parser_check(p, TOKEN_IDENT)) {
        /* '_' wildcard pattern */
        if (sv_eq_cstr(p->current.text, "_")) {
            parser_advance(p);
            return node_ident(p->arena, p->previous.loc, SV("_"));
        }
        Token t = p->current; parser_advance(p);
        return node_ident(p->arena, t.loc, t.text);
    }

    /* Don't error on tokens that end a pattern — let the caller handle these */
    if (p->has_current) {
        TokenType ct = p->current.type;
        if (ct == TOKEN_COMMA || ct == TOKEN_ARROW || ct == TOKEN_NEWLINE || ct == TOKEN_RBRACE) {
            return NULL;
        }
    } else {
        return NULL;
    }

    if (p->has_current) {
        parser_error(p, p->current, "expected pattern");
    }
    return NULL;
}

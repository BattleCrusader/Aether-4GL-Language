#include "aether/parser.h"
#include "aether/parser_internal.h"
#include "aether/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Type parsing
 * ================================================================ */

AstNode *parse_type_annotation(Parser *p) {
    if (parser_match(p, TOKEN_COLON)) {
        return parse_type(p);
    }
    return NULL;
}

AstNode *parse_type(Parser *p) {
    AstNode *base = parse_type_base(p);
    return parse_type_postfix(p, base);
}

AstNode *parse_type_base(Parser *p) {
    /* func(params): rettype — function pointer type */
    if (parser_match(p, TOKEN_KW_FUNC)) {
        AstNode *t = node_create(p->arena, NODE_TYPE_FN, p->previous.loc);
        if (parser_match(p, TOKEN_LPAREN)) {
            while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
                AstNode *ptype = parse_type(p);
                if (ptype) node_list_append(&t->data.type_node.param_types, ptype);
                if (!parser_match(p, TOKEN_COMMA)) break;
            }
            parser_expect(p, TOKEN_RPAREN, "function type parameter list");
        }
        if (parser_match(p, TOKEN_COLON)) {
            t->data.type_node.return_type = parse_type(p);
        }
        return t;
    }

    /* Primitive types */
    if (parser_check(p, TOKEN_IDENT)) {
        StringView name = p->current.text;
        PrimType prim;
        bool is_prim = true;

        if (sv_eq_cstr(name, "void")) prim = PRIM_VOID;
        else if (sv_eq_cstr(name, "bool")) prim = PRIM_BOOL;
        else if (sv_eq_cstr(name, "byte")) prim = PRIM_BYTE;
        else if (sv_eq_cstr(name, "u8")) prim = PRIM_U8;
        else if (sv_eq_cstr(name, "u16")) prim = PRIM_U16;
        else if (sv_eq_cstr(name, "u32")) prim = PRIM_U32;
        else if (sv_eq_cstr(name, "u64")) prim = PRIM_U64;
        else if (sv_eq_cstr(name, "u128")) prim = PRIM_U128;
        else if (sv_eq_cstr(name, "i8")) prim = PRIM_I8;
        else if (sv_eq_cstr(name, "i16")) prim = PRIM_I16;
        else if (sv_eq_cstr(name, "i32")) prim = PRIM_I32;
        else if (sv_eq_cstr(name, "i64")) prim = PRIM_I64;
        else if (sv_eq_cstr(name, "f32")) prim = PRIM_F32;
        else if (sv_eq_cstr(name, "f64")) prim = PRIM_F64;
        else if (sv_eq_cstr(name, "string")) prim = PRIM_STRING;
        else if (sv_eq_cstr(name, "int")) prim = PRIM_I64;
        else if (sv_eq_cstr(name, "float")) prim = PRIM_F32;
        else if (sv_eq_cstr(name, "double")) prim = PRIM_F64;
        else if (sv_eq_cstr(name, "char")) prim = PRIM_BYTE;
        else is_prim = false;

        if (is_prim) {
            parser_advance(p);
            return node_type_prim(p->arena, p->previous.loc, prim);
        }

        /* _ (underscore) — inferred type */
        if (name.len == 1 && name.data[0] == '_') {
            parser_advance(p);
            return node_create(p->arena, NODE_TYPE_INFER, p->previous.loc);
        }

        /* auto — inferred type */
        if (sv_eq_cstr(name, "auto")) {
            parser_advance(p);
            return node_create(p->arena, NODE_TYPE_INFER, p->previous.loc);
        }

        /* Named type (struct/enum name) */
        parser_advance(p);
        return node_type_named(p->arena, p->previous.loc, name);
    }

    /* dyn Trait — dynamic dispatch */
    if (parser_match(p, TOKEN_KW_DYN)) {
        AstNode *inner = parse_type(p);
        AstNode *t = node_create(p->arena, NODE_TYPE_REF, p->previous.loc);
        t->data.type_node.elem_type = inner;
        t->data.type_node.is_ref = true;
        return t;
    }

    /* ptr T — detect from identifier */
    if (parser_check(p, TOKEN_IDENT) && sv_eq_cstr(p->current.text, "ptr")) {
        parser_advance(p);
        AstNode *elem = parse_type(p);
        AstNode *t = node_create(p->arena, NODE_TYPE_PTR, p->previous.loc);
        t->data.type_node.elem_type = elem;
        return t;
    }

    /* *T (deref type / raw pointer shorthand) */
    if (parser_match(p, TOKEN_STAR)) {
        AstNode *elem = parse_type(p);
        AstNode *t = node_create(p->arena, NODE_TYPE_PTR, p->previous.loc);
        t->data.type_node.elem_type = elem;
        return t;
    }

    /* [T] or [T; N] */
    if (parser_match(p, TOKEN_LBRACKET)) {
        AstNode *elem = parse_type(p);
        if (parser_match(p, TOKEN_COMMA) || parser_match(p, TOKEN_SEMICOLON)) {
            /* Fixed-size array: [T; N] */
            Token size = p->current; parser_advance(p);
            AstNode *t = node_create(p->arena, NODE_TYPE_ARRAY, p->previous.loc);
            t->data.type_node.elem_type = elem;
            if (size.type == TOKEN_INT_LITERAL) t->data.type_node.array_size = (int)size.val.int_value;
            parser_expect(p, TOKEN_RBRACKET, "array type");
            return t;
        }
        parser_expect(p, TOKEN_RBRACKET, "type");
        AstNode *t = node_create(p->arena, NODE_TYPE_SLICE, p->previous.loc);
        t->data.type_node.elem_type = elem;
        return t;
    }

    /* ref T — borrowed reference */
    if (parser_match(p, TOKEN_KW_REF)) {
        AstNode *inner = parse_type(p);
        AstNode *t = node_create(p->arena, NODE_TYPE_REF, p->previous.loc);
        t->data.type_node.elem_type = inner;
        return t;
    }

    /* owned T — unique ownership */
    if (parser_match(p, TOKEN_KW_OWNED)) {
        AstNode *inner = parse_type(p);
        AstNode *t = node_create(p->arena, NODE_TYPE_REF, p->previous.loc);
        t->data.type_node.elem_type = inner;
        t->data.type_node.is_owned = true;
        return t;
    }

    /* rc T — shared ownership */
    if (parser_match(p, TOKEN_KW_RC)) {
        AstNode *inner = parse_type(p);
        AstNode *t = node_create(p->arena, NODE_TYPE_REF, p->previous.loc);
        t->data.type_node.elem_type = inner;
        t->data.type_node.is_rc = true;
        return t;
    }

    /* (type1, type2, ...) — tuple type */
    if (parser_match(p, TOKEN_LPAREN)) {
        AstNode *first = parse_type(p);
        if (parser_match(p, TOKEN_COMMA)) {
            /* It's a tuple: (type1, type2, ...) */
            AstNode *t = node_create(p->arena, NODE_TYPE_TUPLE, p->previous.loc);
            node_list_append(&t->data.type_node.tuple_types, first);
            while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
                AstNode *elem = parse_type(p);
                if (elem) node_list_append(&t->data.type_node.tuple_types, elem);
                if (!parser_match(p, TOKEN_COMMA)) break;
            }
            parser_expect(p, TOKEN_RPAREN, "tuple type");
            return t;
        }
        /* Single type in parens — just return the type */
        parser_expect(p, TOKEN_RPAREN, "parenthesized type");
        return first;
    }

    parser_error(p, p->current, "expected type");
    parser_advance(p); /* skip */
    return NULL;
}

/* Postfix type modifiers — called after a base type is parsed */
AstNode *parse_type_postfix(Parser *p, AstNode *base) {
    /* T? (optional) — only consume ? if it's NOT followed by an expression-starting token */
    if (parser_check(p, TOKEN_QUESTION)) {
        Token peek = lexer_peek_next(p->lexer);
        if (peek.type == TOKEN_NEWLINE || peek.type == TOKEN_EQ ||
            peek.type == TOKEN_RPAREN || peek.type == TOKEN_COMMA ||
            peek.type == TOKEN_SEMICOLON || peek.type == TOKEN_RBRACE ||
            peek.type == TOKEN_EOF || peek.type == TOKEN_COLON ||
            peek.type == TOKEN_RBRACKET) {
            parser_advance(p);
            AstNode *t = node_create(p->arena, NODE_TYPE_OPTIONAL, p->previous.loc);
            t->data.type_node.elem_type = base;
            return t;
        }
    }
    return base;
}

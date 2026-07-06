#include "aether/parser.h"
#include "aether/parser_internal.h"
#include "aether/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Struct declarations
 * ================================================================ */

AstNode *parse_struct_decl(Parser *p) {
    if (!parser_check(p, TOKEN_IDENT)) {
        parser_error(p, p->current, "expected struct name");
        return NULL;
    }
    Token name_tok = p->current; parser_advance(p);
    AstNode *name = node_ident(p->arena, name_tok.loc, name_tok.text);
    AstNode *st = node_struct_decl(p->arena, name_tok.loc, name, false);

    if (parser_match(p, TOKEN_LBRACE)) {
        /* Parse fields */
        while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
            if (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON) ||
                parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_NEWLINE)) continue;

            bool is_pub = parser_match(p, TOKEN_KW_PUB);

            /* Check for contract: contract(expr) */
            if (parser_match(p, TOKEN_KW_CONTRACT)) {
                parser_expect(p, TOKEN_LPAREN, "contract condition");
                AstNode *cond = parse_expr(p);
                if (cond) node_list_append(&st->data.struct_decl.contracts, cond);
                parser_expect(p, TOKEN_RPAREN, "contract condition");
                continue;
            }

            /* Check if this is a method declaration: func name(...) */
            if (parser_check(p, TOKEN_KW_FUNC)) {
                parser_advance(p);
                AstNode *method = parse_func_decl(p);
                if (method) {
                    /* Auto-inject 'self' as the first parameter for methods. */
                    AstNode *self_type = node_create(p->arena, NODE_TYPE_REF, name_tok.loc);
                    self_type->data.type_node.elem_type = node_type_named(p->arena, name_tok.loc, name_tok.text);
                    AstNode *self_param = node_param(p->arena, method->loc,
                        node_ident(p->arena, method->loc, SV("self")),
                        self_type, false, false);
                    /* Prepend self to the param list */
                    AstNodeList new_params = {0};
                    node_list_append(&new_params, self_param);
                    for (int pi = 0; pi < method->data.func.params.count; pi++)
                        node_list_append(&new_params, method->data.func.params.items[pi]);
                    method->data.func.params = new_params;
                    node_list_append(&st->data.struct_decl.methods, method);
                }
                continue;
            }

            if (!parser_check(p, TOKEN_IDENT)) {
                parser_error(p, p->current, "expected field name");
                break;
            }
            
            Token fname = p->current; parser_advance(p);
            AstNode *field_type = NULL;
            if (parser_match(p, TOKEN_COLON)) {
                field_type = parse_type(p);
            }

            AstNode *field = node_param(p->arena, fname.loc,
                node_ident(p->arena, fname.loc, fname.text),
                field_type, false, false);
            field->type = NODE_FIELD;
            node_list_append(&st->data.struct_decl.fields, field);
            /* Consume optional semicolon after field */
            parser_match(p, TOKEN_SEMICOLON);
        }
        parser_expect(p, TOKEN_RBRACE, "struct body");
    }

    return st;
}

/* ================================================================
 * Enum declarations
 * ================================================================ */

AstNode *parse_enum_decl(Parser *p) {
    if (!parser_check(p, TOKEN_IDENT)) {
        parser_error(p, p->current, "expected enum name");
        return NULL;
    }
    Token name_tok = p->current; parser_advance(p);
    AstNode *name = node_ident(p->arena, name_tok.loc, name_tok.text);
    AstNode *en = node_enum_decl(p->arena, name_tok.loc, name, false);

    if (parser_match(p, TOKEN_LBRACE)) {
        while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
            if (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON) ||
                parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_NEWLINE)) continue;

            if (!parser_check(p, TOKEN_IDENT)) {
                parser_error(p, p->current, "expected variant name");
                break;
            }
            Token vname = p->current; parser_advance(p);
            AstNode *variant = node_create(p->arena, NODE_ENUM_VARIANT, vname.loc);
            variant->data.enum_variant.name = node_ident(p->arena, vname.loc, vname.text);

            /* Optional payload types */
            if (parser_match(p, TOKEN_LPAREN)) {
                while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
                    AstNode *ptype = parse_type(p);
                    if (ptype) node_list_append(&variant->data.enum_variant.payload_types, ptype);
                    if (!parser_match(p, TOKEN_COMMA)) break;
                }
                parser_expect(p, TOKEN_RPAREN, "enum variant payload");
            }

            node_list_append(&en->data.enum_decl.variants, variant);
        }
        parser_expect(p, TOKEN_RBRACE, "enum body");
    }

    return en;
}

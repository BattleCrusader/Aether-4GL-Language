#include "aether/parser.h"
#include "aether/parser_internal.h"
#include "aether/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Function declarations
 * ================================================================ */

AstNode *parse_func_decl(Parser *p) {
    Location loc = p->previous.loc;

    /* Function name */
    if (!parser_check(p, TOKEN_IDENT)) {
        parser_error(p, p->current, "expected function name");
        return NULL;
    }
    Token name_tok = p->current; parser_advance(p);

    AstNode *name = node_ident(p->arena, name_tok.loc, name_tok.text);
    AstNode *func = node_func_decl(p->arena, loc, name, false, false);

    /* Generic type params: func Name<T, U>(params) or func Name<T: Constraint>(params) */
    if (parser_match(p, TOKEN_LT)) {
        while (!parser_check(p, TOKEN_GT) && !parser_check(p, TOKEN_EOF)) {
            if (parser_check(p, TOKEN_IDENT)) {
                Token tp = p->current; parser_advance(p);
                AstNode *tp_node = node_create(p->arena, NODE_TYPE_PARAM, tp.loc);
                tp_node->data.type_param.name = node_ident(p->arena, tp.loc, tp.text);
                tp_node->data.type_param.constraint = NULL;
                /* Check for optional constraint: T: Constraint */
                if (parser_match(p, TOKEN_COLON)) {
                    tp_node->data.type_param.constraint = parse_type(p);
                }
                node_list_append(&func->data.func.type_params, tp_node);
            } else { parser_advance(p); }
            if (!parser_match(p, TOKEN_COMMA)) break;
        }
        parser_expect(p, TOKEN_GT, "generic type parameter list");
    }

    /* Parameters: (params) */
    if (parser_match(p, TOKEN_LPAREN)) {
        func->data.func.params = parse_params(p);
        parser_expect(p, TOKEN_RPAREN, "function parameter list");
    }

    /* Optional return type: `: type` */
    if (parser_match(p, TOKEN_COLON)) {
        func->data.func.return_type = parse_type(p);
    }

    /* Optional throws annotation */
    if (parser_match(p, TOKEN_KW_THROWS)) {
        func->data.func.is_throws = true;
    }

    /* Check if this is an operator overload method (op_add, op_sub, etc.) */
    {
        const char *fname = arena_strndup(p->arena,
            func->data.func.name->data.ident.name.data,
            func->data.func.name->data.ident.name.len);
        if (strncmp(fname, "op_", 3) == 0) {
            func->data.func.is_operator = true;

            /* Compute signature hash: djb2 over op_<symbol> + param types */
            uint32_t hash = 5381;
            for (size_t si = 0; si < func->data.func.name->data.ident.name.len; si++) {
                hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)func->data.func.name->data.ident.name.data[si]);
            }
            for (int pi = 0; pi < func->data.func.params.count; pi++) {
                AstNode *param = func->data.func.params.items[pi];
                AstNode *ptype = param->data.param.type;
                if (ptype && ptype->type == NODE_TYPE_PRIMITIVE) {
                    hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)ptype->data.type_node.prim);
                } else if (ptype && ptype->type == NODE_TYPE_NAMED) {
                    for (size_t si = 0; si < ptype->data.type_node.name.len; si++) {
                        hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)ptype->data.type_node.name.data[si]);
                    }
                } else {
                    hash = (uint32_t)(((hash << 5) + hash) + 0xFF);
                }
            }
            func->data.func.sig_hash = hash;
        }
    }

    /* Contract conditions: pre(expr) and post(expr) */
    while (true) {
        if (parser_match(p, TOKEN_KW_PRE)) {
            parser_expect(p, TOKEN_LPAREN, "pre condition");
            AstNode *cond = parse_expr(p);
            if (cond) node_list_append(&func->data.func.pre_conditions, cond);
            parser_expect(p, TOKEN_RPAREN, "pre condition");
        } else if (parser_match(p, TOKEN_KW_POST)) {
            parser_expect(p, TOKEN_LPAREN, "post condition");
            AstNode *cond = parse_expr(p);
            if (cond) node_list_append(&func->data.func.post_conditions, cond);
            parser_expect(p, TOKEN_RPAREN, "post condition");
        } else {
            break;
        }
    }

    /* Body */
    if (parser_match(p, TOKEN_LBRACE)) {
        func->data.func.body = parse_block_braced(p);
    } else if (parser_match(p, TOKEN_ARROW)) {
        /* Expression-bodied function: func name(): type -> expr */
        AstNode *expr = parse_expr(p);
        if (expr) {
            /* Wrap the expression in a block with a return statement */
            AstNode *block = node_block(p->arena, func->loc);
            AstNode *ret = node_return(p->arena, expr->loc, expr);
            node_list_append(&block->data.list, ret);
            func->data.func.body = block;
        }
    } else {
        /* No body = function declaration only (extern) */
    }

    return func;
}

/* ================================================================
 * Parameter list
 * ================================================================ */

AstNodeList parse_params(Parser *p) {
    AstNodeList params = {0};

    while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
        bool is_mut = parser_match(p, TOKEN_KW_MUT);
        bool is_varargs = false;
        /* Variadic: ... before param name or ... before type */
        if (parser_match(p, TOKEN_DOT_DOT)) {
            is_varargs = true;
            /* Consume the third dot if present: ...name */
            parser_match(p, TOKEN_DOT);
        }

        /* Allow both TOKEN_IDENT and TOKEN_KW_SELF as param names */
        if (!parser_check(p, TOKEN_IDENT) && !parser_check(p, TOKEN_KW_SELF)) {
            parser_error(p, p->current, "expected parameter name");
            break;
        }
        Token name_tok = p->current; parser_advance(p);

        AstNode *type = NULL;
        if (parser_match(p, TOKEN_COLON)) {
            /* Check for ...Type syntax after colon (variadic) */
            if (parser_match(p, TOKEN_DOT_DOT)) {
                is_varargs = true;
                /* Consume the third dot if present: ...Type */
                parser_match(p, TOKEN_DOT);
            }
            type = parse_type(p);
        }

        /* Default parameter value: param: Type = expr */
        AstNode *default_value = NULL;
        if (parser_match(p, TOKEN_EQ)) {
            default_value = parse_expr(p);
        }

        AstNode *param = node_param(p->arena, name_tok.loc,
            node_ident(p->arena, name_tok.loc, name_tok.text),
            type, is_mut, is_varargs);
        param->data.param.default_value = default_value;

        node_list_append(&params, param);

        if (!parser_match(p, TOKEN_COMMA)) break;
    }

    return params;
}

#include "aether/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Parser creation/destruction
 * ================================================================ */

Parser *parser_create(const char *source, size_t length, const char *filename) {
    Parser *p = (Parser *)calloc(1, sizeof(Parser));
    if (!p) return NULL;

    p->lexer = lexer_create(source, length, filename);
    if (!p->lexer) { free(p); return NULL; }

    p->arena = arena_create();
    p->has_current = false;
    p->error_count = 0;
    p->panic_mode = false;
    p->current_scope = NULL;

    return p;
}

void parser_destroy(Parser *p) {
    if (p) {
        lexer_destroy(p->lexer);
        arena_destroy(p->arena);
        free(p);
    }
}

/* ================================================================
 * Token management
 * ================================================================ */

void parser_advance(Parser *p) {
    p->previous = p->current;
    lexer_advance(p->lexer);
    p->current = p->lexer->current;
    p->has_current = true;
}

bool parser_check(Parser *p, TokenType type) {
    if (!p->has_current) parser_advance(p);
    return p->current.type == type;
}

bool parser_check_any(Parser *p, const TokenType *types, int count) {
    if (!p->has_current) parser_advance(p);
    for (int i = 0; i < count; i++) {
        if (p->current.type == types[i]) return true;
    }
    return false;
}

bool parser_match(Parser *p, TokenType type) {
    if (parser_check(p, type)) {
        parser_advance(p);
        return true;
    }
    return false;
}

void parser_expect(Parser *p, TokenType type, const char *context) {
    if (parser_check(p, type)) {
        parser_advance(p);
    } else {
        parser_error(p, p->current, "expected token");
    }
}

void parser_error(Parser *p, Token token, const char *message) {
    fprintf(stderr, "Error at %s:%d:%d: %s (got '%s')\n",
            token.loc.file ? token.loc.file : "?",
            token.loc.line, token.loc.col,
            message, token_type_name(token.type));
    p->error_count++;
    p->panic_mode = true;
}

/* ================================================================
 * Synchronization
 * ================================================================ */

/* Tokens that start a statement */
static const TokenType STMT_START[] = {
    TOKEN_KW_LET, TOKEN_KW_IF, TOKEN_KW_WHILE, TOKEN_KW_FOR,
    TOKEN_KW_RETURN, TOKEN_KW_BREAK, TOKEN_KW_CONTINUE,
    TOKEN_KW_DEFER, TOKEN_KW_MATCH, TOKEN_KW_ASM,
    TOKEN_KW_FUNC, TOKEN_KW_STRUCT, TOKEN_KW_ENUM,
    TOKEN_KW_CONST, TOKEN_KW_IMPORT, TOKEN_KW_MODULE,
    TOKEN_KW_PUB, TOKEN_KW_STATIC, TOKEN_KW_TEST,
    TOKEN_KW_UNSAFE, TOKEN_KW_TRY, TOKEN_KW_THROW,
    TOKEN_AT,
    TOKEN_RBRACE, /* closing brace can follow statements */
};

bool parser_at_stmt_start(Parser *p) {
    if (!p->has_current) parser_advance(p);
    return parser_check_any(p, STMT_START, sizeof(STMT_START) / sizeof(STMT_START[0]));
}

void parser_sync(Parser *p) {
    p->panic_mode = false;
    while (!parser_check(p, TOKEN_EOF)) {
        if (parser_at_stmt_start(p)) return;
        parser_advance(p);
    }
}

/* ================================================================
 * Top-level parsing entry point
 * ================================================================ */

AstNode *parser_parse(Parser *p) {
    parser_advance(p); /* prime the first token */

    AstNode *program = node_program(p->arena);
    AstNodeList *decls = &program->data.list;

    while (!parser_check(p, TOKEN_EOF)) {
        parse_declaration(p, decls);
        /* Skip newlines between top-level declarations */
        while (parser_match(p, TOKEN_NEWLINE));
    }

    return program;
}

/* ================================================================
 * Top-level declarations
 * ================================================================ */

void parse_declaration(Parser *p, AstNodeList *decls) {
    if (p->panic_mode) { parser_sync(p); if (parser_check(p, TOKEN_EOF)) return; }

    /* Handle attributes like @export, @entry */
    while (parser_match(p, TOKEN_AT)) {
        parse_attribute(p);
    }

    /* Handle pub/private/internal/static modifiers */
    bool is_pub = parser_match(p, TOKEN_KW_PUB);
    bool is_private = parser_match(p, TOKEN_KW_PRIVATE);
    bool is_internal = parser_match(p, TOKEN_KW_INTERNAL);
    bool is_static = parser_match(p, TOKEN_KW_STATIC);
    bool is_test = parser_match(p, TOKEN_KW_TEST);
    AccessLevel access = is_private ? ACCESS_PRIVATE : (is_internal ? ACCESS_INTERNAL : ACCESS_PUB);

    if (parser_match(p, TOKEN_KW_FUNC)) {
        AstNode *func = parse_func_decl(p);
        if (func) {
            func->data.func.access = access;
            func->data.func.is_pub = is_pub;  // kept for backward compat for now
            func->data.func.is_static = is_static;
            func->data.func.is_test = is_test;
            node_list_append(decls, func);
        }
    } else if (parser_match(p, TOKEN_KW_STRUCT)) {
        AstNode *s = parse_struct_decl(p);
        if (s) {
            s->data.struct_decl.is_pub = is_pub;
            node_list_append(decls, s);
        }
    } else if (parser_match(p, TOKEN_KW_CLASS)) {
        AstNode *s = parse_struct_decl(p);
        if (s) {
            s->type = NODE_CLASS_DECL;
            s->data.struct_decl.is_pub = is_pub;
            node_list_append(decls, s);
        }
    } else if (parser_match(p, TOKEN_KW_ENUM)) {
        AstNode *e = parse_enum_decl(p);
        if (e) {
            e->data.enum_decl.is_pub = is_pub;
            node_list_append(decls, e);
        }
    } else if (parser_match(p, TOKEN_KW_CONST)) {
        /* const name = expr */
        if (parser_check(p, TOKEN_IDENT)) {
            Token name_tok = p->current; parser_advance(p);
            parser_expect(p, TOKEN_EQ, "const declaration");
            AstNode *init = parse_expr(p);
            AstNode *node = node_let(p->arena, name_tok.loc,
                node_ident(p->arena, name_tok.loc, name_tok.text),
                NULL, init, false);
            node->type = NODE_CONST_DECL;
            node_list_append(decls, node);
        }
    } else if (parser_match(p, TOKEN_KW_IMPORT)) {
        /* import "path" or import name */
        Token path = p->current; parser_advance(p);
        AstNode *node = node_string_literal(p->arena, path.loc, path.text);
        node->type = NODE_IMPORT;
        node_list_append(decls, node);
    } else if (parser_match(p, TOKEN_KW_MODULE)) {
        /* module name { decls } */
        Token name = p->current; parser_advance(p);
        AstNode *mod = node_create(p->arena, NODE_MODULE_DECL, name.loc);
        if (parser_check(p, TOKEN_LBRACE)) {
            parser_advance(p);
            /* parse module body */
            while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
                parse_declaration(p, decls);
                while (parser_match(p, TOKEN_NEWLINE));
            }
            parser_expect(p, TOKEN_RBRACE, "module body");
        }
        node_list_append(decls, mod);
    } else if (parser_match(p, TOKEN_KW_TRAIT)) {
        /* trait Name { method_signatures } */
        if (parser_check(p, TOKEN_IDENT)) {
            Token name_tok = p->current; parser_advance(p);
            AstNode *t = node_create(p->arena, NODE_TRAIT_DECL, name_tok.loc);
            t->data.trait_decl.name = node_ident(p->arena, name_tok.loc, name_tok.text);
            t->data.trait_decl.is_pub = is_pub;
            if (parser_match(p, TOKEN_LBRACE)) {
                while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
                    if (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON) ||
                        parser_match(p, TOKEN_INDENT) || parser_match(p, TOKEN_DEDENT)) continue;
                    if (parser_match(p, TOKEN_KW_FUNC)) {
                        AstNode *method = parse_func_decl(p);
                        if (method) node_list_append(&t->data.trait_decl.methods, method);
                    } else { parser_advance(p); }
                }
                parser_expect(p, TOKEN_RBRACE, "trait body");
            }
            node_list_append(decls, t);
        }
    } else if (parser_match(p, TOKEN_KW_IMPL)) {
        /* impl Trait for Type { methods } */
        if (parser_check(p, TOKEN_IDENT)) {
            Token trait_tok = p->current; parser_advance(p);
            if (parser_match(p, TOKEN_KW_FOR) && parser_check(p, TOKEN_IDENT)) {
                Token type_tok = p->current; parser_advance(p);
                AstNode *impl = node_create(p->arena, NODE_IMPL_BLOCK, trait_tok.loc);
                impl->data.impl_block.trait_name = trait_tok.text;
                impl->data.impl_block.type_name = type_tok.text;
                if (parser_match(p, TOKEN_LBRACE)) {
                    while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
                        if (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON) ||
                            parser_match(p, TOKEN_INDENT) || parser_match(p, TOKEN_DEDENT)) continue;
                        if (parser_match(p, TOKEN_KW_FUNC)) {
                            AstNode *method = parse_func_decl(p);
                            if (method) node_list_append(&impl->data.impl_block.methods, method);
                        } else { parser_advance(p); }
                    }
                    parser_expect(p, TOKEN_RBRACE, "impl body");
                }
                node_list_append(decls, impl);
            }
        }
    } else if (parser_match(p, TOKEN_HASH)) {
        /* #run { body } — compile-time execution block */
        if (parser_match(p, TOKEN_KW_RUN)) {
            AstNode *run_node = node_create(p->arena, NODE_RUN_BLOCK, p->previous.loc);
            if (parser_match(p, TOKEN_LBRACE)) {
                /* Collect the body as a block of statements */
                AstNode *body = node_block(p->arena, p->previous.loc);
                while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
                    if (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON) ||
                        parser_match(p, TOKEN_INDENT) || parser_match(p, TOKEN_DEDENT)) continue;
                    AstNode *stmt = parse_statement(p);
                    if (stmt) node_list_append(&body->data.list, stmt);
                }
                parser_expect(p, TOKEN_RBRACE, "#run body");
                run_node->data.list = body->data.list;
            }
            node_list_append(decls, run_node);
        } else {
            parser_error(p, p->current, "expected 'run' after #");
        }
    } else {
        /* Try to parse as expression statement (bare identifier = call?) */
        /* Report error if nothing matched */
        Token tok = p->current;
        parser_error(p, tok, "expected declaration");
        parser_advance(p);
    }
}

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

    /* Check for asm func or sys func (already consumed the keyword) */
    /* Note: 'asm func' is detected by the 'asm' keyword having been consumed
       and the next token being 'func' — handled at declaration level */

    /* Generic type params: func Name<T, U>(params) — angle brackets after name */
    if (parser_match(p, TOKEN_LT)) {
        while (!parser_check(p, TOKEN_GT) && !parser_check(p, TOKEN_EOF)) {
            if (parser_check(p, TOKEN_IDENT)) {
                Token tp = p->current; parser_advance(p);
                AstNode *tp_node = node_ident(p->arena, tp.loc, tp.text);
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
        }
    }

    /* Contract conditions: pre(expr) and post(expr) */
    /* Inline between signature and body brace:
       func name(): type pre(cond) { body } */
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
        bool is_varargs = parser_match(p, TOKEN_PIPE_PIPE); /* || means varargs? */
        /* Actually varargs syntax is '...' or use '...int' style */
        if (parser_match(p, TOKEN_DOT_DOT)) {
            is_varargs = true;
        }

        /* Allow both TOKEN_IDENT and TOKEN_KW_SELF as param names */
        if (!parser_check(p, TOKEN_IDENT) && !parser_check(p, TOKEN_KW_SELF)) {
            parser_error(p, p->current, "expected parameter name");
            break;
        }
        Token name_tok = p->current; parser_advance(p);

        AstNode *type = NULL;
        if (parser_match(p, TOKEN_COLON)) {
            type = parse_type(p);
        }

        AstNode *param = node_param(p->arena, name_tok.loc,
            node_ident(p->arena, name_tok.loc, name_tok.text),
            type, is_mut, is_varargs);

        node_list_append(&params, param);

        if (!parser_match(p, TOKEN_COMMA)) break;
    }

    return params;
}

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
                parser_match(p, TOKEN_INDENT) || parser_match(p, TOKEN_DEDENT)) continue;

            bool is_pub = parser_match(p, TOKEN_KW_PUB);

            /* Check if this is a method declaration: func name(...) */
            if (parser_check(p, TOKEN_KW_FUNC)) {
                parser_advance(p);
                AstNode *method = parse_func_decl(p);
                if (method) node_list_append(&st->data.struct_decl.methods, method);
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
                parser_match(p, TOKEN_INDENT) || parser_match(p, TOKEN_DEDENT)) continue;

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

/* ================================================================
 * Statement parsing
 * ================================================================ */

AstNode *parse_statement(Parser *p) {
    if (p->panic_mode) { parser_sync(p); if (parser_check(p, TOKEN_EOF)) return NULL; }

    /* Handle attributes */
    while (parser_match(p, TOKEN_AT)) {
        parse_attribute(p);
    }

    /* let declaration */
    if (parser_match(p, TOKEN_KW_LET)) {
        bool is_mut = parser_match(p, TOKEN_KW_MUT);
        if (!parser_check(p, TOKEN_IDENT)) {
            parser_error(p, p->current, "expected variable name in let");
            return NULL;
        }
        Token name_tok = p->current; parser_advance(p);

        AstNode *type = NULL;
        if (parser_match(p, TOKEN_COLON)) {
            type = parse_type(p);
        }

        AstNode *value = NULL;
        if (parser_match(p, TOKEN_EQ)) {
            value = parse_expr(p);
        }

        return node_let(p->arena, name_tok.loc,
            node_ident(p->arena, name_tok.loc, name_tok.text),
            type, value, is_mut);
    }

    /* if statement */
    if (parser_match(p, TOKEN_KW_IF)) {
        /* if let pattern = expr { body } — pattern binding */
        if (parser_match(p, TOKEN_KW_LET)) {
            AstNode *pattern = parse_pattern(p);
            parser_expect(p, TOKEN_EQ, "if let");
            AstNode *value = parse_expr(p);
            AstNode *body = NULL;
            if (parser_match(p, TOKEN_LBRACE)) {
                body = parse_block_braced(p);
            } else {
                body = parse_block(p);
            }
            AstNode *ifnode = node_if(p->arena, p->previous.loc, value, body, NULL, NULL);
            ifnode->data.if_node.is_if_let = true;
            ifnode->data.if_node.if_let_pattern = pattern;
            return ifnode;
        }

        AstNode *cond = parse_expr(p);
        AstNode *then_block = NULL;

        /* Handle block (braced or indented) */
        if (parser_check(p, TOKEN_LBRACE)) {
            parser_advance(p);
            then_block = parse_block_braced(p);
        } else {
            /* Indented block follows */
            parser_advance(p); /* consume newline? */
            then_block = parse_block(p);
        }

        /* elif chain */
        AstNode *elif_chain = NULL;
        AstNode *current_elif = NULL;
        while (parser_match(p, TOKEN_KW_ELIF)) {
            AstNode *elif_cond = parse_expr(p);
            AstNode *elif_block = NULL;
            if (parser_check(p, TOKEN_LBRACE)) {
                parser_advance(p);
                elif_block = parse_block_braced(p);
            }
            AstNode *elif_node = node_if(p->arena, p->previous.loc, elif_cond, elif_block, NULL, NULL);
            if (!elif_chain) {
                elif_chain = elif_node;
                current_elif = elif_node;
            } else {
                current_elif->data.if_node.elif_chain = elif_node;
                current_elif = elif_node;
            }
        }

        /* else block */
        AstNode *else_block = NULL;
        if (parser_match(p, TOKEN_KW_ELSE)) {
            if (parser_check(p, TOKEN_LBRACE)) {
                parser_advance(p);
                else_block = parse_block_braced(p);
            } else {
                else_block = parse_block(p);
            }
        }

        return node_if(p->arena, p->previous.loc, cond, then_block, elif_chain, else_block);
    }

    /* while loop */
    if (parser_match(p, TOKEN_KW_WHILE)) {
        AstNode *cond = parse_expr(p);
        AstNode *body = NULL;
        if (parser_check(p, TOKEN_LBRACE)) {
            parser_advance(p);
            body = parse_block_braced(p);
        }
        return node_while(p->arena, p->previous.loc, cond, body);
    }

    /* for loop */
    if (parser_match(p, TOKEN_KW_FOR)) {
        AstNode *var = NULL;
        if (parser_check(p, TOKEN_IDENT)) {
            Token v = p->current; parser_advance(p);
            var = node_ident(p->arena, v.loc, v.text);
        }

        AstNode *iterable = NULL;
        if (parser_match(p, TOKEN_KW_IN)) {
            iterable = parse_expr(p);
        }

        AstNode *body = NULL;
        if (parser_check(p, TOKEN_LBRACE)) {
            parser_advance(p);
            body = parse_block_braced(p);
        }

        return node_for(p->arena, p->previous.loc, var, iterable, body);
    }

    /* return */
    if (parser_match(p, TOKEN_KW_RETURN)) {
        AstNode *value = NULL;
        if (!parser_check(p, TOKEN_NEWLINE) && !parser_check(p, TOKEN_RBRACE) &&
            !parser_check(p, TOKEN_EOF)) {
            value = parse_expr(p);
        }
        return node_return(p->arena, p->previous.loc, value);
    }

    /* break / continue */
    if (parser_match(p, TOKEN_KW_BREAK)) return node_break(p->arena, p->previous.loc);
    if (parser_match(p, TOKEN_KW_CONTINUE)) return node_continue(p->arena, p->previous.loc);

    /* defer */
    if (parser_match(p, TOKEN_KW_DEFER)) {
        AstNode *body;
        if (parser_match(p, TOKEN_LBRACE)) {
            body = parse_block_braced(p);
        } else {
            body = parse_statement(p);
        }
        return node_defer(p->arena, p->previous.loc, body);
    }

    /* region("name") { body } */
    if (parser_match(p, TOKEN_KW_REGION)) {
        parser_expect(p, TOKEN_LPAREN, "region name");
        StringView name;
        if (parser_check(p, TOKEN_STRING_LITERAL)) {
            name = p->current.text;
            parser_advance(p);
        } else {
            name = SV("anon");
        }
        parser_expect(p, TOKEN_RPAREN, "region name");
        AstNode *body = NULL;
        if (parser_match(p, TOKEN_LBRACE)) {
            body = parse_block_braced(p);
        } else {
            body = parse_statement(p);
        }
        return node_region(p->arena, p->previous.loc, name, body);
    }

    /* match */
    if (parser_match(p, TOKEN_KW_MATCH)) {
        AstNode *value = parse_expr(p);
        AstNode *match_node = node_match(p->arena, p->previous.loc, value);

        if (parser_match(p, TOKEN_LBRACE)) {
            while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
                if (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON) ||
                    parser_match(p, TOKEN_INDENT) || parser_match(p, TOKEN_DEDENT)) continue;
                
                /* Skip optional 'case' keyword */
                parser_match(p, TOKEN_KW_CASE);
                AstNode *pattern = parse_pattern(p);
                AstNode *body = NULL;
                if (parser_match(p, TOKEN_ARROW)) {
                    /* `=>` token? Use ARROW (->) */
                    body = parse_expr(p);
                }
                AstNode *arm = node_match_arm(p->arena, pattern->loc, pattern, body);
                node_list_append(&match_node->data.match_node.arms, arm);
            }
            parser_expect(p, TOKEN_RBRACE, "match body");
        }

        return match_node;
    }

    /* asm block */
    if (parser_match(p, TOKEN_KW_ASM)) {
        if (parser_match(p, TOKEN_LBRACE)) {
            /* Skip tokens until matching } counting brace depth */
            int brace_depth = 1;
            while (brace_depth > 0 && !parser_check(p, TOKEN_EOF)) {
                parser_advance(p);
                if (p->previous.type == TOKEN_LBRACE) brace_depth++;
                if (p->previous.type == TOKEN_RBRACE) brace_depth--;
            }
            return node_create(p->arena, NODE_ASM_BLOCK, p->previous.loc);
        } else {
            parser_error(p, p->current, "asm block requires { }");
            return NULL;
        }
    }

    /* unsafe block */
    if (parser_match(p, TOKEN_KW_UNSAFE)) {
        return parse_statement(p);
    }

    /* try { body } catch Type(var) { handler } ... */
    if (parser_match(p, TOKEN_KW_TRY)) {
        AstNode *body = NULL;
        if (parser_match(p, TOKEN_LBRACE)) {
            body = parse_block_braced(p);
        } else {
            body = parse_block(p);
        }
        AstNode *try_node = node_try(p->arena, p->previous.loc, body);

        /* Parse catch arms */
        while (parser_match(p, TOKEN_KW_CATCH)) {
            AstNode *catch_type = NULL;
            AstNode *catch_var = NULL;

            /* catch Type or catch Type(var) */
            if (parser_check(p, TOKEN_IDENT)) {
                Token type_tok = p->current; parser_advance(p);
                catch_type = node_type_named(p->arena, type_tok.loc, type_tok.text);

                /* Optional variable binding: catch Type(var) */
                if (parser_match(p, TOKEN_LPAREN)) {
                    if (parser_check(p, TOKEN_IDENT)) {
                        Token var_tok = p->current; parser_advance(p);
                        catch_var = node_ident(p->arena, var_tok.loc, var_tok.text);
                    }
                    parser_expect(p, TOKEN_RPAREN, "catch variable");
                }
            }

            AstNode *catch_body = NULL;
            if (parser_match(p, TOKEN_LBRACE)) {
                catch_body = parse_block_braced(p);
            } else {
                catch_body = parse_block(p);
            }

            AstNode *arm = node_catch_arm(p->arena, p->previous.loc,
                catch_type, catch_var, catch_body);
            node_list_append(&try_node->data.try_node.catch_arms, arm);
        }

        return try_node;
    }

    /* throw expr */
    if (parser_match(p, TOKEN_KW_THROW)) {
        AstNode *value = NULL;
        if (!parser_check(p, TOKEN_NEWLINE) && !parser_check(p, TOKEN_RBRACE) &&
            !parser_check(p, TOKEN_EOF) && !parser_check(p, TOKEN_DEDENT)) {
            value = parse_expr(p);
        }
        return node_throw(p->arena, p->previous.loc, value);
    }

    /* Expression statement */
    AstNode *expr = parse_expr(p);
    if (expr) {
        return node_expr_stmt(p->arena, expr->loc, expr);
    }

    return NULL;
}

/* ================================================================
 * Block parsing (indentation-based)
 * ================================================================ */

AstNode *parse_block(Parser *p) {
    AstNode *block = node_block(p->arena, NO_LOCATION);

    /* Consume statements until dedent or EOF */
    while (!parser_check(p, TOKEN_EOF)) {
        /* Skip newlines */
        while (parser_match(p, TOKEN_NEWLINE));

        /* Dedent or EOF ends the block */
        if (parser_check(p, TOKEN_DEDENT) || parser_check(p, TOKEN_RBRACE) ||
            parser_check(p, TOKEN_EOF)) {
            break;
        }

        AstNode *stmt = parse_statement(p);
        if (stmt) {
            node_list_append(&block->data.list, stmt);
        }
    }

    return block;
}

AstNode *parse_block_braced(Parser *p) {
    AstNode *block = node_block(p->arena, p->previous.loc);

    while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
        /* Skip newlines, semicolons, and indent/dedent tokens */
        while (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON) ||
               parser_match(p, TOKEN_INDENT) || parser_match(p, TOKEN_DEDENT));

        if (parser_check(p, TOKEN_RBRACE)) break;

        AstNode *stmt = parse_statement(p);
        if (stmt) {
            node_list_append(&block->data.list, stmt);
        }
    }

    parser_expect(p, TOKEN_RBRACE, "block");
    return block;
}

/* ================================================================
 * Pattern matching
 * ================================================================ */

AstNode *parse_pattern(Parser *p) {
    /* Simple patterns for now: literals, identifiers, wildcards */
    if (parser_match(p, TOKEN_KW_TRUE)) return node_bool_literal(p->arena, p->previous.loc, true);
    if (parser_match(p, TOKEN_KW_FALSE)) return node_bool_literal(p->arena, p->previous.loc, false);
    if (parser_match(p, TOKEN_KW_NONE)) return node_none_literal(p->arena, p->previous.loc);

    if (parser_check(p, TOKEN_INT_LITERAL)) {
        Token t = p->current; parser_advance(p);
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

    parser_error(p, p->current, "expected pattern");
    return NULL;
}

/* ================================================================
 * Type parsing
 * ================================================================ */

AstNode *parse_type_annotation(Parser *p) {
    if (parser_match(p, TOKEN_COLON)) {
        return parse_type(p);
    }
    return NULL;
}

/* Forward declarations for type parsing */
static AstNode *parse_type_base(Parser *p);
static AstNode *parse_type_postfix(Parser *p, AstNode *base);

AstNode *parse_type(Parser *p) {
    AstNode *base = parse_type_base(p);
    return parse_type_postfix(p, base);
}

/* Forward declarations for type parsing */
static AstNode *parse_type_base(Parser *p);
static AstNode *parse_type_postfix(Parser *p, AstNode *base);

static AstNode *parse_type_base(Parser *p) {
    /* Primitive types */
    if (parser_check(p, TOKEN_IDENT)) {
        StringView name = p->current.text;
        TokenType tt = p->current.type;

        PrimType prim;
        bool is_prim = true;

        if (sv_eq_cstr(name, "void")) prim = PRIM_VOID;
        else if (sv_eq_cstr(name, "bool")) prim = PRIM_BOOL;
        else if (sv_eq_cstr(name, "byte")) prim = PRIM_BYTE;
        else if (sv_eq_cstr(name, "u8")) prim = PRIM_U8;
        else if (sv_eq_cstr(name, "u16")) prim = PRIM_U16;
        else if (sv_eq_cstr(name, "u32")) prim = PRIM_U32;
        else if (sv_eq_cstr(name, "u64")) prim = PRIM_U64;
        else if (sv_eq_cstr(name, "i8")) prim = PRIM_I8;
        else if (sv_eq_cstr(name, "i16")) prim = PRIM_I16;
        else if (sv_eq_cstr(name, "i32")) prim = PRIM_I32;
        else if (sv_eq_cstr(name, "i64")) prim = PRIM_I64;
        else if (sv_eq_cstr(name, "f32")) prim = PRIM_F32;
        else if (sv_eq_cstr(name, "f64")) prim = PRIM_F64;
        else if (sv_eq_cstr(name, "string")) prim = PRIM_STRING;
        else is_prim = false;

        if (is_prim) {
            parser_advance(p);
            return node_type_prim(p->arena, p->previous.loc, prim);
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
        if (parser_match(p, TOKEN_COMMA)) {
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

    /* ptr T — raw pointer (parsed as identifier in parse_type_base) */

    parser_error(p, p->current, "expected type");
    parser_advance(p); /* skip */
    return NULL;
}

/* Postfix type modifiers — called after a base type is parsed */
static AstNode *parse_type_postfix(Parser *p, AstNode *base) {
    /* T? (optional) */
    if (parser_match(p, TOKEN_QUESTION)) {
        AstNode *t = node_create(p->arena, NODE_TYPE_OPTIONAL, p->previous.loc);
        t->data.type_node.elem_type = base;
        return t;
    }
    return base;
}

/* ================================================================
 * Attribute parsing (e.g., @export, @entry(...))
 * ================================================================ */

AstNode *parse_attribute(Parser *p) {
    if (parser_check(p, TOKEN_IDENT)) {
        Token t = p->current; parser_advance(p);
        AstNode *attr = node_create(p->arena, NODE_ATTR, t.loc);
        attr->data.ident.name = t.text;

        /* @entry(0x2000000) — attribute with payload */
        if (parser_match(p, TOKEN_LPAREN)) {
            while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
                parser_advance(p);
            }
            parser_expect(p, TOKEN_RPAREN, "attribute");
        }

        return attr;
    }
    return NULL;
}

/* ================================================================
 * Expression parsing (Pratt parser with precedence climbing)
 * ================================================================ */

/* Forward declaration for prefix/infix parsers */
typedef AstNode *(*PrefixParselet)(Parser *p);
typedef AstNode *(*InfixParselet)(Parser *p, AstNode *left);

/* Precedence of a token (returns PREC_MIN if not an operator) */
static Precedence token_precedence(TokenType type) {
    switch (type) {
        case TOKEN_LPAREN: case TOKEN_LBRACKET: case TOKEN_DOT: case TOKEN_COLON_COLON:
            return PREC_CALL;
        case TOKEN_EQ: case TOKEN_PLUS_EQ: case TOKEN_MINUS_EQ:
        case TOKEN_STAR_EQ: case TOKEN_SLASH_EQ:
            return PREC_ASSIGNMENT;
        case TOKEN_PIPE_PIPE: return PREC_LOGICAL_OR;
        case TOKEN_AND_AND: return PREC_LOGICAL_AND;
        case TOKEN_EQ_EQ: case TOKEN_BANG_EQ: case TOKEN_LT:
        case TOKEN_GT: case TOKEN_LT_EQ: case TOKEN_GT_EQ:
            return PREC_COMPARISON;
        case TOKEN_PIPE: return PREC_BIT_OR;
        case TOKEN_CARET: return PREC_BIT_XOR;
        case TOKEN_AMPERSAND: return PREC_BIT_AND;
        case TOKEN_LT_LT: case TOKEN_GT_GT: return PREC_SHIFT;
        case TOKEN_DOT_DOT: case TOKEN_DOT_DOT_EQ: return PREC_RANGE;
        case TOKEN_PLUS: case TOKEN_MINUS: return PREC_TERM;
        case TOKEN_STAR: case TOKEN_SLASH: case TOKEN_PERCENT: return PREC_FACTOR;
        default: return PREC_MIN;
    }
}

/* Parsing a prefix expression */
static AstNode *parse_prefix(Parser *p) {
    Token token = p->current;
    Location loc = token.loc;

    switch (token.type) {
        case TOKEN_INT_LITERAL:
            parser_advance(p);
            return node_int_literal(p->arena, loc, token.val.int_value);

        case TOKEN_FLOAT_LITERAL:
            parser_advance(p);
            return node_float_literal(p->arena, loc, token.val.float_value);

        case TOKEN_STRING_LITERAL:
            parser_advance(p);
            return node_string_literal(p->arena, loc, token.text);

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

        case TOKEN_IDENT:
            parser_advance(p);
            return node_ident(p->arena, loc, token.text);

        case TOKEN_LPAREN: {
            parser_advance(p);
            /* Could be tuple or parenthesized expression */
            AstNode *expr = parse_expr(p);
            parser_expect(p, TOKEN_RPAREN, "parenthesized expression");
            return expr;
        }

        case TOKEN_LBRACKET: {
            parser_advance(p);
            /* Array literal: [1, 2, 3] */
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
            /* Parse parameter list between pipes */
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
            /* Parse body: either a single expression or a block */
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

        default:
            parser_error(p, token, "expected expression");
            parser_advance(p);
            return NULL;
    }
}

/* Parsing an infix (binary) expression */
static AstNode *parse_infix(Parser *p, AstNode *left, Precedence left_prec) {
    Token token = p->current;
    Location loc = token.loc;

    /* Function call: left(args) */
    if (token.type == TOKEN_LPAREN) {
        parser_advance(p);
        AstNode *call = node_call(p->arena, loc, left);
        while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
            AstNode *arg = parse_expr(p);
            node_list_append(&call->data.call.args, arg);
            if (!parser_match(p, TOKEN_COMMA)) break;
        }
        parser_expect(p, TOKEN_RPAREN, "function call");
        return call;
    }

    /* Index: left[expr] */
    if (token.type == TOKEN_LBRACKET) {
        parser_advance(p);
        /* Check for slice: left[expr..expr] */
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
        /* Build field access as left.field for now — codegen handles it */
        return node_field_access(p->arena, loc, left, node_ident(p->arena, variant.loc, variant.text));
    }

    /* Lambda pipe: left |params| body — only valid in certain contexts */
    if (token.type == TOKEN_PIPE) {
        /* This would be captured at the lambda level */
        return left;
    }

    /* Binary operators */
    BinOp op;
    switch (token.type) {
        case TOKEN_PLUS: op = BIN_ADD; break;
        case TOKEN_MINUS: op = BIN_SUB; break;
        case TOKEN_STAR: op = BIN_MUL; break;
        case TOKEN_SLASH: op = BIN_DIV; break;
        case TOKEN_PERCENT: op = BIN_MOD; break;
        case TOKEN_EQ_EQ: op = BIN_EQ; break;
        case TOKEN_BANG_EQ: op = BIN_NEQ; break;
        case TOKEN_LT: op = BIN_LT; break;
        case TOKEN_GT: op = BIN_GT; break;
        case TOKEN_LT_EQ: op = BIN_LE; break;
        case TOKEN_GT_EQ: op = BIN_GE; break;
        case TOKEN_AND_AND: op = BIN_AND; break;
        case TOKEN_PIPE_PIPE: op = BIN_OR; break;
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
        default:
            return left; /* Not a binary op — return left unchanged */
    }

    Precedence op_prec = token_precedence(token.type);
    parser_advance(p);
    AstNode *right = parse_expr_prec(p, op_prec);
    return node_binary(p->arena, loc, op, left, right);
}

AstNode *parse_expr(Parser *p) {
    return parse_expr_prec(p, PREC_MIN);
}

AstNode *parse_expr_prec(Parser *p, Precedence min_prec) {
    if (!p->has_current) parser_advance(p);
    if (parser_check(p, TOKEN_EOF)) return NULL;

    /* Parse prefix */
    AstNode *left = parse_prefix(p);
    if (!left) return NULL;

    /* Parse infix operators while they have sufficient precedence */
    while (true) {
        if (parser_check(p, TOKEN_EOF) || parser_check(p, TOKEN_NEWLINE) ||
            parser_check(p, TOKEN_RPAREN) || parser_check(p, TOKEN_RBRACKET) ||
            parser_check(p, TOKEN_RBRACE) || parser_check(p, TOKEN_COMMA) ||
            parser_check(p, TOKEN_COLON) || parser_check(p, TOKEN_SEMICOLON) ||
            parser_check(p, TOKEN_DEDENT)) {
            break;
        }

        Precedence prec = token_precedence(p->current.type);
        if (prec < min_prec) break;
        if (prec == PREC_MIN) break;

        left = parse_infix(p, left, prec);
        if (!left) break;
    }

    return left;
}

/* ================================================================
 * Full parse entry
 * ================================================================ */

/* For compile — just parse() is the main entry */
/* parser_parse is the entry point defined above */
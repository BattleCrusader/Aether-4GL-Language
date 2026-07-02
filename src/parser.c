#include "aether/parser.h"
#include "aether/str.h"
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

Parser *parser_create_with_arena(const char *source, size_t length, const char *filename, Arena *arena) {
    Parser *p = (Parser *)calloc(1, sizeof(Parser));
    if (!p) return NULL;

    p->lexer = lexer_create(source, length, filename);
    if (!p->lexer) { free(p); return NULL; }

    p->arena = arena;  /* Use external arena — caller must NOT call arena_destroy on cleanup */
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
    TOKEN_KW_PUB, TOKEN_KW_STATIC,
    TOKEN_KW_UNSAFE, TOKEN_KW_TRY, TOKEN_KW_THROW,
    TOKEN_KW_TYPE,
    TOKEN_KW_SPAWN, TOKEN_KW_YIELD,
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

    /* Skip leading newlines */
    while (parser_match(p, TOKEN_NEWLINE));

    /* Handle attributes like @export, @entry */
    AstNode *last_attr = NULL;
    while (parser_match(p, TOKEN_AT)) {
        last_attr = parse_attribute(p);
        /* Consume newlines after attribute (but not indent/dedent) */
        while (parser_match(p, TOKEN_NEWLINE));
    }

    /* Handle pub/private/internal/static/inline modifiers */
    bool is_pub = parser_match(p, TOKEN_KW_PUB);
    bool is_private = parser_match(p, TOKEN_KW_PRIVATE);
    bool is_internal = parser_match(p, TOKEN_KW_INTERNAL);
    bool is_static = parser_match(p, TOKEN_KW_STATIC);
    bool is_inline = parser_match(p, TOKEN_KW_INLINE);
    AccessLevel access = is_private ? ACCESS_PRIVATE : (is_internal ? ACCESS_INTERNAL : ACCESS_PUB);

    if (parser_match(p, TOKEN_KW_FUNC)) {
        AstNode *func = parse_func_decl(p);
        if (func) {
            func->data.func.access = access;
            func->data.func.is_pub = is_pub;
            func->data.func.is_static = is_static;
            func->data.func.is_inline = is_inline;
            /* Apply @export if the last attribute was export */
            if (last_attr) {
                const char *aname = arena_strndup(p->arena,
                    last_attr->data.ident.name.data,
                    last_attr->data.ident.name.len);
                if (strcmp(aname, "export") == 0) {
                    func->data.func.is_exported = true;
                } else if (strcmp(aname, "force_inline") == 0) {
                    func->data.func.is_force_inline = true;
                } else if (strcmp(aname, "no_inline") == 0) {
                    func->data.func.is_no_inline = true;
                } else if (strcmp(aname, "entry") == 0) {
                    func->data.func.entry_addr = last_attr->data.attr.int_value;
                } else if (strcmp(aname, "layout") == 0) {
                    func->data.func.has_layout = true;
                    func->data.func.layout_start = (uint64_t)last_attr->data.attr.layout_start;
                    func->data.func.layout_max = (uint64_t)last_attr->data.attr.layout_max;
                    func->data.func.layout_bits = last_attr->data.attr.layout_bits;
                    func->data.func.layout_signature = last_attr->data.attr.layout_signature;
                    func->data.func.layout_file = last_attr->data.attr.layout_file;
                } else if (strcmp(aname, "kernel_layout") == 0) {
                    func->data.func.is_kernel_layout = true;
                } else if (strcmp(aname, "test") == 0) {
                    func->data.func.has_test = true;
                }
            }
            node_list_append(decls, func);
        }
    } else if (parser_match(p, TOKEN_KW_SYS)) {
        /* sys func name() at(N) — syscall page declaration */
        if (parser_match(p, TOKEN_KW_FUNC)) {
            AstNode *func = parse_func_decl(p);
            if (func) {
                func->data.func.is_sys = true;
                func->data.func.access = access;
                func->data.func.is_pub = is_pub;
                /* Parse optional at(N) for syscall table index */
                if (parser_match(p, TOKEN_KW_AT)) {
                    parser_expect(p, TOKEN_LPAREN, "syscall index");
                    if (parser_check(p, TOKEN_INT_LITERAL)) {
                        func->data.func.sys_index = (int)p->current.val.int_value;
                        parser_advance(p);
                    }
                    parser_expect(p, TOKEN_RPAREN, "syscall index");
                }
                node_list_append(decls, func);
            }
        } else {
            parser_error(p, p->current, "expected 'func' after 'sys'");
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
    } else if (parser_match(p, TOKEN_KW_LET)) {
        /* let [mut] name[: type] [= expr] — file-scope variable */
        bool is_mut = parser_match(p, TOKEN_KW_MUT);
        if (!parser_check(p, TOKEN_IDENT)) {
            parser_error(p, p->current, "expected variable name in let");
            return;
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
        AstNode *node = node_let(p->arena, name_tok.loc,
            node_ident(p->arena, name_tok.loc, name_tok.text),
            type, value, is_mut);
        node->data.let_decl.is_global = true;
        node_list_append(decls, node);
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
        mod->data.module_decl.name = node_ident(p->arena, name.loc, name.text);
        mod->data.module_decl.module_abi_version = -1;
        if (parser_check(p, TOKEN_LBRACE)) {
            parser_advance(p);
            /* parse module body — decls go to top-level for codegen compatibility */
            while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
                while (parser_match(p, TOKEN_NEWLINE));
                parse_declaration(p, decls);
                while (parser_match(p, TOKEN_NEWLINE));
            }
            parser_expect(p, TOKEN_RBRACE, "module body");
        }
        /* Apply @module_abi(version=N) attribute */
        if (last_attr) {
            const char *aname = arena_strndup(p->arena,
                last_attr->data.ident.name.data,
                last_attr->data.ident.name.len);
            if (strcmp(aname, "module_abi") == 0) {
                if (last_attr->data.attr.has_module_abi) {
                    mod->data.module_decl.module_abi_version = (int)last_attr->data.attr.module_abi_version;
                } else {
                    parser_error(p, name, "@module_abi requires version=N argument");
                }
            }
        }
        node_list_append(decls, mod);
    } else if (parser_match(p, TOKEN_KW_ASM)) {
        /* Top-level asm block — parse as raw assembly, not inside a function */
        /* Capture the raw text between { and } */
        if (parser_match(p, TOKEN_LBRACE)) {
            const char *asm_start = p->lexer->tok->start;
            int depth = 1;
            while (depth > 0 && !parser_check(p, TOKEN_EOF)) {
                if (parser_check(p, TOKEN_LBRACE)) depth++;
                if (parser_check(p, TOKEN_RBRACE)) depth--;
                if (depth > 0) parser_advance(p);
            }
            const char *asm_end = p->lexer->tok->start;
            if (asm_end > asm_start) {
                /* Trim trailing whitespace/brace */
                while (asm_end > asm_start && (asm_end[-1] == ' ' || asm_end[-1] == '\t' ||
                       asm_end[-1] == '\n' || asm_end[-1] == '\r' || asm_end[-1] == '}'))
                    asm_end--;
            }
            AstNode *node = node_create(p->arena, NODE_ASM_BLOCK, LOCATION(p->lexer->tok->filename, 0, 0, 0));
            if (asm_end > asm_start) {
                StringView sv;
                sv.data = asm_start;
                sv.len = (size_t)(asm_end - asm_start);
                node->data.asm_block.text = node_string_literal(p->arena, NO_LOCATION, sv);
            }
            node_list_append(decls, node);
            parser_expect(p, TOKEN_RBRACE, "top-level asm block");
        } else {
            parser_error(p, p->current, "expected '{' after 'asm' at top level");
        }
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
                        parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_NEWLINE)) continue;
                    if (parser_match(p, TOKEN_KW_FUNC)) {
                        AstNode *method = parse_func_decl(p);
                        if (method) node_list_append(&t->data.trait_decl.methods, method);
                    } else { parser_advance(p); }
                }
                parser_expect(p, TOKEN_RBRACE, "trait body");
            }
            node_list_append(decls, t);
        }
    } else if (parser_match(p, TOKEN_KW_POOL)) {
        /* pool Name of size N, count M, alignment A */
        if (parser_check(p, TOKEN_IDENT)) {
            Token name_tok = p->current; parser_advance(p);
            AstNode *pool_node = node_create(p->arena, NODE_POOL_DECL, name_tok.loc);
            pool_node->data.pool_decl.name = node_ident(p->arena, name_tok.loc, name_tok.text);
            pool_node->data.pool_decl.size = 0;
            pool_node->data.pool_decl.count = 0;
            pool_node->data.pool_decl.alignment = 0;

            /* Parse optional "of size N, count M, alignment A" */
            if (parser_check(p, TOKEN_IDENT) && 
                sv_eq_cstr(p->current.text, "of")) {
                parser_advance(p); /* consume "of" */
                if (parser_check(p, TOKEN_IDENT) &&
                    sv_eq_cstr(p->current.text, "size")) {
                    parser_advance(p); /* consume "size" */
                    if (parser_check(p, TOKEN_INT_LITERAL)) {
                        pool_node->data.pool_decl.size = p->current.val.int_value;
                        parser_advance(p);
                    }
                }
                /* Optional ", count M" */
                if (parser_match(p, TOKEN_COMMA)) {
                    if (parser_check(p, TOKEN_IDENT) &&
                        sv_eq_cstr(p->current.text, "count")) {
                        parser_advance(p);
                        if (parser_check(p, TOKEN_INT_LITERAL)) {
                            pool_node->data.pool_decl.count = p->current.val.int_value;
                            parser_advance(p);
                        }
                    }
                }
                /* Optional ", alignment A" */
                if (parser_match(p, TOKEN_COMMA)) {
                    if (parser_check(p, TOKEN_IDENT) &&
                        sv_eq_cstr(p->current.text, "alignment")) {
                        parser_advance(p);
                        if (parser_check(p, TOKEN_INT_LITERAL)) {
                            pool_node->data.pool_decl.alignment = p->current.val.int_value;
                            parser_advance(p);
                        }
                    }
                }
            }

            node_list_append(decls, pool_node);
        } else {
            parser_error(p, p->current, "expected pool name");
        }
    } else if (parser_match(p, TOKEN_KW_PROTOCOL)) {
        /* protocol Name { func_decls } */
        if (parser_check(p, TOKEN_IDENT)) {
            Token name_tok = p->current; parser_advance(p);
            AstNode *proto = node_create(p->arena, NODE_PROTOCOL_DECL, name_tok.loc);
            proto->data.protocol_decl.name = node_ident(p->arena, name_tok.loc, name_tok.text);
            if (parser_match(p, TOKEN_LBRACE)) {
                while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
                    if (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON) ||
                        parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_NEWLINE)) continue;
                    if (parser_match(p, TOKEN_KW_FUNC)) {
                        AstNode *method = parse_func_decl(p);
                        if (method) node_list_append(&proto->data.protocol_decl.methods, method);
                    } else { parser_advance(p); }
                }
                parser_expect(p, TOKEN_RBRACE, "protocol body");
            }
            node_list_append(decls, proto);
        } else {
            parser_error(p, p->current, "expected protocol name");
        }
    } else if (parser_match(p, TOKEN_KW_TYPE)) {
        /* type Name = ExistingType */
        if (parser_check(p, TOKEN_IDENT)) {
            Token name_tok = p->current; parser_advance(p);
            AstNode *alias = node_create(p->arena, NODE_TYPE_ALIAS, name_tok.loc);
            /* Store name as first item in list, underlying type as second */
            AstNode *name_node = node_ident(p->arena, name_tok.loc, name_tok.text);
            node_list_append(&alias->data.list, name_node);
            if (parser_match(p, TOKEN_EQ)) {
                AstNode *underlying = parse_type(p);
                if (underlying) {
                    node_list_append(&alias->data.list, underlying);
                }
            }
            node_list_append(decls, alias);
        } else {
            parser_error(p, p->current, "expected type alias name");
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
                            parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_NEWLINE)) continue;
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
                        parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_NEWLINE)) continue;
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
                    /* Auto-inject 'self' as the first parameter for methods.
                     * The user writes: func fahrenheit(): f64 { return self.celsius * ... }
                     * The parser adds: self: ref StructName as the first param. */
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
                body = parse_block_braced(p);
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
            then_block = parse_block_braced(p);
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
                else_block = parse_block_braced(p);
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
        AstNode *index_var = NULL;
        if (parser_check(p, TOKEN_IDENT)) {
            Token v = p->current; parser_advance(p);
            var = node_ident(p->arena, v.loc, v.text);
            /* Check for index+value: for i, val in arr */
            if (parser_match(p, TOKEN_COMMA)) {
                if (parser_check(p, TOKEN_IDENT)) {
                    Token v2 = p->current; parser_advance(p);
                    index_var = var; /* first ident is the index */
                    var = node_ident(p->arena, v2.loc, v2.text);
                }
            }
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

        AstNode *for_node = node_for(p->arena, p->previous.loc, var, iterable, body);
        /* Store index_var in the for_node */
        if (index_var) {
            for_node->data.for_node.index_var = index_var;
        }
        return for_node;
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

    /* break / continue with optional label */
    if (parser_match(p, TOKEN_KW_BREAK)) {
        AstNode *node = node_break(p->arena, p->previous.loc);
        /* Check for label: break outer_label */
        if (parser_check(p, TOKEN_IDENT)) {
            Token label = p->current; parser_advance(p);
            node->data.ident.name = label.text;
        }
        return node;
    }
    if (parser_match(p, TOKEN_KW_CONTINUE)) {
        AstNode *node = node_continue(p->arena, p->previous.loc);
        /* Check for label: continue outer_label */
        if (parser_check(p, TOKEN_IDENT)) {
            Token label = p->current; parser_advance(p);
            node->data.ident.name = label.text;
        }
        return node;
    }

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

    /* spawn expr(args...) — spawn a function call as a new thread/fiber */
    if (parser_match(p, TOKEN_KW_SPAWN)) {
        /* Parse the function call: callee(args) */
        if (!parser_check(p, TOKEN_IDENT) && !parser_check(p, TOKEN_KW_SELF)) {
            parser_error(p, p->current, "expected function name for spawn");
            return NULL;
        }
        AstNode *callee;
        Location loc = p->current.loc;
        if (parser_check(p, TOKEN_KW_SELF)) {
            callee = node_ident(p->arena, p->current.loc, SV("self"));
            parser_advance(p);
        } else {
            callee = node_ident(p->arena, p->current.loc, p->current.text);
            parser_advance(p);
        }
        /* Expect (args...) */
        if (!parser_match(p, TOKEN_LPAREN)) {
            parser_error(p, p->current, "expected '(' for spawn function call");
            return NULL;
        }
        AstNode *call = node_call(p->arena, loc, callee);
        while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
            while (parser_match(p, TOKEN_NEWLINE));
            if (parser_check(p, TOKEN_RPAREN)) break;
            AstNode *arg = parse_expr(p);
            if (arg) node_list_append(&call->data.call.args, arg);
            if (!parser_match(p, TOKEN_COMMA)) break;
        }
        parser_expect(p, TOKEN_RPAREN, "spawn call arguments");
        return node_spawn(p->arena, p->previous.loc, call);
    }

    /* yield — yield control in a fiber context */
    if (parser_match(p, TOKEN_KW_YIELD)) {
        return node_yield(p->arena, p->previous.loc);
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
                    parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_NEWLINE)) continue;
                
                /* Skip optional 'case' keyword */
                parser_match(p, TOKEN_KW_CASE);
                AstNode *pattern = parse_pattern(p);
                AstNode *body = NULL;
                AstNodeList extra_patterns = {0};
                /* Parse comma-separated additional patterns: case 1,2,3 -> */
                /* parse_pattern returns NULL for comma/arrow/newline/brace.
                   If it returned a real pattern, check for comma and parse more. */
                if (pattern) {
                    while (parser_match(p, TOKEN_COMMA)) {
                        /* parser_match consumed the comma. Ensure has_current is set
                           before calling parse_pattern, which checks current token. */
                        if (!p->has_current) parser_advance(p);
                        AstNode *extra = parse_pattern(p);
                        if (extra) node_list_append(&extra_patterns, extra);
                        else break;
                    }
                }
                if (parser_match(p, TOKEN_ARROW)) {
                    /* `=>` token? Use ARROW (->) */
                    body = parse_expr(p);
                }
                AstNode *arm = node_match_arm(p->arena, pattern ? pattern->loc : p->previous.loc, pattern, body);
                arm->data.match_arm.patterns = extra_patterns;
                node_list_append(&match_node->data.match_node.arms, arm);
            }
            parser_expect(p, TOKEN_RBRACE, "match body");
        }

        return match_node;
    }

    /* asm block */
    if (parser_match(p, TOKEN_KW_ASM)) {
        /* Optional output binding: asm: (var1, var2) { ... } */
        AstNodeList outputs = {0};
        if (parser_match(p, TOKEN_COLON)) {
            parser_expect(p, TOKEN_LPAREN, "asm output list");
            while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
                AstNode *var = node_create(p->arena, NODE_IDENT, p->current.loc);
                if (parser_check(p, TOKEN_IDENT)) {
                    var->data.ident.name = p->current.text;
                    parser_advance(p);
                    node_list_append(&outputs, var);
                    parser_match(p, TOKEN_COMMA);
                } else break;
            }
            parser_expect(p, TOKEN_RPAREN, "asm output list");
        }

        if (parser_match(p, TOKEN_LBRACE)) {
            /* Record the source position right after opening brace */
            /* Use the { token's end position (text.data + text.len) */
            const char *asm_start = p->previous.text.data + p->previous.text.len;
            const char *asm_end = asm_start;
            int brace_depth = 1;
            /* Track line/col of the opening brace for error reporting */
            int start_line = p->previous.loc.line;
            int start_col = p->previous.loc.col;

            while (brace_depth > 0 && !parser_check(p, TOKEN_EOF)) {
                parser_advance(p);
                asm_end = p->lexer->tok->pos; /* track end after each token */
                if (p->previous.type == TOKEN_LBRACE) {
                    brace_depth++;
                    asm_end = p->lexer->tok->start;
                }
                if (p->previous.type == TOKEN_RBRACE) {
                    brace_depth--;
                    if (brace_depth == 0) {
                        /* Calculate end — before the closing } */
                        asm_end = p->lexer->tok->start;
                        /* Trim trailing whitespace/newline from asm text */
                        while (asm_end > asm_start &&
                               (asm_end[-1] == '\n' || asm_end[-1] == '\r' ||
                                asm_end[-1] == ' ' || asm_end[-1] == '\t'))
                            asm_end--;
                    }
                }
            }

            /* Create the asm block node with captured text */
            AstNode *node = node_create(p->arena, NODE_ASM_BLOCK, LOCATION(p->lexer->tok->filename, start_line, start_col, 0));
            /* Store the assembly text as a string literal */
            if (asm_end > asm_start) {
                /* Trim trailing closing brace if present */
                const char *trim_end = asm_end;
                while (trim_end > asm_start && (trim_end[-1] == ' ' || trim_end[-1] == '\t' || trim_end[-1] == '\n' || trim_end[-1] == '\r'))
                    trim_end--;
                if (trim_end > asm_start && trim_end[-1] == '}') {
                    trim_end--; /* remove the closing } */
                    /* Trim again after removing brace */
                    while (trim_end > asm_start && (trim_end[-1] == ' ' || trim_end[-1] == '\t' || trim_end[-1] == '\n' || trim_end[-1] == '\r'))
                        trim_end--;
                }
                if (trim_end > asm_start) {
                    StringView asm_text = sv_from_parts(asm_start, (size_t)(trim_end - asm_start));
                    node->data.asm_block.text = node_string_literal(p->arena, NO_LOCATION, asm_text);
                }
            }
            node->data.asm_block.outputs = outputs;
            return node;
        } else {
            parser_error(p, p->current, "asm block requires { }");
            return NULL;
        }
    }

    /* unsafe block */
    if (parser_match(p, TOKEN_KW_UNSAFE)) {
        AstNode *body = NULL;
        if (parser_match(p, TOKEN_LBRACE)) {
            body = parse_block_braced(p);
        } else {
            body = parse_statement(p);
        }
        if (body) {
            AstNode *unsafe_node = node_create(p->arena, NODE_UNSAFE, body->loc);
            /* Wrap the body in the unsafe node's list */
            node_list_append(&unsafe_node->data.list, body);
            return unsafe_node;
        }
        return body;
    }

    /* try { body } catch Type(var) { handler } ... finally { body } */
    if (parser_match(p, TOKEN_KW_TRY)) {
        AstNode *body = NULL;
        if (parser_match(p, TOKEN_LBRACE)) {
            body = parse_block_braced(p);
        } else {
            body = parse_block_braced(p);
        }
        AstNode *try_node = node_try(p->arena, p->previous.loc, body);

        /* Parse catch arms */
        while (parser_match(p, TOKEN_KW_CATCH)) {
            AstNode *catch_type = NULL;
            AstNode *catch_var = NULL;
            bool is_catch_all = false;

            /* catch _ { } — catch-all */
            if (parser_check(p, TOKEN_IDENT) && p->current.text.len == 1 && p->current.text.data[0] == '_') {
                parser_advance(p);
                is_catch_all = true;
            }
            /* catch Type or catch Type(var) */
            else if (parser_check(p, TOKEN_IDENT)) {
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
                catch_body = parse_block_braced(p);
            }

            AstNode *arm = node_catch_arm(p->arena, p->previous.loc,
                catch_type, catch_var, catch_body, is_catch_all);
            node_list_append(&try_node->data.try_node.catch_arms, arm);
        }

        /* Optional finally block */
        if (parser_match(p, TOKEN_KW_FINALLY)) {
            if (parser_match(p, TOKEN_LBRACE)) {
                try_node->data.try_node.finally_body = parse_block_braced(p);
            } else {
                try_node->data.try_node.finally_body = parse_block_braced(p);
            }
        }

        return try_node;
    }

    /* throw expr */
    if (parser_match(p, TOKEN_KW_THROW)) {
        AstNode *value = NULL;
        if (!parser_check(p, TOKEN_NEWLINE) && !parser_check(p, TOKEN_RBRACE) &&
            !parser_check(p, TOKEN_EOF)) {
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
        if (parser_check(p, TOKEN_RBRACE) ||
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
               parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_NEWLINE));

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
    /* Only check if we have a current token; don't advance to find out */
    if (p->has_current) {
        TokenType ct = p->current.type;
        if (ct == TOKEN_COMMA || ct == TOKEN_ARROW || ct == TOKEN_NEWLINE || ct == TOKEN_RBRACE) {
            return NULL;
        }
    } else {
        /* No current token — also end of pattern */
        return NULL;
    }

    /* Only error if we actually have a current token to report */
    if (p->has_current) {
        parser_error(p, p->current, "expected pattern");
    }
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
            /* Fixed-size array: [T; N] — accepts both ; and , as separator */
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
static AstNode *parse_type_postfix(Parser *p, AstNode *base) {
    /* T? (optional) — only consume ? if it's NOT followed by an expression-starting token
       (which would indicate a ternary operator instead) */
    if (parser_check(p, TOKEN_QUESTION)) {
        /* Peek ahead to see what follows the ? */
        Token peek = lexer_peek_next(p->lexer);
        /* Only consume ? as optional if followed by newline, =, ), ,, ;, }, ], or EOF */
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

/* ================================================================
 * Attribute parsing (e.g., @export, @entry(...))
 * ================================================================ */

AstNode *parse_attribute(Parser *p) {
    if (parser_check(p, TOKEN_IDENT) || parser_check(p, TOKEN_KW_EXPORT) ||
        parser_check(p, TOKEN_KW_ENTRY) || parser_check(p, TOKEN_KW_LAYOUT)) {
        Token t = p->current; parser_advance(p);
        AstNode *attr = node_create(p->arena, NODE_ATTR, t.loc);
        attr->data.ident.name = t.text;
        attr->data.attr.name = t.text;
        attr->data.attr.int_value = -1;
        attr->data.attr.has_layout_start = false;
        attr->data.attr.has_layout_max = false;
        attr->data.attr.layout_bits = 0;  /* 0 = default 64 */
        attr->data.attr.layout_signature = 0;  /* 0 = no signature */
        attr->data.attr.layout_file = (StringView){0};
        attr->data.attr.has_module_abi = false;
        attr->data.attr.module_abi_version = -1;
        attr->data.attr.has_test = false;

        /* @name(payload) — parenthesized attribute arguments */
        if (parser_match(p, TOKEN_LPAREN)) {
            /* Try key=value or key:value pairs first (e.g., @layout(start=0x7C00, ...)) */
            if (parser_check(p, TOKEN_IDENT) || parser_check(p, TOKEN_KW_LAYOUT) ||
                parser_check(p, TOKEN_KW_ENTRY)) {
                /* Parse key = value [, key = value] ... */
                while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
                    /* Skip newlines */
                    while (parser_match(p, TOKEN_NEWLINE));
                    if (parser_check(p, TOKEN_RPAREN)) break;

                    /* Parse key — identifier or keyword */
                    StringView key;
                    if (parser_check(p, TOKEN_IDENT)) {
                        key = p->current.text; parser_advance(p);
                    } else {
                        /* keyword as key name */
                        Token kt = p->current; parser_advance(p);
                        key = kt.text;
                    }

                    /* Expect = or : (key=value or key:value) */
                    if (parser_match(p, TOKEN_EQ) || parser_match(p, TOKEN_COLON)) {
                        /* Parse value — int or string */
                        if (parser_check(p, TOKEN_INT_LITERAL)) {
                            uint64_t val = p->current.val.int_value; parser_advance(p);
                            /* Match key name */
                            size_t klen = key.len;
                            if (klen == 5 && strncmp(key.data, "start", 5) == 0) {
                                attr->data.attr.has_layout_start = true;
                                attr->data.attr.layout_start = (int64_t)val;
                            } else if (klen == 3 && strncmp(key.data, "max", 3) == 0) {
                                attr->data.attr.has_layout_max = true;
                                attr->data.attr.layout_max = (int64_t)val;
                            } else if (klen == 4 && strncmp(key.data, "bits", 4) == 0) {
                                attr->data.attr.layout_bits = (int)val;
                            } else if (klen == 9 && strncmp(key.data, "signature", 9) == 0) {
                                attr->data.attr.layout_signature = (int)val;
                            } else if (klen == 7 && strncmp(key.data, "version", 7) == 0) {
                                attr->data.attr.has_module_abi = true;
                                attr->data.attr.module_abi_version = (int64_t)val;
                            }
                        } else if (parser_check(p, TOKEN_STRING_LITERAL)) {
                            /* String value — for @layout(file="name") */
                            StringView sv = p->current.text;
                            attr->data.attr.layout_file = sv;
                            parser_advance(p);
                        } else {
                            /* Skip unknown value */
                            parser_advance(p);
                        }
                    }

                    /* Skip comma (optional before RPAREN) */
                    while (parser_match(p, TOKEN_COMMA));
                    while (parser_match(p, TOKEN_NEWLINE));
                }
            } else if (parser_check(p, TOKEN_INT_LITERAL)) {
                /* Numeric payload — @entry(0x2000000) */
                Token val_tok = p->current; parser_advance(p);
                attr->data.attr.int_value = (int64_t)val_tok.val.int_value;
            } else {
                /* Skip unknown payload tokens */
                while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
                    parser_advance(p);
                }
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

        case TOKEN_STRING_LITERAL: {
            parser_advance(p);
            StringView raw = token.text;
            /* Strip surrounding quotes */
            const char *content = raw.data;
            size_t content_len = raw.len;
            if (content_len >= 2 && content[0] == '"' && content[content_len-1] == '"') {
                content += 1;
                content_len -= 2;
            }
            /* Scan for {expr} interpolation patterns */
            const char *p_start = content;
            const char *p_end = content + content_len;
            const char *cur = content;
            AstNode *result = NULL;
            bool found_interp = false;
            while (cur < p_end) {
                if (*cur == '{') {
                    found_interp = true;
                    /* Emit literal text before this { */
                    size_t lit_len = (size_t)(cur - p_start);
                    if (lit_len > 0) {
                        /* Wrap in quotes for codegen's process_string_literal */
                        char *quoted = (char *)arena_alloc(p->arena, lit_len + 3);
                        quoted[0] = '"';
                        memcpy(quoted + 1, p_start, lit_len);
                        quoted[lit_len + 1] = '"';
                        quoted[lit_len + 2] = '\0';
                        StringView lit_sv = sv_from_parts(quoted, lit_len + 2);
                        AstNode *lit_node = node_string_literal(p->arena, loc, lit_sv);
                        if (result) {
                            result = node_binary(p->arena, loc, BIN_CONCAT, result, lit_node);
                        } else {
                            result = lit_node;
                        }
                    }
                    /* Find matching } */
                    cur++; /* skip { */
                    const char *expr_start = cur;
                    int depth = 1;
                    while (cur < p_end && depth > 0) {
                        if (*cur == '{') depth++;
                        else if (*cur == '}') depth--;
                        if (depth > 0) cur++;
                    }
                    if (depth == 0) {
                        /* Parse expression between { and } */
                        size_t expr_len = (size_t)(cur - expr_start);
                        if (expr_len > 0) {
                            /* Create a temporary parser using the main arena */
                            Parser *sub_p = (Parser *)calloc(1, sizeof(Parser));
                            if (sub_p) {
                                sub_p->lexer = lexer_create(expr_start, expr_len, loc.file);
                                sub_p->arena = p->arena;  /* Use main arena so nodes persist */
                                sub_p->has_current = false;
                                sub_p->error_count = 0;
                                sub_p->panic_mode = false;
                                sub_p->current_scope = NULL;
                                AstNode *expr_node = parse_expr(sub_p);
                                if (expr_node) {
                                    /* If the expression is a function call, wrap it in a
                                       temp variable so codegen evaluates it first. */
                                    if (expr_node->type == NODE_CALL) {
                                        static int interp_temp_counter = 0;
                                        char tmp_name[64];
                                        snprintf(tmp_name, sizeof(tmp_name), "__interp_tmp_%d", interp_temp_counter++);
                                        AstNode *tmp_ident = node_ident(p->arena, loc, sv_from_cstr(tmp_name));
                                        AstNode *tmp_let = node_let(p->arena, loc, tmp_ident, NULL, expr_node, false);
                                        /* Prepend the let to the current function's body */
                                        /* We can't easily do that here, so instead just use the
                                           expression directly — the codegen handles it. */
                                        if (result) {
                                            result = node_binary(p->arena, loc, BIN_CONCAT, result, expr_node);
                                        } else {
                                            result = expr_node;
                                        }
                                    } else {
                                        if (result) {
                                            result = node_binary(p->arena, loc, BIN_CONCAT, result, expr_node);
                                        } else {
                                            result = expr_node;
                                        }
                                    }
                                }
                                lexer_destroy(sub_p->lexer);
                                free(sub_p);
                            }
                        }
                        cur++; /* skip } */
                        p_start = cur;
                    } else {
                        /* Unmatched { — treat as literal */
                        cur = expr_start; /* backtrack to after the { */
                        p_start = cur;
                    }
                } else {
                    cur++;
                }
            }
            if (found_interp) {
                /* Emit trailing literal text */
                size_t lit_len = (size_t)(p_end - p_start);
                if (lit_len > 0) {
                    /* Wrap in quotes for codegen's process_string_literal */
                    char *quoted = (char *)arena_alloc(p->arena, lit_len + 3);
                    quoted[0] = '"';
                    memcpy(quoted + 1, p_start, lit_len);
                    quoted[lit_len + 1] = '"';
                    quoted[lit_len + 2] = '\0';
                    StringView lit_sv = sv_from_parts(quoted, lit_len + 2);
                    AstNode *lit_node = node_string_literal(p->arena, loc, lit_sv);
                    if (result) {
                        result = node_binary(p->arena, loc, BIN_CONCAT, result, lit_node);
                    } else {
                        result = lit_node;
                    }
                }
                if (!result) {
                    /* Empty string interpolation like "{}" — emit empty string */
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
                    if (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON) ||
                        parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_NEWLINE)) continue;

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
            /* Optional arrow before body expression */
            parser_match(p, TOKEN_ARROW);
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

        case TOKEN_HASH:
            /* #expr — array length prefix operator */
            parser_advance(p);
            return node_unary(p->arena, loc, UNARY_ARRAY_LEN, parse_expr_prec(p, PREC_UNARY));

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
            /* Skip newlines between arguments */
            while (parser_match(p, TOKEN_NEWLINE));
            if (parser_check(p, TOKEN_RPAREN)) break;
            /* Handle empty arg: , , or leading , — means "none" for optional variadics */
            if (parser_check(p, TOKEN_COMMA)) {
                /* Empty argument — emit a NONE literal (0) */
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

    /* Error context: throws_call ? "message" — ? followed by string literal */
    if (token.type == TOKEN_QUESTION) {
        /* Peek ahead: if next token is a string literal, it's error context */
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
        /* Store in data.list: items[0]=cond, items[1]=then_val, items[2]=else_val */
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
            return left; /* Not a binary op — return left unchanged */
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

    /* Parse prefix */
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
    while (true) {
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
    }

    return left;
}

/* ================================================================
 * Full parse entry
 * ================================================================ */

/* For compile — just parse() is the main entry */
/* parser_parse is the entry point defined above */
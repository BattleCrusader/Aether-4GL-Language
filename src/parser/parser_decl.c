#include "aether/parser.h"
#include "aether/parser_internal.h"
#include "aether/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

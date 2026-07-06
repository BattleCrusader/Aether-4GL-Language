#include "aether/parser.h"
#include "aether/parser_internal.h"
#include "aether/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

        /* Handle block (braced) or single statement */
        if (parser_check(p, TOKEN_LBRACE)) {
            parser_advance(p);
            then_block = parse_block_braced(p);
        } else {
            /* Single-statement body: if cond stmt */
            AstNode *stmt = parse_statement(p);
            if (stmt) {
                then_block = node_block(p->arena, stmt->loc);
                node_list_append(&then_block->data.list, stmt);
            }
        }

        /* elif chain */
        AstNode *elif_chain = NULL;
        AstNode *current_elif = NULL;
        /* Skip newlines between } and elif/else */
        while (parser_match(p, TOKEN_NEWLINE));
        while (parser_match(p, TOKEN_KW_ELIF)) {
            AstNode *elif_cond = parse_expr(p);
            AstNode *elif_block = NULL;
            if (parser_check(p, TOKEN_LBRACE)) {
                parser_advance(p);
                elif_block = parse_block_braced(p);
            } else {
                AstNode *stmt = parse_statement(p);
                if (stmt) {
                    elif_block = node_block(p->arena, stmt->loc);
                    node_list_append(&elif_block->data.list, stmt);
                }
            }
            AstNode *elif_node = node_if(p->arena, p->previous.loc, elif_cond, elif_block, NULL, NULL);
            if (!elif_chain) {
                elif_chain = elif_node;
                current_elif = elif_node;
            } else {
                current_elif->data.if_node.elif_chain = elif_node;
                current_elif = elif_node;
            }
            /* Skip newlines before next elif/else */
            while (parser_match(p, TOKEN_NEWLINE));
        }

        /* else block */
        AstNode *else_block = NULL;
        /* Skip newlines before else */
        while (parser_match(p, TOKEN_NEWLINE));
        if (parser_match(p, TOKEN_KW_ELSE)) {
            if (parser_check(p, TOKEN_LBRACE)) {
                parser_advance(p);
                else_block = parse_block_braced(p);
            } else {
                AstNode *stmt = parse_statement(p);
                if (stmt) {
                    else_block = node_block(p->arena, stmt->loc);
                    node_list_append(&else_block->data.list, stmt);
                }
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
        } else {
            AstNode *stmt = parse_statement(p);
            if (stmt) {
                body = node_block(p->arena, stmt->loc);
                node_list_append(&body->data.list, stmt);
            }
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
            while (true) {
                if (!p->has_current) parser_advance(p);
                if (p->current.type == TOKEN_RBRACE || p->current.type == TOKEN_EOF) break;
                if (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON)) continue;
                AstNode *arm = parse_match_arm(p);
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
            const char *asm_start = p->previous.text.data + p->previous.text.len;
            const char *asm_end = asm_start;
            int brace_depth = 1;
            int start_line = p->previous.loc.line;
            int start_col = p->previous.loc.col;

            while (brace_depth > 0 && !parser_check(p, TOKEN_EOF)) {
                parser_advance(p);
                asm_end = p->lexer->tok->pos;
                if (p->previous.type == TOKEN_LBRACE) {
                    brace_depth++;
                    asm_end = p->lexer->tok->start;
                }
                if (p->previous.type == TOKEN_RBRACE) {
                    brace_depth--;
                    if (brace_depth == 0) {
                        asm_end = p->lexer->tok->start;
                        while (asm_end > asm_start &&
                               (asm_end[-1] == '\n' || asm_end[-1] == '\r' ||
                                asm_end[-1] == ' ' || asm_end[-1] == '\t'))
                            asm_end--;
                    }
                }
            }

            AstNode *node = node_create(p->arena, NODE_ASM_BLOCK, LOCATION(p->lexer->tok->filename, start_line, start_col, 0));
            if (asm_end > asm_start) {
                const char *trim_end = asm_end;
                while (trim_end > asm_start && (trim_end[-1] == ' ' || trim_end[-1] == '\t' || trim_end[-1] == '\n' || trim_end[-1] == '\r'))
                    trim_end--;
                if (trim_end > asm_start && trim_end[-1] == '}') {
                    trim_end--;
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

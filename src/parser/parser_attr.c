#include "aether/parser.h"
#include "aether/parser_internal.h"
#include "aether/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        attr->data.attr.layout_bits = 0;
        attr->data.attr.layout_signature = 0;
        attr->data.attr.layout_file = (StringView){0};
        attr->data.attr.has_module_abi = false;
        attr->data.attr.module_abi_version = -1;
        attr->data.attr.has_test = false;

        /* @name(payload) — parenthesized attribute arguments */
        if (parser_match(p, TOKEN_LPAREN)) {
            /* Try key=value or key:value pairs first */
            if (parser_check(p, TOKEN_IDENT) || parser_check(p, TOKEN_KW_LAYOUT) ||
                parser_check(p, TOKEN_KW_ENTRY)) {
                while (!parser_check(p, TOKEN_RPAREN) && !parser_check(p, TOKEN_EOF)) {
                    while (parser_match(p, TOKEN_NEWLINE));
                    if (parser_check(p, TOKEN_RPAREN)) break;

                    StringView key;
                    if (parser_check(p, TOKEN_IDENT)) {
                        key = p->current.text; parser_advance(p);
                    } else {
                        Token kt = p->current; parser_advance(p);
                        key = kt.text;
                    }

                    if (parser_match(p, TOKEN_EQ) || parser_match(p, TOKEN_COLON)) {
                        if (parser_check(p, TOKEN_INT_LITERAL)) {
                            uint64_t val = p->current.val.int_value; parser_advance(p);
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
                            StringView sv = p->current.text;
                            attr->data.attr.layout_file = sv;
                            parser_advance(p);
                        } else {
                            parser_advance(p);
                        }
                    }

                    while (parser_match(p, TOKEN_COMMA));
                    while (parser_match(p, TOKEN_NEWLINE));
                }
            } else if (parser_check(p, TOKEN_INT_LITERAL)) {
                /* Numeric payload — @entry(0x2000000) */
                Token val_tok = p->current; parser_advance(p);
                attr->data.attr.int_value = (int64_t)val_tok.val.int_value;
            } else {
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

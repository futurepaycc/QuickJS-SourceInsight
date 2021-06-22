//
// Created by benpeng.jiang on 2021/6/22.
//

#ifndef LOX_JS_FONT_LEXER_H
#define LOX_JS_FONT_LEXER_H

#include "front-token.h"

int __attribute__((format(printf, 2, 3))) js_parse_error(JSParseState *s, const char *fmt, ...);

__exception int ident_realloc(JSContext *ctx, char **pbuf, size_t *psize,
                              char *static_buf);

__exception int js_parse_string(JSParseState *s, int sep,
                                BOOL do_throw, const uint8_t *p,
                                JSToken *token, const uint8_t **pp);



int js_parse_expect_semi(JSParseState *s);

int js_parse_error_reserved_identifier(JSParseState *s);

__exception int js_parse_template_part(JSParseState *s, const uint8_t *p);

int js_parse_expect(JSParseState *s, int tok);

__exception int next_token(JSParseState *s);

#endif //LOX_JS_FONT_LEXER_H

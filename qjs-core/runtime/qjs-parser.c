//
// Created by benpeng.jiang on 2021/6/3.
//

#include "qjs-parser.h"

static __exception int next_token(JSParseState *s)
{
    printf("filename: %s : %s", s->filename, s->buf_ptr);

    fail:
    s->token.val = TOK_ERROR;
    return -1;
}

void js_parse_init(JSContext *ctx, JSParseState *s,
                          const char *input, size_t input_len,
                          const char *filename)
{
    memset(s, 0, sizeof(*s));
    s->ctx = ctx;
    s->filename = filename;
    s->line_num = 1;
    s->buf_ptr = (const uint8_t *)input;
    s->buf_end = s->buf_ptr + input_len;
    s->token.val = ' ';
    s->token.line_num = 1;
}

__exception int js_parse_program(JSParseState *s) {
    JSFunctionDef *fd = s->cur_func;
    int idx;

    if (next_token(s))
        return -1;
    return 0;
}


//
// Created by benpeng.jiang on 2021/6/3.
//

#include "scanner.h"
#include "jsdump.h"

int __attribute__((format(printf, 2, 3))) js_parse_error(JSParseState *s, const char *fmt, ...) {

}

static void free_token(JSParseState *s, JSToken *token) {

}

static __exception int next_token(JSParseState *s) {
    const uint8_t *p;
    int c;
    BOOL ident_has_escape;
    JSAtom atom;

    if (js_check_stack_overflow(s->ctx->rt, 0)) {
        return js_parse_error(s, "stack overflow");
    }

    free_token(s, &s->token);

    p = s->last_ptr = s->buf_ptr;
    s->got_lf = FALSE;
    s->last_line_num = s->token.line_num;
    redo:
    s->token.line_num = s->line_num;
    s->token.ptr = p;
    c = *p;
    switch (c) {
        case 0:
            if (p >= s->buf_end) {
                s->token.val = TOK_EOF;
            } else {
                goto def_token;
            }
            break;
        case '\r':  /* accept DOS and MAC newline sequences */
            if (p[1] == '\n') {
                p++;
            }
            /* fall thru */
        case '\n':
            p++;
        line_terminator:
            s->got_lf = TRUE;
            s->line_num++;
            goto redo;
        case '\f':
        case '\v':
        case ' ':
        case '\t':
            p++;
            goto redo;
        case '*':
            if (p[1] == '=') {
                p += 2;
                s->token.val = TOK_MUL_ASSIGN;
            } else if (p[1] == '*') {
                if (p[2] == '=') {
                    p += 3;
                    s->token.val = TOK_POW_ASSIGN;
                } else {
                    p += 2;
                    s->token.val = TOK_POW;
                }
            } else {
                goto def_token;
            }
            break;
        case '%':
            if (p[1] == '=') {
                p += 2;
                s->token.val = TOK_MOD_ASSIGN;
            } else {
                goto def_token;
            }
            break;
        default:
        def_token:
            s->token.val = c;
            p++;
            break;
    }
    s->buf_ptr = p;

        dump_token(s, &s->token);
    return 0;

    fail:
    s->token.val = TOK_ERROR;
    return -1;
}

void js_parse_init(JSContext *ctx, JSParseState *s,
                   const char *input, size_t input_len,
                   const char *filename) {
    memset(s, 0, sizeof(*s));
    s->ctx = ctx;
    s->filename = filename;
    s->line_num = 1;
    s->buf_ptr = (const uint8_t *) input;
    s->buf_end = s->buf_ptr + input_len;
    s->token.val = ' ';
    s->token.line_num = 1;
}

void print_tokens(JSParseState *s) {
    while (!next_token(s)) {
       if (TOK_EOF == s->token.val) {
           printf("end of line \n");
           break;
       }
    }
}

__exception int js_parse_program(JSParseState *s) {
    JSFunctionDef *fd = s->cur_func;
    int idx;

    print_tokens(s);
//    if (next_token(s))
//        return -1;
    return 0;
}


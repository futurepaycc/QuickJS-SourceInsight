//
// Created by benpeng.jiang on 2021/6/21.
//

#include "front-token.h"
#include "qjs-atom.h"


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

 __exception int ident_realloc(JSContext *ctx, char **pbuf, size_t *psize,
                                     char *static_buf)
{
    char *buf, *new_buf;
    size_t size, new_size;

    buf = *pbuf;
    size = *psize;
    if (size >= (SIZE_MAX / 3) * 2)
        new_size = SIZE_MAX;
    else
        new_size = size + (size >> 1);
    if (buf == static_buf) {
        new_buf = js_malloc(ctx, new_size);
        if (!new_buf)
            return -1;
        memcpy(new_buf, buf, size);
    } else {
        new_buf = js_realloc(ctx, buf, new_size);
        if (!new_buf)
            return -1;
    }
    *pbuf = new_buf;
    *psize = new_size;
    return 0;
}


 void free_token(JSParseState *s, JSToken *token)
{
    switch(token->val) {
#ifdef CONFIG_BIGNUM
        case TOK_NUMBER:
            JS_FreeValue(s->ctx, token->u.num.val);
            break;
#endif
        case TOK_STRING:
        case TOK_TEMPLATE:
            JS_FreeValue(s->ctx, token->u.str.str);
            break;
        case TOK_REGEXP:
            JS_FreeValue(s->ctx, token->u.regexp.body);
            JS_FreeValue(s->ctx, token->u.regexp.flags);
            break;
        case TOK_IDENT:
        case TOK_PRIVATE_NAME:
            JS_FreeAtom(s->ctx, token->u.ident.atom);
            break;
        default:
            if (token->val >= TOK_FIRST_KEYWORD &&
                token->val <= TOK_LAST_KEYWORD) {
                JS_FreeAtom(s->ctx, token->u.ident.atom);
            }
            break;
    }
}

 void __attribute((unused)) dump_token(JSParseState *s,
                                             const JSToken *token)
{
    switch(token->val) {
        case TOK_NUMBER:
        {
            double d;
            JS_ToFloat64(s->ctx, &d, token->u.num.val);  /* no exception possible */
            printf("number: %.14g\n", d);
        }
            break;
        case TOK_IDENT:
        dump_atom:
        {
            char buf[ATOM_GET_STR_BUF_SIZE];
            printf("ident: '%s'\n",
                   JS_AtomGetStr(s->ctx, buf, sizeof(buf), token->u.ident.atom));
        }
            break;
        case TOK_STRING:
        {
            const char *str;
            /* XXX: quote the string */
            str = JS_ToCString(s->ctx, token->u.str.str);
            printf("string: '%s'\n", str);
            JS_FreeCString(s->ctx, str);
        }
            break;
        case TOK_TEMPLATE:
        {
            const char *str;
            str = JS_ToCString(s->ctx, token->u.str.str);
            printf("template: `%s`\n", str);
            JS_FreeCString(s->ctx, str);
        }
            break;
        case TOK_REGEXP:
        {
            const char *str, *str2;
            str = JS_ToCString(s->ctx, token->u.regexp.body);
            str2 = JS_ToCString(s->ctx, token->u.regexp.flags);
            printf("regexp: '%s' '%s'\n", str, str2);
            JS_FreeCString(s->ctx, str);
            JS_FreeCString(s->ctx, str2);
        }
            break;
        case TOK_EOF:
            printf("eof\n");
            break;
        default:
            if (s->token.val >= TOK_NULL && s->token.val <= TOK_LAST_KEYWORD) {
                goto dump_atom;
            } else if (s->token.val >= 256) {
                printf("token: %d\n", token->val);
            } else {
                printf("token: '%c'\n", token->val);
            }
            break;
    }
}




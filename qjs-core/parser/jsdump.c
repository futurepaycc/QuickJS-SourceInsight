//
// Created by benpeng.jiang on 2021/6/7.
//

#include "jsdump.h"
#include "jsstring.h"

void __attribute((unused)) dump_token(JSParseState *s,
                                      const JSToken *token) {
    switch (token->val) {
        case TOK_NUMBER:
            break;
        case TOK_IDENT:
        dump_atom:
        {
            char buf[ATOM_GET_STR_BUF_SIZE];
            printf("ident: '%s'\n",
                   JS_AtomGetStr(s->ctx, buf, sizeof(buf), token->u.ident.atom));
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
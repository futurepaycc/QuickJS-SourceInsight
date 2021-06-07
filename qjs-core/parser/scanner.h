//
// Created by benpeng.jiang on 2021/6/3.
//

#ifndef TUTORIAL_SCANNER_H
#define TUTORIAL_SCANNER_H
#include "qjs-runtime.h"
#include "lexer.h"

typedef struct JSParseState {
    JSContext *ctx;
    int last_line_num;  /* line number of last token */
    int line_num;       /* line number of current offset */
    const char *filename;
    JSToken token;
    BOOL got_lf; /* true if got line feed before the current token */
    const uint8_t *last_ptr;
    const uint8_t *buf_ptr;
    const uint8_t *buf_end;

    /* current function code */
    JSFunctionDef *cur_func;
    BOOL is_module; /* parsing a module */
    BOOL allow_html_comments;
    BOOL ext_json; /* true if accepting JSON superset */
} JSParseState;

void js_parse_init(JSContext *ctx, JSParseState *s,
                   const char *input, size_t input_len,
                   const char *filename);

int js_parse_program(JSParseState *s);
#endif //TUTORIAL_SCANNER_H

//
// Created by benpeng.jiang on 2021/6/21.
//

#ifndef LOX_JS_FRONT_TOKEN_H
#define LOX_JS_FRONT_TOKEN_H

#include "qjs-api.h"

/* JS parser */

enum {
    TOK_NUMBER = -128,
    TOK_STRING,
    TOK_TEMPLATE,
    TOK_IDENT,
    TOK_REGEXP,
    /* warning: order matters (see js_parse_assign_expr) */
    TOK_MUL_ASSIGN,
    TOK_DIV_ASSIGN,
    TOK_MOD_ASSIGN,
    TOK_PLUS_ASSIGN,
    TOK_MINUS_ASSIGN,
    TOK_SHL_ASSIGN,
    TOK_SAR_ASSIGN,
    TOK_SHR_ASSIGN,
    TOK_AND_ASSIGN,
    TOK_XOR_ASSIGN,
    TOK_OR_ASSIGN,
#ifdef CONFIG_BIGNUM
    TOK_MATH_POW_ASSIGN,
#endif
    TOK_POW_ASSIGN,
    TOK_LAND_ASSIGN,
    TOK_LOR_ASSIGN,
    TOK_DOUBLE_QUESTION_MARK_ASSIGN,
    TOK_DEC,
    TOK_INC,
    TOK_SHL,
    TOK_SAR,
    TOK_SHR,
    TOK_LT,
    TOK_LTE,
    TOK_GT,
    TOK_GTE,
    TOK_EQ,
    TOK_STRICT_EQ,
    TOK_NEQ,
    TOK_STRICT_NEQ,
    TOK_LAND,
    TOK_LOR,
#ifdef CONFIG_BIGNUM
    TOK_MATH_POW,
#endif
    TOK_POW,
    TOK_ARROW,
    TOK_ELLIPSIS,
    TOK_DOUBLE_QUESTION_MARK,
    TOK_QUESTION_MARK_DOT,
    TOK_ERROR,
    TOK_PRIVATE_NAME,
    TOK_EOF,
    /* keywords: WARNING: same order as atoms */
    TOK_NULL, /* must be first */
    TOK_FALSE,
    TOK_TRUE,
    TOK_IF,
    TOK_ELSE,
    TOK_RETURN,
    TOK_VAR,
    TOK_THIS,
    TOK_DELETE,
    TOK_VOID,
    TOK_TYPEOF,
    TOK_NEW,
    TOK_IN,
    TOK_INSTANCEOF,
    TOK_DO,
    TOK_WHILE,
    TOK_FOR,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_SWITCH,
    TOK_CASE,
    TOK_DEFAULT,
    TOK_THROW,
    TOK_TRY,
    TOK_CATCH,
    TOK_FINALLY,
    TOK_FUNCTION,
    TOK_DEBUGGER,
    TOK_WITH,
    /* FutureReservedWord */
    TOK_CLASS,
    TOK_CONST,
    TOK_ENUM,
    TOK_EXPORT,
    TOK_EXTENDS,
    TOK_IMPORT,
    TOK_SUPER,
    /* FutureReservedWords when parsing strict mode code */
    TOK_IMPLEMENTS,
    TOK_INTERFACE,
    TOK_LET,
    TOK_PACKAGE,
    TOK_PRIVATE,
    TOK_PROTECTED,
    TOK_PUBLIC,
    TOK_STATIC,
    TOK_YIELD,
    TOK_AWAIT, /* must be last */
    TOK_OF,     /* only used for js_parse_skip_parens_token() */
};

#define TOK_FIRST_KEYWORD   TOK_NULL
#define TOK_LAST_KEYWORD    TOK_AWAIT

/* unicode code points */
#define CP_NBSP 0x00a0
#define CP_BOM  0xfeff

#define CP_LS   0x2028
#define CP_PS   0x2029

typedef struct JSToken {
    int val;
    int line_num;   /* line number of token start */
    const uint8_t *ptr;
    union {
        struct {
            JSValue str;
            int sep;
        } str;
        struct {
            JSValue val;
#ifdef CONFIG_BIGNUM
            slimb_t exponent; /* may be != 0 only if val is a float */
#endif
        } num;
        struct {
            JSAtom atom;
            BOOL has_escape;
            BOOL is_reserved;
        } ident;
        struct {
            JSValue body;
            JSValue flags;
        } regexp;
    } u;
} JSToken;

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

void free_token(JSParseState *s, JSToken *token);

void __attribute((unused)) dump_token(JSParseState *s,
                                      const JSToken *token);


#define PROP_TYPE_IDENT 0
#define PROP_TYPE_VAR   1
#define PROP_TYPE_GET   2
#define PROP_TYPE_SET   3
#define PROP_TYPE_STAR  4
#define PROP_TYPE_ASYNC 5
#define PROP_TYPE_ASYNC_STAR 6

#define PROP_TYPE_PRIVATE (1 << 4)

static BOOL token_is_ident(int tok)
{
    /* Accept keywords and reserved words as property names */
    return (tok == TOK_IDENT ||
            (tok >= TOK_FIRST_KEYWORD &&
             tok <= TOK_LAST_KEYWORD));
}

#endif //LOX_JS_FRONT_TOKEN_H

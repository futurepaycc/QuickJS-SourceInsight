//
// Created by benpeng.jiang on 2021/7/10.
//

#ifndef LOX_JS_QJS_RESOLVER_H
#define LOX_JS_QJS_RESOLVER_H

#include "qjs-front.h"


typedef struct CodeContext {
    const uint8_t *bc_buf; /* code buffer */
    int bc_len;   /* length of the code buffer */
    int pos;      /* position past the matched code pattern */
    int line_num; /* last visited OP_line_num parameter or -1 */
    int op;
    int idx;
    int label;
    int val;
    JSAtom atom;
} CodeContext;

#define M2(op1, op2)            ((op1) | ((op2) << 8))
#define M3(op1, op2, op3)       ((op1) | ((op2) << 8) | ((op3) << 16))
#define M4(op1, op2, op3, op4)  ((op1) | ((op2) << 8) | ((op3) << 16) | ((op4) << 24))

__exception int resolve_variables(JSContext *ctx, JSFunctionDef *s);

BOOL code_match(CodeContext *s, int pos, ...);

int add_arguments_var(JSContext *ctx, JSFunctionDef *fd);

int add_func_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);

int skip_dead_code(JSFunctionDef *s, const uint8_t *bc_buf, int bc_len,
                   int pos, int *linep);

int get_closure_var(JSContext *ctx, JSFunctionDef *s,
                    JSFunctionDef *fd, BOOL is_arg,
                    int var_idx, JSAtom var_name,
                    BOOL is_const, BOOL is_lexical,
                    JSVarKindEnum var_kind);

int get_closure_var2(JSContext *ctx, JSFunctionDef *s,
                     JSFunctionDef *fd, BOOL is_local,
                     BOOL is_arg, int var_idx, JSAtom var_name,
                     BOOL is_const, BOOL is_lexical,
                     JSVarKindEnum var_kind);

int add_var_this(JSContext *ctx, JSFunctionDef *fd);

int find_closure_var(JSContext *ctx, JSFunctionDef *s,
                     JSAtom var_name);
#endif //LOX_JS_QJS_RESOLVER_H

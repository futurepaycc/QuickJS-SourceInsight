//
// Created by benpeng.jiang on 2021/7/9.
//

#ifndef LOX_JS_FRONT_PARSER_EXPR_H
#define LOX_JS_FRONT_PARSER_EXPR_H

#include "front-token.h"
#include "front-lexer.h"

/* allow the 'in' binary operator */
#define PF_IN_ACCEPTED  (1 << 0)
/* allow function calls parsing in js_parse_postfix_expr() */
#define PF_POSTFIX_CALL (1 << 1)
/* allow arrow functions parsing in js_parse_postfix_expr() */
#define PF_ARROW_FUNC   (1 << 2)
/* allow the exponentiation operator in js_parse_unary() */
#define PF_POW_ALLOWED  (1 << 3)
/* forbid the exponentiation operator in js_parse_unary() */
#define PF_POW_FORBIDDEN (1 << 4)

__exception int js_parse_assign_expr2(JSParseState *s, int parse_flags);

__exception int js_parse_expr(JSParseState *s);

__exception int js_parse_expr2(JSParseState *s, int parse_flags);

__exception int js_parse_regexp(JSParseState *s);

__exception int js_parse_template(JSParseState *s, int call, int *argc);

#define SKIP_HAS_SEMI       (1 << 0)
#define SKIP_HAS_ELLIPSIS   (1 << 1)
#define SKIP_HAS_ASSIGNMENT (1 << 2)

__exception int js_parse_function_decl(JSParseState *s,
                                       JSParseFunctionEnum func_type,
                                       JSFunctionKindEnum func_kind,
                                       JSAtom func_name, const uint8_t *ptr,
                                       int start_line);

JSFunctionDef *js_parse_function_class_fields_init(JSParseState *s);

__exception int js_parse_function_decl2(JSParseState *s,
                                        JSParseFunctionEnum func_type,
                                        JSFunctionKindEnum func_kind,
                                        JSAtom func_name,
                                        const uint8_t *ptr,
                                        int function_line_num,
                                        JSParseExportEnum export_flag,
                                        JSFunctionDef **pfd);

__exception int js_parse_assign_expr(JSParseState *s);

__exception int js_parse_unary(JSParseState *s, int parse_flags);

__exception int js_parse_postfix_expr(JSParseState *s, int parse_flags);

__exception int js_parse_left_hand_side_expr(JSParseState *s);

void set_object_name(JSParseState *s, JSAtom name);

typedef struct JSParsePos {
    int last_line_num;
    int line_num;
    BOOL got_lf;
    const uint8_t *ptr;
} JSParsePos;

int js_parse_get_pos(JSParseState *s, JSParsePos *sp);

__exception int js_parse_seek_token(JSParseState *s, const JSParsePos *sp);

BOOL token_is_pseudo_keyword(JSParseState *s, JSAtom atom);

int add_scope_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name,
                  JSVarKindEnum var_kind);

typedef enum {
    JS_VAR_DEF_WITH,
    JS_VAR_DEF_LET,
    JS_VAR_DEF_CONST,
    JS_VAR_DEF_FUNCTION_DECL, /* function declaration */
    JS_VAR_DEF_NEW_FUNCTION_DECL, /* async/generator function declaration */
    JS_VAR_DEF_CATCH,
    JS_VAR_DEF_VAR,
} JSVarDefEnum;

int push_scope(JSParseState *s);

void pop_scope(JSParseState *s);

int define_var(JSParseState *s, JSFunctionDef *fd, JSAtom name,
               JSVarDefEnum var_def_type);

JSExportEntry *add_export_entry(JSParseState *s, JSModuleDef *m,
                                JSAtom local_name, JSAtom export_name,
                                JSExportTypeEnum export_type);

__exception int js_parse_expr_paren(JSParseState *s);

int js_parse_destructuring_element(JSParseState *s, int tok, int is_arg,
                                   int hasval, int has_ellipsis,
                                   BOOL allow_initializer);

int js_parse_skip_parens_token(JSParseState *s, int *pbits, BOOL no_line_terminator);

__exception int js_parse_class(JSParseState *s, BOOL is_class_expr,
                               JSParseExportEnum export_flag);

int js_update_property_flags(JSContext *ctx, JSObject *p,
                             JSShapeProperty **pprs, int flags);

int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                            JSShapeProperty **pprs);

int __exception js_parse_property_name(JSParseState *s,
                                       JSAtom *pname,
                                       BOOL allow_method, BOOL allow_var,
                                       BOOL allow_private);

JSAtom get_private_setter_name(JSContext *ctx, JSAtom name);
#endif //LOX_JS_FRONT_PARSER_EXPR_H

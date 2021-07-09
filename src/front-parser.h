//
// Created by benpeng.jiang on 2021/6/21.
//

#ifndef LOX_JS_FRONT_PARSER_H
#define LOX_JS_FRONT_PARSER_H

#include "qjs-front.h"

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

__exception int js_parse_source_element(JSParseState *s);

__exception int js_parse_expr(JSParseState *s);

__exception int js_parse_expr2(JSParseState *s, int parse_flags);

__exception int js_parse_regexp(JSParseState *s);

__exception int js_parse_template(JSParseState *s, int call, int *argc);

int js_update_property_flags(JSContext *ctx, JSObject *p,
                             JSShapeProperty **pprs, int flags);

int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                                   JSShapeProperty **pprs);;

#endif //LOX_JS_FRONT_PARSER_H

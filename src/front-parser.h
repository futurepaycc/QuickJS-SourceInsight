//
// Created by benpeng.jiang on 2021/6/21.
//

#ifndef LOX_JS_FRONT_PARSER_H
#define LOX_JS_FRONT_PARSER_H

#include "front.h"

__exception int js_parse_expr(JSParseState *s);


__exception int js_parse_template(JSParseState *s, int call, int *argc);

int js_update_property_flags(JSContext *ctx, JSObject *p,
                             JSShapeProperty **pprs, int flags);

int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                                   JSShapeProperty **pprs);;

#endif //LOX_JS_FRONT_PARSER_H

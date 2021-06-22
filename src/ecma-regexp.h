//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_ECMA_REGEXP_H
#define LOX_JS_ECMA_REGEXP_H
#include "qjs-internal.h"
#include "qjs-runtime.h"

 JSValue js_compile_regexp(JSContext *ctx, JSValueConst pattern,
                                 JSValueConst flags);
 JSValue js_regexp_constructor_internal(JSContext *ctx, JSValueConst ctor,
                                              JSValue pattern, JSValue bc);
#endif //LOX_JS_ECMA_REGEXP_H

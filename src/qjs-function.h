//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_FUNCTION_H
#define LOX_JS_FUNCTION_H
#include "qjs-runtime.h"


 void js_function_set_properties(JSContext *ctx, JSValueConst func_obj,
                                       JSAtom name, int len);
#endif //LOX_JS_FUNCTION_H

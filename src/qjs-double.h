//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_DOUBLE_H
#define LOX_JS_DOUBLE_H
#include "qjs-api.h"
#include "qjs-internal.h"
#include <assert.h>

 JSValue js_dtoa(JSContext *ctx,
                       double d, int radix, int n_digits, int flags);

#endif //LOX_JS_DOUBLE_H

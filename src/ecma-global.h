//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_ECMA_GLOBAL_H
#define LOX_JS_ECMA_GLOBAL_H
#include "qjs-api.h"
 JSValue JS_EvalObject(JSContext *ctx, JSValueConst this_obj,
                             JSValueConst val, int flags, int scope_idx);

 int JS_CreateDataPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                       int64_t idx, JSValue val, int flags);
#endif //LOX_JS_ECMA_GLOBAL_H

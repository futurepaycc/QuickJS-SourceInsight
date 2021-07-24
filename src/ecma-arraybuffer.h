//
// Created by benpeng.jiang on 2021/6/20.
//

#ifndef LOX_JS_ECMA_ARRAYBUFFER_H
#define LOX_JS_ECMA_ARRAYBUFFER_H
#include "qjs-api.h"
 JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);

JSValue js_array_buffer_constructor1(JSContext *ctx,
                                     JSValueConst new_target,
                                     uint64_t len);

JSValue js_array_buffer_constructor2(JSContext *ctx,
                                     JSValueConst new_target,
                                     uint64_t len, JSClassID class_id);
#endif //LOX_JS_ECMA_ARRAYBUFFER_H

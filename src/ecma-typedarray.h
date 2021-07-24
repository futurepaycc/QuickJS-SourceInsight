//
// Created by benpeng.jiang on 2021/7/24.
//

#ifndef LOX_JS_ECMA_TYPEDARRAY_H
#define LOX_JS_ECMA_TYPEDARRAY_H
#include "qjs-api.h"
JSValue js_typed_array_get_buffer(JSContext *ctx,
                                  JSValueConst this_val, int is_dataview);
JSValue js_typed_array_get_byteLength(JSContext *ctx,
                                      JSValueConst this_val,
                                      int is_dataview);
JSValue js_typed_array_get_byteOffset(JSContext *ctx,
                                      JSValueConst this_val,
                                      int is_dataview);

JSValue js_typed_array_base_constructor(JSContext *ctx,
                                        JSValueConst this_val,
                                        int argc, JSValueConst *argv);

JSValue js_typed_array_from(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv);

JSValue js_typed_array_of(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv);
#endif //LOX_JS_ECMA_TYPEDARRAY_H

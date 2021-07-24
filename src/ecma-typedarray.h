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

JSValue js_typed_array_get_length(JSContext *ctx,
                                  JSValueConst this_val);

JSValue js_typed_array_set(JSContext *ctx,
                           JSValueConst this_val,
                           int argc, JSValueConst *argv);

JSValue js_create_typed_array_iterator(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv, int magic);


JSValue js_typed_array_get_toStringTag(JSContext *ctx,
                                       JSValueConst this_val);

JSValue js_typed_array_copyWithin(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv);

JSValue js_typed_array_fill(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv);

JSValue js_typed_array_find(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv, int findIndex);

JSValue js_typed_array_reverse(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv);

JSValue js_typed_array_slice(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv);

JSValue js_typed_array_subarray(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv);

JSValue js_typed_array_sort(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv);

JSValue js_typed_array_join(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv, int toLocaleString);

JSValue js_typed_array_indexOf(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int special);

#define special_indexOf 0
#define special_lastIndexOf 1
#define special_includes -1


void js_typed_array_finalizer(JSRuntime *rt, JSValue val);
void js_typed_array_mark(JSRuntime *rt, JSValueConst val,
                         JS_MarkFunc *mark_func);

#endif //LOX_JS_ECMA_TYPEDARRAY_H

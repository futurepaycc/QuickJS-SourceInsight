//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_ECMA_ITERATOR_H
#define LOX_JS_ECMA_ITERATOR_H

#include "qjs-error.h"

JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async);

JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
                        JSValueConst method,
                        int argc, JSValueConst *argv, BOOL *pdone);

__exception int js_iterator_get_value_done(JSContext *ctx, JSValue *sp);

int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj,
                     BOOL is_exception_pending);

JSValue js_array_iterator_next(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv,
                               BOOL *pdone, int magic);

JSValue js_create_array_iterator(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic);

JSValue js_create_iterator_result(JSContext *ctx,
                                  JSValue val,
                                  BOOL done);


__exception int js_for_of_start(JSContext *ctx, JSValue *sp,
                                BOOL is_async);

JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj,
                        JSValueConst method);

JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj,
                         JSValueConst method,
                         int argc, JSValueConst *argv, int *pdone);

JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj,
                                    BOOL *pdone);

 JSValue JS_CreateAsyncFromSyncIterator(JSContext *ctx,
                                              JSValueConst sync_iter);
#endif //LOX_JS_ECMA_ITERATOR_H

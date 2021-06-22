//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_ECMA_FUNCTION_H
#define LOX_JS_ECMA_FUNCTION_H
#include "qjs-runtime.h"
#include "qjs-internal.h"
JSContext *JS_GetFunctionRealm(JSContext *ctx, JSValueConst func_obj);
JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                            int class_id);

 JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj,
                                  JSValueConst this_obj,
                                  int argc, JSValueConst *argv, int flags);
 JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags);

 void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);
 JSValue *build_arg_list(JSContext *ctx, uint32_t *plen,
                               JSValueConst array_arg);
JSValue js_function_apply(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic);

 void js_method_set_home_object(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst home_obj);

 int js_method_set_properties(JSContext *ctx, JSValueConst func_obj,
                                    JSAtom name, int flags, JSValueConst home_obj);

BOOL JS_IsCFunction(JSContext *ctx, JSValueConst val, JSCFunction *func, int magic);
#endif //LOX_JS_ECMA_FUNCTION_H

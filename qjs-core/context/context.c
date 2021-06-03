//
// Created by benpeng.jiang on 2021/5/22.
//


#include "list.h"
#include "qjs-runtime.h"

/* 'input' must be zero terminated i.e. input[input_len] = '\0'. */
static JSValue __JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                                 const char *input, size_t input_len,
                                 const char *filename, int flags, int scope_idx)
{
    int err, js_mode, eval_type;
    JSValue fun_obj, ret_val;
    return ret_val;
    fail1:
    return JS_EXCEPTION;
}

void JS_AddIntrinsicEval(JSContext *ctx)
{
    ctx->eval_internal = __JS_EvalInternal;
}

JSContext *JS_NewContextRaw(JSRuntime *rt) {
    JSContext *ctx;
    int i;

    ctx = js_mallocz_rt(rt, sizeof(JSContext));
    if (!ctx)
        return NULL;
    return ctx;
}

JSContext *JS_NewContext(JSRuntime *rt) {
    JSContext *ctx;

    ctx = JS_NewContextRaw(rt);
    if (!ctx)
        return NULL;
    JS_AddIntrinsicEval(ctx);

    ctx->header.ref_count = 1;

    ctx->rt = rt;
    return ctx;
}

/* also used to initialize the worker context */
JSContext *JS_NewCustomContext(JSRuntime *rt) {
    JSContext *ctx;
    ctx = JS_NewContext(rt);


    return ctx;
}
//
// Created by benpeng.jiang on 2021/5/22.
//


#include "list.h"
#include "qjs-runtime.h"

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
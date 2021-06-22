//
// Created by benpeng.jiang on 2021/6/18.
//

#include "qjs-interrupts.h"

static no_inline __exception int __js_poll_interrupts(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    ctx->interrupt_counter = JS_INTERRUPT_COUNTER_INIT;
    if (rt->interrupt_handler) {
        if (rt->interrupt_handler(rt, rt->interrupt_opaque)) {
            /* XXX: should set a specific flag to avoid catching */
            JS_ThrowInternalError(ctx, "interrupted");
            JS_SetUncatchableError(ctx, ctx->rt->current_exception, TRUE);
            return -1;
        }
    }
    return 0;
}

  __exception int js_poll_interrupts(JSContext *ctx)
{
    if (unlikely(--ctx->interrupt_counter <= 0)) {
        return __js_poll_interrupts(ctx);
    } else {
        return 0;
    }
}
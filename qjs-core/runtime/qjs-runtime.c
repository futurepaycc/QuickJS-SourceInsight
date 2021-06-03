//
// Created by benpeng.jiang on 2021/5/21.
//

#include "qjs-runtime.h"
#include "jmemory.h"
#include "list.h"
#include <stdio.h>
#include <quickjs.h>
#include "jsstring.h"

static JSValue JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                               const char *input, size_t input_len,
                               const char *filename, int flags, int scope_idx);

static size_t js_malloc_usable_size_unknown(const void *ptr)
{
    return 0;
}

void *js_malloc_rt(JSRuntime *rt, size_t size)
{
    return rt->mf.js_malloc(&rt->malloc_state, size);
}


void js_free(JSContext *ctx, void *ptr)
{
    js_free_rt(ctx->rt, ptr);
}


void js_free_rt(JSRuntime *rt, void *ptr)
{
    rt->mf.js_free(&rt->malloc_state, ptr);
}

void *js_realloc_rt(JSRuntime *rt, void *ptr, size_t size)
{
    return rt->mf.js_realloc(&rt->malloc_state, ptr, size);
}

size_t js_malloc_usable_size_rt(JSRuntime *rt, const void *ptr)
{
    return rt->mf.js_malloc_usable_size(ptr);
}

void *js_mallocz_rt(JSRuntime *rt, size_t size)
{
    void *ptr;
    ptr = js_malloc_rt(rt, size);
    if (!ptr)
        return NULL;
    return memset(ptr, 0, size);
}


void JS_FreeRuntime(JSRuntime *rt)
{

}

JSRuntime *JS_NewRuntime2(const JSMallocFunctions *mf, void *opaque) {
    JSRuntime * rt;
    JSMallocState ms;

    memset(&ms, 0, sizeof(ms));
    ms.opaque = opaque;
    ms.malloc_limit = -1;

    rt = mf->js_malloc(&ms, sizeof(JSRuntime));
    if (!rt)
        return NULL;
    memset(rt, 0, sizeof(*rt));
    rt->mf = *mf;
    if (!rt->mf.js_malloc_usable_size) {
        /* use dummy function if none provided */
        rt->mf.js_malloc_usable_size = js_malloc_usable_size_unknown;
    }
    rt->malloc_state = ms;

    init_list_head(&rt->context_list);
    if (JS_InitAtoms(rt))
        goto fail;

    return rt;
    fail:
    JS_FreeRuntime(rt);
    return NULL;
}

void JS_SetRuntimeInfo(JSRuntime *rt, const char *s)
{
    if (rt)
        rt->rt_info = s;
}



JSRuntime *JS_NewRuntime(void)
{
    return JS_NewRuntime2(&def_malloc_funcs, NULL);
}



void JS_ComputeMemoryUsage(JSRuntime *rt, JSMemoryUsage *s) {
    struct list_head *el, *el1;
    int i;
    JSMemoryUsage_helper mem = {0}, *hp = &mem;

    memset(s, 0, sizeof(*s));
    s->malloc_count = rt->malloc_state.malloc_count;
    s->malloc_size = rt->malloc_state.malloc_size;
    s->malloc_limit = rt->malloc_state.malloc_limit;

    s->memory_used_count = 2; /* rt + rt->class_array */
}


void JS_DumpMemoryUsage(FILE *fp, const JSMemoryUsage *s, JSRuntime *rt) {
    fprintf(fp, "QuickJS memory usage -- "
#ifdef CONFIG_BIGNUM
    "BigNum "
#endif
    CONFIG_VERSION
    " version, %d-bit, malloc limit: %"
    PRId64
    "\n\n",
            (int) sizeof(void *) * 8, (int64_t) (ssize_t) s->malloc_limit);


    fprintf(fp, "%-20s %8s %8s\n", "NAME", "COUNT", "SIZE");

    if (s->malloc_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per block)\n",
                "memory allocated", s->malloc_count, s->malloc_size,
                (double)s->malloc_size / s->malloc_count);
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%d overhead, %0.1f average slack)\n",
                "memory used", s->memory_used_count, s->memory_used_size,
                MALLOC_OVERHEAD, ((double)(s->malloc_size - s->memory_used_size) /
                                  s->memory_used_count));
    }
}


JSValue JS_Eval(JSContext *ctx, const char *input, size_t input_len,
                const char *filename, int eval_flags)
{
    int eval_type = eval_flags & JS_EVAL_TYPE_MASK;
    JSValue ret;

    assert(eval_type == JS_EVAL_TYPE_GLOBAL ||
           eval_type == JS_EVAL_TYPE_MODULE);
    ret = JS_EvalInternal(ctx, ctx->global_obj, input, input_len, filename,
                          eval_flags, -1);
    return ret;
}

/* the indirection is needed to make 'eval' optional */
static JSValue JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                               const char *input, size_t input_len,
                               const char *filename, int flags, int scope_idx)
{
    return ctx->eval_internal(ctx, this_obj, input, input_len, filename,
                              flags, scope_idx);
}


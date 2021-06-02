//
// Created by benpeng.jiang on 2021/5/26.
//

#include "qjs-runtime.h"
#include "quickjs-libc.h"
#include <stdlib.h>
#include "cutils.h"

extern const uint8_t qjsc_repl[];
extern const uint32_t qjsc_repl_size;


static int eval_buf(JSContext *ctx, const void *buf, int buf_len,
                    const char *filename, int eval_flags)
{
    JSValue val;
    int ret;

    val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);

    ret = 0;

    return ret;
}

static int eval_file(JSContext *ctx, const char *filename, int module)
{
    uint8_t *buf;
    int ret, eval_flags;
    size_t buf_len;

    buf = js_load_file(ctx, &buf_len, filename);
    if (!buf) {
        perror(filename);
        exit(1);
    }


    if (module)
        eval_flags = JS_EVAL_TYPE_MODULE;
    else
        eval_flags = JS_EVAL_TYPE_GLOBAL;
    ret = eval_buf(ctx, buf, buf_len, filename, eval_flags);
    js_free(ctx, buf);
    return ret;
}


int main_entry(int argc, char **argv) {
    JSRuntime *rt;
    JSContext *ctx;
    int optind;
    char *expr = NULL;
    int interactive = 0;
    int dump_memory = 0;
    int trace_memory = 0;
    int empty_run = 0;
    int module = -1;
    int load_std = 0;
    int dump_unhandled_promise_rejection = 0;
    size_t memory_limit = 0;
    char *include_list[32];
    int i, include_count = 0;
    size_t stack_size = 0;
    optind = 1;
    rt = JS_NewRuntime();

    ctx = JS_NewCustomContext(rt);


    if (optind >= argc) {
        /* interactive mode */
        interactive = 1;
    } else {
        const char *filename;
        filename = argv[optind];
        if (eval_file(ctx, filename, module))
            goto fail;
    }

    return 0;
    fail:
    JS_FreeRuntime(rt);
    return 1;
}
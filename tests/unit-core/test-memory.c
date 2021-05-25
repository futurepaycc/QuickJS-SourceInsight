//
// Created by benpeng.jiang on 2021/5/21.
//

#include "qjs.h"
#include "test-common.h"
#include "jmemory.h"

int main(int argc, char **argv) {
    int dump_memory = 1;

    JSRuntime *rt;
    JSContext *ctx;
    rt = JS_NewRuntime();

    JSMemoryUsage stats;
    JS_ComputeMemoryUsage(rt, &stats);
    JS_DumpMemoryUsage(stdout, &stats, rt);

    int64_t before_alloc = stats.malloc_size;
    int64_t rt_size = def_malloc_funcs.js_malloc_usable_size(rt);
    TEST_ASSERT(stats.malloc_count==1);
    TEST_ASSERT(before_alloc==rt_size);

    js_malloc_rt(rt, 4);
    JS_ComputeMemoryUsage(rt, &stats);
    JS_DumpMemoryUsage(stdout, &stats, rt);

    TEST_ASSERT(stats.malloc_count==2);

}
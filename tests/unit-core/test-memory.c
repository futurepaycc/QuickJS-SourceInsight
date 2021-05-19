//
// Created by benpeng.jiang on 2021/5/21.
//

#include "qjs.h"
#include "test-common.h"

int main(int argc, char **argv) {
    int dump_memory = 1;

    JSRuntime *rt;
    JSContext *ctx;
    rt = JS_NewRuntime();

    JSMemoryUsage stats;
    JS_ComputeMemoryUsage(rt, &stats);

    TEST_ASSERT(stats.malloc_count==1);

}
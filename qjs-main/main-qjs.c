//
// Created by benpeng.jiang on 2021/5/22.
//
#include "qjs.h"

int main(int argc, char **argv) {
    int dump_memory = 1;

    JSRuntime *rt;
    JSContext *ctx;
    rt = JS_NewRuntime();

    if (dump_memory) {
        JSMemoryUsage stats;
        JS_ComputeMemoryUsage(rt, &stats);
        JS_DumpMemoryUsage(stdout, &stats, rt);
    }
}
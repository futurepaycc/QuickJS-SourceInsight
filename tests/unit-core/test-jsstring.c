//
// Created by benpeng.jiang on 2021/5/26.
//

#include "jsstring.h"

int main(int argc, char* argv[])
{

    JSRuntime *rt;
    JSContext *ctx;
    rt = JS_NewRuntime();

    ctx = JS_NewCustomContext(rt);

    test_dump_str(ctx);

    return 0;

}

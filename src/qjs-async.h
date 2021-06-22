//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_ASYNC_H
#define LOX_JS_ASYNC_H

#include "qjs-api.h"
#include "qjs-internal.h"

 void async_func_mark(JSRuntime *rt, JSAsyncFunctionState *s,
                            JS_MarkFunc *mark_func);
#endif //LOX_JS_ASYNC_H

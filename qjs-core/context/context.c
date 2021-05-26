//
// Created by benpeng.jiang on 2021/5/22.
//

#include "qjs.h"

#include "list.h"
#include "qjs-runtime.h"
#include "gc.h"

typedef struct JSContext {
    JSGCObjectHeader header; /* must come first */
    JSRuntime *rt;
    struct list_head link;
} JSContext;
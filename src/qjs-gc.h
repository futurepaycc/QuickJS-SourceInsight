//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_GC_H
#define LOX_JS_GC_H

#include "qjs-api.h"
#include "qjs-opcode.h"
#include "ecma.h"
#include "qjs-async.h"

void remove_gc_object(JSGCObjectHeader *h);
void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type);
void free_function_bytecode(JSRuntime *rt, JSFunctionBytecode *b);

void js_free_function_def(JSContext *ctx, JSFunctionDef *fd);

void js_free_shape(JSRuntime *rt, JSShape *sh);
 void free_zero_refcount(JSRuntime *rt);

#endif //LOX_JS_GC_H

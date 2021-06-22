//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_CLOSURE_H
#define LOX_JS_CLOSURE_H
#include "qjs-api.h"
#include "qjs-varrefs.h"
JSValue js_closure(JSContext *ctx, JSValue bfunc,
                   JSVarRef **cur_var_refs,
                   JSStackFrame *sf);
JSValue js_closure2(JSContext *ctx, JSValue func_obj,
                    JSFunctionBytecode *b,
                    JSVarRef **cur_var_refs,
                    JSStackFrame *sf);

static const uint16_t func_kind_to_class_id[] = {
        [JS_FUNC_NORMAL] = JS_CLASS_BYTECODE_FUNCTION,
        [JS_FUNC_GENERATOR] = JS_CLASS_GENERATOR_FUNCTION,
        [JS_FUNC_ASYNC] = JS_CLASS_ASYNC_FUNCTION,
        [JS_FUNC_ASYNC_GENERATOR] = JS_CLASS_ASYNC_GENERATOR_FUNCTION,
};
#endif //LOX_JS_CLOSURE_H

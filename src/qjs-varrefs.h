//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_VARREFS_H
#define LOX_JS_VARREFS_H

#include "qjs-api.h"
#include "qjs-gc.h"

void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx,
                             BOOL is_arg);

 void close_var_refs(JSRuntime *rt, JSStackFrame *sf);
#endif //LOX_JS_VARREFS_H

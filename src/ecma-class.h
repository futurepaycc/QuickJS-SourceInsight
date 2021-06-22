//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_ECMA_CLASS_H
#define LOX_JS_ECMA_CLASS_H
#include "qjs-runtime.h"
#include "qjs-internal.h"

 JSValue JS_CallConstructorInternal(JSContext *ctx,
                                          JSValueConst func_obj,
                                          JSValueConst new_target,
                                          int argc, JSValue *argv, int flags);

 int js_op_define_class(JSContext *ctx, JSValue *sp,
                              JSAtom class_name, int class_flags,
                              JSVarRef **cur_var_refs,
                              JSStackFrame *sf, BOOL is_computed_name);
#endif //LOX_JS_ECMA_CLASS_H

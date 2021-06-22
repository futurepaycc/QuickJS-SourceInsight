//
// Created by benpeng.jiang on 2021/6/18.
//

#ifndef LOX_JS_VM_H
#define LOX_JS_VM_H
#include "qjs-api.h"
#include "ecma-object.h"
#include "qjs-error.h"
#include "ecma-type-conversion.h"
#include "qjs-atom.h"
#include "qjs-interrupts.h"
#include "qjs-closure.h"
#include "ecma-modules.h"
#include "ecma.h"
#include "qjs-operation.h"
#include "qjs-backtrace.h"
#include "qjs-varrefs.h"

JSValue JS_CallInternal(JSContext *ctx, JSValueConst func_obj,
                        JSValueConst this_obj, JSValueConst new_target,
                        int argc, JSValue *argv, int flags);

JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                           int argc, JSValueConst *argv);
 JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                             int argc, JSValueConst *argv);
 void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
#endif //LOX_JS_VM_H

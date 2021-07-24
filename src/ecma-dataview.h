//
// Created by benpeng.jiang on 2021/7/24.
//

#ifndef LOX_JS_ECMA_DATAVIEW_H
#define LOX_JS_ECMA_DATAVIEW_H

#include "qjs-api.h"

JSValue js_dataview_constructor(JSContext *ctx,
                                JSValueConst new_target,
                                int argc, JSValueConst *argv);

JSValue js_dataview_getValue(JSContext *ctx,
                             JSValueConst this_obj,
                             int argc, JSValueConst *argv, int class_id);

JSValue js_dataview_setValue(JSContext *ctx,
                             JSValueConst this_obj,
                             int argc, JSValueConst *argv, int class_id);

#endif //LOX_JS_ECMA_DATAVIEW_H


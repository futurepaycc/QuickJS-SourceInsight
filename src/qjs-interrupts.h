//
// Created by benpeng.jiang on 2021/6/18.
//

#ifndef LOX_JS_INTERRUPTS_H
#define LOX_JS_INTERRUPTS_H

#include "qjs-api.h"
#include "qjs-error.h"

 __exception int js_poll_interrupts(JSContext *ctx);

#endif //LOX_JS_INTERRUPTS_H

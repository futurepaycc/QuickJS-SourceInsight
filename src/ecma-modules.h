//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_MODULES_H
#define LOX_JS_MODULES_H
#include "qjs-api.h"
typedef enum JSFreeModuleEnum {
    JS_FREE_MODULE_ALL,
    JS_FREE_MODULE_NOT_RESOLVED,
    JS_FREE_MODULE_NOT_EVALUATED,
} JSFreeModuleEnum;

void js_free_module_def(JSContext *ctx, JSModuleDef *m);
void js_mark_module_def(JSRuntime *rt, JSModuleDef *m,
                               JS_MarkFunc *mark_func);
JSValue js_import_meta(JSContext *ctx);
JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier);

 int js_resolve_module(JSContext *ctx, JSModuleDef *m);
void js_free_modules(JSContext *ctx, JSFreeModuleEnum flag);


#endif //LOX_JS_MODULES_H

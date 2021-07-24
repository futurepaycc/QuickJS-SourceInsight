//
// Created by benpeng.jiang on 2021/7/24.
//

#ifndef LOX_JS_QJS_DUMPER_H
#define LOX_JS_QJS_DUMPER_H
#include "qjs-api.h"

__maybe_unused void JS_DumpValue(JSContext *ctx, JSValueConst val);

void dump_byte_code(JSContext *ctx, int pass,
                    const uint8_t *tab, int len,
                    const JSVarDef *args, int arg_count,
                    const JSVarDef *vars, int var_count,
                    const JSClosureVar *closure_var, int closure_var_count,
                    const JSValue *cpool, uint32_t cpool_count,
                    const char *source, int line_num,
                    const LabelSlot *label_slots, JSFunctionBytecode *b);

__maybe_unused void js_dump_function_bytecode(JSContext *ctx, JSFunctionBytecode *b);
#endif //LOX_JS_QJS_DUMPER_H

//
// Created by benpeng.jiang on 2021/6/21.
//

#ifndef LOX_JS_ECMA_BIGNUMBER_H
#define LOX_JS_ECMA_BIGNUMBER_H
#include "qjs-api.h"

 void js_float_env_finalizer(JSRuntime *rt, JSValue val);
 JSValue JS_NewBigFloat(JSContext *ctx);
static inline bf_t *JS_GetBigFloat(JSValueConst val)
{
    JSBigFloat *p = JS_VALUE_GET_PTR(val);
    return &p->num;
}
 JSValue JS_NewBigDecimal(JSContext *ctx);
static inline bfdec_t *JS_GetBigDecimal(JSValueConst val)
{
    JSBigDecimal *p = JS_VALUE_GET_PTR(val);
    return &p->num;
}
 JSValue JS_NewBigInt(JSContext *ctx);
  bf_t *JS_GetBigInt(JSValueConst val);
 JSValue JS_CompactBigInt1(JSContext *ctx, JSValue val,
                                 BOOL convert_to_safe_integer);
 JSValue JS_CompactBigInt(JSContext *ctx, JSValue val);
 int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
 bf_t *JS_ToBigInt(JSContext *ctx, bf_t *buf, JSValueConst val);
 void JS_FreeBigInt(JSContext *ctx, bf_t *a, bf_t *buf);
 bf_t *JS_ToBigFloat(JSContext *ctx, bf_t *buf, JSValueConst val);
 JSValue JS_ToBigDecimalFree(JSContext *ctx, JSValue val,
                                   BOOL allow_null_or_undefined);
 bfdec_t *JS_ToBigDecimal(JSContext *ctx, JSValueConst val);
#endif //LOX_JS_ECMA_BIGNUMBER_H

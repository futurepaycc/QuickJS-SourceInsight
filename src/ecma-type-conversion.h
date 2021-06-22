//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_TYPE_H
#define LOX_JS_TYPE_H

#include "qjs-api.h"
#include "qjs-vm.h"
#include "qjs-double.h"

JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint);

JSValue JS_ToStringFree(JSContext *ctx, JSValue val);

JSValue JS_ToObject(JSContext *ctx, JSValueConst val);

JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);

int JS_ToBoolFree(JSContext *ctx, JSValue val);

int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val);

int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val);

int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);

JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val);

JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);
int JS_ToInt64SatFree(JSContext *ctx, int64_t *pres, JSValue val);
int JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val,
                    int min, int max, int min_offset);
int JS_ToInt64Clamp(JSContext *ctx, int64_t *pres, JSValueConst val,
                    int64_t min, int64_t max, int64_t neg_offset);

__exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen,
                                     JSValue val, BOOL is_array_ctor);
__maybe_unused JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val);
__exception int JS_ToLengthFree(JSContext *ctx, int64_t *plen,
                                JSValue val);
int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val);
int JS_ToInt32SatFree(JSContext *ctx, int *pres, JSValue val);
int JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val);
#endif //LOX_JS_TYPE_H

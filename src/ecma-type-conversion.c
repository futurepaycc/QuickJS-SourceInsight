//
// Created by benpeng.jiang on 2021/6/19.
//

#include "ecma-type-conversion.h"

JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint)
{
    int i;
    BOOL force_ordinary;

    JSAtom method_name;
    JSValue method, ret;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return val;
    force_ordinary = hint & HINT_FORCE_ORDINARY;
    hint &= ~HINT_FORCE_ORDINARY;
    if (!force_ordinary) {
        method = JS_GetProperty(ctx, val, JS_ATOM_Symbol_toPrimitive);
        if (JS_IsException(method))
            goto exception;
        /* ECMA says *If exoticToPrim is not undefined* but tests in
           test262 use null as a non callable converter */
        if (!JS_IsUndefined(method) && !JS_IsNull(method)) {
            JSAtom atom;
            JSValue arg;
            switch(hint) {
                case HINT_STRING:
                    atom = JS_ATOM_string;
                    break;
                case HINT_NUMBER:
                    atom = JS_ATOM_number;
                    break;
                default:
                case HINT_NONE:
                    atom = JS_ATOM_default;
                    break;
            }
            arg = JS_AtomToString(ctx, atom);
            ret = JS_CallFree(ctx, method, val, 1, (JSValueConst *)&arg);
            JS_FreeValue(ctx, arg);
            if (JS_IsException(ret))
                goto exception;
            JS_FreeValue(ctx, val);
            if (JS_VALUE_GET_TAG(ret) != JS_TAG_OBJECT)
                return ret;
            JS_FreeValue(ctx, ret);
            return JS_ThrowTypeError(ctx, "toPrimitive");
        }
    }
    if (hint != HINT_STRING)
        hint = HINT_NUMBER;
    for(i = 0; i < 2; i++) {
        if ((i ^ hint) == 0) {
            method_name = JS_ATOM_toString;
        } else {
            method_name = JS_ATOM_valueOf;
        }
        method = JS_GetProperty(ctx, val, method_name);
        if (JS_IsException(method))
            goto exception;
        if (JS_IsFunction(ctx, method)) {
            ret = JS_CallFree(ctx, method, val, 0, NULL);
            if (JS_IsException(ret))
                goto exception;
            if (JS_VALUE_GET_TAG(ret) != JS_TAG_OBJECT) {
                JS_FreeValue(ctx, val);
                return ret;
            }
            JS_FreeValue(ctx, ret);
        } else {
            JS_FreeValue(ctx, method);
        }
    }
    JS_ThrowTypeError(ctx, "toPrimitive");
    exception:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

 JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint)
{
    return JS_ToPrimitiveFree(ctx, JS_DupValue(ctx, val), hint);
}

JSValue JS_ToStringFree(JSContext *ctx, JSValue val)
{
    JSValue ret;
    ret = JS_ToString(ctx, val);
    JS_FreeValue(ctx, val);
    return ret;
}


JSValue JS_ToStringInternal(JSContext *ctx, JSValueConst val, BOOL is_ToPropertyKey)
{
    uint32_t tag;
    const char *str;
    char buf[32];

    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
        case JS_TAG_STRING:
            return JS_DupValue(ctx, val);
        case JS_TAG_INT:
            snprintf(buf, sizeof(buf), "%d", JS_VALUE_GET_INT(val));
            str = buf;
            goto new_string;
        case JS_TAG_BOOL:
            return JS_AtomToString(ctx, JS_VALUE_GET_BOOL(val) ?
                                        JS_ATOM_true : JS_ATOM_false);
        case JS_TAG_NULL:
            return JS_AtomToString(ctx, JS_ATOM_null);
        case JS_TAG_UNDEFINED:
            return JS_AtomToString(ctx, JS_ATOM_undefined);
        case JS_TAG_EXCEPTION:
            return JS_EXCEPTION;
        case JS_TAG_OBJECT:
        {
            JSValue val1, ret;
            val1 = JS_ToPrimitive(ctx, val, HINT_STRING);
            if (JS_IsException(val1))
                return val1;
            ret = JS_ToStringInternal(ctx, val1, is_ToPropertyKey);
            JS_FreeValue(ctx, val1);
            return ret;
        }
            break;
        case JS_TAG_FUNCTION_BYTECODE:
            str = "[function bytecode]";
            goto new_string;
        case JS_TAG_SYMBOL:
            if (is_ToPropertyKey) {
                return JS_DupValue(ctx, val);
            } else {
                return JS_ThrowTypeError(ctx, "cannot convert symbol to string");
            }
        case JS_TAG_FLOAT64:
            return js_dtoa(ctx, JS_VALUE_GET_FLOAT64(val), 10, 0,
                           JS_DTOA_VAR_FORMAT);
#ifdef CONFIG_BIGNUM
            case JS_TAG_BIG_INT:
        return ctx->rt->bigint_ops.to_string(ctx, val);
    case JS_TAG_BIG_FLOAT:
        return ctx->rt->bigfloat_ops.to_string(ctx, val);
    case JS_TAG_BIG_DECIMAL:
        return ctx->rt->bigdecimal_ops.to_string(ctx, val);
#endif
        default:
            str = "[unsupported type]";
        new_string:
            return JS_NewString(ctx, str);
    }
}

JSValue JS_ToString(JSContext *ctx, JSValueConst val)
{
    return JS_ToStringInternal(ctx, val, FALSE);
}

 JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val)
{
    if (JS_IsUndefined(val) || JS_IsNull(val))
        return JS_ToStringFree(ctx, val);
    return JS_InvokeFree(ctx, val, JS_ATOM_toLocaleString, 0, NULL);
}

JSValue JS_ToPropertyKey(JSContext *ctx, JSValueConst val)
{
    return JS_ToStringInternal(ctx, val, TRUE);
}


int JS_ToBoolFree(JSContext *ctx, JSValue val)
{
    uint32_t tag = JS_VALUE_GET_TAG(val);
    switch(tag) {
        case JS_TAG_INT:
            return JS_VALUE_GET_INT(val) != 0;
        case JS_TAG_BOOL:
        case JS_TAG_NULL:
        case JS_TAG_UNDEFINED:
            return JS_VALUE_GET_INT(val);
        case JS_TAG_EXCEPTION:
            return -1;
        case JS_TAG_STRING:
        {
            BOOL ret = JS_VALUE_GET_STRING(val)->len != 0;
            JS_FreeValue(ctx, val);
            return ret;
        }
#ifdef CONFIG_BIGNUM
        case JS_TAG_BIG_INT:
        case JS_TAG_BIG_FLOAT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            BOOL ret;
            ret = p->num.expn != BF_EXP_ZERO && p->num.expn != BF_EXP_NAN;
            JS_FreeValue(ctx, val);
            return ret;
        }
        case JS_TAG_BIG_DECIMAL:
        {
            JSBigDecimal *p = JS_VALUE_GET_PTR(val);
            BOOL ret;
            ret = p->num.expn != BF_EXP_ZERO && p->num.expn != BF_EXP_NAN;
            JS_FreeValue(ctx, val);
            return ret;
        }
#endif
        case JS_TAG_OBJECT:
        {
            JSObject *p = JS_VALUE_GET_OBJ(val);
            BOOL ret;
            ret = !p->is_HTMLDDA;
            JS_FreeValue(ctx, val);
            return ret;
        }
            break;
        default:
            if (JS_TAG_IS_FLOAT64(tag)) {
                double d = JS_VALUE_GET_FLOAT64(val);
                return !isnan(d) && d != 0;
            } else {
                JS_FreeValue(ctx, val);
                return TRUE;
            }
    }
}

int JS_ToBool(JSContext *ctx, JSValueConst val)
{
    return JS_ToBoolFree(ctx, JS_DupValue(ctx, val));
}



/* same as JS_ToNumber() but return 0 in case of NaN/Undefined */
 __maybe_unused JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val)
{
    uint32_t tag;
    JSValue ret;

    redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
        case JS_TAG_INT:
        case JS_TAG_BOOL:
        case JS_TAG_NULL:
        case JS_TAG_UNDEFINED:
            ret = JS_NewInt32(ctx, JS_VALUE_GET_INT(val));
            break;
        case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                ret = JS_NewInt32(ctx, 0);
            } else {
                /* convert -0 to +0 */
                d = trunc(d) + 0.0;
                ret = JS_NewFloat64(ctx, d);
            }
        }
            break;
#ifdef CONFIG_BIGNUM
        case JS_TAG_BIG_FLOAT:
        {
            bf_t a_s, *a, r_s, *r = &r_s;
            BOOL is_nan;

            a = JS_ToBigFloat(ctx, &a_s, val);
            if (!bf_is_finite(a)) {
                is_nan = bf_is_nan(a);
                if (is_nan)
                    ret = JS_NewInt32(ctx, 0);
                else
                    ret = JS_DupValue(ctx, val);
            } else {
                ret = JS_NewBigInt(ctx);
                if (!JS_IsException(ret)) {
                    r = JS_GetBigInt(ret);
                    bf_set(r, a);
                    bf_rint(r, BF_RNDZ);
                    ret = JS_CompactBigInt(ctx, ret);
                }
            }
            if (a == &a_s)
                bf_delete(a);
            JS_FreeValue(ctx, val);
        }
            break;
#endif
        default:
            val = JS_ToNumberFree(ctx, val);
            if (JS_IsException(val))
                return val;
            goto redo;
    }
    return ret;
}

/* Note: the integer value is satured to 32 bits */
 int JS_ToInt32SatFree(JSContext *ctx, int *pres, JSValue val)
{
    uint32_t tag;
    int ret;

    redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
        case JS_TAG_INT:
        case JS_TAG_BOOL:
        case JS_TAG_NULL:
        case JS_TAG_UNDEFINED:
            ret = JS_VALUE_GET_INT(val);
            break;
        case JS_TAG_EXCEPTION:
            *pres = 0;
            return -1;
        case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                ret = 0;
            } else {
                if (d < INT32_MIN)
                    ret = INT32_MIN;
                else if (d > INT32_MAX)
                    ret = INT32_MAX;
                else
                    ret = (int)d;
            }
        }
            break;
#ifdef CONFIG_BIGNUM
        case JS_TAG_BIG_FLOAT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            bf_get_int32(&ret, &p->num, 0);
            JS_FreeValue(ctx, val);
        }
            break;
#endif
        default:
            val = JS_ToNumberFree(ctx, val);
            if (JS_IsException(val)) {
                *pres = 0;
                return -1;
            }
            goto redo;
    }
    *pres = ret;
    return 0;
}

int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val)
{
    return JS_ToInt32SatFree(ctx, pres, JS_DupValue(ctx, val));
}

int JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val,
                    int min, int max, int min_offset)
{
    int res = JS_ToInt32SatFree(ctx, pres, JS_DupValue(ctx, val));
    if (res == 0) {
        if (*pres < min) {
            *pres += min_offset;
            if (*pres < min)
                *pres = min;
        } else {
            if (*pres > max)
                *pres = max;
        }
    }
    return res;
}

 int JS_ToInt64SatFree(JSContext *ctx, int64_t *pres, JSValue val)
{
    uint32_t tag;

    redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
        case JS_TAG_INT:
        case JS_TAG_BOOL:
        case JS_TAG_NULL:
        case JS_TAG_UNDEFINED:
            *pres = JS_VALUE_GET_INT(val);
            return 0;
        case JS_TAG_EXCEPTION:
            *pres = 0;
            return -1;
        case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                *pres = 0;
            } else {
                if (d < INT64_MIN)
                    *pres = INT64_MIN;
                else if (d > INT64_MAX)
                    *pres = INT64_MAX;
                else
                    *pres = (int64_t)d;
            }
        }
            return 0;
#ifdef CONFIG_BIGNUM
        case JS_TAG_BIG_FLOAT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            bf_get_int64(pres, &p->num, 0);
            JS_FreeValue(ctx, val);
        }
            return 0;
#endif
        default:
            val = JS_ToNumberFree(ctx, val);
            if (JS_IsException(val)) {
                *pres = 0;
                return -1;
            }
            goto redo;
    }
}

int JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    return JS_ToInt64SatFree(ctx, pres, JS_DupValue(ctx, val));
}

int JS_ToInt64Clamp(JSContext *ctx, int64_t *pres, JSValueConst val,
                    int64_t min, int64_t max, int64_t neg_offset)
{
    int res = JS_ToInt64SatFree(ctx, pres, JS_DupValue(ctx, val));
    if (res == 0) {
        if (*pres < 0)
            *pres += neg_offset;
        if (*pres < min)
            *pres = min;
        else if (*pres > max)
            *pres = max;
    }
    return res;
}

/* Same as JS_ToInt32Free() but with a 64 bit result. Return (<0, 0)
   in case of exception */
static int JS_ToInt64Free(JSContext *ctx, int64_t *pres, JSValue val)
{
    uint32_t tag;
    int64_t ret;

    redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
        case JS_TAG_INT:
        case JS_TAG_BOOL:
        case JS_TAG_NULL:
        case JS_TAG_UNDEFINED:
            ret = JS_VALUE_GET_INT(val);
            break;
        case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            double d;
            int e;
            d = JS_VALUE_GET_FLOAT64(val);
            u.d = d;
            /* we avoid doing fmod(x, 2^64) */
            e = (u.u64 >> 52) & 0x7ff;
            if (likely(e <= (1023 + 62))) {
                /* fast case */
                ret = (int64_t)d;
            } else if (e <= (1023 + 62 + 53)) {
                uint64_t v;
                /* remainder modulo 2^64 */
                v = (u.u64 & (((uint64_t)1 << 52) - 1)) | ((uint64_t)1 << 52);
                ret = v << ((e - 1023) - 52);
                /* take the sign into account */
                if (u.u64 >> 63)
                    ret = -ret;
            } else {
                ret = 0; /* also handles NaN and +inf */
            }
        }
            break;
#ifdef CONFIG_BIGNUM
        case JS_TAG_BIG_FLOAT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            bf_get_int64(&ret, &p->num, BF_GET_INT_MOD);
            JS_FreeValue(ctx, val);
        }
            break;
#endif
        default:
            val = JS_ToNumberFree(ctx, val);
            if (JS_IsException(val)) {
                *pres = 0;
                return -1;
            }
            goto redo;
    }
    *pres = ret;
    return 0;
}

int JS_ToInt64(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    return JS_ToInt64Free(ctx, pres, JS_DupValue(ctx, val));
}

int JS_ToInt64Ext(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    if (JS_IsBigInt(ctx, val))
        return JS_ToBigInt64(ctx, pres, val);
    else
        return JS_ToInt64(ctx, pres, val);
}

/* return (<0, 0) in case of exception */
int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val)
{
    uint32_t tag;
    int32_t ret;

    redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
        case JS_TAG_INT:
        case JS_TAG_BOOL:
        case JS_TAG_NULL:
        case JS_TAG_UNDEFINED:
            ret = JS_VALUE_GET_INT(val);
            break;
        case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            double d;
            int e;
            d = JS_VALUE_GET_FLOAT64(val);
            u.d = d;
            /* we avoid doing fmod(x, 2^32) */
            e = (u.u64 >> 52) & 0x7ff;
            if (likely(e <= (1023 + 30))) {
                /* fast case */
                ret = (int32_t)d;
            } else if (e <= (1023 + 30 + 53)) {
                uint64_t v;
                /* remainder modulo 2^32 */
                v = (u.u64 & (((uint64_t)1 << 52) - 1)) | ((uint64_t)1 << 52);
                v = v << ((e - 1023) - 52 + 32);
                ret = v >> 32;
                /* take the sign into account */
                if (u.u64 >> 63)
                    ret = -ret;
            } else {
                ret = 0; /* also handles NaN and +inf */
            }
        }
            break;
#ifdef CONFIG_BIGNUM
        case JS_TAG_BIG_FLOAT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            bf_get_int32(&ret, &p->num, BF_GET_INT_MOD);
            JS_FreeValue(ctx, val);
        }
            break;
#endif
        default:
            val = JS_ToNumberFree(ctx, val);
            if (JS_IsException(val)) {
                *pres = 0;
                return -1;
            }
            goto redo;
    }
    *pres = ret;
    return 0;
}

int JS_ToInt32(JSContext *ctx, int32_t *pres, JSValueConst val)
{
    return JS_ToInt32Free(ctx, pres, JS_DupValue(ctx, val));
}



int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val)
{
    uint32_t tag;
    int res;

    redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
        case JS_TAG_INT:
        case JS_TAG_BOOL:
        case JS_TAG_NULL:
        case JS_TAG_UNDEFINED:
            res = JS_VALUE_GET_INT(val);
#ifdef CONFIG_BIGNUM
        int_clamp:
#endif
            res = max_int(0, min_int(255, res));
            break;
        case JS_TAG_FLOAT64:
        {
            double d = JS_VALUE_GET_FLOAT64(val);
            if (isnan(d)) {
                res = 0;
            } else {
                if (d < 0)
                    res = 0;
                else if (d > 255)
                    res = 255;
                else
                    res = lrint(d);
            }
        }
            break;
#ifdef CONFIG_BIGNUM
        case JS_TAG_BIG_FLOAT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            bf_t r_s, *r = &r_s;
            bf_init(ctx->bf_ctx, r);
            bf_set(r, &p->num);
            bf_rint(r, BF_RNDN);
            bf_get_int32(&res, r, 0);
            bf_delete(r);
            JS_FreeValue(ctx, val);
        }
            goto int_clamp;
#endif
        default:
            val = JS_ToNumberFree(ctx, val);
            if (JS_IsException(val)) {
                *pres = 0;
                return -1;
            }
            goto redo;
    }
    *pres = res;
    return 0;
}

 __exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen,
                                            JSValue val, BOOL is_array_ctor)
{
    uint32_t tag, len;

    tag = JS_VALUE_GET_TAG(val);
    switch(tag) {
        case JS_TAG_INT:
        case JS_TAG_BOOL:
        case JS_TAG_NULL:
        {
            int v;
            v = JS_VALUE_GET_INT(val);
            if (v < 0)
                goto fail;
            len = v;
        }
            break;
#ifdef CONFIG_BIGNUM
        case JS_TAG_BIG_INT:
        case JS_TAG_BIG_FLOAT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            bf_t a;
            BOOL res;
            bf_get_int32((int32_t *)&len, &p->num, BF_GET_INT_MOD);
            bf_init(ctx->bf_ctx, &a);
            bf_set_ui(&a, len);
            res = bf_cmp_eq(&a, &p->num);
            bf_delete(&a);
            JS_FreeValue(ctx, val);
            if (!res)
                goto fail;
        }
            break;
#endif
        default:
            if (JS_TAG_IS_FLOAT64(tag)) {
                double d;
                d = JS_VALUE_GET_FLOAT64(val);
                len = (uint32_t)d;
                if (len != d)
                    goto fail;
            } else {
                uint32_t len1;

                if (is_array_ctor) {
                    val = JS_ToNumberFree(ctx, val);
                    if (JS_IsException(val))
                        return -1;
                    /* cannot recurse because val is a number */
                    if (JS_ToArrayLengthFree(ctx, &len, val, TRUE))
                        return -1;
                } else {
                    /* legacy behavior: must do the conversion twice and compare */
                    if (JS_ToUint32(ctx, &len, val)) {
                        JS_FreeValue(ctx, val);
                        return -1;
                    }
                    val = JS_ToNumberFree(ctx, val);
                    if (JS_IsException(val))
                        return -1;
                    /* cannot recurse because val is a number */
                    if (JS_ToArrayLengthFree(ctx, &len1, val, FALSE))
                        return -1;
                    if (len1 != len) {
                        fail:
                        JS_ThrowRangeError(ctx, "invalid array length");
                        return -1;
                    }
                }
            }
            break;
    }
    *plen = len;
    return 0;
}




int JS_ToIndex(JSContext *ctx, uint64_t *plen, JSValueConst val)
{
    int64_t v;
    if (JS_ToInt64Sat(ctx, &v, val))
        return -1;
    if (v < 0 || v > MAX_SAFE_INTEGER) {
        JS_ThrowRangeError(ctx, "invalid array index");
        *plen = 0;
        return -1;
    }
    *plen = v;
    return 0;
}

/* convert a value to a length between 0 and MAX_SAFE_INTEGER.
   return -1 for exception */
 __exception int JS_ToLengthFree(JSContext *ctx, int64_t *plen,
                                       JSValue val)
{
    int res = JS_ToInt64Clamp(ctx, plen, val, 0, MAX_SAFE_INTEGER, 0);
    JS_FreeValue(ctx, val);
    return res;
}




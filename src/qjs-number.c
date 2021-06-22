//
// Created by benpeng.jiang on 2021/6/18.
//

#include <math.h>
#include "qjs-number.h"
#include "ecma-bignumber.h"

 int skip_spaces(const char *pc)
{
    const uint8_t *p, *p_next, *p_start;
    uint32_t c;

    p = p_start = (const uint8_t *)pc;
    for (;;) {
        c = *p;
        if (c < 128) {
            if (!((c >= 0x09 && c <= 0x0d) || (c == 0x20)))
                break;
            p++;
        } else {
            c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p_next);
            if (!lre_is_space(c))
                break;
            p = p_next;
        }
    }
    return p - p_start;
}

static inline int to_digit(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    else
        return 36;
}

/* XXX: remove */
static double js_strtod(const char *p, int radix, BOOL is_float)
{
    double d;
    int c;

    if (!is_float || radix != 10) {
        uint64_t n_max, n;
        int int_exp, is_neg;

        is_neg = 0;
        if (*p == '-') {
            is_neg = 1;
            p++;
        }

        /* skip leading zeros */
        while (*p == '0')
            p++;
        n = 0;
        if (radix == 10)
            n_max = ((uint64_t)-1 - 9) / 10; /* most common case */
        else
            n_max = ((uint64_t)-1 - (radix - 1)) / radix;
        /* XXX: could be more precise */
        int_exp = 0;
        while (*p != '\0') {
            c = to_digit((uint8_t)*p);
            if (c >= radix)
                break;
            if (n <= n_max) {
                n = n * radix + c;
            } else {
                int_exp++;
            }
            p++;
        }
        d = n;
        if (int_exp != 0) {
            d *= pow(radix, int_exp);
        }
        if (is_neg)
            d = -d;
    } else {
        d = strtod(p, NULL);
    }
    return d;
}


#ifdef CONFIG_BIGNUM
 JSValue js_string_to_bigint(JSContext *ctx, const char *buf,
                                   int radix, int flags, slimb_t *pexponent)
{
    bf_t a_s, *a = &a_s;
    int ret;
    JSValue val;
    val = JS_NewBigInt(ctx);
    if (JS_IsException(val))
        return val;
    a = JS_GetBigInt(val);
    ret = bf_atof(a, buf, NULL, radix, BF_PREC_INF, BF_RNDZ);
    if (ret & BF_ST_MEM_ERROR) {
        JS_FreeValue(ctx, val);
        return JS_ThrowOutOfMemory(ctx);
    }
    val = JS_CompactBigInt1(ctx, val, (flags & ATOD_MODE_BIGINT) != 0);
    return val;
}

 JSValue js_string_to_bigfloat(JSContext *ctx, const char *buf,
                                     int radix, int flags, slimb_t *pexponent)
{
    bf_t *a;
    int ret;
    JSValue val;

    val = JS_NewBigFloat(ctx);
    if (JS_IsException(val))
        return val;
    a = JS_GetBigFloat(val);
    if (flags & ATOD_ACCEPT_SUFFIX) {
        /* return the exponent to get infinite precision */
        ret = bf_atof2(a, pexponent, buf, NULL, radix, BF_PREC_INF,
                       BF_RNDZ | BF_ATOF_EXPONENT);
    } else {
        ret = bf_atof(a, buf, NULL, radix, ctx->fp_env.prec,
                      ctx->fp_env.flags);
    }
    if (ret & BF_ST_MEM_ERROR) {
        JS_FreeValue(ctx, val);
        return JS_ThrowOutOfMemory(ctx);
    }
    return val;
}

 JSValue js_string_to_bigdecimal(JSContext *ctx, const char *buf,
                                       int radix, int flags, slimb_t *pexponent)
{
    bfdec_t *a;
    int ret;
    JSValue val;

    val = JS_NewBigDecimal(ctx);
    if (JS_IsException(val))
        return val;
    a = JS_GetBigDecimal(val);
    ret = bfdec_atof(a, buf, NULL, BF_PREC_INF,
                     BF_RNDZ | BF_ATOF_NO_NAN_INF);
    if (ret & BF_ST_MEM_ERROR) {
        JS_FreeValue(ctx, val);
        return JS_ThrowOutOfMemory(ctx);
    }
    return val;
}

#endif

/* return an exception in case of memory error. Return JS_NAN if
   invalid syntax */
#ifdef CONFIG_BIGNUM
 JSValue js_atof2(JSContext *ctx, const char *str, const char **pp,
                        int radix, int flags, slimb_t *pexponent)
#else
JSValue js_atof(JSContext *ctx, const char *str, const char **pp,
                       int radix, int flags)
#endif
{
    const char *p, *p_start;
    int sep, is_neg;
    BOOL is_float, has_legacy_octal;
    int atod_type = flags & ATOD_TYPE_MASK;
    char buf1[64], *buf;
    int i, j, len;
    BOOL buf_allocated = FALSE;
    JSValue val;

    /* optional separator between digits */
    sep = (flags & ATOD_ACCEPT_UNDERSCORES) ? '_' : 256;
    has_legacy_octal = FALSE;

    p = str;
    p_start = p;
    is_neg = 0;
    if (p[0] == '+') {
        p++;
        p_start++;
        if (!(flags & ATOD_ACCEPT_PREFIX_AFTER_SIGN))
            goto no_radix_prefix;
    } else if (p[0] == '-') {
        p++;
        p_start++;
        is_neg = 1;
        if (!(flags & ATOD_ACCEPT_PREFIX_AFTER_SIGN))
            goto no_radix_prefix;
    }
    if (p[0] == '0') {
        if ((p[1] == 'x' || p[1] == 'X') &&
            (radix == 0 || radix == 16)) {
            p += 2;
            radix = 16;
        } else if ((p[1] == 'o' || p[1] == 'O') &&
                   radix == 0 && (flags & ATOD_ACCEPT_BIN_OCT)) {
            p += 2;
            radix = 8;
        } else if ((p[1] == 'b' || p[1] == 'B') &&
                   radix == 0 && (flags & ATOD_ACCEPT_BIN_OCT)) {
            p += 2;
            radix = 2;
        } else if ((p[1] >= '0' && p[1] <= '9') &&
                   radix == 0 && (flags & ATOD_ACCEPT_LEGACY_OCTAL)) {
            int i;
            has_legacy_octal = TRUE;
            sep = 256;
            for (i = 1; (p[i] >= '0' && p[i] <= '7'); i++)
                continue;
            if (p[i] == '8' || p[i] == '9')
                goto no_prefix;
            p += 1;
            radix = 8;
        } else {
            goto no_prefix;
        }
        /* there must be a digit after the prefix */
        if (to_digit((uint8_t)*p) >= radix)
            goto fail;
        no_prefix: ;
    } else {
        no_radix_prefix:
        if (!(flags & ATOD_INT_ONLY) &&
            (atod_type == ATOD_TYPE_FLOAT64 ||
             atod_type == ATOD_TYPE_BIG_FLOAT) &&
            strstart(p, "Infinity", &p)) {
#ifdef CONFIG_BIGNUM
            if (atod_type == ATOD_TYPE_BIG_FLOAT) {
                bf_t *a;
                val = JS_NewBigFloat(ctx);
                if (JS_IsException(val))
                    goto done;
                a = JS_GetBigFloat(val);
                bf_set_inf(a, is_neg);
            } else
#endif
            {
                double d = 1.0 / 0.0;
                if (is_neg)
                    d = -d;
                val = JS_NewFloat64(ctx, d);
            }
            goto done;
        }
    }
    if (radix == 0)
        radix = 10;
    is_float = FALSE;
    p_start = p;
    while (to_digit((uint8_t)*p) < radix
           ||  (*p == sep && (radix != 10 ||
                              p != p_start + 1 || p[-1] != '0') &&
                to_digit((uint8_t)p[1]) < radix)) {
        p++;
    }
    if (!(flags & ATOD_INT_ONLY)) {
        if (*p == '.' && (p > p_start || to_digit((uint8_t)p[1]) < radix)) {
            is_float = TRUE;
            p++;
            if (*p == sep)
                goto fail;
            while (to_digit((uint8_t)*p) < radix ||
                   (*p == sep && to_digit((uint8_t)p[1]) < radix))
                p++;
        }
        if (p > p_start &&
            (((*p == 'e' || *p == 'E') && radix == 10) ||
             ((*p == 'p' || *p == 'P') && (radix == 2 || radix == 8 || radix == 16)))) {
            const char *p1 = p + 1;
            is_float = TRUE;
            if (*p1 == '+') {
                p1++;
            } else if (*p1 == '-') {
                p1++;
            }
            if (is_digit((uint8_t)*p1)) {
                p = p1 + 1;
                while (is_digit((uint8_t)*p) || (*p == sep && is_digit((uint8_t)p[1])))
                    p++;
            }
        }
    }
    if (p == p_start)
        goto fail;

    buf = buf1;
    buf_allocated = FALSE;
    len = p - p_start;
    if (unlikely((len + 2) > sizeof(buf1))) {
        buf = js_malloc_rt(ctx->rt, len + 2); /* no exception raised */
        if (!buf)
            goto mem_error;
        buf_allocated = TRUE;
    }
    /* remove the separators and the radix prefixes */
    j = 0;
    if (is_neg)
        buf[j++] = '-';
    for (i = 0; i < len; i++) {
        if (p_start[i] != '_')
            buf[j++] = p_start[i];
    }
    buf[j] = '\0';

#ifdef CONFIG_BIGNUM
    if (flags & ATOD_ACCEPT_SUFFIX) {
        if (*p == 'n') {
            p++;
            atod_type = ATOD_TYPE_BIG_INT;
        } else if (*p == 'l') {
            p++;
            atod_type = ATOD_TYPE_BIG_FLOAT;
        } else if (*p == 'm') {
            p++;
            atod_type = ATOD_TYPE_BIG_DECIMAL;
        } else {
            if (flags & ATOD_MODE_BIGINT) {
                if (!is_float)
                    atod_type = ATOD_TYPE_BIG_INT;
                if (has_legacy_octal)
                    goto fail;
            } else {
                if (is_float && radix != 10)
                    goto fail;
            }
        }
    } else {
        if (atod_type == ATOD_TYPE_FLOAT64) {
            if (flags & ATOD_MODE_BIGINT) {
                if (!is_float)
                    atod_type = ATOD_TYPE_BIG_INT;
                if (has_legacy_octal)
                    goto fail;
            } else {
                if (is_float && radix != 10)
                    goto fail;
            }
        }
    }

    switch(atod_type) {
    case ATOD_TYPE_FLOAT64:
        {
            double d;
            d = js_strtod(buf, radix, is_float);
            /* return int or float64 */
            val = JS_NewFloat64(ctx, d);
        }
        break;
    case ATOD_TYPE_BIG_INT:
        if (has_legacy_octal || is_float)
            goto fail;
        val = ctx->rt->bigint_ops.from_string(ctx, buf, radix, flags, NULL);
        break;
    case ATOD_TYPE_BIG_FLOAT:
        if (has_legacy_octal)
            goto fail;
        val = ctx->rt->bigfloat_ops.from_string(ctx, buf, radix, flags,
                                                pexponent);
        break;
    case ATOD_TYPE_BIG_DECIMAL:
        if (radix != 10)
            goto fail;
        val = ctx->rt->bigdecimal_ops.from_string(ctx, buf, radix, flags, NULL);
        break;
    default:
        abort();
    }
#else
    {
        double d;
        (void)has_legacy_octal;
        if (is_float && radix != 10)
            goto fail;
        d = js_strtod(buf, radix, is_float);
        val = JS_NewFloat64(ctx, d);
    }
#endif

    done:
    if (buf_allocated)
        js_free_rt(ctx->rt, buf);
    if (pp)
        *pp = p;
    return val;
    fail:
    val = JS_NAN;
    goto done;
    mem_error:
    val = JS_ThrowOutOfMemory(ctx);
    goto done;
}

#ifdef CONFIG_BIGNUM
 JSValue js_atof(JSContext *ctx, const char *str, const char **pp,
                       int radix, int flags)
{
    return js_atof2(ctx, str, pp, radix, flags, NULL);
}
#endif

typedef enum JSToNumberHintEnum {
    TON_FLAG_NUMBER,
    TON_FLAG_NUMERIC,
} JSToNumberHintEnum;

static JSValue JS_ToNumberHintFree(JSContext *ctx, JSValue val,
                                   JSToNumberHintEnum flag)
{
    uint32_t tag;
    JSValue ret;

    redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
#ifdef CONFIG_BIGNUM
        case JS_TAG_BIG_DECIMAL:
        if (flag != TON_FLAG_NUMERIC) {
            JS_FreeValue(ctx, val);
            return JS_ThrowTypeError(ctx, "cannot convert bigdecimal to number");
        }
        ret = val;
        break;
    case JS_TAG_BIG_INT:
        if (flag != TON_FLAG_NUMERIC) {
            JS_FreeValue(ctx, val);
            return JS_ThrowTypeError(ctx, "cannot convert bigint to number");
        }
        ret = val;
        break;
    case JS_TAG_BIG_FLOAT:
        if (flag != TON_FLAG_NUMERIC) {
            JS_FreeValue(ctx, val);
            return JS_ThrowTypeError(ctx, "cannot convert bigfloat to number");
        }
        ret = val;
        break;
#endif
        case JS_TAG_FLOAT64:
        case JS_TAG_INT:
        case JS_TAG_EXCEPTION:
            ret = val;
            break;
        case JS_TAG_BOOL:
        case JS_TAG_NULL:
            ret = JS_NewInt32(ctx, JS_VALUE_GET_INT(val));
            break;
        case JS_TAG_UNDEFINED:
            ret = JS_NAN;
            break;
        case JS_TAG_OBJECT:
            val = JS_ToPrimitiveFree(ctx, val, HINT_NUMBER);
            if (JS_IsException(val))
                return JS_EXCEPTION;
            goto redo;
        case JS_TAG_STRING:
        {
            const char *str;
            const char *p;
            size_t len;

            str = JS_ToCStringLen(ctx, &len, val);
            JS_FreeValue(ctx, val);
            if (!str)
                return JS_EXCEPTION;
            p = str;
            p += skip_spaces(p);
            if ((p - str) == len) {
                ret = JS_NewInt32(ctx, 0);
            } else {
                int flags = ATOD_ACCEPT_BIN_OCT;
                ret = js_atof(ctx, p, &p, 0, flags);
                if (!JS_IsException(ret)) {
                    p += skip_spaces(p);
                    if ((p - str) != len) {
                        JS_FreeValue(ctx, ret);
                        ret = JS_NAN;
                    }
                }
            }
            JS_FreeCString(ctx, str);
        }
            break;
        case JS_TAG_SYMBOL:
            JS_FreeValue(ctx, val);
            return JS_ThrowTypeError(ctx, "cannot convert symbol to number");
        default:
            JS_FreeValue(ctx, val);
            ret = JS_NAN;
            break;
    }
    return ret;
}

 JSValue JS_ToNumberFree(JSContext *ctx, JSValue val)
{
    return JS_ToNumberHintFree(ctx, val, TON_FLAG_NUMBER);
}

 JSValue JS_ToNumericFree(JSContext *ctx, JSValue val)
{
    return JS_ToNumberHintFree(ctx, val, TON_FLAG_NUMERIC);
}

JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val)
{
    return JS_ToNumericFree(ctx, JS_DupValue(ctx, val));
}

static __exception int __JS_ToFloat64Free(JSContext *ctx, double *pres,
                                          JSValue val)
{
    double d;
    uint32_t tag;

    val = JS_ToNumberFree(ctx, val);
    if (JS_IsException(val)) {
        *pres = JS_FLOAT64_NAN;
        return -1;
    }
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
        case JS_TAG_INT:
            d = JS_VALUE_GET_INT(val);
            break;
        case JS_TAG_FLOAT64:
            d = JS_VALUE_GET_FLOAT64(val);
            break;
#ifdef CONFIG_BIGNUM
            case JS_TAG_BIG_INT:
    case JS_TAG_BIG_FLOAT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            /* XXX: there can be a double rounding issue with some
               primitives (such as JS_ToUint8ClampFree()), but it is
               not critical to fix it. */
            bf_get_float64(&p->num, &d, BF_RNDN);
            JS_FreeValue(ctx, val);
        }
        break;
#endif
        default:
            abort();
    }
    *pres = d;
    return 0;
}

 inline int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val)
{
    uint32_t tag;

    tag = JS_VALUE_GET_TAG(val);
    if (tag <= JS_TAG_NULL) {
        *pres = JS_VALUE_GET_INT(val);
        return 0;
    } else if (JS_TAG_IS_FLOAT64(tag)) {
        *pres = JS_VALUE_GET_FLOAT64(val);
        return 0;
    } else {
        return __JS_ToFloat64Free(ctx, pres, val);
    }
}

int JS_ToFloat64(JSContext *ctx, double *pres, JSValueConst val)
{
    return JS_ToFloat64Free(ctx, pres, JS_DupValue(ctx, val));
}

JSValue JS_ToNumber(JSContext *ctx, JSValueConst val)
{
    return JS_ToNumberFree(ctx, JS_DupValue(ctx, val));
}




/* Note: can return an exception */
 int JS_NumberIsInteger(JSContext *ctx, JSValueConst val)
{
    double d;
    if (!JS_IsNumber(val))
        return FALSE;
    if (unlikely(JS_ToFloat64(ctx, &d, val)))
        return -1;
    return isfinite(d) && floor(d) == d;
}

 BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val)
{
    uint32_t tag;

    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
        case JS_TAG_INT:
        {
            int v;
            v = JS_VALUE_GET_INT(val);
            return (v < 0);
        }
        case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            u.d = JS_VALUE_GET_FLOAT64(val);
            return (u.u64 >> 63);
        }
#ifdef CONFIG_BIGNUM
        case JS_TAG_BIG_INT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            /* Note: integer zeros are not necessarily positive */
            return p->num.sign && !bf_is_zero(&p->num);
        }
        case JS_TAG_BIG_FLOAT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            return p->num.sign;
        }
            break;
        case JS_TAG_BIG_DECIMAL:
        {
            JSBigDecimal *p = JS_VALUE_GET_PTR(val);
            return p->num.sign;
        }
            break;
#endif
        default:
            return FALSE;
    }
}

#ifdef CONFIG_BIGNUM

 JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix)
{
    JSValue ret;
    bf_t a_s, *a;
    char *str;
    int saved_sign;

    a = JS_ToBigInt(ctx, &a_s, val);
    if (!a)
        return JS_EXCEPTION;
    saved_sign = a->sign;
    if (a->expn == BF_EXP_ZERO)
        a->sign = 0;
    str = bf_ftoa(NULL, a, radix, 0, BF_RNDZ | BF_FTOA_FORMAT_FRAC |
                                     BF_FTOA_JS_QUIRKS);
    a->sign = saved_sign;
    JS_FreeBigInt(ctx, a, &a_s);
    if (!str)
        return JS_ThrowOutOfMemory(ctx);
    ret = JS_NewString(ctx, str);
    bf_free(ctx->bf_ctx, str);
    return ret;
}

 JSValue js_bigint_to_string(JSContext *ctx, JSValueConst val)
{
    return js_bigint_to_string1(ctx, val, 10);
}

 JSValue js_ftoa(JSContext *ctx, JSValueConst val1, int radix,
                       limb_t prec, bf_flags_t flags)
{
    JSValue val, ret;
    bf_t a_s, *a;
    char *str;
    int saved_sign;

    val = JS_ToNumeric(ctx, val1);
    if (JS_IsException(val))
        return val;
    a = JS_ToBigFloat(ctx, &a_s, val);
    saved_sign = a->sign;
    if (a->expn == BF_EXP_ZERO)
        a->sign = 0;
    flags |= BF_FTOA_JS_QUIRKS;
    if ((flags & BF_FTOA_FORMAT_MASK) == BF_FTOA_FORMAT_FREE_MIN) {
        /* Note: for floating point numbers with a radix which is not
           a power of two, the current precision is used to compute
           the number of digits. */
        if ((radix & (radix - 1)) != 0) {
            bf_t r_s, *r = &r_s;
            int prec, flags1;
            /* must round first */
            if (JS_VALUE_GET_TAG(val) == JS_TAG_BIG_FLOAT) {
                prec = ctx->fp_env.prec;
                flags1 = ctx->fp_env.flags &
                         (BF_FLAG_SUBNORMAL | (BF_EXP_BITS_MASK << BF_EXP_BITS_SHIFT));
            } else {
                prec = 53;
                flags1 = bf_set_exp_bits(11) | BF_FLAG_SUBNORMAL;
            }
            bf_init(ctx->bf_ctx, r);
            bf_set(r, a);
            bf_round(r, prec, flags1 | BF_RNDN);
            str = bf_ftoa(NULL, r, radix, prec, flags1 | flags);
            bf_delete(r);
        } else {
            str = bf_ftoa(NULL, a, radix, BF_PREC_INF, flags);
        }
    } else {
        str = bf_ftoa(NULL, a, radix, prec, flags);
    }
    a->sign = saved_sign;
    if (a == &a_s)
        bf_delete(a);
    JS_FreeValue(ctx, val);
    if (!str)
        return JS_ThrowOutOfMemory(ctx);
    ret = JS_NewString(ctx, str);
    bf_free(ctx->bf_ctx, str);
    return ret;
}

 JSValue js_bigfloat_to_string(JSContext *ctx, JSValueConst val)
{
    return js_ftoa(ctx, val, 10, 0, BF_RNDN | BF_FTOA_FORMAT_FREE_MIN);
}

 JSValue js_bigdecimal_to_string1(JSContext *ctx, JSValueConst val,
                                        limb_t prec, int flags)
{
    JSValue ret;
    bfdec_t *a;
    char *str;
    int saved_sign;

    a = JS_ToBigDecimal(ctx, val);
    saved_sign = a->sign;
    if (a->expn == BF_EXP_ZERO)
        a->sign = 0;
    str = bfdec_ftoa(NULL, a, prec, flags | BF_FTOA_JS_QUIRKS);
    a->sign = saved_sign;
    if (!str)
        return JS_ThrowOutOfMemory(ctx);
    ret = JS_NewString(ctx, str);
    bf_free(ctx->bf_ctx, str);
    return ret;
}

 JSValue js_bigdecimal_to_string(JSContext *ctx, JSValueConst val)
{
    return js_bigdecimal_to_string1(ctx, val, 0,
                                    BF_RNDZ | BF_FTOA_FORMAT_FREE);
}

#endif /* CONFIG_BIGNUM */



//
// Created by benpeng.jiang on 2021/6/2.
//
#include "quickjs.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <fenv.h>
#include <math.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__)
#include <malloc.h>
#endif

#include "jsstring.h"
typedef enum JSToNumberHintEnum {
    TON_FLAG_NUMBER,
    TON_FLAG_NUMERIC,
} JSToNumberHintEnum;

static JSValue JS_ToNumberHintFree(JSContext *ctx, JSValue val,
                                   JSToNumberHintEnum flag) {
    uint32_t tag;
    JSValue ret;

    redo:
    tag = JS_VALUE_GET_NORM_TAG(val);

    return ret;
}
static JSValue JS_ToNumberFree(JSContext *ctx, JSValue val)
{
    return JS_ToNumberHintFree(ctx, val, TON_FLAG_NUMBER);
}

static JSValue JS_ToNumericFree(JSContext *ctx, JSValue val)
{
    return JS_ToNumberHintFree(ctx, val, TON_FLAG_NUMERIC);
}

static JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val)
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
        default:
            abort();
    }
    *pres = d;
    return 0;
}

static inline int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val)
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

/* Note: the string contents are uninitialized */
static JSString *js_alloc_string_rt(JSRuntime *rt, int max_len, int is_wide_char)
{
    JSString *str;
    str = js_malloc_rt(rt, sizeof(JSString) + (max_len << is_wide_char) + 1 - is_wide_char);
    if (unlikely(!str))
        return NULL;
    str->header.ref_count = 1;
    str->is_wide_char = is_wide_char;
    str->len = max_len;
    str->atom_type = 0;
    str->hash = 0;          /* optional but costless */
    str->hash_next = 0;     /* optional */
    return str;
}

static JSString *js_alloc_string(JSContext *ctx, int max_len, int is_wide_char)
{
    JSString *p;
    p = js_alloc_string_rt(ctx->rt, max_len, is_wide_char);
    if (unlikely(!p)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return p;
}

JSValue JS_NewString(JSContext *ctx, const char *str)
{
    return JS_NewStringLen(ctx, str, strlen(str));
}

/* create a string from a UTF-8 buffer */
JSValue JS_NewStringLen(JSContext *ctx, const char *buf, size_t buf_len)
{
    const uint8_t *p, *p_end, *p_start, *p_next;
    uint32_t c;
    size_t len1;

    p_start = (const uint8_t *)buf;
    p_end = p_start + buf_len;
    p = p_start;

    return JS_EXCEPTION;
}


JSValue JS_ToStringInternal(JSContext *ctx, JSValueConst val, BOOL is_ToPropertyKey) {
    uint32_t tag;
    const char *str;
    char buf[32];

    tag = JS_VALUE_GET_NORM_TAG(val);


    new_string:
    return JS_NewString(ctx, str);
}

JSValue JS_ToString(JSContext *ctx, JSValueConst val)
{
    return JS_ToStringInternal(ctx, val, FALSE);
}


/* return (NULL, 0) if exception. */
/* return pointer into a JSString with a live ref_count */
/* cesu8 determines if non-BMP1 codepoints are encoded as 1 or 2 utf-8 sequences */
const char *JS_ToCStringLen2(JSContext *ctx, size_t *plen, JSValueConst val1, BOOL cesu8)
{
    JSValue val;
    JSString *str, *str_new;
    int pos, len, c, c1;
    uint8_t *q;

    if (JS_VALUE_GET_TAG(val1) != JS_TAG_STRING) {
        val = JS_ToString(ctx, val1);
        if (JS_IsException(val))
            goto fail;
    } else {
        val = JS_DupValue(ctx, val1);
    }

    str = JS_VALUE_GET_STRING(val);
    len = str->len;
    if (!str->is_wide_char) {
        const uint8_t *src = str->u.str8;
        int count;

        /* count the number of non-ASCII characters */
        /* Scanning the whole string is required for ASCII strings,
           and computing the number of non-ASCII bytes is less expensive
           than testing each byte, hence this method is faster for ASCII
           strings, which is the most common case.
         */
        count = 0;
        for (pos = 0; pos < len; pos++) {
            count += src[pos] >> 7;
        }
        if (count == 0) {
            if (plen)
                *plen = len;
            return (const char *)src;
        }
        str_new = js_alloc_string(ctx, len + count, 0);
        if (!str_new)
            goto fail;
        q = str_new->u.str8;
        for (pos = 0; pos < len; pos++) {
            c = src[pos];
            if (c < 0x80) {
                *q++ = c;
            } else {
                *q++ = (c >> 6) | 0xc0;
                *q++ = (c & 0x3f) | 0x80;
            }
        }
    } else {
        const uint16_t *src = str->u.str16;
        /* Allocate 3 bytes per 16 bit code point. Surrogate pairs may
           produce 4 bytes but use 2 code points.
         */
        str_new = js_alloc_string(ctx, len * 3, 0);
        if (!str_new)
            goto fail;
        q = str_new->u.str8;
        pos = 0;
        while (pos < len) {
            c = src[pos++];
            if (c < 0x80) {
                *q++ = c;
            } else {
                if (c >= 0xd800 && c < 0xdc00) {
                    if (pos < len && !cesu8) {
                        c1 = src[pos];
                        if (c1 >= 0xdc00 && c1 < 0xe000) {
                            pos++;
                            /* surrogate pair */
                            c = (((c & 0x3ff) << 10) | (c1 & 0x3ff)) + 0x10000;
                        } else {
                            /* Keep unmatched surrogate code points */
                            /* c = 0xfffd; */ /* error */
                        }
                    } else {
                        /* Keep unmatched surrogate code points */
                        /* c = 0xfffd; */ /* error */
                    }
                }
                q += unicode_to_utf8(q, c);
            }
        }
    }

    *q = '\0';
    str_new->len = q - str_new->u.str8;
    JS_FreeValue(ctx, val);
    if (plen)
        *plen = str_new->len;
    return (const char *)str_new->u.str8;
    fail:
    if (plen)
        *plen = 0;
    return NULL;
}

void *js_malloc_rt(JSRuntime *rt, size_t size)
{
    return rt->mf.js_malloc(&rt->malloc_state, size);
}

/* Throw out of memory in case of error */
void *js_malloc(JSContext *ctx, size_t size)
{
    void *ptr;
    ptr = js_malloc_rt(ctx->rt, size);
    if (unlikely(!ptr)) {

        return NULL;
    }
    return ptr;
}

void js_free(JSContext *ctx, void *ptr)
{
    js_free_rt(ctx->rt, ptr);
}

JSValue JS_ThrowOutOfMemory(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;

    return JS_EXCEPTION;
}

void __JS_FreeValue(JSContext *ctx, JSValue v)
{

}
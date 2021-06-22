//
// Created by benpeng.jiang on 2021/6/18.
//

#include "qjs-string.h"
#include <stdlib.h>
#include <string.h>
#include "qjs-stringbuffer.h"

static int memcmp16_8(const uint16_t *src1, const uint8_t *src2, int len)
{
    int c, i;
    for(i = 0; i < len; i++) {
        c = src1[i] - src2[i];
        if (c != 0)
            return c;
    }
    return 0;
}

static int memcmp16(const uint16_t *src1, const uint16_t *src2, int len)
{
    int c, i;
    for(i = 0; i < len; i++) {
        c = src1[i] - src2[i];
        if (c != 0)
            return c;
    }
    return 0;
}

int js_string_memcmp(const JSString *p1, const JSString *p2, int len)
{
    int res;

    if (likely(!p1->is_wide_char)) {
        if (likely(!p2->is_wide_char))
            res = memcmp(p1->u.str8, p2->u.str8, len);
        else
            res = -memcmp16_8(p2->u.str16, p1->u.str8, len);
    } else {
        if (!p2->is_wide_char)
            res = memcmp16_8(p1->u.str16, p2->u.str8, len);
        else
            res = memcmp16(p1->u.str16, p2->u.str16, len);
    }
    return res;
}

/* return < 0, 0 or > 0 */
int js_string_compare(JSContext *ctx,
                             const JSString *p1, const JSString *p2)
{
    int res, len;
    len = min_int(p1->len, p2->len);
    res = js_string_memcmp(p1, p2, len);
    if (res == 0) {
        if (p1->len == p2->len)
            res = 0;
        else if (p1->len < p2->len)
            res = -1;
        else
            res = 1;
    }
    return res;
}

static void copy_str16(uint16_t *dst, const JSString *p, int offset, int len)
{
    if (p->is_wide_char) {
        memcpy(dst, p->u.str16 + offset, len * 2);
    } else {
        const uint8_t *src1 = p->u.str8 + offset;
        int i;

        for(i = 0; i < len; i++)
            dst[i] = src1[i];
    }
}

/* same as JS_FreeValueRT() but faster */
void js_free_string(JSRuntime *rt, JSString *str)
{
    if (--str->header.ref_count <= 0) {
        if (str->atom_type) {
            JS_FreeAtomStruct(rt, str);
        } else {
#ifdef DUMP_LEAKS
            list_del(&str->link);
#endif
            js_free_rt(rt, str);
        }
    }
}


void JS_FreeCString(JSContext *ctx, const char *ptr)
{
    JSString *p;
    if (!ptr)
        return;
    /* purposely removing constness */
    p = (JSString *)(void *)(ptr - offsetof(JSString, u));
    JS_FreeValue(ctx, JS_MKPTR(JS_TAG_STRING, p));
}


static JSValue JS_ConcatString1(JSContext *ctx,
                                const JSString *p1, const JSString *p2)
{
    JSString *p;
    uint32_t len;
    int is_wide_char;

    len = p1->len + p2->len;
    if (len > JS_STRING_LEN_MAX)
        return JS_ThrowInternalError(ctx, "string too long");
    is_wide_char = p1->is_wide_char | p2->is_wide_char;
    p = js_alloc_string(ctx, len, is_wide_char);
    if (!p)
        return JS_EXCEPTION;
    if (!is_wide_char) {
        memcpy(p->u.str8, p1->u.str8, p1->len);
        memcpy(p->u.str8 + p1->len, p2->u.str8, p2->len);
        p->u.str8[len] = '\0';
    } else {
        copy_str16(p->u.str16, p1, 0, p1->len);
        copy_str16(p->u.str16 + p1->len, p2, 0, p2->len);
    }
    return JS_MKPTR(JS_TAG_STRING, p);
}

/* op1 and op2 are converted to strings. For convience, op1 or op2 =
   JS_EXCEPTION are accepted and return JS_EXCEPTION.  */
 JSValue JS_ConcatString(JSContext *ctx, JSValue op1, JSValue op2)
{
    JSValue ret;
    JSString *p1, *p2;

    if (unlikely(JS_VALUE_GET_TAG(op1) != JS_TAG_STRING)) {
        op1 = JS_ToStringFree(ctx, op1);
        if (JS_IsException(op1)) {
            JS_FreeValue(ctx, op2);
            return JS_EXCEPTION;
        }
    }
    if (unlikely(JS_VALUE_GET_TAG(op2) != JS_TAG_STRING)) {
        op2 = JS_ToStringFree(ctx, op2);
        if (JS_IsException(op2)) {
            JS_FreeValue(ctx, op1);
            return JS_EXCEPTION;
        }
    }
    p1 = JS_VALUE_GET_STRING(op1);
    p2 = JS_VALUE_GET_STRING(op2);

    /* XXX: could also check if p1 is empty */
    if (p2->len == 0) {
        goto ret_op1;
    }
    if (p1->header.ref_count == 1 && p1->is_wide_char == p2->is_wide_char
        &&  js_malloc_usable_size(ctx, p1) >= sizeof(*p1) + ((p1->len + p2->len) << p2->is_wide_char) + 1 - p1->is_wide_char) {
        /* Concatenate in place in available space at the end of p1 */
        if (p1->is_wide_char) {
            memcpy(p1->u.str16 + p1->len, p2->u.str16, p2->len << 1);
            p1->len += p2->len;
        } else {
            memcpy(p1->u.str8 + p1->len, p2->u.str8, p2->len);
            p1->len += p2->len;
            p1->u.str8[p1->len] = '\0';
        }
        ret_op1:
        JS_FreeValue(ctx, op2);
        return op1;
    }
    ret = JS_ConcatString1(ctx, p1, p2);
    JS_FreeValue(ctx, op1);
    JS_FreeValue(ctx, op2);
    return ret;
}





 int string_get(const JSString *p, int idx) {
    return p->is_wide_char ? p->u.str16[idx] : p->u.str8[idx];
}

 int string_getc(const JSString *p, int *pidx)
{
    int idx, c, c1;
    idx = *pidx;
    if (p->is_wide_char) {
        c = p->u.str16[idx++];
        if (c >= 0xd800 && c < 0xdc00 && idx < p->len) {
            c1 = p->u.str16[idx];
            if (c1 >= 0xdc00 && c1 < 0xe000) {
                c = (((c & 0x3ff) << 10) | (c1 & 0x3ff)) + 0x10000;
                idx++;
            }
        }
    } else {
        c = p->u.str8[idx++];
    }
    *pidx = idx;
    return c;
}


/* create a string from a UTF-8 buffer */
JSValue JS_NewStringLen(JSContext *ctx, const char *buf, size_t buf_len)
{
    const uint8_t *p, *p_end, *p_start, *p_next;
    uint32_t c;
    StringBuffer b_s, *b = &b_s;
    size_t len1;

    p_start = (const uint8_t *)buf;
    p_end = p_start + buf_len;
    p = p_start;
    while (p < p_end && *p < 128)
        p++;
    len1 = p - p_start;
    if (len1 > JS_STRING_LEN_MAX)
        return JS_ThrowInternalError(ctx, "string too long");
    if (p == p_end) {
        /* ASCII string */
        return js_new_string8(ctx, (const uint8_t *)buf, buf_len);
    } else {
        if (string_buffer_init(ctx, b, buf_len))
            goto fail;
        string_buffer_write8(b, p_start, len1);
        while (p < p_end) {
            if (*p < 128) {
                string_buffer_putc8(b, *p++);
            } else {
                /* parse utf-8 sequence, return 0xFFFFFFFF for error */
                c = unicode_from_utf8(p, p_end - p, &p_next);
                if (c < 0x10000) {
                    p = p_next;
                } else if (c <= 0x10FFFF) {
                    p = p_next;
                    /* surrogate pair */
                    c -= 0x10000;
                    string_buffer_putc16(b, (c >> 10) + 0xd800);
                    c = (c & 0x3ff) + 0xdc00;
                } else {
                    /* invalid char */
                    c = 0xfffd;
                    /* skip the invalid chars */
                    /* XXX: seems incorrect. Why not just use c = *p++; ? */
                    while (p < p_end && (*p >= 0x80 && *p < 0xc0))
                        p++;
                    if (p < p_end) {
                        p++;
                        while (p < p_end && (*p >= 0x80 && *p < 0xc0))
                            p++;
                    }
                }
                string_buffer_putc16(b, c);
            }
        }
    }
    return string_buffer_end(b);

    fail:
    string_buffer_free(b);
    return JS_EXCEPTION;
}

 JSValue JS_ConcatString3(JSContext *ctx, const char *str1,
                                JSValue str2, const char *str3)
{
    StringBuffer b_s, *b = &b_s;
    int len1, len3;
    JSString *p;

    if (unlikely(JS_VALUE_GET_TAG(str2) != JS_TAG_STRING)) {
        str2 = JS_ToStringFree(ctx, str2);
        if (JS_IsException(str2))
            goto fail;
    }
    p = JS_VALUE_GET_STRING(str2);
    len1 = strlen(str1);
    len3 = strlen(str3);

    if (string_buffer_init2(ctx, b, len1 + p->len + len3, p->is_wide_char))
        goto fail;

    string_buffer_write8(b, (const uint8_t *)str1, len1);
    string_buffer_concat(b, p, 0, p->len);
    string_buffer_write8(b, (const uint8_t *)str3, len3);

    JS_FreeValue(ctx, str2);
    return string_buffer_end(b);

    fail:
    JS_FreeValue(ctx, str2);
    return JS_EXCEPTION;
}

JSValue JS_NewString(JSContext *ctx, const char *str)
{
    return JS_NewStringLen(ctx, str, strlen(str));
}

JSValue JS_NewAtomString(JSContext *ctx, const char *str)
{
    JSAtom atom = JS_NewAtom(ctx, str);
    if (atom == JS_ATOM_NULL)
        return JS_EXCEPTION;
    JSValue val = JS_AtomToString(ctx, atom);
    JS_FreeAtom(ctx, atom);
    return val;
}

 JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val)
{
    uint32_t tag = JS_VALUE_GET_TAG(val);
    if (tag == JS_TAG_NULL || tag == JS_TAG_UNDEFINED)
        return JS_ThrowTypeError(ctx, "null or undefined are forbidden");
    return JS_ToString(ctx, val);
}

 JSValue JS_ToQuotedString(JSContext *ctx, JSValueConst val1)
{
    JSValue val;
    JSString *p;
    int i;
    uint32_t c;
    StringBuffer b_s, *b = &b_s;
    char buf[16];

    val = JS_ToStringCheckObject(ctx, val1);
    if (JS_IsException(val))
        return val;
    p = JS_VALUE_GET_STRING(val);

    if (string_buffer_init(ctx, b, p->len + 2))
        goto fail;

    if (string_buffer_putc8(b, '\"'))
        goto fail;
    for(i = 0; i < p->len; ) {
        c = string_getc(p, &i);
        switch(c) {
            case '\t':
                c = 't';
                goto quote;
            case '\r':
                c = 'r';
                goto quote;
            case '\n':
                c = 'n';
                goto quote;
            case '\b':
                c = 'b';
                goto quote;
            case '\f':
                c = 'f';
                goto quote;
            case '\"':
            case '\\':
            quote:
                if (string_buffer_putc8(b, '\\'))
                    goto fail;
                if (string_buffer_putc8(b, c))
                    goto fail;
                break;
            default:
                if (c < 32 || (c >= 0xd800 && c < 0xe000)) {
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    if (string_buffer_puts8(b, buf))
                        goto fail;
                } else {
                    if (string_buffer_putc(b, c))
                        goto fail;
                }
                break;
        }
    }
    if (string_buffer_putc8(b, '\"'))
        goto fail;
    JS_FreeValue(ctx, val);
    return string_buffer_end(b);
    fail:
    JS_FreeValue(ctx, val);
    string_buffer_free(b);
    return JS_EXCEPTION;
}
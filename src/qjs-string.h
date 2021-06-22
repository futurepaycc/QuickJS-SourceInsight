//
// Created by benpeng.jiang on 2021/6/18.
//

#ifndef LOX_JS_STRING_H
#define LOX_JS_STRING_H

#include "qjs-api.h"
#include "ecma-type-conversion.h"
#include "qjs-stringbuffer.h"

int js_string_memcmp(const JSString *p1, const JSString *p2, int len);

int js_string_compare(JSContext *ctx,
                      const JSString *p1, const JSString *p2);

void js_free_string(JSRuntime *rt, JSString *str);

void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p);

static inline uint32_t atom_get_free(const JSAtomStruct *p) {
    return (uintptr_t) p >> 1;
}

static inline BOOL atom_is_free(const JSAtomStruct *p) {
    return (uintptr_t) p & 1;
}

/* Note: the string contents are uninitialized */
static JSString *js_alloc_string_rt(JSRuntime *rt, int max_len, int is_wide_char) {
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
#ifdef DUMP_LEAKS
    list_add_tail(&str->link, &rt->string_list);
#endif
    return str;
}

static JSString *js_alloc_string(JSContext *ctx, int max_len, int is_wide_char) {
    JSString *p;
    p = js_alloc_string_rt(ctx->rt, max_len, is_wide_char);
    if (unlikely(!p)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return p;
}

JSValue JS_ToQuotedString(JSContext *ctx, JSValueConst val1);

JSValue js_new_string8(JSContext *ctx, const uint8_t *buf, int len);

JSValue JS_ConcatString(JSContext *ctx, JSValue op1, JSValue op2);

JSValue JS_ConcatString3(JSContext *ctx, const char *str1,
                         JSValue str2, const char *str3);

JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val);

int string_get(const JSString *p, int idx);

int string_getc(const JSString *p, int *pidx);

uint32_t hash_string(const JSString *str, uint32_t h);

JSValue js_sub_string(JSContext *ctx, JSString *p, int start, int end);

#endif //LOX_JS_STRING_H

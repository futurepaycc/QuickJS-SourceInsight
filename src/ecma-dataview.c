//
// Created by benpeng.jiang on 2021/7/24.
//

#include "ecma-dataview.h"

#include "qjs-api.h"
#include "ecma-array.h"

 JSValue js_dataview_constructor(JSContext *ctx,
                                       JSValueConst new_target,
                                       int argc, JSValueConst *argv)
{
    JSArrayBuffer *abuf;
    uint64_t offset;
    uint32_t len;
    JSValueConst buffer;
    JSValue obj;
    JSTypedArray *ta;
    JSObject *p;

    buffer = argv[0];
    abuf = js_get_array_buffer(ctx, buffer);
    if (!abuf)
        return JS_EXCEPTION;
    offset = 0;
    if (argc > 1) {
        if (JS_ToIndex(ctx, &offset, argv[1]))
            return JS_EXCEPTION;
    }
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    if (offset > abuf->byte_length)
        return JS_ThrowRangeError(ctx, "invalid byteOffset");
    len = abuf->byte_length - offset;
    if (argc > 2 && !JS_IsUndefined(argv[2])) {
        uint64_t l;
        if (JS_ToIndex(ctx, &l, argv[2]))
            return JS_EXCEPTION;
        if (l > len)
            return JS_ThrowRangeError(ctx, "invalid byteLength");
        len = l;
    }

    obj = js_create_from_ctor(ctx, new_target, JS_CLASS_DATAVIEW);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    if (abuf->detached) {
        /* could have been detached in js_create_from_ctor() */
        JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
        goto fail;
    }
    ta = js_malloc(ctx, sizeof(*ta));
    if (!ta) {
        fail:
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    p = JS_VALUE_GET_OBJ(obj);
    ta->obj = p;
    ta->buffer = JS_VALUE_GET_OBJ(JS_DupValue(ctx, buffer));
    ta->offset = offset;
    ta->length = len;
    list_add_tail(&ta->link, &abuf->array_list);
    p->u.typed_array = ta;
    return obj;
}

 JSValue js_dataview_getValue(JSContext *ctx,
                                    JSValueConst this_obj,
                                    int argc, JSValueConst *argv, int class_id)
{
    JSTypedArray *ta;
    JSArrayBuffer *abuf;
    int is_swap, size;
    uint8_t *ptr;
    uint32_t v;
    uint64_t pos;

    ta = JS_GetOpaque2(ctx, this_obj, JS_CLASS_DATAVIEW);
    if (!ta)
        return JS_EXCEPTION;
    size = 1 << typed_array_size_log2(class_id);
    if (JS_ToIndex(ctx, &pos, argv[0]))
        return JS_EXCEPTION;
    is_swap = FALSE;
    if (argc > 1)
        is_swap = JS_ToBool(ctx, argv[1]);
#ifndef WORDS_BIGENDIAN
    is_swap ^= 1;
#endif
    abuf = ta->buffer->u.array_buffer;
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    if ((pos + size) > ta->length)
        return JS_ThrowRangeError(ctx, "out of bound");
    ptr = abuf->data + ta->offset + pos;

    switch(class_id) {
        case JS_CLASS_INT8_ARRAY:
            return JS_NewInt32(ctx, *(int8_t *)ptr);
        case JS_CLASS_UINT8_ARRAY:
            return JS_NewInt32(ctx, *(uint8_t *)ptr);
        case JS_CLASS_INT16_ARRAY:
            v = get_u16(ptr);
            if (is_swap)
                v = bswap16(v);
            return JS_NewInt32(ctx, (int16_t)v);
        case JS_CLASS_UINT16_ARRAY:
            v = get_u16(ptr);
            if (is_swap)
                v = bswap16(v);
            return JS_NewInt32(ctx, v);
        case JS_CLASS_INT32_ARRAY:
            v = get_u32(ptr);
            if (is_swap)
                v = bswap32(v);
            return JS_NewInt32(ctx, v);
        case JS_CLASS_UINT32_ARRAY:
            v = get_u32(ptr);
            if (is_swap)
                v = bswap32(v);
            return JS_NewUint32(ctx, v);
#ifdef CONFIG_BIGNUM
            case JS_CLASS_BIG_INT64_ARRAY:
        {
            uint64_t v;
            v = get_u64(ptr);
            if (is_swap)
                v = bswap64(v);
            return JS_NewBigInt64(ctx, v);
        }
        break;
    case JS_CLASS_BIG_UINT64_ARRAY:
        {
            uint64_t v;
            v = get_u64(ptr);
            if (is_swap)
                v = bswap64(v);
            return JS_NewBigUint64(ctx, v);
        }
        break;
#endif
        case JS_CLASS_FLOAT32_ARRAY:
        {
            union {
                float f;
                uint32_t i;
            } u;
            v = get_u32(ptr);
            if (is_swap)
                v = bswap32(v);
            u.i = v;
            return __JS_NewFloat64(ctx, u.f);
        }
        case JS_CLASS_FLOAT64_ARRAY:
        {
            union {
                double f;
                uint64_t i;
            } u;
            u.i = get_u64(ptr);
            if (is_swap)
                u.i = bswap64(u.i);
            return __JS_NewFloat64(ctx, u.f);
        }
        default:
            abort();
    }
}

 JSValue js_dataview_setValue(JSContext *ctx,
                                    JSValueConst this_obj,
                                    int argc, JSValueConst *argv, int class_id)
{
    JSTypedArray *ta;
    JSArrayBuffer *abuf;
    int is_swap, size;
    uint8_t *ptr;
    uint64_t v64;
    uint32_t v;
    uint64_t pos;
    JSValueConst val;

    ta = JS_GetOpaque2(ctx, this_obj, JS_CLASS_DATAVIEW);
    if (!ta)
        return JS_EXCEPTION;
    size = 1 << typed_array_size_log2(class_id);
    if (JS_ToIndex(ctx, &pos, argv[0]))
        return JS_EXCEPTION;
    val = argv[1];
    v = 0; /* avoid warning */
    v64 = 0; /* avoid warning */
    if (class_id <= JS_CLASS_UINT32_ARRAY) {
        if (JS_ToUint32(ctx, &v, val))
            return JS_EXCEPTION;
    } else
#ifdef CONFIG_BIGNUM
        if (class_id <= JS_CLASS_BIG_UINT64_ARRAY) {
        if (JS_ToBigInt64(ctx, (int64_t *)&v64, val))
            return JS_EXCEPTION;
    } else
#endif
    {
        double d;
        if (JS_ToFloat64(ctx, &d, val))
            return JS_EXCEPTION;
        if (class_id == JS_CLASS_FLOAT32_ARRAY) {
            union {
                float f;
                uint32_t i;
            } u;
            u.f = d;
            v = u.i;
        } else {
            JSFloat64Union u;
            u.d = d;
            v64 = u.u64;
        }
    }
    is_swap = FALSE;
    if (argc > 2)
        is_swap = JS_ToBool(ctx, argv[2]);
#ifndef WORDS_BIGENDIAN
    is_swap ^= 1;
#endif
    abuf = ta->buffer->u.array_buffer;
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    if ((pos + size) > ta->length)
        return JS_ThrowRangeError(ctx, "out of bound");
    ptr = abuf->data + ta->offset + pos;

    switch(class_id) {
        case JS_CLASS_INT8_ARRAY:
        case JS_CLASS_UINT8_ARRAY:
            *ptr = v;
            break;
        case JS_CLASS_INT16_ARRAY:
        case JS_CLASS_UINT16_ARRAY:
            if (is_swap)
                v = bswap16(v);
            put_u16(ptr, v);
            break;
        case JS_CLASS_INT32_ARRAY:
        case JS_CLASS_UINT32_ARRAY:
        case JS_CLASS_FLOAT32_ARRAY:
            if (is_swap)
                v = bswap32(v);
            put_u32(ptr, v);
            break;
#ifdef CONFIG_BIGNUM
            case JS_CLASS_BIG_INT64_ARRAY:
    case JS_CLASS_BIG_UINT64_ARRAY:
#endif
        case JS_CLASS_FLOAT64_ARRAY:
            if (is_swap)
                v64 = bswap64(v64);
            put_u64(ptr, v64);
            break;
        default:
            abort();
    }
    return JS_UNDEFINED;
}


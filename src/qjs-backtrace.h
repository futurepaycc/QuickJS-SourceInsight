//
// Created by benpeng.jiang on 2021/6/18.
//

#ifndef LOX_JS_BACKTRACE_H
#define LOX_JS_BACKTRACE_H
#include "qjs-api.h"
#include "ecma-object.h"

 int find_line_num(JSContext *ctx, JSFunctionBytecode *b,
                         uint32_t pc_value);
void build_backtrace(JSContext *ctx, JSValueConst error_obj,
                            const char *filename, int line_num,
                            int backtrace_flags);

 void dbuf_put_leb128(DynBuf *s, uint32_t v);
void dbuf_put_sleb128(DynBuf *s, int32_t v1);
int get_leb128(uint32_t *pval, const uint8_t *buf,
               const uint8_t *buf_end);
 BOOL is_backtrace_needed(JSContext *ctx, JSValueConst obj);


typedef struct BCReaderState {
    JSContext *ctx;
    const uint8_t *buf_start, *ptr, *buf_end;
    uint32_t first_atom;
    uint32_t idx_to_atom_count;
    JSAtom *idx_to_atom;
    int error_state;
    BOOL allow_sab : 8;
    BOOL allow_bytecode : 8;
    BOOL is_rom_data : 8;
    BOOL allow_reference : 8;
    /* object references */
    JSObject **objects;
    int objects_count;
    int objects_size;

#ifdef DUMP_READ_OBJECT
    const uint8_t *ptr_last;
    int level;
#endif
} BCReaderState;

#ifdef DUMP_READ_OBJECT
static void __attribute__((format(printf, 2, 3))) bc_read_trace(BCReaderState *s, const char *fmt, ...) {
    va_list ap;
    int i, n, n0;

    if (!s->ptr_last)
        s->ptr_last = s->buf_start;

    n = n0 = 0;
    if (s->ptr > s->ptr_last || s->ptr == s->buf_start) {
        n0 = printf("%04x: ", (int)(s->ptr_last - s->buf_start));
        n += n0;
    }
    for (i = 0; s->ptr_last < s->ptr; i++) {
        if ((i & 7) == 0 && i > 0) {
            printf("\n%*s", n0, "");
            n = n0;
        }
        n += printf(" %02x", *s->ptr_last++);
    }
    if (*fmt == '}')
        s->level--;
    if (n < 32 + s->level * 2) {
        printf("%*s", 32 + s->level * 2 - n, "");
    }
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    if (strchr(fmt, '{'))
        s->level++;
}
#else
#define bc_read_trace(...)
#endif

static int bc_read_error_end(BCReaderState *s)
{
    if (!s->error_state) {
        JS_ThrowSyntaxError(s->ctx, "read after the end of the buffer");
    }
    return s->error_state = -1;
}

static int bc_get_u8(BCReaderState *s, uint8_t *pval)
{
    if (unlikely(s->buf_end - s->ptr < 1)) {
        *pval = 0; /* avoid warning */
        return bc_read_error_end(s);
    }
    *pval = *s->ptr++;
    return 0;
}

static int bc_get_u16(BCReaderState *s, uint16_t *pval)
{
    if (unlikely(s->buf_end - s->ptr < 2)) {
        *pval = 0; /* avoid warning */
        return bc_read_error_end(s);
    }
    *pval = get_u16(s->ptr);
    s->ptr += 2;
    return 0;
}

static __maybe_unused int bc_get_u32(BCReaderState *s, uint32_t *pval)
{
    if (unlikely(s->buf_end - s->ptr < 4)) {
        *pval = 0; /* avoid warning */
        return bc_read_error_end(s);
    }
    *pval = get_u32(s->ptr);
    s->ptr += 4;
    return 0;
}

static int bc_get_u64(BCReaderState *s, uint64_t *pval)
{
    if (unlikely(s->buf_end - s->ptr < 8)) {
        *pval = 0; /* avoid warning */
        return bc_read_error_end(s);
    }
    *pval = get_u64(s->ptr);
    s->ptr += 8;
    return 0;
}

static int bc_get_leb128(BCReaderState *s, uint32_t *pval)
{
    int ret;
    ret = get_leb128(pval, s->ptr, s->buf_end);
    if (unlikely(ret < 0))
        return bc_read_error_end(s);
    s->ptr += ret;
    return 0;
}



/* XXX: used to read an `int` with a positive value */
static int bc_get_leb128_int(BCReaderState *s, int *pval)
{
    return bc_get_leb128(s, (uint32_t *)pval);
}

static int bc_get_leb128_u16(BCReaderState *s, uint16_t *pval)
{
    uint32_t val;
    if (bc_get_leb128(s, &val)) {
        *pval = 0;
        return -1;
    }
    *pval = val;
    return 0;
}

static int bc_get_buf(BCReaderState *s, uint8_t *buf, uint32_t buf_len)
{
    if (buf_len != 0) {
        if (unlikely(!buf || s->buf_end - s->ptr < buf_len))
            return bc_read_error_end(s);
        memcpy(buf, s->ptr, buf_len);
        s->ptr += buf_len;
    }
    return 0;
}
int bc_idx_to_atom(BCReaderState *s, JSAtom *patom, uint32_t idx);
int bc_get_atom(BCReaderState *s, JSAtom *patom);
int get_sleb128(int32_t *pval, const uint8_t *buf,
                const uint8_t *buf_end);
 int bc_get_sleb128(BCReaderState *s, int32_t *pval);

#endif //LOX_JS_BACKTRACE_H

//
// Created by benpeng.jiang on 2021/6/22.
//

#ifndef LOX_JS_QJS_STRINGBUFFER_H
#define LOX_JS_QJS_STRINGBUFFER_H

#include "qjs-api.h"

typedef struct StringBuffer {
    JSContext *ctx;
    JSString *str;
    int len;
    int size;
    int is_wide_char;
    int error_status;
} StringBuffer;

int string_buffer_init(JSContext *ctx, StringBuffer *s, int size);

int string_buffer_putc(StringBuffer *s, uint32_t c);

JSValue string_buffer_end(StringBuffer *s);

int string_buffer_concat(StringBuffer *s, const JSString *p,
                         uint32_t from, uint32_t to);

void string_buffer_free(StringBuffer *s);


int string_buffer_puts8(StringBuffer *s, const char *str);

int string_buffer_concat_value_free(StringBuffer *s, JSValue v);

int string_buffer_concat_value(StringBuffer *s, JSValueConst v);

/* 0 <= c <= 0xffff */
int string_buffer_putc16(StringBuffer *s, uint32_t c);

int string_buffer_putc8(StringBuffer *s, uint32_t c);

int string_buffer_init2(JSContext *ctx, StringBuffer *s, int size,
                        int is_wide);

int string_buffer_fill(StringBuffer *s, int c, int count);

int string_buffer_write8(StringBuffer *s, const uint8_t *p, int len);

#endif //LOX_JS_QJS_STRINGBUFFER_H

//
// Created by benpeng.jiang on 2021/6/2.
//

#ifndef TUTORIAL_QUICKJS_LIBC_H
#define TUTORIAL_QUICKJS_LIBC_H
#include "qjs-runtime.h"

uint8_t *js_load_file(JSContext *ctx, size_t *pbuf_len, const char *filename);

#endif //TUTORIAL_QUICKJS_LIBC_H

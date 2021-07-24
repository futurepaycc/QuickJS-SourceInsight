//
// Created by benpeng.jiang on 2021/6/20.
//

#ifndef LOX_JS_ECMA_ARRAY_H
#define LOX_JS_ECMA_ARRAY_H
#include "qjs-api.h"
#include "ecma-iterator.h"


typedef struct JSArrayIteratorData {
    JSValue obj;
    JSIteratorKindEnum kind;
    uint32_t idx;
} JSArrayIteratorData;

 int js_typed_array_get_length_internal(JSContext *ctx, JSValueConst obj);

 JSValue js_typed_array___speciesCreate(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv);
 JSValue js_array_buffer_constructor3(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, JSClassID class_id,
                                            uint8_t *buf,
                                            JSFreeArrayBufferDataFunc *free_func,
                                            void *opaque, BOOL alloc_flag);
 JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);
 JSValue js_typed_array_constructor(JSContext *ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst *argv,
                                          int classid);
 BOOL typed_array_is_detached(JSContext *ctx, JSObject *p);
 uint32_t typed_array_get_length(JSContext *ctx, JSObject *p);
 BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj,
                              JSValue **arrpp, uint32_t *countp);
 no_inline int js_realloc_array(JSContext *ctx, void **parray,
                                      int elem_size, int *psize, int req_size);
/* resize the array and update its size if req_size > *psize */
  int js_resize_array(JSContext *ctx, void **parray, int elem_size,
                                  int *psize, int req_size);
 JSValue js_create_array(JSContext *ctx, int len, JSValueConst *tab);


 void js_array_iterator_finalizer(JSRuntime *rt, JSValue val);
 void js_array_iterator_mark(JSRuntime *rt, JSValueConst val,
                                   JS_MarkFunc *mark_func);

JSValue js_array_includes(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv);

JSValue js_array_push(JSContext *ctx, JSValueConst this_val,
                      int argc, JSValueConst *argv, int unshift);


JSValue js_array_pop(JSContext *ctx, JSValueConst this_val,
                     int argc, JSValueConst *argv, int shift);

 int expand_fast_array(JSContext *ctx, JSObject *p, uint32_t new_len);


JSValue js_array_reduce(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv, int special);
JSValue js_array_concat(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv);
JSValue js_array_every(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv, int special);

JSValue js_array_from(JSContext *ctx, JSValueConst this_val,
                      int argc, JSValueConst *argv);
JSValue js_array_isArray(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv);
JSValue js_array_of(JSContext *ctx, JSValueConst this_val,
                    int argc, JSValueConst *argv);

JSValue js_array_fill(JSContext *ctx, JSValueConst this_val,
                      int argc, JSValueConst *argv);

JSValue js_array_find(JSContext *ctx, JSValueConst this_val,
                      int argc, JSValueConst *argv, int findIndex);
JSValue js_array_indexOf(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv);
JSValue js_array_constructor(JSContext *ctx, JSValueConst new_target,
                             int argc, JSValueConst *argv);

JSValue js_array_lastIndexOf(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv);
void JS_SetArrayPropertyFunctionList(JSContext *ctx);


JSValue js_array_buffer_slice(JSContext *ctx,
                              JSValueConst this_val,
                              int argc, JSValueConst *argv, int class_id);
#endif //LOX_JS_ECMA_ARRAY_H

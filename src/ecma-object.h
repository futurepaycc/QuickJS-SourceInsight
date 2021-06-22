//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_PROPERTY_H
#define LOX_JS_PROPERTY_H

#include "qjs-api.h"
#include "qjs-gc.h"
#include "qjs-varrefs.h"
#include "qjs-atom.h"
int init_shape_hash(JSRuntime *rt);

JSShapeProperty *get_shape_prop(JSShape *sh);

JSContext *js_autoinit_get_realm(JSProperty *pr);

JSAutoInitIDEnum js_autoinit_get_id(JSProperty *pr);

void js_autoinit_free(JSRuntime *rt, JSProperty *pr);

void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);

void js_shape_hash_unlink(JSRuntime *rt, JSShape *sh);

void *get_alloc_from_shape(JSShape *sh);

no_inline JSShape *js_new_shape2(JSContext *ctx, JSObject *proto,
                                 int hash_size, int prop_size);

JSShapeProperty *find_own_property1(JSObject *p,
                                    JSAtom atom);

JSShapeProperty *find_own_property(JSProperty **ppr,
                                   JSObject *p,
                                   JSAtom atom);

JSProperty *add_property(JSContext *ctx,
                         JSObject *p, JSAtom prop, int prop_flags);

int add_shape_property(JSContext *ctx, JSShape **psh,
                       JSObject *p, JSAtom atom, int prop_flags);

JSValue JS_GetPrototypeFree(JSContext *ctx, JSValue obj);

int __exception JS_GetOwnPropertyNamesInternal(JSContext *ctx,
                                               JSPropertyEnum **ptab,
                                               uint32_t *plen,
                                               JSObject *p, int flags);

void js_free_prop_enum(JSContext *ctx, JSPropertyEnum *tab, uint32_t len);

int JS_DefineObjectName(JSContext *ctx, JSValueConst obj,
                        JSAtom name, int flags);

int JS_DefineObjectNameComputed(JSContext *ctx, JSValueConst obj,
                                JSValueConst str, int flags);

int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj,
                            JSValueConst proto_val,
                            BOOL throw_flag);

JSValue JS_GetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                            JSValue prop);

int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                        JSValue prop, JSValue val, int flags);

int JS_SetPropertyGeneric(JSContext *ctx,
                          JSValueConst obj, JSAtom prop,
                          JSValue val, JSValueConst this_obj,
                          int flags);

int JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj,
                                JSValue prop, JSValue val, int flags);

__exception int JS_CopyDataProperties(JSContext *ctx,
                                      JSValueConst target,
                                      JSValueConst source,
                                      JSValueConst excluded,
                                      BOOL setprop);

 JSShape *find_hashed_shape_proto(JSRuntime *rt, JSObject *proto);

JSShape *js_dup_shape(JSShape *sh);

 JSShape *js_new_shape(JSContext *ctx, JSObject *proto);

 size_t get_shape_size(size_t hash_size, size_t prop_size);

JSValue JS_GetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx);
no_inline int resize_properties(JSContext *ctx, JSShape **psh,
                                JSObject *p, uint32_t count);

uint32_t *prop_hash_end(JSShape *sh);

int compact_properties(JSContext *ctx, JSObject *p);

JSShape *js_clone_shape(JSContext *ctx, JSShape *sh1);

int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj,
                              JSAtom prop, JSAutoInitIDEnum id,
                              void *opaque, int flags);
 JSValue js_object_keys(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int kind);

 int JS_TryGetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, JSValue *pval);
int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, int flags);

JSValue js_object_valueOf(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv);
 JSValue JS_GetOwnPropertyNames2(JSContext *ctx, JSValueConst obj1,
                                       int flags, int kind);
 JSValue js_object_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv);
#endif //LOX_JS_PROPERTY_H

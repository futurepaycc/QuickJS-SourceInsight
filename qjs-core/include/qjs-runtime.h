//
// Created by benpeng.jiang on 2021/5/21.
//

#ifndef RUNTIME_H
#define RUNTIME_H
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <cutils.h>
#include "list.h"
#include "quickjs.h"

typedef struct JSMallocState {
    size_t malloc_count;
    size_t malloc_size;
    size_t malloc_limit;
    void *opaque; /* user opaque */
} JSMallocState;

typedef struct JSMallocFunctions {
    void *(*js_malloc)(JSMallocState *s, size_t size);
    void (*js_free)(JSMallocState *s, void *ptr);
    void *(*js_realloc)(JSMallocState *s, void *ptr, size_t size);
    size_t (*js_malloc_usable_size)(const void *ptr);
} JSMallocFunctions;


typedef enum {
    JS_GC_OBJ_TYPE_JS_OBJECT,
    JS_GC_OBJ_TYPE_FUNCTION_BYTECODE,
    JS_GC_OBJ_TYPE_SHAPE,
    JS_GC_OBJ_TYPE_VAR_REF,
    JS_GC_OBJ_TYPE_ASYNC_FUNCTION,
    JS_GC_OBJ_TYPE_JS_CONTEXT,
} JSGCObjectTypeEnum;

/* header for GC objects. GC objects are C data structures with a
   reference count that can reference other GC objects. JS Objects are
   a particular type of GC object. */
struct JSGCObjectHeader {
    int ref_count; /* must come first, 32-bit */
    JSGCObjectTypeEnum gc_obj_type : 4;
    uint8_t mark : 4; /* used by the GC */
    uint8_t dummy1; /* not used by the GC */
    uint16_t dummy2; /* not used by the GC */
    struct list_head link;
};

typedef struct JSGCObjectHeader JSGCObjectHeader;

typedef uint32_t JSClassID;
typedef uint32_t JSAtom;


typedef struct JSString JSString;
typedef struct JSString JSAtomStruct;

struct JSRuntime {
    JSMallocFunctions mf;
    JSMallocState malloc_state;
    const char *rt_info;

    int atom_hash_size; /* power of two */
    int atom_count;
    int atom_size;
    int atom_count_resize; /* resize hash table at this count */
    uint32_t *atom_hash;
    JSAtomStruct **atom_array;
    int atom_free_index; /* 0 = none */
    struct list_head context_list; /* list of JSContext.link */

};

typedef struct JSContext {
    JSGCObjectHeader header; /* must come first */
    JSRuntime *rt;
    struct list_head link;

    JSValue *class_proto;
    JSValue function_proto;
    JSValue function_ctor;
    JSValue array_ctor;
    JSValue regexp_ctor;
    JSValue promise_ctor;
    JSValue native_error_proto[JS_NATIVE_ERROR_COUNT];
    JSValue iterator_proto;
    JSValue async_iterator_proto;
    JSValue array_proto_values;
    JSValue throw_type_error;
    JSValue eval_obj;

    JSValue global_obj; /* global object */
    JSValue global_var_obj; /* contains the global let/const definitions */

    uint64_t random_state;

    /* if NULL, eval is not supported */
    JSValue (*eval_internal)(JSContext *ctx, JSValueConst this_obj,
                             const char *input, size_t input_len,
                             const char *filename, int flags, int scope_idx);
    void *user_opaque;
} JSContext;



typedef struct JSMemoryUsage {
    int64_t malloc_size, malloc_limit, memory_used_size;
    int64_t malloc_count;
    int64_t memory_used_count;
    int64_t atom_count, atom_size;
    int64_t str_count, str_size;
    int64_t obj_count, obj_size;
    int64_t prop_count, prop_size;
    int64_t shape_count, shape_size;
    int64_t js_func_count, js_func_size, js_func_code_size;
    int64_t js_func_pc2line_count, js_func_pc2line_size;
    int64_t c_func_count, array_count;
    int64_t fast_array_count, fast_array_elements;
    int64_t binary_object_count, binary_object_size;
} JSMemoryUsage;

/* Compute memory used by various object types */
/* XXX: poor man's approach to handling multiply referenced objects */
typedef struct JSMemoryUsage_helper {
    double memory_used_count;
    double str_count;
    double str_size;
    int64_t js_func_count;
    double js_func_size;
    int64_t js_func_code_size;
    int64_t js_func_pc2line_count;
    int64_t js_func_pc2line_size;
} JSMemoryUsage_helper;

void JS_ComputeMemoryUsage(JSRuntime *rt, JSMemoryUsage *s);
void JS_DumpMemoryUsage(FILE *fp, const JSMemoryUsage *s, JSRuntime *rt);

JSRuntime *JS_NewRuntime(void);

/* info lifetime must exceed that of rt */
void JS_SetRuntimeInfo(JSRuntime *rt, const char *info);
JSRuntime *JS_NewRuntime2(const JSMallocFunctions *mf, void *opaque);
void JS_FreeRuntime(JSRuntime *rt);

void *js_malloc_rt(JSRuntime *rt, size_t size);
void js_free_rt(JSRuntime *rt, void *ptr);
void *js_realloc_rt(JSRuntime *rt, void *ptr, size_t size);
size_t js_malloc_usable_size_rt(JSRuntime *rt, const void *ptr);
void *js_mallocz_rt(JSRuntime *rt, size_t size);

JSContext *JS_NewCustomContext(JSRuntime *rt);


/* 'input' must be zero terminated i.e. input[input_len] = '\0'. */
JSValue JS_Eval(JSContext *ctx, const char *input, size_t input_len,
                const char *filename, int eval_flags);


struct JSString {
    JSRefCountHeader header; /* must come first, 32-bit */
    uint32_t len : 31;
    uint8_t is_wide_char : 1; /* 0 = 8 bits, 1 = 16 bits characters */
    /* for JS_ATOM_TYPE_SYMBOL: hash = 0, atom_type = 3,
       for JS_ATOM_TYPE_PRIVATE: hash = 1, atom_type = 3
       XXX: could change encoding to have one more bit in hash */
    uint32_t hash : 30;
    uint8_t atom_type : 2; /* != 0 if atom, JS_ATOM_TYPE_x */
    uint32_t hash_next; /* atom_index for JS_ATOM_TYPE_SYMBOL */

    union {
        uint8_t str8[0]; /* 8 bit strings will get an extra null terminator */
        uint16_t str16[0];
    } u;
};

typedef struct JSFunctionDef {
    JSContext *ctx;
    struct JSFunctionDef *parent;
    int parent_cpool_idx; /* index in the constant pool of the parent
                             or -1 if none */
    int parent_scope_level; /* scope level in parent at point of definition */
    struct list_head child_list; /* list of JSFunctionDef.link */
    struct list_head link;

    BOOL is_eval; /* TRUE if eval code */
    int eval_type; /* only valid if is_eval = TRUE */
    BOOL is_global_var; /* TRUE if variables are not defined locally:
                           eval global, eval module or non strict eval */
    BOOL is_func_expr; /* TRUE if function expression */
    BOOL has_home_object; /* TRUE if the home object is available */
    BOOL has_prototype; /* true if a prototype field is necessary */
    BOOL has_simple_parameter_list;
    BOOL has_use_strict; /* to reject directive in special cases */
    BOOL has_eval_call; /* true if the function contains a call to eval() */
    BOOL has_arguments_binding; /* true if the 'arguments' binding is
                                   available in the function */
    BOOL has_this_binding; /* true if the 'this' and new.target binding are
                              available in the function */
    BOOL new_target_allowed; /* true if the 'new.target' does not
                                throw a syntax error */
    BOOL super_call_allowed; /* true if super() is allowed */
    BOOL super_allowed; /* true if super. or super[] is allowed */
    BOOL arguments_allowed; /* true if the 'arguments' identifier is allowed */
    BOOL is_derived_class_constructor;
    BOOL in_function_body;
    BOOL backtrace_barrier;
} JSFunctionDef;

#if !defined(CONFIG_STACK_CHECK)
/* no stack limitation */
static inline uint8_t *js_get_stack_pointer(void)
{
    return NULL;
}

static inline BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size)
{
    return FALSE;
}
#else
/* Note: OS and CPU dependent */
inline uint8_t *js_get_stack_pointer(void)
{
    return __builtin_frame_address(0);
}

inline BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size)
{
    size_t size;
    size = rt->stack_top - js_get_stack_pointer();
    return unlikely((size + alloca_size) > rt->stack_size);
}
#endif
#endif //QJS_JRUNTEIM_H

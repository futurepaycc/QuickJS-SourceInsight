//
// Created by benpeng.jiang on 2021/6/19.
//

#include "qjs-varrefs.h"
 void close_var_refs(JSRuntime *rt, JSStackFrame *sf)
{
    struct list_head *el, *el1;
    JSVarRef *var_ref;
    int var_idx;

    list_for_each_safe(el, el1, &sf->var_ref_list) {
        var_ref = list_entry(el, JSVarRef, header.link);
        var_idx = var_ref->var_idx;
        if (var_ref->is_arg)
            var_ref->value = JS_DupValueRT(rt, sf->arg_buf[var_idx]);
        else
            var_ref->value = JS_DupValueRT(rt, sf->var_buf[var_idx]);
        var_ref->pvalue = &var_ref->value;
        /* the reference is no longer to a local variable */
        var_ref->is_detached = TRUE;
        add_gc_object(rt, &var_ref->header, JS_GC_OBJ_TYPE_VAR_REF);
    }
}

JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf,
                      int var_idx, BOOL is_arg)
{
    JSVarRef *var_ref;
    struct list_head *el;

    list_for_each(el, &sf->var_ref_list) {
        var_ref = list_entry(el, JSVarRef, header.link);
        if (var_ref->var_idx == var_idx && var_ref->is_arg == is_arg) {
            var_ref->header.ref_count++;
            return var_ref;
        }
    }
    /* create a new one */
    var_ref = js_malloc(ctx, sizeof(JSVarRef));
    if (!var_ref)
        return NULL;
    var_ref->header.ref_count = 1;
    var_ref->is_detached = FALSE;
    var_ref->is_arg = is_arg;
    var_ref->var_idx = var_idx;
    list_add_tail(&var_ref->header.link, &sf->var_ref_list);
    if (is_arg)
        var_ref->pvalue = &sf->arg_buf[var_idx];
    else
        var_ref->pvalue = &sf->var_buf[var_idx];
    var_ref->value = JS_UNDEFINED;
    return var_ref;
}

//
// Created by benpeng.jiang on 2021/6/19.
//

#include "qjs-gc.h"

static void js_free_shape0(JSRuntime *rt, JSShape *sh)
{
    uint32_t i;
    JSShapeProperty *pr;

    assert(sh->header.ref_count == 0);
    if (sh->is_hashed)
        js_shape_hash_unlink(rt, sh);
    if (sh->proto != NULL) {
        JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_OBJECT, sh->proto));
    }
    pr = get_shape_prop(sh);
    for(i = 0; i < sh->prop_count; i++) {
        JS_FreeAtomRT(rt, pr->atom);
        pr++;
    }
    remove_gc_object(&sh->header);
    js_free_rt(rt, get_alloc_from_shape(sh));
}

void js_free_shape(JSRuntime *rt, JSShape *sh)
{
    if (unlikely(--sh->header.ref_count <= 0)) {
        js_free_shape0(rt, sh);
    }
}


static void js_free_shape_null(JSRuntime *rt, JSShape *sh)
{
    if (sh)
        js_free_shape(rt, sh);
}

static void free_bytecode_atoms(JSRuntime *rt,
                                const uint8_t *bc_buf, int bc_len,
                                BOOL use_short_opcodes)
{
    int pos, len, op;
    JSAtom atom;
    const JSOpCode *oi;

    pos = 0;
    while (pos < bc_len) {
        op = bc_buf[pos];
        if (use_short_opcodes)
            oi = &short_opcode_info(op);
        else
            oi = &opcode_info[op];

        len = oi->size;
        switch(oi->fmt) {
            case OP_FMT_atom:
            case OP_FMT_atom_u8:
            case OP_FMT_atom_u16:
            case OP_FMT_atom_label_u8:
            case OP_FMT_atom_label_u16:
                atom = get_u32(bc_buf + pos + 1);
                JS_FreeAtomRT(rt, atom);
                break;
            default:
                break;
        }
        pos += len;
    }
}

void js_free_function_def(JSContext *ctx, JSFunctionDef *fd)
{
    int i;
    struct list_head *el, *el1;

    /* free the child functions */
    list_for_each_safe(el, el1, &fd->child_list) {
        JSFunctionDef *fd1;
        fd1 = list_entry(el, JSFunctionDef, link);
        js_free_function_def(ctx, fd1);
    }

    free_bytecode_atoms(ctx->rt, fd->byte_code.buf, fd->byte_code.size,
                        fd->use_short_opcodes);
    dbuf_free(&fd->byte_code);
    js_free(ctx, fd->jump_slots);
    js_free(ctx, fd->label_slots);
    js_free(ctx, fd->line_number_slots);

    for(i = 0; i < fd->cpool_count; i++) {
        JS_FreeValue(ctx, fd->cpool[i]);
    }
    js_free(ctx, fd->cpool);

    JS_FreeAtom(ctx, fd->func_name);

    for(i = 0; i < fd->var_count; i++) {
        JS_FreeAtom(ctx, fd->vars[i].var_name);
    }
    js_free(ctx, fd->vars);
    for(i = 0; i < fd->arg_count; i++) {
        JS_FreeAtom(ctx, fd->args[i].var_name);
    }
    js_free(ctx, fd->args);

    for(i = 0; i < fd->global_var_count; i++) {
        JS_FreeAtom(ctx, fd->global_vars[i].var_name);
    }
    js_free(ctx, fd->global_vars);

    for(i = 0; i < fd->closure_var_count; i++) {
        JSClosureVar *cv = &fd->closure_var[i];
        JS_FreeAtom(ctx, cv->var_name);
    }
    js_free(ctx, fd->closure_var);

    if (fd->scopes != fd->def_scope_array)
        js_free(ctx, fd->scopes);

    JS_FreeAtom(ctx, fd->filename);
    dbuf_free(&fd->pc2line);

    js_free(ctx, fd->source);

    if (fd->parent) {
        /* remove in parent list */
        list_del(&fd->link);
    }
    js_free(ctx, fd);
}

void free_function_bytecode(JSRuntime *rt, JSFunctionBytecode *b)
{
    int i;

#if 0
    {
        char buf[ATOM_GET_STR_BUF_SIZE];
        printf("freeing %s\n",
               JS_AtomGetStrRT(rt, buf, sizeof(buf), b->func_name));
    }
#endif
    free_bytecode_atoms(rt, b->byte_code_buf, b->byte_code_len, TRUE);

    if (b->vardefs) {
        for(i = 0; i < b->arg_count + b->var_count; i++) {
            JS_FreeAtomRT(rt, b->vardefs[i].var_name);
        }
    }
    for(i = 0; i < b->cpool_count; i++)
        JS_FreeValueRT(rt, b->cpool[i]);

    for(i = 0; i < b->closure_var_count; i++) {
        JSClosureVar *cv = &b->closure_var[i];
        JS_FreeAtomRT(rt, cv->var_name);
    }
    if (b->realm)
        JS_FreeContext(b->realm);

    JS_FreeAtomRT(rt, b->func_name);
    if (b->has_debug) {
        JS_FreeAtomRT(rt, b->debug.filename);
        js_free_rt(rt, b->debug.pc2line_buf);
        js_free_rt(rt, b->debug.source);
    }

    remove_gc_object(&b->header);
    if (rt->gc_phase == JS_GC_PHASE_REMOVE_CYCLES && b->header.ref_count != 0) {
        list_add_tail(&b->header.link, &rt->gc_zero_ref_count_list);
    } else {
        js_free_rt(rt, b);
    }
}

void js_mark_module_def(JSRuntime *rt, JSModuleDef *m,
                               JS_MarkFunc *mark_func)
{
    int i;

    for(i = 0; i < m->export_entries_count; i++) {
        JSExportEntry *me = &m->export_entries[i];
        if (me->export_type == JS_EXPORT_TYPE_LOCAL &&
            me->u.local.var_ref) {
            mark_func(rt, &me->u.local.var_ref->header);
        }
    }

    JS_MarkValue(rt, m->module_ns, mark_func);
    JS_MarkValue(rt, m->func_obj, mark_func);
    JS_MarkValue(rt, m->eval_exception, mark_func);
    JS_MarkValue(rt, m->meta_obj, mark_func);
}

/* used by the GC */
static void JS_MarkContext(JSRuntime *rt, JSContext *ctx,
                           JS_MarkFunc *mark_func)
{
    int i;
    struct list_head *el;

    /* modules are not seen by the GC, so we directly mark the objects
       referenced by each module */
    list_for_each(el, &ctx->loaded_modules) {
        JSModuleDef *m = list_entry(el, JSModuleDef, link);
        js_mark_module_def(rt, m, mark_func);
    }

    JS_MarkValue(rt, ctx->global_obj, mark_func);
    JS_MarkValue(rt, ctx->global_var_obj, mark_func);

    JS_MarkValue(rt, ctx->throw_type_error, mark_func);
    JS_MarkValue(rt, ctx->eval_obj, mark_func);

    JS_MarkValue(rt, ctx->array_proto_values, mark_func);
    for(i = 0; i < JS_NATIVE_ERROR_COUNT; i++) {
        JS_MarkValue(rt, ctx->native_error_proto[i], mark_func);
    }
    for(i = 0; i < rt->class_count; i++) {
        JS_MarkValue(rt, ctx->class_proto[i], mark_func);
    }
    JS_MarkValue(rt, ctx->iterator_proto, mark_func);
    JS_MarkValue(rt, ctx->async_iterator_proto, mark_func);
    JS_MarkValue(rt, ctx->promise_ctor, mark_func);
    JS_MarkValue(rt, ctx->array_ctor, mark_func);
    JS_MarkValue(rt, ctx->regexp_ctor, mark_func);
    JS_MarkValue(rt, ctx->function_ctor, mark_func);
    JS_MarkValue(rt, ctx->function_proto, mark_func);

    if (ctx->array_shape)
        mark_func(rt, &ctx->array_shape->header);
}

 void free_var_ref(JSRuntime *rt, JSVarRef *var_ref)
{
    if (var_ref) {
        assert(var_ref->header.ref_count > 0);
        if (--var_ref->header.ref_count == 0) {
            if (var_ref->is_detached) {
                JS_FreeValueRT(rt, var_ref->value);
                remove_gc_object(&var_ref->header);
            } else {
                list_del(&var_ref->header.link); /* still on the stack */
            }
            js_free_rt(rt, var_ref);
        }
    }
}

void js_free_module_def(JSContext *ctx, JSModuleDef *m)
{
    int i;

    JS_FreeAtom(ctx, m->module_name);

    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        JS_FreeAtom(ctx, rme->module_name);
    }
    js_free(ctx, m->req_module_entries);

    for(i = 0; i < m->export_entries_count; i++) {
        JSExportEntry *me = &m->export_entries[i];
        if (me->export_type == JS_EXPORT_TYPE_LOCAL)
            free_var_ref(ctx->rt, me->u.local.var_ref);
        JS_FreeAtom(ctx, me->export_name);
        JS_FreeAtom(ctx, me->local_name);
    }
    js_free(ctx, m->export_entries);

    js_free(ctx, m->star_export_entries);

    for(i = 0; i < m->import_entries_count; i++) {
        JSImportEntry *mi = &m->import_entries[i];
        JS_FreeAtom(ctx, mi->import_name);
    }
    js_free(ctx, m->import_entries);

    JS_FreeValue(ctx, m->module_ns);
    JS_FreeValue(ctx, m->func_obj);
    JS_FreeValue(ctx, m->eval_exception);
    JS_FreeValue(ctx, m->meta_obj);
    list_del(&m->link);
    js_free(ctx, m);
}

void JS_FreeContext(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    int i;

    if (--ctx->header.ref_count > 0)
        return;
    assert(ctx->header.ref_count == 0);

#ifdef DUMP_ATOMS
    JS_DumpAtoms(ctx->rt);
#endif
#ifdef DUMP_SHAPES
    JS_DumpShapes(ctx->rt);
#endif
#ifdef DUMP_OBJECTS
    {
        struct list_head *el;
        JSGCObjectHeader *p;
        printf("JSObjects: {\n");
        JS_DumpObjectHeader(ctx->rt);
        list_for_each(el, &rt->gc_obj_list) {
            p = list_entry(el, JSGCObjectHeader, link);
            JS_DumpGCObject(rt, p);
        }
        printf("}\n");
    }
#endif
#ifdef DUMP_MEM
    {
        JSMemoryUsage stats;
        JS_ComputeMemoryUsage(rt, &stats);
        JS_DumpMemoryUsage(stdout, &stats, rt);
    }
#endif

    js_free_modules(ctx, JS_FREE_MODULE_ALL);

    JS_FreeValue(ctx, ctx->global_obj);
    JS_FreeValue(ctx, ctx->global_var_obj);

    JS_FreeValue(ctx, ctx->throw_type_error);
    JS_FreeValue(ctx, ctx->eval_obj);

    JS_FreeValue(ctx, ctx->array_proto_values);
    for(i = 0; i < JS_NATIVE_ERROR_COUNT; i++) {
        JS_FreeValue(ctx, ctx->native_error_proto[i]);
    }
    for(i = 0; i < rt->class_count; i++) {
        JS_FreeValue(ctx, ctx->class_proto[i]);
    }
    js_free_rt(rt, ctx->class_proto);
    JS_FreeValue(ctx, ctx->iterator_proto);
    JS_FreeValue(ctx, ctx->async_iterator_proto);
    JS_FreeValue(ctx, ctx->promise_ctor);
    JS_FreeValue(ctx, ctx->array_ctor);
    JS_FreeValue(ctx, ctx->regexp_ctor);
    JS_FreeValue(ctx, ctx->function_ctor);
    JS_FreeValue(ctx, ctx->function_proto);

    js_free_shape_null(ctx->rt, ctx->array_shape);

    list_del(&ctx->link);
    remove_gc_object(&ctx->header);
    js_free_rt(ctx->rt, ctx);
}

 JSContext *js_autoinit_get_realm(JSProperty *pr)
{
    return (JSContext *)(pr->u.init.realm_and_id & ~3);
}

 JSAutoInitIDEnum js_autoinit_get_id(JSProperty *pr)
{
    return pr->u.init.realm_and_id & 3;
}

 void js_autoinit_free(JSRuntime *rt, JSProperty *pr)
{
    JS_FreeContext(js_autoinit_get_realm(pr));
}

static void js_autoinit_mark(JSRuntime *rt, JSProperty *pr,
                             JS_MarkFunc *mark_func)
{
    mark_func(rt, &js_autoinit_get_realm(pr)->header);
}

 void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSForInIterator *it = p->u.for_in_iterator;
    JS_MarkValue(rt, it->obj, mark_func);
}

static void reset_weak_ref(JSRuntime *rt, JSObject *p)
{
    JSMapRecord *mr, *mr_next;
    JSMapState *s;

    /* first pass to remove the records from the WeakMap/WeakSet
       lists */
    for(mr = p->first_weak_ref; mr != NULL; mr = mr->next_weak_ref) {
        s = mr->map;
        assert(s->is_weak);
        assert(!mr->empty); /* no iterator on WeakMap/WeakSet */
        list_del(&mr->hash_link);
        list_del(&mr->link);
    }

    /* second pass to free the values to avoid modifying the weak
       reference list while traversing it. */
    for(mr = p->first_weak_ref; mr != NULL; mr = mr_next) {
        mr_next = mr->next_weak_ref;
        JS_FreeValueRT(rt, mr->value);
        js_free_rt(rt, mr);
    }

    p->first_weak_ref = NULL; /* fail safe */
}

static void free_object(JSRuntime *rt, JSObject *p)
{
    int i;
    JSClassFinalizer *finalizer;
    JSShape *sh;
    JSShapeProperty *pr;

    p->free_mark = 1; /* used to tell the object is invalid when
                         freeing cycles */
    /* free all the fields */
    sh = p->shape;
    pr = get_shape_prop(sh);
    for(i = 0; i < sh->prop_count; i++) {
        free_property(rt, &p->prop[i], pr->flags);
        pr++;
    }
    js_free_rt(rt, p->prop);
    /* as an optimization we destroy the shape immediately without
       putting it in gc_zero_ref_count_list */
    js_free_shape(rt, sh);

    /* fail safe */
    p->shape = NULL;
    p->prop = NULL;

    if (unlikely(p->first_weak_ref)) {
        reset_weak_ref(rt, p);
    }

    finalizer = rt->class_array[p->class_id].finalizer;
    if (finalizer)
        (*finalizer)(rt, JS_MKPTR(JS_TAG_OBJECT, p));

    /* fail safe */
    p->class_id = 0;
    p->u.opaque = NULL;
    p->u.func.var_refs = NULL;
    p->u.func.home_object = NULL;

    remove_gc_object(&p->header);
    if (rt->gc_phase == JS_GC_PHASE_REMOVE_CYCLES && p->header.ref_count != 0) {
        list_add_tail(&p->header.link, &rt->gc_zero_ref_count_list);
    } else {
        js_free_rt(rt, p);
    }
}

static void free_gc_object(JSRuntime *rt, JSGCObjectHeader *gp)
{
    switch(gp->gc_obj_type) {
        case JS_GC_OBJ_TYPE_JS_OBJECT:
            free_object(rt, (JSObject *)gp);
            break;
        case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
            free_function_bytecode(rt, (JSFunctionBytecode *)gp);
            break;
        default:
            abort();
    }
}

 void free_zero_refcount(JSRuntime *rt)
{
    struct list_head *el;
    JSGCObjectHeader *p;

    rt->gc_phase = JS_GC_PHASE_DECREF;
    for(;;) {
        el = rt->gc_zero_ref_count_list.next;
        if (el == &rt->gc_zero_ref_count_list)
            break;
        p = list_entry(el, JSGCObjectHeader, link);
        assert(p->ref_count == 0);
        free_gc_object(rt, p);
    }
    rt->gc_phase = JS_GC_PHASE_NONE;
}


/* garbage collection */

void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type)
{
    h->mark = 0;
    h->gc_obj_type = type;
    list_add_tail(&h->link, &rt->gc_obj_list);
}

void remove_gc_object(JSGCObjectHeader *h)
{
    list_del(&h->link);
}

void JS_MarkValue(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    if (JS_VALUE_HAS_REF_COUNT(val)) {
        switch(JS_VALUE_GET_TAG(val)) {
            case JS_TAG_OBJECT:
            case JS_TAG_FUNCTION_BYTECODE:
                mark_func(rt, JS_VALUE_GET_PTR(val));
                break;
            default:
                break;
        }
    }
}

static void mark_children(JSRuntime *rt, JSGCObjectHeader *gp,
                          JS_MarkFunc *mark_func)
{
    switch(gp->gc_obj_type) {
        case JS_GC_OBJ_TYPE_JS_OBJECT:
        {
            JSObject *p = (JSObject *)gp;
            JSShapeProperty *prs;
            JSShape *sh;
            int i;
            sh = p->shape;
            mark_func(rt, &sh->header);
            /* mark all the fields */
            prs = get_shape_prop(sh);
            for(i = 0; i < sh->prop_count; i++) {
                JSProperty *pr = &p->prop[i];
                if (prs->atom != JS_ATOM_NULL) {
                    if (prs->flags & JS_PROP_TMASK) {
                        if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                            if (pr->u.getset.getter)
                                mark_func(rt, &pr->u.getset.getter->header);
                            if (pr->u.getset.setter)
                                mark_func(rt, &pr->u.getset.setter->header);
                        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                            if (pr->u.var_ref->is_detached) {
                                /* Note: the tag does not matter
                                   provided it is a GC object */
                                mark_func(rt, &pr->u.var_ref->header);
                            }
                        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                            js_autoinit_mark(rt, pr, mark_func);
                        }
                    } else {
                        JS_MarkValue(rt, pr->u.value, mark_func);
                    }
                }
                prs++;
            }

            if (p->class_id != JS_CLASS_OBJECT) {
                JSClassGCMark *gc_mark;
                gc_mark = rt->class_array[p->class_id].gc_mark;
                if (gc_mark)
                    gc_mark(rt, JS_MKPTR(JS_TAG_OBJECT, p), mark_func);
            }
        }
            break;
        case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
            /* the template objects can be part of a cycle */
        {
            JSFunctionBytecode *b = (JSFunctionBytecode *)gp;
            int i;
            for(i = 0; i < b->cpool_count; i++) {
                JS_MarkValue(rt, b->cpool[i], mark_func);
            }
            if (b->realm)
                mark_func(rt, &b->realm->header);
        }
            break;
        case JS_GC_OBJ_TYPE_VAR_REF:
        {
            JSVarRef *var_ref = (JSVarRef *)gp;
            /* only detached variable referenced are taken into account */
            assert(var_ref->is_detached);
            JS_MarkValue(rt, *var_ref->pvalue, mark_func);
        }
            break;
        case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
        {
            JSAsyncFunctionData *s = (JSAsyncFunctionData *)gp;
            if (s->is_active)
                async_func_mark(rt, &s->func_state, mark_func);
            JS_MarkValue(rt, s->resolving_funcs[0], mark_func);
            JS_MarkValue(rt, s->resolving_funcs[1], mark_func);
        }
            break;
        case JS_GC_OBJ_TYPE_SHAPE:
        {
            JSShape *sh = (JSShape *)gp;
            if (sh->proto != NULL) {
                mark_func(rt, &sh->proto->header);
            }
        }
            break;
        case JS_GC_OBJ_TYPE_JS_CONTEXT:
        {
            JSContext *ctx = (JSContext *)gp;
            JS_MarkContext(rt, ctx, mark_func);
        }
            break;
        default:
            abort();
    }
}

static void gc_decref_child(JSRuntime *rt, JSGCObjectHeader *p)
{
    assert(p->ref_count > 0);
    p->ref_count--;
    if (p->ref_count == 0 && p->mark == 1) {
        list_del(&p->link);
        list_add_tail(&p->link, &rt->tmp_obj_list);
    }
}

static void gc_decref(JSRuntime *rt)
{
    struct list_head *el, *el1;
    JSGCObjectHeader *p;

    init_list_head(&rt->tmp_obj_list);

    /* decrement the refcount of all the children of all the GC
       objects and move the GC objects with zero refcount to
       tmp_obj_list */
    list_for_each_safe(el, el1, &rt->gc_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(p->mark == 0);
        mark_children(rt, p, gc_decref_child);
        p->mark = 1;
        if (p->ref_count == 0) {
            list_del(&p->link);
            list_add_tail(&p->link, &rt->tmp_obj_list);
        }
    }
}

static void gc_scan_incref_child(JSRuntime *rt, JSGCObjectHeader *p)
{
    p->ref_count++;
    if (p->ref_count == 1) {
        /* ref_count was 0: remove from tmp_obj_list and add at the
           end of gc_obj_list */
        list_del(&p->link);
        list_add_tail(&p->link, &rt->gc_obj_list);
        p->mark = 0; /* reset the mark for the next GC call */
    }
}

static void gc_scan_incref_child2(JSRuntime *rt, JSGCObjectHeader *p)
{
    p->ref_count++;
}

static void gc_scan(JSRuntime *rt)
{
    struct list_head *el;
    JSGCObjectHeader *p;

    /* keep the objects with a refcount > 0 and their children. */
    list_for_each(el, &rt->gc_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(p->ref_count > 0);
        p->mark = 0; /* reset the mark for the next GC call */
        mark_children(rt, p, gc_scan_incref_child);
    }

    /* restore the refcount of the objects to be deleted. */
    list_for_each(el, &rt->tmp_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        mark_children(rt, p, gc_scan_incref_child2);
    }
}

static void gc_free_cycles(JSRuntime *rt)
{
    struct list_head *el, *el1;
    JSGCObjectHeader *p;
#ifdef DUMP_GC_FREE
    BOOL header_done = FALSE;
#endif

    rt->gc_phase = JS_GC_PHASE_REMOVE_CYCLES;

    for(;;) {
        el = rt->tmp_obj_list.next;
        if (el == &rt->tmp_obj_list)
            break;
        p = list_entry(el, JSGCObjectHeader, link);
        /* Only need to free the GC object associated with JS
           values. The rest will be automatically removed because they
           must be referenced by them. */
        switch(p->gc_obj_type) {
            case JS_GC_OBJ_TYPE_JS_OBJECT:
            case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
#ifdef DUMP_GC_FREE
                if (!header_done) {
                printf("Freeing cycles:\n");
                JS_DumpObjectHeader(rt);
                header_done = TRUE;
            }
            JS_DumpGCObject(rt, p);
#endif
                free_gc_object(rt, p);
                break;
            default:
                list_del(&p->link);
                list_add_tail(&p->link, &rt->gc_zero_ref_count_list);
                break;
        }
    }
    rt->gc_phase = JS_GC_PHASE_NONE;

    list_for_each_safe(el, el1, &rt->gc_zero_ref_count_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(p->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT ||
               p->gc_obj_type == JS_GC_OBJ_TYPE_FUNCTION_BYTECODE);
        js_free_rt(rt, p);
    }

    init_list_head(&rt->gc_zero_ref_count_list);
}

void JS_RunGC(JSRuntime *rt)
{
    /* decrement the reference of the children of each object. mark =
       1 after this pass. */
    gc_decref(rt);

    /* keep the GC objects with a non zero refcount and their childs */
    gc_scan(rt);

    /* free the GC objects in a cycle */
    gc_free_cycles(rt);
}

/* Return false if not an object or if the object has already been
   freed (zombie objects are visible in finalizers when freeing
   cycles). */
BOOL JS_IsLiveObject(JSRuntime *rt, JSValueConst obj)
{
    JSObject *p;
    if (!JS_IsObject(obj))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    return !p->free_mark;
}

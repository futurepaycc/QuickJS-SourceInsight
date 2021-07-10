//
// Created by benpeng.jiang on 2021/7/10.
//

#include <assert.h>
#include "qjs-resolver.h"
#include "qjs-opcode.h"
#include "qjs-ir.h"

static void instantiate_hoisted_definitions(JSContext *ctx, JSFunctionDef *s, DynBuf *bc)
{
    int i, idx, label_next = -1;

    /* add the hoisted functions in arguments and local variables */
    for(i = 0; i < s->arg_count; i++) {
        JSVarDef *vd = &s->args[i];
        if (vd->func_pool_idx >= 0) {
            dbuf_putc(bc, OP_fclosure);
            dbuf_put_u32(bc, vd->func_pool_idx);
            dbuf_putc(bc, OP_put_arg);
            dbuf_put_u16(bc, i);
        }
    }
    for(i = 0; i < s->var_count; i++) {
        JSVarDef *vd = &s->vars[i];
        if (vd->scope_level == 0 && vd->func_pool_idx >= 0) {
            dbuf_putc(bc, OP_fclosure);
            dbuf_put_u32(bc, vd->func_pool_idx);
            dbuf_putc(bc, OP_put_loc);
            dbuf_put_u16(bc, i);
        }
    }

    /* the module global variables must be initialized before
       evaluating the module so that the exported functions are
       visible if there are cyclic module references */
    if (s->module) {
        label_next = new_label_fd(s, -1);

        /* if 'this' is true, initialize the global variables and return */
        dbuf_putc(bc, OP_push_this);
        dbuf_putc(bc, OP_if_false);
        dbuf_put_u32(bc, label_next);
        update_label(s, label_next, 1);
        s->jump_size++;
    }

    /* add the global variables (only happens if s->is_global_var is
       true) */
    for(i = 0; i < s->global_var_count; i++) {
        JSGlobalVar *hf = &s->global_vars[i];
        int has_closure = 0;
        BOOL force_init = hf->force_init;
        /* we are in an eval, so the closure contains all the
           enclosing variables */
        /* If the outer function has a variable environment,
           create a property for the variable there */
        for(idx = 0; idx < s->closure_var_count; idx++) {
            JSClosureVar *cv = &s->closure_var[idx];
            if (cv->var_name == hf->var_name) {
                has_closure = 2;
                force_init = FALSE;
                break;
            }
            if (cv->var_name == JS_ATOM__var_ ||
                cv->var_name == JS_ATOM__arg_var_) {
                dbuf_putc(bc, OP_get_var_ref);
                dbuf_put_u16(bc, idx);
                has_closure = 1;
                force_init = TRUE;
                break;
            }
        }
        if (!has_closure) {
            int flags;

            flags = 0;
            if (s->eval_type != JS_EVAL_TYPE_GLOBAL)
                flags |= JS_PROP_CONFIGURABLE;
            if (hf->cpool_idx >= 0 && !hf->is_lexical) {
                /* global function definitions need a specific handling */
                dbuf_putc(bc, OP_fclosure);
                dbuf_put_u32(bc, hf->cpool_idx);

                dbuf_putc(bc, OP_define_func);
                dbuf_put_u32(bc, JS_DupAtom(ctx, hf->var_name));
                dbuf_putc(bc, flags);

                goto done_global_var;
            } else {
                if (hf->is_lexical) {
                    flags |= DEFINE_GLOBAL_LEX_VAR;
                    if (!hf->is_const)
                        flags |= JS_PROP_WRITABLE;
                }
                dbuf_putc(bc, OP_define_var);
                dbuf_put_u32(bc, JS_DupAtom(ctx, hf->var_name));
                dbuf_putc(bc, flags);
            }
        }
        if (hf->cpool_idx >= 0 || force_init) {
            if (hf->cpool_idx >= 0) {
                dbuf_putc(bc, OP_fclosure);
                dbuf_put_u32(bc, hf->cpool_idx);
                if (hf->var_name == JS_ATOM__default_) {
                    /* set default export function name */
                    dbuf_putc(bc, OP_set_name);
                    dbuf_put_u32(bc, JS_DupAtom(ctx, JS_ATOM_default));
                }
            } else {
                dbuf_putc(bc, OP_undefined);
            }
            if (has_closure == 2) {
                dbuf_putc(bc, OP_put_var_ref);
                dbuf_put_u16(bc, idx);
            } else if (has_closure == 1) {
                dbuf_putc(bc, OP_define_field);
                dbuf_put_u32(bc, JS_DupAtom(ctx, hf->var_name));
                dbuf_putc(bc, OP_drop);
            } else {
                /* XXX: Check if variable is writable and enumerable */
                dbuf_putc(bc, OP_put_var);
                dbuf_put_u32(bc, JS_DupAtom(ctx, hf->var_name));
            }
        }
        done_global_var:
        JS_FreeAtom(ctx, hf->var_name);
    }

    if (s->module) {
        dbuf_putc(bc, OP_return_undef);

        dbuf_putc(bc, OP_label);
        dbuf_put_u32(bc, label_next);
        s->label_slots[label_next].pos2 = bc->size;
    }

    js_free(ctx, s->global_vars);
    s->global_vars = NULL;
    s->global_var_count = 0;
    s->global_var_size = 0;
}


static int get_label_pos(JSFunctionDef *s, int label)
{
    int i, pos;
    for (i = 0; i < 20; i++) {
        pos = s->label_slots[label].pos;
        for (;;) {
            switch (s->byte_code.buf[pos]) {
                case OP_line_num:
                case OP_label:
                    pos += 5;
                    continue;
                case OP_goto:
                    label = get_u32(s->byte_code.buf + pos + 1);
                    break;
                default:
                    return pos;
            }
            break;
        }
    }
    return pos;
}


int add_closure_var(JSContext *ctx, JSFunctionDef *s,
                    BOOL is_local, BOOL is_arg,
                    int var_idx, JSAtom var_name,
                    BOOL is_const, BOOL is_lexical,
                    JSVarKindEnum var_kind)
{
    JSClosureVar *cv;

    /* the closure variable indexes are currently stored on 16 bits */
    if (s->closure_var_count >= JS_MAX_LOCAL_VARS) {
        JS_ThrowInternalError(ctx, "too many closure variables");
        return -1;
    }

    if (js_resize_array(ctx, (void **)&s->closure_var,
                        sizeof(s->closure_var[0]),
                        &s->closure_var_size, s->closure_var_count + 1))
        return -1;
    cv = &s->closure_var[s->closure_var_count++];
    cv->is_local = is_local;
    cv->is_arg = is_arg;
    cv->is_const = is_const;
    cv->is_lexical = is_lexical;
    cv->var_kind = var_kind;
    cv->var_idx = var_idx;
    cv->var_name = JS_DupAtom(ctx, var_name);
    return s->closure_var_count - 1;
}

 int find_closure_var(JSContext *ctx, JSFunctionDef *s,
                            JSAtom var_name)
{
    int i;
    for(i = 0; i < s->closure_var_count; i++) {
        JSClosureVar *cv = &s->closure_var[i];
        if (cv->var_name == var_name)
            return i;
    }
    return -1;
}

/* 'fd' must be a parent of 's'. Create in 's' a closure referencing a
   local variable (is_local = TRUE) or a closure (is_local = FALSE) in
   'fd' */
 int get_closure_var2(JSContext *ctx, JSFunctionDef *s,
                            JSFunctionDef *fd, BOOL is_local,
                            BOOL is_arg, int var_idx, JSAtom var_name,
                            BOOL is_const, BOOL is_lexical,
                            JSVarKindEnum var_kind)
{
    int i;

    if (fd != s->parent) {
        var_idx = get_closure_var2(ctx, s->parent, fd, is_local,
                                   is_arg, var_idx, var_name,
                                   is_const, is_lexical, var_kind);
        if (var_idx < 0)
            return -1;
        is_local = FALSE;
    }
    for(i = 0; i < s->closure_var_count; i++) {
        JSClosureVar *cv = &s->closure_var[i];
        if (cv->var_idx == var_idx && cv->is_arg == is_arg &&
            cv->is_local == is_local)
            return i;
    }
    return add_closure_var(ctx, s, is_local, is_arg, var_idx, var_name,
                           is_const, is_lexical, var_kind);
}

 int get_closure_var(JSContext *ctx, JSFunctionDef *s,
                           JSFunctionDef *fd, BOOL is_arg,
                           int var_idx, JSAtom var_name,
                           BOOL is_const, BOOL is_lexical,
                           JSVarKindEnum var_kind)
{
    return get_closure_var2(ctx, s, fd, TRUE, is_arg,
                            var_idx, var_name, is_const, is_lexical,
                            var_kind);
}

static int get_with_scope_opcode(int op)
{
    if (op == OP_scope_get_var_undef)
        return OP_with_get_var;
    else
        return OP_with_get_var + (op - OP_scope_get_var);
}

static BOOL can_opt_put_ref_value(const uint8_t *bc_buf, int pos)
{
    int opcode = bc_buf[pos];
    return (bc_buf[pos + 1] == OP_put_ref_value &&
            (opcode == OP_insert3 ||
             opcode == OP_perm4 ||
             opcode == OP_nop ||
             opcode == OP_rot3l));
}

static BOOL can_opt_put_global_ref_value(const uint8_t *bc_buf, int pos)
{
    int opcode = bc_buf[pos];
    return (bc_buf[pos + 1] == OP_put_ref_value &&
            (opcode == OP_insert3 ||
             opcode == OP_perm4 ||
             opcode == OP_nop ||
             opcode == OP_rot3l));
}

static int optimize_scope_make_ref(JSContext *ctx, JSFunctionDef *s,
                                   DynBuf *bc, uint8_t *bc_buf,
                                   LabelSlot *ls, int pos_next,
                                   int get_op, int var_idx)
{
    int label_pos, end_pos, pos;

    /* XXX: should optimize `loc(a) += expr` as `expr add_loc(a)`
       but only if expr does not modify `a`.
       should scan the code between pos_next and label_pos
       for operations that can potentially change `a`:
       OP_scope_make_ref(a), function calls, jumps and gosub.
     */
    /* replace the reference get/put with normal variable
       accesses */
    if (bc_buf[pos_next] == OP_get_ref_value) {
        dbuf_putc(bc, get_op);
        dbuf_put_u16(bc, var_idx);
        pos_next++;
    }
    /* remove the OP_label to make room for replacement */
    /* label should have a refcount of 0 anyway */
    /* XXX: should avoid this patch by inserting nops in phase 1 */
    label_pos = ls->pos;
    pos = label_pos - 5;
    assert(bc_buf[pos] == OP_label);
    /* label points to an instruction pair:
       - insert3 / put_ref_value
       - perm4 / put_ref_value
       - rot3l / put_ref_value
       - nop / put_ref_value
     */
    end_pos = label_pos + 2;
    if (bc_buf[label_pos] == OP_insert3)
        bc_buf[pos++] = OP_dup;
    bc_buf[pos] = get_op + 1;
    put_u16(bc_buf + pos + 1, var_idx);
    pos += 3;
    /* pad with OP_nop */
    while (pos < end_pos)
        bc_buf[pos++] = OP_nop;
    return pos_next;
}

static int optimize_scope_make_global_ref(JSContext *ctx, JSFunctionDef *s,
                                          DynBuf *bc, uint8_t *bc_buf,
                                          LabelSlot *ls, int pos_next,
                                          JSAtom var_name)
{
    int label_pos, end_pos, pos, op;
    BOOL is_strict;
    is_strict = ((s->js_mode & JS_MODE_STRICT) != 0);

    /* replace the reference get/put with normal variable
       accesses */
    if (is_strict) {
        /* need to check if the variable exists before evaluating the right
           expression */
        /* XXX: need an extra OP_true if destructuring an array */
        dbuf_putc(bc, OP_check_var);
        dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
    } else {
        /* XXX: need 2 extra OP_true if destructuring an array */
    }
    if (bc_buf[pos_next] == OP_get_ref_value) {
        dbuf_putc(bc, OP_get_var);
        dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
        pos_next++;
    }
    /* remove the OP_label to make room for replacement */
    /* label should have a refcount of 0 anyway */
    /* XXX: should have emitted several OP_nop to avoid this kludge */
    label_pos = ls->pos;
    pos = label_pos - 5;
    assert(bc_buf[pos] == OP_label);
    end_pos = label_pos + 2;
    op = bc_buf[label_pos];
    if (is_strict) {
        if (op != OP_nop) {
            switch(op) {
                case OP_insert3:
                    op = OP_insert2;
                    break;
                case OP_perm4:
                    op = OP_perm3;
                    break;
                case OP_rot3l:
                    op = OP_swap;
                    break;
                default:
                    abort();
            }
            bc_buf[pos++] = op;
        }
    } else {
        if (op == OP_insert3)
            bc_buf[pos++] = OP_dup;
    }
    if (is_strict) {
        bc_buf[pos] = OP_put_var_strict;
        /* XXX: need 1 extra OP_drop if destructuring an array */
    } else {
        bc_buf[pos] = OP_put_var;
        /* XXX: need 2 extra OP_drop if destructuring an array */
    }
    put_u32(bc_buf + pos + 1, JS_DupAtom(ctx, var_name));
    pos += 5;
    /* pad with OP_nop */
    while (pos < end_pos)
        bc_buf[pos++] = OP_nop;
    return pos_next;
}

 int add_var_this(JSContext *ctx, JSFunctionDef *fd)
{
    int idx;
    idx = add_var(ctx, fd, JS_ATOM_this);
    if (idx >= 0 && fd->is_derived_class_constructor) {
        JSVarDef *vd = &fd->vars[idx];
        /* XXX: should have is_this flag or var type */
        vd->is_lexical = 1; /* used to trigger 'uninitialized' checks
                               in a derived class constructor */
    }
    return idx;
}

static int resolve_pseudo_var(JSContext *ctx, JSFunctionDef *s,
                              JSAtom var_name)
{
    int var_idx;

    if (!s->has_this_binding)
        return -1;
    switch(var_name) {
        case JS_ATOM_home_object:
            /* 'home_object' pseudo variable */
            if (s->home_object_var_idx < 0)
                s->home_object_var_idx = add_var(ctx, s, var_name);
            var_idx = s->home_object_var_idx;
            break;
        case JS_ATOM_this_active_func:
            /* 'this.active_func' pseudo variable */
            if (s->this_active_func_var_idx < 0)
                s->this_active_func_var_idx = add_var(ctx, s, var_name);
            var_idx = s->this_active_func_var_idx;
            break;
        case JS_ATOM_new_target:
            /* 'new.target' pseudo variable */
            if (s->new_target_var_idx < 0)
                s->new_target_var_idx = add_var(ctx, s, var_name);
            var_idx = s->new_target_var_idx;
            break;
        case JS_ATOM_this:
            /* 'this' pseudo variable */
            if (s->this_var_idx < 0)
                s->this_var_idx = add_var_this(ctx, s);
            var_idx = s->this_var_idx;
            break;
        default:
            var_idx = -1;
            break;
    }
    return var_idx;
}

/* test if 'var_name' is in the variable object on the stack. If is it
   the case, handle it and jump to 'label_done' */
static void var_object_test(JSContext *ctx, JSFunctionDef *s,
                            JSAtom var_name, int op, DynBuf *bc,
                            int *plabel_done, BOOL is_with)
{
    dbuf_putc(bc, get_with_scope_opcode(op));
    dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
    *plabel_done = new_label_fd(s, *plabel_done);
    dbuf_put_u32(bc, *plabel_done);
    dbuf_putc(bc, is_with);
    update_label(s, *plabel_done, 1);
    s->jump_size++;
}


static void mark_eval_captured_variables(JSContext *ctx, JSFunctionDef *s,
                                         int scope_level)
{
    int idx;
    JSVarDef *vd;

    for (idx = s->scopes[scope_level].first; idx >= 0;) {
        vd = &s->vars[idx];
        vd->is_captured = 1;
        idx = vd->scope_next;
    }
}

/* return the position of the next opcode */
static int resolve_scope_var(JSContext *ctx, JSFunctionDef *s,
                             JSAtom var_name, int scope_level, int op,
                             DynBuf *bc, uint8_t *bc_buf,
                             LabelSlot *ls, int pos_next)
{
    int idx, var_idx, is_put;
    int label_done;
    JSFunctionDef *fd;
    JSVarDef *vd;
    BOOL is_pseudo_var, is_arg_scope;

    label_done = -1;

    /* XXX: could be simpler to use a specific function to
       resolve the pseudo variables */
    is_pseudo_var = (var_name == JS_ATOM_home_object ||
                     var_name == JS_ATOM_this_active_func ||
                     var_name == JS_ATOM_new_target ||
                     var_name == JS_ATOM_this);

    /* resolve local scoped variables */
    var_idx = -1;
    for (idx = s->scopes[scope_level].first; idx >= 0;) {
        vd = &s->vars[idx];
        if (vd->var_name == var_name) {
            if (op == OP_scope_put_var || op == OP_scope_make_ref) {
                if (vd->is_const) {
                    dbuf_putc(bc, OP_throw_error);
                    dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                    dbuf_putc(bc, JS_THROW_VAR_RO);
                    goto done;
                }
            }
            var_idx = idx;
            break;
        } else
        if (vd->var_name == JS_ATOM__with_ && !is_pseudo_var) {
            dbuf_putc(bc, OP_get_loc);
            dbuf_put_u16(bc, idx);
            var_object_test(ctx, s, var_name, op, bc, &label_done, 1);
        }
        idx = vd->scope_next;
    }
    is_arg_scope = (idx == ARG_SCOPE_END);
    if (var_idx < 0) {
        /* argument scope: variables are not visible but pseudo
           variables are visible */
        if (!is_arg_scope) {
            var_idx = find_var(ctx, s, var_name);
        }

        if (var_idx < 0 && is_pseudo_var)
            var_idx = resolve_pseudo_var(ctx, s, var_name);

        if (var_idx < 0 && var_name == JS_ATOM_arguments &&
            s->has_arguments_binding) {
            /* 'arguments' pseudo variable */
            var_idx = add_arguments_var(ctx, s);
        }
        if (var_idx < 0 && s->is_func_expr && var_name == s->func_name) {
            /* add a new variable with the function name */
            var_idx = add_func_var(ctx, s, var_name);
        }
    }
    if (var_idx >= 0) {
        if ((op == OP_scope_put_var || op == OP_scope_make_ref) &&
            !(var_idx & ARGUMENT_VAR_OFFSET) &&
            s->vars[var_idx].is_const) {
            /* only happens when assigning a function expression name
               in strict mode */
            dbuf_putc(bc, OP_throw_error);
            dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            dbuf_putc(bc, JS_THROW_VAR_RO);
            goto done;
        }
        /* OP_scope_put_var_init is only used to initialize a
           lexical variable, so it is never used in a with or var object. It
           can be used with a closure (module global variable case). */
        switch (op) {
            case OP_scope_make_ref:
                if (!(var_idx & ARGUMENT_VAR_OFFSET) &&
                    s->vars[var_idx].var_kind == JS_VAR_FUNCTION_NAME) {
                    /* Create a dummy object reference for the func_var */
                    dbuf_putc(bc, OP_object);
                    dbuf_putc(bc, OP_get_loc);
                    dbuf_put_u16(bc, var_idx);
                    dbuf_putc(bc, OP_define_field);
                    dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                    dbuf_putc(bc, OP_push_atom_value);
                    dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                } else
                if (label_done == -1 && can_opt_put_ref_value(bc_buf, ls->pos)) {
                    int get_op;
                    if (var_idx & ARGUMENT_VAR_OFFSET) {
                        get_op = OP_get_arg;
                        var_idx -= ARGUMENT_VAR_OFFSET;
                    } else {
                        if (s->vars[var_idx].is_lexical)
                            get_op = OP_get_loc_check;
                        else
                            get_op = OP_get_loc;
                    }
                    pos_next = optimize_scope_make_ref(ctx, s, bc, bc_buf, ls,
                                                       pos_next, get_op, var_idx);
                } else {
                    /* Create a dummy object with a named slot that is
                       a reference to the local variable */
                    if (var_idx & ARGUMENT_VAR_OFFSET) {
                        dbuf_putc(bc, OP_make_arg_ref);
                        dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                        dbuf_put_u16(bc, var_idx - ARGUMENT_VAR_OFFSET);
                    } else {
                        dbuf_putc(bc, OP_make_loc_ref);
                        dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                        dbuf_put_u16(bc, var_idx);
                    }
                }
                break;
            case OP_scope_get_ref:
                dbuf_putc(bc, OP_undefined);
                /* fall thru */
            case OP_scope_get_var_undef:
            case OP_scope_get_var:
            case OP_scope_put_var:
            case OP_scope_put_var_init:
                is_put = (op == OP_scope_put_var || op == OP_scope_put_var_init);
                if (var_idx & ARGUMENT_VAR_OFFSET) {
                    dbuf_putc(bc, OP_get_arg + is_put);
                    dbuf_put_u16(bc, var_idx - ARGUMENT_VAR_OFFSET);
                } else {
                    if (is_put) {
                        if (s->vars[var_idx].is_lexical) {
                            if (op == OP_scope_put_var_init) {
                                /* 'this' can only be initialized once */
                                if (var_name == JS_ATOM_this)
                                    dbuf_putc(bc, OP_put_loc_check_init);
                                else
                                    dbuf_putc(bc, OP_put_loc);
                            } else {
                                dbuf_putc(bc, OP_put_loc_check);
                            }
                        } else {
                            dbuf_putc(bc, OP_put_loc);
                        }
                    } else {
                        if (s->vars[var_idx].is_lexical) {
                            dbuf_putc(bc, OP_get_loc_check);
                        } else {
                            dbuf_putc(bc, OP_get_loc);
                        }
                    }
                    dbuf_put_u16(bc, var_idx);
                }
                break;
            case OP_scope_delete_var:
                dbuf_putc(bc, OP_push_false);
                break;
        }
        goto done;
    }
    /* check eval object */
    if (!is_arg_scope && s->var_object_idx >= 0 && !is_pseudo_var) {
        dbuf_putc(bc, OP_get_loc);
        dbuf_put_u16(bc, s->var_object_idx);
        var_object_test(ctx, s, var_name, op, bc, &label_done, 0);
    }
    /* check eval object in argument scope */
    if (s->arg_var_object_idx >= 0 && !is_pseudo_var) {
        dbuf_putc(bc, OP_get_loc);
        dbuf_put_u16(bc, s->arg_var_object_idx);
        var_object_test(ctx, s, var_name, op, bc, &label_done, 0);
    }

    /* check parent scopes */
    for (fd = s; fd->parent;) {
        scope_level = fd->parent_scope_level;
        fd = fd->parent;
        for (idx = fd->scopes[scope_level].first; idx >= 0;) {
            vd = &fd->vars[idx];
            if (vd->var_name == var_name) {
                if (op == OP_scope_put_var || op == OP_scope_make_ref) {
                    if (vd->is_const) {
                        dbuf_putc(bc, OP_throw_error);
                        dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                        dbuf_putc(bc, JS_THROW_VAR_RO);
                        goto done;
                    }
                }
                var_idx = idx;
                break;
            } else if (vd->var_name == JS_ATOM__with_ && !is_pseudo_var) {
                vd->is_captured = 1;
                idx = get_closure_var(ctx, s, fd, FALSE, idx, vd->var_name, FALSE, FALSE, JS_VAR_NORMAL);
                if (idx >= 0) {
                    dbuf_putc(bc, OP_get_var_ref);
                    dbuf_put_u16(bc, idx);
                    var_object_test(ctx, s, var_name, op, bc, &label_done, 1);
                }
            }
            idx = vd->scope_next;
        }
        is_arg_scope = (idx == ARG_SCOPE_END);
        if (var_idx >= 0)
            break;

        if (!is_arg_scope) {
            var_idx = find_var(ctx, fd, var_name);
            if (var_idx >= 0)
                break;
        }
        if (is_pseudo_var) {
            var_idx = resolve_pseudo_var(ctx, fd, var_name);
            if (var_idx >= 0)
                break;
        }
        if (var_name == JS_ATOM_arguments && fd->has_arguments_binding) {
            var_idx = add_arguments_var(ctx, fd);
            break;
        }
        if (fd->is_func_expr && fd->func_name == var_name) {
            /* add a new variable with the function name */
            var_idx = add_func_var(ctx, fd, var_name);
            break;
        }

        /* check eval object */
        if (!is_arg_scope && fd->var_object_idx >= 0 && !is_pseudo_var) {
            vd = &fd->vars[fd->var_object_idx];
            vd->is_captured = 1;
            idx = get_closure_var(ctx, s, fd, FALSE,
                                  fd->var_object_idx, vd->var_name,
                                  FALSE, FALSE, JS_VAR_NORMAL);
            dbuf_putc(bc, OP_get_var_ref);
            dbuf_put_u16(bc, idx);
            var_object_test(ctx, s, var_name, op, bc, &label_done, 0);
        }

        /* check eval object in argument scope */
        if (fd->arg_var_object_idx >= 0 && !is_pseudo_var) {
            vd = &fd->vars[fd->arg_var_object_idx];
            vd->is_captured = 1;
            idx = get_closure_var(ctx, s, fd, FALSE,
                                  fd->arg_var_object_idx, vd->var_name,
                                  FALSE, FALSE, JS_VAR_NORMAL);
            dbuf_putc(bc, OP_get_var_ref);
            dbuf_put_u16(bc, idx);
            var_object_test(ctx, s, var_name, op, bc, &label_done, 0);
        }

        if (fd->is_eval)
            break; /* it it necessarily the top level function */
    }

    /* check direct eval scope (in the closure of the eval function
       which is necessarily at the top level) */
    if (!fd)
        fd = s;
    if (var_idx < 0 && fd->is_eval) {
        int idx1;
        for (idx1 = 0; idx1 < fd->closure_var_count; idx1++) {
            JSClosureVar *cv = &fd->closure_var[idx1];
            if (var_name == cv->var_name) {
                if (fd != s) {
                    idx = get_closure_var2(ctx, s, fd,
                                           FALSE,
                                           cv->is_arg, idx1,
                                           cv->var_name, cv->is_const,
                                           cv->is_lexical, cv->var_kind);
                } else {
                    idx = idx1;
                }
                goto has_idx;
            } else if ((cv->var_name == JS_ATOM__var_ ||
                        cv->var_name == JS_ATOM__arg_var_ ||
                        cv->var_name == JS_ATOM__with_) && !is_pseudo_var) {
                int is_with = (cv->var_name == JS_ATOM__with_);
                if (fd != s) {
                    idx = get_closure_var2(ctx, s, fd,
                                           FALSE,
                                           cv->is_arg, idx1,
                                           cv->var_name, FALSE, FALSE,
                                           JS_VAR_NORMAL);
                } else {
                    idx = idx1;
                }
                dbuf_putc(bc, OP_get_var_ref);
                dbuf_put_u16(bc, idx);
                var_object_test(ctx, s, var_name, op, bc, &label_done, is_with);
            }
        }
    }

    if (var_idx >= 0) {
        /* find the corresponding closure variable */
        if (var_idx & ARGUMENT_VAR_OFFSET) {
            fd->args[var_idx - ARGUMENT_VAR_OFFSET].is_captured = 1;
            idx = get_closure_var(ctx, s, fd,
                                  TRUE, var_idx - ARGUMENT_VAR_OFFSET,
                                  var_name, FALSE, FALSE, JS_VAR_NORMAL);
        } else {
            fd->vars[var_idx].is_captured = 1;
            idx = get_closure_var(ctx, s, fd,
                                  FALSE, var_idx,
                                  var_name,
                                  fd->vars[var_idx].is_const,
                                  fd->vars[var_idx].is_lexical,
                                  fd->vars[var_idx].var_kind);
        }
        if (idx >= 0) {
            has_idx:
            if ((op == OP_scope_put_var || op == OP_scope_make_ref) &&
                s->closure_var[idx].is_const) {
                dbuf_putc(bc, OP_throw_error);
                dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                dbuf_putc(bc, JS_THROW_VAR_RO);
                goto done;
            }
            switch (op) {
                case OP_scope_make_ref:
                    if (s->closure_var[idx].var_kind == JS_VAR_FUNCTION_NAME) {
                        /* Create a dummy object reference for the func_var */
                        dbuf_putc(bc, OP_object);
                        dbuf_putc(bc, OP_get_var_ref);
                        dbuf_put_u16(bc, idx);
                        dbuf_putc(bc, OP_define_field);
                        dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                        dbuf_putc(bc, OP_push_atom_value);
                        dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                    } else
                    if (label_done == -1 &&
                        can_opt_put_ref_value(bc_buf, ls->pos)) {
                        int get_op;
                        if (s->closure_var[idx].is_lexical)
                            get_op = OP_get_var_ref_check;
                        else
                            get_op = OP_get_var_ref;
                        pos_next = optimize_scope_make_ref(ctx, s, bc, bc_buf, ls,
                                                           pos_next,
                                                           get_op, idx);
                    } else {
                        /* Create a dummy object with a named slot that is
                           a reference to the closure variable */
                        dbuf_putc(bc, OP_make_var_ref_ref);
                        dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                        dbuf_put_u16(bc, idx);
                    }
                    break;
                case OP_scope_get_ref:
                    /* XXX: should create a dummy object with a named slot that is
                       a reference to the closure variable */
                    dbuf_putc(bc, OP_undefined);
                    /* fall thru */
                case OP_scope_get_var_undef:
                case OP_scope_get_var:
                case OP_scope_put_var:
                case OP_scope_put_var_init:
                    is_put = (op == OP_scope_put_var ||
                              op == OP_scope_put_var_init);
                    if (is_put) {
                        if (s->closure_var[idx].is_lexical) {
                            if (op == OP_scope_put_var_init) {
                                /* 'this' can only be initialized once */
                                if (var_name == JS_ATOM_this)
                                    dbuf_putc(bc, OP_put_var_ref_check_init);
                                else
                                    dbuf_putc(bc, OP_put_var_ref);
                            } else {
                                dbuf_putc(bc, OP_put_var_ref_check);
                            }
                        } else {
                            dbuf_putc(bc, OP_put_var_ref);
                        }
                    } else {
                        if (s->closure_var[idx].is_lexical) {
                            dbuf_putc(bc, OP_get_var_ref_check);
                        } else {
                            dbuf_putc(bc, OP_get_var_ref);
                        }
                    }
                    dbuf_put_u16(bc, idx);
                    break;
                case OP_scope_delete_var:
                    dbuf_putc(bc, OP_push_false);
                    break;
            }
            goto done;
        }
    }

    /* global variable access */

    switch (op) {
        case OP_scope_make_ref:
            if (label_done == -1 && can_opt_put_global_ref_value(bc_buf, ls->pos)) {
                pos_next = optimize_scope_make_global_ref(ctx, s, bc, bc_buf, ls,
                                                          pos_next, var_name);
            } else {
                dbuf_putc(bc, OP_make_var_ref);
                dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            }
            break;
        case OP_scope_get_ref:
            /* XXX: should create a dummy object with a named slot that is
               a reference to the global variable */
            dbuf_putc(bc, OP_undefined);
            dbuf_putc(bc, OP_get_var);
            dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            break;
        case OP_scope_get_var_undef:
        case OP_scope_get_var:
        case OP_scope_put_var:
            dbuf_putc(bc, OP_get_var_undef + (op - OP_scope_get_var_undef));
            dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            break;
        case OP_scope_put_var_init:
            dbuf_putc(bc, OP_put_var_init);
            dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            break;
        case OP_scope_delete_var:
            dbuf_putc(bc, OP_delete_var);
            dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            break;
    }
    done:
    if (label_done >= 0) {
        dbuf_putc(bc, OP_label);
        dbuf_put_u32(bc, label_done);
        s->label_slots[label_done].pos2 = bc->size;
    }
    return pos_next;
}



/* convert global variable accesses to local variables or closure
   variables when necessary */
__exception int resolve_variables(JSContext *ctx, JSFunctionDef *s)
{
    int pos, pos_next, bc_len, op, len, i, idx, line_num;
    uint8_t *bc_buf;
    JSAtom var_name;
    DynBuf bc_out;
    CodeContext cc;
    int scope;

    cc.bc_buf = bc_buf = s->byte_code.buf;
    cc.bc_len = bc_len = s->byte_code.size;
    js_dbuf_init(ctx, &bc_out);

    /* first pass for runtime checks (must be done before the
       variables are created) */
    for(i = 0; i < s->global_var_count; i++) {
        JSGlobalVar *hf = &s->global_vars[i];
        int flags;

        /* check if global variable (XXX: simplify) */
        for(idx = 0; idx < s->closure_var_count; idx++) {
            JSClosureVar *cv = &s->closure_var[idx];
            if (cv->var_name == hf->var_name) {
                if (s->eval_type == JS_EVAL_TYPE_DIRECT &&
                    cv->is_lexical) {
                    /* Check if a lexical variable is
                       redefined as 'var'. XXX: Could abort
                       compilation here, but for consistency
                       with the other checks, we delay the
                       error generation. */
                    dbuf_putc(&bc_out, OP_throw_error);
                    dbuf_put_u32(&bc_out, JS_DupAtom(ctx, hf->var_name));
                    dbuf_putc(&bc_out, JS_THROW_VAR_REDECL);
                }
                goto next;
            }
            if (cv->var_name == JS_ATOM__var_ ||
                cv->var_name == JS_ATOM__arg_var_)
                goto next;
        }

        dbuf_putc(&bc_out, OP_check_define_var);
        dbuf_put_u32(&bc_out, JS_DupAtom(ctx, hf->var_name));
        flags = 0;
        if (hf->is_lexical)
            flags |= DEFINE_GLOBAL_LEX_VAR;
        if (hf->cpool_idx >= 0)
            flags |= DEFINE_GLOBAL_FUNC_VAR;
        dbuf_putc(&bc_out, flags);
        next: ;
    }

    line_num = 0; /* avoid warning */
    for (pos = 0; pos < bc_len; pos = pos_next) {
        op = bc_buf[pos];
        len = opcode_info[op].size;
        pos_next = pos + len;
        switch(op) {
            case OP_line_num:
                line_num = get_u32(bc_buf + pos + 1);
                s->line_number_size++;
                goto no_change;

            case OP_eval: /* convert scope index to adjusted variable index */
            {
                int call_argc = get_u16(bc_buf + pos + 1);
                scope = get_u16(bc_buf + pos + 1 + 2);
                mark_eval_captured_variables(ctx, s, scope);
                dbuf_putc(&bc_out, op);
                dbuf_put_u16(&bc_out, call_argc);
                dbuf_put_u16(&bc_out, s->scopes[scope].first + 1);
            }
                break;
            case OP_apply_eval: /* convert scope index to adjusted variable index */
                scope = get_u16(bc_buf + pos + 1);
                mark_eval_captured_variables(ctx, s, scope);
                dbuf_putc(&bc_out, op);
                dbuf_put_u16(&bc_out, s->scopes[scope].first + 1);
                break;
            case OP_scope_get_var_undef:
            case OP_scope_get_var:
            case OP_scope_put_var:
            case OP_scope_delete_var:
            case OP_scope_get_ref:
            case OP_scope_put_var_init:
                var_name = get_u32(bc_buf + pos + 1);
                scope = get_u16(bc_buf + pos + 5);
                pos_next = resolve_scope_var(ctx, s, var_name, scope, op, &bc_out,
                                             NULL, NULL, pos_next);
                JS_FreeAtom(ctx, var_name);
                break;
            case OP_scope_make_ref:
            {
                int label;
                LabelSlot *ls;
                var_name = get_u32(bc_buf + pos + 1);
                label = get_u32(bc_buf + pos + 5);
                scope = get_u16(bc_buf + pos + 9);
                ls = &s->label_slots[label];
                ls->ref_count--;  /* always remove label reference */
                pos_next = resolve_scope_var(ctx, s, var_name, scope, op, &bc_out,
                                             bc_buf, ls, pos_next);
                JS_FreeAtom(ctx, var_name);
            }
                break;
            case OP_scope_get_private_field:
            case OP_scope_get_private_field2:
            case OP_scope_put_private_field:
            {
                int ret;
                var_name = get_u32(bc_buf + pos + 1);
                scope = get_u16(bc_buf + pos + 5);
                ret = resolve_scope_private_field(ctx, s, var_name, scope, op, &bc_out);
                if (ret < 0)
                    goto fail;
                JS_FreeAtom(ctx, var_name);
            }
                break;
            case OP_gosub:
                s->jump_size++;
                if (OPTIMIZE) {
                    /* remove calls to empty finalizers  */
                    int label;
                    LabelSlot *ls;

                    label = get_u32(bc_buf + pos + 1);
                    assert(label >= 0 && label < s->label_count);
                    ls = &s->label_slots[label];
                    if (code_match(&cc, ls->pos, OP_ret, -1)) {
                        ls->ref_count--;
                        break;
                    }
                }
                goto no_change;
            case OP_drop:
                if (0) {
                    /* remove drops before return_undef */
                    /* do not perform this optimization in pass2 because
                       it breaks patterns recognised in resolve_labels */
                    int pos1 = pos_next;
                    int line1 = line_num;
                    while (code_match(&cc, pos1, OP_drop, -1)) {
                        if (cc.line_num >= 0) line1 = cc.line_num;
                        pos1 = cc.pos;
                    }
                    if (code_match(&cc, pos1, OP_return_undef, -1)) {
                        pos_next = pos1;
                        if (line1 != -1 && line1 != line_num) {
                            line_num = line1;
                            s->line_number_size++;
                            dbuf_putc(&bc_out, OP_line_num);
                            dbuf_put_u32(&bc_out, line_num);
                        }
                        break;
                    }
                }
                goto no_change;
            case OP_insert3:
                if (OPTIMIZE) {
                    /* Transformation: insert3 put_array_el|put_ref_value drop -> put_array_el|put_ref_value */
                    if (code_match(&cc, pos_next, M2(OP_put_array_el, OP_put_ref_value), OP_drop, -1)) {
                        dbuf_putc(&bc_out, cc.op);
                        pos_next = cc.pos;
                        if (cc.line_num != -1 && cc.line_num != line_num) {
                            line_num = cc.line_num;
                            s->line_number_size++;
                            dbuf_putc(&bc_out, OP_line_num);
                            dbuf_put_u32(&bc_out, line_num);
                        }
                        break;
                    }
                }
                goto no_change;

            case OP_goto:
                s->jump_size++;
                /* fall thru */
            case OP_tail_call:
            case OP_tail_call_method:
            case OP_return:
            case OP_return_undef:
            case OP_throw:
            case OP_throw_error:
            case OP_ret:
                if (OPTIMIZE) {
                    /* remove dead code */
                    int line = -1;
                    dbuf_put(&bc_out, bc_buf + pos, len);
                    pos = skip_dead_code(s, bc_buf, bc_len, pos + len, &line);
                    pos_next = pos;
                    if (pos < bc_len && line >= 0 && line_num != line) {
                        line_num = line;
                        s->line_number_size++;
                        dbuf_putc(&bc_out, OP_line_num);
                        dbuf_put_u32(&bc_out, line_num);
                    }
                    break;
                }
                goto no_change;

            case OP_label:
            {
                int label;
                LabelSlot *ls;

                label = get_u32(bc_buf + pos + 1);
                assert(label >= 0 && label < s->label_count);
                ls = &s->label_slots[label];
                ls->pos2 = bc_out.size + opcode_info[op].size;
            }
                goto no_change;

            case OP_enter_scope:
            {
                int scope_idx, scope = get_u16(bc_buf + pos + 1);

                if (scope == s->body_scope) {
                    instantiate_hoisted_definitions(ctx, s, &bc_out);
                }

                for(scope_idx = s->scopes[scope].first; scope_idx >= 0;) {
                    JSVarDef *vd = &s->vars[scope_idx];
                    if (vd->scope_level == scope) {
                        if (scope_idx != s->arguments_arg_idx) {
                            if (vd->var_kind == JS_VAR_FUNCTION_DECL ||
                                vd->var_kind == JS_VAR_NEW_FUNCTION_DECL) {
                                /* Initialize lexical variable upon entering scope */
                                dbuf_putc(&bc_out, OP_fclosure);
                                dbuf_put_u32(&bc_out, vd->func_pool_idx);
                                dbuf_putc(&bc_out, OP_put_loc);
                                dbuf_put_u16(&bc_out, scope_idx);
                            } else {
                                /* XXX: should check if variable can be used
                                   before initialization */
                                dbuf_putc(&bc_out, OP_set_loc_uninitialized);
                                dbuf_put_u16(&bc_out, scope_idx);
                            }
                        }
                        scope_idx = vd->scope_next;
                    } else {
                        break;
                    }
                }
            }
                break;

            case OP_leave_scope:
            {
                int scope_idx, scope = get_u16(bc_buf + pos + 1);

                for(scope_idx = s->scopes[scope].first; scope_idx >= 0;) {
                    JSVarDef *vd = &s->vars[scope_idx];
                    if (vd->scope_level == scope) {
                        if (vd->is_captured) {
                            dbuf_putc(&bc_out, OP_close_loc);
                            dbuf_put_u16(&bc_out, scope_idx);
                        }
                        scope_idx = vd->scope_next;
                    } else {
                        break;
                    }
                }
            }
                break;

            case OP_set_name:
            {
                /* remove dummy set_name opcodes */
                JSAtom name = get_u32(bc_buf + pos + 1);
                if (name == JS_ATOM_NULL)
                    break;
            }
                goto no_change;

            case OP_if_false:
            case OP_if_true:
            case OP_catch:
                s->jump_size++;
                goto no_change;

            case OP_dup:
                if (OPTIMIZE) {
                    /* Transformation: dup if_false(l1) drop, l1: if_false(l2) -> if_false(l2) */
                    /* Transformation: dup if_true(l1) drop, l1: if_true(l2) -> if_true(l2) */
                    if (code_match(&cc, pos_next, M2(OP_if_false, OP_if_true), OP_drop, -1)) {
                        int lab0, lab1, op1, pos1, line1, pos2;
                        lab0 = lab1 = cc.label;
                        assert(lab1 >= 0 && lab1 < s->label_count);
                        op1 = cc.op;
                        pos1 = cc.pos;
                        line1 = cc.line_num;
                        while (code_match(&cc, (pos2 = get_label_pos(s, lab1)), OP_dup, op1, OP_drop, -1)) {
                            lab1 = cc.label;
                        }
                        if (code_match(&cc, pos2, op1, -1)) {
                            s->jump_size++;
                            update_label(s, lab0, -1);
                            update_label(s, cc.label, +1);
                            dbuf_putc(&bc_out, op1);
                            dbuf_put_u32(&bc_out, cc.label);
                            pos_next = pos1;
                            if (line1 != -1 && line1 != line_num) {
                                line_num = line1;
                                s->line_number_size++;
                                dbuf_putc(&bc_out, OP_line_num);
                                dbuf_put_u32(&bc_out, line_num);
                            }
                            break;
                        }
                    }
                }
                goto no_change;

            case OP_nop:
                /* remove erased code */
                break;
            case OP_set_class_name:
                /* only used during parsing */
                break;

            default:
            no_change:
                dbuf_put(&bc_out, bc_buf + pos, len);
                break;
        }
    }

    /* set the new byte code */
    dbuf_free(&s->byte_code);
    s->byte_code = bc_out;
    if (dbuf_error(&s->byte_code)) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    return 0;
    fail:
    /* continue the copy to keep the atom refcounts consistent */
    /* XXX: find a better solution ? */
    for (; pos < bc_len; pos = pos_next) {
        op = bc_buf[pos];
        len = opcode_info[op].size;
        pos_next = pos + len;
        dbuf_put(&bc_out, bc_buf + pos, len);
    }
    dbuf_free(&s->byte_code);
    s->byte_code = bc_out;
    return -1;
}

//
// Created by benpeng.jiang on 2021/6/21.
//

#include "front-parser-stmt-decl.h"
#include "qjs-ir-emit.h"
#include "qjs-resolver.h"


static int add_import(JSParseState *s, JSModuleDef *m,
                      JSAtom local_name, JSAtom import_name)
{
    JSContext *ctx = s->ctx;
    int i, var_idx;
    JSImportEntry *mi;
    BOOL is_local;

    if (local_name == JS_ATOM_arguments || local_name == JS_ATOM_eval)
        return js_parse_error(s, "invalid import binding");

    if (local_name != JS_ATOM_default) {
        for (i = 0; i < s->cur_func->closure_var_count; i++) {
            if (s->cur_func->closure_var[i].var_name == local_name)
                return js_parse_error(s, "duplicate import binding");
        }
    }

    is_local = (import_name == JS_ATOM__star_);
    var_idx = add_closure_var(ctx, s->cur_func, is_local, FALSE,
                              m->import_entries_count,
                              local_name, TRUE, TRUE, FALSE);
    if (var_idx < 0)
        return -1;
    if (js_resize_array(ctx, (void **)&m->import_entries,
                        sizeof(JSImportEntry),
                        &m->import_entries_size,
                        m->import_entries_count + 1))
        return -1;
    mi = &m->import_entries[m->import_entries_count++];
    mi->import_name = JS_DupAtom(ctx, import_name);
    mi->var_idx = var_idx;
    return 0;
}

static int add_req_module_entry(JSContext *ctx, JSModuleDef *m,
                                JSAtom module_name)
{
    JSReqModuleEntry *rme;
    int i;

    /* no need to add the module request if it is already present */
    for(i = 0; i < m->req_module_entries_count; i++) {
        rme = &m->req_module_entries[i];
        if (rme->module_name == module_name)
            return i;
    }

    if (js_resize_array(ctx, (void **)&m->req_module_entries,
                        sizeof(JSReqModuleEntry),
                        &m->req_module_entries_size,
                        m->req_module_entries_count + 1))
        return -1;
    rme = &m->req_module_entries[m->req_module_entries_count++];
    rme->module_name = JS_DupAtom(ctx, module_name);
    rme->module = NULL;
    return i;
}

 JSExportEntry *find_export_entry(JSContext *ctx, JSModuleDef *m,
                                        JSAtom export_name)
{
    JSExportEntry *me;
    int i;
    for(i = 0; i < m->export_entries_count; i++) {
        me = &m->export_entries[i];
        if (me->export_name == export_name)
            return me;
    }
    return NULL;
}

 JSExportEntry *add_export_entry2(JSContext *ctx,
                                        JSParseState *s, JSModuleDef *m,
                                        JSAtom local_name, JSAtom export_name,
                                        JSExportTypeEnum export_type)
{
    JSExportEntry *me;

    if (find_export_entry(ctx, m, export_name)) {
        char buf1[ATOM_GET_STR_BUF_SIZE];
        if (s) {
            js_parse_error(s, "duplicate exported name '%s'",
                           JS_AtomGetStr(ctx, buf1, sizeof(buf1), export_name));
        } else {
            JS_ThrowSyntaxErrorAtom(ctx, "duplicate exported name '%s'", export_name);
        }
        return NULL;
    }

    if (js_resize_array(ctx, (void **)&m->export_entries,
                        sizeof(JSExportEntry),
                        &m->export_entries_size,
                        m->export_entries_count + 1))
        return NULL;
    me = &m->export_entries[m->export_entries_count++];
    memset(me, 0, sizeof(*me));
    me->local_name = JS_DupAtom(ctx, local_name);
    me->export_name = JS_DupAtom(ctx, export_name);
    me->export_type = export_type;
    return me;
}

JSExportEntry *add_export_entry(JSParseState *s, JSModuleDef *m,
                                JSAtom local_name, JSAtom export_name,
                                JSExportTypeEnum export_type)
{
    return add_export_entry2(s->ctx, s, m, local_name, export_name,
                             export_type);
}

static int add_star_export_entry(JSContext *ctx, JSModuleDef *m,
                                 int req_module_idx)
{
    JSStarExportEntry *se;

    if (js_resize_array(ctx, (void **)&m->star_export_entries,
                        sizeof(JSStarExportEntry),
                        &m->star_export_entries_size,
                        m->star_export_entries_count + 1))
        return -1;
    se = &m->star_export_entries[m->star_export_entries_count++];
    se->req_module_idx = req_module_idx;
    return 0;
}

static __exception JSAtom js_parse_from_clause(JSParseState *s)
{
    JSAtom module_name;
    if (!token_is_pseudo_keyword(s, JS_ATOM_from)) {
        js_parse_error(s, "from clause expected");
        return JS_ATOM_NULL;
    }
    if (next_token(s))
        return JS_ATOM_NULL;
    if (s->token.val != TOK_STRING) {
        js_parse_error(s, "string expected");
        return JS_ATOM_NULL;
    }
    module_name = JS_ValueToAtom(s->ctx, s->token.u.str.str);
    if (module_name == JS_ATOM_NULL)
        return JS_ATOM_NULL;
    if (next_token(s)) {
        JS_FreeAtom(s->ctx, module_name);
        return JS_ATOM_NULL;
    }
    return module_name;
}

static __exception int js_parse_import(JSParseState *s)
{
    JSContext *ctx = s->ctx;
    JSModuleDef *m = s->cur_func->module;
    JSAtom local_name, import_name, module_name;
    int first_import, i, idx;

    if (next_token(s))
        return -1;

    first_import = m->import_entries_count;
    if (s->token.val == TOK_STRING) {
        module_name = JS_ValueToAtom(ctx, s->token.u.str.str);
        if (module_name == JS_ATOM_NULL)
            return -1;
        if (next_token(s)) {
            JS_FreeAtom(ctx, module_name);
            return -1;
        }
    } else {
        if (s->token.val == TOK_IDENT) {
            if (s->token.u.ident.is_reserved) {
                return js_parse_error_reserved_identifier(s);
            }
            /* "default" import */
            local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            import_name = JS_ATOM_default;
            if (next_token(s))
                goto fail;
            if (add_import(s, m, local_name, import_name))
                goto fail;
            JS_FreeAtom(ctx, local_name);

            if (s->token.val != ',')
                goto end_import_clause;
            if (next_token(s))
                return -1;
        }

        if (s->token.val == '*') {
            /* name space import */
            if (next_token(s))
                return -1;
            if (!token_is_pseudo_keyword(s, JS_ATOM_as))
                return js_parse_error(s, "expecting 'as'");
            if (next_token(s))
                return -1;
            if (!token_is_ident(s->token.val)) {
                js_parse_error(s, "identifier expected");
                return -1;
            }
            local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            import_name = JS_ATOM__star_;
            if (next_token(s))
                goto fail;
            if (add_import(s, m, local_name, import_name))
                goto fail;
            JS_FreeAtom(ctx, local_name);
        } else if (s->token.val == '{') {
            if (next_token(s))
                return -1;

            while (s->token.val != '}') {
                if (!token_is_ident(s->token.val)) {
                    js_parse_error(s, "identifier expected");
                    return -1;
                }
                import_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                local_name = JS_ATOM_NULL;
                if (next_token(s))
                    goto fail;
                if (token_is_pseudo_keyword(s, JS_ATOM_as)) {
                    if (next_token(s))
                        goto fail;
                    if (!token_is_ident(s->token.val)) {
                        js_parse_error(s, "identifier expected");
                        goto fail;
                    }
                    local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                    if (next_token(s)) {
                        fail:
                        JS_FreeAtom(ctx, local_name);
                        JS_FreeAtom(ctx, import_name);
                        return -1;
                    }
                } else {
                    local_name = JS_DupAtom(ctx, import_name);
                }
                if (add_import(s, m, local_name, import_name))
                    goto fail;
                JS_FreeAtom(ctx, local_name);
                JS_FreeAtom(ctx, import_name);
                if (s->token.val != ',')
                    break;
                if (next_token(s))
                    return -1;
            }
            if (js_parse_expect(s, '}'))
                return -1;
        }
        end_import_clause:
        module_name = js_parse_from_clause(s);
        if (module_name == JS_ATOM_NULL)
            return -1;
    }
    idx = add_req_module_entry(ctx, m, module_name);
    JS_FreeAtom(ctx, module_name);
    if (idx < 0)
        return -1;
    for(i = first_import; i < m->import_entries_count; i++)
        m->import_entries[i].req_module_idx = idx;

    return js_parse_expect_semi(s);
}



static __exception int js_parse_export(JSParseState *s)
{
    JSContext *ctx = s->ctx;
    JSModuleDef *m = s->cur_func->module;
    JSAtom local_name, export_name;
    int first_export, idx, i, tok;
    JSAtom module_name;
    JSExportEntry *me;

    if (next_token(s))
        return -1;

    tok = s->token.val;
    if (tok == TOK_CLASS) {
        return js_parse_class(s, FALSE, JS_PARSE_EXPORT_NAMED);
    } else if (tok == TOK_FUNCTION ||
               (token_is_pseudo_keyword(s, JS_ATOM_async) &&
                peek_token(s, TRUE) == TOK_FUNCTION)) {
        return js_parse_function_decl2(s, JS_PARSE_FUNC_STATEMENT,
                                       JS_FUNC_NORMAL, JS_ATOM_NULL,
                                       s->token.ptr, s->token.line_num,
                                       JS_PARSE_EXPORT_NAMED, NULL);
    }

    if (next_token(s))
        return -1;

    switch(tok) {
        case '{':
            first_export = m->export_entries_count;
            while (s->token.val != '}') {
                if (!token_is_ident(s->token.val)) {
                    js_parse_error(s, "identifier expected");
                    return -1;
                }
                local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                export_name = JS_ATOM_NULL;
                if (next_token(s))
                    goto fail;
                if (token_is_pseudo_keyword(s, JS_ATOM_as)) {
                    if (next_token(s))
                        goto fail;
                    if (!token_is_ident(s->token.val)) {
                        js_parse_error(s, "identifier expected");
                        goto fail;
                    }
                    export_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                    if (next_token(s)) {
                        fail:
                        JS_FreeAtom(ctx, local_name);
                        fail1:
                        JS_FreeAtom(ctx, export_name);
                        return -1;
                    }
                } else {
                    export_name = JS_DupAtom(ctx, local_name);
                }
                me = add_export_entry(s, m, local_name, export_name,
                                      JS_EXPORT_TYPE_LOCAL);
                JS_FreeAtom(ctx, local_name);
                JS_FreeAtom(ctx, export_name);
                if (!me)
                    return -1;
                if (s->token.val != ',')
                    break;
                if (next_token(s))
                    return -1;
            }
            if (js_parse_expect(s, '}'))
                return -1;
            if (token_is_pseudo_keyword(s, JS_ATOM_from)) {
                module_name = js_parse_from_clause(s);
                if (module_name == JS_ATOM_NULL)
                    return -1;
                idx = add_req_module_entry(ctx, m, module_name);
                JS_FreeAtom(ctx, module_name);
                if (idx < 0)
                    return -1;
                for(i = first_export; i < m->export_entries_count; i++) {
                    me = &m->export_entries[i];
                    me->export_type = JS_EXPORT_TYPE_INDIRECT;
                    me->u.req_module_idx = idx;
                }
            }
            break;
        case '*':
            if (token_is_pseudo_keyword(s, JS_ATOM_as)) {
                /* export ns from */
                if (next_token(s))
                    return -1;
                if (!token_is_ident(s->token.val)) {
                    js_parse_error(s, "identifier expected");
                    return -1;
                }
                export_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                if (next_token(s))
                    goto fail1;
                module_name = js_parse_from_clause(s);
                if (module_name == JS_ATOM_NULL)
                    goto fail1;
                idx = add_req_module_entry(ctx, m, module_name);
                JS_FreeAtom(ctx, module_name);
                if (idx < 0)
                    goto fail1;
                me = add_export_entry(s, m, JS_ATOM__star_, export_name,
                                      JS_EXPORT_TYPE_INDIRECT);
                JS_FreeAtom(ctx, export_name);
                if (!me)
                    return -1;
                me->u.req_module_idx = idx;
            } else {
                module_name = js_parse_from_clause(s);
                if (module_name == JS_ATOM_NULL)
                    return -1;
                idx = add_req_module_entry(ctx, m, module_name);
                JS_FreeAtom(ctx, module_name);
                if (idx < 0)
                    return -1;
                if (add_star_export_entry(ctx, m, idx) < 0)
                    return -1;
            }
            break;
        case TOK_DEFAULT:
            if (s->token.val == TOK_CLASS) {
                return js_parse_class(s, FALSE, JS_PARSE_EXPORT_DEFAULT);
            } else if (s->token.val == TOK_FUNCTION ||
                       (token_is_pseudo_keyword(s, JS_ATOM_async) &&
                        peek_token(s, TRUE) == TOK_FUNCTION)) {
                return js_parse_function_decl2(s, JS_PARSE_FUNC_STATEMENT,
                                               JS_FUNC_NORMAL, JS_ATOM_NULL,
                                               s->token.ptr, s->token.line_num,
                                               JS_PARSE_EXPORT_DEFAULT, NULL);
            } else {
                if (js_parse_assign_expr(s))
                    return -1;
            }
            /* set the name of anonymous functions */
            set_object_name(s, JS_ATOM_default);

            /* store the value in the _default_ global variable and export
               it */
            local_name = JS_ATOM__default_;
            if (define_var(s, s->cur_func, local_name, JS_VAR_DEF_LET) < 0)
                return -1;
            emit_op(s, OP_scope_put_var_init);
            emit_atom(s, local_name);
            emit_u16(s, 0);

            if (!add_export_entry(s, m, local_name, JS_ATOM_default,
                                  JS_EXPORT_TYPE_LOCAL))
                return -1;
            break;
        case TOK_VAR:
        case TOK_LET:
        case TOK_CONST:
            return js_parse_var(s, TRUE, tok, TRUE);
        default:
            return js_parse_error(s, "invalid export syntax");
    }
    return js_parse_expect_semi(s);
}


__exception int js_parse_source_element(JSParseState *s) {
    JSFunctionDef *fd = s->cur_func;
    int tok;

    if (s->token.val == TOK_FUNCTION ||
        (token_is_pseudo_keyword(s, JS_ATOM_async) &&
         peek_token(s, TRUE) == TOK_FUNCTION)) {
        if (js_parse_function_decl(s, JS_PARSE_FUNC_STATEMENT,
                                   JS_FUNC_NORMAL, JS_ATOM_NULL,
                                   s->token.ptr, s->token.line_num))
            return -1;
    } else if (s->token.val == TOK_EXPORT && fd->module) {
        if (js_parse_export(s))
            return -1;
    } else if (s->token.val == TOK_IMPORT && fd->module &&
               ((tok = peek_token(s, FALSE)) != '(' && tok != '.')) {
        /* the peek_token is needed to avoid confusion with ImportCall
           (dynamic import) or import.meta */
        if (js_parse_import(s))
            return -1;
    } else {
        if (js_parse_statement_or_decl(s, DECL_MASK_ALL))
            return -1;
    }
    return 0;
}



/* return the variable index or -1 if not found,
   add ARGUMENT_VAR_OFFSET for argument variables */
static int find_arg(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
{
    int i;
    for(i = fd->arg_count; i-- > 0;) {
        if (fd->args[i].var_name == name)
            return i | ARGUMENT_VAR_OFFSET;
    }
    return -1;
}

int find_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
{
    int i;
    for(i = fd->var_count; i-- > 0;) {
        if (fd->vars[i].var_name == name && fd->vars[i].scope_level == 0)
            return i;
    }
    return find_arg(ctx, fd, name);
}

/* find a variable declaration in a given scope */
static int find_var_in_scope(JSContext *ctx, JSFunctionDef *fd,
                             JSAtom name, int scope_level)
{
    int scope_idx;
    for(scope_idx = fd->scopes[scope_level].first; scope_idx >= 0;
        scope_idx = fd->vars[scope_idx].scope_next) {
        if (fd->vars[scope_idx].scope_level != scope_level)
            break;
        if (fd->vars[scope_idx].var_name == name)
            return scope_idx;
    }
    return -1;
}

/* return true if scope == parent_scope or if scope is a child of
   parent_scope */
static BOOL is_child_scope(JSContext *ctx, JSFunctionDef *fd,
                           int scope, int parent_scope)
{
    while (scope >= 0) {
        if (scope == parent_scope)
            return TRUE;
        scope = fd->scopes[scope].parent;
    }
    return FALSE;
}

/* find a 'var' declaration in the same scope or a child scope */
static int find_var_in_child_scope(JSContext *ctx, JSFunctionDef *fd,
                                   JSAtom name, int scope_level)
{
    int i;
    for(i = 0; i < fd->var_count; i++) {
        JSVarDef *vd = &fd->vars[i];
        if (vd->var_name == name && vd->scope_level == 0) {
            if (is_child_scope(ctx, fd, vd->scope_next,
                               scope_level))
                return i;
        }
    }
    return -1;
}


static JSGlobalVar *find_global_var(JSFunctionDef *fd, JSAtom name)
{
    int i;
    for(i = 0; i < fd->global_var_count; i++) {
        JSGlobalVar *hf = &fd->global_vars[i];
        if (hf->var_name == name)
            return hf;
    }
    return NULL;

}

static JSGlobalVar *find_lexical_global_var(JSFunctionDef *fd, JSAtom name)
{
    JSGlobalVar *hf = find_global_var(fd, name);
    if (hf && hf->is_lexical)
        return hf;
    else
        return NULL;
}

static int find_lexical_decl(JSContext *ctx, JSFunctionDef *fd, JSAtom name,
                             int scope_idx, BOOL check_catch_var)
{
    while (scope_idx >= 0) {
        JSVarDef *vd = &fd->vars[scope_idx];
        if (vd->var_name == name &&
            (vd->is_lexical || (vd->var_kind == JS_VAR_CATCH &&
                                check_catch_var)))
            return scope_idx;
        scope_idx = vd->scope_next;
    }

    if (fd->is_eval && fd->eval_type == JS_EVAL_TYPE_GLOBAL) {
        if (find_lexical_global_var(fd, name))
            return GLOBAL_VAR_OFFSET;
    }
    return -1;
}





static void close_scopes(JSParseState *s, int scope, int scope_stop)
{
    while (scope > scope_stop) {
        emit_op(s, OP_leave_scope);
        emit_u16(s, scope);
        scope = s->cur_func->scopes[scope].parent;
    }
}

/* return the variable index or -1 if error */
int add_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
{
    JSVarDef *vd;

    /* the local variable indexes are currently stored on 16 bits */
    if (fd->var_count >= JS_MAX_LOCAL_VARS) {
        JS_ThrowInternalError(ctx, "too many local variables");
        return -1;
    }
    if (js_resize_array(ctx, (void **)&fd->vars, sizeof(fd->vars[0]),
                        &fd->var_size, fd->var_count + 1))
        return -1;
    vd = &fd->vars[fd->var_count++];
    memset(vd, 0, sizeof(*vd));
    vd->var_name = JS_DupAtom(ctx, name);
    vd->func_pool_idx = -1;
    return fd->var_count - 1;
}

int add_scope_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name,
                  JSVarKindEnum var_kind)
{
    int idx = add_var(ctx, fd, name);
    if (idx >= 0) {
        JSVarDef *vd = &fd->vars[idx];
        vd->var_kind = var_kind;
        vd->scope_level = fd->scope_level;
        vd->scope_next = fd->scope_first;
        fd->scopes[fd->scope_level].first = idx;
        fd->scope_first = idx;
    }
    return idx;
}

int add_func_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
{
    int idx = fd->func_var_idx;
    if (idx < 0 && (idx = add_var(ctx, fd, name)) >= 0) {
        fd->func_var_idx = idx;
        fd->vars[idx].var_kind = JS_VAR_FUNCTION_NAME;
        if (fd->js_mode & JS_MODE_STRICT)
            fd->vars[idx].is_const = TRUE;
    }
    return idx;
}

int add_arguments_var(JSContext *ctx, JSFunctionDef *fd)
{
    int idx = fd->arguments_var_idx;
    if (idx < 0 && (idx = add_var(ctx, fd, JS_ATOM_arguments)) >= 0) {
        fd->arguments_var_idx = idx;
    }
    return idx;
}

/* add an argument definition in the argument scope. Only needed when
   "eval()" may be called in the argument scope. Return 0 if OK. */
 int add_arguments_arg(JSContext *ctx, JSFunctionDef *fd)
{
    int idx;
    if (fd->arguments_arg_idx < 0) {
        idx = find_var_in_scope(ctx, fd, JS_ATOM_arguments, ARG_SCOPE_INDEX);
        if (idx < 0) {
            /* XXX: the scope links are not fully updated. May be an
               issue if there are child scopes of the argument
               scope */
            idx = add_var(ctx, fd, JS_ATOM_arguments);
            if (idx < 0)
                return -1;
            fd->vars[idx].scope_next = fd->scopes[ARG_SCOPE_INDEX].first;
            fd->scopes[ARG_SCOPE_INDEX].first = idx;
            fd->vars[idx].scope_level = ARG_SCOPE_INDEX;
            fd->vars[idx].is_lexical = TRUE;

            fd->arguments_arg_idx = idx;
        }
    }
    return 0;
}

static int add_arg(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
{
    JSVarDef *vd;

    /* the local variable indexes are currently stored on 16 bits */
    if (fd->arg_count >= JS_MAX_LOCAL_VARS) {
        JS_ThrowInternalError(ctx, "too many arguments");
        return -1;
    }
    if (js_resize_array(ctx, (void **)&fd->args, sizeof(fd->args[0]),
                        &fd->arg_size, fd->arg_count + 1))
        return -1;
    vd = &fd->args[fd->arg_count++];
    memset(vd, 0, sizeof(*vd));
    vd->var_name = JS_DupAtom(ctx, name);
    vd->func_pool_idx = -1;
    return fd->arg_count - 1;
}

/* add a global variable definition */
static JSGlobalVar *add_global_var(JSContext *ctx, JSFunctionDef *s,
                                   JSAtom name)
{
    JSGlobalVar *hf;

    if (js_resize_array(ctx, (void **)&s->global_vars,
                        sizeof(s->global_vars[0]),
                        &s->global_var_size, s->global_var_count + 1))
        return NULL;
    hf = &s->global_vars[s->global_var_count++];
    hf->cpool_idx = -1;
    hf->force_init = FALSE;
    hf->is_lexical = FALSE;
    hf->is_const = FALSE;
    hf->scope_level = s->scope_level;
    hf->var_name = JS_DupAtom(ctx, name);
    return hf;
}



int define_var(JSParseState *s, JSFunctionDef *fd, JSAtom name,
               JSVarDefEnum var_def_type)
{
    JSContext *ctx = s->ctx;
    JSVarDef *vd;
    int idx;

    switch (var_def_type) {
        case JS_VAR_DEF_WITH:
            idx = add_scope_var(ctx, fd, name, JS_VAR_NORMAL);
            break;

        case JS_VAR_DEF_LET:
        case JS_VAR_DEF_CONST:
        case JS_VAR_DEF_FUNCTION_DECL:
        case JS_VAR_DEF_NEW_FUNCTION_DECL:
            idx = find_lexical_decl(ctx, fd, name, fd->scope_first, TRUE);
            if (idx >= 0) {
                if (idx < GLOBAL_VAR_OFFSET) {
                    if (fd->vars[idx].scope_level == fd->scope_level) {
                        /* same scope: in non strict mode, functions
                           can be redefined (annex B.3.3.4). */
                        if (!(!(fd->js_mode & JS_MODE_STRICT) &&
                              var_def_type == JS_VAR_DEF_FUNCTION_DECL &&
                              fd->vars[idx].var_kind == JS_VAR_FUNCTION_DECL)) {
                            goto redef_lex_error;
                        }
                    } else if (fd->vars[idx].var_kind == JS_VAR_CATCH && (fd->vars[idx].scope_level + 2) == fd->scope_level) {
                        goto redef_lex_error;
                    }
                } else {
                    if (fd->scope_level == fd->body_scope) {
                        redef_lex_error:
                        /* redefining a scoped var in the same scope: error */
                        return js_parse_error(s, "invalid redefinition of lexical identifier");
                    }
                }
            }
            if (var_def_type != JS_VAR_DEF_FUNCTION_DECL &&
                var_def_type != JS_VAR_DEF_NEW_FUNCTION_DECL &&
                fd->scope_level == fd->body_scope &&
                find_arg(ctx, fd, name) >= 0) {
                /* lexical variable redefines a parameter name */
                return js_parse_error(s, "invalid redefinition of parameter name");
            }

            if (find_var_in_child_scope(ctx, fd, name, fd->scope_level) >= 0) {
                return js_parse_error(s, "invalid redefinition of a variable");
            }

            if (fd->is_global_var) {
                JSGlobalVar *hf;
                hf = find_global_var(fd, name);
                if (hf && is_child_scope(ctx, fd, hf->scope_level,
                                         fd->scope_level)) {
                    return js_parse_error(s, "invalid redefinition of global identifier");
                }
            }

            if (fd->is_eval &&
                (fd->eval_type == JS_EVAL_TYPE_GLOBAL ||
                 fd->eval_type == JS_EVAL_TYPE_MODULE) &&
                fd->scope_level == fd->body_scope) {
                JSGlobalVar *hf;
                hf = add_global_var(s->ctx, fd, name);
                if (!hf)
                    return -1;
                hf->is_lexical = TRUE;
                hf->is_const = (var_def_type == JS_VAR_DEF_CONST);
                idx = GLOBAL_VAR_OFFSET;
            } else {
                JSVarKindEnum var_kind;
                if (var_def_type == JS_VAR_DEF_FUNCTION_DECL)
                    var_kind = JS_VAR_FUNCTION_DECL;
                else if (var_def_type == JS_VAR_DEF_NEW_FUNCTION_DECL)
                    var_kind = JS_VAR_NEW_FUNCTION_DECL;
                else
                    var_kind = JS_VAR_NORMAL;
                idx = add_scope_var(ctx, fd, name, var_kind);
                if (idx >= 0) {
                    vd = &fd->vars[idx];
                    vd->is_lexical = 1;
                    vd->is_const = (var_def_type == JS_VAR_DEF_CONST);
                }
            }
            break;

        case JS_VAR_DEF_CATCH:
            idx = add_scope_var(ctx, fd, name, JS_VAR_CATCH);
            break;

        case JS_VAR_DEF_VAR:
            if (find_lexical_decl(ctx, fd, name, fd->scope_first,
                                  FALSE) >= 0) {
                invalid_lexical_redefinition:
                /* error to redefine a var that inside a lexical scope */
                return js_parse_error(s, "invalid redefinition of lexical identifier");
            }
            if (fd->is_global_var) {
                JSGlobalVar *hf;
                hf = find_global_var(fd, name);
                if (hf && hf->is_lexical && hf->scope_level == fd->scope_level &&
                    fd->eval_type == JS_EVAL_TYPE_MODULE) {
                    goto invalid_lexical_redefinition;
                }
                hf = add_global_var(s->ctx, fd, name);
                if (!hf)
                    return -1;
                idx = GLOBAL_VAR_OFFSET;
            } else {
                /* if the variable already exists, don't add it again  */
                idx = find_var(ctx, fd, name);
                if (idx >= 0)
                    break;
                idx = add_var(ctx, fd, name);
                if (idx >= 0) {
                    if (name == JS_ATOM_arguments && fd->has_arguments_binding)
                        fd->arguments_var_idx = idx;
                    fd->vars[idx].scope_next = fd->scope_level;
                }
            }
            break;
        default:
            abort();
    }
    return idx;
}




static void push_break_entry(JSFunctionDef *fd, BlockEnv *be,
                             JSAtom label_name,
                             int label_break, int label_cont,
                             int drop_count);
static void pop_break_entry(JSFunctionDef *fd);


__exception int get_lvalue(JSParseState *s, int *popcode, int *pscope,
                           JSAtom *pname, int *plabel, int *pdepth, BOOL keep,
                           int tok)
{
    JSFunctionDef *fd;
    int opcode, scope, label, depth;
    JSAtom name;

    /* we check the last opcode to get the lvalue type */
    fd = s->cur_func;
    scope = 0;
    name = JS_ATOM_NULL;
    label = -1;
    depth = 0;
    switch(opcode = get_prev_opcode(fd)) {
        case OP_scope_get_var:
            name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
            scope = get_u16(fd->byte_code.buf + fd->last_opcode_pos + 5);
            if ((name == JS_ATOM_arguments || name == JS_ATOM_eval) &&
                (fd->js_mode & JS_MODE_STRICT)) {
                return js_parse_error(s, "invalid lvalue in strict mode");
            }
            if (name == JS_ATOM_this || name == JS_ATOM_new_target)
                goto invalid_lvalue;
            depth = 2;  /* will generate OP_get_ref_value */
            break;
        case OP_get_field:
            name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
            depth = 1;
            break;
        case OP_scope_get_private_field:
            name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
            scope = get_u16(fd->byte_code.buf + fd->last_opcode_pos + 5);
            depth = 1;
            break;
        case OP_get_array_el:
            depth = 2;
            break;
        case OP_get_super_value:
            depth = 3;
            break;
        default:
        invalid_lvalue:
            if (tok == TOK_FOR) {
                return js_parse_error(s, "invalid for in/of left hand-side");
            } else if (tok == TOK_INC || tok == TOK_DEC) {
                return js_parse_error(s, "invalid increment/decrement operand");
            } else if (tok == '[' || tok == '{') {
                return js_parse_error(s, "invalid destructuring target");
            } else {
                return js_parse_error(s, "invalid assignment left-hand side");
            }
    }
    /* remove the last opcode */
    fd->byte_code.size = fd->last_opcode_pos;
    fd->last_opcode_pos = -1;

    if (keep) {
        /* get the value but keep the object/fields on the stack */
        switch(opcode) {
            case OP_scope_get_var:
                label = new_label(s);
                emit_op(s, OP_scope_make_ref);
                emit_atom(s, name);
                emit_u32(s, label);
                emit_u16(s, scope);
                update_label(fd, label, 1);
                emit_op(s, OP_get_ref_value);
                opcode = OP_get_ref_value;
                break;
            case OP_get_field:
                emit_op(s, OP_get_field2);
                emit_atom(s, name);
                break;
            case OP_scope_get_private_field:
                emit_op(s, OP_scope_get_private_field2);
                emit_atom(s, name);
                emit_u16(s, scope);
                break;
            case OP_get_array_el:
                /* XXX: replace by a single opcode ? */
                emit_op(s, OP_to_propkey2);
                emit_op(s, OP_dup2);
                emit_op(s, OP_get_array_el);
                break;
            case OP_get_super_value:
                emit_op(s, OP_to_propkey);
                emit_op(s, OP_dup3);
                emit_op(s, OP_get_super_value);
                break;
            default:
                abort();
        }
    } else {
        switch(opcode) {
            case OP_scope_get_var:
                label = new_label(s);
                emit_op(s, OP_scope_make_ref);
                emit_atom(s, name);
                emit_u32(s, label);
                emit_u16(s, scope);
                update_label(fd, label, 1);
                opcode = OP_get_ref_value;
                break;
            case OP_get_array_el:
                emit_op(s, OP_to_propkey2);
                break;
            case OP_get_super_value:
                emit_op(s, OP_to_propkey);
                break;
        }
    }

    *popcode = opcode;
    *pscope = scope;
    /* name has refcount for OP_get_field and OP_get_ref_value,
       and JS_ATOM_NULL for other opcodes */
    *pname = name;
    *plabel = label;
    if (pdepth)
        *pdepth = depth;
    return 0;
}



/* name has a live reference. 'is_let' is only used with opcode =
   OP_scope_get_var which is never generated by get_lvalue(). */
void put_lvalue(JSParseState *s, int opcode, int scope,
                JSAtom name, int label, PutLValueEnum special,
                BOOL is_let)
{
    switch(opcode) {
        case OP_get_field:
        case OP_scope_get_private_field:
            /* depth = 1 */
            switch(special) {
                case PUT_LVALUE_NOKEEP:
                case PUT_LVALUE_NOKEEP_DEPTH:
                    break;
                case PUT_LVALUE_KEEP_TOP:
                    emit_op(s, OP_insert2); /* obj v -> v obj v */
                    break;
                case PUT_LVALUE_KEEP_SECOND:
                    emit_op(s, OP_perm3); /* obj v0 v -> v0 obj v */
                    break;
                case PUT_LVALUE_NOKEEP_BOTTOM:
                    emit_op(s, OP_swap);
                    break;
                default:
                    abort();
            }
            break;
        case OP_get_array_el:
        case OP_get_ref_value:
            /* depth = 2 */
            if (opcode == OP_get_ref_value) {
                JS_FreeAtom(s->ctx, name);
                emit_label(s, label);
            }
            switch(special) {
                case PUT_LVALUE_NOKEEP:
                    emit_op(s, OP_nop); /* will trigger optimization */
                    break;
                case PUT_LVALUE_NOKEEP_DEPTH:
                    break;
                case PUT_LVALUE_KEEP_TOP:
                    emit_op(s, OP_insert3); /* obj prop v -> v obj prop v */
                    break;
                case PUT_LVALUE_KEEP_SECOND:
                    emit_op(s, OP_perm4); /* obj prop v0 v -> v0 obj prop v */
                    break;
                case PUT_LVALUE_NOKEEP_BOTTOM:
                    emit_op(s, OP_rot3l);
                    break;
                default:
                    abort();
            }
            break;
        case OP_get_super_value:
            /* depth = 3 */
            switch(special) {
                case PUT_LVALUE_NOKEEP:
                case PUT_LVALUE_NOKEEP_DEPTH:
                    break;
                case PUT_LVALUE_KEEP_TOP:
                    emit_op(s, OP_insert4); /* this obj prop v -> v this obj prop v */
                    break;
                case PUT_LVALUE_KEEP_SECOND:
                    emit_op(s, OP_perm5); /* this obj prop v0 v -> v0 this obj prop v */
                    break;
                case PUT_LVALUE_NOKEEP_BOTTOM:
                    emit_op(s, OP_rot4l);
                    break;
                default:
                    abort();
            }
            break;
        default:
            break;
    }

    switch(opcode) {
        case OP_scope_get_var:  /* val -- */
            assert(special == PUT_LVALUE_NOKEEP ||
                   special == PUT_LVALUE_NOKEEP_DEPTH);
            emit_op(s, is_let ? OP_scope_put_var_init : OP_scope_put_var);
            emit_u32(s, name);  /* has refcount */
            emit_u16(s, scope);
            break;
        case OP_get_field:
            emit_op(s, OP_put_field);
            emit_u32(s, name);  /* name has refcount */
            break;
        case OP_scope_get_private_field:
            emit_op(s, OP_scope_put_private_field);
            emit_u32(s, name);  /* name has refcount */
            emit_u16(s, scope);
            break;
        case OP_get_array_el:
            emit_op(s, OP_put_array_el);
            break;
        case OP_get_ref_value:
            emit_op(s, OP_put_ref_value);
            break;
        case OP_get_super_value:
            emit_op(s, OP_put_super_value);
            break;
        default:
            abort();
    }
}

__exception int js_parse_expr_paren(JSParseState *s)
{
    if (js_parse_expect(s, '('))
        return -1;
    if (js_parse_expr(s))
        return -1;
    if (js_parse_expect(s, ')'))
        return -1;
    return 0;
}

static int js_unsupported_keyword(JSParseState *s, JSAtom atom)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return js_parse_error(s, "unsupported keyword: %s",
                          JS_AtomGetStr(s->ctx, buf, sizeof(buf), atom));
}

static __exception int js_define_var(JSParseState *s, JSAtom name, int tok)
{
    JSFunctionDef *fd = s->cur_func;
    JSVarDefEnum var_def_type;

    if (name == JS_ATOM_yield && fd->func_kind == JS_FUNC_GENERATOR) {
        return js_parse_error(s, "yield is a reserved identifier");
    }
    if ((name == JS_ATOM_arguments || name == JS_ATOM_eval)
        &&  (fd->js_mode & JS_MODE_STRICT)) {
        return js_parse_error(s, "invalid variable name in strict mode");
    }
    if ((name == JS_ATOM_let || name == JS_ATOM_undefined)
        &&  (tok == TOK_LET || tok == TOK_CONST)) {
        return js_parse_error(s, "invalid lexical variable name");
    }
    switch(tok) {
        case TOK_LET:
            var_def_type = JS_VAR_DEF_LET;
            break;
        case TOK_CONST:
            var_def_type = JS_VAR_DEF_CONST;
            break;
        case TOK_VAR:
            var_def_type = JS_VAR_DEF_VAR;
            break;
        case TOK_CATCH:
            var_def_type = JS_VAR_DEF_CATCH;
            break;
        default:
            abort();
    }
    if (define_var(s, fd, name, var_def_type) < 0)
        return -1;
    return 0;
}

static void js_emit_spread_code(JSParseState *s, int depth)
{
    int label_rest_next, label_rest_done;

    /* XXX: could check if enum object is an actual array and optimize
       slice extraction. enumeration record and target array are in a
       different order from OP_append case. */
    /* enum_rec xxx -- enum_rec xxx array 0 */
    emit_op(s, OP_array_from);
    emit_u16(s, 0);
    emit_op(s, OP_push_i32);
    emit_u32(s, 0);
    emit_label(s, label_rest_next = new_label(s));
    emit_op(s, OP_for_of_next);
    emit_u8(s, 2 + depth);
    label_rest_done = emit_goto(s, OP_if_true, -1);
    /* array idx val -- array idx */
    emit_op(s, OP_define_array_el);
    emit_op(s, OP_inc);
    emit_goto(s, OP_goto, label_rest_next);
    emit_label(s, label_rest_done);
    /* enum_rec xxx array idx undef -- enum_rec xxx array */
    emit_op(s, OP_drop);
    emit_op(s, OP_drop);
}

static int js_parse_check_duplicate_parameter(JSParseState *s, JSAtom name)
{
    /* Check for duplicate parameter names */
    JSFunctionDef *fd = s->cur_func;
    int i;
    for (i = 0; i < fd->arg_count; i++) {
        if (fd->args[i].var_name == name)
            goto duplicate;
    }
    for (i = 0; i < fd->var_count; i++) {
        if (fd->vars[i].var_name == name)
            goto duplicate;
    }
    return 0;

    duplicate:
    return js_parse_error(s, "duplicate parameter names not allowed in this context");
}

static JSAtom js_parse_destructuring_var(JSParseState *s, int tok, int is_arg)
{
    JSAtom name;

    if (!(s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved)
        ||  ((s->cur_func->js_mode & JS_MODE_STRICT) &&
             (s->token.u.ident.atom == JS_ATOM_eval || s->token.u.ident.atom == JS_ATOM_arguments))) {
        js_parse_error(s, "invalid destructuring target");
        return JS_ATOM_NULL;
    }
    name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
    if (is_arg && js_parse_check_duplicate_parameter(s, name))
        goto fail;
    if (next_token(s))
        goto fail;

    return name;
    fail:
    JS_FreeAtom(s->ctx, name);
    return JS_ATOM_NULL;
}

/* Return -1 if error, 0 if no initializer, 1 if an initializer is
   present at the top level. */
int js_parse_destructuring_element(JSParseState *s, int tok, int is_arg,
                                   int hasval, int has_ellipsis,
                                   BOOL allow_initializer)
{
    int label_parse, label_assign, label_done, label_lvalue, depth_lvalue;
    int start_addr, assign_addr;
    JSAtom prop_name, var_name;
    int opcode, scope, tok1, skip_bits;
    BOOL has_initializer;

    if (has_ellipsis < 0) {
        /* pre-parse destructuration target for spread detection */
        js_parse_skip_parens_token(s, &skip_bits, FALSE);
        has_ellipsis = skip_bits & SKIP_HAS_ELLIPSIS;
    }

    label_parse = new_label(s);
    label_assign = new_label(s);

    start_addr = s->cur_func->byte_code.size;
    if (hasval) {
        /* consume value from the stack */
        emit_op(s, OP_dup);
        emit_op(s, OP_undefined);
        emit_op(s, OP_strict_eq);
        emit_goto(s, OP_if_true, label_parse);
        emit_label(s, label_assign);
    } else {
        emit_goto(s, OP_goto, label_parse);
        emit_label(s, label_assign);
        /* leave value on the stack */
        emit_op(s, OP_dup);
    }
    assign_addr = s->cur_func->byte_code.size;
    if (s->token.val == '{') {
        if (next_token(s))
            return -1;
        /* throw an exception if the value cannot be converted to an object */
        emit_op(s, OP_to_object);
        if (has_ellipsis) {
            /* add excludeList on stack just below src object */
            emit_op(s, OP_object);
            emit_op(s, OP_swap);
        }
        while (s->token.val != '}') {
            int prop_type;
            if (s->token.val == TOK_ELLIPSIS) {
                if (!has_ellipsis) {
                    JS_ThrowInternalError(s->ctx, "unexpected ellipsis token");
                    return -1;
                }
                if (next_token(s))
                    return -1;
                if (tok) {
                    var_name = js_parse_destructuring_var(s, tok, is_arg);
                    if (var_name == JS_ATOM_NULL)
                        return -1;
                    opcode = OP_scope_get_var;
                    scope = s->cur_func->scope_level;
                    label_lvalue = -1;
                    depth_lvalue = 0;
                } else {
                    if (js_parse_left_hand_side_expr(s))
                        return -1;

                    if (get_lvalue(s, &opcode, &scope, &var_name,
                                   &label_lvalue, &depth_lvalue, FALSE, '{'))
                        return -1;
                }
                if (s->token.val != '}') {
                    js_parse_error(s, "assignment rest property must be last");
                    goto var_error;
                }
                emit_op(s, OP_object);  /* target */
                emit_op(s, OP_copy_data_properties);
                emit_u8(s, 0 | ((depth_lvalue + 1) << 2) | ((depth_lvalue + 2) << 5));
                goto set_val;
            }
            prop_type = js_parse_property_name(s, &prop_name, FALSE, TRUE, FALSE);
            if (prop_type < 0)
                return -1;
            var_name = JS_ATOM_NULL;
            opcode = OP_scope_get_var;
            scope = s->cur_func->scope_level;
            label_lvalue = -1;
            depth_lvalue = 0;
            if (prop_type == PROP_TYPE_IDENT) {
                if (next_token(s))
                    goto prop_error;
                if ((s->token.val == '[' || s->token.val == '{')
                    &&  ((tok1 = js_parse_skip_parens_token(s, &skip_bits, FALSE)) == ',' ||
                         tok1 == '=' || tok1 == '}')) {
                    if (prop_name == JS_ATOM_NULL) {
                        /* computed property name on stack */
                        if (has_ellipsis) {
                            /* define the property in excludeList */
                            emit_op(s, OP_to_propkey); /* avoid calling ToString twice */
                            emit_op(s, OP_perm3); /* TOS: src excludeList prop */
                            emit_op(s, OP_null); /* TOS: src excludeList prop null */
                            emit_op(s, OP_define_array_el); /* TOS: src excludeList prop */
                            emit_op(s, OP_perm3); /* TOS: excludeList src prop */
                        }
                        /* get the computed property from the source object */
                        emit_op(s, OP_get_array_el2);
                    } else {
                        /* named property */
                        if (has_ellipsis) {
                            /* define the property in excludeList */
                            emit_op(s, OP_swap); /* TOS: src excludeList */
                            emit_op(s, OP_null); /* TOS: src excludeList null */
                            emit_op(s, OP_define_field); /* TOS: src excludeList */
                            emit_atom(s, prop_name);
                            emit_op(s, OP_swap); /* TOS: excludeList src */
                        }
                        /* get the named property from the source object */
                        emit_op(s, OP_get_field2);
                        emit_u32(s, prop_name);
                    }
                    if (js_parse_destructuring_element(s, tok, is_arg, TRUE, -1, TRUE) < 0)
                        return -1;
                    if (s->token.val == '}')
                        break;
                    /* accept a trailing comma before the '}' */
                    if (js_parse_expect(s, ','))
                        return -1;
                    continue;
                }
                if (prop_name == JS_ATOM_NULL) {
                    emit_op(s, OP_to_propkey2);
                    if (has_ellipsis) {
                        /* define the property in excludeList */
                        emit_op(s, OP_perm3);
                        emit_op(s, OP_null);
                        emit_op(s, OP_define_array_el);
                        emit_op(s, OP_perm3);
                    }
                    /* source prop -- source source prop */
                    emit_op(s, OP_dup1);
                } else {
                    if (has_ellipsis) {
                        /* define the property in excludeList */
                        emit_op(s, OP_swap);
                        emit_op(s, OP_null);
                        emit_op(s, OP_define_field);
                        emit_atom(s, prop_name);
                        emit_op(s, OP_swap);
                    }
                    /* source -- source source */
                    emit_op(s, OP_dup);
                }
                if (tok) {
                    var_name = js_parse_destructuring_var(s, tok, is_arg);
                    if (var_name == JS_ATOM_NULL)
                        goto prop_error;
                } else {
                    if (js_parse_left_hand_side_expr(s))
                        goto prop_error;
                    lvalue:
                    if (get_lvalue(s, &opcode, &scope, &var_name,
                                   &label_lvalue, &depth_lvalue, FALSE, '{'))
                        goto prop_error;
                    /* swap ref and lvalue object if any */
                    if (prop_name == JS_ATOM_NULL) {
                        switch(depth_lvalue) {
                            case 1:
                                /* source prop x -> x source prop */
                                emit_op(s, OP_rot3r);
                                break;
                            case 2:
                                /* source prop x y -> x y source prop */
                                emit_op(s, OP_swap2);   /* t p2 s p1 */
                                break;
                            case 3:
                                /* source prop x y z -> x y z source prop */
                                emit_op(s, OP_rot5l);
                                emit_op(s, OP_rot5l);
                                break;
                        }
                    } else {
                        switch(depth_lvalue) {
                            case 1:
                                /* source x -> x source */
                                emit_op(s, OP_swap);
                                break;
                            case 2:
                                /* source x y -> x y source */
                                emit_op(s, OP_rot3l);
                                break;
                            case 3:
                                /* source x y z -> x y z source */
                                emit_op(s, OP_rot4l);
                                break;
                        }
                    }
                }
                if (prop_name == JS_ATOM_NULL) {
                    /* computed property name on stack */
                    /* XXX: should have OP_get_array_el2x with depth */
                    /* source prop -- val */
                    emit_op(s, OP_get_array_el);
                } else {
                    /* named property */
                    /* XXX: should have OP_get_field2x with depth */
                    /* source -- val */
                    emit_op(s, OP_get_field);
                    emit_u32(s, prop_name);
                }
            } else {
                /* prop_type = PROP_TYPE_VAR, cannot be a computed property */
                if (is_arg && js_parse_check_duplicate_parameter(s, prop_name))
                    goto prop_error;
                if ((s->cur_func->js_mode & JS_MODE_STRICT) &&
                    (prop_name == JS_ATOM_eval || prop_name == JS_ATOM_arguments)) {
                    js_parse_error(s, "invalid destructuring target");
                    goto prop_error;
                }
                if (has_ellipsis) {
                    /* define the property in excludeList */
                    emit_op(s, OP_swap);
                    emit_op(s, OP_null);
                    emit_op(s, OP_define_field);
                    emit_atom(s, prop_name);
                    emit_op(s, OP_swap);
                }
                if (!tok || tok == TOK_VAR) {
                    /* generate reference */
                    /* source -- source source */
                    emit_op(s, OP_dup);
                    emit_op(s, OP_scope_get_var);
                    emit_atom(s, prop_name);
                    emit_u16(s, s->cur_func->scope_level);
                    goto lvalue;
                }
                var_name = JS_DupAtom(s->ctx, prop_name);
                /* source -- source val */
                emit_op(s, OP_get_field2);
                emit_u32(s, prop_name);
            }
            set_val:
            if (tok) {
                if (js_define_var(s, var_name, tok))
                    goto var_error;
                scope = s->cur_func->scope_level;
            }
            if (s->token.val == '=') {  /* handle optional default value */
                int label_hasval;
                emit_op(s, OP_dup);
                emit_op(s, OP_undefined);
                emit_op(s, OP_strict_eq);
                label_hasval = emit_goto(s, OP_if_false, -1);
                if (next_token(s))
                    goto var_error;
                emit_op(s, OP_drop);
                if (js_parse_assign_expr(s))
                    goto var_error;
                if (opcode == OP_scope_get_var || opcode == OP_get_ref_value)
                    set_object_name(s, var_name);
                emit_label(s, label_hasval);
            }
            /* store value into lvalue object */
            put_lvalue(s, opcode, scope, var_name, label_lvalue,
                       PUT_LVALUE_NOKEEP_DEPTH,
                       (tok == TOK_CONST || tok == TOK_LET));
            if (s->token.val == '}')
                break;
            /* accept a trailing comma before the '}' */
            if (js_parse_expect(s, ','))
                return -1;
        }
        /* drop the source object */
        emit_op(s, OP_drop);
        if (has_ellipsis) {
            emit_op(s, OP_drop); /* pop excludeList */
        }
        if (next_token(s))
            return -1;
    } else if (s->token.val == '[') {
        BOOL has_spread;
        int enum_depth;
        BlockEnv block_env;

        if (next_token(s))
            return -1;
        /* the block environment is only needed in generators in case
           'yield' triggers a 'return' */
        push_break_entry(s->cur_func, &block_env,
                         JS_ATOM_NULL, -1, -1, 2);
        block_env.has_iterator = TRUE;
        emit_op(s, OP_for_of_start);
        has_spread = FALSE;
        while (s->token.val != ']') {
            /* get the next value */
            if (s->token.val == TOK_ELLIPSIS) {
                if (next_token(s))
                    return -1;
                if (s->token.val == ',' || s->token.val == ']')
                    return js_parse_error(s, "missing binding pattern...");
                has_spread = TRUE;
            }
            if (s->token.val == ',') {
                /* do nothing, skip the value, has_spread is false */
                emit_op(s, OP_for_of_next);
                emit_u8(s, 0);
                emit_op(s, OP_drop);
                emit_op(s, OP_drop);
            } else if ((s->token.val == '[' || s->token.val == '{')
                       &&  ((tok1 = js_parse_skip_parens_token(s, &skip_bits, FALSE)) == ',' ||
                            tok1 == '=' || tok1 == ']')) {
                if (has_spread) {
                    if (tok1 == '=')
                        return js_parse_error(s, "rest element cannot have a default value");
                    js_emit_spread_code(s, 0);
                } else {
                    emit_op(s, OP_for_of_next);
                    emit_u8(s, 0);
                    emit_op(s, OP_drop);
                }
                if (js_parse_destructuring_element(s, tok, is_arg, TRUE, skip_bits & SKIP_HAS_ELLIPSIS, TRUE) < 0)
                    return -1;
            } else {
                var_name = JS_ATOM_NULL;
                enum_depth = 0;
                if (tok) {
                    var_name = js_parse_destructuring_var(s, tok, is_arg);
                    if (var_name == JS_ATOM_NULL)
                        goto var_error;
                    if (js_define_var(s, var_name, tok))
                        goto var_error;
                    opcode = OP_scope_get_var;
                    scope = s->cur_func->scope_level;
                } else {
                    if (js_parse_left_hand_side_expr(s))
                        return -1;
                    if (get_lvalue(s, &opcode, &scope, &var_name,
                                   &label_lvalue, &enum_depth, FALSE, '[')) {
                        return -1;
                    }
                }
                if (has_spread) {
                    js_emit_spread_code(s, enum_depth);
                } else {
                    emit_op(s, OP_for_of_next);
                    emit_u8(s, enum_depth);
                    emit_op(s, OP_drop);
                }
                if (s->token.val == '=' && !has_spread) {
                    /* handle optional default value */
                    int label_hasval;
                    emit_op(s, OP_dup);
                    emit_op(s, OP_undefined);
                    emit_op(s, OP_strict_eq);
                    label_hasval = emit_goto(s, OP_if_false, -1);
                    if (next_token(s))
                        goto var_error;
                    emit_op(s, OP_drop);
                    if (js_parse_assign_expr(s))
                        goto var_error;
                    if (opcode == OP_scope_get_var || opcode == OP_get_ref_value)
                        set_object_name(s, var_name);
                    emit_label(s, label_hasval);
                }
                /* store value into lvalue object */
                put_lvalue(s, opcode, scope, var_name,
                           label_lvalue, PUT_LVALUE_NOKEEP_DEPTH,
                           (tok == TOK_CONST || tok == TOK_LET));
            }
            if (s->token.val == ']')
                break;
            if (has_spread)
                return js_parse_error(s, "rest element must be the last one");
            /* accept a trailing comma before the ']' */
            if (js_parse_expect(s, ','))
                return -1;
        }
        /* close iterator object:
           if completed, enum_obj has been replaced by undefined */
        emit_op(s, OP_iterator_close);
        pop_break_entry(s->cur_func);
        if (next_token(s))
            return -1;
    } else {
        return js_parse_error(s, "invalid assignment syntax");
    }
    if (s->token.val == '=' && allow_initializer) {
        label_done = emit_goto(s, OP_goto, -1);
        if (next_token(s))
            return -1;
        emit_label(s, label_parse);
        if (hasval)
            emit_op(s, OP_drop);
        if (js_parse_assign_expr(s))
            return -1;
        emit_goto(s, OP_goto, label_assign);
        emit_label(s, label_done);
        has_initializer = TRUE;
    } else {
        /* normally hasval is true except if
           js_parse_skip_parens_token() was wrong in the parsing */
        //        assert(hasval);
        if (!hasval) {
            js_parse_error(s, "too complicated destructuring expression");
            return -1;
        }
        /* remove test and decrement label ref count */
        memset(s->cur_func->byte_code.buf + start_addr, OP_nop,
               assign_addr - start_addr);
        s->cur_func->label_slots[label_parse].ref_count--;
        has_initializer = FALSE;
    }
    return has_initializer;

    prop_error:
    JS_FreeAtom(s->ctx, prop_name);
    var_error:
    JS_FreeAtom(s->ctx, var_name);
    return -1;
}




static void push_break_entry(JSFunctionDef *fd, BlockEnv *be,
                             JSAtom label_name,
                             int label_break, int label_cont,
                             int drop_count)
{
    be->prev = fd->top_break;
    fd->top_break = be;
    be->label_name = label_name;
    be->label_break = label_break;
    be->label_cont = label_cont;
    be->drop_count = drop_count;
    be->label_finally = -1;
    be->scope_level = fd->scope_level;
    be->has_iterator = FALSE;
}

static void pop_break_entry(JSFunctionDef *fd)
{
    BlockEnv *be;
    be = fd->top_break;
    fd->top_break = be->prev;
}

static __exception int emit_break(JSParseState *s, JSAtom name, int is_cont)
{
    BlockEnv *top;
    int i, scope_level;

    scope_level = s->cur_func->scope_level;
    top = s->cur_func->top_break;
    while (top != NULL) {
        close_scopes(s, scope_level, top->scope_level);
        scope_level = top->scope_level;
        if (is_cont &&
            top->label_cont != -1 &&
            (name == JS_ATOM_NULL || top->label_name == name)) {
            /* continue stays inside the same block */
            emit_goto(s, OP_goto, top->label_cont);
            return 0;
        }
        if (!is_cont &&
            top->label_break != -1 &&
            (name == JS_ATOM_NULL || top->label_name == name)) {
            emit_goto(s, OP_goto, top->label_break);
            return 0;
        }
        i = 0;
        if (top->has_iterator) {
            emit_op(s, OP_iterator_close);
            i += 3;
        }
        for(; i < top->drop_count; i++)
            emit_op(s, OP_drop);
        if (top->label_finally != -1) {
            /* must push dummy value to keep same stack depth */
            emit_op(s, OP_undefined);
            emit_goto(s, OP_gosub, top->label_finally);
            emit_op(s, OP_drop);
        }
        top = top->prev;
    }
    if (name == JS_ATOM_NULL) {
        if (is_cont)
            return js_parse_error(s, "continue must be inside loop");
        else
            return js_parse_error(s, "break must be inside loop or switch");
    } else {
        return js_parse_error(s, "break/continue label not found");
    }
}

/* execute the finally blocks before return */
void emit_return(JSParseState *s, BOOL hasval)
{
    BlockEnv *top;
    int drop_count;

    drop_count = 0;
    top = s->cur_func->top_break;
    while (top != NULL) {
        /* XXX: emit the appropriate OP_leave_scope opcodes? Probably not
           required as all local variables will be closed upon returning
           from JS_CallInternal, but not in the same order. */
        if (top->has_iterator) {
            /* with 'yield', the exact number of OP_drop to emit is
               unknown, so we use a specific operation to look for
               the catch offset */
            if (!hasval) {
                emit_op(s, OP_undefined);
                hasval = TRUE;
            }
            emit_op(s, OP_iterator_close_return);
            if (s->cur_func->func_kind == JS_FUNC_ASYNC_GENERATOR) {
                int label_next, label_next2;

                emit_op(s, OP_drop); /* catch offset */
                emit_op(s, OP_drop); /* next */
                emit_op(s, OP_get_field2);
                emit_atom(s, JS_ATOM_return);
                /* stack: iter_obj return_func */
                emit_op(s, OP_dup);
                emit_op(s, OP_is_undefined_or_null);
                label_next = emit_goto(s, OP_if_true, -1);
                emit_op(s, OP_call_method);
                emit_u16(s, 0);
                emit_op(s, OP_iterator_check_object);
                emit_op(s, OP_await);
                label_next2 = emit_goto(s, OP_goto, -1);
                emit_label(s, label_next);
                emit_op(s, OP_drop);
                emit_label(s, label_next2);
                emit_op(s, OP_drop);
            } else {
                emit_op(s, OP_iterator_close);
            }
            drop_count = -3;
        }
        drop_count += top->drop_count;
        if (top->label_finally != -1) {
            while(drop_count) {
                /* must keep the stack top if hasval */
                emit_op(s, hasval ? OP_nip : OP_drop);
                drop_count--;
            }
            if (!hasval) {
                /* must push return value to keep same stack size */
                emit_op(s, OP_undefined);
                hasval = TRUE;
            }
            emit_goto(s, OP_gosub, top->label_finally);
        }
        top = top->prev;
    }
    if (s->cur_func->is_derived_class_constructor) {
        int label_return;

        /* 'this' can be uninitialized, so it may be accessed only if
           the derived class constructor does not return an object */
        if (hasval) {
            emit_op(s, OP_check_ctor_return);
            label_return = emit_goto(s, OP_if_false, -1);
            emit_op(s, OP_drop);
        } else {
            label_return = -1;
        }

        /* XXX: if this is not initialized, should throw the
           ReferenceError in the caller realm */
        emit_op(s, OP_scope_get_var);
        emit_atom(s, JS_ATOM_this);
        emit_u16(s, 0);

        emit_label(s, label_return);
        emit_op(s, OP_return);
    } else if (s->cur_func->func_kind != JS_FUNC_NORMAL) {
        if (!hasval) {
            emit_op(s, OP_undefined);
        } else if (s->cur_func->func_kind == JS_FUNC_ASYNC_GENERATOR) {
            emit_op(s, OP_await);
        }
        emit_op(s, OP_return_async);
    } else {
        emit_op(s, hasval ? OP_return : OP_return_undef);
    }
}



static __exception int js_parse_statement(JSParseState *s)
{
    return js_parse_statement_or_decl(s, 0);
}

static __exception int js_parse_block(JSParseState *s)
{
    if (js_parse_expect(s, '{'))
        return -1;
    if (s->token.val != '}') {
        push_scope(s);
        for(;;) {
            if (js_parse_statement_or_decl(s, DECL_MASK_ALL))
                return -1;
            if (s->token.val == '}')
                break;
        }
        pop_scope(s);
    }
    if (next_token(s))
        return -1;
    return 0;
}

/* allowed parse_flags: PF_IN_ACCEPTED */
__exception int js_parse_var(JSParseState *s, int parse_flags, int tok,
                             BOOL export_flag)
{
    JSContext *ctx = s->ctx;
    JSFunctionDef *fd = s->cur_func;
    JSAtom name = JS_ATOM_NULL;

    for (;;) {
        if (s->token.val == TOK_IDENT) {
            if (s->token.u.ident.is_reserved) {
                return js_parse_error_reserved_identifier(s);
            }
            name = JS_DupAtom(ctx, s->token.u.ident.atom);
            if (name == JS_ATOM_let && (tok == TOK_LET || tok == TOK_CONST)) {
                js_parse_error(s, "'let' is not a valid lexical identifier");
                goto var_error;
            }
            if (next_token(s))
                goto var_error;
            if (js_define_var(s, name, tok))
                goto var_error;
            if (export_flag) {
                if (!add_export_entry(s, s->cur_func->module, name, name,
                                      JS_EXPORT_TYPE_LOCAL))
                    goto var_error;
            }

            if (s->token.val == '=') {
                if (next_token(s))
                    goto var_error;
                if (tok == TOK_VAR) {
                    /* Must make a reference for proper `with` semantics */
                    int opcode, scope, label;
                    JSAtom name1;

                    emit_op(s, OP_scope_get_var);
                    emit_atom(s, name);
                    emit_u16(s, fd->scope_level);
                    if (get_lvalue(s, &opcode, &scope, &name1, &label, NULL, FALSE, '=') < 0)
                        goto var_error;
                    if (js_parse_assign_expr2(s, parse_flags)) {
                        JS_FreeAtom(ctx, name1);
                        goto var_error;
                    }
                    set_object_name(s, name);
                    put_lvalue(s, opcode, scope, name1, label,
                               PUT_LVALUE_NOKEEP, FALSE);
                } else {
                    if (js_parse_assign_expr2(s, parse_flags))
                        goto var_error;
                    set_object_name(s, name);
                    emit_op(s, (tok == TOK_CONST || tok == TOK_LET) ?
                               OP_scope_put_var_init : OP_scope_put_var);
                    emit_atom(s, name);
                    emit_u16(s, fd->scope_level);
                }
            } else {
                if (tok == TOK_CONST) {
                    js_parse_error(s, "missing initializer for const variable");
                    goto var_error;
                }
                if (tok == TOK_LET) {
                    /* initialize lexical variable upon entering its scope */
                    emit_op(s, OP_undefined);
                    emit_op(s, OP_scope_put_var_init);
                    emit_atom(s, name);
                    emit_u16(s, fd->scope_level);
                }
            }
            JS_FreeAtom(ctx, name);
        } else {
            int skip_bits;
            if ((s->token.val == '[' || s->token.val == '{')
                &&  js_parse_skip_parens_token(s, &skip_bits, FALSE) == '=') {
                emit_op(s, OP_undefined);
                if (js_parse_destructuring_element(s, tok, 0, TRUE, skip_bits & SKIP_HAS_ELLIPSIS, TRUE) < 0)
                    return -1;
            } else {
                return js_parse_error(s, "variable name expected");
            }
        }
        if (s->token.val != ',')
            break;
        if (next_token(s))
            return -1;
    }
    return 0;

    var_error:
    JS_FreeAtom(ctx, name);
    return -1;
}

/* test if the current token is a label. Use simplistic look-ahead scanner */
static BOOL is_label(JSParseState *s)
{
    return (s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved &&
            peek_token(s, FALSE) == ':');
}

/* test if the current token is a let keyword. Use simplistic look-ahead scanner */
static int is_let(JSParseState *s, int decl_mask)
{
    int res = FALSE;

    if (token_is_pseudo_keyword(s, JS_ATOM_let)) {
#if 1
        JSParsePos pos;
        js_parse_get_pos(s, &pos);
        for (;;) {
            if (next_token(s)) {
                res = -1;
                break;
            }
            if (s->token.val == '[') {
                /* let [ is a syntax restriction:
                   it never introduces an ExpressionStatement */
                res = TRUE;
                break;
            }
            if (s->token.val == '{' ||
                (s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved) ||
                s->token.val == TOK_LET ||
                s->token.val == TOK_YIELD ||
                s->token.val == TOK_AWAIT) {
                /* Check for possible ASI if not scanning for Declaration */
                /* XXX: should also check that `{` introduces a BindingPattern,
                   but Firefox does not and rejects eval("let=1;let\n{if(1)2;}") */
                if (s->last_line_num == s->token.line_num || (decl_mask & DECL_MASK_OTHER)) {
                    res = TRUE;
                    break;
                }
                break;
            }
            break;
        }
        if (js_parse_seek_token(s, &pos)) {
            res = -1;
        }
#else
        int tok = peek_token(s, TRUE);
        if (tok == '{' || tok == TOK_IDENT || peek_token(s, FALSE) == '[') {
            res = TRUE;
        }
#endif
    }
    return res;
}

/* XXX: handle IteratorClose when exiting the loop before the
   enumeration is done */
static __exception int js_parse_for_in_of(JSParseState *s, int label_name,
                                          BOOL is_async)
{
    JSContext *ctx = s->ctx;
    JSFunctionDef *fd = s->cur_func;
    JSAtom var_name;
    BOOL has_initializer, is_for_of, has_destructuring;
    int tok, tok1, opcode, scope, block_scope_level;
    int label_next, label_expr, label_cont, label_body, label_break;
    int pos_next, pos_expr;
    BlockEnv break_entry;

    has_initializer = FALSE;
    has_destructuring = FALSE;
    is_for_of = FALSE;
    block_scope_level = fd->scope_level;
    label_cont = new_label(s);
    label_body = new_label(s);
    label_break = new_label(s);
    label_next = new_label(s);

    /* create scope for the lexical variables declared in the enumeration
       expressions. XXX: Not completely correct because of weird capturing
       semantics in `for (i of o) a.push(function(){return i})` */
    push_scope(s);

    /* local for_in scope starts here so individual elements
       can be closed in statement. */
    push_break_entry(s->cur_func, &break_entry,
                     label_name, label_break, label_cont, 1);
    break_entry.scope_level = block_scope_level;

    label_expr = emit_goto(s, OP_goto, -1);

    pos_next = s->cur_func->byte_code.size;
    emit_label(s, label_next);

    tok = s->token.val;
    switch (is_let(s, DECL_MASK_OTHER)) {
        case TRUE:
            tok = TOK_LET;
            break;
        case FALSE:
            break;
        default:
            return -1;
    }
    if (tok == TOK_VAR || tok == TOK_LET || tok == TOK_CONST) {
        if (next_token(s))
            return -1;

        if (!(s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved)) {
            if (s->token.val == '[' || s->token.val == '{') {
                if (js_parse_destructuring_element(s, tok, 0, TRUE, -1, FALSE) < 0)
                    return -1;
                has_destructuring = TRUE;
            } else {
                return js_parse_error(s, "variable name expected");
            }
            var_name = JS_ATOM_NULL;
        } else {
            var_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            if (next_token(s)) {
                JS_FreeAtom(s->ctx, var_name);
                return -1;
            }
            if (js_define_var(s, var_name, tok)) {
                JS_FreeAtom(s->ctx, var_name);
                return -1;
            }
            emit_op(s, (tok == TOK_CONST || tok == TOK_LET) ?
                       OP_scope_put_var_init : OP_scope_put_var);
            emit_atom(s, var_name);
            emit_u16(s, fd->scope_level);
        }
    } else {
        int skip_bits;
        if ((s->token.val == '[' || s->token.val == '{')
            &&  ((tok1 = js_parse_skip_parens_token(s, &skip_bits, FALSE)) == TOK_IN || tok1 == TOK_OF)) {
            if (js_parse_destructuring_element(s, 0, 0, TRUE, skip_bits & SKIP_HAS_ELLIPSIS, TRUE) < 0)
                return -1;
        } else {
            int lvalue_label;
            if (js_parse_left_hand_side_expr(s))
                return -1;
            if (get_lvalue(s, &opcode, &scope, &var_name, &lvalue_label,
                           NULL, FALSE, TOK_FOR))
                return -1;
            put_lvalue(s, opcode, scope, var_name, lvalue_label,
                       PUT_LVALUE_NOKEEP_BOTTOM, FALSE);
        }
        var_name = JS_ATOM_NULL;
    }
    emit_goto(s, OP_goto, label_body);

    pos_expr = s->cur_func->byte_code.size;
    emit_label(s, label_expr);
    if (s->token.val == '=') {
        /* XXX: potential scoping issue if inside `with` statement */
        has_initializer = TRUE;
        /* parse and evaluate initializer prior to evaluating the
           object (only used with "for in" with a non lexical variable
           in non strict mode */
        if (next_token(s) || js_parse_assign_expr2(s, 0)) {
            JS_FreeAtom(ctx, var_name);
            return -1;
        }
        if (var_name != JS_ATOM_NULL) {
            emit_op(s, OP_scope_put_var);
            emit_atom(s, var_name);
            emit_u16(s, fd->scope_level);
        }
    }
    JS_FreeAtom(ctx, var_name);

    if (token_is_pseudo_keyword(s, JS_ATOM_of)) {
        break_entry.has_iterator = is_for_of = TRUE;
        break_entry.drop_count += 2;
        if (has_initializer)
            goto initializer_error;
    } else if (s->token.val == TOK_IN) {
        if (is_async)
            return js_parse_error(s, "'for await' loop should be used with 'of'");
        if (has_initializer &&
            (tok != TOK_VAR || (fd->js_mode & JS_MODE_STRICT) ||
             has_destructuring)) {
            initializer_error:
            return js_parse_error(s, "a declaration in the head of a for-%s loop can't have an initializer",
                                  is_for_of ? "of" : "in");
        }
    } else {
        return js_parse_error(s, "expected 'of' or 'in' in for control expression");
    }
    if (next_token(s))
        return -1;
    if (is_for_of) {
        if (js_parse_assign_expr(s))
            return -1;
    } else {
        if (js_parse_expr(s))
            return -1;
    }
    /* close the scope after having evaluated the expression so that
       the TDZ values are in the closures */
    close_scopes(s, s->cur_func->scope_level, block_scope_level);
    if (is_for_of) {
        if (is_async)
            emit_op(s, OP_for_await_of_start);
        else
            emit_op(s, OP_for_of_start);
        /* on stack: enum_rec */
    } else {
        emit_op(s, OP_for_in_start);
        /* on stack: enum_obj */
    }
    emit_goto(s, OP_goto, label_cont);

    if (js_parse_expect(s, ')'))
        return -1;

    if (OPTIMIZE) {
        /* move the `next` code here */
        DynBuf *bc = &s->cur_func->byte_code;
        int chunk_size = pos_expr - pos_next;
        int offset = bc->size - pos_next;
        int i;
        dbuf_realloc(bc, bc->size + chunk_size);
        dbuf_put(bc, bc->buf + pos_next, chunk_size);
        memset(bc->buf + pos_next, OP_nop, chunk_size);
        /* `next` part ends with a goto */
        s->cur_func->last_opcode_pos = bc->size - 5;
        /* relocate labels */
        for (i = label_cont; i < s->cur_func->label_count; i++) {
            LabelSlot *ls = &s->cur_func->label_slots[i];
            if (ls->pos >= pos_next && ls->pos < pos_expr)
                ls->pos += offset;
        }
    }

    emit_label(s, label_body);
    if (js_parse_statement(s))
        return -1;

    close_scopes(s, s->cur_func->scope_level, block_scope_level);

    emit_label(s, label_cont);
    if (is_for_of) {
        if (is_async) {
            /* call the next method */
            /* stack: iter_obj next catch_offset */
            emit_op(s, OP_dup3);
            emit_op(s, OP_drop);
            emit_op(s, OP_call_method);
            emit_u16(s, 0);
            /* get the result of the promise */
            emit_op(s, OP_await);
            /* unwrap the value and done values */
            emit_op(s, OP_iterator_get_value_done);
        } else {
            emit_op(s, OP_for_of_next);
            emit_u8(s, 0);
        }
    } else {
        emit_op(s, OP_for_in_next);
    }
    /* on stack: enum_rec / enum_obj value bool */
    emit_goto(s, OP_if_false, label_next);
    /* drop the undefined value from for_xx_next */
    emit_op(s, OP_drop);

    emit_label(s, label_break);
    if (is_for_of) {
        /* close and drop enum_rec */
        emit_op(s, OP_iterator_close);
    } else {
        emit_op(s, OP_drop);
    }
    pop_break_entry(s->cur_func);
    pop_scope(s);
    return 0;
}

static void set_eval_ret_undefined(JSParseState *s)
{
    if (s->cur_func->eval_ret_idx >= 0) {
        emit_op(s, OP_undefined);
        emit_op(s, OP_put_loc);
        emit_u16(s, s->cur_func->eval_ret_idx);
    }
}

__exception int js_parse_statement_or_decl(JSParseState *s,
                                           int decl_mask)
{
    JSContext *ctx = s->ctx;
    JSAtom label_name;
    int tok;

    /* specific label handling */
    /* XXX: support multiple labels on loop statements */
    label_name = JS_ATOM_NULL;
    if (is_label(s)) {
        BlockEnv *be;

        label_name = JS_DupAtom(ctx, s->token.u.ident.atom);

        for (be = s->cur_func->top_break; be; be = be->prev) {
            if (be->label_name == label_name) {
                js_parse_error(s, "duplicate label name");
                goto fail;
            }
        }

        if (next_token(s))
            goto fail;
        if (js_parse_expect(s, ':'))
            goto fail;
        if (s->token.val != TOK_FOR
            &&  s->token.val != TOK_DO
            &&  s->token.val != TOK_WHILE) {
            /* labelled regular statement */
            int label_break, mask;
            BlockEnv break_entry;

            label_break = new_label(s);
            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, -1, 0);
            if (!(s->cur_func->js_mode & JS_MODE_STRICT) &&
                (decl_mask & DECL_MASK_FUNC_WITH_LABEL)) {
                mask = DECL_MASK_FUNC | DECL_MASK_FUNC_WITH_LABEL;
            } else {
                mask = 0;
            }
            if (js_parse_statement_or_decl(s, mask))
                goto fail;
            emit_label(s, label_break);
            pop_break_entry(s->cur_func);
            goto done;
        }
    }

    switch(tok = s->token.val) {
        case '{':
            if (js_parse_block(s))
                goto fail;
            break;
        case TOK_RETURN:
            if (s->cur_func->is_eval) {
                js_parse_error(s, "return not in a function");
                goto fail;
            }
            if (next_token(s))
                goto fail;
            if (s->token.val != ';' && s->token.val != '}' && !s->got_lf) {
                if (js_parse_expr(s))
                    goto fail;
                emit_return(s, TRUE);
            } else {
                emit_return(s, FALSE);
            }
            if (js_parse_expect_semi(s))
                goto fail;
            break;
        case TOK_THROW:
            if (next_token(s))
                goto fail;
            if (s->got_lf) {
                js_parse_error(s, "line terminator not allowed after throw");
                goto fail;
            }
            if (js_parse_expr(s))
                goto fail;
            emit_op(s, OP_throw);
            if (js_parse_expect_semi(s))
                goto fail;
            break;
        case TOK_LET:
        case TOK_CONST:
        haslet:
            if (!(decl_mask & DECL_MASK_OTHER)) {
                js_parse_error(s, "lexical declarations can't appear in single-statement context");
                goto fail;
            }
            /* fall thru */
        case TOK_VAR:
            if (next_token(s))
                goto fail;
            if (js_parse_var(s, TRUE, tok, FALSE))
                goto fail;
            if (js_parse_expect_semi(s))
                goto fail;
            break;
        case TOK_IF:
        {
            int label1, label2, mask;
            if (next_token(s))
                goto fail;
            /* create a new scope for `let f;if(1) function f(){}` */
            push_scope(s);
            set_eval_ret_undefined(s);
            if (js_parse_expr_paren(s))
                goto fail;
            label1 = emit_goto(s, OP_if_false, -1);
            if (s->cur_func->js_mode & JS_MODE_STRICT)
                mask = 0;
            else
                mask = DECL_MASK_FUNC; /* Annex B.3.4 */

            if (js_parse_statement_or_decl(s, mask))
                goto fail;

            if (s->token.val == TOK_ELSE) {
                label2 = emit_goto(s, OP_goto, -1);
                if (next_token(s))
                    goto fail;

                emit_label(s, label1);
                if (js_parse_statement_or_decl(s, mask))
                    goto fail;

                label1 = label2;
            }
            emit_label(s, label1);
            pop_scope(s);
        }
            break;
        case TOK_WHILE:
        {
            int label_cont, label_break;
            BlockEnv break_entry;

            label_cont = new_label(s);
            label_break = new_label(s);

            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, label_cont, 0);

            if (next_token(s))
                goto fail;

            set_eval_ret_undefined(s);

            emit_label(s, label_cont);
            if (js_parse_expr_paren(s))
                goto fail;
            emit_goto(s, OP_if_false, label_break);

            if (js_parse_statement(s))
                goto fail;
            emit_goto(s, OP_goto, label_cont);

            emit_label(s, label_break);

            pop_break_entry(s->cur_func);
        }
            break;
        case TOK_DO:
        {
            int label_cont, label_break, label1;
            BlockEnv break_entry;

            label_cont = new_label(s);
            label_break = new_label(s);
            label1 = new_label(s);

            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, label_cont, 0);

            if (next_token(s))
                goto fail;

            emit_label(s, label1);

            set_eval_ret_undefined(s);

            if (js_parse_statement(s))
                goto fail;

            emit_label(s, label_cont);
            if (js_parse_expect(s, TOK_WHILE))
                goto fail;
            if (js_parse_expr_paren(s))
                goto fail;
            /* Insert semicolon if missing */
            if (s->token.val == ';') {
                if (next_token(s))
                    goto fail;
            }
            emit_goto(s, OP_if_true, label1);

            emit_label(s, label_break);

            pop_break_entry(s->cur_func);
        }
            break;
        case TOK_FOR:
        {
            int label_cont, label_break, label_body, label_test;
            int pos_cont, pos_body, block_scope_level;
            BlockEnv break_entry;
            int tok, bits;
            BOOL is_async;

            if (next_token(s))
                goto fail;

            set_eval_ret_undefined(s);
            bits = 0;
            is_async = FALSE;
            if (s->token.val == '(') {
                js_parse_skip_parens_token(s, &bits, FALSE);
            } else if (s->token.val == TOK_AWAIT) {
                if (!(s->cur_func->func_kind & JS_FUNC_ASYNC)) {
                    js_parse_error(s, "for await is only valid in asynchronous functions");
                    goto fail;
                }
                is_async = TRUE;
                if (next_token(s))
                    goto fail;
            }
            if (js_parse_expect(s, '('))
                goto fail;

            if (!(bits & SKIP_HAS_SEMI)) {
                /* parse for/in or for/of */
                if (js_parse_for_in_of(s, label_name, is_async))
                    goto fail;
                break;
            }
            block_scope_level = s->cur_func->scope_level;

            /* create scope for the lexical variables declared in the initial,
               test and increment expressions */
            push_scope(s);
            /* initial expression */
            tok = s->token.val;
            if (tok != ';') {
                switch (is_let(s, DECL_MASK_OTHER)) {
                    case TRUE:
                        tok = TOK_LET;
                        break;
                    case FALSE:
                        break;
                    default:
                        goto fail;
                }
                if (tok == TOK_VAR || tok == TOK_LET || tok == TOK_CONST) {
                    if (next_token(s))
                        goto fail;
                    if (js_parse_var(s, FALSE, tok, FALSE))
                        goto fail;
                } else {
                    if (js_parse_expr2(s, FALSE))
                        goto fail;
                    emit_op(s, OP_drop);
                }

                /* close the closures before the first iteration */
                close_scopes(s, s->cur_func->scope_level, block_scope_level);
            }
            if (js_parse_expect(s, ';'))
                goto fail;

            label_test = new_label(s);
            label_cont = new_label(s);
            label_body = new_label(s);
            label_break = new_label(s);

            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, label_cont, 0);

            /* test expression */
            if (s->token.val == ';') {
                /* no test expression */
                label_test = label_body;
            } else {
                emit_label(s, label_test);
                if (js_parse_expr(s))
                    goto fail;
                emit_goto(s, OP_if_false, label_break);
            }
            if (js_parse_expect(s, ';'))
                goto fail;

            if (s->token.val == ')') {
                /* no end expression */
                break_entry.label_cont = label_cont = label_test;
                pos_cont = 0; /* avoid warning */
            } else {
                /* skip the end expression */
                emit_goto(s, OP_goto, label_body);

                pos_cont = s->cur_func->byte_code.size;
                emit_label(s, label_cont);
                if (js_parse_expr(s))
                    goto fail;
                emit_op(s, OP_drop);
                if (label_test != label_body)
                    emit_goto(s, OP_goto, label_test);
            }
            if (js_parse_expect(s, ')'))
                goto fail;

            pos_body = s->cur_func->byte_code.size;
            emit_label(s, label_body);
            if (js_parse_statement(s))
                goto fail;

            /* close the closures before the next iteration */
            /* XXX: check continue case */
            close_scopes(s, s->cur_func->scope_level, block_scope_level);

            if (OPTIMIZE && label_test != label_body && label_cont != label_test) {
                /* move the increment code here */
                DynBuf *bc = &s->cur_func->byte_code;
                int chunk_size = pos_body - pos_cont;
                int offset = bc->size - pos_cont;
                int i;
                dbuf_realloc(bc, bc->size + chunk_size);
                dbuf_put(bc, bc->buf + pos_cont, chunk_size);
                memset(bc->buf + pos_cont, OP_nop, chunk_size);
                /* increment part ends with a goto */
                s->cur_func->last_opcode_pos = bc->size - 5;
                /* relocate labels */
                for (i = label_cont; i < s->cur_func->label_count; i++) {
                    LabelSlot *ls = &s->cur_func->label_slots[i];
                    if (ls->pos >= pos_cont && ls->pos < pos_body)
                        ls->pos += offset;
                }
            } else {
                emit_goto(s, OP_goto, label_cont);
            }

            emit_label(s, label_break);

            pop_break_entry(s->cur_func);
            pop_scope(s);
        }
            break;
        case TOK_BREAK:
        case TOK_CONTINUE:
        {
            int is_cont = s->token.val - TOK_BREAK;
            int label;

            if (next_token(s))
                goto fail;
            if (!s->got_lf && s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved)
                label = s->token.u.ident.atom;
            else
                label = JS_ATOM_NULL;
            if (emit_break(s, label, is_cont))
                goto fail;
            if (label != JS_ATOM_NULL) {
                if (next_token(s))
                    goto fail;
            }
            if (js_parse_expect_semi(s))
                goto fail;
        }
            break;
        case TOK_SWITCH:
        {
            int label_case, label_break, label1;
            int default_label_pos;
            BlockEnv break_entry;

            if (next_token(s))
                goto fail;

            set_eval_ret_undefined(s);
            if (js_parse_expr_paren(s))
                goto fail;

            push_scope(s);
            label_break = new_label(s);
            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, -1, 1);

            if (js_parse_expect(s, '{'))
                goto fail;

            default_label_pos = -1;
            label_case = -1;
            while (s->token.val != '}') {
                if (s->token.val == TOK_CASE) {
                    label1 = -1;
                    if (label_case >= 0) {
                        /* skip the case if needed */
                        label1 = emit_goto(s, OP_goto, -1);
                    }
                    emit_label(s, label_case);
                    label_case = -1;
                    for (;;) {
                        /* parse a sequence of case clauses */
                        if (next_token(s))
                            goto fail;
                        emit_op(s, OP_dup);
                        if (js_parse_expr(s))
                            goto fail;
                        if (js_parse_expect(s, ':'))
                            goto fail;
                        emit_op(s, OP_strict_eq);
                        if (s->token.val == TOK_CASE) {
                            label1 = emit_goto(s, OP_if_true, label1);
                        } else {
                            label_case = emit_goto(s, OP_if_false, -1);
                            emit_label(s, label1);
                            break;
                        }
                    }
                } else if (s->token.val == TOK_DEFAULT) {
                    if (next_token(s))
                        goto fail;
                    if (js_parse_expect(s, ':'))
                        goto fail;
                    if (default_label_pos >= 0) {
                        js_parse_error(s, "duplicate default");
                        goto fail;
                    }
                    if (label_case < 0) {
                        /* falling thru direct from switch expression */
                        label_case = emit_goto(s, OP_goto, -1);
                    }
                    /* Emit a dummy label opcode. Label will be patched after
                       the end of the switch body. Do not use emit_label(s, 0)
                       because it would clobber label 0 address, preventing
                       proper optimizer operation.
                     */
                    emit_op(s, OP_label);
                    emit_u32(s, 0);
                    default_label_pos = s->cur_func->byte_code.size - 4;
                } else {
                    if (label_case < 0) {
                        /* falling thru direct from switch expression */
                        js_parse_error(s, "invalid switch statement");
                        goto fail;
                    }
                    if (js_parse_statement_or_decl(s, DECL_MASK_ALL))
                        goto fail;
                }
            }
            if (js_parse_expect(s, '}'))
                goto fail;
            if (default_label_pos >= 0) {
                /* Ugly patch for the the `default` label, shameful and risky */
                put_u32(s->cur_func->byte_code.buf + default_label_pos,
                        label_case);
                s->cur_func->label_slots[label_case].pos = default_label_pos + 4;
            } else {
                emit_label(s, label_case);
            }
            emit_label(s, label_break);
            emit_op(s, OP_drop); /* drop the switch expression */

            pop_break_entry(s->cur_func);
            pop_scope(s);
        }
            break;
        case TOK_TRY:
        {
            int label_catch, label_catch2, label_finally, label_end;
            JSAtom name;
            BlockEnv block_env;

            set_eval_ret_undefined(s);
            if (next_token(s))
                goto fail;
            label_catch = new_label(s);
            label_catch2 = new_label(s);
            label_finally = new_label(s);
            label_end = new_label(s);

            emit_goto(s, OP_catch, label_catch);

            push_break_entry(s->cur_func, &block_env,
                             JS_ATOM_NULL, -1, -1, 1);
            block_env.label_finally = label_finally;

            if (js_parse_block(s))
                goto fail;

            pop_break_entry(s->cur_func);

            if (js_is_live_code(s)) {
                /* drop the catch offset */
                emit_op(s, OP_drop);
                /* must push dummy value to keep same stack size */
                emit_op(s, OP_undefined);
                emit_goto(s, OP_gosub, label_finally);
                emit_op(s, OP_drop);

                emit_goto(s, OP_goto, label_end);
            }

            if (s->token.val == TOK_CATCH) {
                if (next_token(s))
                    goto fail;

                push_scope(s);  /* catch variable */
                emit_label(s, label_catch);

                if (s->token.val == '{') {
                    /* support optional-catch-binding feature */
                    emit_op(s, OP_drop);    /* pop the exception object */
                } else {
                    if (js_parse_expect(s, '('))
                        goto fail;
                    if (!(s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved)) {
                        if (s->token.val == '[' || s->token.val == '{') {
                            /* XXX: TOK_LET is not completely correct */
                            if (js_parse_destructuring_element(s, TOK_LET, 0, TRUE, -1, TRUE) < 0)
                                goto fail;
                        } else {
                            js_parse_error(s, "identifier expected");
                            goto fail;
                        }
                    } else {
                        name = JS_DupAtom(ctx, s->token.u.ident.atom);
                        if (next_token(s)
                            ||  js_define_var(s, name, TOK_CATCH) < 0) {
                            JS_FreeAtom(ctx, name);
                            goto fail;
                        }
                        /* store the exception value in the catch variable */
                        emit_op(s, OP_scope_put_var);
                        emit_u32(s, name);
                        emit_u16(s, s->cur_func->scope_level);
                    }
                    if (js_parse_expect(s, ')'))
                        goto fail;
                }
                /* XXX: should keep the address to nop it out if there is no finally block */
                emit_goto(s, OP_catch, label_catch2);

                push_scope(s);  /* catch block */
                push_break_entry(s->cur_func, &block_env, JS_ATOM_NULL,
                                 -1, -1, 1);
                block_env.label_finally = label_finally;

                if (js_parse_block(s))
                    goto fail;

                pop_break_entry(s->cur_func);
                pop_scope(s);  /* catch block */
                pop_scope(s);  /* catch variable */

                if (js_is_live_code(s)) {
                    /* drop the catch2 offset */
                    emit_op(s, OP_drop);
                    /* XXX: should keep the address to nop it out if there is no finally block */
                    /* must push dummy value to keep same stack size */
                    emit_op(s, OP_undefined);
                    emit_goto(s, OP_gosub, label_finally);
                    emit_op(s, OP_drop);
                    emit_goto(s, OP_goto, label_end);
                }
                /* catch exceptions thrown in the catch block to execute the
                 * finally clause and rethrow the exception */
                emit_label(s, label_catch2);
                /* catch value is at TOS, no need to push undefined */
                emit_goto(s, OP_gosub, label_finally);
                emit_op(s, OP_throw);

            } else if (s->token.val == TOK_FINALLY) {
                /* finally without catch : execute the finally clause
                 * and rethrow the exception */
                emit_label(s, label_catch);
                /* catch value is at TOS, no need to push undefined */
                emit_goto(s, OP_gosub, label_finally);
                emit_op(s, OP_throw);
            } else {
                js_parse_error(s, "expecting catch or finally");
                goto fail;
            }
            emit_label(s, label_finally);
            if (s->token.val == TOK_FINALLY) {
                int saved_eval_ret_idx = 0; /* avoid warning */

                if (next_token(s))
                    goto fail;
                /* on the stack: ret_value gosub_ret_value */
                push_break_entry(s->cur_func, &block_env, JS_ATOM_NULL,
                                 -1, -1, 2);

                if (s->cur_func->eval_ret_idx >= 0) {
                    /* 'finally' updates eval_ret only if not a normal
                       termination */
                    saved_eval_ret_idx =
                            add_var(s->ctx, s->cur_func, JS_ATOM__ret_);
                    if (saved_eval_ret_idx < 0)
                        goto fail;
                    emit_op(s, OP_get_loc);
                    emit_u16(s, s->cur_func->eval_ret_idx);
                    emit_op(s, OP_put_loc);
                    emit_u16(s, saved_eval_ret_idx);
                    set_eval_ret_undefined(s);
                }

                if (js_parse_block(s))
                    goto fail;

                if (s->cur_func->eval_ret_idx >= 0) {
                    emit_op(s, OP_get_loc);
                    emit_u16(s, saved_eval_ret_idx);
                    emit_op(s, OP_put_loc);
                    emit_u16(s, s->cur_func->eval_ret_idx);
                }
                pop_break_entry(s->cur_func);
            }
            emit_op(s, OP_ret);
            emit_label(s, label_end);
        }
            break;
        case ';':
            /* empty statement */
            if (next_token(s))
                goto fail;
            break;
        case TOK_WITH:
            if (s->cur_func->js_mode & JS_MODE_STRICT) {
                js_parse_error(s, "invalid keyword: with");
                goto fail;
            } else {
                int with_idx;

                if (next_token(s))
                    goto fail;

                if (js_parse_expr_paren(s))
                    goto fail;

                push_scope(s);
                with_idx = define_var(s, s->cur_func, JS_ATOM__with_,
                                      JS_VAR_DEF_WITH);
                if (with_idx < 0)
                    goto fail;
                emit_op(s, OP_to_object);
                emit_op(s, OP_put_loc);
                emit_u16(s, with_idx);

                set_eval_ret_undefined(s);
                if (js_parse_statement(s))
                    goto fail;

                /* Popping scope drops lexical context for the with object variable */
                pop_scope(s);
            }
            break;
        case TOK_FUNCTION:
            /* ES6 Annex B.3.2 and B.3.3 semantics */
            if (!(decl_mask & DECL_MASK_FUNC))
                goto func_decl_error;
            if (!(decl_mask & DECL_MASK_OTHER) && peek_token(s, FALSE) == '*')
                goto func_decl_error;
            goto parse_func_var;
        case TOK_IDENT:
            if (s->token.u.ident.is_reserved) {
                js_parse_error_reserved_identifier(s);
                goto fail;
            }
            /* Determine if `let` introduces a Declaration or an ExpressionStatement */
            switch (is_let(s, decl_mask)) {
                case TRUE:
                    tok = TOK_LET;
                    goto haslet;
                case FALSE:
                    break;
                default:
                    goto fail;
            }
            if (token_is_pseudo_keyword(s, JS_ATOM_async) &&
                peek_token(s, TRUE) == TOK_FUNCTION) {
                if (!(decl_mask & DECL_MASK_OTHER)) {
                    func_decl_error:
                    js_parse_error(s, "function declarations can't appear in single-statement context");
                    goto fail;
                }
                parse_func_var:
                if (js_parse_function_decl(s, JS_PARSE_FUNC_VAR,
                                           JS_FUNC_NORMAL, JS_ATOM_NULL,
                                           s->token.ptr, s->token.line_num))
                    goto fail;
                break;
            }
            goto hasexpr;

        case TOK_CLASS:
            if (!(decl_mask & DECL_MASK_OTHER)) {
                js_parse_error(s, "class declarations can't appear in single-statement context");
                goto fail;
            }
            if (js_parse_class(s, FALSE, JS_PARSE_EXPORT_NONE))
                return -1;
            break;

        case TOK_DEBUGGER:
            /* currently no debugger, so just skip the keyword */
            if (next_token(s))
                goto fail;
            if (js_parse_expect_semi(s))
                goto fail;
            break;

        case TOK_ENUM:
        case TOK_EXPORT:
        case TOK_EXTENDS:
            js_unsupported_keyword(s, s->token.u.ident.atom);
            goto fail;

        default:
        hasexpr:
            if (js_parse_expr(s))
                goto fail;
            if (s->cur_func->eval_ret_idx >= 0) {
                /* store the expression value so that it can be returned
                   by eval() */
                emit_op(s, OP_put_loc);
                emit_u16(s, s->cur_func->eval_ret_idx);
            } else {
                emit_op(s, OP_drop); /* drop the result */
            }
            if (js_parse_expect_semi(s))
                goto fail;
            break;
    }
    done:
    JS_FreeAtom(ctx, label_name);
    return 0;
    fail:
    JS_FreeAtom(ctx, label_name);
    return -1;
}


static __exception int js_parse_directives(JSParseState *s)
{
    char str[20];
    JSParsePos pos;
    BOOL has_semi;

    if (s->token.val != TOK_STRING)
        return 0;

    js_parse_get_pos(s, &pos);

    while(s->token.val == TOK_STRING) {
        /* Copy actual source string representation */
        snprintf(str, sizeof str, "%.*s",
                 (int)(s->buf_ptr - s->token.ptr - 2), s->token.ptr + 1);

        if (next_token(s))
            return -1;

        has_semi = FALSE;
        switch (s->token.val) {
            case ';':
                if (next_token(s))
                    return -1;
                has_semi = TRUE;
                break;
            case '}':
            case TOK_EOF:
                has_semi = TRUE;
                break;
            case TOK_NUMBER:
            case TOK_STRING:
            case TOK_TEMPLATE:
            case TOK_IDENT:
            case TOK_REGEXP:
            case TOK_DEC:
            case TOK_INC:
            case TOK_NULL:
            case TOK_FALSE:
            case TOK_TRUE:
            case TOK_IF:
            case TOK_RETURN:
            case TOK_VAR:
            case TOK_THIS:
            case TOK_DELETE:
            case TOK_TYPEOF:
            case TOK_NEW:
            case TOK_DO:
            case TOK_WHILE:
            case TOK_FOR:
            case TOK_SWITCH:
            case TOK_THROW:
            case TOK_TRY:
            case TOK_FUNCTION:
            case TOK_DEBUGGER:
            case TOK_WITH:
            case TOK_CLASS:
            case TOK_CONST:
            case TOK_ENUM:
            case TOK_EXPORT:
            case TOK_IMPORT:
            case TOK_SUPER:
            case TOK_INTERFACE:
            case TOK_LET:
            case TOK_PACKAGE:
            case TOK_PRIVATE:
            case TOK_PROTECTED:
            case TOK_PUBLIC:
            case TOK_STATIC:
                /* automatic insertion of ';' */
                if (s->got_lf)
                    has_semi = TRUE;
                break;
            default:
                break;
        }
        if (!has_semi)
            break;
        if (!strcmp(str, "use strict")) {
            s->cur_func->has_use_strict = TRUE;
            s->cur_func->js_mode |= JS_MODE_STRICT;
        }
#if !defined(DUMP_BYTECODE) || !(DUMP_BYTECODE & 8)
        else if (!strcmp(str, "use strip")) {
            s->cur_func->js_mode |= JS_MODE_STRIP;
        }
#endif
#ifdef CONFIG_BIGNUM
        else if (s->ctx->bignum_ext && !strcmp(str, "use math")) {
            s->cur_func->js_mode |= JS_MODE_MATH;
        }
#endif
    }
    return js_parse_seek_token(s, &pos);
}

static int js_parse_function_check_names(JSParseState *s, JSFunctionDef *fd,
                                         JSAtom func_name)
{
    JSAtom name;
    int i, idx;

    if (fd->js_mode & JS_MODE_STRICT) {
        if (!fd->has_simple_parameter_list && fd->has_use_strict) {
            return js_parse_error(s, "\"use strict\" not allowed in function with default or destructuring parameter");
        }
        if (func_name == JS_ATOM_eval || func_name == JS_ATOM_arguments) {
            return js_parse_error(s, "invalid function name in strict code");
        }
        for (idx = 0; idx < fd->arg_count; idx++) {
            name = fd->args[idx].var_name;

            if (name == JS_ATOM_eval || name == JS_ATOM_arguments) {
                return js_parse_error(s, "invalid argument name in strict code");
            }
        }
    }
    /* check async_generator case */
    if ((fd->js_mode & JS_MODE_STRICT)
        ||  !fd->has_simple_parameter_list
        ||  (fd->func_type == JS_PARSE_FUNC_METHOD && fd->func_kind == JS_FUNC_ASYNC)
        ||  fd->func_type == JS_PARSE_FUNC_ARROW
        ||  fd->func_type == JS_PARSE_FUNC_METHOD) {
        for (idx = 0; idx < fd->arg_count; idx++) {
            name = fd->args[idx].var_name;
            if (name != JS_ATOM_NULL) {
                for (i = 0; i < idx; i++) {
                    if (fd->args[i].var_name == name)
                        goto duplicate;
                }
                /* Check if argument name duplicates a destructuring parameter */
                /* XXX: should have a flag for such variables */
                for (i = 0; i < fd->var_count; i++) {
                    if (fd->vars[i].var_name == name &&
                        fd->vars[i].scope_level == 0)
                        goto duplicate;
                }
            }
        }
    }
    return 0;

    duplicate:
    return js_parse_error(s, "duplicate argument names not allowed in this context");
}

/* create a function to initialize class fields */
JSFunctionDef *js_parse_function_class_fields_init(JSParseState *s)
{
    JSFunctionDef *fd;

    fd = js_new_function_def(s->ctx, s->cur_func, FALSE, FALSE,
                             s->filename, 0);
    if (!fd)
        return NULL;
    fd->func_name = JS_ATOM_NULL;
    fd->has_prototype = FALSE;
    fd->has_home_object = TRUE;

    fd->has_arguments_binding = FALSE;
    fd->has_this_binding = TRUE;
    fd->is_derived_class_constructor = FALSE;
    fd->new_target_allowed = TRUE;
    fd->super_call_allowed = FALSE;
    fd->super_allowed = fd->has_home_object;
    fd->arguments_allowed = FALSE;

    fd->func_kind = JS_FUNC_NORMAL;
    fd->func_type = JS_PARSE_FUNC_METHOD;
    return fd;
}

/* func_name must be JS_ATOM_NULL for JS_PARSE_FUNC_STATEMENT and
   JS_PARSE_FUNC_EXPR, JS_PARSE_FUNC_ARROW and JS_PARSE_FUNC_VAR */
__exception int js_parse_function_decl2(JSParseState *s,
                                        JSParseFunctionEnum func_type,
                                        JSFunctionKindEnum func_kind,
                                        JSAtom func_name,
                                        const uint8_t *ptr,
                                        int function_line_num,
                                        JSParseExportEnum export_flag,
                                        JSFunctionDef **pfd)
{
    JSContext *ctx = s->ctx;
    JSFunctionDef *fd = s->cur_func;
    BOOL is_expr;
    int func_idx, lexical_func_idx = -1;
    BOOL has_opt_arg;
    BOOL create_func_var = FALSE;

    is_expr = (func_type != JS_PARSE_FUNC_STATEMENT &&
               func_type != JS_PARSE_FUNC_VAR);

    if (func_type == JS_PARSE_FUNC_STATEMENT ||
        func_type == JS_PARSE_FUNC_VAR ||
        func_type == JS_PARSE_FUNC_EXPR) {
        if (func_kind == JS_FUNC_NORMAL &&
            token_is_pseudo_keyword(s, JS_ATOM_async) &&
            peek_token(s, TRUE) != '\n') {
            if (next_token(s))
                return -1;
            func_kind = JS_FUNC_ASYNC;
        }
        if (next_token(s))
            return -1;
        if (s->token.val == '*') {
            if (next_token(s))
                return -1;
            func_kind |= JS_FUNC_GENERATOR;
        }

        if (s->token.val == TOK_IDENT) {
            if (s->token.u.ident.is_reserved ||
                (s->token.u.ident.atom == JS_ATOM_yield &&
                 func_type == JS_PARSE_FUNC_EXPR &&
                 (func_kind & JS_FUNC_GENERATOR)) ||
                (s->token.u.ident.atom == JS_ATOM_await &&
                 func_type == JS_PARSE_FUNC_EXPR &&
                 (func_kind & JS_FUNC_ASYNC))) {
                return js_parse_error_reserved_identifier(s);
            }
        }
        if (s->token.val == TOK_IDENT ||
            (((s->token.val == TOK_YIELD && !(fd->js_mode & JS_MODE_STRICT)) ||
              (s->token.val == TOK_AWAIT && !s->is_module)) &&
             func_type == JS_PARSE_FUNC_EXPR)) {
            func_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            if (next_token(s)) {
                JS_FreeAtom(ctx, func_name);
                return -1;
            }
        } else {
            if (func_type != JS_PARSE_FUNC_EXPR &&
                export_flag != JS_PARSE_EXPORT_DEFAULT) {
                return js_parse_error(s, "function name expected");
            }
        }
    } else if (func_type != JS_PARSE_FUNC_ARROW) {
        func_name = JS_DupAtom(ctx, func_name);
    }

    if (fd->is_eval && fd->eval_type == JS_EVAL_TYPE_MODULE &&
        (func_type == JS_PARSE_FUNC_STATEMENT || func_type == JS_PARSE_FUNC_VAR)) {
        JSGlobalVar *hf;
        hf = find_global_var(fd, func_name);
        /* XXX: should check scope chain */
        if (hf && hf->scope_level == fd->scope_level) {
            js_parse_error(s, "invalid redefinition of global identifier in module code");
            JS_FreeAtom(ctx, func_name);
            return -1;
        }
    }

    if (func_type == JS_PARSE_FUNC_VAR) {
        if (!(fd->js_mode & JS_MODE_STRICT)
            && func_kind == JS_FUNC_NORMAL
            &&  find_lexical_decl(ctx, fd, func_name, fd->scope_first, FALSE) < 0
            &&  !((func_idx = find_var(ctx, fd, func_name)) >= 0 && (func_idx & ARGUMENT_VAR_OFFSET))
            &&  !(func_name == JS_ATOM_arguments && fd->has_arguments_binding)) {
            create_func_var = TRUE;
        }
        /* Create the lexical name here so that the function closure
           contains it */
        if (fd->is_eval &&
            (fd->eval_type == JS_EVAL_TYPE_GLOBAL ||
             fd->eval_type == JS_EVAL_TYPE_MODULE) &&
            fd->scope_level == fd->body_scope) {
            /* avoid creating a lexical variable in the global
               scope. XXX: check annex B */
            JSGlobalVar *hf;
            hf = find_global_var(fd, func_name);
            /* XXX: should check scope chain */
            if (hf && hf->scope_level == fd->scope_level) {
                js_parse_error(s, "invalid redefinition of global identifier");
                JS_FreeAtom(ctx, func_name);
                return -1;
            }
        } else {
            /* Always create a lexical name, fail if at the same scope as
               existing name */
            /* Lexical variable will be initialized upon entering scope */
            lexical_func_idx = define_var(s, fd, func_name,
                                          func_kind != JS_FUNC_NORMAL ?
                                          JS_VAR_DEF_NEW_FUNCTION_DECL :
                                          JS_VAR_DEF_FUNCTION_DECL);
            if (lexical_func_idx < 0) {
                JS_FreeAtom(ctx, func_name);
                return -1;
            }
        }
    }

    fd = js_new_function_def(ctx, fd, FALSE, is_expr,
                             s->filename, function_line_num);
    if (!fd) {
        JS_FreeAtom(ctx, func_name);
        return -1;
    }
    if (pfd)
        *pfd = fd;
    s->cur_func = fd;
    fd->func_name = func_name;
    /* XXX: test !fd->is_generator is always false */
    fd->has_prototype = (func_type == JS_PARSE_FUNC_STATEMENT ||
                         func_type == JS_PARSE_FUNC_VAR ||
                         func_type == JS_PARSE_FUNC_EXPR) &&
                        func_kind == JS_FUNC_NORMAL;
    fd->has_home_object = (func_type == JS_PARSE_FUNC_METHOD ||
                           func_type == JS_PARSE_FUNC_GETTER ||
                           func_type == JS_PARSE_FUNC_SETTER ||
                           func_type == JS_PARSE_FUNC_CLASS_CONSTRUCTOR ||
                           func_type == JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR);
    fd->has_arguments_binding = (func_type != JS_PARSE_FUNC_ARROW);
    fd->has_this_binding = fd->has_arguments_binding;
    fd->is_derived_class_constructor = (func_type == JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR);
    if (func_type == JS_PARSE_FUNC_ARROW) {
        fd->new_target_allowed = fd->parent->new_target_allowed;
        fd->super_call_allowed = fd->parent->super_call_allowed;
        fd->super_allowed = fd->parent->super_allowed;
        fd->arguments_allowed = fd->parent->arguments_allowed;
    } else {
        fd->new_target_allowed = TRUE;
        fd->super_call_allowed = fd->is_derived_class_constructor;
        fd->super_allowed = fd->has_home_object;
        fd->arguments_allowed = TRUE;
    }

    /* fd->in_function_body == FALSE prevents yield/await during the parsing
       of the arguments in generator/async functions. They are parsed as
       regular identifiers for other function kinds. */
    fd->func_kind = func_kind;
    fd->func_type = func_type;

    if (func_type == JS_PARSE_FUNC_CLASS_CONSTRUCTOR ||
        func_type == JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR) {
        /* error if not invoked as a constructor */
        emit_op(s, OP_check_ctor);
    }

    if (func_type == JS_PARSE_FUNC_CLASS_CONSTRUCTOR) {
        emit_class_field_init(s);
    }

    /* parse arguments */
    fd->has_simple_parameter_list = TRUE;
    fd->has_parameter_expressions = FALSE;
    has_opt_arg = FALSE;
    if (func_type == JS_PARSE_FUNC_ARROW && s->token.val == TOK_IDENT) {
        JSAtom name;
        if (s->token.u.ident.is_reserved) {
            js_parse_error_reserved_identifier(s);
            goto fail;
        }
        name = s->token.u.ident.atom;
        if (add_arg(ctx, fd, name) < 0)
            goto fail;
        fd->defined_arg_count = 1;
    } else {
        if (s->token.val == '(') {
            int skip_bits;
            /* if there is an '=' inside the parameter list, we
               consider there is a parameter expression inside */
            js_parse_skip_parens_token(s, &skip_bits, FALSE);
            if (skip_bits & SKIP_HAS_ASSIGNMENT)
                fd->has_parameter_expressions = TRUE;
            if (next_token(s))
                goto fail;
        } else {
            if (js_parse_expect(s, '('))
                goto fail;
        }

        if (fd->has_parameter_expressions) {
            fd->scope_level = -1; /* force no parent scope */
            if (push_scope(s) < 0)
                return -1;
        }

        while (s->token.val != ')') {
            JSAtom name;
            BOOL rest = FALSE;
            int idx, has_initializer;

            if (s->token.val == TOK_ELLIPSIS) {
                fd->has_simple_parameter_list = FALSE;
                rest = TRUE;
                if (next_token(s))
                    goto fail;
            }
            if (s->token.val == '[' || s->token.val == '{') {
                fd->has_simple_parameter_list = FALSE;
                if (rest) {
                    emit_op(s, OP_rest);
                    emit_u16(s, fd->arg_count);
                } else {
                    /* unnamed arg for destructuring */
                    idx = add_arg(ctx, fd, JS_ATOM_NULL);
                    emit_op(s, OP_get_arg);
                    emit_u16(s, idx);
                }
                has_initializer = js_parse_destructuring_element(s, fd->has_parameter_expressions ? TOK_LET : TOK_VAR, 1, TRUE, -1, TRUE);
                if (has_initializer < 0)
                    goto fail;
                if (has_initializer)
                    has_opt_arg = TRUE;
                if (!has_opt_arg)
                    fd->defined_arg_count++;
            } else if (s->token.val == TOK_IDENT) {
                if (s->token.u.ident.is_reserved) {
                    js_parse_error_reserved_identifier(s);
                    goto fail;
                }
                name = s->token.u.ident.atom;
                if (name == JS_ATOM_yield && fd->func_kind == JS_FUNC_GENERATOR) {
                    js_parse_error_reserved_identifier(s);
                    goto fail;
                }
                if (fd->has_parameter_expressions) {
                    if (define_var(s, fd, name, JS_VAR_DEF_LET) < 0)
                        goto fail;
                }
                /* XXX: could avoid allocating an argument if rest is true */
                idx = add_arg(ctx, fd, name);
                if (idx < 0)
                    goto fail;
                if (next_token(s))
                    goto fail;
                if (rest) {
                    emit_op(s, OP_rest);
                    emit_u16(s, idx);
                    if (fd->has_parameter_expressions) {
                        emit_op(s, OP_dup);
                        emit_op(s, OP_scope_put_var_init);
                        emit_atom(s, name);
                        emit_u16(s, fd->scope_level);
                    }
                    emit_op(s, OP_put_arg);
                    emit_u16(s, idx);
                    fd->has_simple_parameter_list = FALSE;
                    has_opt_arg = TRUE;
                } else if (s->token.val == '=') {
                    int label;

                    fd->has_simple_parameter_list = FALSE;
                    has_opt_arg = TRUE;

                    if (next_token(s))
                        goto fail;

                    label = new_label(s);
                    emit_op(s, OP_get_arg);
                    emit_u16(s, idx);
                    emit_op(s, OP_dup);
                    emit_op(s, OP_undefined);
                    emit_op(s, OP_strict_eq);
                    emit_goto(s, OP_if_false, label);
                    emit_op(s, OP_drop);
                    if (js_parse_assign_expr(s))
                        goto fail;
                    set_object_name(s, name);
                    emit_op(s, OP_dup);
                    emit_op(s, OP_put_arg);
                    emit_u16(s, idx);
                    emit_label(s, label);
                    emit_op(s, OP_scope_put_var_init);
                    emit_atom(s, name);
                    emit_u16(s, fd->scope_level);
                } else {
                    if (!has_opt_arg) {
                        fd->defined_arg_count++;
                    }
                    if (fd->has_parameter_expressions) {
                        /* copy the argument to the argument scope */
                        emit_op(s, OP_get_arg);
                        emit_u16(s, idx);
                        emit_op(s, OP_scope_put_var_init);
                        emit_atom(s, name);
                        emit_u16(s, fd->scope_level);
                    }
                }
            } else {
                js_parse_error(s, "missing formal parameter");
                goto fail;
            }
            if (rest && s->token.val != ')') {
                js_parse_expect(s, ')');
                goto fail;
            }
            if (s->token.val == ')')
                break;
            if (js_parse_expect(s, ','))
                goto fail;
        }
        if ((func_type == JS_PARSE_FUNC_GETTER && fd->arg_count != 0) ||
            (func_type == JS_PARSE_FUNC_SETTER && fd->arg_count != 1)) {
            js_parse_error(s, "invalid number of arguments for getter or setter");
            goto fail;
        }
    }

    if (fd->has_parameter_expressions) {
        int idx;

        /* Copy the variables in the argument scope to the variable
           scope (see FunctionDeclarationInstantiation() in spec). The
           normal arguments are already present, so no need to copy
           them. */
        idx = fd->scopes[fd->scope_level].first;
        while (idx >= 0) {
            JSVarDef *vd = &fd->vars[idx];
            if (vd->scope_level != fd->scope_level)
                break;
            if (find_var(ctx, fd, vd->var_name) < 0) {
                if (add_var(ctx, fd, vd->var_name) < 0)
                    goto fail;
                vd = &fd->vars[idx]; /* fd->vars may have been reallocated */
                emit_op(s, OP_scope_get_var);
                emit_atom(s, vd->var_name);
                emit_u16(s, fd->scope_level);
                emit_op(s, OP_scope_put_var);
                emit_atom(s, vd->var_name);
                emit_u16(s, 0);
            }
            idx = vd->scope_next;
        }

        /* the argument scope has no parent, hence we don't use pop_scope(s) */
        emit_op(s, OP_leave_scope);
        emit_u16(s, fd->scope_level);

        /* set the variable scope as the current scope */
        fd->scope_level = 0;
        fd->scope_first = fd->scopes[fd->scope_level].first;
    }

    if (next_token(s))
        goto fail;

    /* generator function: yield after the parameters are evaluated */
    if (func_kind == JS_FUNC_GENERATOR ||
        func_kind == JS_FUNC_ASYNC_GENERATOR)
        emit_op(s, OP_initial_yield);

    /* in generators, yield expression is forbidden during the parsing
       of the arguments */
    fd->in_function_body = TRUE;
    push_scope(s);  /* enter body scope */
    fd->body_scope = fd->scope_level;

    if (s->token.val == TOK_ARROW) {
        if (next_token(s))
            goto fail;

        if (s->token.val != '{') {
            if (js_parse_function_check_names(s, fd, func_name))
                goto fail;

            if (js_parse_assign_expr(s))
                goto fail;

            if (func_kind != JS_FUNC_NORMAL)
                emit_op(s, OP_return_async);
            else
                emit_op(s, OP_return);

            if (!(fd->js_mode & JS_MODE_STRIP)) {
                /* save the function source code */
                /* the end of the function source code is after the last
                   token of the function source stored into s->last_ptr */
                fd->source_len = s->last_ptr - ptr;
                fd->source = js_strndup(ctx, (const char *)ptr, fd->source_len);
                if (!fd->source)
                    goto fail;
            }
            goto done;
        }
    }

    if (js_parse_expect(s, '{'))
        goto fail;

    if (js_parse_directives(s))
        goto fail;

    /* in strict_mode, check function and argument names */
    if (js_parse_function_check_names(s, fd, func_name))
        goto fail;

    while (s->token.val != '}') {
        if (js_parse_source_element(s))
            goto fail;
    }
    if (!(fd->js_mode & JS_MODE_STRIP)) {
        /* save the function source code */
        fd->source_len = s->buf_ptr - ptr;
        fd->source = js_strndup(ctx, (const char *)ptr, fd->source_len);
        if (!fd->source)
            goto fail;
    }

    if (next_token(s)) {
        /* consume the '}' */
        goto fail;
    }

    /* in case there is no return, add one */
    if (js_is_live_code(s)) {
        emit_return(s, FALSE);
    }
    done:
    s->cur_func = fd->parent;

    /* create the function object */
    {
        int idx;
        JSAtom func_name = fd->func_name;

        /* the real object will be set at the end of the compilation */
        idx = cpool_add(s, JS_NULL);
        fd->parent_cpool_idx = idx;

        if (is_expr) {
            /* for constructors, no code needs to be generated here */
            if (func_type != JS_PARSE_FUNC_CLASS_CONSTRUCTOR &&
                func_type != JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR) {
                /* OP_fclosure creates the function object from the bytecode
                   and adds the scope information */
                emit_op(s, OP_fclosure);
                emit_u32(s, idx);
                if (func_name == JS_ATOM_NULL) {
                    emit_op(s, OP_set_name);
                    emit_u32(s, JS_ATOM_NULL);
                }
            }
        } else if (func_type == JS_PARSE_FUNC_VAR) {
            emit_op(s, OP_fclosure);
            emit_u32(s, idx);
            if (create_func_var) {
                if (s->cur_func->is_global_var) {
                    JSGlobalVar *hf;
                    /* the global variable must be defined at the start of the
                       function */
                    hf = add_global_var(ctx, s->cur_func, func_name);
                    if (!hf)
                        goto fail;
                    /* it is considered as defined at the top level
                       (needed for annex B.3.3.4 and B.3.3.5
                       checks) */
                    hf->scope_level = 0;
                    hf->force_init = ((s->cur_func->js_mode & JS_MODE_STRICT) != 0);
                    /* store directly into global var, bypass lexical scope */
                    emit_op(s, OP_dup);
                    emit_op(s, OP_scope_put_var);
                    emit_atom(s, func_name);
                    emit_u16(s, 0);
                } else {
                    /* do not call define_var to bypass lexical scope check */
                    func_idx = find_var(ctx, s->cur_func, func_name);
                    if (func_idx < 0) {
                        func_idx = add_var(ctx, s->cur_func, func_name);
                        if (func_idx < 0)
                            goto fail;
                    }
                    /* store directly into local var, bypass lexical catch scope */
                    emit_op(s, OP_dup);
                    emit_op(s, OP_scope_put_var);
                    emit_atom(s, func_name);
                    emit_u16(s, 0);
                }
            }
            if (lexical_func_idx >= 0) {
                /* lexical variable will be initialized upon entering scope */
                s->cur_func->vars[lexical_func_idx].func_pool_idx = idx;
                emit_op(s, OP_drop);
            } else {
                /* store function object into its lexical name */
                /* XXX: could use OP_put_loc directly */
                emit_op(s, OP_scope_put_var_init);
                emit_atom(s, func_name);
                emit_u16(s, s->cur_func->scope_level);
            }
        } else {
            if (!s->cur_func->is_global_var) {
                int var_idx = define_var(s, s->cur_func, func_name, JS_VAR_DEF_VAR);

                if (var_idx < 0)
                    goto fail;
                /* the variable will be assigned at the top of the function */
                if (var_idx & ARGUMENT_VAR_OFFSET) {
                    s->cur_func->args[var_idx - ARGUMENT_VAR_OFFSET].func_pool_idx = idx;
                } else {
                    s->cur_func->vars[var_idx].func_pool_idx = idx;
                }
            } else {
                JSAtom func_var_name;
                JSGlobalVar *hf;
                if (func_name == JS_ATOM_NULL)
                    func_var_name = JS_ATOM__default_; /* export default */
                else
                    func_var_name = func_name;
                /* the variable will be assigned at the top of the function */
                hf = add_global_var(ctx, s->cur_func, func_var_name);
                if (!hf)
                    goto fail;
                hf->cpool_idx = idx;
                if (export_flag != JS_PARSE_EXPORT_NONE) {
                    if (!add_export_entry(s, s->cur_func->module, func_var_name,
                                          export_flag == JS_PARSE_EXPORT_NAMED ? func_var_name : JS_ATOM_default, JS_EXPORT_TYPE_LOCAL))
                        goto fail;
                }
            }
        }
    }
    return 0;
    fail:
    s->cur_func = fd->parent;
    js_free_function_def(ctx, fd);
    if (pfd)
        *pfd = NULL;
    return -1;
}

__exception int js_parse_function_decl(JSParseState *s,
                                       JSParseFunctionEnum func_type,
                                       JSFunctionKindEnum func_kind,
                                       JSAtom func_name,
                                       const uint8_t *ptr,
                                       int function_line_num)
{
    return js_parse_function_decl2(s, func_type, func_kind, func_name, ptr,
                                   function_line_num, JS_PARSE_EXPORT_NONE,
                                   NULL);
}

 __exception int js_parse_program(JSParseState *s)
{
    JSFunctionDef *fd = s->cur_func;
    int idx;

    if (next_token(s))
        return -1;

    if (js_parse_directives(s))
        return -1;

    fd->is_global_var = (fd->eval_type == JS_EVAL_TYPE_GLOBAL) ||
                        (fd->eval_type == JS_EVAL_TYPE_MODULE) ||
                        !(fd->js_mode & JS_MODE_STRICT);

    if (!s->is_module) {
        /* hidden variable for the return value */
        fd->eval_ret_idx = idx = add_var(s->ctx, fd, JS_ATOM__ret_);
        if (idx < 0)
            return -1;
    }

    while (s->token.val != TOK_EOF) {
        if (js_parse_source_element(s))
            return -1;
    }

    if (!s->is_module) {
        /* return the value of the hidden variable eval_ret_idx  */
        emit_op(s, OP_get_loc);
        emit_u16(s, fd->eval_ret_idx);

        emit_op(s, OP_return);
    } else {
        emit_op(s, OP_return_undef);
    }

    return 0;
}


 JSFunctionDef *js_new_function_def(JSContext *ctx,
                                          JSFunctionDef *parent,
                                          BOOL is_eval,
                                          BOOL is_func_expr,
                                          const char *filename, int line_num)
{
    JSFunctionDef *fd;

    fd = js_mallocz(ctx, sizeof(*fd));
    if (!fd)
        return NULL;

    fd->ctx = ctx;
    init_list_head(&fd->child_list);

    /* insert in parent list */
    fd->parent = parent;
    fd->parent_cpool_idx = -1;
    if (parent) {
        list_add_tail(&fd->link, &parent->child_list);
        fd->js_mode = parent->js_mode;
        fd->parent_scope_level = parent->scope_level;
    }

    fd->is_eval = is_eval;
    fd->is_func_expr = is_func_expr;
    js_dbuf_init(ctx, &fd->byte_code);
    fd->last_opcode_pos = -1;
    fd->func_name = JS_ATOM_NULL;
    fd->var_object_idx = -1;
    fd->arg_var_object_idx = -1;
    fd->arguments_var_idx = -1;
    fd->arguments_arg_idx = -1;
    fd->func_var_idx = -1;
    fd->eval_ret_idx = -1;
    fd->this_var_idx = -1;
    fd->new_target_var_idx = -1;
    fd->this_active_func_var_idx = -1;
    fd->home_object_var_idx = -1;

    /* XXX: should distinguish arg, var and var object and body scopes */
    fd->scopes = fd->def_scope_array;
    fd->scope_size = countof(fd->def_scope_array);
    fd->scope_count = 1;
    fd->scopes[0].first = -1;
    fd->scopes[0].parent = -1;
    fd->scope_level = 0;  /* 0: var/arg scope */
    fd->scope_first = -1;
    fd->body_scope = -1;

    fd->filename = JS_NewAtom(ctx, filename);
    fd->line_num = line_num;

    js_dbuf_init(ctx, &fd->pc2line);
    //fd->pc2line_last_line_num = line_num;
    //fd->pc2line_last_pc = 0;
    fd->last_opcode_line_num = line_num;

    return fd;
}


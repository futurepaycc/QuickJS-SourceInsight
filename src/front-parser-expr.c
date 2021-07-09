//
// Created by benpeng.jiang on 2021/7/9.
//

#include "front-parser-expr.h"
#include "qjs-ir-emit.h"
#include "qjs-atom.h"

int push_scope(JSParseState *s) {
    if (s->cur_func) {
        JSFunctionDef *fd = s->cur_func;
        int scope = fd->scope_count;
        /* XXX: should check for scope overflow */
        if ((fd->scope_count + 1) > fd->scope_size) {
            int new_size;
            size_t slack;
            JSVarScope *new_buf;
            /* XXX: potential arithmetic overflow */
            new_size = max_int(fd->scope_count + 1, fd->scope_size * 3 / 2);
            if (fd->scopes == fd->def_scope_array) {
                new_buf = js_realloc2(s->ctx, NULL, new_size * sizeof(*fd->scopes), &slack);
                if (!new_buf)
                    return -1;
                memcpy(new_buf, fd->scopes, fd->scope_count * sizeof(*fd->scopes));
            } else {
                new_buf = js_realloc2(s->ctx, fd->scopes, new_size * sizeof(*fd->scopes), &slack);
                if (!new_buf)
                    return -1;
            }
            new_size += slack / sizeof(*new_buf);
            fd->scopes = new_buf;
            fd->scope_size = new_size;
        }
        fd->scope_count++;
        fd->scopes[scope].parent = fd->scope_level;
        fd->scopes[scope].first = fd->scope_first;
        emit_op(s, OP_enter_scope);
        emit_u16(s, scope);
        return fd->scope_level = scope;
    }
    return 0;
}

static int get_first_lexical_var(JSFunctionDef *fd, int scope) {
    while (scope >= 0) {
        int scope_idx = fd->scopes[scope].first;
        if (scope_idx >= 0)
            return scope_idx;
        scope = fd->scopes[scope].parent;
    }
    return -1;
}


void pop_scope(JSParseState *s) {
    if (s->cur_func) {
        /* disable scoped variables */
        JSFunctionDef *fd = s->cur_func;
        int scope = fd->scope_level;
        emit_op(s, OP_leave_scope);
        emit_u16(s, scope);
        fd->scope_level = fd->scopes[scope].parent;
        fd->scope_first = get_first_lexical_var(fd, fd->scope_level);
    }
}


/* if the property is an expression, name = JS_ATOM_NULL */
int __exception js_parse_property_name(JSParseState *s,
                                       JSAtom *pname,
                                       BOOL allow_method, BOOL allow_var,
                                       BOOL allow_private) {
    int is_private = 0;
    BOOL is_non_reserved_ident;
    JSAtom name;
    int prop_type;

    prop_type = PROP_TYPE_IDENT;
    if (allow_method) {
        if (token_is_pseudo_keyword(s, JS_ATOM_get)
            || token_is_pseudo_keyword(s, JS_ATOM_set)) {
            /* get x(), set x() */
            name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
            if (next_token(s))
                goto fail1;
            if (s->token.val == ':' || s->token.val == ',' ||
                s->token.val == '}' || s->token.val == '(') {
                is_non_reserved_ident = TRUE;
                goto ident_found;
            }
            prop_type = PROP_TYPE_GET + (name == JS_ATOM_set);
            JS_FreeAtom(s->ctx, name);
        } else if (s->token.val == '*') {
            if (next_token(s))
                goto fail;
            prop_type = PROP_TYPE_STAR;
        } else if (token_is_pseudo_keyword(s, JS_ATOM_async) &&
                   peek_token(s, TRUE) != '\n') {
            name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
            if (next_token(s))
                goto fail1;
            if (s->token.val == ':' || s->token.val == ',' ||
                s->token.val == '}' || s->token.val == '(') {
                is_non_reserved_ident = TRUE;
                goto ident_found;
            }
            JS_FreeAtom(s->ctx, name);
            if (s->token.val == '*') {
                if (next_token(s))
                    goto fail;
                prop_type = PROP_TYPE_ASYNC_STAR;
            } else {
                prop_type = PROP_TYPE_ASYNC;
            }
        }
    }

    if (token_is_ident(s->token.val)) {
        /* variable can only be a non-reserved identifier */
        is_non_reserved_ident =
                (s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved);
        /* keywords and reserved words have a valid atom */
        name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
        if (next_token(s))
            goto fail1;
        ident_found:
        if (is_non_reserved_ident &&
            prop_type == PROP_TYPE_IDENT && allow_var) {
            if (!(s->token.val == ':' ||
                  (s->token.val == '(' && allow_method))) {
                prop_type = PROP_TYPE_VAR;
            }
        }
    } else if (s->token.val == TOK_STRING) {
        name = JS_ValueToAtom(s->ctx, s->token.u.str.str);
        if (name == JS_ATOM_NULL)
            goto fail;
        if (next_token(s))
            goto fail1;
    } else if (s->token.val == TOK_NUMBER) {
        JSValue val;
        val = s->token.u.num.val;
#ifdef CONFIG_BIGNUM
        if (JS_VALUE_GET_TAG(val) == JS_TAG_BIG_FLOAT) {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            val = s->ctx->rt->bigfloat_ops.
                    mul_pow10_to_float64(s->ctx, &p->num,
                                         s->token.u.num.exponent);
            if (JS_IsException(val))
                goto fail;
            name = JS_ValueToAtom(s->ctx, val);
            JS_FreeValue(s->ctx, val);
        } else
#endif
        {
            name = JS_ValueToAtom(s->ctx, val);
        }
        if (name == JS_ATOM_NULL)
            goto fail;
        if (next_token(s))
            goto fail1;
    } else if (s->token.val == '[') {
        if (next_token(s))
            goto fail;
        if (js_parse_expr(s))
            goto fail;
        if (js_parse_expect(s, ']'))
            goto fail;
        name = JS_ATOM_NULL;
    } else if (s->token.val == TOK_PRIVATE_NAME && allow_private) {
        name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
        if (next_token(s))
            goto fail1;
        is_private = PROP_TYPE_PRIVATE;
    } else {
        goto invalid_prop;
    }
    if (prop_type != PROP_TYPE_IDENT && prop_type != PROP_TYPE_VAR &&
        s->token.val != '(') {
        JS_FreeAtom(s->ctx, name);
        invalid_prop:
        js_parse_error(s, "invalid property name");
        goto fail;
    }
    *pname = name;
    return prop_type | is_private;
    fail1:
    JS_FreeAtom(s->ctx, name);
    fail:
    *pname = JS_ATOM_NULL;
    return -1;
}


void set_object_name(JSParseState *s, JSAtom name) {
    JSFunctionDef *fd = s->cur_func;
    int opcode;

    opcode = get_prev_opcode(fd);
    if (opcode == OP_set_name) {
        /* XXX: should free atom after OP_set_name? */
        fd->byte_code.size = fd->last_opcode_pos;
        fd->last_opcode_pos = -1;
        emit_op(s, OP_set_name);
        emit_atom(s, name);
    } else if (opcode == OP_set_class_name) {
        int define_class_pos;
        JSAtom atom;
        define_class_pos = fd->last_opcode_pos + 1 -
                           get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
        assert(fd->byte_code.buf[define_class_pos] == OP_define_class);
        /* for consistency we free the previous atom which is
           JS_ATOM_empty_string */
        atom = get_u32(fd->byte_code.buf + define_class_pos + 1);
        JS_FreeAtom(s->ctx, atom);
        put_u32(fd->byte_code.buf + define_class_pos + 1,
                JS_DupAtom(s->ctx, name));
        fd->last_opcode_pos = -1;
    }
}

static void set_object_name_computed(JSParseState *s) {
    JSFunctionDef *fd = s->cur_func;
    int opcode;

    opcode = get_prev_opcode(fd);
    if (opcode == OP_set_name) {
        /* XXX: should free atom after OP_set_name? */
        fd->byte_code.size = fd->last_opcode_pos;
        fd->last_opcode_pos = -1;
        emit_op(s, OP_set_name_computed);
    } else if (opcode == OP_set_class_name) {
        int define_class_pos;
        define_class_pos = fd->last_opcode_pos + 1 -
                           get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
        assert(fd->byte_code.buf[define_class_pos] == OP_define_class);
        fd->byte_code.buf[define_class_pos] = OP_define_class_computed;
        fd->last_opcode_pos = -1;
    }
}


static __exception int js_parse_object_literal(JSParseState *s) {
    JSAtom name = JS_ATOM_NULL;
    const uint8_t *start_ptr;
    int start_line, prop_type;
    BOOL has_proto;

    if (next_token(s))
        goto fail;
    /* XXX: add an initial length that will be patched back */
    emit_op(s, OP_object);
    has_proto = FALSE;
    while (s->token.val != '}') {
        /* specific case for getter/setter */
        start_ptr = s->token.ptr;
        start_line = s->token.line_num;

        if (s->token.val == TOK_ELLIPSIS) {
            if (next_token(s))
                return -1;
            if (js_parse_assign_expr(s))
                return -1;
            emit_op(s, OP_null);  /* dummy excludeList */
            emit_op(s, OP_copy_data_properties);
            emit_u8(s, 2 | (1 << 2) | (0 << 5));
            emit_op(s, OP_drop); /* pop excludeList */
            emit_op(s, OP_drop); /* pop src object */
            goto next;
        }

        prop_type = js_parse_property_name(s, &name, TRUE, TRUE, FALSE);
        if (prop_type < 0)
            goto fail;

        if (prop_type == PROP_TYPE_VAR) {
            /* shortcut for x: x */
            emit_op(s, OP_scope_get_var);
            emit_atom(s, name);
            emit_u16(s, s->cur_func->scope_level);
            emit_op(s, OP_define_field);
            emit_atom(s, name);
        } else if (s->token.val == '(') {
            BOOL is_getset = (prop_type == PROP_TYPE_GET ||
                              prop_type == PROP_TYPE_SET);
            JSParseFunctionEnum func_type;
            JSFunctionKindEnum func_kind;
            int op_flags;

            func_kind = JS_FUNC_NORMAL;
            if (is_getset) {
                func_type = JS_PARSE_FUNC_GETTER + prop_type - PROP_TYPE_GET;
            } else {
                func_type = JS_PARSE_FUNC_METHOD;
                if (prop_type == PROP_TYPE_STAR)
                    func_kind = JS_FUNC_GENERATOR;
                else if (prop_type == PROP_TYPE_ASYNC)
                    func_kind = JS_FUNC_ASYNC;
                else if (prop_type == PROP_TYPE_ASYNC_STAR)
                    func_kind = JS_FUNC_ASYNC_GENERATOR;
            }
            if (js_parse_function_decl(s, func_type, func_kind, JS_ATOM_NULL,
                                       start_ptr, start_line))
                goto fail;
            if (name == JS_ATOM_NULL) {
                emit_op(s, OP_define_method_computed);
            } else {
                emit_op(s, OP_define_method);
                emit_atom(s, name);
            }
            if (is_getset) {
                op_flags = OP_DEFINE_METHOD_GETTER +
                           prop_type - PROP_TYPE_GET;
            } else {
                op_flags = OP_DEFINE_METHOD_METHOD;
            }
            emit_u8(s, op_flags | OP_DEFINE_METHOD_ENUMERABLE);
        } else {
            if (js_parse_expect(s, ':'))
                goto fail;
            if (js_parse_assign_expr(s))
                goto fail;
            if (name == JS_ATOM_NULL) {
                set_object_name_computed(s);
                emit_op(s, OP_define_array_el);
                emit_op(s, OP_drop);
            } else if (name == JS_ATOM___proto__) {
                if (has_proto) {
                    js_parse_error(s, "duplicate __proto__ property name");
                    goto fail;
                }
                emit_op(s, OP_set_proto);
                has_proto = TRUE;
            } else {
                set_object_name(s, name);
                emit_op(s, OP_define_field);
                emit_atom(s, name);
            }
        }
        JS_FreeAtom(s->ctx, name);
        next:
        name = JS_ATOM_NULL;
        if (s->token.val != ',')
            break;
        if (next_token(s))
            goto fail;
    }
    if (js_parse_expect(s, '}'))
        goto fail;
    return 0;
    fail:
    JS_FreeAtom(s->ctx, name);
    return -1;
}


/* add a private field variable in the current scope */
static int add_private_class_field(JSParseState *s, JSFunctionDef *fd,
                                   JSAtom name, JSVarKindEnum var_kind) {
    JSContext *ctx = s->ctx;
    JSVarDef *vd;
    int idx;

    idx = add_scope_var(ctx, fd, name, var_kind);
    if (idx < 0)
        return idx;
    vd = &fd->vars[idx];
    vd->is_lexical = 1;
    vd->is_const = 1;
    return idx;
}


__exception int js_parse_left_hand_side_expr(JSParseState *s) {
    return js_parse_postfix_expr(s, PF_POSTFIX_CALL);
}

/* XXX: could generate specific bytecode */
static __exception int js_parse_class_default_ctor(JSParseState *s,
                                                   BOOL has_super,
                                                   JSFunctionDef **pfd) {
    JSParsePos pos;
    const char *str;
    int ret, line_num;
    JSParseFunctionEnum func_type;
    const uint8_t *saved_buf_end;

    js_parse_get_pos(s, &pos);
    if (has_super) {
        /* spec change: no argument evaluation */
        str = "(){super(...arguments);}";
        func_type = JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR;
    } else {
        str = "(){}";
        func_type = JS_PARSE_FUNC_CLASS_CONSTRUCTOR;
    }
    line_num = s->token.line_num;
    saved_buf_end = s->buf_end;
    s->buf_ptr = (uint8_t *) str;
    s->buf_end = (uint8_t *) (str + strlen(str));
    ret = next_token(s);
    if (!ret) {
        ret = js_parse_function_decl2(s, func_type, JS_FUNC_NORMAL,
                                      JS_ATOM_NULL, (uint8_t *) str,
                                      line_num, JS_PARSE_EXPORT_NONE, pfd);
    }
    s->buf_end = saved_buf_end;
    ret |= js_parse_seek_token(s, &pos);
    return ret;
}

/* find field in the current scope */
static int find_private_class_field(JSContext *ctx, JSFunctionDef *fd,
                                    JSAtom name, int scope_level) {
    int idx;
    idx = fd->scopes[scope_level].first;
    while (idx != -1) {
        if (fd->vars[idx].scope_level != scope_level)
            break;
        if (fd->vars[idx].var_name == name)
            return idx;
        idx = fd->vars[idx].scope_next;
    }
    return -1;
}


/* build a private setter function name from the private getter name */
JSAtom get_private_setter_name(JSContext *ctx, JSAtom name) {
    return js_atom_concat_str(ctx, name, "<set>");
}

typedef struct {
    JSFunctionDef *fields_init_fd;
    int computed_fields_count;
    BOOL has_brand;
    int brand_push_pos;
} ClassFieldsDef;

static __exception int emit_class_init_start(JSParseState *s,
                                             ClassFieldsDef *cf) {
    int label_add_brand;

    cf->fields_init_fd = js_parse_function_class_fields_init(s);
    if (!cf->fields_init_fd)
        return -1;

    s->cur_func = cf->fields_init_fd;

    /* XXX: would be better to add the code only if needed, maybe in a
       later pass */
    emit_op(s, OP_push_false); /* will be patched later */
    cf->brand_push_pos = cf->fields_init_fd->last_opcode_pos;
    label_add_brand = emit_goto(s, OP_if_false, -1);

    emit_op(s, OP_scope_get_var);
    emit_atom(s, JS_ATOM_this);
    emit_u16(s, 0);

    emit_op(s, OP_scope_get_var);
    emit_atom(s, JS_ATOM_home_object);
    emit_u16(s, 0);

    emit_op(s, OP_add_brand);

    emit_label(s, label_add_brand);

    s->cur_func = s->cur_func->parent;
    return 0;
}

static __exception int add_brand(JSParseState *s, ClassFieldsDef *cf) {
    if (!cf->has_brand) {
        /* define the brand field in 'this' of the initializer */
        if (!cf->fields_init_fd) {
            if (emit_class_init_start(s, cf))
                return -1;
        }
        /* patch the start of the function to enable the OP_add_brand code */
        cf->fields_init_fd->byte_code.buf[cf->brand_push_pos] = OP_push_true;

        cf->has_brand = TRUE;
    }
    return 0;
}

static void emit_class_init_end(JSParseState *s, ClassFieldsDef *cf) {
    int cpool_idx;

    s->cur_func = cf->fields_init_fd;
    emit_op(s, OP_return_undef);
    s->cur_func = s->cur_func->parent;

    cpool_idx = cpool_add(s, JS_NULL);
    cf->fields_init_fd->parent_cpool_idx = cpool_idx;
    emit_op(s, OP_fclosure);
    emit_u32(s, cpool_idx);
    emit_op(s, OP_set_home_object);
}


__exception int js_parse_class(JSParseState *s, BOOL is_class_expr,
                               JSParseExportEnum export_flag) {
    JSContext *ctx = s->ctx;
    JSFunctionDef *fd = s->cur_func;
    JSAtom name = JS_ATOM_NULL, class_name = JS_ATOM_NULL, class_name1;
    JSAtom class_var_name = JS_ATOM_NULL;
    JSFunctionDef *method_fd, *ctor_fd;
    int saved_js_mode, class_name_var_idx, prop_type, ctor_cpool_offset;
    int class_flags = 0, i, define_class_offset;
    BOOL is_static, is_private;
    const uint8_t *class_start_ptr = s->token.ptr;
    const uint8_t *start_ptr;
    ClassFieldsDef class_fields[2];

    /* classes are parsed and executed in strict mode */
    saved_js_mode = fd->js_mode;
    fd->js_mode |= JS_MODE_STRICT;
    if (next_token(s))
        goto fail;
    if (s->token.val == TOK_IDENT) {
        if (s->token.u.ident.is_reserved) {
            js_parse_error_reserved_identifier(s);
            goto fail;
        }
        class_name = JS_DupAtom(ctx, s->token.u.ident.atom);
        if (next_token(s))
            goto fail;
    } else if (!is_class_expr && export_flag != JS_PARSE_EXPORT_DEFAULT) {
        js_parse_error(s, "class statement requires a name");
        goto fail;
    }
    if (!is_class_expr) {
        if (class_name == JS_ATOM_NULL)
            class_var_name = JS_ATOM__default_; /* export default */
        else
            class_var_name = class_name;
        class_var_name = JS_DupAtom(ctx, class_var_name);
    }

    push_scope(s);

    if (s->token.val == TOK_EXTENDS) {
        class_flags = JS_DEFINE_CLASS_HAS_HERITAGE;
        if (next_token(s))
            goto fail;
        if (js_parse_left_hand_side_expr(s))
            goto fail;
    } else {
        emit_op(s, OP_undefined);
    }

    /* add a 'const' definition for the class name */
    if (class_name != JS_ATOM_NULL) {
        class_name_var_idx = define_var(s, fd, class_name, JS_VAR_DEF_CONST);
        if (class_name_var_idx < 0)
            goto fail;
    }

    if (js_parse_expect(s, '{'))
        goto fail;

    /* this scope contains the private fields */
    push_scope(s);

    emit_op(s, OP_push_const);
    ctor_cpool_offset = fd->byte_code.size;
    emit_u32(s, 0); /* will be patched at the end of the class parsing */

    if (class_name == JS_ATOM_NULL) {
        if (class_var_name != JS_ATOM_NULL)
            class_name1 = JS_ATOM_default;
        else
            class_name1 = JS_ATOM_empty_string;
    } else {
        class_name1 = class_name;
    }

    emit_op(s, OP_define_class);
    emit_atom(s, class_name1);
    emit_u8(s, class_flags);
    define_class_offset = fd->last_opcode_pos;

    for (i = 0; i < 2; i++) {
        ClassFieldsDef *cf = &class_fields[i];
        cf->fields_init_fd = NULL;
        cf->computed_fields_count = 0;
        cf->has_brand = FALSE;
    }

    ctor_fd = NULL;
    while (s->token.val != '}') {
        if (s->token.val == ';') {
            if (next_token(s))
                goto fail;
            continue;
        }
        is_static = (s->token.val == TOK_STATIC);
        prop_type = -1;
        if (is_static) {
            if (next_token(s))
                goto fail;
            /* allow "static" field name */
            if (s->token.val == ';' || s->token.val == '=') {
                is_static = FALSE;
                name = JS_DupAtom(ctx, JS_ATOM_static);
                prop_type = PROP_TYPE_IDENT;
            }
        }
        if (is_static)
            emit_op(s, OP_swap);
        start_ptr = s->token.ptr;
        if (prop_type < 0) {
            prop_type = js_parse_property_name(s, &name, TRUE, FALSE, TRUE);
            if (prop_type < 0)
                goto fail;
        }
        is_private = prop_type & PROP_TYPE_PRIVATE;
        prop_type &= ~PROP_TYPE_PRIVATE;

        if ((name == JS_ATOM_constructor && !is_static &&
             prop_type != PROP_TYPE_IDENT) ||
            (name == JS_ATOM_prototype && is_static) ||
            name == JS_ATOM_hash_constructor) {
            js_parse_error(s, "invalid method name");
            goto fail;
        }
        if (prop_type == PROP_TYPE_GET || prop_type == PROP_TYPE_SET) {
            BOOL is_set = prop_type - PROP_TYPE_GET;
            JSFunctionDef *method_fd;

            if (is_private) {
                int idx, var_kind;
                idx = find_private_class_field(ctx, fd, name, fd->scope_level);
                if (idx >= 0) {
                    var_kind = fd->vars[idx].var_kind;
                    if (var_kind == JS_VAR_PRIVATE_FIELD ||
                        var_kind == JS_VAR_PRIVATE_METHOD ||
                        var_kind == JS_VAR_PRIVATE_GETTER_SETTER ||
                        var_kind == (JS_VAR_PRIVATE_GETTER + is_set)) {
                        goto private_field_already_defined;
                    }
                    fd->vars[idx].var_kind = JS_VAR_PRIVATE_GETTER_SETTER;
                } else {
                    if (add_private_class_field(s, fd, name,
                                                JS_VAR_PRIVATE_GETTER + is_set) < 0)
                        goto fail;
                }
                if (add_brand(s, &class_fields[is_static]) < 0)
                    goto fail;
            }

            if (js_parse_function_decl2(s, JS_PARSE_FUNC_GETTER + is_set,
                                        JS_FUNC_NORMAL, JS_ATOM_NULL,
                                        start_ptr, s->token.line_num,
                                        JS_PARSE_EXPORT_NONE, &method_fd))
                goto fail;
            if (is_private) {
                method_fd->need_home_object = TRUE; /* needed for brand check */
                emit_op(s, OP_set_home_object);
                /* XXX: missing function name */
                emit_op(s, OP_scope_put_var_init);
                if (is_set) {
                    JSAtom setter_name;
                    int ret;

                    setter_name = get_private_setter_name(ctx, name);
                    if (setter_name == JS_ATOM_NULL)
                        goto fail;
                    emit_atom(s, setter_name);
                    ret = add_private_class_field(s, fd, setter_name,
                                                  JS_VAR_PRIVATE_SETTER);
                    JS_FreeAtom(ctx, setter_name);
                    if (ret < 0)
                        goto fail;
                } else {
                    emit_atom(s, name);
                }
                emit_u16(s, s->cur_func->scope_level);
            } else {
                if (name == JS_ATOM_NULL) {
                    emit_op(s, OP_define_method_computed);
                } else {
                    emit_op(s, OP_define_method);
                    emit_atom(s, name);
                }
                emit_u8(s, OP_DEFINE_METHOD_GETTER + is_set);
            }
        } else if (prop_type == PROP_TYPE_IDENT && s->token.val != '(') {
            ClassFieldsDef *cf = &class_fields[is_static];
            JSAtom field_var_name = JS_ATOM_NULL;

            /* class field */

            /* XXX: spec: not consistent with method name checks */
            if (name == JS_ATOM_constructor || name == JS_ATOM_prototype) {
                js_parse_error(s, "invalid field name");
                goto fail;
            }

            if (is_private) {
                if (find_private_class_field(ctx, fd, name,
                                             fd->scope_level) >= 0) {
                    goto private_field_already_defined;
                }
                if (add_private_class_field(s, fd, name,
                                            JS_VAR_PRIVATE_FIELD) < 0)
                    goto fail;
                emit_op(s, OP_private_symbol);
                emit_atom(s, name);
                emit_op(s, OP_scope_put_var_init);
                emit_atom(s, name);
                emit_u16(s, s->cur_func->scope_level);
            }

            if (!cf->fields_init_fd) {
                if (emit_class_init_start(s, cf))
                    goto fail;
            }
            if (name == JS_ATOM_NULL) {
                /* save the computed field name into a variable */
                field_var_name = js_atom_concat_num(ctx, JS_ATOM_computed_field + is_static, cf->computed_fields_count);
                if (field_var_name == JS_ATOM_NULL)
                    goto fail;
                if (define_var(s, fd, field_var_name, JS_VAR_DEF_CONST) < 0) {
                    JS_FreeAtom(ctx, field_var_name);
                    goto fail;
                }
                emit_op(s, OP_to_propkey);
                emit_op(s, OP_scope_put_var_init);
                emit_atom(s, field_var_name);
                emit_u16(s, s->cur_func->scope_level);
            }
            s->cur_func = cf->fields_init_fd;
            emit_op(s, OP_scope_get_var);
            emit_atom(s, JS_ATOM_this);
            emit_u16(s, 0);

            if (name == JS_ATOM_NULL) {
                emit_op(s, OP_scope_get_var);
                emit_atom(s, field_var_name);
                emit_u16(s, s->cur_func->scope_level);
                cf->computed_fields_count++;
                JS_FreeAtom(ctx, field_var_name);
            } else if (is_private) {
                emit_op(s, OP_scope_get_var);
                emit_atom(s, name);
                emit_u16(s, s->cur_func->scope_level);
            }

            if (s->token.val == '=') {
                if (next_token(s))
                    goto fail;
                if (js_parse_assign_expr(s))
                    goto fail;
            } else {
                emit_op(s, OP_undefined);
            }
            if (is_private) {
                set_object_name_computed(s);
                emit_op(s, OP_define_private_field);
            } else if (name == JS_ATOM_NULL) {
                set_object_name_computed(s);
                emit_op(s, OP_define_array_el);
                emit_op(s, OP_drop);
            } else {
                set_object_name(s, name);
                emit_op(s, OP_define_field);
                emit_atom(s, name);
            }
            s->cur_func = s->cur_func->parent;
            if (js_parse_expect_semi(s))
                goto fail;
        } else {
            JSParseFunctionEnum func_type;
            JSFunctionKindEnum func_kind;

            func_type = JS_PARSE_FUNC_METHOD;
            func_kind = JS_FUNC_NORMAL;
            if (prop_type == PROP_TYPE_STAR) {
                func_kind = JS_FUNC_GENERATOR;
            } else if (prop_type == PROP_TYPE_ASYNC) {
                func_kind = JS_FUNC_ASYNC;
            } else if (prop_type == PROP_TYPE_ASYNC_STAR) {
                func_kind = JS_FUNC_ASYNC_GENERATOR;
            } else if (name == JS_ATOM_constructor && !is_static) {
                if (ctor_fd) {
                    js_parse_error(s, "property constructor appears more than once");
                    goto fail;
                }
                if (class_flags & JS_DEFINE_CLASS_HAS_HERITAGE)
                    func_type = JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR;
                else
                    func_type = JS_PARSE_FUNC_CLASS_CONSTRUCTOR;
            }
            if (is_private) {
                if (add_brand(s, &class_fields[is_static]) < 0)
                    goto fail;
            }
            if (js_parse_function_decl2(s, func_type, func_kind, JS_ATOM_NULL, start_ptr, s->token.line_num,
                                        JS_PARSE_EXPORT_NONE, &method_fd))
                goto fail;
            if (func_type == JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR ||
                func_type == JS_PARSE_FUNC_CLASS_CONSTRUCTOR) {
                ctor_fd = method_fd;
            } else if (is_private) {
                method_fd->need_home_object = TRUE; /* needed for brand check */
                if (find_private_class_field(ctx, fd, name,
                                             fd->scope_level) >= 0) {
                    private_field_already_defined:
                    js_parse_error(s, "private class field is already defined");
                    goto fail;
                }
                if (add_private_class_field(s, fd, name,
                                            JS_VAR_PRIVATE_METHOD) < 0)
                    goto fail;
                emit_op(s, OP_set_home_object);
                emit_op(s, OP_set_name);
                emit_atom(s, name);
                emit_op(s, OP_scope_put_var_init);
                emit_atom(s, name);
                emit_u16(s, s->cur_func->scope_level);
            } else {
                if (name == JS_ATOM_NULL) {
                    emit_op(s, OP_define_method_computed);
                } else {
                    emit_op(s, OP_define_method);
                    emit_atom(s, name);
                }
                emit_u8(s, OP_DEFINE_METHOD_METHOD);
            }
        }
        if (is_static)
            emit_op(s, OP_swap);
        JS_FreeAtom(ctx, name);
        name = JS_ATOM_NULL;
    }

    if (s->token.val != '}') {
        js_parse_error(s, "expecting '%c'", '}');
        goto fail;
    }

    if (!ctor_fd) {
        if (js_parse_class_default_ctor(s, class_flags & JS_DEFINE_CLASS_HAS_HERITAGE, &ctor_fd))
            goto fail;
    }
    /* patch the constant pool index for the constructor */
    put_u32(fd->byte_code.buf + ctor_cpool_offset, ctor_fd->parent_cpool_idx);

    /* store the class source code in the constructor. */
    if (!(fd->js_mode & JS_MODE_STRIP)) {
        js_free(ctx, ctor_fd->source);
        ctor_fd->source_len = s->buf_ptr - class_start_ptr;
        ctor_fd->source = js_strndup(ctx, (const char *) class_start_ptr,
                                     ctor_fd->source_len);
        if (!ctor_fd->source)
            goto fail;
    }

    /* consume the '}' */
    if (next_token(s))
        goto fail;

    /* store the function to initialize the fields to that it can be
       referenced by the constructor */
    {
        ClassFieldsDef *cf = &class_fields[0];
        int var_idx;

        var_idx = define_var(s, fd, JS_ATOM_class_fields_init,
                             JS_VAR_DEF_CONST);
        if (var_idx < 0)
            goto fail;
        if (cf->fields_init_fd) {
            emit_class_init_end(s, cf);
        } else {
            emit_op(s, OP_undefined);
        }
        emit_op(s, OP_scope_put_var_init);
        emit_atom(s, JS_ATOM_class_fields_init);
        emit_u16(s, s->cur_func->scope_level);
    }

    /* drop the prototype */
    emit_op(s, OP_drop);

    /* initialize the static fields */
    if (class_fields[1].fields_init_fd != NULL) {
        ClassFieldsDef *cf = &class_fields[1];
        emit_op(s, OP_dup);
        emit_class_init_end(s, cf);
        emit_op(s, OP_call_method);
        emit_u16(s, 0);
        emit_op(s, OP_drop);
    }

    if (class_name != JS_ATOM_NULL) {
        /* store the class name in the scoped class name variable (it
           is independent from the class statement variable
           definition) */
        emit_op(s, OP_dup);
        emit_op(s, OP_scope_put_var_init);
        emit_atom(s, class_name);
        emit_u16(s, fd->scope_level);
    }
    pop_scope(s);
    pop_scope(s);

    /* the class statements have a block level scope */
    if (class_var_name != JS_ATOM_NULL) {
        if (define_var(s, fd, class_var_name, JS_VAR_DEF_LET) < 0)
            goto fail;
        emit_op(s, OP_scope_put_var_init);
        emit_atom(s, class_var_name);
        emit_u16(s, fd->scope_level);
    } else {
        if (class_name == JS_ATOM_NULL) {
            /* cannot use OP_set_name because the name of the class
               must be defined before the static initializers are
               executed */
            emit_op(s, OP_set_class_name);
            emit_u32(s, fd->last_opcode_pos + 1 - define_class_offset);
        }
    }

    if (export_flag != JS_PARSE_EXPORT_NONE) {
        if (!add_export_entry(s, fd->module,
                              class_var_name,
                              export_flag == JS_PARSE_EXPORT_NAMED ? class_var_name : JS_ATOM_default,
                              JS_EXPORT_TYPE_LOCAL))
            goto fail;
    }

    JS_FreeAtom(ctx, class_name);
    JS_FreeAtom(ctx, class_var_name);
    fd->js_mode = saved_js_mode;
    return 0;
    fail:
    JS_FreeAtom(ctx, name);
    JS_FreeAtom(ctx, class_name);
    JS_FreeAtom(ctx, class_var_name);
    fd->js_mode = saved_js_mode;
    return -1;
}

static __exception int js_parse_array_literal(JSParseState *s) {
    uint32_t idx;
    BOOL need_length;

    if (next_token(s))
        return -1;
    /* small regular arrays are created on the stack */
    idx = 0;
    while (s->token.val != ']' && idx < 32) {
        if (s->token.val == ',' || s->token.val == TOK_ELLIPSIS)
            break;
        if (js_parse_assign_expr(s))
            return -1;
        idx++;
        /* accept trailing comma */
        if (s->token.val == ',') {
            if (next_token(s))
                return -1;
        } else if (s->token.val != ']')
            goto done;
    }
    emit_op(s, OP_array_from);
    emit_u16(s, idx);

    /* larger arrays and holes are handled with explicit indices */
    need_length = FALSE;
    while (s->token.val != ']' && idx < 0x7fffffff) {
        if (s->token.val == TOK_ELLIPSIS)
            break;
        need_length = TRUE;
        if (s->token.val != ',') {
            if (js_parse_assign_expr(s))
                return -1;
            emit_op(s, OP_define_field);
            emit_u32(s, __JS_AtomFromUInt32(idx));
            need_length = FALSE;
        }
        idx++;
        /* accept trailing comma */
        if (s->token.val == ',') {
            if (next_token(s))
                return -1;
        }
    }
    if (s->token.val == ']') {
        if (need_length) {
            /* Set the length: Cannot use OP_define_field because
               length is not configurable */
            emit_op(s, OP_dup);
            emit_op(s, OP_push_i32);
            emit_u32(s, idx);
            emit_op(s, OP_put_field);
            emit_atom(s, JS_ATOM_length);
        }
        goto done;
    }

    /* huge arrays and spread elements require a dynamic index on the stack */
    emit_op(s, OP_push_i32);
    emit_u32(s, idx);

    /* stack has array, index */
    while (s->token.val != ']') {
        if (s->token.val == TOK_ELLIPSIS) {
            if (next_token(s))
                return -1;
            if (js_parse_assign_expr(s))
                return -1;
#if 1
            emit_op(s, OP_append);
#else
            int label_next, label_done;
            label_next = new_label(s);
            label_done = new_label(s);
            /* enumerate object */
            emit_op(s, OP_for_of_start);
            emit_op(s, OP_rot5l);
            emit_op(s, OP_rot5l);
            emit_label(s, label_next);
            /* on stack: enum_rec array idx */
            emit_op(s, OP_for_of_next);
            emit_u8(s, 2);
            emit_goto(s, OP_if_true, label_done);
            /* append element */
            /* enum_rec array idx val -> enum_rec array new_idx */
            emit_op(s, OP_define_array_el);
            emit_op(s, OP_inc);
            emit_goto(s, OP_goto, label_next);
            emit_label(s, label_done);
            /* close enumeration */
            emit_op(s, OP_drop); /* drop undef val */
            emit_op(s, OP_nip1); /* drop enum_rec */
            emit_op(s, OP_nip1);
            emit_op(s, OP_nip1);
#endif
        } else {
            need_length = TRUE;
            if (s->token.val != ',') {
                if (js_parse_assign_expr(s))
                    return -1;
                /* a idx val */
                emit_op(s, OP_define_array_el);
                need_length = FALSE;
            }
            emit_op(s, OP_inc);
        }
        if (s->token.val != ',')
            break;
        if (next_token(s))
            return -1;
    }
    if (need_length) {
        /* Set the length: cannot use OP_define_field because
           length is not configurable */
        emit_op(s, OP_dup1);    /* array length - array array length */
        emit_op(s, OP_put_field);
        emit_atom(s, JS_ATOM_length);
    } else {
        emit_op(s, OP_drop);    /* array length - array */
    }
    done:
    return js_parse_expect(s, ']');
}

/* XXX: remove */
static BOOL has_with_scope(JSFunctionDef *s, int scope_level) {
    /* check if scope chain contains a with statement */
    while (s) {
        int scope_idx = s->scopes[scope_level].first;
        while (scope_idx >= 0) {
            JSVarDef *vd = &s->vars[scope_idx];

            if (vd->var_name == JS_ATOM__with_)
                return TRUE;
            scope_idx = vd->scope_next;
        }
        /* check parent scopes */
        scope_level = s->parent_scope_level;
        s = s->parent;
    }
    return FALSE;
}


inline BOOL token_is_pseudo_keyword(JSParseState *s, JSAtom atom) {
    return s->token.val == TOK_IDENT && s->token.u.ident.atom == atom &&
           !s->token.u.ident.has_escape;
}


int js_parse_get_pos(JSParseState *s, JSParsePos *sp) {
    sp->last_line_num = s->last_line_num;
    sp->line_num = s->token.line_num;
    sp->ptr = s->token.ptr;
    sp->got_lf = s->got_lf;
    return 0;
}

__exception int js_parse_seek_token(JSParseState *s, const JSParsePos *sp) {
    s->token.line_num = sp->last_line_num;
    s->line_num = sp->line_num;
    s->buf_ptr = sp->ptr;
    s->got_lf = sp->got_lf;
    return next_token(s);
}

/* return TRUE if a regexp literal is allowed after this token */
static BOOL is_regexp_allowed(int tok) {
    switch (tok) {
        case TOK_NUMBER:
        case TOK_STRING:
        case TOK_REGEXP:
        case TOK_DEC:
        case TOK_INC:
        case TOK_NULL:
        case TOK_FALSE:
        case TOK_TRUE:
        case TOK_THIS:
        case ')':
        case ']':
        case '}': /* XXX: regexp may occur after */
        case TOK_IDENT:
            return FALSE;
        default:
            return TRUE;
    }
}

/* XXX: improve speed with early bailout */
/* XXX: no longer works if regexps are present. Could use previous
   regexp parsing heuristics to handle most cases */
int js_parse_skip_parens_token(JSParseState *s, int *pbits, BOOL no_line_terminator) {
    char state[256];
    size_t level = 0;
    JSParsePos pos;
    int last_tok, tok = TOK_EOF;
    int c, tok_len, bits = 0;

    /* protect from underflow */
    state[level++] = 0;

    js_parse_get_pos(s, &pos);
    last_tok = 0;
    for (;;) {
        switch (s->token.val) {
            case '(':
            case '[':
            case '{':
                if (level >= sizeof(state))
                    goto done;
                state[level++] = s->token.val;
                break;
            case ')':
                if (state[--level] != '(')
                    goto done;
                break;
            case ']':
                if (state[--level] != '[')
                    goto done;
                break;
            case '}':
                c = state[--level];
                if (c == '`') {
                    /* continue the parsing of the template */
                    free_token(s, &s->token);
                    /* Resume TOK_TEMPLATE parsing (s->token.line_num and
                     * s->token.ptr are OK) */
                    s->got_lf = FALSE;
                    s->last_line_num = s->token.line_num;
                    if (js_parse_template_part(s, s->buf_ptr))
                        goto done;
                    goto handle_template;
                } else if (c != '{') {
                    goto done;
                }
                break;
            case TOK_TEMPLATE:
            handle_template:
                if (s->token.u.str.sep != '`') {
                    /* '${' inside the template : closing '}' and continue
                       parsing the template */
                    if (level >= sizeof(state))
                        goto done;
                    state[level++] = '`';
                }
                break;
            case TOK_EOF:
                goto done;
            case ';':
                if (level == 2) {
                    bits |= SKIP_HAS_SEMI;
                }
                break;
            case TOK_ELLIPSIS:
                if (level == 2) {
                    bits |= SKIP_HAS_ELLIPSIS;
                }
                break;
            case '=':
                bits |= SKIP_HAS_ASSIGNMENT;
                break;

            case TOK_DIV_ASSIGN:
                tok_len = 2;
                goto parse_regexp;
            case '/':
                tok_len = 1;
            parse_regexp:
                if (is_regexp_allowed(last_tok)) {
                    s->buf_ptr -= tok_len;
                    if (js_parse_regexp(s)) {
                        /* XXX: should clear the exception */
                        goto done;
                    }
                }
                break;
        }
        /* last_tok is only used to recognize regexps */
        if (s->token.val == TOK_IDENT &&
            (token_is_pseudo_keyword(s, JS_ATOM_of) ||
             token_is_pseudo_keyword(s, JS_ATOM_yield))) {
            last_tok = TOK_OF;
        } else {
            last_tok = s->token.val;
        }
        if (next_token(s)) {
            /* XXX: should clear the exception generated by next_token() */
            break;
        }
        if (level <= 1) {
            tok = s->token.val;
            if (token_is_pseudo_keyword(s, JS_ATOM_of))
                tok = TOK_OF;
            if (no_line_terminator && s->last_line_num != s->token.line_num)
                tok = '\n';
            break;
        }
    }
    done:
    if (pbits) {
        *pbits = bits;
    }
    if (js_parse_seek_token(s, &pos))
        return -1;
    return tok;
}


typedef enum FuncCallType {
    FUNC_CALL_NORMAL,
    FUNC_CALL_NEW,
    FUNC_CALL_SUPER_CTOR,
    FUNC_CALL_TEMPLATE,
} FuncCallType;


static void optional_chain_test(JSParseState *s, int *poptional_chaining_label,
                                int drop_count) {
    int label_next, i;
    if (*poptional_chaining_label < 0)
        *poptional_chaining_label = new_label(s);
    /* XXX: could be more efficient with a specific opcode */
    emit_op(s, OP_dup);
    emit_op(s, OP_is_undefined_or_null);
    label_next = emit_goto(s, OP_if_false, -1);
    for (i = 0; i < drop_count; i++)
        emit_op(s, OP_drop);
    emit_op(s, OP_undefined);
    emit_goto(s, OP_goto, *poptional_chaining_label);
    emit_label(s, label_next);
}

/* allowed parse_flags: PF_POSTFIX_CALL, PF_ARROW_FUNC */
__exception int js_parse_postfix_expr(JSParseState *s, int parse_flags) {
    FuncCallType call_type;
    int optional_chaining_label;
    BOOL accept_lparen = (parse_flags & PF_POSTFIX_CALL) != 0;

    call_type = FUNC_CALL_NORMAL;
    switch (s->token.val) {
        case TOK_NUMBER: {
            JSValue val;
            val = s->token.u.num.val;

            if (JS_VALUE_GET_TAG(val) == JS_TAG_INT) {
                emit_op(s, OP_push_i32);
                emit_u32(s, JS_VALUE_GET_INT(val));
            } else
#ifdef CONFIG_BIGNUM
            if (JS_VALUE_GET_TAG(val) == JS_TAG_BIG_FLOAT) {
                slimb_t e;
                int ret;

                /* need a runtime conversion */
                /* XXX: could add a cache and/or do it once at
                   the start of the function */
                if (emit_push_const(s, val, 0) < 0)
                    return -1;
                e = s->token.u.num.exponent;
                if (e == (int32_t) e) {
                    emit_op(s, OP_push_i32);
                    emit_u32(s, e);
                } else {
                    val = JS_NewBigInt64_1(s->ctx, e);
                    if (JS_IsException(val))
                        return -1;
                    ret = emit_push_const(s, val, 0);
                    JS_FreeValue(s->ctx, val);
                    if (ret < 0)
                        return -1;
                }
                emit_op(s, OP_mul_pow10);
            } else
#endif
            {
                if (emit_push_const(s, val, 0) < 0)
                    return -1;
            }
        }
            if (next_token(s))
                return -1;
            break;
        case TOK_TEMPLATE:
            if (js_parse_template(s, 0, NULL))
                return -1;
            break;
        case TOK_STRING:
            if (emit_push_const(s, s->token.u.str.str, 1))
                return -1;
            if (next_token(s))
                return -1;
            break;

        case TOK_DIV_ASSIGN:
            s->buf_ptr -= 2;
            goto parse_regexp;
        case '/':
            s->buf_ptr--;
        parse_regexp:
        {
            JSValue str;
            int ret, backtrace_flags;
            if (!s->ctx->compile_regexp)
                return js_parse_error(s, "RegExp are not supported");
            /* the previous token is '/' or '/=', so no need to free */
            if (js_parse_regexp(s))
                return -1;
            ret = emit_push_const(s, s->token.u.regexp.body, 0);
            str = s->ctx->compile_regexp(s->ctx, s->token.u.regexp.body,
                                         s->token.u.regexp.flags);
            if (JS_IsException(str)) {
                /* add the line number info */
                backtrace_flags = 0;
                if (s->cur_func && s->cur_func->backtrace_barrier)
                    backtrace_flags = JS_BACKTRACE_FLAG_SINGLE_LEVEL;
                build_backtrace(s->ctx, s->ctx->rt->current_exception,
                                s->filename, s->token.line_num,
                                backtrace_flags);
                return -1;
            }
            ret = emit_push_const(s, str, 0);
            JS_FreeValue(s->ctx, str);
            if (ret)
                return -1;
            /* we use a specific opcode to be sure the correct
               function is called (otherwise the bytecode would have
               to be verified by the RegExp constructor) */
            emit_op(s, OP_regexp);
            if (next_token(s))
                return -1;
        }
            break;
        case '(':
            if ((parse_flags & PF_ARROW_FUNC) &&
                js_parse_skip_parens_token(s, NULL, TRUE) == TOK_ARROW) {
                if (js_parse_function_decl(s, JS_PARSE_FUNC_ARROW,
                                           JS_FUNC_NORMAL, JS_ATOM_NULL,
                                           s->token.ptr, s->token.line_num))
                    return -1;
            } else {
                if (js_parse_expr_paren(s))
                    return -1;
            }
            break;
        case TOK_FUNCTION:
            if (js_parse_function_decl(s, JS_PARSE_FUNC_EXPR,
                                       JS_FUNC_NORMAL, JS_ATOM_NULL,
                                       s->token.ptr, s->token.line_num))
                return -1;
            break;
        case TOK_CLASS:
            if (js_parse_class(s, TRUE, JS_PARSE_EXPORT_NONE))
                return -1;
            break;
        case TOK_NULL:
            if (next_token(s))
                return -1;
            emit_op(s, OP_null);
            break;
        case TOK_THIS:
            if (next_token(s))
                return -1;
            emit_op(s, OP_scope_get_var);
            emit_atom(s, JS_ATOM_this);
            emit_u16(s, 0);
            break;
        case TOK_FALSE:
            if (next_token(s))
                return -1;
            emit_op(s, OP_push_false);
            break;
        case TOK_TRUE:
            if (next_token(s))
                return -1;
            emit_op(s, OP_push_true);
            break;
        case TOK_IDENT: {
            JSAtom name;
            if (s->token.u.ident.is_reserved) {
                return js_parse_error_reserved_identifier(s);
            }
            if ((parse_flags & PF_ARROW_FUNC) &&
                peek_token(s, TRUE) == TOK_ARROW) {
                if (js_parse_function_decl(s, JS_PARSE_FUNC_ARROW,
                                           JS_FUNC_NORMAL, JS_ATOM_NULL,
                                           s->token.ptr, s->token.line_num))
                    return -1;
            } else if (token_is_pseudo_keyword(s, JS_ATOM_async) &&
                       peek_token(s, TRUE) != '\n') {
                const uint8_t *source_ptr;
                int source_line_num;

                source_ptr = s->token.ptr;
                source_line_num = s->token.line_num;
                if (next_token(s))
                    return -1;
                if (s->token.val == TOK_FUNCTION) {
                    if (js_parse_function_decl(s, JS_PARSE_FUNC_EXPR,
                                               JS_FUNC_ASYNC, JS_ATOM_NULL,
                                               source_ptr, source_line_num))
                        return -1;
                } else if ((parse_flags & PF_ARROW_FUNC) &&
                           ((s->token.val == '(' &&
                             js_parse_skip_parens_token(s, NULL, TRUE) == TOK_ARROW) ||
                            (s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved &&
                             peek_token(s, TRUE) == TOK_ARROW))) {
                    if (js_parse_function_decl(s, JS_PARSE_FUNC_ARROW,
                                               JS_FUNC_ASYNC, JS_ATOM_NULL,
                                               source_ptr, source_line_num))
                        return -1;
                } else {
                    name = JS_DupAtom(s->ctx, JS_ATOM_async);
                    goto do_get_var;
                }
            } else {
                if (s->token.u.ident.atom == JS_ATOM_arguments &&
                    !s->cur_func->arguments_allowed) {
                    js_parse_error(s, "'arguments' identifier is not allowed in class field initializer");
                    return -1;
                }
                name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
                if (next_token(s))  /* update line number before emitting code */
                    return -1;
                do_get_var:
                emit_op(s, OP_scope_get_var);
                emit_u32(s, name);
                emit_u16(s, s->cur_func->scope_level);
            }
        }
            break;
        case '{':
        case '[': {
            int skip_bits;
            if (js_parse_skip_parens_token(s, &skip_bits, FALSE) == '=') {
                if (js_parse_destructuring_element(s, 0, 0, FALSE, skip_bits & SKIP_HAS_ELLIPSIS, TRUE) < 0)
                    return -1;
            } else {
                if (s->token.val == '{') {
                    if (js_parse_object_literal(s))
                        return -1;
                } else {
                    if (js_parse_array_literal(s))
                        return -1;
                }
            }
        }
            break;
        case TOK_NEW:
            if (next_token(s))
                return -1;
            if (s->token.val == '.') {
                if (next_token(s))
                    return -1;
                if (!token_is_pseudo_keyword(s, JS_ATOM_target))
                    return js_parse_error(s, "expecting target");
                if (!s->cur_func->new_target_allowed)
                    return js_parse_error(s, "new.target only allowed within functions");
                if (next_token(s))
                    return -1;
                emit_op(s, OP_scope_get_var);
                emit_atom(s, JS_ATOM_new_target);
                emit_u16(s, 0);
            } else {
                if (js_parse_postfix_expr(s, 0))
                    return -1;
                accept_lparen = TRUE;
                if (s->token.val != '(') {
                    /* new operator on an object */
                    emit_op(s, OP_dup);
                    emit_op(s, OP_call_constructor);
                    emit_u16(s, 0);
                } else {
                    call_type = FUNC_CALL_NEW;
                }
            }
            break;
        case TOK_SUPER:
            if (next_token(s))
                return -1;
            if (s->token.val == '(') {
                if (!s->cur_func->super_call_allowed)
                    return js_parse_error(s, "super() is only valid in a derived class constructor");
                call_type = FUNC_CALL_SUPER_CTOR;
            } else if (s->token.val == '.' || s->token.val == '[') {
                if (!s->cur_func->super_allowed)
                    return js_parse_error(s, "'super' is only valid in a method");
                emit_op(s, OP_scope_get_var);
                emit_atom(s, JS_ATOM_this);
                emit_u16(s, 0);
                emit_op(s, OP_scope_get_var);
                emit_atom(s, JS_ATOM_home_object);
                emit_u16(s, 0);
                emit_op(s, OP_get_super);
            } else {
                return js_parse_error(s, "invalid use of 'super'");
            }
            break;
        case TOK_IMPORT:
            if (next_token(s))
                return -1;
            if (s->token.val == '.') {
                if (next_token(s))
                    return -1;
                if (!token_is_pseudo_keyword(s, JS_ATOM_meta))
                    return js_parse_error(s, "meta expected");
                if (!s->is_module)
                    return js_parse_error(s, "import.meta only valid in module code");
                if (next_token(s))
                    return -1;
                emit_op(s, OP_special_object);
                emit_u8(s, OP_SPECIAL_OBJECT_IMPORT_META);
            } else {
                if (js_parse_expect(s, '('))
                    return -1;
                if (!accept_lparen)
                    return js_parse_error(s, "invalid use of 'import()'");
                if (js_parse_assign_expr(s))
                    return -1;
                if (js_parse_expect(s, ')'))
                    return -1;
                emit_op(s, OP_import);
            }
            break;
        default:
            return js_parse_error(s, "unexpected token in expression: '%.*s'",
                                  (int) (s->buf_ptr - s->token.ptr), s->token.ptr);
    }

    optional_chaining_label = -1;
    for (;;) {
        JSFunctionDef *fd = s->cur_func;
        BOOL has_optional_chain = FALSE;

        if (s->token.val == TOK_QUESTION_MARK_DOT) {
            /* optional chaining */
            if (next_token(s))
                return -1;
            has_optional_chain = TRUE;
            if (s->token.val == '(' && accept_lparen) {
                goto parse_func_call;
            } else if (s->token.val == '[') {
                goto parse_array_access;
            } else {
                goto parse_property;
            }
        } else if (s->token.val == TOK_TEMPLATE &&
                   call_type == FUNC_CALL_NORMAL) {
            if (optional_chaining_label >= 0) {
                return js_parse_error(s, "template literal cannot appear in an optional chain");
            }
            call_type = FUNC_CALL_TEMPLATE;
            goto parse_func_call2;
        } else if (s->token.val == '(' && accept_lparen) {
            int opcode, arg_count, drop_count;

            /* function call */
            parse_func_call:
            if (next_token(s))
                return -1;

            if (call_type == FUNC_CALL_NORMAL) {
                parse_func_call2:
                switch (opcode = get_prev_opcode(fd)) {
                    case OP_get_field:
                        /* keep the object on the stack */
                        fd->byte_code.buf[fd->last_opcode_pos] = OP_get_field2;
                        drop_count = 2;
                        break;
                    case OP_scope_get_private_field:
                        /* keep the object on the stack */
                        fd->byte_code.buf[fd->last_opcode_pos] = OP_scope_get_private_field2;
                        drop_count = 2;
                        break;
                    case OP_get_array_el:
                        /* keep the object on the stack */
                        fd->byte_code.buf[fd->last_opcode_pos] = OP_get_array_el2;
                        drop_count = 2;
                        break;
                    case OP_scope_get_var: {
                        JSAtom name;
                        int scope;
                        name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
                        scope = get_u16(fd->byte_code.buf + fd->last_opcode_pos + 5);
                        if (name == JS_ATOM_eval && call_type == FUNC_CALL_NORMAL && !has_optional_chain) {
                            /* direct 'eval' */
                            opcode = OP_eval;
                        } else {
                            /* verify if function name resolves to a simple
                               get_loc/get_arg: a function call inside a `with`
                               statement can resolve to a method call of the
                               `with` context object
                             */
                            /* XXX: always generate the OP_scope_get_ref
                               and remove it in variable resolution
                               pass ? */
                            if (has_with_scope(fd, scope)) {
                                opcode = OP_scope_get_ref;
                                fd->byte_code.buf[fd->last_opcode_pos] = opcode;
                            }
                        }
                        drop_count = 1;
                    }
                        break;
                    case OP_get_super_value:
                        fd->byte_code.buf[fd->last_opcode_pos] = OP_get_array_el;
                        /* on stack: this func_obj */
                        opcode = OP_get_array_el;
                        drop_count = 2;
                        break;
                    default:
                        opcode = OP_invalid;
                        drop_count = 1;
                        break;
                }
                if (has_optional_chain) {
                    optional_chain_test(s, &optional_chaining_label,
                                        drop_count);
                }
            } else {
                opcode = OP_invalid;
            }

            if (call_type == FUNC_CALL_TEMPLATE) {
                if (js_parse_template(s, 1, &arg_count))
                    return -1;
                goto emit_func_call;
            } else if (call_type == FUNC_CALL_SUPER_CTOR) {
                emit_op(s, OP_scope_get_var);
                emit_atom(s, JS_ATOM_this_active_func);
                emit_u16(s, 0);

                emit_op(s, OP_get_super);

                emit_op(s, OP_scope_get_var);
                emit_atom(s, JS_ATOM_new_target);
                emit_u16(s, 0);
            } else if (call_type == FUNC_CALL_NEW) {
                emit_op(s, OP_dup); /* new.target = function */
            }

            /* parse arguments */
            arg_count = 0;
            while (s->token.val != ')') {
                if (arg_count >= 65535) {
                    return js_parse_error(s, "Too many call arguments");
                }
                if (s->token.val == TOK_ELLIPSIS)
                    break;
                if (js_parse_assign_expr(s))
                    return -1;
                arg_count++;
                if (s->token.val == ')')
                    break;
                /* accept a trailing comma before the ')' */
                if (js_parse_expect(s, ','))
                    return -1;
            }
            if (s->token.val == TOK_ELLIPSIS) {
                emit_op(s, OP_array_from);
                emit_u16(s, arg_count);
                emit_op(s, OP_push_i32);
                emit_u32(s, arg_count);

                /* on stack: array idx */
                while (s->token.val != ')') {
                    if (s->token.val == TOK_ELLIPSIS) {
                        if (next_token(s))
                            return -1;
                        if (js_parse_assign_expr(s))
                            return -1;
#if 1
                        /* XXX: could pass is_last indicator? */
                        emit_op(s, OP_append);
#else
                        int label_next, label_done;
                        label_next = new_label(s);
                        label_done = new_label(s);
                        /* push enumerate object below array/idx pair */
                        emit_op(s, OP_for_of_start);
                        emit_op(s, OP_rot5l);
                        emit_op(s, OP_rot5l);
                        emit_label(s, label_next);
                        /* on stack: enum_rec array idx */
                        emit_op(s, OP_for_of_next);
                        emit_u8(s, 2);
                        emit_goto(s, OP_if_true, label_done);
                        /* append element */
                        /* enum_rec array idx val -> enum_rec array new_idx */
                        emit_op(s, OP_define_array_el);
                        emit_op(s, OP_inc);
                        emit_goto(s, OP_goto, label_next);
                        emit_label(s, label_done);
                        /* close enumeration, drop enum_rec and idx */
                        emit_op(s, OP_drop); /* drop undef */
                        emit_op(s, OP_nip1); /* drop enum_rec */
                        emit_op(s, OP_nip1);
                        emit_op(s, OP_nip1);
#endif
                    } else {
                        if (js_parse_assign_expr(s))
                            return -1;
                        /* array idx val */
                        emit_op(s, OP_define_array_el);
                        emit_op(s, OP_inc);
                    }
                    if (s->token.val == ')')
                        break;
                    /* accept a trailing comma before the ')' */
                    if (js_parse_expect(s, ','))
                        return -1;
                }
                if (next_token(s))
                    return -1;
                /* drop the index */
                emit_op(s, OP_drop);

                /* apply function call */
                switch (opcode) {
                    case OP_get_field:
                    case OP_scope_get_private_field:
                    case OP_get_array_el:
                    case OP_scope_get_ref:
                        /* obj func array -> func obj array */
                        emit_op(s, OP_perm3);
                        emit_op(s, OP_apply);
                        emit_u16(s, call_type == FUNC_CALL_NEW);
                        break;
                    case OP_eval:
                        emit_op(s, OP_apply_eval);
                        emit_u16(s, fd->scope_level);
                        fd->has_eval_call = TRUE;
                        break;
                    default:
                        if (call_type == FUNC_CALL_SUPER_CTOR) {
                            emit_op(s, OP_apply);
                            emit_u16(s, 1);
                            /* set the 'this' value */
                            emit_op(s, OP_dup);
                            emit_op(s, OP_scope_put_var_init);
                            emit_atom(s, JS_ATOM_this);
                            emit_u16(s, 0);

                            emit_class_field_init(s);
                        } else if (call_type == FUNC_CALL_NEW) {
                            /* obj func array -> func obj array */
                            emit_op(s, OP_perm3);
                            emit_op(s, OP_apply);
                            emit_u16(s, 1);
                        } else {
                            /* func array -> func undef array */
                            emit_op(s, OP_undefined);
                            emit_op(s, OP_swap);
                            emit_op(s, OP_apply);
                            emit_u16(s, 0);
                        }
                        break;
                }
            } else {
                if (next_token(s))
                    return -1;
                emit_func_call:
                switch (opcode) {
                    case OP_get_field:
                    case OP_scope_get_private_field:
                    case OP_get_array_el:
                    case OP_scope_get_ref:
                        emit_op(s, OP_call_method);
                        emit_u16(s, arg_count);
                        break;
                    case OP_eval:
                        emit_op(s, OP_eval);
                        emit_u16(s, arg_count);
                        emit_u16(s, fd->scope_level);
                        fd->has_eval_call = TRUE;
                        break;
                    default:
                        if (call_type == FUNC_CALL_SUPER_CTOR) {
                            emit_op(s, OP_call_constructor);
                            emit_u16(s, arg_count);

                            /* set the 'this' value */
                            emit_op(s, OP_dup);
                            emit_op(s, OP_scope_put_var_init);
                            emit_atom(s, JS_ATOM_this);
                            emit_u16(s, 0);

                            emit_class_field_init(s);
                        } else if (call_type == FUNC_CALL_NEW) {
                            emit_op(s, OP_call_constructor);
                            emit_u16(s, arg_count);
                        } else {
                            emit_op(s, OP_call);
                            emit_u16(s, arg_count);
                        }
                        break;
                }
            }
            call_type = FUNC_CALL_NORMAL;
        } else if (s->token.val == '.') {
            if (next_token(s))
                return -1;
            parse_property:
            if (s->token.val == TOK_PRIVATE_NAME) {
                /* private class field */
                if (get_prev_opcode(fd) == OP_get_super) {
                    return js_parse_error(s, "private class field forbidden after super");
                }
                if (has_optional_chain) {
                    optional_chain_test(s, &optional_chaining_label, 1);
                }
                emit_op(s, OP_scope_get_private_field);
                emit_atom(s, s->token.u.ident.atom);
                emit_u16(s, s->cur_func->scope_level);
            } else {
                if (!token_is_ident(s->token.val)) {
                    return js_parse_error(s, "expecting field name");
                }
                if (get_prev_opcode(fd) == OP_get_super) {
                    JSValue val;
                    int ret;
                    val = JS_AtomToValue(s->ctx, s->token.u.ident.atom);
                    ret = emit_push_const(s, val, 1);
                    JS_FreeValue(s->ctx, val);
                    if (ret)
                        return -1;
                    emit_op(s, OP_get_super_value);
                } else {
                    if (has_optional_chain) {
                        optional_chain_test(s, &optional_chaining_label, 1);
                    }
                    emit_op(s, OP_get_field);
                    emit_atom(s, s->token.u.ident.atom);
                }
            }
            if (next_token(s))
                return -1;
        } else if (s->token.val == '[') {
            int prev_op;

            parse_array_access:
            prev_op = get_prev_opcode(fd);
            if (has_optional_chain) {
                optional_chain_test(s, &optional_chaining_label, 1);
            }
            if (next_token(s))
                return -1;
            if (js_parse_expr(s))
                return -1;
            if (js_parse_expect(s, ']'))
                return -1;
            if (prev_op == OP_get_super) {
                emit_op(s, OP_get_super_value);
            } else {
                emit_op(s, OP_get_array_el);
            }
        } else {
            break;
        }
    }
    if (optional_chaining_label >= 0)
        emit_label(s, optional_chaining_label);
    return 0;
}

static __exception int js_parse_delete(JSParseState *s) {
    JSFunctionDef *fd = s->cur_func;
    JSAtom name;
    int opcode;

    if (next_token(s))
        return -1;
    if (js_parse_unary(s, PF_POW_FORBIDDEN))
        return -1;
    switch (opcode = get_prev_opcode(fd)) {
        case OP_get_field: {
            JSValue val;
            int ret;

            name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
            fd->byte_code.size = fd->last_opcode_pos;
            fd->last_opcode_pos = -1;
            val = JS_AtomToValue(s->ctx, name);
            ret = emit_push_const(s, val, 1);
            JS_FreeValue(s->ctx, val);
            JS_FreeAtom(s->ctx, name);
            if (ret)
                return ret;
        }
            goto do_delete;
        case OP_get_array_el:
            fd->byte_code.size = fd->last_opcode_pos;
            fd->last_opcode_pos = -1;
        do_delete:
            emit_op(s, OP_delete);
            break;
        case OP_scope_get_var:
            /* 'delete this': this is not a reference */
            name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
            if (name == JS_ATOM_this || name == JS_ATOM_new_target)
                goto ret_true;
            if (fd->js_mode & JS_MODE_STRICT) {
                return js_parse_error(s, "cannot delete a direct reference in strict mode");
            } else {
                fd->byte_code.buf[fd->last_opcode_pos] = OP_scope_delete_var;
            }
            break;
        case OP_scope_get_private_field:
            return js_parse_error(s, "cannot delete a private class field");
        case OP_get_super_value:
            emit_op(s, OP_throw_error);
            emit_atom(s, JS_ATOM_NULL);
            emit_u8(s, JS_THROW_ERROR_DELETE_SUPER);
            break;
        default:
        ret_true:
            emit_op(s, OP_drop);
            emit_op(s, OP_push_true);
            break;
    }
    return 0;
}

/* allowed parse_flags: PF_ARROW_FUNC, PF_POW_ALLOWED, PF_POW_FORBIDDEN */
__exception int js_parse_unary(JSParseState *s, int parse_flags) {
    int op;

    switch (s->token.val) {
        case '+':
        case '-':
        case '!':
        case '~':
        case TOK_VOID:
            op = s->token.val;
            if (next_token(s))
                return -1;
            if (js_parse_unary(s, PF_POW_FORBIDDEN))
                return -1;
            switch (op) {
                case '-':
                    emit_op(s, OP_neg);
                    break;
                case '+':
                    emit_op(s, OP_plus);
                    break;
                case '!':
                    emit_op(s, OP_lnot);
                    break;
                case '~':
                    emit_op(s, OP_not);
                    break;
                case TOK_VOID:
                    emit_op(s, OP_drop);
                    emit_op(s, OP_undefined);
                    break;
                default:
                    abort();
            }
            parse_flags = 0;
            break;
        case TOK_DEC:
        case TOK_INC: {
            int opcode, op, scope, label;
            JSAtom name;
            op = s->token.val;
            if (next_token(s))
                return -1;
            if (js_parse_unary(s, 0))
                return -1;
            if (get_lvalue(s, &opcode, &scope, &name, &label, NULL, TRUE, op))
                return -1;
            emit_op(s, OP_dec + op - TOK_DEC);
            put_lvalue(s, opcode, scope, name, label, PUT_LVALUE_KEEP_TOP,
                       FALSE);
        }
            break;
        case TOK_TYPEOF: {
            JSFunctionDef *fd;
            if (next_token(s))
                return -1;
            if (js_parse_unary(s, PF_POW_FORBIDDEN))
                return -1;
            /* reference access should not return an exception, so we
               patch the get_var */
            fd = s->cur_func;
            if (get_prev_opcode(fd) == OP_scope_get_var) {
                fd->byte_code.buf[fd->last_opcode_pos] = OP_scope_get_var_undef;
            }
            emit_op(s, OP_typeof);
            parse_flags = 0;
        }
            break;
        case TOK_DELETE:
            if (js_parse_delete(s))
                return -1;
            parse_flags = 0;
            break;
        case TOK_AWAIT:
            if (!(s->cur_func->func_kind & JS_FUNC_ASYNC))
                return js_parse_error(s, "unexpected 'await' keyword");
            if (!s->cur_func->in_function_body)
                return js_parse_error(s, "await in default expression");
            if (next_token(s))
                return -1;
            if (js_parse_unary(s, PF_POW_FORBIDDEN))
                return -1;
            emit_op(s, OP_await);
            parse_flags = 0;
            break;
        default:
            if (js_parse_postfix_expr(s, (parse_flags & PF_ARROW_FUNC) |
                                         PF_POSTFIX_CALL))
                return -1;
            if (!s->got_lf &&
                (s->token.val == TOK_DEC || s->token.val == TOK_INC)) {
                int opcode, op, scope, label;
                JSAtom name;
                op = s->token.val;
                if (get_lvalue(s, &opcode, &scope, &name, &label, NULL, TRUE, op))
                    return -1;
                emit_op(s, OP_post_dec + op - TOK_DEC);
                put_lvalue(s, opcode, scope, name, label, PUT_LVALUE_KEEP_SECOND,
                           FALSE);
                if (next_token(s))
                    return -1;
            }
            break;
    }
    if (parse_flags & (PF_POW_ALLOWED | PF_POW_FORBIDDEN)) {
#ifdef CONFIG_BIGNUM
        if (s->token.val == TOK_POW || s->token.val == TOK_MATH_POW) {
            /* Extended exponentiation syntax rules: we extend the ES7
               grammar in order to have more intuitive semantics:
               -2**2 evaluates to -4. */
            if (!(s->cur_func->js_mode & JS_MODE_MATH)) {
                if (parse_flags & PF_POW_FORBIDDEN) {
                    JS_ThrowSyntaxError(s->ctx,
                                        "unparenthesized unary expression can't appear on the left-hand side of '**'");
                    return -1;
                }
            }
            if (next_token(s))
                return -1;
            if (js_parse_unary(s, PF_POW_ALLOWED))
                return -1;
            emit_op(s, OP_pow);
        }
#else
        if (s->token.val == TOK_POW) {
            /* Strict ES7 exponentiation syntax rules: To solve
               conficting semantics between different implementations
               regarding the precedence of prefix operators and the
               postifx exponential, ES7 specifies that -2**2 is a
               syntax error. */
            if (parse_flags & PF_POW_FORBIDDEN) {
                JS_ThrowSyntaxError(s->ctx, "unparenthesized unary expression can't appear on the left-hand side of '**'");
                return -1;
            }
            if (next_token(s))
                return -1;
            if (js_parse_unary(s, PF_POW_ALLOWED))
                return -1;
            emit_op(s, OP_pow);
        }
#endif
    }
    return 0;
}

/* allowed parse_flags: PF_ARROW_FUNC, PF_IN_ACCEPTED */
static __exception int js_parse_expr_binary(JSParseState *s, int level,
                                            int parse_flags) {
    int op, opcode;

    if (level == 0) {
        return js_parse_unary(s, (parse_flags & PF_ARROW_FUNC) |
                                 PF_POW_ALLOWED);
    }
    if (js_parse_expr_binary(s, level - 1, parse_flags))
        return -1;
    for (;;) {
        op = s->token.val;
        switch (level) {
            case 1:
                switch (op) {
                    case '*':
                        opcode = OP_mul;
                        break;
                    case '/':
                        opcode = OP_div;
                        break;
                    case '%':
#ifdef CONFIG_BIGNUM
                        if (s->cur_func->js_mode & JS_MODE_MATH)
                            opcode = OP_math_mod;
                        else
#endif
                            opcode = OP_mod;
                        break;
                    default:
                        return 0;
                }
                break;
            case 2:
                switch (op) {
                    case '+':
                        opcode = OP_add;
                        break;
                    case '-':
                        opcode = OP_sub;
                        break;
                    default:
                        return 0;
                }
                break;
            case 3:
                switch (op) {
                    case TOK_SHL:
                        opcode = OP_shl;
                        break;
                    case TOK_SAR:
                        opcode = OP_sar;
                        break;
                    case TOK_SHR:
                        opcode = OP_shr;
                        break;
                    default:
                        return 0;
                }
                break;
            case 4:
                switch (op) {
                    case '<':
                        opcode = OP_lt;
                        break;
                    case '>':
                        opcode = OP_gt;
                        break;
                    case TOK_LTE:
                        opcode = OP_lte;
                        break;
                    case TOK_GTE:
                        opcode = OP_gte;
                        break;
                    case TOK_INSTANCEOF:
                        opcode = OP_instanceof;
                        break;
                    case TOK_IN:
                        if (parse_flags & PF_IN_ACCEPTED) {
                            opcode = OP_in;
                        } else {
                            return 0;
                        }
                        break;
                    default:
                        return 0;
                }
                break;
            case 5:
                switch (op) {
                    case TOK_EQ:
                        opcode = OP_eq;
                        break;
                    case TOK_NEQ:
                        opcode = OP_neq;
                        break;
                    case TOK_STRICT_EQ:
                        opcode = OP_strict_eq;
                        break;
                    case TOK_STRICT_NEQ:
                        opcode = OP_strict_neq;
                        break;
                    default:
                        return 0;
                }
                break;
            case 6:
                switch (op) {
                    case '&':
                        opcode = OP_and;
                        break;
                    default:
                        return 0;
                }
                break;
            case 7:
                switch (op) {
                    case '^':
                        opcode = OP_xor;
                        break;
                    default:
                        return 0;
                }
                break;
            case 8:
                switch (op) {
                    case '|':
                        opcode = OP_or;
                        break;
                    default:
                        return 0;
                }
                break;
            default:
                abort();
        }
        if (next_token(s))
            return -1;
        if (js_parse_expr_binary(s, level - 1, parse_flags & ~PF_ARROW_FUNC))
            return -1;
        emit_op(s, opcode);
    }
    return 0;
}

/* allowed parse_flags: PF_ARROW_FUNC, PF_IN_ACCEPTED */
static __exception int js_parse_logical_and_or(JSParseState *s, int op,
                                               int parse_flags) {
    int label1;

    if (op == TOK_LAND) {
        if (js_parse_expr_binary(s, 8, parse_flags))
            return -1;
    } else {
        if (js_parse_logical_and_or(s, TOK_LAND, parse_flags))
            return -1;
    }
    if (s->token.val == op) {
        label1 = new_label(s);

        for (;;) {
            if (next_token(s))
                return -1;
            emit_op(s, OP_dup);
            emit_goto(s, op == TOK_LAND ? OP_if_false : OP_if_true, label1);
            emit_op(s, OP_drop);

            if (op == TOK_LAND) {
                if (js_parse_expr_binary(s, 8, parse_flags & ~PF_ARROW_FUNC))
                    return -1;
            } else {
                if (js_parse_logical_and_or(s, TOK_LAND,
                                            parse_flags & ~PF_ARROW_FUNC))
                    return -1;
            }
            if (s->token.val != op) {
                if (s->token.val == TOK_DOUBLE_QUESTION_MARK)
                    return js_parse_error(s, "cannot mix ?? with && or ||");
                break;
            }
        }

        emit_label(s, label1);
    }
    return 0;
}

static __exception int js_parse_coalesce_expr(JSParseState *s, int parse_flags) {
    int label1;

    if (js_parse_logical_and_or(s, TOK_LOR, parse_flags))
        return -1;
    if (s->token.val == TOK_DOUBLE_QUESTION_MARK) {
        label1 = new_label(s);
        for (;;) {
            if (next_token(s))
                return -1;

            emit_op(s, OP_dup);
            emit_op(s, OP_is_undefined_or_null);
            emit_goto(s, OP_if_false, label1);
            emit_op(s, OP_drop);

            if (js_parse_expr_binary(s, 8, parse_flags & ~PF_ARROW_FUNC))
                return -1;
            if (s->token.val != TOK_DOUBLE_QUESTION_MARK)
                break;
        }
        emit_label(s, label1);
    }
    return 0;
}


/* allowed parse_flags: PF_ARROW_FUNC, PF_IN_ACCEPTED */
static __exception int js_parse_cond_expr(JSParseState *s, int parse_flags) {
    int label1, label2;

    if (js_parse_coalesce_expr(s, parse_flags))
        return -1;
    if (s->token.val == '?') {
        if (next_token(s))
            return -1;
        label1 = emit_goto(s, OP_if_false, -1);

        if (js_parse_assign_expr(s))
            return -1;
        if (js_parse_expect(s, ':'))
            return -1;

        label2 = emit_goto(s, OP_goto, -1);

        emit_label(s, label1);

        if (js_parse_assign_expr2(s, parse_flags & PF_IN_ACCEPTED))
            return -1;

        emit_label(s, label2);
    }
    return 0;
}



/* allowed parse_flags: PF_IN_ACCEPTED */
__exception int js_parse_assign_expr2(JSParseState *s, int parse_flags) {
    int opcode, op, scope;
    JSAtom name0 = JS_ATOM_NULL;
    JSAtom name;

    if (s->token.val == TOK_YIELD) {
        BOOL is_star = FALSE, is_async;

        if (!(s->cur_func->func_kind & JS_FUNC_GENERATOR))
            return js_parse_error(s, "unexpected 'yield' keyword");
        if (!s->cur_func->in_function_body)
            return js_parse_error(s, "yield in default expression");
        if (next_token(s))
            return -1;
        /* XXX: is there a better method to detect 'yield' without
           parameters ? */
        if (s->token.val != ';' && s->token.val != ')' &&
            s->token.val != ']' && s->token.val != '}' &&
            s->token.val != ',' && s->token.val != ':' && !s->got_lf) {
            if (s->token.val == '*') {
                is_star = TRUE;
                if (next_token(s))
                    return -1;
            }
            if (js_parse_assign_expr2(s, parse_flags))
                return -1;
        } else {
            emit_op(s, OP_undefined);
        }
        is_async = (s->cur_func->func_kind == JS_FUNC_ASYNC_GENERATOR);

        if (is_star) {
            int label_loop, label_return, label_next;
            int label_return1, label_yield, label_throw, label_throw1;
            int label_throw2;

            label_loop = new_label(s);
            label_yield = new_label(s);

            emit_op(s, is_async ? OP_for_await_of_start : OP_for_of_start);

            /* remove the catch offset (XXX: could avoid pushing back
               undefined) */
            emit_op(s, OP_drop);
            emit_op(s, OP_undefined);

            emit_op(s, OP_undefined); /* initial value */

            emit_label(s, label_loop);
            emit_op(s, OP_iterator_next);
            if (is_async)
                emit_op(s, OP_await);
            emit_op(s, OP_iterator_check_object);
            emit_op(s, OP_get_field2);
            emit_atom(s, JS_ATOM_done);
            label_next = emit_goto(s, OP_if_true, -1); /* end of loop */
            emit_label(s, label_yield);
            if (is_async) {
                /* OP_async_yield_star takes the value as parameter */
                emit_op(s, OP_get_field);
                emit_atom(s, JS_ATOM_value);
                emit_op(s, OP_await);
                emit_op(s, OP_async_yield_star);
            } else {
                /* OP_yield_star takes (value, done) as parameter */
                emit_op(s, OP_yield_star);
            }
            emit_op(s, OP_dup);
            label_return = emit_goto(s, OP_if_true, -1);
            emit_op(s, OP_drop);
            emit_goto(s, OP_goto, label_loop);

            emit_label(s, label_return);
            emit_op(s, OP_push_i32);
            emit_u32(s, 2);
            emit_op(s, OP_strict_eq);
            label_throw = emit_goto(s, OP_if_true, -1);

            /* return handling */
            if (is_async)
                emit_op(s, OP_await);
            emit_op(s, OP_iterator_call);
            emit_u8(s, 0);
            label_return1 = emit_goto(s, OP_if_true, -1);
            if (is_async)
                emit_op(s, OP_await);
            emit_op(s, OP_iterator_check_object);
            emit_op(s, OP_get_field2);
            emit_atom(s, JS_ATOM_done);
            emit_goto(s, OP_if_false, label_yield);

            emit_op(s, OP_get_field);
            emit_atom(s, JS_ATOM_value);

            emit_label(s, label_return1);
            emit_op(s, OP_nip);
            emit_op(s, OP_nip);
            emit_op(s, OP_nip);
            emit_return(s, TRUE);

            /* throw handling */
            emit_label(s, label_throw);
            emit_op(s, OP_iterator_call);
            emit_u8(s, 1);
            label_throw1 = emit_goto(s, OP_if_true, -1);
            if (is_async)
                emit_op(s, OP_await);
            emit_op(s, OP_iterator_check_object);
            emit_op(s, OP_get_field2);
            emit_atom(s, JS_ATOM_done);
            emit_goto(s, OP_if_false, label_yield);
            emit_goto(s, OP_goto, label_next);
            /* close the iterator and throw a type error exception */
            emit_label(s, label_throw1);
            emit_op(s, OP_iterator_call);
            emit_u8(s, 2);
            label_throw2 = emit_goto(s, OP_if_true, -1);
            if (is_async)
                emit_op(s, OP_await);
            emit_label(s, label_throw2);

            emit_op(s, OP_throw_error);
            emit_atom(s, JS_ATOM_NULL);
            emit_u8(s, JS_THROW_ERROR_ITERATOR_THROW);

            emit_label(s, label_next);
            emit_op(s, OP_get_field);
            emit_atom(s, JS_ATOM_value);
            emit_op(s, OP_nip); /* keep the value associated with
                                   done = true */
            emit_op(s, OP_nip);
            emit_op(s, OP_nip);
        } else {
            int label_next;

            if (is_async)
                emit_op(s, OP_await);
            emit_op(s, OP_yield);
            label_next = emit_goto(s, OP_if_false, -1);
            emit_return(s, TRUE);
            emit_label(s, label_next);
        }
        return 0;
    }
    if (s->token.val == TOK_IDENT) {
        /* name0 is used to check for OP_set_name pattern, not duplicated */
        name0 = s->token.u.ident.atom;
    }
    if (js_parse_cond_expr(s, parse_flags | PF_ARROW_FUNC))
        return -1;

    op = s->token.val;
    if (op == '=' || (op >= TOK_MUL_ASSIGN && op <= TOK_POW_ASSIGN)) {
        int label;
        if (next_token(s))
            return -1;
        if (get_lvalue(s, &opcode, &scope, &name, &label, NULL, (op != '='), op) < 0)
            return -1;

        if (js_parse_assign_expr2(s, parse_flags)) {
            JS_FreeAtom(s->ctx, name);
            return -1;
        }

        if (op == '=') {
            if (opcode == OP_get_ref_value && name == name0) {
                set_object_name(s, name);
            }
        } else {
            static const uint8_t assign_opcodes[] = {
                    OP_mul, OP_div, OP_mod, OP_add, OP_sub,
                    OP_shl, OP_sar, OP_shr, OP_and, OP_xor, OP_or,
#ifdef CONFIG_BIGNUM
                    OP_pow,
#endif
                    OP_pow,
            };
            op = assign_opcodes[op - TOK_MUL_ASSIGN];
#ifdef CONFIG_BIGNUM
            if (s->cur_func->js_mode & JS_MODE_MATH) {
                if (op == OP_mod)
                    op = OP_math_mod;
            }
#endif
            emit_op(s, op);
        }
        put_lvalue(s, opcode, scope, name, label, PUT_LVALUE_KEEP_TOP, FALSE);
    } else if (op >= TOK_LAND_ASSIGN && op <= TOK_DOUBLE_QUESTION_MARK_ASSIGN) {
        int label, label1, depth_lvalue, label2;

        if (next_token(s))
            return -1;
        if (get_lvalue(s, &opcode, &scope, &name, &label,
                       &depth_lvalue, TRUE, op) < 0)
            return -1;

        emit_op(s, OP_dup);
        if (op == TOK_DOUBLE_QUESTION_MARK_ASSIGN)
            emit_op(s, OP_is_undefined_or_null);
        label1 = emit_goto(s, op == TOK_LOR_ASSIGN ? OP_if_true : OP_if_false,
                           -1);
        emit_op(s, OP_drop);

        if (js_parse_assign_expr2(s, parse_flags)) {
            JS_FreeAtom(s->ctx, name);
            return -1;
        }

        if (opcode == OP_get_ref_value && name == name0) {
            set_object_name(s, name);
        }

        switch (depth_lvalue) {
            case 1:
                emit_op(s, OP_insert2);
                break;
            case 2:
                emit_op(s, OP_insert3);
                break;
            case 3:
                emit_op(s, OP_insert4);
                break;
            default:
                abort();
        }

        /* XXX: we disable the OP_put_ref_value optimization by not
           using put_lvalue() otherwise depth_lvalue is not correct */
        put_lvalue(s, opcode, scope, name, label, PUT_LVALUE_NOKEEP_DEPTH,
                   FALSE);
        label2 = emit_goto(s, OP_goto, -1);

        emit_label(s, label1);

        /* remove the lvalue stack entries */
        while (depth_lvalue != 0) {
            emit_op(s, OP_nip);
            depth_lvalue--;
        }

        emit_label(s, label2);
    }
    return 0;
}

__exception int js_parse_assign_expr(JSParseState *s) {
    return js_parse_assign_expr2(s, PF_IN_ACCEPTED);
}


__exception int js_parse_expr(JSParseState *s) {
    return js_parse_expr2(s, PF_IN_ACCEPTED);
}

/* allowed parse_flags: PF_IN_ACCEPTED */
__exception int js_parse_expr2(JSParseState *s, int parse_flags) {
    BOOL comma = FALSE;
    for (;;) {
        if (js_parse_assign_expr2(s, parse_flags))
            return -1;
        if (comma) {
            /* prevent get_lvalue from using the last expression
               as an lvalue. This also prevents the conversion of
               of get_var to get_ref for method lookup in function
               call inside `with` statement.
             */
            s->cur_func->last_opcode_pos = -1;
        }
        if (s->token.val != ',')
            break;
        comma = TRUE;
        if (next_token(s))
            return -1;
        emit_op(s, OP_drop);
    }
    return 0;
}


__exception int js_parse_regexp(JSParseState *s) {
    const uint8_t *p;
    BOOL in_class;
    StringBuffer b_s, *b = &b_s;
    StringBuffer b2_s, *b2 = &b2_s;
    uint32_t c;

    p = s->buf_ptr;
    p++;
    in_class = FALSE;
    if (string_buffer_init(s->ctx, b, 32))
        return -1;
    if (string_buffer_init(s->ctx, b2, 1))
        goto fail;
    for (;;) {
        if (p >= s->buf_end) {
            eof_error:
            js_parse_error(s, "unexpected end of regexp");
            goto fail;
        }
        c = *p++;
        if (c == '\n' || c == '\r') {
            goto eol_error;
        } else if (c == '/') {
            if (!in_class)
                break;
        } else if (c == '[') {
            in_class = TRUE;
        } else if (c == ']') {
            /* XXX: incorrect as the first character in a class */
            in_class = FALSE;
        } else if (c == '\\') {
            if (string_buffer_putc8(b, c))
                goto fail;
            c = *p++;
            if (c == '\n' || c == '\r')
                goto eol_error;
            else if (c == '\0' && p >= s->buf_end)
                goto eof_error;
            else if (c >= 0x80) {
                const uint8_t *p_next;
                c = unicode_from_utf8(p - 1, UTF8_CHAR_LEN_MAX, &p_next);
                if (c > 0x10FFFF) {
                    goto invalid_utf8;
                }
                p = p_next;
                if (c == CP_LS || c == CP_PS)
                    goto eol_error;
            }
        } else if (c >= 0x80) {
            const uint8_t *p_next;
            c = unicode_from_utf8(p - 1, UTF8_CHAR_LEN_MAX, &p_next);
            if (c > 0x10FFFF) {
                invalid_utf8:
                js_parse_error(s, "invalid UTF-8 sequence");
                goto fail;
            }
            p = p_next;
            /* LS or PS are considered as line terminator */
            if (c == CP_LS || c == CP_PS) {
                eol_error:
                js_parse_error(s, "unexpected line terminator in regexp");
                goto fail;
            }
        }
        if (string_buffer_putc(b, c))
            goto fail;
    }

    /* flags */
    for (;;) {
        const uint8_t *p_next = p;
        c = *p_next++;
        if (c >= 0x80) {
            c = unicode_from_utf8(p, UTF8_CHAR_LEN_MAX, &p_next);
            if (c > 0x10FFFF) {
                goto invalid_utf8;
            }
        }
        if (!lre_js_is_ident_next(c))
            break;
        if (string_buffer_putc(b2, c))
            goto fail;
        p = p_next;
    }

    s->token.val = TOK_REGEXP;
    s->token.u.regexp.body = string_buffer_end(b);
    s->token.u.regexp.flags = string_buffer_end(b2);
    s->buf_ptr = p;
    return 0;
    fail:
    string_buffer_free(b);
    string_buffer_free(b2);
    return -1;
}

int js_update_property_flags(JSContext *ctx, JSObject *p,
                             JSShapeProperty **pprs, int flags) {
    if (flags != (*pprs)->flags) {
        if (js_shape_prepare_update(ctx, p, pprs))
            return -1;
        (*pprs)->flags = flags;
    }
    return 0;
}


/* Note: all the fields are already sealed except length */
static int seal_template_obj(JSContext *ctx, JSValueConst obj) {
    JSObject *p;
    JSShapeProperty *prs;

    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property1(p, JS_ATOM_length);
    if (prs) {
        if (js_update_property_flags(ctx, p, &prs,
                                     prs->flags & ~(JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE)))
            return -1;
    }
    p->extensible = FALSE;
    return 0;
}


__exception int js_parse_template(JSParseState *s, int call, int *argc) {
    JSContext *ctx = s->ctx;
    JSValue raw_array, template_object;
    JSToken cooked;
    int depth, ret;

    raw_array = JS_UNDEFINED; /* avoid warning */
    template_object = JS_UNDEFINED; /* avoid warning */
    if (call) {
        /* Create a template object: an array of cooked strings */
        /* Create an array of raw strings and store it to the raw property */
        template_object = JS_NewArray(ctx);
        if (JS_IsException(template_object))
            return -1;
        //        pool_idx = s->cur_func->cpool_count;
        ret = emit_push_const(s, template_object, 0);
        JS_FreeValue(ctx, template_object);
        if (ret)
            return -1;
        raw_array = JS_NewArray(ctx);
        if (JS_IsException(raw_array))
            return -1;
        if (JS_DefinePropertyValue(ctx, template_object, JS_ATOM_raw,
                                   raw_array, JS_PROP_THROW) < 0) {
            return -1;
        }
    }

    depth = 0;
    while (s->token.val == TOK_TEMPLATE) {
        const uint8_t *p = s->token.ptr + 1;
        cooked = s->token;
        if (call) {
            if (JS_DefinePropertyValueUint32(ctx, raw_array, depth,
                                             JS_DupValue(ctx, s->token.u.str.str),
                                             JS_PROP_ENUMERABLE | JS_PROP_THROW) < 0) {
                return -1;
            }
            /* re-parse the string with escape sequences but do not throw a
               syntax error if it contains invalid sequences
             */
            if (js_parse_string(s, '`', FALSE, p, &cooked, &p)) {
                cooked.u.str.str = JS_UNDEFINED;
            }
            if (JS_DefinePropertyValueUint32(ctx, template_object, depth,
                                             cooked.u.str.str,
                                             JS_PROP_ENUMERABLE | JS_PROP_THROW) < 0) {
                return -1;
            }
        } else {
            JSString *str;
            /* re-parse the string with escape sequences and throw a
               syntax error if it contains invalid sequences
             */
            JS_FreeValue(ctx, s->token.u.str.str);
            s->token.u.str.str = JS_UNDEFINED;
            if (js_parse_string(s, '`', TRUE, p, &cooked, &p))
                return -1;
            str = JS_VALUE_GET_STRING(cooked.u.str.str);
            if (str->len != 0 || depth == 0) {
                ret = emit_push_const(s, cooked.u.str.str, 1);
                JS_FreeValue(s->ctx, cooked.u.str.str);
                if (ret)
                    return -1;
                if (depth == 0) {
                    if (s->token.u.str.sep == '`')
                        goto done1;
                    emit_op(s, OP_get_field2);
                    emit_atom(s, JS_ATOM_concat);
                }
                depth++;
            } else {
                JS_FreeValue(s->ctx, cooked.u.str.str);
            }
        }
        if (s->token.u.str.sep == '`')
            goto done;
        if (next_token(s))
            return -1;
        if (js_parse_expr(s))
            return -1;
        depth++;
        if (s->token.val != '}') {
            return js_parse_error(s, "expected '}' after template expression");
        }
        /* XXX: should convert to string at this stage? */
        free_token(s, &s->token);
        /* Resume TOK_TEMPLATE parsing (s->token.line_num and
         * s->token.ptr are OK) */
        s->got_lf = FALSE;
        s->last_line_num = s->token.line_num;
        if (js_parse_template_part(s, s->buf_ptr))
            return -1;
    }
    return js_parse_expect(s, TOK_TEMPLATE);

    done:
    if (call) {
        /* Seal the objects */
        seal_template_obj(ctx, raw_array);
        seal_template_obj(ctx, template_object);
        *argc = depth + 1;
    } else {
        emit_op(s, OP_call_method);
        emit_u16(s, depth - 1);
    }
    done1:
    return next_token(s);
}



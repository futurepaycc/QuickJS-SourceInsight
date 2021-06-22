//
// Created by benpeng.jiang on 2021/6/18.
//

#include "qjs-backtrace.h"


 void dbuf_put_leb128(DynBuf *s, uint32_t v)
{
    uint32_t a;
    for(;;) {
        a = v & 0x7f;
        v >>= 7;
        if (v != 0) {
            dbuf_putc(s, a | 0x80);
        } else {
            dbuf_putc(s, a);
            break;
        }
    }
}

 int bc_get_atom(BCReaderState *s, JSAtom *patom)
{
    uint32_t v;
    if (bc_get_leb128(s, &v))
        return -1;
    if (v & 1) {
        *patom = __JS_AtomFromUInt32(v >> 1);
        return 0;
    } else {
        return bc_idx_to_atom(s, patom, v >> 1);
    }
}

 int bc_idx_to_atom(BCReaderState *s, JSAtom *patom, uint32_t idx)
{
    JSAtom atom;

    if (__JS_AtomIsTaggedInt(idx)) {
        atom = idx;
    } else if (idx < s->first_atom) {
        atom = JS_DupAtom(s->ctx, idx);
    } else {
        idx -= s->first_atom;
        if (idx >= s->idx_to_atom_count) {
            JS_ThrowSyntaxError(s->ctx, "invalid atom index (pos=%u)",
                                (unsigned int)(s->ptr - s->buf_start));
            *patom = JS_ATOM_NULL;
            return s->error_state = -1;
        }
        atom = JS_DupAtom(s->ctx, s->idx_to_atom[idx]);
    }
    *patom = atom;
    return 0;
}


 void dbuf_put_sleb128(DynBuf *s, int32_t v1)
{
    uint32_t v = v1;
    dbuf_put_leb128(s, (2 * v) ^ -(v >> 31));
}

 int bc_get_sleb128(BCReaderState *s, int32_t *pval)
{
    int ret;
    ret = get_sleb128(pval, s->ptr, s->buf_end);
    if (unlikely(ret < 0))
        return bc_read_error_end(s);
    s->ptr += ret;
    return 0;
}

 int get_leb128(uint32_t *pval, const uint8_t *buf,
                      const uint8_t *buf_end)
{
    const uint8_t *ptr = buf;
    uint32_t v, a, i;
    v = 0;
    for(i = 0; i < 5; i++) {
        if (unlikely(ptr >= buf_end))
            break;
        a = *ptr++;
        v |= (a & 0x7f) << (i * 7);
        if (!(a & 0x80)) {
            *pval = v;
            return ptr - buf;
        }
    }
    *pval = 0;
    return -1;
}

 int get_sleb128(int32_t *pval, const uint8_t *buf,
                       const uint8_t *buf_end)
{
    int ret;
    uint32_t val;
    ret = get_leb128(&val, buf, buf_end);
    if (ret < 0) {
        *pval = 0;
        return -1;
    }
    *pval = (val >> 1) ^ -(val & 1);
    return ret;
}


 int find_line_num(JSContext *ctx, JSFunctionBytecode *b,
                         uint32_t pc_value)
{
    const uint8_t *p_end, *p;
    int new_line_num, line_num, pc, v, ret;
    unsigned int op;

    if (!b->has_debug || !b->debug.pc2line_buf) {
        /* function was stripped */
        return -1;
    }

    p = b->debug.pc2line_buf;
    p_end = p + b->debug.pc2line_len;
    pc = 0;
    line_num = b->debug.line_num;
    while (p < p_end) {
        op = *p++;
        if (op == 0) {
            uint32_t val;
            ret = get_leb128(&val, p, p_end);
            if (ret < 0)
                goto fail;
            pc += val;
            p += ret;
            ret = get_sleb128(&v, p, p_end);
            if (ret < 0) {
                fail:
                /* should never happen */
                return b->debug.line_num;
            }
            p += ret;
            new_line_num = line_num + v;
        } else {
            op -= PC2LINE_OP_FIRST;
            pc += (op / PC2LINE_RANGE);
            new_line_num = line_num + (op % PC2LINE_RANGE) + PC2LINE_BASE;
        }
        if (pc_value < pc)
            return line_num;
        line_num = new_line_num;
    }
    return line_num;
}

/* in order to avoid executing arbitrary code during the stack trace
   generation, we only look at simple 'name' properties containing a
   string. */
static const char *get_func_name(JSContext *ctx, JSValueConst func)
{
    JSProperty *pr;
    JSShapeProperty *prs;
    JSValueConst val;

    if (JS_VALUE_GET_TAG(func) != JS_TAG_OBJECT)
        return NULL;
    prs = find_own_property(&pr, JS_VALUE_GET_OBJ(func), JS_ATOM_name);
    if (!prs)
        return NULL;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return NULL;
    val = pr->u.value;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_STRING)
        return NULL;
    return JS_ToCString(ctx, val);
}

/* if filename != NULL, an additional level is added with the filename
   and line number information (used for parse error). */
void build_backtrace(JSContext *ctx, JSValueConst error_obj,
                            const char *filename, int line_num,
                            int backtrace_flags)
{
    JSStackFrame *sf;
    JSValue str;
    DynBuf dbuf;
    const char *func_name_str;
    const char *str1;
    JSObject *p;
    BOOL backtrace_barrier;

    js_dbuf_init(ctx, &dbuf);
    if (filename) {
        dbuf_printf(&dbuf, "    at %s", filename);
        if (line_num != -1)
            dbuf_printf(&dbuf, ":%d", line_num);
        dbuf_putc(&dbuf, '\n');
        str = JS_NewString(ctx, filename);
        JS_DefinePropertyValue(ctx, error_obj, JS_ATOM_fileName, str,
                               JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
        JS_DefinePropertyValue(ctx, error_obj, JS_ATOM_lineNumber, JS_NewInt32(ctx, line_num),
                               JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
        if (backtrace_flags & JS_BACKTRACE_FLAG_SINGLE_LEVEL)
            goto done;
    }
    for(sf = ctx->rt->current_stack_frame; sf != NULL; sf = sf->prev_frame) {
        if (backtrace_flags & JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL) {
            backtrace_flags &= ~JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL;
            continue;
        }
        func_name_str = get_func_name(ctx, sf->cur_func);
        if (!func_name_str || func_name_str[0] == '\0')
            str1 = "<anonymous>";
        else
            str1 = func_name_str;
        dbuf_printf(&dbuf, "    at %s", str1);
        JS_FreeCString(ctx, func_name_str);

        p = JS_VALUE_GET_OBJ(sf->cur_func);
        backtrace_barrier = FALSE;
        if (js_class_has_bytecode(p->class_id)) {
            JSFunctionBytecode *b;
            const char *atom_str;
            int line_num1;

            b = p->u.func.function_bytecode;
            backtrace_barrier = b->backtrace_barrier;
            if (b->has_debug) {
                line_num1 = find_line_num(ctx, b,
                                          sf->cur_pc - b->byte_code_buf - 1);
                atom_str = JS_AtomToCString(ctx, b->debug.filename);
                dbuf_printf(&dbuf, " (%s",
                            atom_str ? atom_str : "<null>");
                JS_FreeCString(ctx, atom_str);
                if (line_num1 != -1)
                    dbuf_printf(&dbuf, ":%d", line_num1);
                dbuf_putc(&dbuf, ')');
            }
        } else {
            dbuf_printf(&dbuf, " (native)");
        }
        dbuf_putc(&dbuf, '\n');
        /* stop backtrace if JS_EVAL_FLAG_BACKTRACE_BARRIER was used */
        if (backtrace_barrier)
            break;
    }
    done:
    dbuf_putc(&dbuf, '\0');
    if (dbuf_error(&dbuf))
        str = JS_NULL;
    else
        str = JS_NewString(ctx, (char *)dbuf.buf);
    dbuf_free(&dbuf);
    JS_DefinePropertyValue(ctx, error_obj, JS_ATOM_stack, str,
                           JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
}

/* Note: it is important that no exception is returned by this function */
 BOOL is_backtrace_needed(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id != JS_CLASS_ERROR)
        return FALSE;
    if (find_own_property1(p, JS_ATOM_stack))
        return FALSE;
    return TRUE;
}

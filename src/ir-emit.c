//
// Created by benpeng.jiang on 2021/7/2.
//

#include "ir-emit.h"


__exception int emit_push_const(JSParseState *s, JSValueConst val,
                                       BOOL as_atom)
{
    int idx;

    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING && as_atom) {
        JSAtom atom;
        /* warning: JS_NewAtomStr frees the string value */
        JS_DupValue(s->ctx, val);
        atom = JS_NewAtomStr(s->ctx, JS_VALUE_GET_STRING(val));
        if (atom != JS_ATOM_NULL && !__JS_AtomIsTaggedInt(atom)) {
            emit_op(s, OP_push_atom_value);
            emit_u32(s, atom);
            return 0;
        }
    }

    idx = cpool_add(s, JS_DupValue(s->ctx, val));
    if (idx < 0)
        return -1;
    emit_op(s, OP_push_const);
    emit_u32(s, idx);
    return 0;
}
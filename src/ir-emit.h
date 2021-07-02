//
// Created by benpeng.jiang on 2021/7/2.
//

#ifndef LOX_JS_QJS_EMIT_H
#define LOX_JS_QJS_EMIT_H
#include "front.h"

 void emit_u8(JSParseState *s, uint8_t val);
void emit_u16(JSParseState *s, uint16_t val);
void emit_u32(JSParseState *s, uint32_t val);
void emit_op(JSParseState *s, uint8_t val);
void emit_atom(JSParseState *s, JSAtom name);
int cpool_add(JSParseState *s, JSValue val);

__exception int emit_push_const(JSParseState *s, JSValueConst val,
                                BOOL as_atom);
#endif //LOX_JS_QJS_EMIT_H

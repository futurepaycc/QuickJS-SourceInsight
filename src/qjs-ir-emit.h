//
// Created by benpeng.jiang on 2021/7/2.
//

#ifndef LOX_JS_QJS_EMIT_H
#define LOX_JS_QJS_EMIT_H

#include "front-token.h"

BOOL js_is_live_code(JSParseState *s);

void emit_u8(JSParseState *s, uint8_t val);

void emit_u16(JSParseState *s, uint16_t val);

void emit_u32(JSParseState *s, uint32_t val);

void emit_op(JSParseState *s, uint8_t val);

void emit_atom(JSParseState *s, JSAtom name);

int update_label(JSFunctionDef *s, int label, int delta);

int new_label_fd(JSFunctionDef *fd, int label);

int new_label(JSParseState *s);

int emit_label(JSParseState *s, int label);

int emit_goto(JSParseState *s, int opcode, int label);

int cpool_add(JSParseState *s, JSValue val);

__exception int emit_push_const(JSParseState *s, JSValueConst val,
                                BOOL as_atom);

void emit_return(JSParseState *s, BOOL hasval);

typedef enum {
    PUT_LVALUE_NOKEEP, /* [depth] v -> */
    PUT_LVALUE_NOKEEP_DEPTH, /* [depth] v -> , keep depth (currently
                                just disable optimizations) */
    PUT_LVALUE_KEEP_TOP,  /* [depth] v -> v */
    PUT_LVALUE_KEEP_SECOND, /* [depth] v0 v -> v0 */
    PUT_LVALUE_NOKEEP_BOTTOM, /* v [depth] -> */
} PutLValueEnum;

 void put_lvalue(JSParseState *s, int opcode, int scope,
                       JSAtom name, int label, PutLValueEnum special,
                       BOOL is_let);

 __exception int get_lvalue(JSParseState *s, int *popcode, int *pscope,
                                  JSAtom *pname, int *plabel, int *pdepth, BOOL keep,
                                  int tok);

static inline int get_prev_opcode(JSFunctionDef *fd) {
    if (fd->last_opcode_pos < 0)
        return OP_invalid;
    else
        return fd->byte_code.buf[fd->last_opcode_pos];
};

void emit_class_field_init(JSParseState *s);
#endif //LOX_JS_QJS_EMIT_H

//
// Created by benpeng.jiang on 2021/6/18.
//

#ifndef LOX_JS_ATOM_H
#define LOX_JS_ATOM_H

#include "qjs-api.h"
#include "qjs-string.h"
#include "qjs-number.h"


__maybe_unused void JS_DumpString(JSRuntime *rt,
                                  const JSString *p);

__maybe_unused void JS_DumpAtoms(JSRuntime *rt);

JSValue JS_NewSymbol(JSContext *ctx, JSString *p, int atom_type);

 JSAtom __JS_NewAtomInit(JSRuntime *rt, const char *str, int len,
                               int atom_type);

 int JS_InitAtoms(JSRuntime *rt);

JSAtom js_symbol_to_atom(JSContext *ctx, JSValue val);

JSValue JS_NewSymbolFromAtom(JSContext *ctx, JSAtom descr,
                             int atom_type);

JSAtom js_atom_concat_num(JSContext *ctx, JSAtom name, uint32_t n);

JSAtom js_atom_concat_str(JSContext *ctx, JSAtom name, const char *str1);

const char *JS_AtomGetStr(JSContext *ctx, char *buf, int buf_size, JSAtom atom);

const char *JS_AtomGetStrRT(JSRuntime *rt, char *buf, int buf_size,
                            JSAtom atom);

/* JSAtom support */

#define JS_ATOM_TAG_INT (1U << 31)
#define JS_ATOM_MAX_INT (JS_ATOM_TAG_INT - 1)
#define JS_ATOM_MAX     ((1U << 30) - 1)

/* return the max count from the hash size */
#define JS_ATOM_COUNT_RESIZE(n) ((n) * 2)

static inline BOOL __JS_AtomIsConst(JSAtom v) {
#if defined(DUMP_LEAKS) && DUMP_LEAKS > 1
    return (int32_t)v <= 0;
#else
    return (int32_t) v < JS_ATOM_END;
#endif
}

BOOL __JS_AtomIsTaggedInt(JSAtom v);

BOOL JS_IsEmptyString(JSValueConst v);

static inline JSAtom __JS_AtomFromUInt32(uint32_t v) {
    return v | JS_ATOM_TAG_INT;
}

static inline uint32_t __JS_AtomToUInt32(JSAtom atom) {
    return atom & ~JS_ATOM_TAG_INT;
}

static inline int is_num(int c) {
    return c >= '0' && c <= '9';
}

/* return TRUE if the string is a number n with 0 <= n <= 2^32-1 */
static inline BOOL is_num_string(uint32_t *pval, const JSString *p) {
    uint32_t n;
    uint64_t n64;
    int c, i, len;

    len = p->len;
    if (len == 0 || len > 10)
        return FALSE;
    if (p->is_wide_char)
        c = p->u.str16[0];
    else
        c = p->u.str8[0];
    if (is_num(c)) {
        if (c == '0') {
            if (len != 1)
                return FALSE;
            n = 0;
        } else {
            n = c - '0';
            for (i = 1; i < len; i++) {
                if (p->is_wide_char)
                    c = p->u.str16[i];
                else
                    c = p->u.str8[i];
                if (!is_num(c))
                    return FALSE;
                n64 = (uint64_t) n * 10 + (c - '0');
                if ((n64 >> 32) != 0)
                    return FALSE;
                n = n64;
            }
        }
        *pval = n;
        return TRUE;
    } else {
        return FALSE;
    }
}


JSAtom JS_DupAtomRT(JSRuntime *rt, JSAtom v);

JSAtomKindEnum JS_AtomGetKind(JSContext *ctx, JSAtom v);

BOOL JS_AtomIsArrayIndex(JSContext *ctx, uint32_t *pval, JSAtom atom);

JSAtom js_get_atom_index(JSRuntime *rt, JSAtomStruct *p);

/* return -1 if exception or TRUE/FALSE */
int JS_AtomIsNumericIndex(JSContext *ctx, JSAtom atom);

JSAtom JS_NewAtomStr(JSContext *ctx, JSString *p);

 JSAtom __JS_FindAtom(JSRuntime *rt, const char *str, size_t len,
                            int atom_type);

 JSAtom JS_NewAtomInt64(JSContext *ctx, int64_t n);

 BOOL JS_AtomSymbolHasDescription(JSContext *ctx, JSAtom v);


 JSValue JS_AtomIsNumericIndex1(JSContext *ctx, JSAtom atom);

BOOL JS_AtomIsString(JSContext *ctx, JSAtom v);

 __maybe_unused void print_atom(JSContext *ctx, JSAtom atom);


#endif //LOX_JS_ATOM_H

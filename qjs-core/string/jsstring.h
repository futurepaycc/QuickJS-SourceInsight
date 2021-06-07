//
// Created by benpeng.jiang on 2021/5/21.
//

#ifndef QJS_JSSTRING_H
#define QJS_JSSTRING_H

#include <qjs-runtime.h>
#include "cutils.h"
#include "quickjs-atom.h"
#include "quickjs.h"


enum {
    JS_ATOM_TYPE_STRING = 1,
    JS_ATOM_TYPE_GLOBAL_SYMBOL,
    JS_ATOM_TYPE_SYMBOL,
    JS_ATOM_TYPE_PRIVATE,
};

enum {
    JS_ATOM_HASH_SYMBOL,
    JS_ATOM_HASH_PRIVATE,
};

typedef enum {
    JS_ATOM_KIND_STRING,
    JS_ATOM_KIND_SYMBOL,
    JS_ATOM_KIND_PRIVATE,
} JSAtomKindEnum;

#define JS_ATOM_HASH_MASK  ((1 << 30) - 1)


/* atom support */
#define JS_ATOM_NULL 0

enum {
    __JS_ATOM_NULL = JS_ATOM_NULL,
#define DEF(name, str) JS_ATOM_ ## name,
#include "quickjs-atom.h"
#undef DEF
    JS_ATOM_END,
};
#define JS_ATOM_LAST_KEYWORD JS_ATOM_super
#define JS_ATOM_LAST_STRICT_KEYWORD JS_ATOM_yield

static const char js_atom_init[] =
#define DEF(name, str) str "\0"
#include "quickjs-atom.h"
#undef DEF
;
int
JS_InitAtoms(JSRuntime *rt);

void JS_DumpString(JSRuntime *rt,
                   const JSString *p);

void test_dump_str(JSContext *ctx);

#define ATOM_GET_STR_BUF_SIZE 64


const char *JS_AtomGetStrRT(JSRuntime *rt, char *buf, int buf_size,
                            JSAtom atom);
const char *JS_AtomGetStr(JSContext *ctx, char *buf, int buf_size, JSAtom atom);
#endif //QJS_JSSTRING_H

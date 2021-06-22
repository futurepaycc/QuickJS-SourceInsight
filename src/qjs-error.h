//
// Created by benpeng.jiang on 2021/6/18.
//

#ifndef LOX_JS_ERROR_H
#define LOX_JS_ERROR_H
#include "qjs-api.h"
#include "qjs-atom.h"

const char *JS_AtomGetStr(JSContext *ctx, char *buf, int buf_size, JSAtom atom);

/* never use it directly */
static JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowTypeErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowTypeError(ctx, fmt,
                             JS_AtomGetStr(ctx, buf, sizeof(buf), atom));
}

/* never use it directly */
static JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowSyntaxErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowSyntaxError(ctx, fmt,
                               JS_AtomGetStr(ctx, buf, sizeof(buf), atom));
}
/* %s is replaced by 'atom'. The macro is used so that gcc can check
    the format string. */
#define JS_ThrowTypeErrorAtom(ctx, fmt, atom) __JS_ThrowTypeErrorAtom(ctx, atom, fmt, "")
#define JS_ThrowSyntaxErrorAtom(ctx, fmt, atom) __JS_ThrowSyntaxErrorAtom(ctx, atom, fmt, "")

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...);
void JS_SetUncatchableError(JSContext *ctx, JSValueConst val, BOOL flag);
 JSValue JS_ThrowTypeErrorInvalidClass(JSContext *ctx, int class_id);
int __attribute__((format(printf, 3, 4))) JS_ThrowTypeErrorOrFalse(JSContext *ctx, int flags, const char *fmt, ...);
JSValue JS_ThrowStackOverflow(JSContext *ctx);
JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);
int JS_ThrowTypeErrorReadOnly(JSContext *ctx, int flags, JSAtom atom);
JSValue JS_ThrowSyntaxErrorVarRedeclaration(JSContext *ctx, JSAtom prop);
JSValue JS_ThrowReferenceErrorUninitialized(JSContext *ctx, JSAtom name);
JSValue JS_ThrowReferenceErrorNotDefined(JSContext *ctx, JSAtom name);
JSValue JS_ThrowReferenceErrorUninitialized2(JSContext *ctx,
                                             JSFunctionBytecode *b,
                                             int idx, BOOL is_ref);

JSValue JS_ThrowError2(JSContext *ctx, JSErrorEnum error_num,
                       const char *fmt, va_list ap, BOOL add_backtrace);

 JSValue JS_ThrowError(JSContext *ctx, JSErrorEnum error_num,
                             const char *fmt, va_list ap);

 JSValue JS_ThrowTypeErrorRevokedProxy(JSContext *ctx);
JSValue JS_ThrowOutOfMemory(JSContext *ctx);
JSValue JS_ThrowTypeErrorRevokedProxy(JSContext *ctx);
#endif //LOX_JS_ERROR_H

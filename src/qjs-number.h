//
// Created by benpeng.jiang on 2021/6/18.
//

#ifndef LOX_JS_NUMBER_H
#define LOX_JS_NUMBER_H

#include "qjs-api.h"
#include "qjs-internal.h"
#include "qjs-string.h"
#include "libregexp.h"
#include "libbf.h"


#define ATOD_INT_ONLY        (1 << 0)
/* accept Oo and Ob prefixes in addition to 0x prefix if radix = 0 */
#define ATOD_ACCEPT_BIN_OCT  (1 << 2)
/* accept O prefix as octal if radix == 0 and properly formed (Annex B) */
#define ATOD_ACCEPT_LEGACY_OCTAL  (1 << 4)
/* accept _ between digits as a digit separator */
#define ATOD_ACCEPT_UNDERSCORES  (1 << 5)
/* allow a suffix to override the type */
#define ATOD_ACCEPT_SUFFIX    (1 << 6)
/* default type */
#define ATOD_TYPE_MASK        (3 << 7)
#define ATOD_TYPE_FLOAT64     (0 << 7)
#define ATOD_TYPE_BIG_INT     (1 << 7)
#define ATOD_TYPE_BIG_FLOAT   (2 << 7)
#define ATOD_TYPE_BIG_DECIMAL (3 << 7)
/* assume bigint mode: floats are parsed as integers if no decimal
   point nor exponent */
#define ATOD_MODE_BIGINT      (1 << 9)
/* accept -0x1 */
#define ATOD_ACCEPT_PREFIX_AFTER_SIGN (1 << 10)

JSValue JS_ToNumber(JSContext *ctx, JSValueConst val);
JSValue JS_ToNumericFree(JSContext *ctx, JSValue val);
JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val);

/* return an exception in case of memory error. Return JS_NAN if
   invalid syntax */
#ifdef CONFIG_BIGNUM

JSValue js_atof2(JSContext *ctx, const char *str, const char **pp,
                 int radix, int flags, slimb_t *pexponent);

#else
JSValue js_atof(JSContext *ctx, const char *str, const char **pp,
                      int radix, int flags);
#endif

#ifdef CONFIG_BIGNUM

JSValue js_atof(JSContext *ctx, const char *str, const char **pp,
                int radix, int flags);

#endif

 int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);
BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val);
 JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);

BOOL is_math_mode(JSContext *ctx);

int skip_spaces(const char *pc);

JSValue js_string_to_bigfloat(JSContext *ctx, const char *buf,
                              int radix, int flags, slimb_t *pexponent);

JSValue js_string_to_bigint(JSContext *ctx, const char *buf,
                            int radix, int flags, slimb_t *pexponent);

JSValue js_bigfloat_to_string(JSContext *ctx, JSValueConst val);

JSValue js_bigdecimal_to_string1(JSContext *ctx, JSValueConst val,
                                 limb_t prec, int flags);

JSValue js_bigdecimal_to_string(JSContext *ctx, JSValueConst val);

JSValue js_ftoa(JSContext *ctx, JSValueConst val1, int radix,
                limb_t prec, bf_flags_t flags);

JSValue js_bigint_to_string(JSContext *ctx, JSValueConst val);

JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix);

JSValue js_string_to_bigdecimal(JSContext *ctx, const char *buf,
                                int radix, int flags, slimb_t *pexponent);

#endif //LOX_JS_NUMBER_H

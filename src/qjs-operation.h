//
// Created by benpeng.jiang on 2021/6/20.
//

#ifndef LOX_JS_QJS_OPERATION_H
#define LOX_JS_QJS_OPERATION_H

#include "qjs-api.h"
#include "ecma-type-conversion.h"
#include "libbf.h"

 JSValue js_number_isSafeInteger(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv);

 no_inline __exception int js_eq_slow(JSContext *ctx, JSValue *sp,
                                            BOOL is_neq);

 no_inline int js_relational_slow(JSContext *ctx, JSValue *sp,
                                        OPCodeEnum op);

 int js_compare_bigdecimal(JSContext *ctx, OPCodeEnum op,
                                 JSValue op1, JSValue op2);
 no_inline int js_shr_slow(JSContext *ctx, JSValue *sp);

no_inline __exception int js_binary_logic_slow(JSContext *ctx,
                                               JSValue *sp,
                                               OPCodeEnum op);

no_inline int js_not_slow(JSContext *ctx, JSValue *sp);

__exception int js_post_inc_slow(JSContext *ctx,
                                 JSValue *sp, OPCodeEnum op);

no_inline __exception int js_binary_arith_slow(JSContext *ctx, JSValue *sp,
                                               OPCodeEnum op);

no_inline __exception int js_add_slow(JSContext *ctx, JSValue *sp);

no_inline __exception int js_unary_arith_slow(JSContext *ctx,
                                              JSValue *sp,
                                              OPCodeEnum op);

no_inline int js_strict_eq_slow(JSContext *ctx, JSValue *sp,
                                       BOOL is_neq);
__exception int js_operator_in(JSContext *ctx, JSValue *sp);
 __exception int js_operator_instanceof(JSContext *ctx, JSValue *sp);
 __exception int js_operator_typeof(JSContext *ctx, JSValueConst op1);
 __exception int js_operator_delete(JSContext *ctx, JSValue *sp);
 __exception int js_has_unscopable(JSContext *ctx, JSValueConst obj,
                                         JSAtom atom);

JSValue JS_NewBigInt64_1(JSContext *ctx, int64_t v);
#ifdef CONFIG_BIGNUM
JSBigFloat *js_new_bf(JSContext *ctx);
#endif

/* must be kept in sync with JSOverloadableOperatorEnum */
/* XXX: use atoms ? */
static const char js_overloadable_operator_names[JS_OVOP_COUNT][4] = {
        "+",
        "-",
        "*",
        "/",
        "%",
        "**",
        "|",
        "&",
        "^",
        "<<",
        ">>",
        ">>>",
        "==",
        "<",
        "pos",
        "neg",
        "++",
        "--",
        "~",
};
 JSValue throw_bf_exception(JSContext *ctx, int status);
 bf_t *JS_ToBigIntFree(JSContext *ctx, bf_t *buf, JSValue val);
 JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val);
 double js_pow(double a, double b);
 int JS_ToUint32Free(JSContext *ctx, uint32_t *pres, JSValue val);
 int js_unary_arith_bigint(JSContext *ctx,
                                 JSValue *pres, OPCodeEnum op, JSValue op1);

 int js_binary_arith_bigint(JSContext *ctx, OPCodeEnum op,
                                  JSValue *pres, JSValue op1, JSValue op2);

/* Note: also used for bigint */
 int js_compare_bigfloat(JSContext *ctx, OPCodeEnum op,
                               JSValue op1, JSValue op2);

 int js_unary_arith_bigfloat(JSContext *ctx,
                                   JSValue *pres, OPCodeEnum op, JSValue op1);

 int js_binary_arith_bigfloat(JSContext *ctx, OPCodeEnum op,
                                    JSValue *pres, JSValue op1, JSValue op2);

 JSValue js_mul_pow10_to_float64(JSContext *ctx, const bf_t *a,
                                       int64_t exponent);

 no_inline int js_mul_pow10(JSContext *ctx, JSValue *sp);

 __maybe_unused JSValue JS_ToBigIntValueFree(JSContext *ctx, JSValue val);

 int js_unary_arith_bigdecimal(JSContext *ctx,
                                     JSValue *pres, OPCodeEnum op, JSValue op1);

 int js_binary_arith_bigdecimal(JSContext *ctx, OPCodeEnum op,
                                      JSValue *pres, JSValue op1, JSValue op2);
 #endif //LOX_JS_QJS_OPERATION_H


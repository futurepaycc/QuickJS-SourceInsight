//
// Created by benpeng.jiang on 2021/6/18.
//

#ifndef LOX_JS_CONSTANTS_H
#define LOX_JS_CONSTANTS_H

#ifdef CONFIG_BIGNUM
#include "libbf.h"
#endif

#define OPTIMIZE         1
#define SHORT_OPCODES    1
#if defined(EMSCRIPTEN)
#define DIRECT_DISPATCH  0
#else
#define DIRECT_DISPATCH  1
#endif

#if defined(__APPLE__)
#define MALLOC_OVERHEAD  0
#else
#define MALLOC_OVERHEAD  8
#endif

#if !defined(_WIN32)
/* define it if printf uses the RNDN rounding mode instead of RNDNA */
#define CONFIG_PRINTF_RNDN
#endif

/* define to include Atomics.* operations which depend on the OS
   threads */
#if !defined(EMSCRIPTEN)
#define CONFIG_ATOMICS
#endif

#if !defined(EMSCRIPTEN)
/* enable stack limitation */
#define CONFIG_STACK_CHECK
#endif

#define FUNC_RET_AWAIT      0
#define FUNC_RET_YIELD      1
#define FUNC_RET_YIELD_STAR 2

#define OP_DEFINE_METHOD_METHOD 0
#define OP_DEFINE_METHOD_GETTER 1
#define OP_DEFINE_METHOD_SETTER 2
#define OP_DEFINE_METHOD_ENUMERABLE 4

#define JS_THROW_VAR_RO             0
#define JS_THROW_VAR_REDECL         1
#define JS_THROW_VAR_UNINITIALIZED  2
#define JS_THROW_ERROR_DELETE_SUPER   3
#define JS_THROW_ERROR_ITERATOR_THROW 4

#define JS_CALL_FLAG_COPY_ARGV   (1 << 1)
#define JS_CALL_FLAG_GENERATOR   (1 << 2)

#define ATOM_GET_STR_BUF_SIZE 64


#define JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL (1 << 0)
/* only taken into account if filename is provided */
#define JS_BACKTRACE_FLAG_SINGLE_LEVEL     (1 << 1)

#define HINT_STRING  0
#define HINT_NUMBER  1
#define HINT_NONE    2
/* don't try Symbol.toPrimitive */
#define HINT_FORCE_ORDINARY (1 << 4)

/* radix != 10 is only supported with flags = JS_DTOA_VAR_FORMAT */
/* use as many digits as necessary */
#define JS_DTOA_VAR_FORMAT   (0 << 0)
/* use n_digits significant digits (1 <= n_digits <= 101) */
#define JS_DTOA_FIXED_FORMAT (1 << 0)
/* force fractional format: [-]dd.dd with n_digits fractional digits */
#define JS_DTOA_FRAC_FORMAT  (2 << 0)
/* force exponential notation either in fixed or variable format */
#define JS_DTOA_FORCE_EXP    (1 << 2)

/* maximum buffer size for js_dtoa */
#define JS_DTOA_BUF_SIZE 128

#define MAX_SAFE_INTEGER (((int64_t)1 << 53) - 1)

#define JS_DEFINE_CLASS_HAS_HERITAGE     (1 << 0)

#define MAGIC_SET (1 << 0)
#define MAGIC_WEAK (1 << 1)

#define GLOBAL_VAR_OFFSET 0x40000000
#define ARGUMENT_VAR_OFFSET 0x20000000

#define DEFINE_GLOBAL_LEX_VAR (1 << 7)
#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)


#define special_every    0
#define special_some     1
#define special_forEach  2
#define special_map      3
#define special_filter   4
#define special_TA       8


#define special_reduce       0
#define special_reduceRight  1


#endif //LOX_JS_CONSTANTS_H

cmake_minimum_required(VERSION 3.19)
project(Lox_JS C)

set(CMAKE_C_STANDARD 11)

set(LIB_FILES src/cutils.c src/quickjs.c src/quickjs-libc.c src/libregexp.c src/libunicode.c src/libbf.c)

set(BITECODE_FILES src/qjscalc.c src/repl.c)
set(QJS_SOURCE_FILES src/qjs.c ${LIB_FILES} ${BITECODE_FILES} src/qjs-vm.c src/qjs-vm.h src/qjs-api.h src/qjs-internal.h src/qjs-constants.h src/qjs-interrupts.c src/qjs-interrupts.h src/qjs-error.c src/qjs-error.h src/qjs-backtrace.c src/qjs-backtrace.h src/qjs-atom.c src/qjs-atom.h src/qjs-string.c src/qjs-string.h src/qjs-number.h src/qjs-number.c src/ecma-type-conversion.c src/ecma-type-conversion.h src/qjs-double.c src/qjs-double.h src/qjs-symbol.c src/qjs-symbol.h src/qjs-varrefs.c src/qjs-varrefs.h src/qjs-gc.c src/qjs-gc.h src/qjs-opcode.h src/qjs-function.c src/qjs-function.h src/ecma-collections.c src/ecma-collections.h src/ecma-object.c src/ecma-object.h src/qjs-async.c src/qjs-async.h src/qjs-runtime.h src/qjs-closure.c src/qjs-closure.h src/ecma-modules.c src/ecma-modules.h src/ecma-class.c src/ecma-class.h src/ecma-function.c src/ecma-function.h src/ecma.h src/ecma-global.c src/ecma-global.h src/ecma-regexp.c src/ecma-regexp.h src/ecma-iterator.c src/ecma-iterator.h src/ecma-array.c src/ecma-array.h src/ecma-arraybuffer.c src/ecma-arraybuffer.h src/qjs-operation.c src/qjs-operation.h src/ecma-bignumber.c src/ecma-bignumber.h src/ecma-symbol.c src/ecma-symbol.h src/front-parser-stmt-decl.c src/front-parser-stmt-decl.h src/front-token.c src/front-token.h src/qjs-front.h src/front-json.c src/front-json.h src/ecma-typed-array.c src/ecma-typed-array.h src/qjs-stringbuffer.c src/qjs-stringbuffer.h src/front-lexer.c src/front-lexer.h src/qjs-ir-emit.c src/qjs-ir-emit.h src/qjs-ir.h src/front-parser-expr.c src/front-parser-expr.h src/qjs-resolver.c src/qjs-resolver.h)
add_executable(qjs_CLion ${QJS_SOURCE_FILES})


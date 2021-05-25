//
// Created by benpeng.jiang on 2021/5/24.
//

#ifndef TUTORIAL_QJS_PORT_H
#define TUTORIAL_QJS_PORT_H
#include "qjs-compiler.h"

/**
 * Error codes
 */
typedef enum
{
    ERR_OUT_OF_MEMORY = 10,
    ERR_REF_COUNT_LIMIT = 12,
    ERR_DISABLED_BYTE_CODE = 13,
    ERR_UNTERMINATED_GC_LOOPS = 14,
    ERR_FAILED_INTERNAL_ASSERTION = 120
} jerry_fatal_code_t;

void jerry_port_fatal (jerry_fatal_code_t code);

/**
 * Jerry log levels. The levels are in severity order
 * where the most serious levels come first.
 */
typedef enum
{
    JERRY_LOG_LEVEL_ERROR,    /**< the engine will terminate after the message is printed */
    JERRY_LOG_LEVEL_WARNING,  /**< a request is aborted, but the engine continues its operation */
    JERRY_LOG_LEVEL_DEBUG,    /**< debug messages from the engine, low volume */
    JERRY_LOG_LEVEL_TRACE     /**< detailed info about engine internals, potentially high volume */
} jerry_log_level_t;

void jerry_port_log (jerry_log_level_t level, const char *format, ...);


#endif //TUTORIAL_QJS_PORT_H

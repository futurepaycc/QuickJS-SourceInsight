//
// Created by benpeng.jiang on 2021/5/24.
//
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

#include "qjs-port.h"

void
jerry_port_log (jerry_log_level_t level, /**< log level */
                const char *format, /**< format string */
                ...)  /**< parameters */
{
    (void) level; /* ignore log level */

    va_list args;
    va_start (args, format);
    vfprintf (stderr, format, args);
    va_end (args);
} /* jerry_port_log */

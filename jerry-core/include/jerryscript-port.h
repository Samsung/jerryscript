/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef JERRYSCRIPT_PORT_H
#define JERRYSCRIPT_PORT_H

#include "jerryscript-compiler.h"
#include "jerryscript-core.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** \addtogroup jerry_port Jerry engine port
 * @{
 */

/*
 * Termination Port API
 *
 * Note:
 *      It is questionable whether a library should be able to terminate an
 *      application. However, as of now, we only have the concept of completion
 *      code around jerry_parse and jerry_run. Most of the other API functions
 *      have no way of signaling an error. So, we keep the termination approach
 *      with this port function.
 */

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

/**
 * Signal the port that jerry experienced a fatal failure from which it cannot
 * recover.
 *
 * @param code gives the cause of the error.
 *
 * Note:
 *      Jerry expects the function not to return.
 *
 * Example: a libc-based port may implement this with exit() or abort(), or both.
 */
void JERRY_ATTR_NORETURN jerry_port_fatal (jerry_fatal_code_t code);

/*
 *  I/O Port API
 */

/**
 * Jerry log levels. The levels are in severity order
 * where the most serious levels come first.
 */
typedef enum
{
  JERRY_LOG_LEVEL_ERROR, /**< the engine will terminate after the message is printed */
  JERRY_LOG_LEVEL_WARNING, /**< a request is aborted, but the engine continues its operation */
  JERRY_LOG_LEVEL_DEBUG, /**< debug messages from the engine, low volume */
  JERRY_LOG_LEVEL_TRACE /**< detailed info about engine internals, potentially high volume */
} jerry_log_level_t;

/**
 * Display or log a debug/error message. The function should implement a printf-like
 * interface, where the first argument specifies the log level
 * and the second argument specifies a format string on how to stringify the rest
 * of the parameter list.
 *
 * This function is only called with messages coming from the jerry engine as
 * the result of some abnormal operation or describing its internal operations
 * (e.g., data structure dumps or tracing info).
 *
 * It should be the port that decides whether error and debug messages are logged to
 * the console, or saved to a database or to a file.
 *
 * Example: a libc-based port may implement this with vfprintf(stderr) or
 * vfprintf(logfile), or both, depending on log level.
 *
 * Note:
 *      This port function is called by jerry-core when JERRY_LOGGING is
 *      enabled. It is also common practice though to use this function in
 *      application code.
 */
void JERRY_ATTR_FORMAT (printf, 2, 3) jerry_port_log (jerry_log_level_t level, const char *format, ...);

/*
 * Date Port API
 */

/**
 * Get local time zone adjustment, in milliseconds, for the given timestamp.
 * The timestamp can be specified in either UTC or local time, depending on
 * the value of is_utc. Adding the value returned from this function to
 * a timestamp in UTC time should result in local time for the current time
 * zone, and subtracting it from a timestamp in local time should result in
 * UTC time.
 *
 * Ideally, this function should satisfy the stipulations applied to LocalTZA
 * in section 20.3.1.7 of the ECMAScript version 9.0 spec.
 *
 * See Also:
 *          ECMA-262 v9, 20.3.1.7
 *
 * Note:
 *      This port function is called by jerry-core when
 *      JERRY_BUILTIN_DATE is defined to 1. Otherwise this function is
 *      not used.
 *
 * @param unix_ms The unix timestamp we want an offset for, given in
 *                millisecond precision (could be now, in the future,
 *                or in the past). As with all unix timestamps, 0 refers to
 *                1970-01-01, a day is exactly 86 400 000 milliseconds, and
 *                leap seconds cause the same second to occur twice.
 * @param is_utc Is the given timestamp in UTC time? If false, it is in local
 *               time.
 *
 * @return milliseconds between local time and UTC for the given timestamp,
 *         if available
 *.        0 if not available / we are in UTC.
 */
double jerry_port_get_local_time_zone_adjustment (double unix_ms, bool is_utc);

/**
 * Get system time
 *
 * Note:
 *      This port function is called by jerry-core when
 *      JERRY_BUILTIN_DATE is defined to 1. It is also common practice
 *      in application code to use this function for the initialization of the
 *      random number generator.
 *
 * @return milliseconds since Unix epoch
 */
double jerry_port_get_current_time (void);

/**
 * Get the current context of the engine. Each port should provide its own
 * implementation of this interface.
 *
 * Note:
 *      This port function is called by jerry-core when
 *      JERRY_EXTERNAL_CONTEXT is enabled. Otherwise this function is not
 *      used.
 *
 * @return the pointer to the engine context.
 */
struct jerry_context_t *jerry_port_get_current_context (void);

/**
 * Makes the process sleep for a given time.
 *
 * Note:
 *      This port function is called by jerry-core when JERRY_DEBUGGER is
 *      enabled (set to 1). Otherwise this function is not used.
 *
 * @param sleep_time milliseconds to sleep.
 */
void jerry_port_sleep (uint32_t sleep_time);

/**
 * Print a single character.
 *
 * Note:
 *      This port function is here so the jerry-ext components would have
 *      a common way to print out information.
 *      If possible do not use from the jerry-core.
 *
 * @param c the character to print.
 */
void jerry_port_print_char (char c);

/**
 * Open a source file and read its contents into a buffer.
 *
 * Note:
 *      This port function is called by jerry-core when JERRY_MODULE_SYSTEM
 *      is enabled. The path is specified in the import statement's 'from "..."'
 *      section.
 *
 * @param file_name_p Path that points to the EcmaScript file in the
 *                    filesystem.
 * @param out_size_p The opened file's size in bytes.
 *
 * @return the pointer to the buffer which contains the content of the file.
 */
uint8_t *jerry_port_read_source (const char *file_name_p, size_t *out_size_p);

/**
 * Frees the allocated buffer after the contents of the file are not needed
 * anymore.
 *
 * @param buffer_p The pointer the allocated buffer.
 */
void jerry_port_release_source (uint8_t *buffer_p);

/**
 * Normalize a file path string.
 *
 * Note:
 *      This port function is called by jerry-core when JERRY_MODULE_SYSTEM
 *      is enabled. The normalized path is used to uniquely identify modules.
 *
 * @param in_path_p Input path as a zero terminated string.
 * @param out_buf_p Pointer to the output buffer where the normalized path should be written.
 * @param out_buf_size Size of the output buffer.
 * @param base_file_p A file path that 'in_path_p' is relative to, usually the current module file.
 *                    A NULL value represents that 'in_path_p' is relative to the current working directory.
 *
 * @return length of the string written to the output buffer
 *         zero, if the buffer was not sufficient or an error occured
 */
size_t jerry_port_normalize_path (const char *in_path_p, char *out_buf_p, size_t out_buf_size, char *base_file_p);

/**
 * Get the module object of a native module.
 *
 * Note:
 *      This port function is called by jerry-core when JERRY_MODULE_SYSTEM
 *      is enabled.
 *
 * @param name String value of the module specifier.
 *
 * @return Undefined, if 'name' is not a native module
 *         jerry_value_t containing the module object, otherwise
 */
jerry_value_t jerry_port_get_native_module (jerry_value_t name);

/**
 * HostPromiseRejectionTracker operations
 */
typedef enum
{
  JERRY_PROMISE_REJECTION_OPERATION_REJECT, /**< promise is rejected without any handlers */
  JERRY_PROMISE_REJECTION_OPERATION_HANDLE, /**< handler is added to a rejected promise for the first time */
} jerry_promise_rejection_operation_t;

/**
 * Track unhandled promise rejections.
 *
 * Note:
 *      This port function is called by jerry-core when JERRY_BUILTIN_PROMISE
 *      is enabled.
 *
 * @param promise rejected promise
 * @param operation HostPromiseRejectionTracker operation
 */
void jerry_port_track_promise_rejection (const jerry_value_t promise,
                                         const jerry_promise_rejection_operation_t operation);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYSCRIPT_PORT_H */

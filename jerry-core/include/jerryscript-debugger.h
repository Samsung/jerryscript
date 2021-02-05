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

#ifndef JERRYSCRIPT_DEBUGGER_H
#define JERRYSCRIPT_DEBUGGER_H

#include "jerryscript-core.h"
#include "jerryscript-port.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** \addtogroup jerry-debugger Jerry engine interface - Debugger feature
 * @{
 */

/**
 * JerryScript debugger protocol version.
 */
#define JERRY_DEBUGGER_VERSION (9)

/**
 * Types for the client source wait and run method.
 */
typedef enum
{
  JERRY_DEBUGGER_SOURCE_RECEIVE_FAILED = 0, /**< source is not received */
  JERRY_DEBUGGER_SOURCE_RECEIVED = 1, /**< a source has been received */
  JERRY_DEBUGGER_SOURCE_END = 2, /**< the end of the sources signal received */
  JERRY_DEBUGGER_CONTEXT_RESET_RECEIVED, /**< the context reset request has been received */
} jerry_debugger_wait_for_source_status_t;

/**
 * Callback for jerry_debugger_wait_and_run_client_source
 *
 * The callback receives the resource name, source code and a user pointer.
 *
 * @return this value is passed back by jerry_debugger_wait_and_run_client_source
 */
typedef jerry_value_t (*jerry_debugger_wait_for_source_callback_t) (const jerry_char_t *resource_name_p,
                                                                    size_t resource_name_size,
                                                                    const jerry_char_t *source_p,
                                                                    size_t source_size,
                                                                    void *user_p);

/**
 * Engine debugger functions.
 */
bool jerry_debugger_is_connected (void);
void jerry_debugger_stop (void);
void jerry_debugger_continue (void);
void jerry_debugger_stop_at_breakpoint (bool enable_stop_at_breakpoint);
jerry_debugger_wait_for_source_status_t
jerry_debugger_wait_for_client_source (jerry_debugger_wait_for_source_callback_t callback_p,
                                       void *user_p,
                                       jerry_value_t *return_value);
void jerry_debugger_send_output (const jerry_char_t *buffer, jerry_size_t str_size);
void jerry_debugger_send_log (jerry_log_level_t level, const jerry_char_t *buffer, jerry_size_t str_size);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYSCRIPT_DEBUGGER_H */

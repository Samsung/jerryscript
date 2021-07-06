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

#include "config.h"

#ifndef ECMA_ERRORS_H
#define ECMA_ERRORS_H

#if JERRY_ERROR_MESSAGES

extern const char * const ecma_error_value_msg_p;
extern const char * const ecma_error_wrong_args_msg_p;

#if !JERRY_PARSER
extern const char * const ecma_error_parser_not_supported_p;
#endif /* !JERRY_PARSER */

#if !JERRY_BUILTIN_JSON
extern const char * const ecma_error_json_not_supported_p;
#endif /* !JERRY_BUILTIN_JSON */

#if !JERRY_ESNEXT
extern const char * const ecma_error_symbol_not_supported_p;
#endif /* !JERRY_ESNEXT */

#if !JERRY_BUILTIN_PROMISE
extern const char * const ecma_error_promise_not_supported_p;
#endif /* !JERRY_BUILTIN_PROMISE */

#if !JERRY_BUILTIN_TYPEDARRAY
extern const char * const ecma_error_typed_array_not_supported_p;
#endif /* !JERRY_BUILTIN_TYPEDARRAY */

#if !JERRY_BUILTIN_DATAVIEW
extern const char * const ecma_error_data_view_not_supported_p;
#endif /* !JERRY_BUILTIN_DATAVIEW */

#if !JERRY_BUILTIN_BIGINT
extern const char * const ecma_error_bigint_not_supported_p;
#endif /* !JERRY_BUILTIN_BIGINT */

#if !JERRY_BUILTIN_CONTAINER
extern const char * const ecma_error_container_not_supported_p;
#endif /* !JERRY_BUILTIN_CONTAINER */

#if JERRY_MODULE_SYSTEM
extern const char * const ecma_error_not_module_p;
extern const char * const ecma_error_unknown_export_p;
#else /* !JERRY_MODULE_SYSTEM */
extern const char * const ecma_error_module_not_supported_p;
#endif /* JERRY_MODULE_SYSTEM */

extern const char * const ecma_error_callback_is_not_callable;
extern const char * const ecma_error_arraybuffer_is_detached;
extern const char * const ecma_error_cannot_convert_to_object;
extern const char * const ecma_error_class_is_non_configurable;
extern const char * const ecma_error_argument_is_not_an_object;
extern const char * const ecma_error_argument_is_not_a_proxy;
extern const char * const ecma_error_target_is_not_a_constructor;
extern const char * const ecma_error_argument_is_not_an_regexp;
extern const char * const ecma_error_invalid_array_length;
extern const char * const ecma_error_local_variable_is_redeclared;
extern const char * const ecma_error_expected_a_function;

#if JERRY_ESNEXT
extern const char * const ecma_error_class_constructor_new;
extern const char * const ecma_error_let_const_not_initialized;
#endif /* JERRY_ESNEXT */

#endif /* JERRY_ERROR_MESSAGES */

/* snapshot errors */
extern const char * const ecma_error_maximum_snapshot_size;
extern const char * const ecma_error_regular_expression_not_supported;
extern const char * const ecma_error_snapshot_buffer_small;
extern const char * const ecma_error_snapshot_flag_not_supported;
extern const char * const ecma_error_cannot_allocate_memory_literals;
extern const char * const ecma_error_tagged_template_literals;
#endif  /* !ECMA_ERRORS_H */

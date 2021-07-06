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

#include "ecma-errors.h"

#if JERRY_ERROR_MESSAGES

/**
 * Error message, if an argument is has an error flag
 */
const char * const ecma_error_value_msg_p = "Argument cannot be marked as error";

/**
 * Error message, if an argument has a wrong type
 */
const char * const ecma_error_wrong_args_msg_p = "This type of argument is not allowed";

#if !JERRY_PARSER
/**
 * Error message, if parsing is disabled
 */
const char * const ecma_error_parser_not_supported_p = "Source code parsing is disabled";
#endif /* !JERRY_PARSER  */

#if !JERRY_BUILTIN_JSON
/**
 * Error message, if JSON support is disabled
 */
const char * const ecma_error_json_not_supported_p = "JSON support is disabled";
#endif /* !JERRY_BUILTIN_JSON  */

#if !JERRY_ESNEXT
/**
 * Error message, if Symbol support is disabled
 */
const char * const ecma_error_symbol_not_supported_p = "Symbol support is disabled";
#endif /* !JERRY_ESNEXT  */

#if !JERRY_BUILTIN_PROMISE
/**
 * Error message, if Promise support is disabled
 */
const char * const ecma_error_promise_not_supported_p = "Promise support is disabled";
#endif /* !JERRY_BUILTIN_PROMISE  */

#if !JERRY_BUILTIN_TYPEDARRAY
/**
 * Error message, if TypedArray support is disabled
 */
const char * const ecma_error_typed_array_not_supported_p = "TypedArray support is disabled";
#endif /* !JERRY_BUILTIN_TYPEDARRAY  */

#if !JERRY_BUILTIN_DATAVIEW
/**
 * Error message, if DataView support is disabled
 */
const char * const ecma_error_data_view_not_supported_p = "DataView support is disabled";
#endif /* !JERRY_BUILTIN_DATAVIEW  */

#if !JERRY_BUILTIN_BIGINT
/**
 * Error message, if BigInt support is disabled
 */
const char * const ecma_error_bigint_not_supported_p = "BigInt support is disabled";
#endif /* !JERRY_BUILTIN_BIGINT  */

#if !JERRY_BUILTIN_CONTAINER
/**
 * Error message, if Container support is disabled
 */
const char * const ecma_error_container_not_supported_p = "Container support is disabled";
#endif /* JERRY_BUILTIN_CONTAINER  */

#if JERRY_MODULE_SYSTEM
/**
 * Error message, if argument is not a module
 */
const char * const ecma_error_not_module_p = "Argument is not a module";

/**
 * Error message, if a native module export is not found
 */
const char * const ecma_error_unknown_export_p = "Native module export not found";
#else /* !JERRY_MODULE_SYSTEM */
/**
 * Error message, if Module support is disabled
 */
const char * const ecma_error_module_not_supported_p = "Module support is disabled";
#endif /* JERRY_MODULE_SYSTEM */

/**
   * Error message, if callback function is not callable
   */
const char * const ecma_error_callback_is_not_callable = "Callback function is not callable";

/**
 * Error message, if arrayBuffer is detached
 */
const char * const ecma_error_arraybuffer_is_detached = "ArrayBuffer has been detached";

/**
 * Error message, cannot convert undefined or null to object
 */
const char * const ecma_error_cannot_convert_to_object = "Cannot convert undefined or null to object";

/**
 * Error message, prototype property of a class is non-configurable
 */
const char * const ecma_error_class_is_non_configurable = "Prototype property of a class is non-configurable";

/**
 * Error message, argument is not an object
 */
const char * const ecma_error_argument_is_not_an_object = "Argument is not an object";

/**
 * Error message, argument is not a Proxy object
 */
const char * const ecma_error_argument_is_not_a_proxy = "Argument is not a Proxy object";

/**
 * Error message, target is not a constructor
 */
const char * const ecma_error_target_is_not_a_constructor = "Target is not a constructor";

/**
 * Error message, argument is not an regexp
 */
const char * const ecma_error_argument_is_not_an_regexp = "Argument 'this' is not a RegExp object";

/**
 * Error message, invalid array length
 */
const char * const ecma_error_invalid_array_length = "Invalid Array length";

/**
 * Error message, local variable is redeclared
 */
const char * const ecma_error_local_variable_is_redeclared = "Local variable is redeclared";

/**
 * Error message, expected a function
 */
const char * const ecma_error_expected_a_function = "Expected a function";

#if JERRY_ESNEXT

/**
 * Error message, class constructor invoked without new keyword
 */
const char * const ecma_error_class_constructor_new = "Class constructor cannot be invoked without 'new'";

/**
 * Error message, variables declared by let/const must be initialized before reading their value
 */
const char * const ecma_error_let_const_not_initialized = ("Variables declared by let/const must be"
                                                           " initialized before reading their value");

#endif /* JERRY_ESNEXT */

#endif /* JERRY_ERROR_MESSAGES */

#if JERRY_SNAPSHOT_SAVE || JERRY_SNAPSHOT_EXEC

/**
 * Error message, maximum snapshot size reached
 */
const char * const ecma_error_maximum_snapshot_size = "Maximum snapshot size reached";

/**
 * Error message, regular expressions literals are not supported
 */
const char * const ecma_error_regular_expression_not_supported = "Regular expression literals are not supported";

/**
 * Error message, snapshot buffer too small
 */
const char * const ecma_error_snapshot_buffer_small = "Snapshot buffer too small";

/**
 * Error message, Unsupported generate snapshot flags specified
 */
const char * const ecma_error_snapshot_flag_not_supported = "Unsupported generate snapshot flags specified";

/**
 * Error message, Cannot allocate memory for literals
 */
const char * const ecma_error_cannot_allocate_memory_literals = "Cannot allocate memory for literals";

/**
 * Error message, Unsupported feature: tagged template literals
 */
const char * const ecma_error_tagged_template_literals = "Unsupported feature: tagged template literals";

#endif /* JERRY_SNAPSHOT_SAVE || JERRY_SNAPSHOT_EXEC */

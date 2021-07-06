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

#ifndef JERRYSCRIPT_TYPES_H
#define JERRYSCRIPT_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "jerryscript-compiler.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/** \addtogroup jerry Jerry engine interface
 * @{
 */

/**
 * Major version of JerryScript API.
 */
#define JERRY_API_MAJOR_VERSION 3

/**
 * Minor version of JerryScript API.
 */
#define JERRY_API_MINOR_VERSION 0

/**
 * Patch version of JerryScript API.
 */
#define JERRY_API_PATCH_VERSION 0

/**
 * JerryScript init flags.
 */
typedef enum
{
  JERRY_INIT_EMPTY               = (0u),      /**< empty flag set */
  JERRY_INIT_SHOW_OPCODES        = (1u << 0), /**< dump byte-code to log after parse */
  JERRY_INIT_SHOW_REGEXP_OPCODES = (1u << 1), /**< dump regexp byte-code to log after compilation */
  JERRY_INIT_MEM_STATS           = (1u << 2), /**< dump memory statistics */
  JERRY_INIT_MEM_STATS_SEPARATE  = (1u << 3), /**< deprecated, an unused placeholder now */
  JERRY_INIT_DEBUGGER            = (1u << 4), /**< deprecated, an unused placeholder now */
} jerry_init_flag_t;

/**
 * JerryScript API Error object types.
 */
typedef enum
{
  JERRY_ERROR_NONE = 0,  /**< No Error */

  JERRY_ERROR_COMMON,    /**< Error */
  JERRY_ERROR_EVAL,      /**< EvalError */
  JERRY_ERROR_RANGE,     /**< RangeError */
  JERRY_ERROR_REFERENCE, /**< ReferenceError */
  JERRY_ERROR_SYNTAX,    /**< SyntaxError */
  JERRY_ERROR_TYPE,      /**< TypeError */
  JERRY_ERROR_URI,       /**< URIError */
  JERRY_ERROR_AGGREGATE  /**< AggregateError */
} jerry_error_t;

/**
 * JerryScript feature types.
 */
typedef enum
{
  JERRY_FEATURE_CPOINTER_32_BIT, /**< 32 bit compressed pointers */
  JERRY_FEATURE_ERROR_MESSAGES, /**< error messages */
  JERRY_FEATURE_JS_PARSER, /**< js-parser */
  JERRY_FEATURE_MEM_STATS, /**< memory statistics */
  JERRY_FEATURE_PARSER_DUMP, /**< parser byte-code dumps */
  JERRY_FEATURE_REGEXP_DUMP, /**< regexp byte-code dumps */
  JERRY_FEATURE_SNAPSHOT_SAVE, /**< saving snapshot files */
  JERRY_FEATURE_SNAPSHOT_EXEC, /**< executing snapshot files */
  JERRY_FEATURE_DEBUGGER, /**< debugging */
  JERRY_FEATURE_VM_EXEC_STOP, /**< stopping ECMAScript execution */
  JERRY_FEATURE_JSON, /**< JSON support */
  JERRY_FEATURE_PROMISE, /**< promise support */
  JERRY_FEATURE_TYPEDARRAY, /**< Typedarray support */
  JERRY_FEATURE_DATE, /**< Date support */
  JERRY_FEATURE_REGEXP, /**< Regexp support */
  JERRY_FEATURE_LINE_INFO, /**< line info available */
  JERRY_FEATURE_LOGGING, /**< logging */
  JERRY_FEATURE_SYMBOL, /**< symbol support */
  JERRY_FEATURE_DATAVIEW, /**< DataView support */
  JERRY_FEATURE_PROXY, /**< Proxy support */
  JERRY_FEATURE_MAP, /**< Map support */
  JERRY_FEATURE_SET, /**< Set support */
  JERRY_FEATURE_WEAKMAP, /**< WeakMap support */
  JERRY_FEATURE_WEAKSET, /**< WeakSet support */
  JERRY_FEATURE_BIGINT, /**< BigInt support */
  JERRY_FEATURE_REALM, /**< realm support */
  JERRY_FEATURE_GLOBAL_THIS, /**< GlobalThisValue support */
  JERRY_FEATURE_PROMISE_CALLBACK, /**< Promise callback support */
  JERRY_FEATURE_MODULE, /**< Module support */
  JERRY_FEATURE_WEAKREF, /**< WeakRef support */
  JERRY_FEATURE__COUNT /**< number of features. NOTE: must be at the end of the list */
} jerry_feature_t;

/**
 * GC operational modes.
 */
typedef enum
{
  JERRY_GC_PRESSURE_LOW, /**< free unused objects, but keep memory
                          *   allocated for performance improvements
                          *   such as property hash tables for large objects */
  JERRY_GC_PRESSURE_HIGH /**< free as much memory as possible */
} jerry_gc_mode_t;

/**
 * Jerry regexp flags.
 */
typedef enum
{
  JERRY_REGEXP_FLAG_GLOBAL = (1u << 1),      /**< Globally scan string */
  JERRY_REGEXP_FLAG_IGNORE_CASE = (1u << 2), /**< Ignore case */
  JERRY_REGEXP_FLAG_MULTILINE = (1u << 3),   /**< Multiline string scan */
  JERRY_REGEXP_FLAG_STICKY = (1u << 4),      /**< ECMAScript v11, 21.2.5.14 */
  JERRY_REGEXP_FLAG_UNICODE = (1u << 5),     /**< ECMAScript v11, 21.2.5.17 */
  JERRY_REGEXP_FLAG_DOTALL = (1u << 6)       /**< ECMAScript v11, 21.2.5.3 */
} jerry_regexp_flags_t;

/**
 * Character type of JerryScript.
 */
typedef uint8_t jerry_char_t;

/**
 * Size type of JerryScript.
 */
typedef uint32_t jerry_size_t;

/**
 * Length type of JerryScript.
 */
typedef uint32_t jerry_length_t;

/**
 * Description of a JerryScript value.
 */
typedef uint32_t jerry_value_t;

/**
 * Option bits for jerry_parse_options_t.
 */
typedef enum
{
  JERRY_PARSE_NO_OPTS = 0, /**< no options passed */
  JERRY_PARSE_STRICT_MODE = (1 << 0), /**< enable strict mode */
  JERRY_PARSE_MODULE = (1 << 1), /**< parse source as an ECMAScript module */
  JERRY_PARSE_HAS_RESOURCE = (1 << 2), /**< resource_name_p and resource_name_length fields are valid */
  JERRY_PARSE_HAS_START = (1 << 3), /**< start_line and start_column fields are valid */
} jerry_parse_option_enable_feature_t;

/**
 * Various configuration options for parsing functions such as jerry_parse or jerry_parse_function.
 */
typedef struct
{
  uint32_t options; /**< combination of jerry_parse_option_enable_feature_t values */
  const jerry_char_t *resource_name_p; /**< resource name (usually a file name)
                                        *   if JERRY_PARSE_HAS_RESOURCE is set in options */
  size_t resource_name_length; /**< length of resource name
                                *   if JERRY_PARSE_HAS_RESOURCE is set in options */
  uint32_t start_line; /**< start line of the source code if JERRY_PARSE_HAS_START is set in options */
  uint32_t start_column; /**< start column of the source code if JERRY_PARSE_HAS_START is set in options */
} jerry_parse_options_t;

/**
 * Description of ECMA property descriptor.
 */
typedef enum
{
  JERRY_PROP_NO_OPTS = (0), /**< empty property descriptor */
  JERRY_PROP_IS_CONFIGURABLE = (1 << 0), /**< [[Configurable]] */
  JERRY_PROP_IS_ENUMERABLE = (1 << 1), /**< [[Enumerable]] */
  JERRY_PROP_IS_WRITABLE = (1 << 2), /**< [[Writable]] */

  JERRY_PROP_IS_CONFIGURABLE_DEFINED = (1 << 3), /**< is [[Configurable]] defined? */
  JERRY_PROP_IS_ENUMERABLE_DEFINED = (1 << 4), /**< is [[Enumerable]] defined? */
  JERRY_PROP_IS_WRITABLE_DEFINED = (1 << 5), /**< is [[Writable]] defined? */

  JERRY_PROP_IS_VALUE_DEFINED = (1 << 6), /**< is [[Value]] defined? */
  JERRY_PROP_IS_GET_DEFINED = (1 << 7), /**< is [[Get]] defined? */
  JERRY_PROP_IS_SET_DEFINED = (1 << 8), /**< is [[Set]] defined? */

  JERRY_PROP_SHOULD_THROW = (1 << 9), /**< should throw on error, instead of returning with false */
} jerry_property_descriptor_flags_t;

/**
 * Description of ECMA property descriptor.
 */
typedef struct
{
  uint16_t flags; /**< any combination of jerry_property_descriptor_flags_t bits */
  jerry_value_t value; /**< [[Value]] */
  jerry_value_t getter; /**< [[Get]] */
  jerry_value_t setter; /**< [[Set]] */
} jerry_property_descriptor_t;

/**
 * JerryScript object property filter options.
 */
typedef enum
{
  JERRY_PROPERTY_FILTER_ALL = 0,                               /**< List all property keys independently
                                                                *   from key type or property value attributes
                                                                *   (equivalent to Reflect.ownKeys call)  */
  JERRY_PROPERTY_FILTER_TRAVERSE_PROTOTYPE_CHAIN = (1 << 0),   /**< Include keys from the objects's
                                                                *   prototype chain as well */
  JERRY_PROPERTY_FILTER_EXLCUDE_NON_CONFIGURABLE = (1 << 1),   /**< Exclude property key if
                                                                *   the property is non-configurable */
  JERRY_PROPERTY_FILTER_EXLCUDE_NON_ENUMERABLE = (1 << 2),     /**< Exclude property key if
                                                                *   the property is non-enumerable */
  JERRY_PROPERTY_FILTER_EXLCUDE_NON_WRITABLE = (1 << 3),       /**< Exclude property key if
                                                                *   the property is non-writable */
  JERRY_PROPERTY_FILTER_EXLCUDE_STRINGS = (1 << 4),            /**< Exclude property key if it is a string */
  JERRY_PROPERTY_FILTER_EXLCUDE_SYMBOLS = (1 << 5),            /**< Exclude property key if it is a symbol */
  JERRY_PROPERTY_FILTER_EXLCUDE_INTEGER_INDICES = (1 << 6),    /**< Exclude property key if it is an integer index */
  JERRY_PROPERTY_FILTER_INTEGER_INDICES_AS_NUMBER = (1 << 7),  /**< By default integer index property keys are
                                                                *   converted to string. Enabling this flags keeps
                                                                *   integer index property keys as numbers. */
} jerry_property_filter_t;

/**
 * Description of JerryScript heap memory stats.
 * It is for memory profiling.
 */
typedef struct
{
  size_t version; /**< the version of the stats struct */
  size_t size; /**< heap total size */
  size_t allocated_bytes; /**< currently allocated bytes */
  size_t peak_allocated_bytes; /**< peak allocated bytes */
  size_t reserved[4]; /**< padding for future extensions */
} jerry_heap_stats_t;

/**
 * Call related information passed to jerry_external_handler_t.
 */
typedef struct jerry_call_info_t
{
  jerry_value_t function; /**< invoked function object */
  jerry_value_t this_value; /**< this value passed to the function  */
  jerry_value_t new_target; /**< current new target value, undefined for non-constructor calls */
} jerry_call_info_t;

/**
 * Type of an external function handler.
 */
typedef jerry_value_t (*jerry_external_handler_t) (const jerry_call_info_t *call_info_p,
                                                   const jerry_value_t args_p[],
                                                   const jerry_length_t args_count);

/**
 * Native free callback of generic value types.
 */
typedef void (*jerry_value_free_callback_t) (void *native_p);

/**
 * Forward definition of jerry_object_native_info_t.
 */
struct jerry_object_native_info_t;

/**
 * Native free callback of an object.
 */
typedef void (*jerry_object_native_free_callback_t) (void *native_p, struct jerry_object_native_info_t *info_p);

/**
 * Free callback for external strings.
 */
typedef void (*jerry_external_string_free_callback_t) (jerry_char_t *string_p, jerry_size_t string_size,
                                                       void *user_p);

/**
 * Decorator callback for Error objects. The decorator can create
 * or update any properties of the newly created Error object.
 */
typedef void (*jerry_error_object_created_callback_t) (const jerry_value_t error_object, void *user_p);

/**
 * Callback which tells whether the ECMAScript execution should be stopped.
 *
 * As long as the function returns with undefined the execution continues.
 * When a non-undefined value is returned the execution stops and the value
 * is thrown by the engine (if the error flag is not set for the returned
 * value the engine automatically sets it).
 *
 * Note: if the function returns with a non-undefined value it
 *       must return with the same value for future calls.
 */
typedef jerry_value_t (*jerry_vm_exec_stop_callback_t) (void *user_p);

/**
 * Function type applied for each data property of an object.
 */
typedef bool (*jerry_object_property_foreach_t) (const jerry_value_t property_name,
                                                 const jerry_value_t property_value,
                                                 void *user_data_p);

/**
 * Function type applied for each object in the engine.
 */
typedef bool (*jerry_objects_foreach_t) (const jerry_value_t object,
                                         void *user_data_p);

/**
 * Function type applied for each matching object in the engine.
 */
typedef bool (*jerry_objects_foreach_by_native_info_t) (const jerry_value_t object,
                                                        void *object_data_p,
                                                        void *user_data_p);

/**
 * User context item manager
 */
typedef struct
{
  /**
   * Callback responsible for initializing a context item, or NULL to zero out the memory. This is called lazily, the
   * first time jerry_get_context_data () is called with this manager.
   *
   * @param [in] data The buffer that JerryScript allocated for the manager. The buffer is zeroed out. The size is
   * determined by the bytes_needed field. The buffer is kept alive until jerry_cleanup () is called.
   */
  void (*init_cb) (void *data);

  /**
   * Callback responsible for deinitializing a context item, or NULL. This is called as part of jerry_cleanup (),
   * right *before* the VM has been cleaned up. This is a good place to release strong references to jerry_value_t's
   * that the manager may be holding.
   * Note: because the VM has not been fully cleaned up yet, jerry_object_native_info_t free_cb's can still get called
   * *after* all deinit_cb's have been run. See finalize_cb for a callback that is guaranteed to run *after* all
   * free_cb's have been run.
   *
   * @param [in] data The buffer that JerryScript allocated for the manager.
   */
  void (*deinit_cb) (void *data);

  /**
   * Callback responsible for finalizing a context item, or NULL. This is called as part of jerry_cleanup (),
   * right *after* the VM has been cleaned up and destroyed and jerry_... APIs cannot be called any more. At this point,
   * all values in the VM have been cleaned up. This is a good place to clean up native state that can only be cleaned
   * up at the very end when there are no more VM values around that may need to access that state.
   *
   * @param [in] data The buffer that JerryScript allocated for the manager. After returning from this callback,
   * the data pointer may no longer be used.
   */
  void (*finalize_cb) (void *data);

  /**
   * Number of bytes to allocate for this manager. This is the size of the buffer that JerryScript will allocate on
   * behalf of the manager. The pointer to this buffer is passed into init_cb, deinit_cb and finalize_cb. It is also
   * returned from the jerry_get_context_data () API.
   */
  size_t bytes_needed;
} jerry_context_data_manager_t;

/**
 * Function type for allocating buffer for JerryScript context.
 */
typedef void *(*jerry_context_alloc_t) (size_t size, void *cb_data_p);

/**
 * Type information of a native pointer.
 */
typedef struct jerry_object_native_info_t
{
  jerry_object_native_free_callback_t free_cb; /**< the free callback of the native pointer */
  uint16_t number_of_references; /**< the number of value references which are marked by the garbage collector */
  uint16_t offset_of_references; /**< byte offset indicating the start offset of value
                                  *   references in the user allocated buffer */
} jerry_object_native_info_t;

/**
 * An opaque declaration of the JerryScript context structure.
 */
typedef struct jerry_context_t jerry_context_t;

/**
 * Enum that contains the supported binary operation types
 */
typedef enum
{
  JERRY_BIN_OP_EQUAL = 0u,    /**< equal comparison (==) */
  JERRY_BIN_OP_STRICT_EQUAL,  /**< strict equal comparison (===) */
  JERRY_BIN_OP_LESS,          /**< less relation (<) */
  JERRY_BIN_OP_LESS_EQUAL,    /**< less or equal relation (<=) */
  JERRY_BIN_OP_GREATER,       /**< greater relation (>) */
  JERRY_BIN_OP_GREATER_EQUAL, /**< greater or equal relation (>=)*/
  JERRY_BIN_OP_INSTANCEOF,    /**< instanceof operation */
  JERRY_BIN_OP_ADD,           /**< addition operator (+) */
  JERRY_BIN_OP_SUB,           /**< subtraction operator (-) */
  JERRY_BIN_OP_MUL,           /**< multiplication operator (*) */
  JERRY_BIN_OP_DIV,           /**< division operator (/) */
  JERRY_BIN_OP_REM,           /**< remainder operator (%) */
} jerry_binary_operation_t;

/**
 * Backtrace related types.
 */

/**
 * List of backtrace frame types returned by jerry_backtrace_get_frame_type.
 */
typedef enum
{
  JERRY_BACKTRACE_FRAME_JS, /**< indicates that the frame is created for a JavaScript function/method */
} jerry_backtrace_frame_types_t;

/**
 * Location info retreived by jerry_backtrace_get_location.
 */
typedef struct
{
  jerry_value_t resource_name; /**< resource name */
  jerry_size_t line; /**< line index */
  jerry_size_t column; /**< column index */
} jerry_backtrace_location_t;

/**
 * Internal data structure for jerry_backtrace_frame_t definition.
 */
struct jerry_backtrace_frame_internal_t;

/**
 * Backtrace frame data passed to the jerry_backtrace_callback_t handler.
 */
typedef struct jerry_backtrace_frame_internal_t jerry_backtrace_frame_t;

/**
 * Callback function which is called by jerry_backtrace_capture for each stack frame.
 */
typedef bool (*jerry_backtrace_callback_t) (jerry_backtrace_frame_t *frame_p, void *user_p);

/**
 * Detailed value type related types.
 */

/**
 * JerryScript API value type information.
 */
typedef enum
{
  JERRY_TYPE_NONE = 0u, /**< no type information */
  JERRY_TYPE_UNDEFINED, /**< undefined type */
  JERRY_TYPE_NULL,      /**< null type */
  JERRY_TYPE_BOOLEAN,   /**< boolean type */
  JERRY_TYPE_NUMBER,    /**< number type */
  JERRY_TYPE_STRING,    /**< string type */
  JERRY_TYPE_OBJECT,    /**< object type */
  JERRY_TYPE_FUNCTION,  /**< function type */
  JERRY_TYPE_ERROR,     /**< error/abort type */
  JERRY_TYPE_SYMBOL,    /**< symbol type */
  JERRY_TYPE_BIGINT,    /**< bigint type */
} jerry_type_t;

/**
 * JerryScript object type information.
 */
typedef enum
{
  JERRY_OBJECT_TYPE_NONE = 0u,        /**< Non object type */
  JERRY_OBJECT_TYPE_GENERIC,          /**< Generic JavaScript object without any internal property */
  JERRY_OBJECT_TYPE_MODULE_NAMESPACE, /**< Namespace object */
  JERRY_OBJECT_TYPE_ARRAY,            /**< Array object */
  JERRY_OBJECT_TYPE_PROXY,            /**< Proxy object */
  JERRY_OBJECT_TYPE_SCRIPT,           /**< Script object (see jerry_parse) */
  JERRY_OBJECT_TYPE_MODULE,           /**< Module object (see jerry_parse) */
  JERRY_OBJECT_TYPE_PROMISE,          /**< Promise object */
  JERRY_OBJECT_TYPE_DATAVIEW,         /**< Dataview object */
  JERRY_OBJECT_TYPE_FUNCTION,         /**< Function object (see jerry_function_get_type) */
  JERRY_OBJECT_TYPE_TYPEDARRAY,       /**< %TypedArray% object (see jerry_get_typedarray_type) */
  JERRY_OBJECT_TYPE_ITERATOR,         /**< Iterator object (see jerry_iterator_get_type) */
  JERRY_OBJECT_TYPE_CONTAINER,        /**< Container object (see jerry_container_get_type) */
  JERRY_OBJECT_TYPE_ERROR,            /**< Error object */
  JERRY_OBJECT_TYPE_ARRAYBUFFER,      /**< Array buffer object */

  JERRY_OBJECT_TYPE_ARGUMENTS,        /**< Arguments object */
  JERRY_OBJECT_TYPE_BOOLEAN,          /**< Boolean object */
  JERRY_OBJECT_TYPE_DATE,             /**< Date object */
  JERRY_OBJECT_TYPE_NUMBER,           /**< Number object */
  JERRY_OBJECT_TYPE_REGEXP,           /**< RegExp object */
  JERRY_OBJECT_TYPE_STRING,           /**< String object */
  JERRY_OBJECT_TYPE_SYMBOL,           /**< Symbol object */
  JERRY_OBJECT_TYPE_GENERATOR,        /**< Generator object */
  JERRY_OBJECT_TYPE_BIGINT,           /**< BigInt object */
  JERRY_OBJECT_TYPE_WEAKREF,           /**< WeakRef object */
} jerry_object_type_t;

/**
 * JerryScript function object type information.
 */
typedef enum
{
  JERRY_FUNCTION_TYPE_NONE = 0u,    /**< Non function type */
  JERRY_FUNCTION_TYPE_GENERIC,      /**< Generic JavaScript function */
  JERRY_FUNCTION_TYPE_ACCESSOR,     /**< Accessor function */
  JERRY_FUNCTION_TYPE_BOUND,        /**< Bound function */
  JERRY_FUNCTION_TYPE_ARROW,        /**< Arrow fuction */
  JERRY_FUNCTION_TYPE_GENERATOR,    /**< Generator function */
} jerry_function_type_t;

/**
 * JerryScript iterator object type information.
 */
typedef enum
{
  JERRY_ITERATOR_TYPE_NONE = 0u,    /**< Non iterator type */
  JERRY_ITERATOR_TYPE_ARRAY,        /**< Array iterator */
  JERRY_ITERATOR_TYPE_STRING,       /**< String iterator */
  JERRY_ITERATOR_TYPE_MAP,          /**< Map iterator */
  JERRY_ITERATOR_TYPE_SET,          /**< Set iterator */
} jerry_iterator_type_t;

/**
 * Module related types.
 */

/**
 * An enum representing the current status of a module
 */
typedef enum
{
  JERRY_MODULE_STATE_INVALID = 0,  /**< return value for jerry_module_get_state when its argument is not a module */
  JERRY_MODULE_STATE_UNLINKED = 1, /**< module is currently unlinked */
  JERRY_MODULE_STATE_LINKING = 2, /**< module is currently being linked */
  JERRY_MODULE_STATE_LINKED = 3, /**< module has been linked (its depencencies has been resolved) */
  JERRY_MODULE_STATE_EVALUATING = 4, /**< module is currently being evaluated */
  JERRY_MODULE_STATE_EVALUATED = 5, /**< module has been evaluated (its source code has been executed) */
  JERRY_MODULE_STATE_ERROR = 6, /**< an error has been encountered before the evaluated state is reached */
} jerry_module_state_t;

/**
 * Callback which is called by jerry_module_link to get the referenced module.
 */
typedef jerry_value_t (*jerry_module_resolve_callback_t) (const jerry_value_t specifier,
                                                          const jerry_value_t referrer,
                                                          void *user_p);

/**
 * Callback which is called after the module enters into linked, evaluated or error state.
 */
typedef void (*jerry_module_state_changed_callback_t) (jerry_module_state_t new_state, const jerry_value_t module,
                                                       const jerry_value_t value, void *user_p);

/**
 * Callback which is called by jerry_module_evaluate to evaluate the native module.
 */
typedef jerry_value_t (*jerry_native_module_evaluate_callback_t) (const jerry_value_t native_module);

/**
 * Proxy related types.
 */

/**
 * JerryScript special Proxy object options.
 */
typedef enum
{
  JERRY_PROXY_SKIP_RESULT_VALIDATION = (1u << 0), /**< skip result validation for [[GetPrototypeOf]],
                                                   *   [[SetPrototypeOf]], [[IsExtensible]],
                                                   *   [[PreventExtensions]], [[GetOwnProperty]],
                                                   *   [[DefineOwnProperty]], [[HasProperty]], [[Get]],
                                                   *   [[Set]], [[Delete]] and [[OwnPropertyKeys]] */
} jerry_proxy_object_options_t;

/**
 * Promise related types.
 */

/**
 * Enum values representing various Promise states.
 */
typedef enum
{
  JERRY_PROMISE_STATE_NONE = 0u, /**< Invalid/Unknown state (possibly called on a non-promise object). */
  JERRY_PROMISE_STATE_PENDING,   /**< Promise is in "Pending" state. */
  JERRY_PROMISE_STATE_FULFILLED, /**< Promise is in "Fulfilled" state. */
  JERRY_PROMISE_STATE_REJECTED,  /**< Promise is in "Rejected" state. */
} jerry_promise_state_t;

/**
 * Event types for jerry_promise_callback_t callback function.
 * The description of the 'object' and 'value' arguments are provided for each type.
 */
typedef enum
{
  JERRY_PROMISE_EVENT_CREATE = 0u, /**< a new Promise object is created
                                    *   object: the new Promise object
                                    *   value: parent Promise for `then` chains, undefined otherwise */
  JERRY_PROMISE_EVENT_RESOLVE, /**< called when a Promise is about to be resolved
                                *   object: the Promise object
                                *   value: value for resolving */
  JERRY_PROMISE_EVENT_REJECT, /**< called when a Promise is about to be rejected
                               *   object: the Promise object
                               *   value: value for rejecting */
  JERRY_PROMISE_EVENT_RESOLVE_FULFILLED, /**< called when a resolve is called on a fulfilled Promise
                                          *   object: the Promise object
                                          *   value: value for resolving */
  JERRY_PROMISE_EVENT_REJECT_FULFILLED, /**< called when a reject is called on a fulfilled Promise
                                          *  object: the Promise object
                                          *  value: value for rejecting */
  JERRY_PROMISE_EVENT_REJECT_WITHOUT_HANDLER, /**< called when a Promise is rejected without a handler
                                               *   object: the Promise object
                                               *   value: value for rejecting */
  JERRY_PROMISE_EVENT_CATCH_HANDLER_ADDED, /**< called when a catch handler is added to a rejected
                                            *   Promise which did not have a catch handler before
                                            *   object: the Promise object
                                            *   value: undefined */
  JERRY_PROMISE_EVENT_BEFORE_REACTION_JOB, /**< called before executing a Promise reaction job
                                            *   object: the Promise object
                                            *   value: undefined */
  JERRY_PROMISE_EVENT_AFTER_REACTION_JOB, /**< called after a Promise reaction job is completed
                                           *   object: the Promise object
                                           *   value: undefined */
  JERRY_PROMISE_EVENT_ASYNC_AWAIT, /**< called when an async function awaits the result of a Promise object
                                    *   object: internal object representing the execution status
                                    *   value: the Promise object */
  JERRY_PROMISE_EVENT_ASYNC_BEFORE_RESOLVE, /**< called when an async function is continued with resolve
                                             *   object: internal object representing the execution status
                                             *   value: value for resolving */
  JERRY_PROMISE_EVENT_ASYNC_BEFORE_REJECT, /**< called when an async function is continued with reject
                                            *   object: internal object representing the execution status
                                            *   value: value for rejecting */
  JERRY_PROMISE_EVENT_ASYNC_AFTER_RESOLVE, /**< called when an async function resolve is completed
                                            *   object: internal object representing the execution status
                                            *   value: value for resolving */
  JERRY_PROMISE_EVENT_ASYNC_AFTER_REJECT, /**< called when an async function reject is completed
                                           *   object: internal object representing the execution status
                                           *   value: value for rejecting */
} jerry_promise_event_type_t;

/**
 * Filter types for jerry_promise_set_callback callback function.
 * The callback is only called for those events which are enabled by the filters.
 */
typedef enum
{
  JERRY_PROMISE_EVENT_FILTER_DISABLE = 0, /**< disable reporting of all events */
  JERRY_PROMISE_EVENT_FILTER_MAIN = (1 << 0), /**< enables the following events:
                                               *   JERRY_PROMISE_EVENT_CREATE
                                               *   JERRY_PROMISE_EVENT_RESOLVE
                                               *   JERRY_PROMISE_EVENT_REJECT */
  JERRY_PROMISE_EVENT_FILTER_ERROR = (1 << 1), /**< enables the following events:
                                                *   JERRY_PROMISE_EVENT_RESOLVE_FULFILLED
                                                *   JERRY_PROMISE_EVENT_REJECT_FULFILLED
                                                *   JERRY_PROMISE_EVENT_REJECT_WITHOUT_HANDLER
                                                *   JERRY_PROMISE_EVENT_CATCH_HANDLER_ADDED */
  JERRY_PROMISE_EVENT_FILTER_REACTION_JOB = (1 << 2), /**< enables the following events:
                                                       *   JERRY_PROMISE_EVENT_BEFORE_REACTION_JOB
                                                       *   JERRY_PROMISE_EVENT_AFTER_REACTION_JOB */
  JERRY_PROMISE_EVENT_FILTER_ASYNC_MAIN = (1 << 3), /**< enables the following events:
                                                     *   JERRY_PROMISE_EVENT_ASYNC_AWAIT */
  JERRY_PROMISE_EVENT_FILTER_ASYNC_REACTION_JOB = (1 << 4), /**< enables the following events:
                                                             *   JERRY_PROMISE_EVENT_ASYNC_BEFORE_RESOLVE
                                                             *   JERRY_PROMISE_EVENT_ASYNC_BEFORE_REJECT
                                                             *   JERRY_PROMISE_EVENT_ASYNC_AFTER_RESOLVE
                                                             *   JERRY_PROMISE_EVENT_ASYNC_AFTER_REJECT */
} jerry_promise_event_filter_t;

/**
 * Notification callback for tracking Promise and async function operations.
 */
typedef void (*jerry_promise_callback_t) (jerry_promise_event_type_t event_type,
                                          const jerry_value_t object, const jerry_value_t value,
                                          void *user_p);

/**
 * Symbol related types.
 */

/**
 * List of well-known symbols.
 */
typedef enum
{
  JERRY_SYMBOL_ASYNC_ITERATOR,       /**< @@asyncIterator well-known symbol */
  JERRY_SYMBOL_HAS_INSTANCE,         /**< @@hasInstance well-known symbol */
  JERRY_SYMBOL_IS_CONCAT_SPREADABLE, /**< @@isConcatSpreadable well-known symbol */
  JERRY_SYMBOL_ITERATOR,             /**< @@iterator well-known symbol */
  JERRY_SYMBOL_MATCH,                /**< @@match well-known symbol */
  JERRY_SYMBOL_REPLACE,              /**< @@replace well-known symbol */
  JERRY_SYMBOL_SEARCH,               /**< @@search well-known symbol */
  JERRY_SYMBOL_SPECIES,              /**< @@species well-known symbol */
  JERRY_SYMBOL_SPLIT,                /**< @@split well-known symbol */
  JERRY_SYMBOL_TO_PRIMITIVE,         /**< @@toPrimitive well-known symbol */
  JERRY_SYMBOL_TO_STRING_TAG,        /**< @@toStringTag well-known symbol */
  JERRY_SYMBOL_UNSCOPABLES,          /**< @@unscopables well-known symbol */
  JERRY_SYMBOL_MATCH_ALL,            /**< @@matchAll well-known symbol */
} jerry_well_known_symbol_t;

/**
 * TypedArray related types.
 */

/**
 * TypedArray types.
 */
typedef enum
{
  JERRY_TYPEDARRAY_INVALID = 0,
  JERRY_TYPEDARRAY_UINT8,
  JERRY_TYPEDARRAY_UINT8CLAMPED,
  JERRY_TYPEDARRAY_INT8,
  JERRY_TYPEDARRAY_UINT16,
  JERRY_TYPEDARRAY_INT16,
  JERRY_TYPEDARRAY_UINT32,
  JERRY_TYPEDARRAY_INT32,
  JERRY_TYPEDARRAY_FLOAT32,
  JERRY_TYPEDARRAY_FLOAT64,
  JERRY_TYPEDARRAY_BIGINT64,
  JERRY_TYPEDARRAY_BIGUINT64,
} jerry_typedarray_type_t;

/**
 * Container types.
 */
typedef enum
{
  JERRY_CONTAINER_TYPE_INVALID = 0, /**< Invalid container */
  JERRY_CONTAINER_TYPE_MAP, /**< Map type */
  JERRY_CONTAINER_TYPE_SET, /**< Set type */
  JERRY_CONTAINER_TYPE_WEAKMAP, /**< WeakMap type */
  JERRY_CONTAINER_TYPE_WEAKSET, /**< WeakSet type */
} jerry_container_type_t;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYSCRIPT_TYPES_H */

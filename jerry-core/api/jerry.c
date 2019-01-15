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

#include <stdio.h>

#include "debugger.h"
#include "ecma-alloc.h"
#include "ecma-array-object.h"
#include "ecma-arraybuffer-object.h"
#include "ecma-builtin-helpers.h"
#include "ecma-builtins.h"
#include "ecma-exceptions.h"
#include "ecma-eval.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-helpers.h"
#include "ecma-init-finalize.h"
#include "ecma-lex-env.h"
#include "ecma-literal-storage.h"
#include "ecma-objects.h"
#include "ecma-objects-general.h"
#include "ecma-regexp-object.h"
#include "ecma-promise-object.h"
#include "ecma-typedarray-object.h"
#include "jcontext.h"
#include "jerryscript.h"
#include "jerryscript-debugger-transport.h"
#include "jmem.h"
#include "js-parser.h"
#include "re-compiler.h"

JERRY_STATIC_ASSERT (sizeof (jerry_value_t) == sizeof (ecma_value_t),
                     size_of_jerry_value_t_must_be_equal_to_size_of_ecma_value_t);

JERRY_STATIC_ASSERT ((int) ECMA_ERROR_NONE == (int) JERRY_ERROR_NONE
                     && (int) ECMA_ERROR_COMMON == (int) JERRY_ERROR_COMMON
                     && (int) ECMA_ERROR_EVAL == (int) JERRY_ERROR_EVAL
                     && (int) ECMA_ERROR_RANGE == (int) JERRY_ERROR_RANGE
                     && (int) ECMA_ERROR_REFERENCE == (int) JERRY_ERROR_REFERENCE
                     && (int) ECMA_ERROR_SYNTAX == (int) JERRY_ERROR_SYNTAX
                     && (int) ECMA_ERROR_TYPE == (int) JERRY_ERROR_TYPE
                     && (int) ECMA_ERROR_URI == (int) JERRY_ERROR_URI,
                     ecma_standard_error_t_must_be_equal_to_jerry_error_t);

JERRY_STATIC_ASSERT ((int) ECMA_INIT_EMPTY == (int) JERRY_INIT_EMPTY
                     && (int) ECMA_INIT_SHOW_OPCODES == (int) JERRY_INIT_SHOW_OPCODES
                     && (int) ECMA_INIT_SHOW_REGEXP_OPCODES == (int) JERRY_INIT_SHOW_REGEXP_OPCODES
                     && (int) ECMA_INIT_MEM_STATS == (int) JERRY_INIT_MEM_STATS,
                     ecma_init_flag_t_must_be_equal_to_jerry_init_flag_t);

#ifndef CONFIG_DISABLE_REGEXP_BUILTIN
JERRY_STATIC_ASSERT ((int) RE_FLAG_GLOBAL == (int) JERRY_REGEXP_FLAG_GLOBAL
                     && (int) RE_FLAG_MULTILINE == (int) JERRY_REGEXP_FLAG_MULTILINE
                     && (int) RE_FLAG_IGNORE_CASE == (int) JERRY_REGEXP_FLAG_IGNORE_CASE,
                     re_flags_t_must_be_equal_to_jerry_regexp_flags_t);
#endif /* !CONFIG_DISABLE_REGEXP_BUILTIN */

#if defined JERRY_DISABLE_JS_PARSER && !defined JERRY_ENABLE_SNAPSHOT_EXEC
#error JERRY_ENABLE_SNAPSHOT_EXEC must be defined if JERRY_DISABLE_JS_PARSER is defined!
#endif /* JERRY_DISABLE_JS_PARSER && !JERRY_ENABLE_SNAPSHOT_EXEC */

#ifdef JERRY_ENABLE_ERROR_MESSAGES

/**
 * Error message, if an argument is has an error flag
 */
static const char * const error_value_msg_p = "argument cannot have an error flag";

/**
 * Error message, if types of arguments are incorrect
 */
static const char * const wrong_args_msg_p = "wrong type of argument";

#endif /* JERRY_ENABLE_ERROR_MESSAGES */

/** \addtogroup jerry Jerry engine interface
 * @{
 */

/**
 * Assert that it is correct to call API in current state.
 *
 * Note:
 *         By convention, there are some states when API could not be invoked.
 *
 *         The API can be and only be invoked when the ECMA_STATUS_API_AVAILABLE
 *         flag is set.
 *
 *         This procedure checks whether the API is available, and terminates
 *         the engine if it is unavailable. Otherwise it is a no-op.
 *
 * Note:
 *         The API could not be invoked in the following cases:
 *           - before jerry_init and after jerry_cleanup
 *           - between enter to and return from a native free callback
 */
static inline void JERRY_ATTR_ALWAYS_INLINE
jerry_assert_api_available (void)
{
  JERRY_ASSERT (JERRY_CONTEXT (status_flags) & ECMA_STATUS_API_AVAILABLE);
} /* jerry_assert_api_available */

/**
 * Turn on API availability
 */
static inline void JERRY_ATTR_ALWAYS_INLINE
jerry_make_api_available (void)
{
  JERRY_CONTEXT (status_flags) |= ECMA_STATUS_API_AVAILABLE;
} /* jerry_make_api_available */

/**
 * Turn off API availability
 */
static inline void JERRY_ATTR_ALWAYS_INLINE
jerry_make_api_unavailable (void)
{
  JERRY_CONTEXT (status_flags) &= (uint32_t) ~ECMA_STATUS_API_AVAILABLE;
} /* jerry_make_api_unavailable */

/**
 * Create an API compatible return value.
 *
 * @return return value for Jerry API functions
 */
static jerry_value_t
jerry_return (jerry_value_t value) /**< return value */
{
  if (ECMA_IS_VALUE_ERROR (value))
  {
    value = ecma_create_error_reference_from_context ();
  }

  return value;
} /* jerry_return */

/**
 * Throw an API compatible return value.
 *
 * @return return value for Jerry API functions
 */
static inline jerry_value_t JERRY_ATTR_ALWAYS_INLINE
jerry_throw (jerry_value_t value) /**< return value */
{
  JERRY_ASSERT (ECMA_IS_VALUE_ERROR (value));
  return ecma_create_error_reference_from_context ();
} /* jerry_throw */

/**
 * Jerry engine initialization
 */
void
jerry_init (jerry_init_flag_t flags) /**< combination of Jerry flags */
{
  /* This function cannot be called twice unless jerry_cleanup is called. */
  JERRY_ASSERT (!(JERRY_CONTEXT (status_flags) & ECMA_STATUS_API_AVAILABLE));

  /* Zero out all non-external members. */
  memset (&JERRY_CONTEXT (JERRY_CONTEXT_FIRST_MEMBER), 0,
          sizeof (jerry_context_t) - offsetof (jerry_context_t, JERRY_CONTEXT_FIRST_MEMBER));

  JERRY_CONTEXT (jerry_init_flags) = flags;

  jerry_make_api_available ();

  jmem_init ();
  ecma_init ();
} /* jerry_init */

/**
 * Terminate Jerry engine
 */
void
jerry_cleanup (void)
{
  jerry_assert_api_available ();

#ifdef JERRY_DEBUGGER
  if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
  {
    jerry_debugger_transport_close ();
  }
#endif /* JERRY_DEBUGGER */

  for (jerry_context_data_header_t *this_p = JERRY_CONTEXT (context_data_p);
       this_p != NULL;
       this_p = this_p->next_p)
  {
    if (this_p->manager_p->deinit_cb)
    {
      this_p->manager_p->deinit_cb (JERRY_CONTEXT_DATA_HEADER_USER_DATA (this_p));
    }
  }

#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
  ecma_free_all_enqueued_jobs ();
#endif /* CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
  ecma_finalize ();
  jerry_make_api_unavailable ();

  for (jerry_context_data_header_t *this_p = JERRY_CONTEXT (context_data_p), *next_p = NULL;
       this_p != NULL;
       this_p = next_p)
  {
    next_p = this_p->next_p;
    if (this_p->manager_p->finalize_cb)
    {
      this_p->manager_p->finalize_cb (JERRY_CONTEXT_DATA_HEADER_USER_DATA (this_p));
    }
    jmem_heap_free_block (this_p, sizeof (jerry_context_data_header_t) + this_p->manager_p->bytes_needed);
  }

  jmem_finalize ();
} /* jerry_cleanup */

/**
 * Retrieve a context data item, or create a new one.
 *
 * @param manager_p pointer to the manager whose context data item should be returned.
 *
 * @return a pointer to the user-provided context-specific data item for the given manager, creating such a pointer if
 * none was found.
 */
void *
jerry_get_context_data (const jerry_context_data_manager_t *manager_p)
{
  void *ret = NULL;
  jerry_context_data_header_t *item_p;

  for (item_p = JERRY_CONTEXT (context_data_p); item_p != NULL; item_p = item_p->next_p)
  {
    if (item_p->manager_p == manager_p)
    {
      return JERRY_CONTEXT_DATA_HEADER_USER_DATA (item_p);
    }
  }

  item_p = jmem_heap_alloc_block (sizeof (jerry_context_data_header_t) + manager_p->bytes_needed);
  item_p->manager_p = manager_p;
  item_p->next_p = JERRY_CONTEXT (context_data_p);
  JERRY_CONTEXT (context_data_p) = item_p;
  ret = JERRY_CONTEXT_DATA_HEADER_USER_DATA (item_p);

  memset (ret, 0, manager_p->bytes_needed);
  if (manager_p->init_cb)
  {
    manager_p->init_cb (ret);
  }

  return ret;
} /* jerry_get_context_data */

/**
 * Register external magic string array
 */
void
jerry_register_magic_strings (const jerry_char_t * const *ex_str_items_p, /**< character arrays, representing
                                                                           *   external magic strings' contents */
                              uint32_t count, /**< number of the strings */
                              const jerry_length_t *str_lengths_p) /**< lengths of all strings */
{
  jerry_assert_api_available ();

  lit_magic_strings_ex_set ((const lit_utf8_byte_t * const *) ex_str_items_p,
                            count,
                            (const lit_utf8_size_t *) str_lengths_p);
} /* jerry_register_magic_strings */

/**
 * Run garbage collection
 */
void
jerry_gc (jerry_gc_mode_t mode) /**< operational mode */
{
  jerry_assert_api_available ();

  ecma_gc_run (mode == JERRY_GC_SEVERITY_LOW ? JMEM_FREE_UNUSED_MEMORY_SEVERITY_LOW
                                             : JMEM_FREE_UNUSED_MEMORY_SEVERITY_HIGH);
} /* jerry_gc */

/**
 * Get heap memory stats.
 *
 * @return true - get the heap stats successful
 *         false - otherwise. Usually it is because the MEM_STATS feature is not enabled.
 */
bool
jerry_get_memory_stats (jerry_heap_stats_t *out_stats_p) /**< [out] heap memory stats */
{
#ifdef JMEM_STATS
  if (out_stats_p == NULL)
  {
    return false;
  }

  jmem_heap_stats_t jmem_heap_stats;
  memset (&jmem_heap_stats, 0, sizeof (jmem_heap_stats));
  jmem_heap_get_stats (&jmem_heap_stats);

  *out_stats_p = (jerry_heap_stats_t)
  {
    .version = 1,
    .size = jmem_heap_stats.size,
    .allocated_bytes = jmem_heap_stats.allocated_bytes,
    .peak_allocated_bytes = jmem_heap_stats.peak_allocated_bytes
  };

  return true;
#else
  JERRY_UNUSED (out_stats_p);
  return false;
#endif
} /* jerry_get_memory_stats */

/**
 * Simple Jerry runner
 *
 * @return true  - if run was successful
 *         false - otherwise
 */
bool
jerry_run_simple (const jerry_char_t *script_source_p, /**< script source */
                  size_t script_source_size, /**< script source size */
                  jerry_init_flag_t flags) /**< combination of Jerry flags */
{
  bool result = false;

  jerry_init (flags);

  jerry_value_t parse_ret_val = jerry_parse (NULL, 0, script_source_p, script_source_size, JERRY_PARSE_NO_OPTS);

  if (!ecma_is_value_error_reference (parse_ret_val))
  {
    jerry_value_t run_ret_val = jerry_run (parse_ret_val);

    if (!ecma_is_value_error_reference (run_ret_val))
    {
      result = true;
    }

    jerry_release_value (run_ret_val);
  }

  jerry_release_value (parse_ret_val);
  jerry_cleanup ();

  return result;
} /* jerry_run_simple */

/**
 * Parse script and construct an EcmaScript function. The lexical
 * environment is set to the global lexical environment.
 *
 * @return function object value - if script was parsed successfully,
 *         thrown error - otherwise
 */
jerry_value_t
jerry_parse (const jerry_char_t *resource_name_p, /**< resource name (usually a file name) */
             size_t resource_name_length, /**< length of resource name */
             const jerry_char_t *source_p, /**< script source */
             size_t source_size, /**< script source size */
             uint32_t parse_opts) /**< jerry_parse_opts_t option bits */
{
#if defined JERRY_DEBUGGER && !defined JERRY_DISABLE_JS_PARSER
  if ((JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
      && resource_name_length > 0)
  {
    jerry_debugger_send_string (JERRY_DEBUGGER_SOURCE_CODE_NAME,
                                JERRY_DEBUGGER_NO_SUBTYPE,
                                resource_name_p,
                                resource_name_length);
  }
#else /* !(JERRY_DEBUGGER && !JERRY_DISABLE_JS_PARSER) */
  JERRY_UNUSED (resource_name_p);
  JERRY_UNUSED (resource_name_length);
#endif /* JERRY_DEBUGGER && !JERRY_DISABLE_JS_PARSER */

#ifndef JERRY_DISABLE_JS_PARSER
  jerry_assert_api_available ();

#if defined JERRY_ENABLE_LINE_INFO && !defined JERRY_DISABLE_JS_PARSER
  JERRY_CONTEXT (resource_name) = ecma_find_or_create_literal_string (resource_name_p,
                                                                      (lit_utf8_size_t) resource_name_length);
#endif /* JERRY_ENABLE_LINE_INFO && !JERRY_DISABLE_JS_PARSER */

  ecma_compiled_code_t *bytecode_data_p;
  ecma_value_t parse_status;

  parse_status = parser_parse_script (NULL,
                                      0,
                                      source_p,
                                      source_size,
                                      parse_opts,
                                      &bytecode_data_p);

  if (ECMA_IS_VALUE_ERROR (parse_status))
  {
    return ecma_create_error_reference_from_context ();
  }

  ecma_free_value (parse_status);

  ecma_object_t *lex_env_p = ecma_get_global_environment ();
  ecma_object_t *func_obj_p = ecma_op_create_function_object (lex_env_p,
                                                              bytecode_data_p);
  ecma_bytecode_deref (bytecode_data_p);

  return ecma_make_object_value (func_obj_p);
#else /* JERRY_DISABLE_JS_PARSER */
  JERRY_UNUSED (source_p);
  JERRY_UNUSED (source_size);
  JERRY_UNUSED (parse_opts);

  return jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG ("The parser has been disabled.")));
#endif /* !JERRY_DISABLE_JS_PARSER */
} /* jerry_parse */

/**
 * Parse function and construct an EcmaScript function. The lexical
 * environment is set to the global lexical environment.
 *
 * @return function object value - if script was parsed successfully,
 *         thrown error - otherwise
 */
jerry_value_t
jerry_parse_function (const jerry_char_t *resource_name_p, /**< resource name (usually a file name) */
                      size_t resource_name_length, /**< length of resource name */
                      const jerry_char_t *arg_list_p, /**< script source */
                      size_t arg_list_size, /**< script source size */
                      const jerry_char_t *source_p, /**< script source */
                      size_t source_size, /**< script source size */
                      uint32_t parse_opts) /**< jerry_parse_opts_t option bits */
{
#if defined JERRY_DEBUGGER && !defined JERRY_DISABLE_JS_PARSER
  if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
  {
    jerry_debugger_send_string (JERRY_DEBUGGER_SOURCE_CODE_NAME,
                                JERRY_DEBUGGER_NO_SUBTYPE,
                                resource_name_p,
                                resource_name_length);
  }
#else /* !(JERRY_DEBUGGER && !JERRY_DISABLE_JS_PARSER) */
  JERRY_UNUSED (resource_name_p);
  JERRY_UNUSED (resource_name_length);
#endif /* JERRY_DEBUGGER && !JERRY_DISABLE_JS_PARSER */

#ifndef JERRY_DISABLE_JS_PARSER
  jerry_assert_api_available ();

  ecma_compiled_code_t *bytecode_data_p;
  ecma_value_t parse_status;

#ifdef JERRY_ENABLE_LINE_INFO
  JERRY_CONTEXT (resource_name) = ecma_find_or_create_literal_string (resource_name_p,
                                                                      (lit_utf8_size_t) resource_name_length);
#endif /* JERRY_ENABLE_LINE_INFO */

  if (arg_list_p == NULL)
  {
    /* Must not be a NULL value. */
    arg_list_p = (const jerry_char_t *) "";
  }

  parse_status = parser_parse_script (arg_list_p,
                                      arg_list_size,
                                      source_p,
                                      source_size,
                                      parse_opts,
                                      &bytecode_data_p);

  if (ECMA_IS_VALUE_ERROR (parse_status))
  {
    return ecma_create_error_reference_from_context ();
  }

  ecma_free_value (parse_status);

  ecma_object_t *lex_env_p = ecma_get_global_environment ();
  ecma_object_t *func_obj_p = ecma_op_create_function_object (lex_env_p,
                                                              bytecode_data_p);
  ecma_bytecode_deref (bytecode_data_p);

  return ecma_make_object_value (func_obj_p);
#else /* JERRY_DISABLE_JS_PARSER */
  JERRY_UNUSED (arg_list_p);
  JERRY_UNUSED (arg_list_size);
  JERRY_UNUSED (source_p);
  JERRY_UNUSED (source_size);
  JERRY_UNUSED (parse_opts);

  return jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG ("The parser has been disabled.")));
#endif /* !JERRY_DISABLE_JS_PARSER */
} /* jerry_parse_function */

/**
 * Run an EcmaScript function created by jerry_parse.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return result of bytecode - if run was successful
 *         thrown error - otherwise
 */
jerry_value_t
jerry_run (const jerry_value_t func_val) /**< function to run */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (func_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  ecma_object_t *func_obj_p = ecma_get_object_from_value (func_val);

  if (ecma_get_object_type (func_obj_p) != ECMA_OBJECT_TYPE_FUNCTION
      || ecma_get_object_is_builtin (func_obj_p))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) func_obj_p;

  ecma_object_t *scope_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_object_t,
                                                            ext_func_p->u.function.scope_cp);

  if (scope_p != ecma_get_global_environment ())
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  return jerry_return (vm_run_global (ecma_op_function_get_compiled_code (ext_func_p)));
} /* jerry_run */

/**
 * Perform eval
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return result of eval, may be error value.
 */
jerry_value_t
jerry_eval (const jerry_char_t *source_p, /**< source code */
            size_t source_size, /**< length of source code */
            uint32_t parse_opts) /**< jerry_parse_opts_t option bits */
{
  jerry_assert_api_available ();

  return jerry_return (ecma_op_eval_chars_buffer ((const lit_utf8_byte_t *) source_p,
                                                  source_size,
                                                  parse_opts));
} /* jerry_eval */

/**
 * Run enqueued Promise jobs until the first thrown error or until all get executed.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return result of last executed job, may be error value.
 */
jerry_value_t
jerry_run_all_enqueued_jobs (void)
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
  return ecma_process_all_enqueued_jobs ();
#else /* CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
  return ECMA_VALUE_UNDEFINED;
#endif /* CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
} /* jerry_run_all_enqueued_jobs */

/**
 * Get global object
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return api value of global object
 */
jerry_value_t
jerry_get_global_object (void)
{
  jerry_assert_api_available ();
  ecma_object_t *global_obj_p = ecma_builtin_get_global ();
  ecma_ref_object (global_obj_p);
  return ecma_make_object_value (global_obj_p);
} /* jerry_get_global_object */

/**
 * Check if the specified value is an abort value.
 *
 * @return true  - if both the error and abort values are set,
 *         false - otherwise
 */
bool
jerry_value_is_abort (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_error_reference (value))
  {
    return false;
  }

  ecma_error_reference_t *error_ref_p = ecma_get_error_reference_from_value (value);

  return (error_ref_p->refs_and_flags & ECMA_ERROR_REF_ABORT) != 0;
} /* jerry_value_is_abort */

/**
 * Check if the specified value is an array object value.
 *
 * @return true  - if the specified value is an array object,
 *         false - otherwise
 */
bool
jerry_value_is_array (const jerry_value_t value) /**< jerry api value */
{
  jerry_assert_api_available ();

  return (ecma_is_value_object (value)
          && ecma_get_object_type (ecma_get_object_from_value (value)) == ECMA_OBJECT_TYPE_ARRAY);
} /* jerry_value_is_array */

/**
 * Check if the specified value is boolean.
 *
 * @return true  - if the specified value is boolean,
 *         false - otherwise
 */
bool
jerry_value_is_boolean (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  return ecma_is_value_boolean (value);
} /* jerry_value_is_boolean */

/**
 * Check if the specified value is a constructor function object value.
 *
 * @return true - if the specified value is a function value that implements [[Construct]],
 *         false - otherwise
 */
bool
jerry_value_is_constructor (const jerry_value_t value) /**< jerry api value */
{
  jerry_assert_api_available ();

  return ecma_is_constructor (value);
} /* jerry_value_is_constructor */

/**
 * Check if the specified value is an error or abort value.
 *
 * @return true  - if the specified value is an error value,
 *         false - otherwise
 */
bool
jerry_value_is_error (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  return ecma_is_value_error_reference (value);
} /* jerry_value_is_error */

/**
 * Check if the specified value is a function object value.
 *
 * @return true - if the specified value is callable,
 *         false - otherwise
 */
bool
jerry_value_is_function (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  return ecma_op_is_callable (value);
} /* jerry_value_is_function */

/**
 * Check if the specified value is number.
 *
 * @return true  - if the specified value is number,
 *         false - otherwise
 */
bool
jerry_value_is_number (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  return ecma_is_value_number (value);
} /* jerry_value_is_number */

/**
 * Check if the specified value is null.
 *
 * @return true  - if the specified value is null,
 *         false - otherwise
 */
bool
jerry_value_is_null (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  return ecma_is_value_null (value);
} /* jerry_value_is_null */

/**
 * Check if the specified value is object.
 *
 * @return true  - if the specified value is object,
 *         false - otherwise
 */
bool
jerry_value_is_object (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  return ecma_is_value_object (value);
} /* jerry_value_is_object */

/**
 * Check if the specified value is promise.
 *
 * @return true  - if the specified value is promise,
 *         false - otherwise
 */
bool
jerry_value_is_promise (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();
#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
  return (ecma_is_value_object (value)
          && ecma_is_promise (ecma_get_object_from_value (value)));
#else /* CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
  JERRY_UNUSED (value);
  return false;
#endif /* !CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
} /* jerry_value_is_promise */

/**
 * Check if the specified value is string.
 *
 * @return true  - if the specified value is string,
 *         false - otherwise
 */
bool
jerry_value_is_string (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  return ecma_is_value_string (value);
} /* jerry_value_is_string */

/**
 * Check if the specified value is undefined.
 *
 * @return true  - if the specified value is undefined,
 *         false - otherwise
 */
bool
jerry_value_is_undefined (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  return ecma_is_value_undefined (value);
} /* jerry_value_is_undefined */

/**
 * Perform the base type of the JavaScript value.
 *
 * @return jerry_type_t value
 */
jerry_type_t
jerry_value_get_type (const jerry_value_t value) /**< input value to check */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (value))
  {
    return JERRY_TYPE_ERROR;
  }

  lit_magic_string_id_t lit_id = ecma_get_typeof_lit_id (value);

  JERRY_ASSERT (lit_id != LIT_MAGIC_STRING__EMPTY);

  switch (lit_id)
  {
    case LIT_MAGIC_STRING_UNDEFINED:
    {
      return JERRY_TYPE_UNDEFINED;
    }
    case LIT_MAGIC_STRING_BOOLEAN:
    {
      return JERRY_TYPE_BOOLEAN;
    }
    case LIT_MAGIC_STRING_NUMBER:
    {
      return JERRY_TYPE_NUMBER;
    }
    case LIT_MAGIC_STRING_STRING:
    {
      return JERRY_TYPE_STRING;
    }
    case LIT_MAGIC_STRING_FUNCTION:
    {
      return JERRY_TYPE_FUNCTION;
    }
    default:
    {
      JERRY_ASSERT (lit_id == LIT_MAGIC_STRING_OBJECT);

      /* Based on the ECMA 262 5.1 standard the 'null' value is an object.
       * Thus we'll do an extra check for 'null' here.
       */
      return ecma_is_value_null (value) ? JERRY_TYPE_NULL : JERRY_TYPE_OBJECT;
    }
  }
} /* jerry_value_get_type */

/**
 * Check if the specified feature is enabled.
 *
 * @return true  - if the specified feature is enabled,
 *         false - otherwise
 */
bool
jerry_is_feature_enabled (const jerry_feature_t feature) /**< feature to check */
{
  JERRY_ASSERT (feature < JERRY_FEATURE__COUNT);

  return (false
#ifdef JERRY_CPOINTER_32_BIT
          || feature == JERRY_FEATURE_CPOINTER_32_BIT
#endif /* JERRY_CPOINTER_32_BIT */
#ifdef JERRY_ENABLE_ERROR_MESSAGES
          || feature == JERRY_FEATURE_ERROR_MESSAGES
#endif /* JERRY_ENABLE_ERROR_MESSAGES */
#ifndef JERRY_DISABLE_JS_PARSER
          || feature == JERRY_FEATURE_JS_PARSER
#endif /* !JERRY_DISABLE_JS_PARSER */
#ifdef JMEM_STATS
          || feature == JERRY_FEATURE_MEM_STATS
#endif /* JMEM_STATS */
#ifdef JMEM_TRACK_ALLOCATION_SIZES
          || feature == JERRY_FEATURE_MEM_TRACK_ALLOCATION_SIZES
#endif /* JMEM_TRACK_ALLOCATION_SIZES */
#ifdef JERRY_ENABLE_HEAP_SNAPSHOT
          || feature == JERRY_FEATURE_HEAP_SNAPSHOT
#endif /* JERRY_ENABLE_HEAP_SNAPSHOT */
#ifdef PARSER_DUMP_BYTE_CODE
          || feature == JERRY_FEATURE_PARSER_DUMP
#endif /* PARSER_DUMP_BYTE_CODE */
#ifdef REGEXP_DUMP_BYTE_CODE
          || feature == JERRY_FEATURE_REGEXP_DUMP
#endif /* REGEXP_DUMP_BYTE_CODE */
#ifdef JERRY_ENABLE_SNAPSHOT_SAVE
          || feature == JERRY_FEATURE_SNAPSHOT_SAVE
#endif /* JERRY_ENABLE_SNAPSHOT_SAVE */
#ifdef JERRY_ENABLE_SNAPSHOT_EXEC
          || feature == JERRY_FEATURE_SNAPSHOT_EXEC
#endif /* JERRY_ENABLE_SNAPSHOT_EXEC */
#ifdef JERRY_DEBUGGER
          || feature == JERRY_FEATURE_DEBUGGER
#endif /* JERRY_DEBUGGER */
#ifdef JERRY_VM_EXEC_STOP
          || feature == JERRY_FEATURE_VM_EXEC_STOP
#endif /* JERRY_VM_EXEC_STOP */
#ifndef CONFIG_DISABLE_JSON_BUILTIN
          || feature == JERRY_FEATURE_JSON
#endif /* !CONFIG_DISABLE_JSON_BUILTIN */
#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
          || feature == JERRY_FEATURE_PROMISE
#endif /* !CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
          || feature == JERRY_FEATURE_TYPEDARRAY
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
#ifndef CONFIG_DISABLE_DATE_BUILTIN
          || feature == JERRY_FEATURE_DATE
#endif /* !CONFIG_DISABLE_DATE_BUILTIN */
#ifndef CONFIG_DISABLE_REGEXP_BUILTIN
          || feature == JERRY_FEATURE_REGEXP
#endif /* !CONFIG_DISABLE_REGEXP_BUILTIN */
#ifdef JERRY_ENABLE_LINE_INFO
          || feature == JERRY_FEATURE_LINE_INFO
#endif /* JERRY_ENABLE_LINE_INFO */
#ifdef JERRY_ENABLE_LOGGING
          || feature == JERRY_FEATURE_LOGGING
#endif /* JERRY_ENABLE_LOGGING */
          );
} /* jerry_is_feature_enabled */

/**
 * Create abort from an api value.
 *
 * Create abort value from an api value. If the second argument is true
 * it will release the input api value.
 *
 * @return api abort value
 */
jerry_value_t
jerry_create_abort_from_value (jerry_value_t value, /**< api value */
                               bool release) /**< release api value */
{
  jerry_assert_api_available ();

  if (JERRY_UNLIKELY (ecma_is_value_error_reference (value)))
  {
    /* This is a rare case so it is optimized for
     * binary size rather than performance. */
    if (jerry_value_is_abort (value))
    {
      return release ? value : jerry_acquire_value (value);
    }

    value = jerry_get_value_from_error (value, release);
    release = true;
  }

  if (!release)
  {
    value = ecma_copy_value (value);
  }

  return ecma_create_error_reference (value, false);
} /* jerry_create_abort_from_value */

/**
 * Create error from an api value.
 *
 * Create error value from an api value. If the second argument is true
 * it will release the input api value.
 *
 * @return api error value
 */
jerry_value_t
jerry_create_error_from_value (jerry_value_t value, /**< api value */
                               bool release) /**< release api value */
{
  jerry_assert_api_available ();

  if (JERRY_UNLIKELY (ecma_is_value_error_reference (value)))
  {
    /* This is a rare case so it is optimized for
     * binary size rather than performance. */
    if (!jerry_value_is_abort (value))
    {
      return release ? value : jerry_acquire_value (value);
    }

    value = jerry_get_value_from_error (value, release);
    release = true;
  }

  if (!release)
  {
    value = ecma_copy_value (value);
  }

  return ecma_create_error_reference (value, true);
} /* jerry_create_error_from_value */

/**
 * Get the value from an error value.
 *
 * Extract the api value from an error. If the second argument is true
 * it will release the input error value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return jerry_value_t value
 */
jerry_value_t
jerry_get_value_from_error (jerry_value_t value, /**< api value */
                            bool release) /**< release api value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_error_reference (value))
  {
    return release ? value : ecma_copy_value (value);
  }

  jerry_value_t ret_val = jerry_acquire_value (ecma_get_error_reference_from_value (value)->value);

  if (release)
  {
    jerry_release_value (value);
  }
  return ret_val;
} /* jerry_get_value_from_error */

/**
 * Return the type of the Error object if possible.
 *
 * @return one of the jerry_error_t value as the type of the Error object
 *         JERRY_ERROR_NONE - if the input value is not an Error object
 */
jerry_error_t
jerry_get_error_type (jerry_value_t value) /**< api value */
{
  if (JERRY_UNLIKELY (ecma_is_value_error_reference (value)))
  {
    value = ecma_get_error_reference_from_value (value)->value;
  }

  if (!ecma_is_value_object (value))
  {
    return JERRY_ERROR_NONE;
  }

  ecma_object_t *object_p = ecma_get_object_from_value (value);
  ecma_standard_error_t error_type = ecma_get_error_type (object_p);

  return (jerry_error_t) error_type;
} /* jerry_get_error_type */

/**
 * Get boolean from the specified value.
 *
 * @return true or false.
 */
bool
jerry_get_boolean_value (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  return ecma_is_value_true (value);
} /* jerry_get_boolean_value */

/**
 * Get number from the specified value as a double.
 *
 * @return stored number as double
 */
double
jerry_get_number_value (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_number (value))
  {
    return 0;
  }

  return (double) ecma_get_number_from_value (value);
} /* jerry_get_number_value */

/**
 * Call ToBoolean operation on the api value.
 *
 * @return true  - if the logical value is true
 *         false - otherwise
 */
bool
jerry_value_to_boolean (const jerry_value_t value) /**< input value */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (value))
  {
    return false;
  }

  return ecma_op_to_boolean (value);
} /* jerry_value_to_boolean */

/**
 * Call ToNumber operation on the api value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return converted number value - if success
 *         thrown error - otherwise
 */
jerry_value_t
jerry_value_to_number (const jerry_value_t value) /**< input value */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (error_value_msg_p)));
  }

  return jerry_return (ecma_op_to_number (value));
} /* jerry_value_to_number */

/**
 * Call ToObject operation on the api value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return converted object value - if success
 *         thrown error - otherwise
 */
jerry_value_t
jerry_value_to_object (const jerry_value_t value) /**< input value */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (error_value_msg_p)));
  }

  return jerry_return (ecma_op_to_object (value));
} /* jerry_value_to_object */

/**
 * Call ToPrimitive operation on the api value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return converted primitive value - if success
 *         thrown error - otherwise
 */
jerry_value_t
jerry_value_to_primitive (const jerry_value_t value) /**< input value */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (error_value_msg_p)));
  }

  return jerry_return (ecma_op_to_primitive (value, ECMA_PREFERRED_TYPE_NO));
} /* jerry_value_to_primitive */

/**
 * Call the ToString ecma builtin operation on the api value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return converted string value - if success
 *         thrown error - otherwise
 */
jerry_value_t
jerry_value_to_string (const jerry_value_t value) /**< input value */
{

  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (error_value_msg_p)));
  }

  return jerry_return (ecma_op_to_string (value));
} /* jerry_value_to_string */

/**
 * Acquire specified Jerry API value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return acquired api value
 */
jerry_value_t
jerry_acquire_value (jerry_value_t value) /**< API value */
{
  jerry_assert_api_available ();

  if (JERRY_UNLIKELY (ecma_is_value_error_reference (value)))
  {
    ecma_ref_error_reference (ecma_get_error_reference_from_value (value));
    return value;
  }

  return ecma_copy_value (value);
} /* jerry_acquire_value */

/**
 * Release specified Jerry API value
 */
void
jerry_release_value (jerry_value_t value) /**< API value */
{
  jerry_assert_api_available ();

  if (JERRY_UNLIKELY (ecma_is_value_error_reference (value)))
  {
    ecma_deref_error_reference (ecma_get_error_reference_from_value (value));
    return;
  }

  ecma_free_value (value);
} /* jerry_release_value */

/**
 * Create an array object value
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the constructed array object
 */
jerry_value_t
jerry_create_array (uint32_t size) /**< size of array */
{
  jerry_assert_api_available ();

  ecma_value_t array_length = ecma_make_uint32_value (size);

  const jerry_length_t argument_size = 1;
  ecma_value_t array_value = ecma_op_create_array_object (&array_length, argument_size, true);
  ecma_free_value (array_length);

  JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (array_value));

  return array_value;
} /* jerry_create_array */

/**
 * Create a jerry_value_t representing a boolean value from the given boolean parameter.
 *
 * @return value of the created boolean
 */
jerry_value_t
jerry_create_boolean (bool value) /**< bool value from which a jerry_value_t will be created */
{
  jerry_assert_api_available ();

  return jerry_return (ecma_make_boolean_value (value));
} /* jerry_create_boolean */

/**
 * Create an error object
 *
 * Note:
 *      - returned value must be freed with jerry_release_value, when it is no longer needed
 *      - the error flag is set for the returned value
 *
 * @return value of the constructed error object
 */
jerry_value_t
jerry_create_error (jerry_error_t error_type, /**< type of error */
                    const jerry_char_t *message_p) /**< value of 'message' property
                                                    *   of constructed error object */
{
  return jerry_create_error_sz (error_type,
                                (lit_utf8_byte_t *) message_p,
                                lit_zt_utf8_string_size (message_p));
} /* jerry_create_error */

/**
 * Create an error object
 *
 * Note:
 *      - returned value must be freed with jerry_release_value, when it is no longer needed
 *      - the error flag is set for the returned value
 *
 * @return value of the constructed error object
 */
jerry_value_t
jerry_create_error_sz (jerry_error_t error_type, /**< type of error */
                       const jerry_char_t *message_p, /**< value of 'message' property
                                                       *   of constructed error object */
                       jerry_size_t message_size) /**< size of the message in bytes */
{
  jerry_assert_api_available ();

  if (message_p == NULL || message_size == 0)
  {
    return ecma_create_error_object_reference (ecma_new_standard_error ((ecma_standard_error_t) error_type));
  }
  else
  {
    ecma_string_t *message_string_p = ecma_new_ecma_string_from_utf8 ((lit_utf8_byte_t *) message_p,
                                                                      (lit_utf8_size_t) message_size);

    ecma_object_t *error_object_p = ecma_new_standard_error_with_message ((ecma_standard_error_t) error_type,
                                                                          message_string_p);

    ecma_deref_ecma_string (message_string_p);

    return ecma_create_error_object_reference (error_object_p);
  }
} /* jerry_create_error_sz */

/**
 * Create an external function object
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the constructed function object
 */
jerry_value_t
jerry_create_external_function (jerry_external_handler_t handler_p) /**< pointer to native handler
                                                                     *   for the function */
{
  jerry_assert_api_available ();

  ecma_object_t *func_obj_p = ecma_op_create_external_function_object (handler_p);
  return ecma_make_object_value (func_obj_p);
} /* jerry_create_external_function */

/**
 * Creates a jerry_value_t representing a number value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return jerry_value_t created from the given double argument.
 */
jerry_value_t
jerry_create_number (double value) /**< double value from which a jerry_value_t will be created */
{
  jerry_assert_api_available ();

  return ecma_make_number_value ((ecma_number_t) value);
} /* jerry_create_number */

/**
 * Creates a jerry_value_t representing a positive or negative infinity value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return jerry_value_t representing an infinity value.
 */
jerry_value_t
jerry_create_number_infinity (bool sign) /**< true for negative Infinity
                                          *   false for positive Infinity */
{
  jerry_assert_api_available ();

  return ecma_make_number_value (ecma_number_make_infinity (sign));
} /* jerry_create_number_infinity */

/**
 * Creates a jerry_value_t representing a not-a-number value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return jerry_value_t representing a not-a-number value.
 */
jerry_value_t
jerry_create_number_nan (void)
{
  jerry_assert_api_available ();

  return ecma_make_nan_value ();
} /* jerry_create_number_nan */

/**
 * Creates a jerry_value_t representing an undefined value.
 *
 * @return value of undefined
 */
jerry_value_t
jerry_create_undefined (void)
{
  jerry_assert_api_available ();

  return ECMA_VALUE_UNDEFINED;
} /* jerry_create_undefined */

/**
 * Creates and returns a jerry_value_t with type null object.
 *
 * @return jerry_value_t representing null
 */
jerry_value_t
jerry_create_null (void)
{
  jerry_assert_api_available ();

  return ECMA_VALUE_NULL;
} /* jerry_create_null */

/**
 * Create new JavaScript object, like with new Object().
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the created object
 */
jerry_value_t
jerry_create_object (void)
{
  jerry_assert_api_available ();

  return ecma_make_object_value (ecma_op_create_object_object_noarg ());
} /* jerry_create_object */

/**
 * Create an empty Promise object which can be resolve/reject later
 * by calling jerry_resolve_or_reject_promise.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the created object
 */
jerry_value_t
jerry_create_promise (void)
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
  return ecma_op_create_promise_object (ECMA_VALUE_EMPTY, ECMA_PROMISE_EXECUTOR_EMPTY);
#else /* !CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Promise not supported.")));
#endif /* CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
} /* jerry_create_promise */

/**
 * Create string from a valid UTF-8 string
 *
 * Note:
 *      returned value must be freed with jerry_release_value when it is no longer needed.
 *
 * @return value of the created string
 */
jerry_value_t
jerry_create_string_from_utf8 (const jerry_char_t *str_p) /**< pointer to string */
{
  return jerry_create_string_sz_from_utf8 (str_p, lit_zt_utf8_string_size ((lit_utf8_byte_t *) str_p));
} /* jerry_create_string_from_utf8 */

/**
 * Create string from a valid UTF-8 string
 *
 * Note:
 *      returned value must be freed with jerry_release_value when it is no longer needed.
 *
 * @return value of the created string
 */
jerry_value_t
jerry_create_string_sz_from_utf8 (const jerry_char_t *str_p, /**< pointer to string */
                                  jerry_size_t str_size) /**< string size */
{
  jerry_assert_api_available ();

  ecma_string_t *ecma_str_p = ecma_new_ecma_string_from_utf8_converted_to_cesu8 ((lit_utf8_byte_t *) str_p,
                                                                                 (lit_utf8_size_t) str_size);

  return ecma_make_string_value (ecma_str_p);
} /* jerry_create_string_sz_from_utf8 */

/**
 * Create string from a valid CESU-8 string
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the created string
 */
jerry_value_t
jerry_create_string (const jerry_char_t *str_p) /**< pointer to string */
{
  return jerry_create_string_sz (str_p, lit_zt_utf8_string_size ((lit_utf8_byte_t *) str_p));
} /* jerry_create_string */

/**
 * Create string from a valid CESU-8 string
 *
 * Note:
 *      returned value must be freed with jerry_release_value when it is no longer needed.
 *
 * @return value of the created string
 */
jerry_value_t
jerry_create_string_sz (const jerry_char_t *str_p, /**< pointer to string */
                        jerry_size_t str_size) /**< string size */
{
  jerry_assert_api_available ();

  ecma_string_t *ecma_str_p = ecma_new_ecma_string_from_utf8 ((lit_utf8_byte_t *) str_p,
                                                              (lit_utf8_size_t) str_size);
  return ecma_make_string_value (ecma_str_p);
} /* jerry_create_string_sz */

/**
 * Calculates the size of the given pattern and creates a RegExp object.
 *
 * @return value of the constructed RegExp object.
 */
jerry_value_t
jerry_create_regexp (const jerry_char_t *pattern_p, /**< zero-terminated UTF-8 string as RegExp pattern */
                     uint16_t flags) /**< optional RegExp flags */
{
  return jerry_create_regexp_sz (pattern_p, lit_zt_utf8_string_size (pattern_p), flags);
} /* jerry_create_regexp */

/**
 * Creates a RegExp object with the given pattern and flags.
 *
 * @return value of the constructed RegExp object.
 */
jerry_value_t
jerry_create_regexp_sz (const jerry_char_t *pattern_p, /**< zero-terminated UTF-8 string as RegExp pattern */
                        jerry_size_t pattern_size, /**< length of the pattern */
                        uint16_t flags) /**< optional RegExp flags */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_REGEXP_BUILTIN
  if (!lit_is_valid_utf8_string (pattern_p, pattern_size))
  {
    return jerry_throw (ecma_raise_common_error (ECMA_ERR_MSG ("Input must be a valid utf8 string")));
  }

  ecma_string_t *ecma_pattern = ecma_new_ecma_string_from_utf8 (pattern_p, pattern_size);

  jerry_value_t ret_val = ecma_op_create_regexp_object (ecma_pattern, flags);

  ecma_deref_ecma_string (ecma_pattern);
  return ret_val;

#else /* CONFIG_DISABLE_REGEXP_BUILTIN */
  JERRY_UNUSED (pattern_p);
  JERRY_UNUSED (pattern_size);
  JERRY_UNUSED (flags);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("RegExp is not supported.")));
#endif /* !CONFIG_DISABLE_REGEXP_BUILTIN */
} /* jerry_create_regexp_sz */

/**
 * Get length of an array object
 *
 * Note:
 *      Returns 0, if the value parameter is not an array object.
 *
 * @return length of the given array
 */
uint32_t
jerry_get_array_length (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  if (!jerry_value_is_array (value))
  {
    return 0;
  }

  ecma_value_t len_value = ecma_op_object_get_by_magic_id (ecma_get_object_from_value (value),
                                                           LIT_MAGIC_STRING_LENGTH);

  jerry_length_t length = ecma_number_to_uint32 (ecma_get_number_from_value (len_value));
  ecma_free_value (len_value);

  return length;
} /* jerry_get_array_length */

/**
 * Get size of Jerry string
 *
 * Note:
 *      Returns 0, if the value parameter is not a string.
 *
 * @return number of bytes in the buffer needed to represent the string
 */
jerry_size_t
jerry_get_string_size (const jerry_value_t value) /**< input string */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_string (value))
  {
    return 0;
  }

  return ecma_string_get_size (ecma_get_string_from_value (value));
} /* jerry_get_string_size */

/**
 * Get UTF-8 encoded string size from Jerry string
 *
 * Note:
 *      Returns 0, if the value parameter is not a string.
 *
 * @return number of bytes in the buffer needed to represent the UTF-8 encoded string
 */
jerry_size_t
jerry_get_utf8_string_size (const jerry_value_t value) /**< input string */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_string (value))
  {
    return 0;
  }

  return ecma_string_get_utf8_size (ecma_get_string_from_value (value));
} /* jerry_get_utf8_string_size */

/**
 * Get length of Jerry string
 *
 * Note:
 *      Returns 0, if the value parameter is not a string.
 *
 * @return number of characters in the string
 */
jerry_length_t
jerry_get_string_length (const jerry_value_t value) /**< input string */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_string (value))
  {
    return 0;
  }

  return ecma_string_get_length (ecma_get_string_from_value (value));
} /* jerry_get_string_length */

/**
 * Get UTF-8 string length from Jerry string
 *
 * Note:
 *      Returns 0, if the value parameter is not a string.
 *
 * @return number of characters in the string
 */
jerry_length_t
jerry_get_utf8_string_length (const jerry_value_t value) /**< input string */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_string (value))
  {
    return 0;
  }

  return ecma_string_get_utf8_length (ecma_get_string_from_value (value));
} /* jerry_get_utf8_string_length */

/**
 * Copy the characters of a string into a specified buffer.
 *
 * Note:
 *      The '\0' character could occur in character buffer.
 *      Returns 0, if the value parameter is not a string or
 *      the buffer is not large enough for the whole string.
 *
 * Note:
 *      If the size of the string in jerry value is larger than the size of the
 *      target buffer, the copy will fail.
 *      To copy substring use jerry_substring_to_char_buffer() instead.
 *
 * @return number of bytes, actually copied to the buffer.
 */
jerry_size_t
jerry_string_to_char_buffer (const jerry_value_t value, /**< input string value */
                             jerry_char_t *buffer_p, /**< [out] output characters buffer */
                             jerry_size_t buffer_size) /**< size of output buffer */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_string (value) || buffer_p == NULL)
  {
    return 0;
  }

  ecma_string_t *str_p = ecma_get_string_from_value (value);

  if (ecma_string_get_size (str_p) > buffer_size)
  {
    return 0;
  }

  return ecma_string_copy_to_cesu8_buffer (str_p,
                                           (lit_utf8_byte_t *) buffer_p,
                                           buffer_size);
} /* jerry_string_to_char_buffer */

/**
 * Copy the characters of an utf-8 encoded string into a specified buffer.
 *
 * Note:
 *      The '\0' character could occur anywhere in the returned string
 *      Returns 0, if the value parameter is not a string or the buffer
 *      is not large enough for the whole string.
 *
 * Note:
 *      If the size of the string in jerry value is larger than the size of the
 *      target buffer, the copy will fail.
 *      To copy a substring use jerry_substring_to_utf8_char_buffer() instead.
 *
 * @return number of bytes copied to the buffer.
 */
jerry_size_t
jerry_string_to_utf8_char_buffer (const jerry_value_t value, /**< input string value */
                                  jerry_char_t *buffer_p, /**< [out] output characters buffer */
                                  jerry_size_t buffer_size) /**< size of output buffer */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_string (value) || buffer_p == NULL)
  {
    return 0;
  }

  ecma_string_t *str_p = ecma_get_string_from_value (value);

  if (ecma_string_get_utf8_size (str_p) > buffer_size)
  {
    return 0;
  }

  return ecma_string_copy_to_utf8_buffer (str_p,
                                          (lit_utf8_byte_t *) buffer_p,
                                          buffer_size);
} /* jerry_string_to_utf8_char_buffer */

/**
 * Copy the characters of an cesu-8 encoded substring into a specified buffer.
 *
 * Note:
 *      The '\0' character could occur anywhere in the returned string
 *      Returns 0, if the value parameter is not a string.
 *      It will extract the substring beetween the specified start position
 *      and the end position (or the end of the string, whichever comes first).
 *
 * @return number of bytes copied to the buffer.
 */
jerry_size_t
jerry_substring_to_char_buffer (const jerry_value_t value, /**< input string value */
                                jerry_length_t start_pos, /**< position of the first character */
                                jerry_length_t end_pos, /**< position of the last character */
                                jerry_char_t *buffer_p, /**< [out] output characters buffer */
                                jerry_size_t buffer_size) /**< size of output buffer */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_string (value) || buffer_p == NULL)
  {
    return 0;
  }

  ecma_string_t *str_p = ecma_get_string_from_value (value);

  return ecma_substring_copy_to_cesu8_buffer (str_p,
                                              start_pos,
                                              end_pos,
                                              (lit_utf8_byte_t *) buffer_p,
                                              buffer_size);
} /* jerry_substring_to_char_buffer */

/**
 * Copy the characters of an utf-8 encoded substring into a specified buffer.
 *
 * Note:
 *      The '\0' character could occur anywhere in the returned string
 *      Returns 0, if the value parameter is not a string.
 *      It will extract the substring beetween the specified start position
 *      and the end position (or the end of the string, whichever comes first).
 *
 * @return number of bytes copied to the buffer.
 */
jerry_size_t
jerry_substring_to_utf8_char_buffer (const jerry_value_t value, /**< input string value */
                                     jerry_length_t start_pos, /**< position of the first character */
                                     jerry_length_t end_pos, /**< position of the last character */
                                     jerry_char_t *buffer_p, /**< [out] output characters buffer */
                                     jerry_size_t buffer_size) /**< size of output buffer */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_string (value) || buffer_p == NULL)
  {
    return 0;
  }

  ecma_string_t *str_p = ecma_get_string_from_value (value);

  return ecma_substring_copy_to_utf8_buffer (str_p,
                                             start_pos,
                                             end_pos,
                                             (lit_utf8_byte_t *) buffer_p,
                                             buffer_size);
} /* jerry_substring_to_utf8_char_buffer */

/**
 * Checks whether the object or it's prototype objects have the given property.
 *
 * @return true  - if the property exists
 *         false - otherwise
 */
jerry_value_t
jerry_has_property (const jerry_value_t obj_val, /**< object value */
                    const jerry_value_t prop_name_val) /**< property name (string value) */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_string (prop_name_val))
  {
    return ECMA_VALUE_FALSE;
  }

  bool has_property = ecma_op_object_has_property (ecma_get_object_from_value (obj_val),
                                                   ecma_get_string_from_value (prop_name_val));

  return ecma_make_boolean_value (has_property);
} /* jerry_has_property */

/**
 * Checks whether the object has the given property.
 *
 * @return true  - if the property exists
 *         false - otherwise
 */
jerry_value_t
jerry_has_own_property (const jerry_value_t obj_val, /**< object value */
                        const jerry_value_t prop_name_val) /**< property name (string value) */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_string (prop_name_val))
  {
    return ECMA_VALUE_FALSE;
  }

  bool has_property = ecma_op_object_has_own_property (ecma_get_object_from_value (obj_val),
                                                       ecma_get_string_from_value (prop_name_val));

  return ecma_make_boolean_value (has_property);
} /* jerry_has_own_property */

/**
 * Delete a property from an object.
 *
 * @return true  - if property was deleted successfully
 *         false - otherwise
 */
bool
jerry_delete_property (const jerry_value_t obj_val, /**< object value */
                       const jerry_value_t prop_name_val) /**< property name (string value) */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_string (prop_name_val))
  {
    return false;
  }

  ecma_value_t ret_value = ecma_op_object_delete (ecma_get_object_from_value (obj_val),
                                                  ecma_get_string_from_value (prop_name_val),
                                                  false);
  return ecma_is_value_true (ret_value);
} /* jerry_delete_property */

/**
 * Delete indexed property from the specified object.
 *
 * @return true  - if property was deleted successfully
 *         false - otherwise
 */
bool
jerry_delete_property_by_index (const jerry_value_t obj_val, /**< object value */
                                uint32_t index) /**< index to be written */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val))
  {
    return false;
  }

  ecma_string_t *str_idx_p = ecma_new_ecma_string_from_uint32 (index);
  ecma_value_t ret_value = ecma_op_object_delete (ecma_get_object_from_value (obj_val),
                                                  str_idx_p,
                                                  false);
  ecma_deref_ecma_string (str_idx_p);

  return ecma_is_value_true (ret_value);
} /* jerry_delete_property_by_index */

/**
 * Get value of a property to the specified object with the given name.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the property - if success
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_get_property (const jerry_value_t obj_val, /**< object value */
                    const jerry_value_t prop_name_val) /**< property name (string value) */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_string (prop_name_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  jerry_value_t ret_value = ecma_op_object_get (ecma_get_object_from_value (obj_val),
                                                ecma_get_string_from_value (prop_name_val));
  return jerry_return (ret_value);
} /* jerry_get_property */

/**
 * Get value by an index from the specified object.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the property specified by the index - if success
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_get_property_by_index (const jerry_value_t obj_val, /**< object value */
                             uint32_t index) /**< index to be written */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  ecma_string_t *str_idx_p = ecma_new_ecma_string_from_uint32 (index);
  ecma_value_t ret_value = ecma_op_object_get (ecma_get_object_from_value (obj_val), str_idx_p);
  ecma_deref_ecma_string (str_idx_p);

  return jerry_return (ret_value);
} /* jerry_get_property_by_index */

/**
 * Set a property to the specified object with the given name.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return true value - if the operation was successful
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_set_property (const jerry_value_t obj_val, /**< object value */
                    const jerry_value_t prop_name_val, /**< property name (string value) */
                    const jerry_value_t value_to_set) /**< value to set */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (value_to_set)
      || !ecma_is_value_object (obj_val)
      || !ecma_is_value_string (prop_name_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  return jerry_return (ecma_op_object_put (ecma_get_object_from_value (obj_val),
                                           ecma_get_string_from_value (prop_name_val),
                                           value_to_set,
                                           true));
} /* jerry_set_property */

/**
 * Set indexed value in the specified object
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return true value - if the operation was successful
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_set_property_by_index (const jerry_value_t obj_val, /**< object value */
                             uint32_t index, /**< index to be written */
                             const jerry_value_t value_to_set) /**< value to set */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (value_to_set)
      || !ecma_is_value_object (obj_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  ecma_string_t *str_idx_p = ecma_new_ecma_string_from_uint32 ((uint32_t) index);
  ecma_value_t ret_value = ecma_op_object_put (ecma_get_object_from_value (obj_val),
                                               str_idx_p,
                                               value_to_set,
                                               true);
  ecma_deref_ecma_string (str_idx_p);

  return jerry_return (ret_value);
} /* jerry_set_property_by_index */

/**
 * Initialize property descriptor.
 */
void
jerry_init_property_descriptor_fields (jerry_property_descriptor_t *prop_desc_p) /**< [out] property descriptor */
{
  prop_desc_p->is_value_defined = false;
  prop_desc_p->value = ECMA_VALUE_UNDEFINED;
  prop_desc_p->is_writable_defined = false;
  prop_desc_p->is_writable = false;
  prop_desc_p->is_enumerable_defined = false;
  prop_desc_p->is_enumerable = false;
  prop_desc_p->is_configurable_defined = false;
  prop_desc_p->is_configurable = false;
  prop_desc_p->is_get_defined = false;
  prop_desc_p->getter = ECMA_VALUE_UNDEFINED;
  prop_desc_p->is_set_defined = false;
  prop_desc_p->setter = ECMA_VALUE_UNDEFINED;
} /* jerry_init_property_descriptor_fields */

/**
 * Define a property to the specified object with the given name.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return true value - if the operation was successful
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_define_own_property (const jerry_value_t obj_val, /**< object value */
                           const jerry_value_t prop_name_val, /**< property name (string value) */
                           const jerry_property_descriptor_t *prop_desc_p) /**< property descriptor */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_string (prop_name_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  if ((prop_desc_p->is_writable_defined || prop_desc_p->is_value_defined)
      && (prop_desc_p->is_get_defined || prop_desc_p->is_set_defined))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  ecma_property_descriptor_t prop_desc = ecma_make_empty_property_descriptor ();

  prop_desc.is_enumerable_defined = ECMA_BOOL_TO_BITFIELD (prop_desc_p->is_enumerable_defined);
  prop_desc.is_enumerable = ECMA_BOOL_TO_BITFIELD (prop_desc_p->is_enumerable_defined ? prop_desc_p->is_enumerable
                                                                                      : false);

  prop_desc.is_configurable_defined = ECMA_BOOL_TO_BITFIELD (prop_desc_p->is_configurable_defined);
  prop_desc.is_configurable = ECMA_BOOL_TO_BITFIELD (prop_desc_p->is_configurable_defined ? prop_desc_p->is_configurable
                                                                                          : false);

  /* Copy data property info. */
  prop_desc.is_value_defined = ECMA_BOOL_TO_BITFIELD (prop_desc_p->is_value_defined);

  if (prop_desc_p->is_value_defined)
  {
    if (ecma_is_value_error_reference (prop_desc_p->value))
    {
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
    }

    prop_desc.value = prop_desc_p->value;
  }

  prop_desc.is_writable_defined = ECMA_BOOL_TO_BITFIELD (prop_desc_p->is_writable_defined);
  prop_desc.is_writable = ECMA_BOOL_TO_BITFIELD (prop_desc_p->is_writable_defined ? prop_desc_p->is_writable
                                                                                  : false);

  /* Copy accessor property info. */
  if (prop_desc_p->is_get_defined)
  {
    ecma_value_t getter = prop_desc_p->getter;
    prop_desc.is_get_defined = true;

    if (ecma_is_value_error_reference (getter))
    {
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
    }

    if (ecma_op_is_callable (getter))
    {
      prop_desc.get_p = ecma_get_object_from_value (getter);
    }
    else if (!ecma_is_value_null (getter))
    {
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
    }
  }

  if (prop_desc_p->is_set_defined)
  {
    ecma_value_t setter = prop_desc_p->setter;
    prop_desc.is_set_defined = true;

    if (ecma_is_value_error_reference (setter))
    {
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
    }

    if (ecma_op_is_callable (setter))
    {
      prop_desc.set_p = ecma_get_object_from_value (setter);
    }
    else if (!ecma_is_value_null (setter))
    {
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
    }
  }

  return ecma_op_object_define_own_property (ecma_get_object_from_value (obj_val),
                                             ecma_get_string_from_value (prop_name_val),
                                             &prop_desc,
                                             true);
} /* jerry_define_own_property */

/**
 * Construct property descriptor from specified property.
 *
 * @return true - if success, the prop_desc_p fields contains the property info
 *         false - otherwise, the prop_desc_p is unchanged
 */
bool
jerry_get_own_property_descriptor (const jerry_value_t  obj_val, /**< object value */
                                   const jerry_value_t prop_name_val, /**< property name (string value) */
                                   jerry_property_descriptor_t *prop_desc_p) /**< property descriptor */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_string (prop_name_val))
  {
    return false;
  }

  ecma_property_descriptor_t prop_desc;

  if (!ecma_op_object_get_own_property_descriptor (ecma_get_object_from_value (obj_val),
                                                   ecma_get_string_from_value (prop_name_val),
                                                   &prop_desc))
  {
    return false;
  }

  prop_desc_p->is_configurable_defined = true;
  prop_desc_p->is_configurable = prop_desc.is_configurable;
  prop_desc_p->is_enumerable_defined = true;
  prop_desc_p->is_enumerable = prop_desc.is_enumerable;

  prop_desc_p->is_writable_defined = prop_desc.is_writable_defined;
  prop_desc_p->is_writable = prop_desc.is_writable_defined ? prop_desc.is_writable : false;

  prop_desc_p->is_value_defined = prop_desc.is_value_defined;
  prop_desc_p->is_get_defined = prop_desc.is_get_defined;
  prop_desc_p->is_set_defined = prop_desc.is_set_defined;

  prop_desc_p->value = ECMA_VALUE_UNDEFINED;
  prop_desc_p->getter = ECMA_VALUE_UNDEFINED;
  prop_desc_p->setter = ECMA_VALUE_UNDEFINED;

  if (prop_desc.is_value_defined)
  {
    prop_desc_p->value = prop_desc.value;
  }

  if (prop_desc.is_get_defined)
  {
    if (prop_desc.get_p != NULL)
    {
      prop_desc_p->getter = ecma_make_object_value (prop_desc.get_p);
    }
    else
    {
      prop_desc_p->getter = ECMA_VALUE_NULL;
    }
  }

  if (prop_desc.is_set_defined)
  {
    if (prop_desc.set_p != NULL)
    {
      prop_desc_p->setter = ecma_make_object_value (prop_desc.set_p);
    }
    else
    {
      prop_desc_p->setter = ECMA_VALUE_NULL;
    }
  }

  return true;
} /* jerry_get_own_property_descriptor */

/**
 * Free fields of property descriptor (setter, getter and value).
 */
void
jerry_free_property_descriptor_fields (const jerry_property_descriptor_t *prop_desc_p) /**< property descriptor */
{
  if (prop_desc_p->is_value_defined)
  {
    jerry_release_value (prop_desc_p->value);
  }

  if (prop_desc_p->is_get_defined)
  {
    jerry_release_value (prop_desc_p->getter);
  }

  if (prop_desc_p->is_set_defined)
  {
    jerry_release_value (prop_desc_p->setter);
  }
} /* jerry_free_property_descriptor_fields */

/**
 * Invoke function specified by a function value
 *
 * Note:
 *      - returned value must be freed with jerry_release_value, when it is no longer needed.
 *      - If function is invoked as constructor, it should support [[Construct]] method,
 *        otherwise, if function is simply called - it should support [[Call]] method.
 *
 * @return returned jerry value of the invoked function
 */
static jerry_value_t
jerry_invoke_function (bool is_invoke_as_constructor, /**< true - invoke function as constructor
                                                       *          (this_arg_p should be NULL, as it is ignored),
                                                       *   false - perform function call */
                       const jerry_value_t func_obj_val, /**< function object to call */
                       const jerry_value_t this_val, /**< object value of 'this' binding */
                       const jerry_value_t args_p[], /**< function's call arguments */
                       const jerry_size_t args_count) /**< number of the arguments */
{
  JERRY_ASSERT (args_count == 0 || args_p != NULL);

  if (ecma_is_value_error_reference (func_obj_val)
      || ecma_is_value_error_reference (this_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (error_value_msg_p)));
  }

  for (uint32_t i = 0; i < args_count; i++)
  {
    if (ecma_is_value_error_reference (args_p[i]))
    {
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (error_value_msg_p)));
    }
  }

  if (is_invoke_as_constructor)
  {
    JERRY_ASSERT (jerry_value_is_constructor (func_obj_val));

    return jerry_return (ecma_op_function_construct (ecma_get_object_from_value (func_obj_val),
                                                     ECMA_VALUE_UNDEFINED,
                                                     args_p,
                                                     args_count));
  }
  else
  {
    JERRY_ASSERT (jerry_value_is_function (func_obj_val));

    return jerry_return (ecma_op_function_call (ecma_get_object_from_value (func_obj_val),
                                                this_val,
                                                args_p,
                                                args_count));
  }
} /* jerry_invoke_function */

/**
 * Call function specified by a function value
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *      error flag must not be set for any arguments of this function.
 *
 * @return returned jerry value of the called function
 */
jerry_value_t
jerry_call_function (const jerry_value_t func_obj_val, /**< function object to call */
                     const jerry_value_t this_val, /**< object for 'this' binding */
                     const jerry_value_t args_p[], /**< function's call arguments */
                     jerry_size_t args_count) /**< number of the arguments */
{
  jerry_assert_api_available ();

  if (jerry_value_is_function (func_obj_val))
  {
    return jerry_invoke_function (false, func_obj_val, this_val, args_p, args_count);
  }

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
} /* jerry_call_function */

/**
 * Construct object value invoking specified function value as a constructor
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *      error flag must not be set for any arguments of this function.
 *
 * @return returned jerry value of the invoked constructor
 */
jerry_value_t
jerry_construct_object (const jerry_value_t func_obj_val, /**< function object to call */
                        const jerry_value_t args_p[], /**< function's call arguments
                                                       *   (NULL if arguments number is zero) */
                        jerry_size_t args_count) /**< number of the arguments */
{
  jerry_assert_api_available ();

  if (jerry_value_is_constructor (func_obj_val))
  {
    ecma_value_t this_val = ECMA_VALUE_UNDEFINED;
    return jerry_invoke_function (true, func_obj_val, this_val, args_p, args_count);
  }

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
} /* jerry_construct_object */

/**
 * Get keys of the specified object value
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return array object value - if success
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_get_object_keys (const jerry_value_t obj_val) /**< object value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  return ecma_builtin_helper_object_get_properties (ecma_get_object_from_value (obj_val), true);
} /* jerry_get_object_keys */

/**
 * Get the prototype of the specified object
 *
 * @return prototype object or null value - if success
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_get_prototype (const jerry_value_t obj_val) /**< object value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  ecma_object_t *proto_obj_p = ecma_get_object_prototype (ecma_get_object_from_value (obj_val));

  if (proto_obj_p == NULL)
  {
    return ECMA_VALUE_NULL;
  }

  return ecma_make_object_value (proto_obj_p);
} /* jerry_get_prototype */

/**
 * Set the prototype of the specified object
 *
 * @return true value - if success
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_set_prototype (const jerry_value_t obj_val, /**< object value */
                     const jerry_value_t proto_obj_val) /**< prototype object value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || ecma_is_value_error_reference (proto_obj_val)
      || (!ecma_is_value_object (proto_obj_val) && !ecma_is_value_null (proto_obj_val)))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  if (ecma_is_value_null (proto_obj_val))
  {
    ECMA_SET_POINTER (ecma_get_object_from_value (obj_val)->prototype_or_outer_reference_cp, NULL);
  }
  else
  {
    ECMA_SET_POINTER (ecma_get_object_from_value (obj_val)->prototype_or_outer_reference_cp,
                      ecma_get_object_from_value (proto_obj_val));
  }

  return ECMA_VALUE_TRUE;
} /* jerry_set_prototype */

/**
 * Traverse objects.
 *
 * @return true - traversal was interrupted by the callback.
 *         false - otherwise - traversal visited all objects.
 */
bool
jerry_objects_foreach (jerry_objects_foreach_t foreach_p, /**< function pointer of the iterator function */
                       void *user_data_p) /**< pointer to user data */
{
  jerry_assert_api_available ();

  JERRY_ASSERT (foreach_p != NULL);

  for (ecma_object_t *iter_p = JERRY_CONTEXT (ecma_gc_objects_p);
       iter_p != NULL;
       iter_p = ECMA_GET_POINTER (ecma_object_t, iter_p->gc_next_cp))
  {
    if (!ecma_is_lexical_environment (iter_p)
        && !foreach_p (ecma_make_object_value (iter_p), user_data_p))
    {
      return true;
    }
  }

  return false;
} /* jerry_objects_foreach */

/**
 * Traverse objects having a given native type info.
 *
 * @return true - traversal was interrupted by the callback.
 *         false - otherwise - traversal visited all objects.
 */
bool
jerry_objects_foreach_by_native_info (const jerry_object_native_info_t *native_info_p, /**< the type info
                                                                                        *   of the native pointer */
                                      jerry_objects_foreach_by_native_info_t foreach_p, /**< function to apply for
                                                                                         *   each matching object */
                                      void *user_data_p) /**< pointer to user data */
{
  jerry_assert_api_available ();

  JERRY_ASSERT (native_info_p != NULL);
  JERRY_ASSERT (foreach_p != NULL);

  ecma_native_pointer_t *native_pointer_p;

  for (ecma_object_t *iter_p = JERRY_CONTEXT (ecma_gc_objects_p);
       iter_p != NULL;
       iter_p = ECMA_GET_POINTER (ecma_object_t, iter_p->gc_next_cp))
  {
    if (!ecma_is_lexical_environment (iter_p))
    {
      native_pointer_p = ecma_get_native_pointer_value (iter_p);
      if (native_pointer_p
          && ((const jerry_object_native_info_t *) native_pointer_p->info_p) == native_info_p
          && !foreach_p (ecma_make_object_value (iter_p), native_pointer_p->data_p, user_data_p))
      {
        return true;
      }
    }
  }

  return false;
} /* jerry_objects_foreach_by_native_info */

/**
 * Get native pointer and its type information, associated with specified object.
 *
 * Note:
 *  If native pointer is present, its type information is returned
 *  in out_native_pointer_p and out_native_info_p.
 *
 * @return true - if there is an associated pointer,
 *         false - otherwise
 */
bool
jerry_get_object_native_pointer (const jerry_value_t obj_val, /**< object to get native pointer from */
                                 void **out_native_pointer_p, /**< [out] native pointer */
                                 const jerry_object_native_info_t **out_native_info_p) /**< [out] the type info
                                                                                        *   of the native pointer */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val))
  {
    return false;
  }

  ecma_native_pointer_t *native_pointer_p;
  native_pointer_p = ecma_get_native_pointer_value (ecma_get_object_from_value (obj_val));

  if (native_pointer_p == NULL)
  {
    return false;
  }

  if (out_native_pointer_p != NULL)
  {
    *out_native_pointer_p = native_pointer_p->data_p;
  }

  if (out_native_info_p != NULL)
  {
    *out_native_info_p = (const jerry_object_native_info_t *) native_pointer_p->info_p;
  }

  return true;
} /* jerry_get_object_native_pointer */

/**
 * Set native pointer and an optional type info for the specified object.
 *
 *
 * Note:
 *      If native pointer was already set for the object, its value is updated.
 *
 * Note:
 *      If a non-NULL free callback is specified in the native type info,
 *      it will be called by the garbage collector when the object is freed.
 *      The type info is always overwrites the previous value, so passing
 *      a NULL value deletes the current type info.
 */
void
jerry_set_object_native_pointer (const jerry_value_t obj_val, /**< object to set native pointer in */
                                 void *native_pointer_p, /**< native pointer */
                                 const jerry_object_native_info_t *native_info_p) /**< object's native type info */
{
  jerry_assert_api_available ();

  if (ecma_is_value_object (obj_val))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (obj_val);

    ecma_create_native_pointer_property (object_p, native_pointer_p, (void *) native_info_p);
  }
} /* jerry_set_object_native_pointer */

/**
 * Applies the given function to the every property in the object.
 *
 * @return true - if object fields traversal was performed successfully, i.e.:
 *                - no unhandled exceptions were thrown in object fields traversal;
 *                - object fields traversal was stopped on callback that returned false;
 *         false - otherwise,
 *                 if getter of field threw a exception or unhandled exceptions were thrown during traversal;
 */
bool
jerry_foreach_object_property (const jerry_value_t obj_val, /**< object value */
                               jerry_object_property_foreach_t foreach_p, /**< foreach function */
                               void *user_data_p) /**< user data for foreach function */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val))
  {
    return false;
  }

  ecma_object_t *object_p = ecma_get_object_from_value (obj_val);
  ecma_collection_header_t *names_p = ecma_op_object_get_property_names (object_p, ECMA_LIST_ENUMERABLE_PROTOTYPE);
  ecma_value_t *ecma_value_p = ecma_collection_iterator_init (names_p);

  ecma_value_t property_value = ECMA_VALUE_EMPTY;

  bool continuous = true;

  while (continuous && ecma_value_p != NULL)
  {
    ecma_string_t *property_name_p = ecma_get_string_from_value (*ecma_value_p);

    property_value = ecma_op_object_get (object_p, property_name_p);

    if (ECMA_IS_VALUE_ERROR (property_value))
    {
      break;
    }

    continuous = foreach_p (*ecma_value_p, property_value, user_data_p);
    ecma_free_value (property_value);

    ecma_value_p = ecma_collection_iterator_next (ecma_value_p);
  }

  ecma_free_values_collection (names_p, 0);

  if (!ECMA_IS_VALUE_ERROR (property_value))
  {
    return true;
  }

  ecma_free_value (JERRY_CONTEXT (error_value));
  return false;
} /* jerry_foreach_object_property */

/**
 * Resolve or reject the promise with an argument.
 *
 * @return undefined value - if success
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_resolve_or_reject_promise (jerry_value_t promise, /**< the promise value */
                                 jerry_value_t argument, /**< the argument */
                                 bool is_resolve) /**< whether the promise should be resolved or rejected */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
  if (!ecma_is_value_object (promise) || !ecma_is_promise (ecma_get_object_from_value (promise)))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (wrong_args_msg_p)));
  }

  lit_magic_string_id_t prop_name = (is_resolve ? LIT_INTERNAL_MAGIC_STRING_RESOLVE_FUNCTION
                                                : LIT_INTERNAL_MAGIC_STRING_REJECT_FUNCTION);

  ecma_value_t function = ecma_op_object_get_by_magic_id (ecma_get_object_from_value (promise), prop_name);

  ecma_value_t ret = ecma_op_function_call (ecma_get_object_from_value (function),
                                            ECMA_VALUE_UNDEFINED,
                                            &argument,
                                            1);

  ecma_free_value (function);

  return ret;
#else /* CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
  JERRY_UNUSED (promise);
  JERRY_UNUSED (argument);
  JERRY_UNUSED (is_resolve);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Promise not supported.")));
#endif /* !CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
} /* jerry_resolve_or_reject_promise */

/**
 * Validate UTF-8 string
 *
 * @return true - if UTF-8 string is well-formed
 *         false - otherwise
 */
bool
jerry_is_valid_utf8_string (const jerry_char_t *utf8_buf_p, /**< UTF-8 string */
                            jerry_size_t buf_size) /**< string size */
{
  return lit_is_valid_utf8_string ((lit_utf8_byte_t *) utf8_buf_p,
                                   (lit_utf8_size_t) buf_size);
} /* jerry_is_valid_utf8_string */

/**
 * Validate CESU-8 string
 *
 * @return true - if CESU-8 string is well-formed
 *         false - otherwise
 */
bool
jerry_is_valid_cesu8_string (const jerry_char_t *cesu8_buf_p, /**< CESU-8 string */
                             jerry_size_t buf_size) /**< string size */
{
  return lit_is_valid_cesu8_string ((lit_utf8_byte_t *) cesu8_buf_p,
                                    (lit_utf8_size_t) buf_size);
} /* jerry_is_valid_cesu8_string */

/**
 * Allocate memory on the engine's heap.
 *
 * Note:
 *      This function may take away memory from the executed JavaScript code.
 *      If any other dynamic memory allocation API is available (e.g., libc
 *      malloc), it should be used instead.
 *
 * @return allocated memory on success
 *         NULL otherwise
 */
void *
jerry_heap_alloc (size_t size) /**< size of the memory block */
{
  jerry_assert_api_available ();

  return jmem_heap_alloc_block_null_on_error (size);
} /* jerry_heap_alloc */

/**
 * Free memory allocated on the engine's heap.
 */
void
jerry_heap_free (void *mem_p, /**< value returned by jerry_heap_alloc */
                 size_t size) /**< same size as passed to jerry_heap_alloc */
{
  jerry_assert_api_available ();

  jmem_heap_free_block (mem_p, size);
} /* jerry_heap_free */

/**
 * Create an external engine context.
 *
 * @return the pointer to the context.
 */
jerry_context_t *
jerry_create_context (uint32_t heap_size, /**< the size of heap */
                      jerry_context_alloc_t alloc, /**< the alloc function */
                      void *cb_data_p) /**< the cb_data for alloc function */
{
  JERRY_UNUSED (heap_size);

#ifdef JERRY_ENABLE_EXTERNAL_CONTEXT

  size_t total_size = sizeof (jerry_context_t) + JMEM_ALIGNMENT;

#ifndef JERRY_SYSTEM_ALLOCATOR
  heap_size = JERRY_ALIGNUP (heap_size, JMEM_ALIGNMENT);

  /* Minimum heap size is 1Kbyte. */
  if (heap_size < 1024)
  {
    return NULL;
  }

  total_size += heap_size;
#endif /* !JERRY_SYSTEM_ALLOCATOR */

  total_size = JERRY_ALIGNUP (total_size, JMEM_ALIGNMENT);

  jerry_context_t *context_p = (jerry_context_t *) alloc (total_size, cb_data_p);

  if (context_p == NULL)
  {
    return NULL;
  }

  memset (context_p, 0, total_size);

  uintptr_t context_ptr = ((uintptr_t) context_p) + sizeof (jerry_context_t);
  context_ptr = JERRY_ALIGNUP (context_ptr, (uintptr_t) JMEM_ALIGNMENT);

  uint8_t *byte_p = (uint8_t *) context_ptr;

#ifndef JERRY_SYSTEM_ALLOCATOR
  context_p->heap_p = (jmem_heap_t *) byte_p;
  context_p->heap_size = heap_size;
  byte_p += heap_size;
#endif /* !JERRY_SYSTEM_ALLOCATOR */

  JERRY_ASSERT (byte_p <= ((uint8_t *) context_p) + total_size);

  JERRY_UNUSED (byte_p);
  return context_p;

#else /* !JERRY_ENABLE_EXTERNAL_CONTEXT */

  JERRY_UNUSED (alloc);
  JERRY_UNUSED (cb_data_p);

  return NULL;

#endif /* JERRY_ENABLE_EXTERNAL_CONTEXT */
} /* jerry_create_context */

/**
 * If JERRY_VM_EXEC_STOP is defined the callback passed to this function is
 * periodically called with the user_p argument. If frequency is greater
 * than 1, the callback is only called at every frequency ticks.
 */
void
jerry_set_vm_exec_stop_callback (jerry_vm_exec_stop_callback_t stop_cb, /**< periodically called user function */
                                 void *user_p, /**< pointer passed to the function */
                                 uint32_t frequency) /**< frequency of the function call */
{
#ifdef JERRY_VM_EXEC_STOP
  if (frequency == 0)
  {
    frequency = 1;
  }

  JERRY_CONTEXT (vm_exec_stop_frequency) = frequency;
  JERRY_CONTEXT (vm_exec_stop_counter) = frequency;
  JERRY_CONTEXT (vm_exec_stop_user_p) = user_p;
  JERRY_CONTEXT (vm_exec_stop_cb) = stop_cb;
#else /* !JERRY_VM_EXEC_STOP */
  JERRY_UNUSED (stop_cb);
  JERRY_UNUSED (user_p);
  JERRY_UNUSED (frequency);
#endif /* JERRY_VM_EXEC_STOP */
} /* jerry_set_vm_exec_stop_callback */

/**
 * Get backtrace. The backtrace is an array of strings where
 * each string contains the position of the corresponding frame.
 * The array length is zero if the backtrace is not available.
 *
 * @return array value
 */
jerry_value_t
jerry_get_backtrace (uint32_t max_depth) /**< depth limit of the backtrace */
{
  return vm_get_backtrace (max_depth);
} /* jerry_get_backtrace */

/**
 * Check if the given value is an ArrayBuffer object.
 *
 * @return true - if it is an ArrayBuffer object
 *         false - otherwise
 */
bool
jerry_value_is_arraybuffer (const jerry_value_t value) /**< value to check if it is an ArrayBuffer */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  return ecma_is_arraybuffer (value);
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (value);
  return false;
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
} /* jerry_value_is_arraybuffer */

/**
 * Creates an ArrayBuffer object with the given length (size).
 *
 * Notes:
 *      * the length is specified in bytes.
 *      * returned value must be freed with jerry_release_value, when it is no longer needed.
 *      * if the typed arrays are disabled this will return a TypeError.
 *
 * @return value of the constructed ArrayBuffer object
 */
jerry_value_t
jerry_create_arraybuffer (const jerry_length_t size) /**< size of the ArrayBuffer to create */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  return jerry_return (ecma_make_object_value (ecma_arraybuffer_new_object (size)));
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (size);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("ArrayBuffer not supported.")));
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
} /* jerry_create_arraybuffer */

/**
 * Creates an ArrayBuffer object with user specified buffer.
 *
 * Notes:
 *     * the size is specified in bytes.
 *     * the buffer passed should be at least the specified bytes big.
 *     * if the typed arrays are disabled this will return a TypeError.
 *     * if the size is zero or the buffer_p is a null pointer this will return a RangeError.
 *
 * @return value of the construced ArrayBuffer object
 */
jerry_value_t
jerry_create_arraybuffer_external (const jerry_length_t size, /**< size of the buffer to used */
                                   uint8_t *buffer_p, /**< buffer to use as the ArrayBuffer's backing */
                                   jerry_object_native_free_callback_t free_cb) /**< buffer free callback */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  if (size == 0 || buffer_p == NULL)
  {
    return jerry_throw (ecma_raise_range_error (ECMA_ERR_MSG ("invalid buffer size or storage reference")));
  }

  ecma_object_t *arraybuffer = ecma_arraybuffer_new_object_external (size,
                                                                     buffer_p,
                                                                     (ecma_object_native_free_callback_t) free_cb);
  return jerry_return (ecma_make_object_value (arraybuffer));
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (size);
  JERRY_UNUSED (buffer_p);
  JERRY_UNUSED (free_cb);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("ArrayBuffer not supported.")));
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
} /* jerry_create_arraybuffer_external */

/**
 * Copy bytes into the ArrayBuffer from a buffer.
 *
 * Note:
 *     * if the object passed is not an ArrayBuffer will return 0.
 *
 * @return number of bytes copied into the ArrayBuffer.
 */
jerry_length_t
jerry_arraybuffer_write (const jerry_value_t value, /**< target ArrayBuffer */
                         jerry_length_t offset, /**< start offset of the ArrayBuffer */
                         const uint8_t *buf_p, /**< buffer to copy from */
                         jerry_length_t buf_size) /**< number of bytes to copy from the buffer */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  if (!ecma_is_arraybuffer (value))
  {
    return 0;
  }

  ecma_object_t *buffer_p = ecma_get_object_from_value (value);
  jerry_length_t length = ecma_arraybuffer_get_length (buffer_p);

  if (offset >= length)
  {
    return 0;
  }

  jerry_length_t copy_count = JERRY_MIN (length - offset, buf_size);

  if (copy_count > 0)
  {
    lit_utf8_byte_t *mem_buffer_p = ecma_arraybuffer_get_buffer (buffer_p);

    memcpy ((void *) (mem_buffer_p + offset), (void *) buf_p, copy_count);
  }

  return copy_count;
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (value);
  JERRY_UNUSED (offset);
  JERRY_UNUSED (buf_p);
  JERRY_UNUSED (buf_size);
  return 0;
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
} /* jerry_arraybuffer_write */

/**
 * Copy bytes from a buffer into an ArrayBuffer.
 *
 * Note:
 *     * if the object passed is not an ArrayBuffer will return 0.
 *
 * @return number of bytes read from the ArrayBuffer.
 */
jerry_length_t
jerry_arraybuffer_read (const jerry_value_t value, /**< ArrayBuffer to read from */
                        jerry_length_t offset, /**< start offset of the ArrayBuffer */
                        uint8_t *buf_p, /**< destination buffer to copy to */
                        jerry_length_t buf_size) /**< number of bytes to copy into the buffer */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  if (!ecma_is_arraybuffer (value))
  {
    return 0;
  }

  ecma_object_t *buffer_p = ecma_get_object_from_value (value);
  jerry_length_t length = ecma_arraybuffer_get_length (buffer_p);

  if (offset >= length)
  {
    return 0;
  }

  jerry_length_t copy_count = JERRY_MIN (length - offset, buf_size);

  if (copy_count > 0)
  {
    lit_utf8_byte_t *mem_buffer_p = ecma_arraybuffer_get_buffer (buffer_p);

    memcpy ((void *) buf_p, (void *) (mem_buffer_p + offset), copy_count);
  }

  return copy_count;
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (value);
  JERRY_UNUSED (offset);
  JERRY_UNUSED (buf_p);
  JERRY_UNUSED (buf_size);
  return 0;
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
} /* jerry_arraybuffer_read */

/**
 * Get the length (size) of the ArrayBuffer in bytes.
 *
 * Note:
 *     This is the 'byteLength' property of an ArrayBuffer.
 *
 * @return the length of the ArrayBuffer in bytes.
 */
jerry_length_t
jerry_get_arraybuffer_byte_length (const jerry_value_t value) /**< ArrayBuffer */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  if (ecma_is_arraybuffer (value))
  {
    ecma_object_t *buffer_p = ecma_get_object_from_value (value);
    return ecma_arraybuffer_get_length (buffer_p);
  }
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (value);
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  return 0;
} /* jerry_get_arraybuffer_byte_length */

/**
 * Get a pointer for the start of the ArrayBuffer.
 *
 * Note:
 *    * Only valid for ArrayBuffers created with jerry_create_arraybuffer_external.
 *    * This is a high-risk operation as the bounds are not checked
 *      when accessing the pointer elements.
 *    * jerry_release_value must be called on the ArrayBuffer when the pointer is no longer needed.
 *
 * @return pointer to the back-buffer of the ArrayBuffer.
 *         pointer is NULL if the parameter is not an ArrayBuffer with external memory
             or it is not an ArrayBuffer at all.
 */
uint8_t *
jerry_get_arraybuffer_pointer (const jerry_value_t value) /**< Array Buffer to use */
{
  jerry_assert_api_available ();
#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  if (!ecma_is_arraybuffer (value))
  {
    return NULL;
  }

  ecma_object_t *buffer_p = ecma_get_object_from_value (value);
  if (ECMA_ARRAYBUFFER_HAS_EXTERNAL_MEMORY (buffer_p))
  {
    jerry_acquire_value (value);
    lit_utf8_byte_t *mem_buffer_p = ecma_arraybuffer_get_buffer (buffer_p);
    return (uint8_t *const) mem_buffer_p;
  }
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (value);
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */

  return NULL;
} /* jerry_get_arraybuffer_pointer */


/**
 * TypedArray related functions
 */

/**
 * Check if the given value is a TypedArray object.
 *
 * @return true - if it is a TypedArray object
 *         false - otherwise
 */
bool
jerry_value_is_typedarray (jerry_value_t value) /**< value to check if it is a TypedArray */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  return ecma_is_typedarray (value);
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (value);
  return false;
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
} /* jerry_value_is_typedarray */

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
/**
 * TypedArray mapping type
 */
typedef struct
{
  jerry_typedarray_type_t api_type; /**< api type */
  ecma_builtin_id_t prototype_id; /**< prototype ID */
  lit_magic_string_id_t lit_id; /**< literal ID */
  uint8_t element_size_shift; /**< element size shift */
} jerry_typedarray_mapping_t;

/**
 * List of TypedArray mappings
 */
static jerry_typedarray_mapping_t jerry_typedarray_mappings[] =
{
#define TYPEDARRAY_ENTRY(NAME, LIT_NAME, SIZE_SHIFT) \
  { JERRY_TYPEDARRAY_ ## NAME, ECMA_BUILTIN_ID_ ## NAME ## ARRAY_PROTOTYPE, \
    LIT_MAGIC_STRING_ ## LIT_NAME ## _ARRAY_UL, SIZE_SHIFT }

  TYPEDARRAY_ENTRY (UINT8, UINT8, 0),
  TYPEDARRAY_ENTRY (UINT8CLAMPED, UINT8_CLAMPED, 0),
  TYPEDARRAY_ENTRY (INT8, INT8, 0),
  TYPEDARRAY_ENTRY (UINT16, UINT16, 1),
  TYPEDARRAY_ENTRY (INT16, INT16, 1),
  TYPEDARRAY_ENTRY (UINT32, UINT32, 2),
  TYPEDARRAY_ENTRY (INT32, INT32, 2),
  TYPEDARRAY_ENTRY (FLOAT32, FLOAT32, 2),
#if CONFIG_ECMA_NUMBER_TYPE == CONFIG_ECMA_NUMBER_FLOAT64
  TYPEDARRAY_ENTRY (FLOAT64, FLOAT64, 3),
#endif

#undef TYPEDARRAY_ENTRY
};

/**
 * Helper function to get the TypedArray prototype, literal id, and element size shift
 * information.
 *
 * @return true - if the TypedArray information was found
 *         false - if there is no such TypedArray type
 */
static bool
jerry_typedarray_find_by_type (jerry_typedarray_type_t type_name, /**< type of the TypedArray */
                               ecma_builtin_id_t *prototype_id, /**< [out] found prototype object id */
                               lit_magic_string_id_t *lit_id, /**< [out] found literal id */
                               uint8_t *element_size_shift) /**< [out] found element size shift value */
{
  JERRY_ASSERT (prototype_id != NULL);
  JERRY_ASSERT (lit_id != NULL);
  JERRY_ASSERT (element_size_shift != NULL);

  for (uint32_t i = 0; i < sizeof (jerry_typedarray_mappings) / sizeof (jerry_typedarray_mappings[0]); i++)
  {
    if (type_name == jerry_typedarray_mappings[i].api_type)
    {
      *prototype_id = jerry_typedarray_mappings[i].prototype_id;
      *lit_id = jerry_typedarray_mappings[i].lit_id;
      *element_size_shift = jerry_typedarray_mappings[i].element_size_shift;
      return true;
    }
  }

  return false;
} /* jerry_typedarray_find_by_type */

#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */

/**
 * Create a TypedArray object with a given type and length.
 *
 * Notes:
 *      * returns TypeError if an incorrect type (type_name) is specified.
 *      * byteOffset property will be set to 0.
 *      * byteLength property will be a multiple of the length parameter (based on the type).
 *
 * @return - new TypedArray object
 */
jerry_value_t
jerry_create_typedarray (jerry_typedarray_type_t type_name, /**< type of TypedArray to create */
                         jerry_length_t length) /**< element count of the new TypedArray */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  ecma_builtin_id_t prototype_id = 0;
  lit_magic_string_id_t lit_id = 0;
  uint8_t element_size_shift = 0;

  if (!jerry_typedarray_find_by_type (type_name, &prototype_id, &lit_id, &element_size_shift))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("incorrect type for TypedArray.")));
  }

  ecma_object_t *prototype_obj_p = ecma_builtin_get (prototype_id);

  ecma_value_t array_value = ecma_typedarray_create_object_with_length (length,
                                                                        prototype_obj_p,
                                                                        element_size_shift,
                                                                        lit_id);

  JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (array_value));

  return array_value;
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (type_name);
  JERRY_UNUSED (length);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("TypedArray not supported.")));
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
} /* jerry_create_typedarray */

/**
 * Create a TypedArray object using the given arraybuffer and size information.
 *
 * Notes:
 *      * returns TypeError if an incorrect type (type_name) is specified.
 *      * this is the 'new %TypedArray%(arraybuffer, byteOffset, length)' equivalent call.
 *
 * @return - new TypedArray object
 */
jerry_value_t
jerry_create_typedarray_for_arraybuffer_sz (jerry_typedarray_type_t type_name, /**< type of TypedArray to create */
                                            const jerry_value_t arraybuffer, /**< ArrayBuffer to use */
                                            jerry_length_t byte_offset, /**< offset for the ArrayBuffer */
                                            jerry_length_t length) /**< number of elements to use from ArrayBuffer */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  ecma_builtin_id_t prototype_id = 0;
  lit_magic_string_id_t lit_id = 0;
  uint8_t element_size_shift = 0;

  if (!jerry_typedarray_find_by_type (type_name, &prototype_id, &lit_id, &element_size_shift))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("incorrect type for TypedArray.")));
  }

  if (!ecma_is_arraybuffer (arraybuffer))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an ArrayBuffer")));
  }

  ecma_object_t *prototype_obj_p = ecma_builtin_get (prototype_id);
  ecma_value_t arguments_p[3] =
  {
    arraybuffer,
    ecma_make_uint32_value (byte_offset),
    ecma_make_uint32_value (length)
  };

  ecma_value_t array_value = ecma_op_create_typedarray (arguments_p, 3, prototype_obj_p, element_size_shift, lit_id);
  ecma_free_value (arguments_p[1]);
  ecma_free_value (arguments_p[2]);

  return jerry_return (array_value);
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (type_name);
  JERRY_UNUSED (arraybuffer);
  JERRY_UNUSED (byte_offset);
  JERRY_UNUSED (length);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("TypedArray not supported.")));
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
} /* jerry_create_typedarray_for_arraybuffer_sz */

/**
 * Create a TypedArray object using the given arraybuffer and size information.
 *
 * Notes:
 *      * returns TypeError if an incorrect type (type_name) is specified.
 *      * this is the 'new %TypedArray%(arraybuffer)' equivalent call.
 *
 * @return - new TypedArray object
 */
jerry_value_t
jerry_create_typedarray_for_arraybuffer (jerry_typedarray_type_t type_name, /**< type of TypedArray to create */
                                         const jerry_value_t arraybuffer) /**< ArrayBuffer to use */
{
  jerry_assert_api_available ();
  jerry_length_t byteLength = jerry_get_arraybuffer_byte_length (arraybuffer);
  return jerry_create_typedarray_for_arraybuffer_sz (type_name, arraybuffer, 0, byteLength);
} /* jerry_create_typedarray_for_arraybuffer */

/**
 * Get the type of the TypedArray.
 *
 * @return - type of the TypedArray
 *         - JERRY_TYPEDARRAY_INVALID if the argument is not a TypedArray
 */
jerry_typedarray_type_t
jerry_get_typedarray_type (jerry_value_t value) /**< object to get the TypedArray type */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  if (!ecma_is_typedarray (value))
  {
    return JERRY_TYPEDARRAY_INVALID;
  }

  ecma_object_t *array_p = ecma_get_object_from_value (value);

  lit_magic_string_id_t class_name_id = ecma_object_get_class_name (array_p);
  for (uint32_t i = 0; i < sizeof (jerry_typedarray_mappings) / sizeof (jerry_typedarray_mappings[0]); i++)
  {
    if (class_name_id == jerry_typedarray_mappings[i].lit_id)
    {
      return jerry_typedarray_mappings[i].api_type;
    }
  }
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (value);
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */

  return JERRY_TYPEDARRAY_INVALID;
} /* jerry_get_typedarray_type */

/**
 * Get the element count of the TypedArray.
 *
 * @return length of the TypedArray.
 */
jerry_length_t
jerry_get_typedarray_length (jerry_value_t value) /**< TypedArray to query */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  if (ecma_is_typedarray (value))
  {
    ecma_object_t *array_p = ecma_get_object_from_value (value);
    return ecma_typedarray_get_length (array_p);
  }
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (value);
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */

  return 0;
} /* jerry_get_typedarray_length */

/**
 * Get the underlying ArrayBuffer from a TypedArray.
 *
 * Additionally the byteLength and byteOffset properties are also returned
 * which were specified when the TypedArray was created.
 *
 * Note:
 *     the returned value must be freed with a jerry_release_value call
 *
 * @return ArrayBuffer of a TypedArray
 *         TypeError if the object is not a TypedArray.
 */
jerry_value_t
jerry_get_typedarray_buffer (jerry_value_t value, /**< TypedArray to get the arraybuffer from */
                             jerry_length_t *byte_offset, /**< [out] byteOffset property */
                             jerry_length_t *byte_length) /**< [out] byteLength property */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
  if (!ecma_is_typedarray (value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Object is not a TypedArray.")));
  }

  ecma_object_t *array_p = ecma_get_object_from_value (value);
  uint8_t shift = ecma_typedarray_get_element_size_shift (array_p);

  if (byte_length != NULL)
  {
    *byte_length = (jerry_length_t) (ecma_typedarray_get_length (array_p) << shift);
  }

  if (byte_offset != NULL)
  {
    *byte_offset = (jerry_length_t) ecma_typedarray_get_offset (array_p);
  }

  ecma_object_t *arraybuffer_p = ecma_typedarray_get_arraybuffer (array_p);
  ecma_ref_object (arraybuffer_p);
  return jerry_return (ecma_make_object_value (arraybuffer_p));
#else /* CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
  JERRY_UNUSED (value);
  JERRY_UNUSED (byte_length);
  JERRY_UNUSED (byte_offset);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("TypedArray is not supported.")));
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
} /* jerry_get_typedarray_buffer */

/**
 * Create an object from JSON
 *
 * Note:
 *      The returned value must be freed with jerry_release_value
 * @return jerry_value_t from json formated string or an error massage
 */
jerry_value_t
jerry_json_parse (const jerry_char_t *string_p, /**< json string */
                  jerry_size_t string_size) /**< json string size */
{
  jerry_assert_api_available ();

#ifndef CONFIG_DISABLE_JSON_BUILTIN
  ecma_value_t ret_value = ecma_builtin_json_parse_buffer (string_p, string_size);

  if (ecma_is_value_undefined (ret_value))
  {
    ret_value = jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG ("JSON string parse error.")));
  }

  return ret_value;
#else /* CONFIG_DISABLE_JSON_BUILTIN */
  JERRY_UNUSED (string_p);
  JERRY_UNUSED (string_size);

  return jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG ("The JSON has been disabled.")));
#endif /* !CONFIG_DISABLE_JSON_BUILTIN */
} /* jerry_json_parse */

/**
 * Create a Json formated string from an object
 *
 * Note:
 *      The returned value must be freed with jerry_release_value
 * @return json formated jerry_value_t or an error massage
 */
jerry_value_t
jerry_json_stringify (const jerry_value_t object_to_stringify) /**< a jerry_object_t to stringify */
{
  jerry_assert_api_available ();
#ifndef CONFIG_DISABLE_JSON_BUILTIN
  ecma_value_t ret_value = ecma_builtin_json_string_from_object (object_to_stringify);

  if (ecma_is_value_undefined (ret_value))
  {
    ret_value = jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG ("JSON stringify error.")));
  }

  return ret_value;
#else /* CONFIG_DISABLE_JSON_BUILTIN */
  JERRY_UNUSED (object_to_stringify);

  return jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG ("The JSON has been disabled.")));
#endif /* !CONFIG_DISABLE_JSON_BUILTIN */
} /* jerry_json_stringify */

/**
 * @}
 */

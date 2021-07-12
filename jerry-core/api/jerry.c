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
#include <math.h>

#include "debugger.h"
#include "ecma-alloc.h"
#include "ecma-array-object.h"
#include "ecma-arraybuffer-object.h"
#include "ecma-bigint.h"
#include "ecma-builtin-helpers.h"
#include "ecma-builtins.h"
#include "ecma-comparison.h"
#include "ecma-container-object.h"
#include "ecma-dataview-object.h"
#include "ecma-exceptions.h"
#include "ecma-eval.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-helpers.h"
#include "ecma-init-finalize.h"
#include "ecma-iterator-object.h"
#include "ecma-lex-env.h"
#include "lit-char-helpers.h"
#include "ecma-literal-storage.h"
#include "ecma-objects.h"
#include "ecma-objects-general.h"
#include "ecma-regexp-object.h"
#include "ecma-promise-object.h"
#include "ecma-proxy-object.h"
#include "ecma-symbol-object.h"
#include "ecma-typedarray-object.h"
#include "opcodes.h"
#include "jcontext.h"
#include "jerryscript.h"
#include "jerryscript-debugger-transport.h"
#include "jmem.h"
#include "js-parser.h"
#include "re-compiler.h"

JERRY_STATIC_ASSERT (sizeof (jerry_value_t) == sizeof (ecma_value_t),
                     size_of_jerry_value_t_must_be_equal_to_size_of_ecma_value_t);

JERRY_STATIC_ASSERT ((int) ECMA_PARSE_STRICT_MODE == (int) JERRY_PARSE_STRICT_MODE
                     && (int) ECMA_PARSE_MODULE == (int) JERRY_PARSE_MODULE
                     && (int) ECMA_PARSE_HAS_RESOURCE == (int) JERRY_PARSE_HAS_RESOURCE
                     && (int) ECMA_PARSE_HAS_START == (int) JERRY_PARSE_HAS_START,
                     ecma_parse_config_options_t_must_be_equal_to_jerry_parse_config_options_t);

JERRY_STATIC_ASSERT (sizeof (jerry_parse_options_t) == sizeof (ecma_parse_options_t),
                     ecma_parse_options_t_and_jerry_parse_options_t_must_be_the_same);

#if JERRY_BUILTIN_REGEXP
JERRY_STATIC_ASSERT ((int) RE_FLAG_GLOBAL == (int) JERRY_REGEXP_FLAG_GLOBAL
                     && (int) RE_FLAG_MULTILINE == (int) JERRY_REGEXP_FLAG_MULTILINE
                     && (int) RE_FLAG_IGNORE_CASE == (int) JERRY_REGEXP_FLAG_IGNORE_CASE
                     && (int) RE_FLAG_STICKY== (int) JERRY_REGEXP_FLAG_STICKY
                     && (int) RE_FLAG_UNICODE == (int) JERRY_REGEXP_FLAG_UNICODE
                     && (int) RE_FLAG_DOTALL == (int) JERRY_REGEXP_FLAG_DOTALL,
                     re_flags_t_must_be_equal_to_jerry_regexp_flags_t);
#endif /* JERRY_BUILTIN_REGEXP */

#if JERRY_BUILTIN_PROMISE
/* The internal ECMA_PROMISE_STATE_* values are "one byte away" from the API values */
JERRY_STATIC_ASSERT ((int) ECMA_PROMISE_IS_PENDING == (int) JERRY_PROMISE_STATE_PENDING
                     && (int) ECMA_PROMISE_IS_FULFILLED == (int) JERRY_PROMISE_STATE_FULFILLED,
                     promise_internal_state_matches_external);
#endif /* JERRY_BUILTIN_PROMISE */

/**
 * Offset between internal and external arithmetic operator types
 */
#define ECMA_NUMBER_ARITHMETIC_OP_API_OFFSET (JERRY_BIN_OP_SUB - NUMBER_ARITHMETIC_SUBTRACTION)

JERRY_STATIC_ASSERT (((NUMBER_ARITHMETIC_SUBTRACTION + ECMA_NUMBER_ARITHMETIC_OP_API_OFFSET) == JERRY_BIN_OP_SUB)
                     && ((NUMBER_ARITHMETIC_MULTIPLICATION + ECMA_NUMBER_ARITHMETIC_OP_API_OFFSET) == JERRY_BIN_OP_MUL)
                     && ((NUMBER_ARITHMETIC_DIVISION + ECMA_NUMBER_ARITHMETIC_OP_API_OFFSET) == JERRY_BIN_OP_DIV)
                     && ((NUMBER_ARITHMETIC_REMAINDER + ECMA_NUMBER_ARITHMETIC_OP_API_OFFSET) == JERRY_BIN_OP_REM),
                     number_arithmetics_operation_type_matches_external);

#if !JERRY_PARSER && !JERRY_SNAPSHOT_EXEC
#error "JERRY_SNAPSHOT_EXEC must be enabled if JERRY_PARSER is disabled!"
#endif /* !JERRY_PARSER && !JERRY_SNAPSHOT_EXEC */

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
  memset ((char *) &JERRY_CONTEXT_STRUCT + offsetof (jerry_context_t, JERRY_CONTEXT_FIRST_MEMBER), 0,
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

#if JERRY_DEBUGGER
  if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
  {
    jerry_debugger_send_type (JERRY_DEBUGGER_CLOSE_CONNECTION);

    jerry_debugger_transport_close ();
  }
#endif /* JERRY_DEBUGGER */

  for (jerry_context_data_header_t *this_p = JERRY_CONTEXT (context_data_p);
       this_p != NULL;
       this_p = this_p->next_p)
  {
    if (this_p->manager_p->deinit_cb)
    {
      void *data = (this_p->manager_p->bytes_needed > 0) ? JERRY_CONTEXT_DATA_HEADER_USER_DATA (this_p) : NULL;
      this_p->manager_p->deinit_cb (data);
    }
  }

#if JERRY_BUILTIN_PROMISE
  ecma_free_all_enqueued_jobs ();
#endif /* JERRY_BUILTIN_PROMISE */
  ecma_finalize ();
  jerry_make_api_unavailable ();

  for (jerry_context_data_header_t *this_p = JERRY_CONTEXT (context_data_p), *next_p = NULL;
       this_p != NULL;
       this_p = next_p)
  {
    next_p = this_p->next_p;
    if (this_p->manager_p->finalize_cb)
    {
      void *data = (this_p->manager_p->bytes_needed > 0) ? JERRY_CONTEXT_DATA_HEADER_USER_DATA (this_p) : NULL;
      this_p->manager_p->finalize_cb (data);
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
      return (manager_p->bytes_needed > 0) ? JERRY_CONTEXT_DATA_HEADER_USER_DATA (item_p) : NULL;
    }
  }

  item_p = jmem_heap_alloc_block (sizeof (jerry_context_data_header_t) + manager_p->bytes_needed);
  item_p->manager_p = manager_p;
  item_p->next_p = JERRY_CONTEXT (context_data_p);
  JERRY_CONTEXT (context_data_p) = item_p;

  if (manager_p->bytes_needed > 0)
  {
    ret = JERRY_CONTEXT_DATA_HEADER_USER_DATA (item_p);
    memset (ret, 0, manager_p->bytes_needed);
  }

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

  if (mode == JERRY_GC_PRESSURE_LOW)
  {
    /* Call GC directly, because 'ecma_free_unused_memory' might decide it's not yet worth it. */
    ecma_gc_run ();
    return;
  }

  ecma_free_unused_memory (JMEM_PRESSURE_HIGH);
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
#if JERRY_MEM_STATS
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
#else /* !JERRY_MEM_STATS */
  JERRY_UNUSED (out_stats_p);
  return false;
#endif /* JERRY_MEM_STATS */
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

  jerry_value_t parse_ret_val = jerry_parse (script_source_p, script_source_size, NULL);

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
jerry_parse (const jerry_char_t *source_p, /**< script source */
             size_t source_size, /**< script source size */
             const jerry_parse_options_t *options_p) /**< parsing options, can be NULL if not used */
{
#if JERRY_PARSER
  jerry_assert_api_available ();

  uint32_t allowed_parse_options = (JERRY_PARSE_STRICT_MODE
                                    | JERRY_PARSE_MODULE
                                    | JERRY_PARSE_HAS_RESOURCE
                                    | JERRY_PARSE_HAS_START);

  if (options_p != NULL && (options_p->options & ~allowed_parse_options) != 0)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }
#endif /* JERRY_PARSER */

#if JERRY_DEBUGGER && JERRY_PARSER
  if ((JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
      && options_p != NULL
      && (options_p->options & JERRY_PARSE_HAS_RESOURCE)
      && options_p->resource_name_length > 0)
  {
    jerry_debugger_send_string (JERRY_DEBUGGER_SOURCE_CODE_NAME,
                                JERRY_DEBUGGER_NO_SUBTYPE,
                                options_p->resource_name_p,
                                options_p->resource_name_length);
  }
#endif /* JERRY_DEBUGGER && JERRY_PARSER */

#if JERRY_PARSER
  uint32_t parse_opts = 0;

  if (options_p != NULL)
  {
    parse_opts |= options_p->options & (JERRY_PARSE_STRICT_MODE | JERRY_PARSE_MODULE);
  }

  if ((parse_opts & JERRY_PARSE_MODULE) != 0)
  {
#if JERRY_MODULE_SYSTEM
    JERRY_CONTEXT (module_current_p) = ecma_module_create ();
#else /* !JERRY_MODULE_SYSTEM */
    return jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG (ecma_error_module_not_supported_p)));
#endif /* JERRY_MODULE_SYSTEM */
  }

  ecma_compiled_code_t *bytecode_data_p = parser_parse_script (NULL,
                                                               0,
                                                               source_p,
                                                               source_size,
                                                               parse_opts,
                                                               (const ecma_parse_options_t *) options_p);

  if (JERRY_UNLIKELY (bytecode_data_p == NULL))
  {
#if JERRY_MODULE_SYSTEM
    if ((parse_opts & JERRY_PARSE_MODULE) != 0)
    {
      ecma_module_cleanup_context ();
    }
#endif /* JERRY_MODULE_SYSTEM */

    return ecma_create_error_reference_from_context ();
  }

#if JERRY_MODULE_SYSTEM
  if (JERRY_UNLIKELY (parse_opts & JERRY_PARSE_MODULE))
  {
    ecma_module_t *module_p = JERRY_CONTEXT (module_current_p);
    module_p->u.compiled_code_p = bytecode_data_p;

    JERRY_CONTEXT (module_current_p) = NULL;

    return ecma_make_object_value ((ecma_object_t *) module_p);
  }
#endif /* JERRY_MODULE_SYSTEM */

  ecma_object_t *object_p = ecma_create_object (NULL, sizeof (ecma_extended_object_t), ECMA_OBJECT_TYPE_CLASS);

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;
  ext_object_p->u.cls.type = ECMA_OBJECT_CLASS_SCRIPT;
  ECMA_SET_INTERNAL_VALUE_POINTER (ext_object_p->u.cls.u3.value, bytecode_data_p);

  return ecma_make_object_value (object_p);
#else /* !JERRY_PARSER */
  JERRY_UNUSED (source_p);
  JERRY_UNUSED (source_size);
  JERRY_UNUSED (options_p);

  return jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG (ecma_error_parser_not_supported_p)));
#endif /* JERRY_PARSER */
} /* jerry_parse */

/**
 * Parse function and construct an EcmaScript function. The lexical
 * environment is set to the global lexical environment.
 *
 * @return function object value - if script was parsed successfully,
 *         thrown error - otherwise
 */
jerry_value_t
jerry_parse_function (const jerry_char_t *arg_list_p, /**< script source */
                      size_t arg_list_size, /**< script source size */
                      const jerry_char_t *source_p, /**< script source */
                      size_t source_size, /**< script source size */
                      const jerry_parse_options_t *options_p) /**< parsing options, can be NULL if not used */
{
#if JERRY_PARSER
  jerry_assert_api_available ();

  uint32_t allowed_parse_options = (JERRY_PARSE_STRICT_MODE
                                    | JERRY_PARSE_HAS_RESOURCE
                                    | JERRY_PARSE_HAS_START);

  if (options_p != NULL && (options_p->options & ~allowed_parse_options) != 0)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }
#endif /* JERRY_PARSER */

#if JERRY_DEBUGGER && JERRY_PARSER
  if ((JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
      && options_p != NULL
      && (options_p->options & JERRY_PARSE_HAS_RESOURCE)
      && options_p->resource_name_length > 0)
  {
    jerry_debugger_send_string (JERRY_DEBUGGER_SOURCE_CODE_NAME,
                                JERRY_DEBUGGER_NO_SUBTYPE,
                                options_p->resource_name_p,
                                options_p->resource_name_length);
  }
#endif /* JERRY_DEBUGGER && JERRY_PARSER */

#if JERRY_PARSER
  uint32_t parse_opts = 0;

  if (options_p != NULL)
  {
    parse_opts |= options_p->options & JERRY_PARSE_STRICT_MODE;
  }

  if (arg_list_p == NULL)
  {
    /* Must not be a NULL value. */
    arg_list_p = (const jerry_char_t *) "";
  }

  ecma_compiled_code_t *bytecode_p = parser_parse_script (arg_list_p,
                                                          arg_list_size,
                                                          source_p,
                                                          source_size,
                                                          parse_opts,
                                                          (const ecma_parse_options_t *) options_p);

  if (JERRY_UNLIKELY (bytecode_p == NULL))
  {
    return ecma_create_error_reference_from_context ();
  }

  ecma_object_t *global_object_p = ecma_builtin_get_global ();

#if JERRY_BUILTIN_REALMS
  JERRY_ASSERT (global_object_p == (ecma_object_t *) ecma_op_function_get_realm (bytecode_p));
#endif /* JERRY_BUILTIN_REALMS */

  ecma_object_t *lex_env_p = ecma_get_global_environment (global_object_p);
  ecma_object_t *func_obj_p = ecma_op_create_simple_function_object (lex_env_p, bytecode_p);
  ecma_bytecode_deref (bytecode_p);

  return ecma_make_object_value (func_obj_p);
#else /* !JERRY_PARSER */
  JERRY_UNUSED (arg_list_p);
  JERRY_UNUSED (arg_list_size);
  JERRY_UNUSED (source_p);
  JERRY_UNUSED (source_size);
  JERRY_UNUSED (options_p);

  return jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG (ecma_error_parser_not_supported_p)));
#endif /* JERRY_PARSER */
} /* jerry_parse_function */

/**
 * Run a Script or Module created by jerry_parse.
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
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_object_t *object_p = ecma_get_object_from_value (func_val);

  if (ecma_get_object_type (object_p) != ECMA_OBJECT_TYPE_CLASS)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

  if (ext_object_p->u.cls.type != ECMA_OBJECT_CLASS_SCRIPT)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  const ecma_compiled_code_t *bytecode_data_p;
  bytecode_data_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_compiled_code_t, ext_object_p->u.cls.u3.value);

  JERRY_ASSERT (CBC_FUNCTION_GET_TYPE (bytecode_data_p->status_flags) == CBC_FUNCTION_SCRIPT);

  return jerry_return (vm_run_global (bytecode_data_p));
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

  uint32_t allowed_parse_options = JERRY_PARSE_STRICT_MODE;

  if ((parse_opts & ~allowed_parse_options) != 0)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  return jerry_return (ecma_op_eval_chars_buffer ((const lit_utf8_byte_t *) source_p,
                                                  source_size,
                                                  parse_opts));
} /* jerry_eval */

/**
 * Link modules to their dependencies. The dependencies are resolved by a user callback.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return true - if linking is successful, error - otherwise
 */
jerry_value_t
jerry_module_link (const jerry_value_t module_val, /**< root module */
                   jerry_module_resolve_callback_t callback, /**< resolve module callback, uses
                                                              *   jerry_port_module_resolve when NULL is passed */
                   void *user_p) /**< pointer passed to the resolve callback */
{
  jerry_assert_api_available ();

#if JERRY_MODULE_SYSTEM
  if (callback == NULL)
  {
    callback = jerry_port_module_resolve;
  }

  ecma_module_t *module_p = ecma_module_get_resolved_module (module_val);

  if (module_p == NULL)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_not_module_p)));
  }

  return jerry_return (ecma_module_link (module_p, callback, user_p));
#else /* !JERRY_MODULE_SYSTEM */
  JERRY_UNUSED (module_val);
  JERRY_UNUSED (callback);
  JERRY_UNUSED (user_p);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_module_not_supported_p)));
#endif /* JERRY_MODULE_SYSTEM */
} /* jerry_module_link */

/**
 * Evaluate a module and its dependencies. The module must be in linked state.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return result of module bytecode execution - if evaluation was successful
 *         error - otherwise
 */
jerry_value_t
jerry_module_evaluate (const jerry_value_t module_val) /**< root module */
{
  jerry_assert_api_available ();

#if JERRY_MODULE_SYSTEM
  ecma_module_t *module_p = ecma_module_get_resolved_module (module_val);

  if (module_p == NULL)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_not_module_p)));
  }

  if (module_p->header.u.cls.u1.module_state != JERRY_MODULE_STATE_LINKED)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Module must be in linked state")));
  }

  return jerry_return (ecma_module_evaluate (module_p));
#else /* !JERRY_MODULE_SYSTEM */
  JERRY_UNUSED (module_val);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_module_not_supported_p)));
#endif /* JERRY_MODULE_SYSTEM */
} /* jerry_module_evaluate */

/**
 * Returns the current status of a module
 *
 * @return current status - if module_val is a module,
 *         JERRY_MODULE_STATE_INVALID - otherwise
 */
jerry_module_state_t
jerry_module_get_state (const jerry_value_t module_val) /**< module object */
{
  jerry_assert_api_available ();

#if JERRY_MODULE_SYSTEM
  ecma_module_t *module_p = ecma_module_get_resolved_module (module_val);

  if (module_p == NULL)
  {
    return JERRY_MODULE_STATE_INVALID;
  }

  return (jerry_module_state_t) module_p->header.u.cls.u1.module_state;
#else /* !JERRY_MODULE_SYSTEM */
  JERRY_UNUSED (module_val);

  return JERRY_MODULE_STATE_INVALID;
#endif /* JERRY_MODULE_SYSTEM */
} /* jerry_module_get_state */

/**
 * Sets a callback which is called after a module state is changed to linked, evaluated, or error.
 */
void
jerry_module_set_state_changed_callback (jerry_module_state_changed_callback_t callback, /**< callback */
                                         void *user_p) /**< pointer passed to the callback */
{
  jerry_assert_api_available ();

#if JERRY_MODULE_SYSTEM
  JERRY_CONTEXT (module_state_changed_callback_p) = callback;
  JERRY_CONTEXT (module_state_changed_callback_user_p) = user_p;
#else /* !JERRY_MODULE_SYSTEM */
  JERRY_UNUSED (callback);
  JERRY_UNUSED (user_p);
#endif /* JERRY_MODULE_SYSTEM */
} /* jerry_module_set_state_changed_callback */

/**
 * Returns the number of import/export requests of a module
 *
 * @return number of import/export requests of a module
 */
size_t
jerry_module_get_number_of_requests (const jerry_value_t module_val) /**< module */
{
  jerry_assert_api_available ();

#if JERRY_MODULE_SYSTEM
  ecma_module_t *module_p = ecma_module_get_resolved_module (module_val);

  if (module_p == NULL)
  {
    return 0;
  }

  size_t number_of_requests = 0;

  ecma_module_node_t *node_p = module_p->imports_p;

  while (node_p != NULL)
  {
    number_of_requests++;
    node_p = node_p->next_p;
  }

  return number_of_requests;
#else /* !JERRY_MODULE_SYSTEM */
  JERRY_UNUSED (module_val);

  return 0;
#endif /* JERRY_MODULE_SYSTEM */
} /* jerry_module_get_number_of_requests */

/**
 * Returns the module request specified by the request_index argument
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return string - if the request has not been resolved yet,
 *         module object - if the request has been resolved successfully,
 *         error - otherwise
 */
jerry_value_t
jerry_module_get_request (const jerry_value_t module_val, /**< module */
                          size_t request_index) /**< request index */
{
  jerry_assert_api_available ();

#if JERRY_MODULE_SYSTEM
  ecma_module_t *module_p = ecma_module_get_resolved_module (module_val);

  if (module_p == NULL)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_not_module_p)));
  }

  ecma_module_node_t *node_p = module_p->imports_p;

  while (node_p != NULL)
  {
    if (request_index == 0)
    {
      return ecma_copy_value (node_p->u.path_or_module);
    }

    --request_index;
    node_p = node_p->next_p;
  }

  return jerry_throw (ecma_raise_range_error (ECMA_ERR_MSG ("Request is not available")));
#else /* !JERRY_MODULE_SYSTEM */
  JERRY_UNUSED (module_val);
  JERRY_UNUSED (request_index);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_module_not_supported_p)));
#endif /* JERRY_MODULE_SYSTEM */
} /* jerry_module_get_request */

/**
 * Returns the namespace object of a module
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return object - if namespace object is available,
 *         error - otherwise
 */
jerry_value_t
jerry_module_get_namespace (const jerry_value_t module_val) /**< module */
{
  jerry_assert_api_available ();

#if JERRY_MODULE_SYSTEM
  ecma_module_t *module_p = ecma_module_get_resolved_module (module_val);

  if (module_p == NULL)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_not_module_p)));
  }

  if (module_p->header.u.cls.u1.module_state < JERRY_MODULE_STATE_LINKED
      || module_p->header.u.cls.u1.module_state > JERRY_MODULE_STATE_EVALUATED)
  {
    return jerry_throw (ecma_raise_range_error (ECMA_ERR_MSG ("Namespace object is not available")));
  }

  JERRY_ASSERT (module_p->namespace_object_p != NULL);

  ecma_ref_object (module_p->namespace_object_p);
  return ecma_make_object_value (module_p->namespace_object_p);
#else /* !JERRY_MODULE_SYSTEM */
  JERRY_UNUSED (module_val);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_module_not_supported_p)));
#endif /* JERRY_MODULE_SYSTEM */
} /* jerry_module_get_namespace */

/**
 * Creates a native module with a list of exports. The initial state of the module is linked.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return native module - if the module is successfully created,
 *         error - otherwise
 */
jerry_value_t
jerry_native_module_create (jerry_native_module_evaluate_callback_t callback, /**< evaluation callback for
                                                                               *   native modules */
                            const jerry_value_t * const exports_p, /**< list of the exported bindings of the module,
                                                                    *   must be valid string identifiers */
                            size_t number_of_exports) /**< number of exports in the exports_p list */
{
  jerry_assert_api_available ();

#if JERRY_MODULE_SYSTEM
  ecma_object_t *global_object_p = ecma_builtin_get_global ();
  ecma_object_t *scope_p = ecma_create_decl_lex_env (ecma_get_global_environment (global_object_p));
  ecma_module_names_t *local_exports_p = NULL;

  for (size_t i = 0; i < number_of_exports; i++)
  {
    if (!ecma_is_value_string (exports_p[i]))
    {
      ecma_deref_object (scope_p);
      ecma_module_release_module_names (local_exports_p);
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Module exports must be string values")));
    }

    ecma_string_t *name_str_p = ecma_get_string_from_value (exports_p[i]);

    bool valid_identifier = false;

    ECMA_STRING_TO_UTF8_STRING (name_str_p, name_start_p, name_size);

    if (name_size > 0)
    {
      const lit_utf8_byte_t *name_p = name_start_p;
      const lit_utf8_byte_t *name_end_p = name_start_p + name_size;
      lit_code_point_t code_point;

      lit_utf8_size_t size = lit_read_code_point_from_cesu8 (name_p, name_end_p, &code_point);

      if (lit_code_point_is_identifier_start (code_point))
      {
        name_p += size;

        valid_identifier = true;

        while (name_p < name_end_p)
        {
          size = lit_read_code_point_from_cesu8 (name_p, name_end_p, &code_point);

          if (!lit_code_point_is_identifier_part (code_point))
          {
            valid_identifier = false;
            break;
          }

          name_p += size;
        }
      }
    }

    ECMA_FINALIZE_UTF8_STRING (name_start_p, name_size);

    if (!valid_identifier)
    {
      ecma_deref_object (scope_p);
      ecma_module_release_module_names (local_exports_p);
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Module exports must be valid identifiers")));
    }

    if (ecma_find_named_property (scope_p, name_str_p) != NULL)
    {
      continue;
    }

    ecma_create_named_data_property (scope_p, name_str_p, ECMA_PROPERTY_FLAG_WRITABLE, NULL);

    ecma_module_names_t *new_export_p;
    new_export_p = (ecma_module_names_t *) jmem_heap_alloc_block (sizeof (ecma_module_names_t));

    new_export_p->next_p = local_exports_p;
    local_exports_p = new_export_p;

    ecma_ref_ecma_string (name_str_p);
    new_export_p->imex_name_p = name_str_p;

    ecma_ref_ecma_string (name_str_p);
    new_export_p->local_name_p = name_str_p;
  }

  ecma_module_t *module_p = ecma_module_create ();

  module_p->header.u.cls.u2.module_flags |= ECMA_MODULE_IS_NATIVE;
  module_p->scope_p = scope_p;
  module_p->local_exports_p = local_exports_p;
  module_p->u.callback = callback;

  ecma_deref_object (scope_p);

  return ecma_make_object_value (&module_p->header.object);

#else /* !JERRY_MODULE_SYSTEM */
  JERRY_UNUSED (callback);
  JERRY_UNUSED (exports_p);
  JERRY_UNUSED (number_of_exports);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_module_not_supported_p)));
#endif /* JERRY_MODULE_SYSTEM */
} /* jerry_native_module_create */

/**
 * Gets the value of an export which belongs to a native module.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the export - if success
 *         error - otherwise
 */
jerry_value_t
jerry_native_module_get_export (const jerry_value_t native_module_val, /**< a native module object */
                                const jerry_value_t export_name_val) /**< string identifier of the export */
{
  jerry_assert_api_available ();

#if JERRY_MODULE_SYSTEM
  ecma_module_t *module_p = ecma_module_get_resolved_module (native_module_val);

  if (module_p == NULL)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_not_module_p)));
  }

  if (!(module_p->header.u.cls.u2.module_flags & ECMA_MODULE_IS_NATIVE)
      || !ecma_is_value_string (export_name_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_property_t *property_p = ecma_find_named_property (module_p->scope_p,
                                                          ecma_get_string_from_value (export_name_val));

  if (property_p == NULL)
  {
    return jerry_throw (ecma_raise_reference_error (ECMA_ERR_MSG (ecma_error_unknown_export_p)));
  }

  return ecma_copy_value (ECMA_PROPERTY_VALUE_PTR (property_p)->value);
#else /* !JERRY_MODULE_SYSTEM */
  JERRY_UNUSED (native_module_val);
  JERRY_UNUSED (export_name_val);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_module_not_supported_p)));
#endif /* JERRY_MODULE_SYSTEM */
} /* jerry_native_module_get_export */

/**
 * Sets the value of an export which belongs to a native module.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return true value - if the operation was successful
 *         error - otherwise
 */
jerry_value_t
jerry_native_module_set_export (const jerry_value_t native_module_val, /**< a native module object */
                                const jerry_value_t export_name_val, /**< string identifier of the export */
                                const jerry_value_t value_to_set) /**< new value of the export */
{
  jerry_assert_api_available ();

#if JERRY_MODULE_SYSTEM
  ecma_module_t *module_p = ecma_module_get_resolved_module (native_module_val);

  if (module_p == NULL)
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_not_module_p)));
  }

  if (!(module_p->header.u.cls.u2.module_flags & ECMA_MODULE_IS_NATIVE)
      || !ecma_is_value_string (export_name_val)
      || ecma_is_value_error_reference (value_to_set))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_property_t *property_p = ecma_find_named_property (module_p->scope_p,
                                                          ecma_get_string_from_value (export_name_val));

  if (property_p == NULL)
  {
    return jerry_throw (ecma_raise_reference_error (ECMA_ERR_MSG (ecma_error_unknown_export_p)));
  }

  ecma_named_data_property_assign_value (module_p->scope_p,
                                         ECMA_PROPERTY_VALUE_PTR (property_p),
                                         value_to_set);
  return ECMA_VALUE_TRUE;
#else /* !JERRY_MODULE_SYSTEM */
  JERRY_UNUSED (native_module_val);
  JERRY_UNUSED (export_name_val);
  JERRY_UNUSED (value_to_set);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_module_not_supported_p)));
#endif /* JERRY_MODULE_SYSTEM */
} /* jerry_native_module_set_export */

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

#if JERRY_BUILTIN_PROMISE
  return ecma_process_all_enqueued_jobs ();
#else /* !JERRY_BUILTIN_PROMISE */
  return ECMA_VALUE_UNDEFINED;
#endif /* JERRY_BUILTIN_PROMISE */
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

  ecma_extended_primitive_t *error_ref_p = ecma_get_extended_primitive_from_value (value);

  return ECMA_EXTENDED_PRIMITIVE_GET_TYPE (error_ref_p) == ECMA_EXTENDED_PRIMITIVE_ABORT;
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
 * Check if the specified value is true.
 *
 * @return true  - if the specified value is true
 *         false - otherwise
 */
bool
jerry_value_is_true (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  return ecma_is_value_true (value);
} /* jerry_value_is_true */

/**
 * Check if the specified value is false.
 *
 * @return true  - if the specified value is false
 *         false - otherwise
 */
bool
jerry_value_is_false (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  return ecma_is_value_false (value);
} /* jerry_value_is_false */

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
 * Check if the specified value is an async function object value.
 *
 * @return true - if the specified value is an async function,
 *         false - otherwise
 */
bool
jerry_value_is_async_function (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

#if JERRY_ESNEXT
  if (ecma_is_value_object (value))
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (value);

    if (ecma_get_object_type (obj_p) == ECMA_OBJECT_TYPE_FUNCTION
        && !ecma_get_object_is_builtin (obj_p))
    {
      const ecma_compiled_code_t *bytecode_data_p;
      bytecode_data_p = ecma_op_function_get_compiled_code ((ecma_extended_object_t *) obj_p);
      uint16_t type = CBC_FUNCTION_GET_TYPE (bytecode_data_p->status_flags);

      return (type == CBC_FUNCTION_ASYNC
              || type == CBC_FUNCTION_ASYNC_ARROW
              || type == CBC_FUNCTION_ASYNC_GENERATOR);
    }
  }
#else /* !JERRY_ESNEXT */
  JERRY_UNUSED (value);
#endif /* JERRY_ESNEXT */

  return false;
} /* jerry_value_is_async_function */

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
#if JERRY_BUILTIN_PROMISE
  return (ecma_is_value_object (value)
          && ecma_is_promise (ecma_get_object_from_value (value)));
#else /* !JERRY_BUILTIN_PROMISE */
  JERRY_UNUSED (value);
  return false;
#endif /* JERRY_BUILTIN_PROMISE */
} /* jerry_value_is_promise */

/**
 * Check if the specified value is a proxy object.
 *
 * @return true  - if the specified value is a proxy object,
 *         false - otherwise
 */
bool
jerry_value_is_proxy (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();
#if JERRY_BUILTIN_PROXY
  return (ecma_is_value_object (value)
          && ECMA_OBJECT_IS_PROXY (ecma_get_object_from_value (value)));
#else /* !JERRY_BUILTIN_PROXY */
  JERRY_UNUSED (value);
  return false;
#endif /* JERRY_BUILTIN_PROXY */
} /* jerry_value_is_proxy */

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
 * Check if the specified value is symbol.
 *
 * @return true  - if the specified value is symbol,
 *         false - otherwise
 */
bool
jerry_value_is_symbol (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

#if JERRY_ESNEXT
  return ecma_is_value_symbol (value);
#else /* !JERRY_ESNEXT */
  JERRY_UNUSED (value);
  return false;
#endif /* JERRY_ESNEXT */
} /* jerry_value_is_symbol */

/**
 * Check if the specified value is BigInt.
 *
 * @return true  - if the specified value is BigInt,
 *         false - otherwise
 */
bool
jerry_value_is_bigint (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_BIGINT
  return ecma_is_value_bigint (value);
#else /* !JERRY_BUILTIN_BIGINT */
  JERRY_UNUSED (value);
  return false;
#endif /* JERRY_BUILTIN_BIGINT */
} /* jerry_value_is_bigint */

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
#if JERRY_ESNEXT
    case LIT_MAGIC_STRING_SYMBOL:
    {
      return JERRY_TYPE_SYMBOL;
    }
#endif /* JERRY_ESNEXT */
    case LIT_MAGIC_STRING_FUNCTION:
    {
      return JERRY_TYPE_FUNCTION;
    }
#if JERRY_BUILTIN_BIGINT
    case LIT_MAGIC_STRING_BIGINT:
    {
      return JERRY_TYPE_BIGINT;
    }
#endif /* JERRY_BUILTIN_BIGINT */
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
 * Used by jerry_object_get_type to get the type of class objects
 */
static const uint8_t jerry_class_object_type[] =
{
  /* These objects require custom property resolving. */
  JERRY_OBJECT_TYPE_STRING, /**< type of ECMA_OBJECT_CLASS_STRING */
  JERRY_OBJECT_TYPE_ARGUMENTS, /**< type of ECMA_OBJECT_CLASS_ARGUMENTS */
#if JERRY_BUILTIN_TYPEDARRAY
  JERRY_OBJECT_TYPE_TYPEDARRAY, /**< type of ECMA_OBJECT_CLASS_TYPEDARRAY */
#endif /* JERRY_BUILTIN_TYPEDARRAY */
#if JERRY_MODULE_SYSTEM
  JERRY_OBJECT_TYPE_MODULE_NAMESPACE, /**< type of ECMA_OBJECT_CLASS_MODULE_NAMESPACE */
#endif

  /* These objects are marked by Garbage Collector. */
#if JERRY_ESNEXT
  JERRY_OBJECT_TYPE_GENERATOR, /**< type of ECMA_OBJECT_CLASS_GENERATOR */
  JERRY_OBJECT_TYPE_GENERATOR, /**< type of ECMA_OBJECT_CLASS_ASYNC_GENERATOR */
  JERRY_OBJECT_TYPE_ITERATOR, /**< type of ECMA_OBJECT_CLASS_ARRAY_ITERATOR */
  JERRY_OBJECT_TYPE_ITERATOR, /**< type of ECMA_OBJECT_CLASS_SET_ITERATOR */
  JERRY_OBJECT_TYPE_ITERATOR, /**< type of ECMA_OBJECT_CLASS_MAP_ITERATOR */
#if JERRY_BUILTIN_REGEXP
  JERRY_OBJECT_TYPE_ITERATOR, /**< type of ECMA_OBJECT_CLASS_REGEXP_STRING_ITERATOR */
#endif /* JERRY_BUILTIN_REGEXP */
#endif /* JERRY_ESNEXT */
#if JERRY_MODULE_SYSTEM
  JERRY_OBJECT_TYPE_MODULE, /**< type of ECMA_OBJECT_CLASS_MODULE */
#endif
#if JERRY_BUILTIN_PROMISE
  JERRY_OBJECT_TYPE_PROMISE, /**< type of ECMA_OBJECT_CLASS_PROMISE */
  JERRY_OBJECT_TYPE_GENERIC, /**< type of ECMA_OBJECT_CLASS_PROMISE_CAPABILITY */
#endif /* JERRY_BUILTIN_PROMISE */
#if JERRY_BUILTIN_DATAVIEW
  JERRY_OBJECT_TYPE_DATAVIEW, /**< type of ECMA_OBJECT_CLASS_DATAVIEW */
#endif /* JERRY_BUILTIN_DATAVIEW */
#if JERRY_BUILTIN_CONTAINER
  JERRY_OBJECT_TYPE_CONTAINER, /**< type of ECMA_OBJECT_CLASS_CONTAINER */
#endif /* JERRY_BUILTIN_CONTAINER */

  /* Normal objects. */
  JERRY_OBJECT_TYPE_BOOLEAN, /**< type of ECMA_OBJECT_CLASS_BOOLEAN */
  JERRY_OBJECT_TYPE_NUMBER, /**< type of ECMA_OBJECT_CLASS_NUMBER */
  JERRY_OBJECT_TYPE_ERROR, /**< type of ECMA_OBJECT_CLASS_ERROR */
  JERRY_OBJECT_TYPE_GENERIC, /**< type of ECMA_OBJECT_CLASS_INTERNAL_OBJECT */
#if JERRY_PARSER
  JERRY_OBJECT_TYPE_SCRIPT, /**< type of ECMA_OBJECT_CLASS_SCRIPT */
#endif /* JERRY_PARSER */
#if JERRY_BUILTIN_DATE
  JERRY_OBJECT_TYPE_DATE, /**< type of ECMA_OBJECT_CLASS_DATE */
#endif /* JERRY_BUILTIN_DATE */
#if JERRY_BUILTIN_REGEXP
  JERRY_OBJECT_TYPE_REGEXP, /**< type of ECMA_OBJECT_CLASS_REGEXP */
#endif /* JERRY_BUILTIN_REGEXP */
#if JERRY_ESNEXT
  JERRY_OBJECT_TYPE_SYMBOL, /**< type of ECMA_OBJECT_CLASS_SYMBOL */
  JERRY_OBJECT_TYPE_ITERATOR, /**< type of ECMA_OBJECT_CLASS_STRING_ITERATOR */
#endif /* JERRY_ESNEXT */
#if JERRY_BUILTIN_TYPEDARRAY
  JERRY_OBJECT_TYPE_ARRAYBUFFER, /**< type of ECMA_OBJECT_CLASS_ARRAY_BUFFER */
#endif /* JERRY_BUILTIN_TYPEDARRAY */
#if JERRY_BUILTIN_BIGINT
  JERRY_OBJECT_TYPE_BIGINT, /**< type of ECMA_OBJECT_CLASS_BIGINT */
#endif /* JERRY_BUILTIN_BIGINT */
#if JERRY_BUILTIN_WEAKREF
  JERRY_OBJECT_TYPE_WEAKREF, /**< type of ECMA_OBJECT_CLASS_WEAKREF */
#endif /* JERRY_BUILTIN_WEAKREF */
};

JERRY_STATIC_ASSERT (sizeof (jerry_class_object_type) == ECMA_OBJECT_CLASS__MAX,
                     jerry_class_object_type_must_have_object_class_max_elements);

/**
 * Get the object type of the given value
 *
 * @return JERRY_OBJECT_TYPE_NONE - if the given value is not an object
 *         jerry_object_type_t value - otherwise
 */
jerry_object_type_t
jerry_object_get_type (const jerry_value_t value) /**< input value to check */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (value))
  {
    return JERRY_OBJECT_TYPE_NONE;
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (value);
  ecma_extended_object_t *ext_obj_p = (ecma_extended_object_t *) obj_p;

  switch (ecma_get_object_type (obj_p))
  {
    case ECMA_OBJECT_TYPE_FUNCTION:
    case ECMA_OBJECT_TYPE_BOUND_FUNCTION:
    case ECMA_OBJECT_TYPE_NATIVE_FUNCTION:
    {
      return JERRY_OBJECT_TYPE_FUNCTION;
    }
    case ECMA_OBJECT_TYPE_ARRAY:
    {
      return JERRY_OBJECT_TYPE_ARRAY;
    }
#if JERRY_ESNEXT
    case ECMA_OBJECT_TYPE_PROXY:
    {
      return JERRY_OBJECT_TYPE_PROXY;
    }
#endif /* JERRY_ESNEXT */
    case ECMA_OBJECT_TYPE_CLASS:
    {
      JERRY_ASSERT (ext_obj_p->u.cls.type < ECMA_OBJECT_CLASS__MAX);
      return jerry_class_object_type[ext_obj_p->u.cls.type];
    }
    default:
    {
      break;
    }
  }

  return JERRY_OBJECT_TYPE_GENERIC;
} /* jerry_object_get_type */

/**
 * Get the function type of the given value
 *
 * @return JERRY_FUNCTION_TYPE_NONE - if the given value is not a function object
 *         jerry_function_type_t value - otherwise
 */
jerry_function_type_t
jerry_function_get_type (const jerry_value_t value) /**< input value to check */
{
  jerry_assert_api_available ();

  if (ecma_is_value_object (value))
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (value);
    ecma_extended_object_t *ext_obj_p = (ecma_extended_object_t *) obj_p;

    switch (ecma_get_object_type (obj_p))
    {
      case ECMA_OBJECT_TYPE_BOUND_FUNCTION:
      {
        return JERRY_FUNCTION_TYPE_BOUND;
      }
      case ECMA_OBJECT_TYPE_NATIVE_FUNCTION:
      {
        return JERRY_FUNCTION_TYPE_GENERIC;
      }
      case ECMA_OBJECT_TYPE_FUNCTION:
      {
        if (!ecma_get_object_is_builtin (obj_p))
        {
          const ecma_compiled_code_t *bytecode_data_p = ecma_op_function_get_compiled_code (ext_obj_p);

          switch (CBC_FUNCTION_GET_TYPE (bytecode_data_p->status_flags))
          {
#if JERRY_ESNEXT
            case CBC_FUNCTION_ARROW:
            case CBC_FUNCTION_ASYNC_ARROW:
            {
              return JERRY_FUNCTION_TYPE_ARROW;
            }
            case CBC_FUNCTION_GENERATOR:
            case CBC_FUNCTION_ASYNC_GENERATOR:
            {
              return JERRY_FUNCTION_TYPE_GENERATOR;
            }
#endif /* JERRY_ESNEXT */
            case CBC_FUNCTION_ACCESSOR:
            {
              return JERRY_FUNCTION_TYPE_ACCESSOR;
            }
            default:
            {
              break;
            }
          }
        }
        return JERRY_FUNCTION_TYPE_GENERIC;
      }
      default:
      {
        break;
      }
    }
  }

  return JERRY_FUNCTION_TYPE_NONE;
} /* jerry_function_get_type */

/**
 * Get the itearator type of the given value
 *
 * @return JERRY_ITERATOR_TYPE_NONE - if the given value is not an iterator object
 *         jerry_iterator_type_t value - otherwise
 */
jerry_iterator_type_t
jerry_iterator_get_type (const jerry_value_t value) /**< input value to check */
{
  jerry_assert_api_available ();

#if JERRY_ESNEXT
  if (ecma_is_value_object (value))
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (value);
    ecma_extended_object_t *ext_obj_p = (ecma_extended_object_t *) obj_p;

    if (ecma_get_object_type (obj_p) == ECMA_OBJECT_TYPE_CLASS)
    {
      switch (ext_obj_p->u.cls.type)
      {
        case ECMA_OBJECT_CLASS_ARRAY_ITERATOR:
        {
          return JERRY_ITERATOR_TYPE_ARRAY;
        }
#if JERRY_BUILTIN_CONTAINER
        case ECMA_OBJECT_CLASS_SET_ITERATOR:
        {
          return JERRY_ITERATOR_TYPE_SET;
        }
        case ECMA_OBJECT_CLASS_MAP_ITERATOR:
        {
          return JERRY_ITERATOR_TYPE_MAP;
        }
#endif /* JERRY_BUILTIN_CONTAINER */
        case ECMA_OBJECT_CLASS_STRING_ITERATOR:
        {
          return JERRY_ITERATOR_TYPE_STRING;
        }
        default:
        {
          break;
        }
      }
    }
  }
#else /* !JERRY_ESNEXT */
  JERRY_UNUSED (value);
#endif /* JERRY_ESNEXT */

  return JERRY_ITERATOR_TYPE_NONE;
} /* jerry_iterator_get_type */

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
#if JERRY_CPOINTER_32_BIT
          || feature == JERRY_FEATURE_CPOINTER_32_BIT
#endif /* JERRY_CPOINTER_32_BIT */
#if JERRY_ERROR_MESSAGES
          || feature == JERRY_FEATURE_ERROR_MESSAGES
#endif /* JERRY_ERROR_MESSAGES */
#if JERRY_PARSER
          || feature == JERRY_FEATURE_JS_PARSER
#endif /* JERRY_PARSER */
#if JERRY_MEM_STATS
          || feature == JERRY_FEATURE_MEM_STATS
#endif /* JERRY_MEM_STATS */
#if JERRY_PARSER_DUMP_BYTE_CODE
          || feature == JERRY_FEATURE_PARSER_DUMP
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */
#if JERRY_REGEXP_DUMP_BYTE_CODE
          || feature == JERRY_FEATURE_REGEXP_DUMP
#endif /* JERRY_REGEXP_DUMP_BYTE_CODE */
#if JERRY_SNAPSHOT_SAVE
          || feature == JERRY_FEATURE_SNAPSHOT_SAVE
#endif /* JERRY_SNAPSHOT_SAVE */
#if JERRY_SNAPSHOT_EXEC
          || feature == JERRY_FEATURE_SNAPSHOT_EXEC
#endif /* JERRY_SNAPSHOT_EXEC */
#if JERRY_DEBUGGER
          || feature == JERRY_FEATURE_DEBUGGER
#endif /* JERRY_DEBUGGER */
#if JERRY_VM_EXEC_STOP
          || feature == JERRY_FEATURE_VM_EXEC_STOP
#endif /* JERRY_VM_EXEC_STOP */
#if JERRY_BUILTIN_JSON
          || feature == JERRY_FEATURE_JSON
#endif /* JERRY_BUILTIN_JSON */
#if JERRY_BUILTIN_PROMISE
          || feature == JERRY_FEATURE_PROMISE
#endif /* JERRY_BUILTIN_PROMISE */
#if JERRY_ESNEXT
          || feature == JERRY_FEATURE_SYMBOL
#endif /* JERRY_ESNEXT */
#if JERRY_BUILTIN_TYPEDARRAY
          || feature == JERRY_FEATURE_TYPEDARRAY
#endif /* JERRY_BUILTIN_TYPEDARRAY */
#if JERRY_BUILTIN_DATAVIEW
          || feature == JERRY_FEATURE_DATAVIEW
#endif /* JERRY_BUILTIN_DATAVIEW */
#if JERRY_BUILTIN_PROXY
          || feature == JERRY_FEATURE_PROXY
#endif /* JERRY_BUILTIN_PROXY */
#if JERRY_BUILTIN_DATE
          || feature == JERRY_FEATURE_DATE
#endif /* JERRY_BUILTIN_DATE */
#if JERRY_BUILTIN_REGEXP
          || feature == JERRY_FEATURE_REGEXP
#endif /* JERRY_BUILTIN_REGEXP */
#if JERRY_LINE_INFO
          || feature == JERRY_FEATURE_LINE_INFO
#endif /* JERRY_LINE_INFO */
#if JERRY_LOGGING
          || feature == JERRY_FEATURE_LOGGING
#endif /* JERRY_LOGGING */
#if JERRY_BUILTIN_GLOBAL_THIS
          || feature == JERRY_FEATURE_GLOBAL_THIS
#endif /* JERRY_BUILTIN_GLOBAL_THIS */
#if JERRY_BUILTIN_CONTAINER
          || feature == JERRY_FEATURE_MAP
          || feature == JERRY_FEATURE_SET
          || feature == JERRY_FEATURE_WEAKMAP
          || feature == JERRY_FEATURE_WEAKSET
#endif /* JERRY_BUILTIN_CONTAINER */
#if JERRY_BUILTIN_WEAKREF
          || feature == JERRY_FEATURE_WEAKREF
#endif /* JERRY_BUILTIN_WEAKREF */
#if JERRY_BUILTIN_BIGINT
          || feature == JERRY_FEATURE_BIGINT
#endif /* JERRY_BUILTIN_BIGINT */
#if JERRY_BUILTIN_REALMS
          || feature == JERRY_FEATURE_REALM
#endif /* JERRY_BUILTIN_REALMS */
#if JERRY_PROMISE_CALLBACK
          || feature == JERRY_FEATURE_PROMISE_CALLBACK
#endif /* JERRY_PROMISE_CALLBACK */
#if JERRY_MODULE_SYSTEM
          || feature == JERRY_FEATURE_MODULE
#endif /* JERRY_MODULE_SYSTEM */
          );
} /* jerry_is_feature_enabled */

/**
 * Perform binary operation on the given operands (==, ===, <, >, etc.).
 *
 * @return error - if argument has an error flag or operation is unsuccessful or unsupported
 *         true/false - the result of the binary operation on the given operands otherwise
 */
jerry_value_t
jerry_binary_operation (jerry_binary_operation_t op, /**< operation */
                        const jerry_value_t lhs, /**< first operand */
                        const jerry_value_t rhs) /**< second operand */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (lhs) || ecma_is_value_error_reference (rhs))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
  }

  switch (op)
  {
    case JERRY_BIN_OP_EQUAL:
    {
      return jerry_return (ecma_op_abstract_equality_compare (lhs, rhs));
    }
    case JERRY_BIN_OP_STRICT_EQUAL:
    {
      return ecma_make_boolean_value (ecma_op_strict_equality_compare (lhs, rhs));
    }
    case JERRY_BIN_OP_LESS:
    {
      return jerry_return (opfunc_relation (lhs, rhs, true, false));
    }
    case JERRY_BIN_OP_LESS_EQUAL:
    {
      return jerry_return (opfunc_relation (lhs, rhs, false, true));
    }
    case JERRY_BIN_OP_GREATER:
    {
      return jerry_return (opfunc_relation (lhs, rhs, false, false));
    }
    case JERRY_BIN_OP_GREATER_EQUAL:
    {
      return jerry_return (opfunc_relation (lhs, rhs, true, true));
    }
    case JERRY_BIN_OP_INSTANCEOF:
    {
      if (!ecma_is_value_object (lhs)
          || !ecma_op_is_callable (rhs))
      {
        return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
      }

      ecma_object_t *proto_obj_p = ecma_get_object_from_value (rhs);
      return jerry_return (ecma_op_object_has_instance (proto_obj_p, lhs));
    }
    case JERRY_BIN_OP_ADD:
    {
      return jerry_return (opfunc_addition (lhs, rhs));
    }
    case JERRY_BIN_OP_SUB:
    case JERRY_BIN_OP_MUL:
    case JERRY_BIN_OP_DIV:
    case JERRY_BIN_OP_REM:
    {
      return jerry_return (do_number_arithmetic (op - ECMA_NUMBER_ARITHMETIC_OP_API_OFFSET, lhs, rhs));
    }
    default:
    {
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Unsupported binary operation")));
    }
  }
} /* jerry_binary_operation */

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

  jerry_value_t ret_val = jerry_acquire_value (ecma_get_extended_primitive_from_value (value)->u.value);

  if (release)
  {
    jerry_release_value (value);
  }
  return ret_val;
} /* jerry_get_value_from_error */

/**
 * Set new decorator callback for Error objects. The decorator can
 * create or update any properties of the newly created Error object.
 */
void
jerry_set_error_object_created_callback (jerry_error_object_created_callback_t callback, /**< new callback */
                                         void *user_p) /**< user pointer passed to the callback */
{
  jerry_assert_api_available ();

  JERRY_CONTEXT (error_object_created_callback_p) = callback;
  JERRY_CONTEXT (error_object_created_callback_user_p) = user_p;
} /* jerry_set_error_object_created_callback */

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
    value = ecma_get_extended_primitive_from_value (value)->u.value;
  }

  if (!ecma_is_value_object (value))
  {
    return JERRY_ERROR_NONE;
  }

  ecma_object_t *object_p = ecma_get_object_from_value (value);
  jerry_error_t error_type = ecma_get_error_type (object_p);

  return (jerry_error_t) error_type;
} /* jerry_get_error_type */

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
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
  }

  ecma_number_t num;
  ecma_value_t ret_value = ecma_op_to_number (value, &num);

  if (ECMA_IS_VALUE_ERROR (ret_value))
  {
    return ecma_create_error_reference_from_context ();
  }

  return ecma_make_number_value (num);
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
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
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
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
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
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
  }

  ecma_string_t *str_p = ecma_op_to_string (value);
  if (JERRY_UNLIKELY (str_p == NULL))
  {
    return ecma_create_error_reference_from_context ();
  }

  return ecma_make_string_value (str_p);
} /* jerry_value_to_string */

/**
 * Call the BigInt constructor ecma builtin operation on the api value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return BigInt value - if success
 *         thrown error - otherwise
 */
jerry_value_t
jerry_value_to_bigint (const jerry_value_t value) /**< input value */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_BIGINT
  if (ecma_is_value_error_reference (value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
  }

  return jerry_return (ecma_bigint_to_bigint (value, true));
#else /* !JERRY_BUILTIN_BIGINT */
  JERRY_UNUSED (value);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_bigint_not_supported_p)));
#endif /* JERRY_BUILTIN_BIGINT */
} /* jerry_value_to_bigint */

/**
 * Convert any number to integer number.
 *
 * Note:
 *      For non-number values 0 is returned.
 *
 * @return integer representation of the number.
 */
double
jerry_value_as_integer (const jerry_value_t value) /**< input value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_number (value))
  {
    return 0;
  }

  double number = ecma_get_number_from_value (value);

  if (ecma_number_is_nan (number))
  {
    return ECMA_NUMBER_ZERO;
  }

  if (ecma_number_is_zero (number) || ecma_number_is_infinity (number))
  {
    return number;
  }

  ecma_number_t floor_fabs = (ecma_number_t) floor (fabs (number));

  return ecma_number_is_negative (number) ? -floor_fabs : floor_fabs;
} /* jerry_value_as_integer */

/**
 * Convert any number to int32 number.
 *
 * Note:
 *      For non-number values 0 is returned.
 *
 * @return int32 representation of the number.
 */
int32_t
jerry_value_as_int32 (const jerry_value_t value) /**< input value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_number (value))
  {
    return 0;
  }

  return ecma_number_to_int32 (ecma_get_number_from_value (value));
} /* jerry_value_as_int32 */

/**
 * Convert any number to uint32 number.
 *
 * Note:
 *      For non-number values 0 is returned.
 *
 * @return uint32 representation of the number.
 */
uint32_t
jerry_value_as_uint32 (const jerry_value_t value) /**< input value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_number (value))
  {
    return 0;
  }

  return ecma_number_to_uint32 (ecma_get_number_from_value (value));
} /* jerry_value_as_uint32 */

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
    ecma_ref_extended_primitive (ecma_get_extended_primitive_from_value (value));
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
    ecma_deref_error_reference (ecma_get_extended_primitive_from_value (value));
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

  ecma_object_t *array_p = ecma_op_new_array_object (size);
  return ecma_make_object_value (array_p);
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
    return ecma_create_error_object_reference (ecma_new_standard_error ((jerry_error_t) error_type, NULL));
  }
  else
  {
    ecma_string_t *message_string_p = ecma_new_ecma_string_from_utf8 ((lit_utf8_byte_t *) message_p,
                                                                      (lit_utf8_size_t) message_size);

    ecma_object_t *error_object_p = ecma_new_standard_error ((jerry_error_t) error_type,
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

#if JERRY_BUILTIN_PROMISE
  ecma_value_t promise_value = ecma_op_create_promise_object (ECMA_VALUE_EMPTY, ECMA_VALUE_UNDEFINED, NULL);

  return promise_value;
#else /* !JERRY_BUILTIN_PROMISE */
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_promise_not_supported_p)));
#endif /* JERRY_BUILTIN_PROMISE */
} /* jerry_create_promise */

/**
 * Create a new Proxy object with the given target and handler
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the created Proxy object
 */
jerry_value_t
jerry_create_proxy (const jerry_value_t target, /**< target argument */
                    const jerry_value_t handler) /**< handler argument */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (target)
      || ecma_is_value_error_reference (handler))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

#if JERRY_BUILTIN_PROXY
  ecma_object_t *proxy_p = ecma_proxy_create (target, handler, 0);
  return jerry_return (proxy_p == NULL ? ECMA_VALUE_ERROR : ecma_make_object_value (proxy_p));
#else /* !JERRY_BUILTIN_PROXY */
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Proxy is not supported")));
#endif /* JERRY_BUILTIN_PROXY */
} /* jerry_create_proxy */

#if JERRY_BUILTIN_PROXY

JERRY_STATIC_ASSERT ((int) JERRY_PROXY_SKIP_RESULT_VALIDATION == (int) ECMA_PROXY_SKIP_RESULT_VALIDATION,
                     jerry_and_ecma_proxy_skip_result_validation_must_be_equal);

#endif /* JERRY_BUILTIN_PROXY */

/**
 * Create a new Proxy object with the given target, handler, and special options
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the created Proxy object
 */
jerry_value_t
jerry_create_special_proxy (const jerry_value_t target, /**< target argument */
                            const jerry_value_t handler, /**< handler argument */
                            uint32_t options) /**< jerry_proxy_object_options_t option bits */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (target)
      || ecma_is_value_error_reference (handler))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

#if JERRY_BUILTIN_PROXY
  options &= JERRY_PROXY_SKIP_RESULT_VALIDATION;

  ecma_object_t *proxy_p = ecma_proxy_create (target, handler, options);
  return jerry_return (proxy_p == NULL ? ECMA_VALUE_ERROR : ecma_make_object_value (proxy_p));
#else /* !JERRY_BUILTIN_PROXY */
  JERRY_UNUSED (options);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Proxy is not supported")));
#endif /* JERRY_BUILTIN_PROXY */
} /* jerry_create_special_proxy */

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
 * Create external string from a valid CESU-8 string
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the created external string
 */
jerry_value_t
jerry_create_external_string (const jerry_char_t *str_p, /**< pointer to string */
                              jerry_value_free_callback_t free_cb) /**< free callback */
{
  return jerry_create_external_string_sz (str_p, lit_zt_utf8_string_size ((lit_utf8_byte_t *) str_p), free_cb);
} /* jerry_create_external_string */

/**
 * Create external string from a valid CESU-8 string
 *
 * Note:
 *      returned value must be freed with jerry_release_value when it is no longer needed.
 *
 * @return value of the created external string
 */
jerry_value_t
jerry_create_external_string_sz (const jerry_char_t *str_p, /**< pointer to string */
                                 jerry_size_t str_size, /**< string size */
                                 jerry_value_free_callback_t free_cb) /**< free callback */
{
  jerry_assert_api_available ();

  ecma_string_t *ecma_str_p = ecma_new_ecma_external_string_from_cesu8 ((lit_utf8_byte_t *) str_p,
                                                                        (lit_utf8_size_t) str_size,
                                                                        free_cb);
  return ecma_make_string_value (ecma_str_p);
} /* jerry_create_external_string_sz */

/**
 * Create symbol from an api value
 *
 * Note:
 *      The given argument is converted to string. This operation can throw an error.
 *      returned value must be freed with jerry_release_value when it is no longer needed.
 *
 * @return value of the created symbol, if success
 *         thrown error, otherwise
 */
jerry_value_t
jerry_create_symbol (const jerry_value_t value) /**< api value */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

#if JERRY_ESNEXT
  return jerry_return (ecma_op_create_symbol (&value, 1));
#else /* !JERRY_ESNEXT */
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_symbol_not_supported_p)));
#endif /* JERRY_ESNEXT */
} /* jerry_create_symbol */

/**
 * Create BigInt from a sequence of uint64 digits
 *
 * @return value of the created bigint, if success
 *         thrown error, otherwise
 */
jerry_value_t
jerry_create_bigint (const uint64_t *digits_p, /**< BigInt digits (lowest digit first) */
                     uint32_t size, /**< number of BigInt digits */
                     bool sign) /**< sign bit, true if the result should be negative */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_BIGINT
  return jerry_return (ecma_bigint_create_from_digits (digits_p, size, sign));
#else /* !JERRY_BUILTIN_BIGINT */
  JERRY_UNUSED (digits_p);
  JERRY_UNUSED (size);
  JERRY_UNUSED (sign);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_bigint_not_supported_p)));
#endif /* JERRY_BUILTIN_BIGINT */
} /* jerry_create_bigint */

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

#if JERRY_BUILTIN_REGEXP
  if (!lit_is_valid_utf8_string (pattern_p, pattern_size))
  {
    return jerry_throw (ecma_raise_common_error (ECMA_ERR_MSG ("Input must be a valid utf8 string")));
  }

  ecma_object_t *regexp_obj_p = ecma_op_regexp_alloc (NULL);

  if (JERRY_UNLIKELY (regexp_obj_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }

  ecma_string_t *ecma_pattern = ecma_new_ecma_string_from_utf8 (pattern_p, pattern_size);

  jerry_value_t ret_val = ecma_op_create_regexp_with_flags (regexp_obj_p,
                                                            ecma_make_string_value (ecma_pattern),
                                                            flags);
  ecma_deref_ecma_string (ecma_pattern);

  return ret_val;

#else /* !JERRY_BUILTIN_REGEXP */
  JERRY_UNUSED (pattern_p);
  JERRY_UNUSED (pattern_size);
  JERRY_UNUSED (flags);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("RegExp is not supported")));
#endif /* JERRY_BUILTIN_REGEXP */
} /* jerry_create_regexp_sz */

/**
 * Creates a new realm (global object).
 *
 * @return new realm object
 */
jerry_value_t
jerry_create_realm (void)
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_REALMS
  ecma_global_object_t *global_object_p = ecma_builtin_create_global_object ();
  return ecma_make_object_value ((ecma_object_t *) global_object_p);
#else /* !JERRY_BUILTIN_REALMS */
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Realms are disabled")));
#endif /* JERRY_BUILTIN_REALMS */
} /* jerry_create_realm */

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

  if (!jerry_value_is_object (value))
  {
    return 0;
  }

  ecma_object_t *object_p = ecma_get_object_from_value (value);

  if (JERRY_LIKELY (ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_ARRAY))
  {
    return ecma_array_get_length (object_p);
  }

  return 0;
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
 * @return raised error - if the operation fail
 *         true/false API value  - depend on whether the property exists
 */
jerry_value_t
jerry_has_property (const jerry_value_t obj_val, /**< object value */
                    const jerry_value_t prop_name_val) /**< property name (string value) */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_prop_name (prop_name_val))
  {
    return ECMA_VALUE_FALSE;
  }

  return jerry_return (ecma_op_object_has_property (ecma_get_object_from_value (obj_val),
                                                    ecma_get_prop_name_from_value (prop_name_val)));
} /* jerry_has_property */

/**
 * Checks whether the object has the given property.
 *
 * @return ECMA_VALUE_ERROR - if the operation raises error
 *         ECMA_VALUE_{TRUE, FALSE} - based on whether the property exists
 */
jerry_value_t
jerry_has_own_property (const jerry_value_t obj_val, /**< object value */
                        const jerry_value_t prop_name_val) /**< property name (string value) */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_prop_name (prop_name_val))
  {
    return ECMA_VALUE_FALSE;
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (obj_val);
  ecma_string_t *prop_name_p = ecma_get_prop_name_from_value (prop_name_val);

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_IS_PROXY (obj_p))
  {
    ecma_property_descriptor_t prop_desc;

    ecma_value_t status = ecma_proxy_object_get_own_property_descriptor (obj_p, prop_name_p, &prop_desc);

    if (ecma_is_value_true (status))
    {
      ecma_free_property_descriptor (&prop_desc);
    }

    return jerry_return (status);
  }
#endif /* JERRY_BUILTIN_PROXY */

  return ecma_make_boolean_value (ecma_op_ordinary_object_has_own_property (obj_p, prop_name_p));
} /* jerry_has_own_property */

/**
 * Checks whether the object has the given internal property.
 *
 * @return true  - if the internal property exists
 *         false - otherwise
 */
bool
jerry_has_internal_property (const jerry_value_t obj_val, /**< object value */
                             const jerry_value_t prop_name_val) /**< property name value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_prop_name (prop_name_val))
  {
    return false;
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (obj_val);

  ecma_string_t *internal_string_p = ecma_get_magic_string (LIT_INTERNAL_MAGIC_API_INTERNAL);

  if (ecma_op_object_is_fast_array (obj_p))
  {
    return false;
  }

  ecma_property_t *property_p = ecma_find_named_property (obj_p, internal_string_p);

  if (property_p == NULL)
  {
    return false;
  }

  ecma_object_t *internal_object_p = ecma_get_object_from_value (ECMA_PROPERTY_VALUE_PTR (property_p)->value);
  property_p = ecma_find_named_property (internal_object_p, ecma_get_prop_name_from_value (prop_name_val));

  return property_p != NULL;
} /* jerry_has_internal_property */

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
      || !ecma_is_value_prop_name (prop_name_val))
  {
    return false;
  }

  ecma_value_t ret_value = ecma_op_object_delete (ecma_get_object_from_value (obj_val),
                                                  ecma_get_prop_name_from_value (prop_name_val),
                                                  false);

#if JERRY_BUILTIN_PROXY
  if (ECMA_IS_VALUE_ERROR (ret_value))
  {
    // TODO: Due to Proxies the return value must be changed to jerry_value_t on next release
    jcontext_release_exception ();
  }
#endif /* JERRY_BUILTIN_PROXY */

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

#if JERRY_BUILTIN_PROXY
  if (ECMA_IS_VALUE_ERROR (ret_value))
  {
    // TODO: Due to Proxies the return value must be changed to jerry_value_t on next release
    jcontext_release_exception ();
  }
#endif /* JERRY_BUILTIN_PROXY */

  return ecma_is_value_true (ret_value);
} /* jerry_delete_property_by_index */

/**
 * Delete an internal property from an object.
 *
 * @return true  - if property was deleted successfully
 *         false - otherwise
 */
bool
jerry_delete_internal_property (const jerry_value_t obj_val, /**< object value */
                                const jerry_value_t prop_name_val) /**< property name value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_prop_name (prop_name_val))
  {
    return false;
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (obj_val);

  ecma_string_t *internal_string_p = ecma_get_magic_string (LIT_INTERNAL_MAGIC_API_INTERNAL);

  if (ecma_op_object_is_fast_array (obj_p))
  {
    return true;
  }

  ecma_property_t *property_p = ecma_find_named_property (obj_p, internal_string_p);

  if (property_p == NULL)
  {
    return true;
  }

  ecma_object_t *internal_object_p = ecma_get_object_from_value (ECMA_PROPERTY_VALUE_PTR (property_p)->value);
  property_p = ecma_find_named_property (internal_object_p, ecma_get_prop_name_from_value (prop_name_val));

  if (property_p == NULL)
  {
    return true;
  }

  ecma_delete_property (internal_object_p, ECMA_PROPERTY_VALUE_PTR (property_p));

  return true;
} /* jerry_delete_internal_property */

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
      || !ecma_is_value_prop_name (prop_name_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  jerry_value_t ret_value = ecma_op_object_get (ecma_get_object_from_value (obj_val),
                                                ecma_get_prop_name_from_value (prop_name_val));
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
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_value_t ret_value = ecma_op_object_get_by_index (ecma_get_object_from_value (obj_val), index);

  return jerry_return (ret_value);
} /* jerry_get_property_by_index */

/**
 * Get the own property value of an object with the given name.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the property - if success
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_get_own_property (const jerry_value_t obj_val, /**< object value */
                        const jerry_value_t prop_name_val, /**< property name (string value) */
                        const jerry_value_t receiver_val, /**< receiver object value */
                        bool *found_p) /**< [out] true, if the property is found
                                        *   or obj_val is a Proxy object, false otherwise */
{
  jerry_assert_api_available ();

  if (found_p != NULL)
  {
    *found_p = false;
  }

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_prop_name (prop_name_val)
      || !ecma_is_value_object (receiver_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_object_t *object_p = ecma_get_object_from_value (obj_val);
  ecma_string_t *property_name_p = ecma_get_prop_name_from_value (prop_name_val);

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_IS_PROXY (object_p))
  {
    if (found_p != NULL)
    {
      *found_p = true;
    }

    return jerry_return (ecma_proxy_object_get (object_p, property_name_p, receiver_val));
  }
#endif /* JERRY_BUILTIN_PROXY */

  ecma_value_t ret_value = ecma_op_object_find_own (receiver_val, object_p, property_name_p);

  if (ecma_is_value_found (ret_value))
  {
    if (found_p != NULL)
    {
      *found_p = true;
    }

    return jerry_return (ret_value);
  }

  return ECMA_VALUE_UNDEFINED;
} /* jerry_get_own_property */

/**
 * Get value of an internal property to the specified object with the given name.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return value of the internal property - if the internal property exists
 *         undefined value - if the internal does not property exists
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_get_internal_property (const jerry_value_t obj_val, /**< object value */
                             const jerry_value_t prop_name_val) /**< property name value */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_prop_name (prop_name_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (obj_val);

  ecma_string_t *internal_string_p = ecma_get_magic_string (LIT_INTERNAL_MAGIC_API_INTERNAL);

  if (ecma_op_object_is_fast_array (obj_p))
  {
    return jerry_return (ECMA_VALUE_UNDEFINED);
  }

  ecma_property_t *property_p = ecma_find_named_property (obj_p, internal_string_p);

  if (property_p == NULL)
  {
    return jerry_return (ECMA_VALUE_UNDEFINED);
  }

  ecma_object_t *internal_object_p = ecma_get_object_from_value (ECMA_PROPERTY_VALUE_PTR (property_p)->value);
  property_p = ecma_find_named_property (internal_object_p, ecma_get_prop_name_from_value (prop_name_val));

  if (property_p == NULL)
  {
    return jerry_return (ECMA_VALUE_UNDEFINED);
  }

  return jerry_return (ecma_copy_value (ECMA_PROPERTY_VALUE_PTR (property_p)->value));
} /* jerry_get_internal_property */

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
      || !ecma_is_value_prop_name (prop_name_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  return jerry_return (ecma_op_object_put (ecma_get_object_from_value (obj_val),
                                           ecma_get_prop_name_from_value (prop_name_val),
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
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_value_t ret_value = ecma_op_object_put_by_index (ecma_get_object_from_value (obj_val),
                                                        index,
                                                        value_to_set,
                                                        true);

  return jerry_return (ret_value);
} /* jerry_set_property_by_index */

/**
 * Set an internal property to the specified object with the given name.
 *
 * Note:
 *      - the property cannot be accessed from the JavaScript context, only from the public API
 *      - returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return true value - if the operation was successful
 *         value marked with error flag - otherwise
 */
bool
jerry_set_internal_property (const jerry_value_t obj_val, /**< object value */
                             const jerry_value_t prop_name_val, /**< property name value */
                             const jerry_value_t value_to_set) /**< value to set */
{
  jerry_assert_api_available ();

  if (ecma_is_value_error_reference (value_to_set)
      || !ecma_is_value_object (obj_val)
      || !ecma_is_value_prop_name (prop_name_val))
  {
    return false;
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (obj_val);

  ecma_string_t *internal_string_p = ecma_get_magic_string (LIT_INTERNAL_MAGIC_API_INTERNAL);

  if (ecma_op_object_is_fast_array (obj_p))
  {
    ecma_fast_array_convert_to_normal (obj_p);
  }

  ecma_property_t *property_p = ecma_find_named_property (obj_p, internal_string_p);
  ecma_object_t *internal_object_p;

  if (property_p == NULL)
  {
    ecma_property_value_t *value_p = ecma_create_named_data_property (obj_p,
                                                                      internal_string_p,
                                                                      ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE,
                                                                      NULL);

    internal_object_p = ecma_create_object (NULL,
                                            sizeof (ecma_extended_object_t),
                                            ECMA_OBJECT_TYPE_CLASS);
    {
      ecma_extended_object_t *container_p = (ecma_extended_object_t *) internal_object_p;
      container_p->u.cls.type = ECMA_OBJECT_CLASS_INTERNAL_OBJECT;
    }

    value_p->value = ecma_make_object_value (internal_object_p);
    ecma_deref_object (internal_object_p);
  }
  else
  {
    internal_object_p = ecma_get_object_from_value (ECMA_PROPERTY_VALUE_PTR (property_p)->value);
  }

  ecma_string_t *prop_name_p = ecma_get_prop_name_from_value (prop_name_val);
  property_p = ecma_find_named_property (internal_object_p, prop_name_p);

  if (property_p == NULL)
  {
    ecma_property_value_t *value_p = ecma_create_named_data_property (internal_object_p,
                                                                      prop_name_p,
                                                                      ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE,
                                                                      NULL);

    value_p->value = ecma_copy_value_if_not_object (value_to_set);
  }
  else
  {
    ecma_named_data_property_assign_value (internal_object_p, ECMA_PROPERTY_VALUE_PTR (property_p), value_to_set);
  }

  return true;
} /* jerry_set_internal_property */

/**
 * Construct empty property descriptor, i.e.:
 *  property descriptor with all is_defined flags set to false and the rest - to default value.
 *
 * @return empty property descriptor
 */
jerry_property_descriptor_t
jerry_property_descriptor_create (void)
{
  jerry_property_descriptor_t prop_desc;

  prop_desc.flags = JERRY_PROP_NO_OPTS;
  prop_desc.value = ECMA_VALUE_UNDEFINED;
  prop_desc.getter = ECMA_VALUE_UNDEFINED;
  prop_desc.setter = ECMA_VALUE_UNDEFINED;

  return prop_desc;
} /* jerry_property_descriptor_create */

/**
 * Convert a ecma_property_descriptor_t to a jerry_property_descriptor_t
 *
 * if error occurs the property descriptor's value field is filled with ECMA_VALUE_ERROR
 *
 * @return jerry_property_descriptor_t
 */
static jerry_property_descriptor_t
jerry_property_descriptor_from_ecma (const ecma_property_descriptor_t *prop_desc_p) /**<[out] property_descriptor */
{
  jerry_property_descriptor_t prop_desc = jerry_property_descriptor_create ();

  prop_desc.flags = prop_desc_p->flags;

  if (prop_desc.flags & (JERRY_PROP_IS_VALUE_DEFINED))
  {
    prop_desc.value = prop_desc_p->value;
  }

  if (prop_desc_p->flags & JERRY_PROP_IS_GET_DEFINED)
  {
    prop_desc.getter = ECMA_VALUE_NULL;

    if (prop_desc_p->get_p != NULL)
    {
      prop_desc.getter = ecma_make_object_value (prop_desc_p->get_p);
      JERRY_ASSERT (ecma_op_is_callable (prop_desc.getter));
    }
  }

  if (prop_desc_p->flags & JERRY_PROP_IS_SET_DEFINED)
  {
    prop_desc.setter = ECMA_VALUE_NULL;

    if (prop_desc_p->set_p != NULL)
    {
      prop_desc.setter = ecma_make_object_value (prop_desc_p->set_p);
      JERRY_ASSERT (ecma_op_is_callable (prop_desc.setter));
    }
  }

  return prop_desc;
} /* jerry_property_descriptor_from_ecma */

/**
 * Convert a jerry_property_descriptor_t to a ecma_property_descriptor_t
 *
 * Note:
 *     if error occurs the property descriptor's value field
 *     is set to ECMA_VALUE_ERROR, but no error is thrown
 *
 * @return ecma_property_descriptor_t
 */
static ecma_property_descriptor_t
jerry_property_descriptor_to_ecma (const jerry_property_descriptor_t *prop_desc_p) /**< input property_descriptor */
{
  ecma_property_descriptor_t prop_desc = ecma_make_empty_property_descriptor ();

  prop_desc.flags = prop_desc_p->flags;

  /* Copy data property info. */
  if (prop_desc_p->flags & JERRY_PROP_IS_VALUE_DEFINED)
  {
    if (ecma_is_value_error_reference (prop_desc_p->value)
        || (prop_desc_p->flags & (JERRY_PROP_IS_GET_DEFINED | JERRY_PROP_IS_SET_DEFINED)))
    {
      prop_desc.value = ECMA_VALUE_ERROR;
      return prop_desc;
    }

    prop_desc.value = prop_desc_p->value;
  }

  /* Copy accessor property info. */
  if (prop_desc_p->flags & JERRY_PROP_IS_GET_DEFINED)
  {
    ecma_value_t getter = prop_desc_p->getter;

    if (ecma_is_value_error_reference (getter))
    {
      prop_desc.value = ECMA_VALUE_ERROR;
      return prop_desc;
    }

    if (ecma_op_is_callable (getter))
    {
      prop_desc.get_p = ecma_get_object_from_value (getter);
    }
    else if (!ecma_is_value_null (getter))
    {
      prop_desc.value = ECMA_VALUE_ERROR;
      return prop_desc;
    }
  }

  if (prop_desc_p->flags & JERRY_PROP_IS_SET_DEFINED)
  {
    ecma_value_t setter = prop_desc_p->setter;

    if (ecma_is_value_error_reference (setter))
    {
      prop_desc.value = ECMA_VALUE_ERROR;
      return prop_desc;
    }

    if (ecma_op_is_callable (setter))
    {
      prop_desc.set_p = ecma_get_object_from_value (setter);
    }
    else if (!ecma_is_value_null (setter))
    {
      prop_desc.value = ECMA_VALUE_ERROR;
      return prop_desc;
    }
  }

  const uint16_t configurable_mask = JERRY_PROP_IS_CONFIGURABLE | JERRY_PROP_IS_CONFIGURABLE_DEFINED;
  const uint16_t enumerable_mask = JERRY_PROP_IS_ENUMERABLE | JERRY_PROP_IS_ENUMERABLE_DEFINED;
  const uint16_t writable_mask = JERRY_PROP_IS_WRITABLE | JERRY_PROP_IS_WRITABLE_DEFINED;

  if ((prop_desc_p->flags & configurable_mask) == JERRY_PROP_IS_CONFIGURABLE
      || (prop_desc_p->flags & enumerable_mask) == JERRY_PROP_IS_ENUMERABLE
      || (prop_desc_p->flags & writable_mask) == JERRY_PROP_IS_WRITABLE)
  {
    prop_desc.value = ECMA_VALUE_ERROR;
    return prop_desc;
  }

  prop_desc.flags |= (uint16_t) (prop_desc_p->flags | JERRY_PROP_SHOULD_THROW);

  return prop_desc;
} /* jerry_property_descriptor_to_ecma */

/** Helper function to return false value or error depending on the given flag.
 *
 * @return value marked with error flag - if is_throw is true
 *         false value - otherwise
 */
static jerry_value_t
jerry_type_error_or_false (const char *msg_p, /**< message */
                           uint16_t flags) /**< property descriptor flags */
{
  if (!(flags & JERRY_PROP_SHOULD_THROW))
  {
    return ECMA_VALUE_FALSE;
  }

  return jerry_throw (ecma_raise_type_error (msg_p));
} /* jerry_type_error_or_false */

/**
 * Define a property to the specified object with the given name.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return true value - if the operation was successful
 *         false value - if the property cannot be defined and JERRY_PROP_SHOULD_THROW is not set
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_define_own_property (const jerry_value_t obj_val, /**< object value */
                           const jerry_value_t prop_name_val, /**< property name (string value) */
                           const jerry_property_descriptor_t *prop_desc_p) /**< property descriptor */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_prop_name (prop_name_val))
  {
    return jerry_type_error_or_false (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p), prop_desc_p->flags);
  }

  if (prop_desc_p->flags & (JERRY_PROP_IS_WRITABLE_DEFINED | JERRY_PROP_IS_VALUE_DEFINED)
      && prop_desc_p->flags & (JERRY_PROP_IS_GET_DEFINED | JERRY_PROP_IS_SET_DEFINED))
  {
    return jerry_type_error_or_false (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p), prop_desc_p->flags);
  }

  ecma_property_descriptor_t prop_desc = jerry_property_descriptor_to_ecma (prop_desc_p);

  if (ECMA_IS_VALUE_ERROR (prop_desc.value))
  {
    return jerry_type_error_or_false (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p), prop_desc_p->flags);
  }

  return jerry_return (ecma_op_object_define_own_property (ecma_get_object_from_value (obj_val),
                                                           ecma_get_prop_name_from_value (prop_name_val),
                                                           &prop_desc));
} /* jerry_define_own_property */

/**
 * Construct property descriptor from specified property.
 *
 * @return true - if success, the prop_desc_p fields contains the property info
 *         false - otherwise, the prop_desc_p is unchanged
 */
jerry_value_t
jerry_get_own_property_descriptor (const jerry_value_t  obj_val, /**< object value */
                                   const jerry_value_t prop_name_val, /**< property name (string value) */
                                   jerry_property_descriptor_t *prop_desc_p) /**< property descriptor */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val)
      || !ecma_is_value_prop_name (prop_name_val))
  {
    return ECMA_VALUE_FALSE;
  }

  ecma_property_descriptor_t prop_desc;

  ecma_value_t status = ecma_op_object_get_own_property_descriptor (ecma_get_object_from_value (obj_val),
                                                                    ecma_get_prop_name_from_value (prop_name_val),
                                                                    &prop_desc);

#if JERRY_BUILTIN_PROXY
  if (ECMA_IS_VALUE_ERROR (status))
  {
    return jerry_throw (status);
  }
#endif /* JERRY_BUILTIN_PROXY */

  if (!ecma_is_value_true (status))
  {
    return ECMA_VALUE_FALSE;
  }

  /* The flags are always filled in the returned descriptor. */
  JERRY_ASSERT ((prop_desc.flags & JERRY_PROP_IS_CONFIGURABLE_DEFINED)
                && (prop_desc.flags & JERRY_PROP_IS_ENUMERABLE_DEFINED)
                && ((prop_desc.flags & JERRY_PROP_IS_WRITABLE_DEFINED)
                    || !(prop_desc.flags & JERRY_PROP_IS_VALUE_DEFINED)));

  prop_desc_p->flags = prop_desc.flags;
  prop_desc_p->value = ECMA_VALUE_UNDEFINED;
  prop_desc_p->getter = ECMA_VALUE_UNDEFINED;
  prop_desc_p->setter = ECMA_VALUE_UNDEFINED;

  if (prop_desc_p->flags & JERRY_PROP_IS_VALUE_DEFINED)
  {
    prop_desc_p->value = prop_desc.value;
  }

  if (prop_desc_p->flags & JERRY_PROP_IS_GET_DEFINED)
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

  if (prop_desc_p->flags & JERRY_PROP_IS_SET_DEFINED)
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

  return ECMA_VALUE_TRUE;
} /* jerry_get_own_property_descriptor */

/**
 * Free fields of property descriptor (setter, getter and value).
 */
void
jerry_property_descriptor_free (const jerry_property_descriptor_t *prop_desc_p) /**< property descriptor */
{
  if (prop_desc_p->flags & JERRY_PROP_IS_VALUE_DEFINED)
  {
    jerry_release_value (prop_desc_p->value);
  }

  if (prop_desc_p->flags & JERRY_PROP_IS_GET_DEFINED)
  {
    jerry_release_value (prop_desc_p->getter);
  }

  if (prop_desc_p->flags & JERRY_PROP_IS_SET_DEFINED)
  {
    jerry_release_value (prop_desc_p->setter);
  }
} /* jerry_property_descriptor_free */

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
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
  }

  for (uint32_t i = 0; i < args_count; i++)
  {
    if (ecma_is_value_error_reference (args_p[i]))
    {
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
    }
  }

  if (is_invoke_as_constructor)
  {
    JERRY_ASSERT (jerry_value_is_constructor (func_obj_val));

    return jerry_return (ecma_op_function_construct (ecma_get_object_from_value (func_obj_val),
                                                     ecma_get_object_from_value (func_obj_val),
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

  if (jerry_value_is_function (func_obj_val) && !ecma_is_value_error_reference (this_val))
  {
    for (jerry_size_t i = 0; i < args_count; i++)
    {
      if (ecma_is_value_error_reference (args_p[i]))
      {
        return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
      }
    }

    return jerry_invoke_function (false, func_obj_val, this_val, args_p, args_count);
  }

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
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
    for (jerry_size_t i = 0; i < args_count; i++)
    {
      if (ecma_is_value_error_reference (args_p[i]))
      {
        return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
      }
    }

    ecma_value_t this_val = ECMA_VALUE_UNDEFINED;
    return jerry_invoke_function (true, func_obj_val, this_val, args_p, args_count);
  }

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
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
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_collection_t *prop_names = ecma_op_object_get_enumerable_property_names (ecma_get_object_from_value (obj_val),
                                                                                ECMA_ENUMERABLE_PROPERTY_KEYS);

#if JERRY_BUILTIN_PROXY
  if (JERRY_UNLIKELY (prop_names == NULL))
  {
    return ECMA_VALUE_ERROR;
  }
#endif /* JERRY_BUILTIN_PROXY */

  return ecma_op_new_array_object_from_collection (prop_names, false);
} /* jerry_get_object_keys */

/**
 * Get the prototype of the specified object
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
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
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (obj_val);

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_IS_PROXY (obj_p))
  {
    return jerry_return (ecma_proxy_object_get_prototype_of (obj_p));
  }
#endif /* JERRY_BUILTIN_PROXY */

  if (obj_p->u2.prototype_cp == JMEM_CP_NULL)
  {
    return ECMA_VALUE_NULL;
  }

  ecma_object_t *proto_obj_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, obj_p->u2.prototype_cp);
  ecma_ref_object (proto_obj_p);

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
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }
  ecma_object_t *obj_p = ecma_get_object_from_value (obj_val);

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_IS_PROXY (obj_p))
  {
    return jerry_return (ecma_proxy_object_set_prototype_of (obj_p, proto_obj_val));
  }
#endif /* JERRY_BUILTIN_PROXY */

  return ecma_op_ordinary_object_set_prototype_of (obj_p, proto_obj_val);
} /* jerry_set_prototype */

/**
 * Utility to check if a given object can be used for the foreach api calls.
 *
 * Some objects/classes uses extra internal objects to correctly store data.
 * These extre object should never be exposed externally to the API user.
 *
 * @returns true - if the user can access the object in the callback.
 *          false - if the object is an internal object which should no be accessed by the user.
 */
static
bool jerry_object_is_valid_foreach (ecma_object_t *object_p) /**< object to test */
{
  if (ecma_is_lexical_environment (object_p))
  {
    return false;
  }

  ecma_object_type_t object_type = ecma_get_object_type (object_p);

  if (object_type == ECMA_OBJECT_TYPE_CLASS)
  {
    ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;
    switch (ext_object_p->u.cls.type)
    {
      /* An object's internal property object should not be iterable by foreach. */
      case ECMA_OBJECT_CLASS_INTERNAL_OBJECT:
      {
        return false;
      }
    }
  }

  return true;
} /* jerry_object_is_valid_foreach */

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

  jmem_cpointer_t iter_cp = JERRY_CONTEXT (ecma_gc_objects_cp);

  while (iter_cp != JMEM_CP_NULL)
  {
    ecma_object_t *iter_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, iter_cp);

    if (jerry_object_is_valid_foreach (iter_p)
        && !foreach_p (ecma_make_object_value (iter_p), user_data_p))
    {
      return true;
    }

    iter_cp = iter_p->gc_next_cp;
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

  jmem_cpointer_t iter_cp = JERRY_CONTEXT (ecma_gc_objects_cp);

  while (iter_cp != JMEM_CP_NULL)
  {
    ecma_object_t *iter_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, iter_cp);

    if (jerry_object_is_valid_foreach (iter_p))
    {
      native_pointer_p = ecma_get_native_pointer_value (iter_p, (void *) native_info_p);
      if (native_pointer_p
          && !foreach_p (ecma_make_object_value (iter_p), native_pointer_p->native_p, user_data_p))
      {
        return true;
      }
    }

    iter_cp = iter_p->gc_next_cp;
  }

  return false;
} /* jerry_objects_foreach_by_native_info */

/**
 * Get native pointer and its type information, associated with the given native type info.
 *
 * Note:
 *  If native pointer is present, its type information is returned in out_native_pointer_p
 *
 * @return true - if there is an associated pointer,
 *         false - otherwise
 */
bool
jerry_get_object_native_pointer (const jerry_value_t obj_val, /**< object to get native pointer from */
                                 void **out_native_pointer_p, /**< [out] native pointer */
                                 const jerry_object_native_info_t *native_info_p) /**< the type info
                                                                                   *   of the native pointer */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val))
  {
    return false;
  }

  ecma_native_pointer_t *native_pointer_p;
  native_pointer_p = ecma_get_native_pointer_value (ecma_get_object_from_value (obj_val), (void *) native_info_p);

  if (native_pointer_p == NULL)
  {
    return false;
  }

  if (out_native_pointer_p != NULL)
  {
    *out_native_pointer_p = native_pointer_p->native_p;
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
 *      Referred values by this method must have at least 1 reference. (Correct API usage satisfies this condition)
 *      The type info always overwrites the previous value, so passing
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

    ecma_create_native_pointer_property (object_p, native_pointer_p, native_info_p);
  }
} /* jerry_set_object_native_pointer */

/**
 * Delete the previously set native pointer by the native type info from the specified object.
 *
 * Note:
 *      If the specified object has no matching native pointer for the given native type info
 *      the function has no effect.
 *
 * Note:
 *      This operation cannot throw an exception.
 *
 * @return true - if the native pointer has been deleted succesfully
 *         false - otherwise
 */
bool
jerry_delete_object_native_pointer (const jerry_value_t obj_val, /**< object to delete native pointer from */
                                    const jerry_object_native_info_t *native_info_p) /**< object's native type info */
{
  jerry_assert_api_available ();

  if (ecma_is_value_object (obj_val))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (obj_val);

    return ecma_delete_native_pointer_property (object_p, (void *) native_info_p);
  }

  return false;
} /* jerry_delete_object_native_pointer */

/**
 * Initialize the references stored in a buffer pointed by a native pointer.
 * The references are initialized to undefined.
 */
void
jerry_native_pointer_init_references (void *native_pointer_p, /**< a valid non-NULL pointer to a native buffer */
                                      const jerry_object_native_info_t *native_info_p) /**< the type info of
                                                                                        *   the native pointer */
{
  jerry_assert_api_available ();

  if (native_pointer_p == NULL || native_info_p == NULL)
  {
    return;
  }

  ecma_value_t *value_p = (ecma_value_t *) (((uint8_t *) native_pointer_p) + native_info_p->offset_of_references);
  ecma_value_t *end_p = value_p + native_info_p->number_of_references;

  while (value_p < end_p)
  {
    *value_p++ = ECMA_VALUE_UNDEFINED;
  }
} /* jerry_native_pointer_init_references */

/**
 * Release the value references after a buffer pointed by a native pointer
 * is not attached to an object anymore. All references are set to undefined
 * similar to jerry_native_pointer_init_references.
 */
void
jerry_native_pointer_release_references (void *native_pointer_p, /**< a valid non-NULL pointer to a native buffer */
                                         const jerry_object_native_info_t *native_info_p) /**< the type info of
                                                                                           *   the native pointer */
{
  jerry_assert_api_available ();

  if (native_pointer_p == NULL || native_info_p == NULL)
  {
    return;
  }

  ecma_value_t *value_p = (ecma_value_t *) (((uint8_t *) native_pointer_p) + native_info_p->offset_of_references);
  ecma_value_t *end_p = value_p + native_info_p->number_of_references;

  while (value_p < end_p)
  {
    ecma_free_value_if_not_object (*value_p);
    *value_p++ = ECMA_VALUE_UNDEFINED;
  }
} /* jerry_native_pointer_release_references */

/**
 * Updates a value reference inside the area specified by the number_of_references and
 * offset_of_references fields in its corresponding jerry_object_native_info_t data.
 * The area must be part of a buffer which is currently assigned to an object.
 *
 * Note:
 *      Error references are not supported, they are replaced by undefined values.
 */
void
jerry_native_pointer_set_reference (jerry_value_t *reference_p, /**< a valid non-NULL pointer to
                                                                 *   a reference in a native buffer. */
                                    jerry_value_t value) /**< new value of the reference */
{
  jerry_assert_api_available ();

  if (reference_p == NULL)
  {
    return;
  }

  if (ecma_is_value_error_reference (value))
  {
    value = ECMA_VALUE_UNDEFINED;
  }

  ecma_free_value_if_not_object (*reference_p);
  *reference_p = ecma_copy_value_if_not_object (value);
} /* jerry_native_pointer_set_reference */

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
  ecma_collection_t *names_p = ecma_op_object_enumerate (object_p);

#if JERRY_BUILTIN_PROXY
  if (names_p == NULL)
  {
    // TODO: Due to Proxies the return value must be changed to jerry_value_t on next release
    jcontext_release_exception ();
    return false;
  }
#endif /* JERRY_BUILTIN_PROXY */

  ecma_value_t *buffer_p = names_p->buffer_p;

  ecma_value_t property_value = ECMA_VALUE_EMPTY;

  bool continuous = true;

  for (uint32_t i = 0; continuous && (i < names_p->item_count); i++)
  {
    ecma_string_t *property_name_p = ecma_get_string_from_value (buffer_p[i]);

    property_value = ecma_op_object_get (object_p, property_name_p);

    if (ECMA_IS_VALUE_ERROR (property_value))
    {
      break;
    }

    continuous = foreach_p (buffer_p[i], property_value, user_data_p);
    ecma_free_value (property_value);
  }

  ecma_collection_free (names_p);

  if (!ECMA_IS_VALUE_ERROR (property_value))
  {
    return true;
  }

  jcontext_release_exception ();
  return false;
} /* jerry_foreach_object_property */

/**
 * Gets the property keys for the given object using the selected filters.
 *
 * @return array containing the filtered property keys in successful operation
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_object_get_property_names (const jerry_value_t obj_val, /**< object */
                                 jerry_property_filter_t filter) /**< property filter options */
{
  jerry_assert_api_available ();

  if (!ecma_is_value_object (obj_val))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (obj_val);
  ecma_object_t *obj_iter_p = obj_p;
  ecma_collection_t *result_p = ecma_new_collection ();

  ecma_ref_object (obj_iter_p);

  while (true)
  {
    /* Step 1. Get Object.[[OwnKeys]] */
    ecma_collection_t *prop_names_p = ecma_op_object_own_property_keys (obj_iter_p);

#if JERRY_BUILTIN_PROXY
    if (prop_names_p == NULL)
    {
      ecma_deref_object (obj_iter_p);
      return jerry_throw (ECMA_VALUE_ERROR);
    }
#endif /* JERRY_BUILTIN_PROXY */

    for (uint32_t i = 0; i < prop_names_p->item_count; i++)
    {
      ecma_value_t key = prop_names_p->buffer_p[i];
      ecma_string_t *key_p = ecma_get_prop_name_from_value (key);
      uint32_t index = ecma_string_get_array_index (key_p);

      /* Step 2. Filter by key type */
      if (filter & (JERRY_PROPERTY_FILTER_EXLCUDE_STRINGS
                    | JERRY_PROPERTY_FILTER_EXLCUDE_SYMBOLS
                    | JERRY_PROPERTY_FILTER_EXLCUDE_INTEGER_INDICES))
      {
        if (ecma_is_value_symbol (key))
        {
          if (filter & JERRY_PROPERTY_FILTER_EXLCUDE_SYMBOLS)
          {
            continue;
          }
        }
        else if (index != ECMA_STRING_NOT_ARRAY_INDEX)
        {
          if ((filter & JERRY_PROPERTY_FILTER_EXLCUDE_INTEGER_INDICES)
              || ((filter & JERRY_PROPERTY_FILTER_EXLCUDE_STRINGS)
                  && !(filter & JERRY_PROPERTY_FILTER_INTEGER_INDICES_AS_NUMBER)))
          {
            continue;
          }
        }
        else if (filter & JERRY_PROPERTY_FILTER_EXLCUDE_STRINGS)
        {
          continue;
        }
      }

      /* Step 3. Filter property attributes */
      if (filter & (JERRY_PROPERTY_FILTER_EXLCUDE_NON_CONFIGURABLE
                    | JERRY_PROPERTY_FILTER_EXLCUDE_NON_ENUMERABLE
                    | JERRY_PROPERTY_FILTER_EXLCUDE_NON_WRITABLE))
      {
        ecma_property_descriptor_t prop_desc;
        ecma_value_t status = ecma_op_object_get_own_property_descriptor (obj_iter_p, key_p, &prop_desc);

#if JERRY_BUILTIN_PROXY
        if (ECMA_IS_VALUE_ERROR (status))
        {
          ecma_collection_free (prop_names_p);
          ecma_collection_free (result_p);
          ecma_deref_object (obj_iter_p);
          return jerry_throw (ECMA_VALUE_ERROR);
        }
#endif /* JERRY_BUILTIN_PROXY */

        JERRY_ASSERT (ecma_is_value_true (status));
        uint16_t flags = prop_desc.flags;
        ecma_free_property_descriptor (&prop_desc);

        if ((!(flags & JERRY_PROP_IS_CONFIGURABLE)
             && (filter & JERRY_PROPERTY_FILTER_EXLCUDE_NON_CONFIGURABLE))
            || (!(flags & JERRY_PROP_IS_ENUMERABLE)
                && (filter & JERRY_PROPERTY_FILTER_EXLCUDE_NON_ENUMERABLE))
            || (!(flags & JERRY_PROP_IS_WRITABLE)
                && (filter & JERRY_PROPERTY_FILTER_EXLCUDE_NON_WRITABLE)))
        {
          continue;
        }
      }

      if (index != ECMA_STRING_NOT_ARRAY_INDEX
          && (filter & JERRY_PROPERTY_FILTER_INTEGER_INDICES_AS_NUMBER))
      {
        ecma_deref_ecma_string (key_p);
        key = ecma_make_uint32_value (index);
      }
      else
      {
        ecma_ref_ecma_string (key_p);
      }

      if ((filter & JERRY_PROPERTY_FILTER_TRAVERSE_PROTOTYPE_CHAIN) && obj_iter_p != obj_p)
      {
        uint32_t duplicate_idx = 0;
        while (duplicate_idx < result_p->item_count)
        {
          ecma_value_t value = result_p->buffer_p[duplicate_idx];
          JERRY_ASSERT (ecma_is_value_prop_name (value) || ecma_is_value_number (value));
          if (JERRY_UNLIKELY (ecma_is_value_number (value)))
          {
            if (ecma_get_number_from_value (value) == ecma_get_number_from_value (key))
            {
              break;
            }
          }
          else if (ecma_compare_ecma_strings (ecma_get_prop_name_from_value (value), key_p))
          {
            break;
          }

          duplicate_idx++;
        }

        if (duplicate_idx == result_p->item_count)
        {
          ecma_collection_push_back (result_p, key);
        }
      }
      else
      {
        ecma_collection_push_back (result_p, key);
      }
    }

    ecma_collection_free (prop_names_p);

    /* Step 4: Traverse prototype chain */

    if ((filter & JERRY_PROPERTY_FILTER_TRAVERSE_PROTOTYPE_CHAIN) != JERRY_PROPERTY_FILTER_TRAVERSE_PROTOTYPE_CHAIN)
    {
      break;
    }

    ecma_object_t *proto_p = ecma_op_object_get_prototype_of (obj_iter_p);

    if (proto_p == NULL)
    {
      break;
    }

    ecma_deref_object (obj_iter_p);

    if (JERRY_UNLIKELY (proto_p == ECMA_OBJECT_POINTER_ERROR))
    {
      ecma_collection_free (result_p);
      return jerry_throw (ECMA_VALUE_ERROR);
    }

    obj_iter_p = proto_p;
  }

  ecma_deref_object (obj_iter_p);

  return ecma_op_new_array_object_from_collection (result_p, false);
} /* jerry_object_get_property_names */

/**
 * FromPropertyDescriptor abstract operation.
 *
 * @return new jerry_value_t - if success
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_from_property_descriptor (const jerry_property_descriptor_t *src_prop_desc_p) /**< property descriptor */
{
  jerry_assert_api_available ();

  ecma_property_descriptor_t prop_desc = jerry_property_descriptor_to_ecma (src_prop_desc_p);

  if (ECMA_IS_VALUE_ERROR (prop_desc.value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_object_t *desc_obj_p = ecma_op_from_property_descriptor (&prop_desc);

  return ecma_make_object_value (desc_obj_p);
} /* jerry_from_property_descriptor */

/**
 * ToPropertyDescriptor abstract operation.
 *
 * @return true - if the conversion is successful
 *         thrown error - otherwise
 */
jerry_value_t
jerry_to_property_descriptor (jerry_value_t obj_value, /**< object value */
                              jerry_property_descriptor_t *out_prop_desc_p) /**< [out] filled property descriptor
                                                                             *   if return value is true,
                                                                             *   unmodified otherwise */
{
  jerry_assert_api_available ();

  ecma_property_descriptor_t prop_desc;
  jerry_value_t result = ecma_op_to_property_descriptor (obj_value, &prop_desc);

  if (ECMA_IS_VALUE_ERROR (result))
  {
    return jerry_throw (result);
  }

  JERRY_ASSERT (result == ECMA_VALUE_EMPTY);

  *out_prop_desc_p = jerry_property_descriptor_from_ecma (&prop_desc);
  return ECMA_VALUE_TRUE;
} /* jerry_to_property_descriptor */

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

#if JERRY_BUILTIN_PROMISE
  if (!ecma_is_value_object (promise) || !ecma_is_promise (ecma_get_object_from_value (promise)))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  if (ecma_is_value_error_reference (argument))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
  }

  if (is_resolve)
  {
    return ecma_fulfill_promise_with_checks (promise, argument);
  }

  return ecma_reject_promise_with_checks (promise, argument);
#else /* !JERRY_BUILTIN_PROMISE */
  JERRY_UNUSED (promise);
  JERRY_UNUSED (argument);
  JERRY_UNUSED (is_resolve);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_promise_not_supported_p)));
#endif /* JERRY_BUILTIN_PROMISE */
} /* jerry_resolve_or_reject_promise */

/**
 * Get the result of a promise.
 *
 * @return - Promise result
 *         - Type error if the promise support was not enabled or the input was not a promise object
 */
jerry_value_t
jerry_get_promise_result (const jerry_value_t promise) /**< promise object to get the result from */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_PROMISE
  if (!jerry_value_is_promise (promise))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  return ecma_promise_get_result (ecma_get_object_from_value (promise));
#else /* !JERRY_BUILTIN_PROMISE */
  JERRY_UNUSED (promise);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_promise_not_supported_p)));
#endif /* JERRY_BUILTIN_PROMISE */
} /* jerry_get_promise_result */

/**
 * Get the state of a promise object.
 *
 * @return - the state of the promise (one of the jerry_promise_state_t enum values)
 *         - JERRY_PROMISE_STATE_NONE is only returned if the input is not a promise object
 *           or the promise support was not enabled.
 */
jerry_promise_state_t
jerry_get_promise_state (const jerry_value_t promise) /**< promise object to get the state from */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_PROMISE
  if (!jerry_value_is_promise (promise))
  {
    return JERRY_PROMISE_STATE_NONE;
  }

  uint16_t flags = ecma_promise_get_flags (ecma_get_object_from_value (promise));
  flags &= (ECMA_PROMISE_IS_PENDING | ECMA_PROMISE_IS_FULFILLED);

  return (flags ? flags : JERRY_PROMISE_STATE_REJECTED);
#else /* !JERRY_BUILTIN_PROMISE */
  JERRY_UNUSED (promise);
  return JERRY_PROMISE_STATE_NONE;
#endif /* JERRY_BUILTIN_PROMISE */
} /* jerry_get_promise_state */

/**
 * Sets a callback for tracking Promise and async operations.
 *
 * Note:
 *     the previous callback is overwritten
 */
void jerry_promise_set_callback (jerry_promise_event_filter_t filters, /**< combination of event filters */
                                 jerry_promise_callback_t callback, /**< notification callback */
                                 void *user_p) /**< user pointer passed to the callback */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_PROMISE && JERRY_PROMISE_CALLBACK
  if (filters == JERRY_PROMISE_EVENT_FILTER_DISABLE || callback == NULL)
  {
    JERRY_CONTEXT (promise_callback_filters) = JERRY_PROMISE_EVENT_FILTER_DISABLE;
    return;
  }

  JERRY_CONTEXT (promise_callback_filters) = (uint32_t) filters;
  JERRY_CONTEXT (promise_callback) = callback;
  JERRY_CONTEXT (promise_callback_user_p) = user_p;
#else /* !JERRY_BUILTIN_PROMISE && !JERRY_PROMISE_CALLBACK */
  JERRY_UNUSED (filters);
  JERRY_UNUSED (callback);
  JERRY_UNUSED (user_p);
#endif /* JERRY_BUILTIN_PROMISE && JERRY_PROMISE_CALLBACK */
} /* jerry_promise_set_callback */

/**
 * Get the well-knwon symbol represented by the given `symbol` enum value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return undefined value - if invalid well-known symbol was requested
 *         well-known symbol value - otherwise
 */
jerry_value_t
jerry_get_well_known_symbol (jerry_well_known_symbol_t symbol) /**< jerry_well_known_symbol_t enum value */
{
  jerry_assert_api_available ();

#if JERRY_ESNEXT
  lit_magic_string_id_t id = (lit_magic_string_id_t) (LIT_GLOBAL_SYMBOL__FIRST + symbol);

  if (!LIT_IS_GLOBAL_SYMBOL (id))
  {
    return ECMA_VALUE_UNDEFINED;
  }

  return ecma_make_symbol_value (ecma_op_get_global_symbol (id));
#else /* !JERRY_ESNEXT */
  JERRY_UNUSED (symbol);

  return ECMA_VALUE_UNDEFINED;
#endif /* JERRY_ESNEXT */
} /** jerry_get_well_known_symbol */

/**
 * Returns the description internal property of a symbol.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return string or undefined value containing the symbol's description - if success
 *         thrown error - otherwise
 */
jerry_value_t
jerry_get_symbol_description (const jerry_value_t symbol) /**< symbol value */
{
  jerry_assert_api_available ();

#if JERRY_ESNEXT
  if (!ecma_is_value_symbol (symbol))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  /* Note: This operation cannot throw an error */
  return ecma_copy_value (ecma_get_symbol_description (ecma_get_symbol_from_value (symbol)));
#else /* !JERRY_ESNEXT */
  JERRY_UNUSED (symbol);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_symbol_not_supported_p)));
#endif /* JERRY_ESNEXT */
} /* jerry_get_symbol_description */

/**
 * Call the SymbolDescriptiveString ecma builtin operation on the symbol value.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return string value containing the symbol's descriptive string - if success
 *         thrown error - otherwise
 */
jerry_value_t
jerry_get_symbol_descriptive_string (const jerry_value_t symbol) /**< symbol value */
{
  jerry_assert_api_available ();

#if JERRY_ESNEXT
  if (!ecma_is_value_symbol (symbol))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  /* Note: This operation cannot throw an error */
  return ecma_get_symbol_descriptive_string (symbol);
#else /* !JERRY_ESNEXT */
  JERRY_UNUSED (symbol);

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_symbol_not_supported_p)));
#endif /* JERRY_ESNEXT */
} /** jerry_get_symbol_descriptive_string */

/**
 * Get the number of uint64 digits of a BigInt value
 *
 * @return number of uint64 digits
 */
uint32_t
jerry_get_bigint_size_in_digits (jerry_value_t value) /**< BigInt value */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_BIGINT
  if (!ecma_is_value_bigint (value))
  {
    return 0;
  }

  return ecma_bigint_get_size_in_digits (value);
#else /* !JERRY_BUILTIN_BIGINT */
  JERRY_UNUSED (value);
  return 0;
#endif /* JERRY_BUILTIN_BIGINT */
} /* jerry_get_bigint_size_in_digits */

/**
 * Get the uint64 digits of a BigInt value (lowest digit first)
 */
void
jerry_get_bigint_digits (jerry_value_t value, /**< BigInt value */
                         uint64_t *digits_p, /**< [out] buffer for digits */
                         uint32_t size, /**< buffer size in digits */
                         bool *sign_p) /**< [out] sign of BigInt */
{
#if JERRY_BUILTIN_BIGINT
  if (!ecma_is_value_bigint (value))
  {
    if (sign_p != NULL)
    {
      *sign_p = false;
    }
    memset (digits_p, 0, size * sizeof (uint64_t));
  }

  ecma_bigint_get_digits_and_sign (value, digits_p, size, sign_p);
#else /* !JERRY_BUILTIN_BIGINT */
  JERRY_UNUSED (value);

  if (sign_p != NULL)
  {
    *sign_p = false;
  }
  memset (digits_p, 0, size * sizeof (uint64_t));
#endif /* JERRY_BUILTIN_BIGINT */
} /* jerry_get_bigint_digits */

/**
 * Get the target object of a Proxy object
 *
 * @return type error - if proxy_value is not a Proxy object
 *         target object - otherwise
 */
jerry_value_t
jerry_get_proxy_target (jerry_value_t proxy_value) /**< proxy value */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_PROXY
  if (ecma_is_value_object (proxy_value))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (proxy_value);

    if (ECMA_OBJECT_IS_PROXY (object_p))
    {
      ecma_proxy_object_t *proxy_object_p = (ecma_proxy_object_t *) object_p;

      if (!ecma_is_value_null (proxy_object_p->target))
      {
        ecma_ref_object (ecma_get_object_from_value (proxy_object_p->target));
      }
      return proxy_object_p->target;
    }
  }
#else /* !JERRY_BUILTIN_PROXY */
  JERRY_UNUSED (proxy_value);
#endif /* JERRY_BUILTIN_PROXY */

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_argument_is_not_a_proxy)));
} /* jerry_get_proxy_target */

/**
 * Get the handler object of a Proxy object
 *
 * @return type error - if proxy_value is not a Proxy object
 *         handler object - otherwise
 */
jerry_value_t
jerry_get_proxy_handler (jerry_value_t proxy_value) /**< proxy value */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_PROXY
  if (ecma_is_value_object (proxy_value))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (proxy_value);

    if (ECMA_OBJECT_IS_PROXY (object_p))
    {
      ecma_proxy_object_t *proxy_object_p = (ecma_proxy_object_t *) object_p;

      if (!ecma_is_value_null (proxy_object_p->handler))
      {
        ecma_ref_object (ecma_get_object_from_value (proxy_object_p->handler));
      }
      return proxy_object_p->handler;
    }
  }
#else /* !JERRY_BUILTIN_PROXY */
  JERRY_UNUSED (proxy_value);
#endif /* JERRY_BUILTIN_PROXY */

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_argument_is_not_a_proxy)));
} /* jerry_get_proxy_handler */

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

#if JERRY_EXTERNAL_CONTEXT

  size_t total_size = sizeof (jerry_context_t) + JMEM_ALIGNMENT;

#if !JERRY_SYSTEM_ALLOCATOR
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

#if !JERRY_SYSTEM_ALLOCATOR
  context_p->heap_p = (jmem_heap_t *) byte_p;
  context_p->heap_size = heap_size;
  byte_p += heap_size;
#endif /* !JERRY_SYSTEM_ALLOCATOR */

  JERRY_ASSERT (byte_p <= ((uint8_t *) context_p) + total_size);

  JERRY_UNUSED (byte_p);
  return context_p;

#else /* !JERRY_EXTERNAL_CONTEXT */

  JERRY_UNUSED (alloc);
  JERRY_UNUSED (cb_data_p);

  return NULL;

#endif /* JERRY_EXTERNAL_CONTEXT */
} /* jerry_create_context */

/**
 * If JERRY_VM_EXEC_STOP is enabled the callback passed to this function is
 * periodically called with the user_p argument. If frequency is greater
 * than 1, the callback is only called at every frequency ticks.
 */
void
jerry_set_vm_exec_stop_callback (jerry_vm_exec_stop_callback_t stop_cb, /**< periodically called user function */
                                 void *user_p, /**< pointer passed to the function */
                                 uint32_t frequency) /**< frequency of the function call */
{
#if JERRY_VM_EXEC_STOP
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
 * Low-level function to capture each backtrace frame.
 * The captured frame data is passed to a callback function.
 */
void
jerry_backtrace_capture (jerry_backtrace_callback_t callback, /**< callback function */
                         void *user_p) /**< user pointer passed to the callback function */
{
  jerry_backtrace_frame_t frame;
  vm_frame_ctx_t *context_p = JERRY_CONTEXT (vm_top_context_p);

  while (context_p != NULL)
  {
    frame.context_p = context_p;
    frame.frame_type = JERRY_BACKTRACE_FRAME_JS;

    if (!callback (&frame, user_p))
    {
      return;
    }

    context_p = context_p->prev_context_p;
  }
} /* jerry_backtrace_capture */

/**
 * Returns with the type of the backtrace frame.
 *
 * @return frame type listed in jerry_backtrace_frame_types_t
 */
jerry_backtrace_frame_types_t
jerry_backtrace_get_frame_type (jerry_backtrace_frame_t *frame_p) /**< frame pointer */
{
  return (jerry_backtrace_frame_types_t) frame_p->frame_type;
} /* jerry_backtrace_get_frame_type */

/**
 * Initialize and return with the location private field of a backtrace frame.
 *
 * @return pointer to the location private field - if the location is available,
 *         NULL - otherwise
 */
const jerry_backtrace_location_t *
jerry_backtrace_get_location (jerry_backtrace_frame_t *frame_p) /**< frame pointer */
{
  JERRY_UNUSED (frame_p);

#if JERRY_LINE_INFO
  if (frame_p->frame_type == JERRY_BACKTRACE_FRAME_JS)
  {
    vm_frame_ctx_t *context_p = frame_p->context_p;

    frame_p->location.resource_name = ecma_get_resource_name (context_p->shared_p->bytecode_header_p);
    frame_p->location.line = context_p->current_line;
    frame_p->location.column = 1;
    return &frame_p->location;
  }
#endif /* JERRY_LINE_INFO */

  return NULL;
} /* jerry_backtrace_get_location */

/**
 * Initialize and return with the called function private field of a backtrace frame.
 * The backtrace frame is created for running the code bound to this function.
 *
 * @return pointer to the called function - if the function is available,
 *         NULL - otherwise
 */
const jerry_value_t *
jerry_backtrace_get_function (jerry_backtrace_frame_t *frame_p) /**< frame pointer */
{
  if (frame_p->frame_type == JERRY_BACKTRACE_FRAME_JS)
  {
    vm_frame_ctx_t *context_p = frame_p->context_p;

    if (context_p->shared_p->function_object_p != NULL)
    {
      frame_p->function = ecma_make_object_value (context_p->shared_p->function_object_p);
      return &frame_p->function;
    }
  }

  return NULL;
} /* jerry_backtrace_get_function */

/**
 * Initialize and return with the 'this' binding private field of a backtrace frame.
 * The 'this' binding is a hidden value passed to the called function. As for arrow
 * functions, the 'this' binding is assigned at function creation.
 *
 * @return pointer to the 'this' binding - if the binding is available,
 *         NULL - otherwise
 */
const jerry_value_t *
jerry_backtrace_get_this (jerry_backtrace_frame_t *frame_p) /**< frame pointer */
{
  if (frame_p->frame_type == JERRY_BACKTRACE_FRAME_JS)
  {
    frame_p->this_binding = frame_p->context_p->this_binding;
    return &frame_p->this_binding;
  }

  return NULL;
} /* jerry_backtrace_get_this */

/**
 * Returns true, if the code bound to the backtrace frame is strict mode code.
 *
 * @return true - if strict mode code is bound to the frame,
 *         false - otherwise
 */
bool
jerry_backtrace_is_strict (jerry_backtrace_frame_t *frame_p) /**< frame pointer */
{
  return (frame_p->frame_type == JERRY_BACKTRACE_FRAME_JS
          && (frame_p->context_p->status_flags & VM_FRAME_CTX_IS_STRICT) != 0);
} /* jerry_backtrace_is_strict */

/**
 * Get the resource name (usually a file name) of the currently executed script or the given function object
 *
 * Note: returned value must be freed with jerry_release_value, when it is no longer needed
 *
 * @return JS string constructed from
 *         - the currently executed function object's resource name, if the given value is undefined
 *         - resource name of the function object, if the given value is a function object
 *         - "<anonymous>", otherwise
 */
jerry_value_t
jerry_get_resource_name (const jerry_value_t value) /**< jerry api value */
{
#if JERRY_RESOURCE_NAME
  if (ecma_is_value_undefined (value))
  {
    if (JERRY_CONTEXT (vm_top_context_p) != NULL)
    {
      return ecma_copy_value (ecma_get_resource_name (JERRY_CONTEXT (vm_top_context_p)->shared_p->bytecode_header_p));
    }
  }
  else if (ecma_is_value_object (value))
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (value);

    if (ecma_get_object_type (obj_p) == ECMA_OBJECT_TYPE_FUNCTION
        && !ecma_get_object_is_builtin (obj_p))
    {
      ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) obj_p;

      const ecma_compiled_code_t *bytecode_data_p = ecma_op_function_get_compiled_code (ext_func_p);

      return ecma_copy_value (ecma_get_resource_name (bytecode_data_p));
    }
  }
#endif /* JERRY_RESOURCE_NAME */

  JERRY_UNUSED (value);
  return ecma_make_magic_string_value (LIT_MAGIC_STRING_RESOURCE_ANON);
} /* jerry_get_resource_name */

/**
 * Replaces the currently active realm with another realm.
 *
 * The replacement should be temporary, and the original realm must be
 * restored after the tasks are completed. During the replacement, the
 * realm must be referenced by the application (i.e. the gc must not
 * reclaim it). This is also true to the returned previously active
 * realm, so there is no need to free the value after the restoration.
 *
 * @return previous realm value - if the passed value is a realm
 *         exception - otherwise
 */
jerry_value_t
jerry_set_realm (jerry_value_t realm_value) /**< jerry api value */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_REALMS
  if (ecma_is_value_object (realm_value))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (realm_value);

    if (ecma_get_object_is_builtin (object_p)
        && ecma_builtin_is_global (object_p))
    {
      ecma_global_object_t *previous_global_object_p = JERRY_CONTEXT (global_object_p);
      JERRY_CONTEXT (global_object_p) = (ecma_global_object_t *) object_p;
      return ecma_make_object_value ((ecma_object_t *) previous_global_object_p);
    }
  }

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Passed argument is not a realm")));
#else /* !JERRY_BUILTIN_REALMS */
  JERRY_UNUSED (realm_value);
  return jerry_throw (ecma_raise_reference_error (ECMA_ERR_MSG ("Realm is not available")));
#endif /* JERRY_BUILTIN_REALMS */
} /* jerry_set_realm */

/**
 * Gets the 'this' binding of a realm
 *
 * @return type error - if realm_value is not a realm
 *         this value - otherwise
 */
jerry_value_t
jerry_realm_get_this (jerry_value_t realm_value) /**< realm value */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_REALMS
  if (ecma_is_value_object (realm_value))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (realm_value);

    if (ecma_get_object_is_builtin (object_p)
        && ecma_builtin_is_global (object_p))
    {
      ecma_global_object_t *global_object_p = (ecma_global_object_t *) object_p;

      ecma_ref_object (ecma_get_object_from_value (global_object_p->this_binding));
      return global_object_p->this_binding;
    }
  }

#else /* !JERRY_BUILTIN_REALMS */
  ecma_object_t *global_object_p = ecma_builtin_get_global ();

  if (realm_value == ecma_make_object_value (global_object_p))
  {
    ecma_ref_object (global_object_p);
    return realm_value;
  }
#endif /* JERRY_BUILTIN_REALMS */

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Passed argument is not a realm")));
} /* jerry_realm_get_this */

/**
 * Sets the 'this' binding of a realm
 *
 * This function must be called before executing any script on the realm.
 * Otherwise the operation is undefined.
 *
 * @return type error - if realm_value is not a realm or this_value is not object
 *         true - otherwise
 */
jerry_value_t
jerry_realm_set_this (jerry_value_t realm_value, /**< realm value */
                      jerry_value_t this_value) /**< this value */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_REALMS
  if (!ecma_is_value_object (this_value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Second argument must be an object")));
  }

  if (ecma_is_value_object (realm_value))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (realm_value);

    if (ecma_get_object_is_builtin (object_p)
        && ecma_builtin_is_global (object_p))
    {
      ecma_global_object_t *global_object_p = (ecma_global_object_t *) object_p;
      global_object_p->this_binding = this_value;

      ecma_object_t *global_lex_env_p = ecma_create_object_lex_env (NULL, ecma_get_object_from_value (this_value));

      ECMA_SET_NON_NULL_POINTER (global_object_p->global_env_cp, global_lex_env_p);
#if JERRY_ESNEXT
      global_object_p->global_scope_cp = global_object_p->global_env_cp;
#endif /* JERRY_ESNEXT */
      ecma_deref_object (global_lex_env_p);
      return ECMA_VALUE_TRUE;
    }
  }

  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("First argument is not a realm")));
#else /* !JERRY_BUILTIN_REALMS */
  JERRY_UNUSED (realm_value);
  JERRY_UNUSED (this_value);
  return jerry_throw (ecma_raise_reference_error (ECMA_ERR_MSG ("Realm is not available")));
#endif /* JERRY_BUILTIN_REALMS */
} /* jerry_realm_set_this */

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

#if JERRY_BUILTIN_TYPEDARRAY
  return ecma_is_arraybuffer (value);
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (value);
  return false;
#endif /* JERRY_BUILTIN_TYPEDARRAY */
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

#if JERRY_BUILTIN_TYPEDARRAY
  return jerry_return (ecma_make_object_value (ecma_arraybuffer_new_object (size)));
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (size);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_typed_array_not_supported_p)));
#endif /* JERRY_BUILTIN_TYPEDARRAY */
} /* jerry_create_arraybuffer */

/**
 * Creates an ArrayBuffer object with user specified buffer.
 *
 * Notes:
 *     * the size is specified in bytes.
 *     * the buffer passed should be at least the specified bytes big.
 *     * if the typed arrays are disabled this will return a TypeError.
 *     * if the size is zero or buffer_p is a null pointer this will return an empty ArrayBuffer.
 *
 * @return value of the construced ArrayBuffer object
 */
jerry_value_t
jerry_create_arraybuffer_external (const jerry_length_t size, /**< size of the buffer to used */
                                   uint8_t *buffer_p, /**< buffer to use as the ArrayBuffer's backing */
                                   jerry_value_free_callback_t free_cb) /**< buffer free callback */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_TYPEDARRAY
  ecma_object_t *arraybuffer;

  if (JERRY_UNLIKELY (size == 0 || buffer_p == NULL))
  {
    arraybuffer = ecma_arraybuffer_new_object (0);
  }
  else
  {
    arraybuffer = ecma_arraybuffer_new_object_external (size, buffer_p, free_cb);
  }

  return jerry_return (ecma_make_object_value (arraybuffer));
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (size);
  JERRY_UNUSED (buffer_p);
  JERRY_UNUSED (free_cb);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_typed_array_not_supported_p)));
#endif /* JERRY_BUILTIN_TYPEDARRAY */
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

#if JERRY_BUILTIN_TYPEDARRAY
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
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (value);
  JERRY_UNUSED (offset);
  JERRY_UNUSED (buf_p);
  JERRY_UNUSED (buf_size);
  return 0;
#endif /* JERRY_BUILTIN_TYPEDARRAY */
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

#if JERRY_BUILTIN_TYPEDARRAY
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
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (value);
  JERRY_UNUSED (offset);
  JERRY_UNUSED (buf_p);
  JERRY_UNUSED (buf_size);
  return 0;
#endif /* JERRY_BUILTIN_TYPEDARRAY */
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

#if JERRY_BUILTIN_TYPEDARRAY
  if (ecma_is_arraybuffer (value))
  {
    ecma_object_t *buffer_p = ecma_get_object_from_value (value);
    return ecma_arraybuffer_get_length (buffer_p);
  }
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (value);
#endif /* JERRY_BUILTIN_TYPEDARRAY */
  return 0;
} /* jerry_get_arraybuffer_byte_length */

/**
 * Get a pointer for the start of the ArrayBuffer.
 *
 * Note:
 *    * This is a high-risk operation as the bounds are not checked
 *      when accessing the pointer elements.
 *
 * @return pointer to the back-buffer of the ArrayBuffer.
 *         pointer is NULL if:
 *            - the parameter is not an ArrayBuffer
 *            - an external ArrayBuffer has been detached
 */
uint8_t *
jerry_get_arraybuffer_pointer (const jerry_value_t array_buffer) /**< Array Buffer to use */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_TYPEDARRAY
  if (ecma_is_value_error_reference (array_buffer)
      || !ecma_is_arraybuffer (array_buffer))
  {
    return NULL;
  }

  ecma_object_t *buffer_p = ecma_get_object_from_value (array_buffer);
  lit_utf8_byte_t *mem_buffer_p = ecma_arraybuffer_get_buffer (buffer_p);
  return (uint8_t *const) mem_buffer_p;
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (array_buffer);
#endif /* JERRY_BUILTIN_TYPEDARRAY */

  return NULL;
} /* jerry_get_arraybuffer_pointer */

/**
 * Get if the ArrayBuffer is detachable.
 *
 * @return boolean value - if success
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_is_arraybuffer_detachable (const jerry_value_t value) /**< ArrayBuffer */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_TYPEDARRAY
  if (ecma_is_arraybuffer (value))
  {
    ecma_object_t *buffer_p = ecma_get_object_from_value (value);
    return ecma_arraybuffer_is_detached (buffer_p) ? ECMA_VALUE_FALSE : ECMA_VALUE_TRUE;
  }
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (value);
#endif /* JERRY_BUILTIN_TYPEDARRAY */
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Expected an ArrayBuffer")));
} /* jerry_is_arraybuffer_detachable */

/**
 * Detach the underlying data block from ArrayBuffer and set its bytelength to 0.
 *
 * Note: If the ArrayBuffer has been created with `jerry_create_arraybuffer_external`
 *       the optional free callback is called on a successful detach operation
 *
 * @return null value - if success
 *         value marked with error flag - otherwise
 */
jerry_value_t
jerry_detach_arraybuffer (const jerry_value_t value) /**< ArrayBuffer */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_TYPEDARRAY
  if (ecma_is_arraybuffer (value))
  {
    ecma_object_t *buffer_p = ecma_get_object_from_value (value);
    if (ecma_arraybuffer_detach (buffer_p))
    {
      return ECMA_VALUE_NULL;
    }
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("ArrayBuffer has already been detached")));
  }
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (value);
#endif /* JERRY_BUILTIN_TYPEDARRAY */
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Expected an ArrayBuffer")));
} /* jerry_detach_arraybuffer */

/**
 * DataView related functions
 */

/**
 * Creates a DataView object with the given ArrayBuffer, ByteOffset and ByteLength arguments.
 *
 * Notes:
 *      * returned value must be freed with jerry_release_value, when it is no longer needed.
 *      * if the DataView bulitin is disabled this will return a TypeError.
 *
 * @return value of the constructed DataView object - if success
 *         created error - otherwise
 */
jerry_value_t
jerry_create_dataview (const jerry_value_t array_buffer, /**< arraybuffer to create DataView from */
                       const jerry_length_t byte_offset, /**< offset in bytes, to the first byte in the buffer */
                       const jerry_length_t byte_length) /**< number of elements in the byte array */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_DATAVIEW
  if (ecma_is_value_error_reference (array_buffer))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_value_t arguments_p[3] =
  {
    array_buffer,
    ecma_make_uint32_value (byte_offset),
    ecma_make_uint32_value (byte_length)
  };
  ecma_object_t *old_new_target_p = JERRY_CONTEXT (current_new_target_p);
  if (old_new_target_p == NULL)
  {
    JERRY_CONTEXT (current_new_target_p) = ecma_builtin_get (ECMA_BUILTIN_ID_DATAVIEW);
  }

  ecma_value_t dataview_value = ecma_op_dataview_create (arguments_p, 3);
  JERRY_CONTEXT (current_new_target_p) = old_new_target_p;
  return jerry_return (dataview_value);
#else /* !JERRY_BUILTIN_DATAVIEW */
  JERRY_UNUSED (array_buffer);
  JERRY_UNUSED (byte_offset);
  JERRY_UNUSED (byte_length);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_data_view_not_supported_p)));
#endif /* JERRY_BUILTIN_DATAVIEW */
} /* jerry_create_dataview */

/**
 * Check if the given value is a DataView object.
 *
 * @return true - if it is a DataView object
 *         false - otherwise
 */
bool
jerry_value_is_dataview (const jerry_value_t value) /**< value to check if it is a DataView object */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_DATAVIEW
  return ecma_is_dataview (value);
#else /* !JERRY_BUILTIN_DATAVIEW */
  JERRY_UNUSED (value);
  return false;
#endif /* JERRY_BUILTIN_DATAVIEW */
} /* jerry_value_is_dataview */

/**
 * Get the underlying ArrayBuffer from a DataView.
 *
 * Additionally the byteLength and byteOffset properties are also returned
 * which were specified when the DataView was created.
 *
 * Note:
 *     the returned value must be freed with a jerry_release_value call
 *
 * @return ArrayBuffer of a DataView
 *         TypeError if the object is not a DataView.
 */
jerry_value_t
jerry_get_dataview_buffer (const jerry_value_t value, /**< DataView to get the arraybuffer from */
                           jerry_length_t *byte_offset, /**< [out] byteOffset property */
                           jerry_length_t *byte_length) /**< [out] byteLength property */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_DATAVIEW
  if (ecma_is_value_error_reference (value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_wrong_args_msg_p)));
  }

  ecma_dataview_object_t *dataview_p = ecma_op_dataview_get_object (value);

  if (JERRY_UNLIKELY (dataview_p == NULL))
  {
    return ecma_create_error_reference_from_context ();
  }

  if (byte_offset != NULL)
  {
    *byte_offset = dataview_p->byte_offset;
  }

  if (byte_length != NULL)
  {
    *byte_length = dataview_p->header.u.cls.u3.length;
  }

  ecma_object_t *arraybuffer_p = dataview_p->buffer_p;
  ecma_ref_object (arraybuffer_p);

  return ecma_make_object_value (arraybuffer_p);
#else /* !JERRY_BUILTIN_DATAVIEW */
  JERRY_UNUSED (value);
  JERRY_UNUSED (byte_offset);
  JERRY_UNUSED (byte_length);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_data_view_not_supported_p)));
#endif /* JERRY_BUILTIN_DATAVIEW */
} /* jerry_get_dataview_buffer */

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

#if JERRY_BUILTIN_TYPEDARRAY
  return ecma_is_typedarray (value);
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (value);
  return false;
#endif /* JERRY_BUILTIN_TYPEDARRAY */
} /* jerry_value_is_typedarray */

#if JERRY_BUILTIN_TYPEDARRAY
/**
 * TypedArray mapping type
 */
typedef struct
{
  jerry_typedarray_type_t api_type; /**< api type */
  ecma_builtin_id_t prototype_id; /**< prototype ID */
  ecma_typedarray_type_t id; /**< typedArray ID */
  uint8_t element_size_shift; /**< element size shift */
} jerry_typedarray_mapping_t;

/**
 * List of TypedArray mappings
 */
static jerry_typedarray_mapping_t jerry_typedarray_mappings[] =
{
#define TYPEDARRAY_ENTRY(NAME, LIT_NAME, SIZE_SHIFT) \
  { JERRY_TYPEDARRAY_ ## NAME, ECMA_BUILTIN_ID_ ## NAME ## ARRAY_PROTOTYPE, \
    ECMA_ ## LIT_NAME ## _ARRAY, SIZE_SHIFT }

  TYPEDARRAY_ENTRY (UINT8, UINT8, 0),
  TYPEDARRAY_ENTRY (UINT8CLAMPED, UINT8_CLAMPED, 0),
  TYPEDARRAY_ENTRY (INT8, INT8, 0),
  TYPEDARRAY_ENTRY (UINT16, UINT16, 1),
  TYPEDARRAY_ENTRY (INT16, INT16, 1),
  TYPEDARRAY_ENTRY (UINT32, UINT32, 2),
  TYPEDARRAY_ENTRY (INT32, INT32, 2),
  TYPEDARRAY_ENTRY (FLOAT32, FLOAT32, 2),
#if JERRY_NUMBER_TYPE_FLOAT64
  TYPEDARRAY_ENTRY (FLOAT64, FLOAT64, 3),
#endif /* JERRY_NUMBER_TYPE_FLOAT64 */
#if JERRY_BUILTIN_BIGINT
  TYPEDARRAY_ENTRY (BIGINT64, BIGINT64, 3),
  TYPEDARRAY_ENTRY (BIGUINT64, BIGUINT64, 3),
#endif /* JERRY_BUILTIN_BIGINT */
#undef TYPEDARRAY_ENTRY
};

/**
 * Helper function to get the TypedArray prototype, typedArray id, and element size shift
 * information.
 *
 * @return true - if the TypedArray information was found
 *         false - if there is no such TypedArray type
 */
static bool
jerry_typedarray_find_by_type (jerry_typedarray_type_t type_name, /**< type of the TypedArray */
                               ecma_builtin_id_t *prototype_id, /**< [out] found prototype object id */
                               ecma_typedarray_type_t *id, /**< [out] found typedArray id */
                               uint8_t *element_size_shift) /**< [out] found element size shift value */
{
  JERRY_ASSERT (prototype_id != NULL);
  JERRY_ASSERT (id != NULL);
  JERRY_ASSERT (element_size_shift != NULL);

  for (uint32_t i = 0; i < sizeof (jerry_typedarray_mappings) / sizeof (jerry_typedarray_mappings[0]); i++)
  {
    if (type_name == jerry_typedarray_mappings[i].api_type)
    {
      *prototype_id = jerry_typedarray_mappings[i].prototype_id;
      *id = jerry_typedarray_mappings[i].id;
      *element_size_shift = jerry_typedarray_mappings[i].element_size_shift;
      return true;
    }
  }

  return false;
} /* jerry_typedarray_find_by_type */

#endif /* JERRY_BUILTIN_TYPEDARRAY */

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

#if JERRY_BUILTIN_TYPEDARRAY
  ecma_builtin_id_t prototype_id = 0;
  ecma_typedarray_type_t id = 0;
  uint8_t element_size_shift = 0;

  if (!jerry_typedarray_find_by_type (type_name, &prototype_id, &id, &element_size_shift))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Incorrect type for TypedArray")));
  }

  ecma_object_t *prototype_obj_p = ecma_builtin_get (prototype_id);

  ecma_value_t array_value = ecma_typedarray_create_object_with_length (length,
                                                                        NULL,
                                                                        prototype_obj_p,
                                                                        element_size_shift,
                                                                        id);

  JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (array_value));

  return array_value;
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (type_name);
  JERRY_UNUSED (length);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_typed_array_not_supported_p)));
#endif /* JERRY_BUILTIN_TYPEDARRAY */
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

#if JERRY_BUILTIN_TYPEDARRAY
  if (ecma_is_value_error_reference (arraybuffer))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
  }

  ecma_builtin_id_t prototype_id = 0;
  ecma_typedarray_type_t id = 0;
  uint8_t element_size_shift = 0;

  if (!jerry_typedarray_find_by_type (type_name, &prototype_id, &id, &element_size_shift))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Incorrect type for TypedArray")));
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

  ecma_value_t array_value = ecma_op_create_typedarray (arguments_p, 3, prototype_obj_p, element_size_shift, id);
  ecma_free_value (arguments_p[1]);
  ecma_free_value (arguments_p[2]);

  return jerry_return (array_value);
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (type_name);
  JERRY_UNUSED (arraybuffer);
  JERRY_UNUSED (byte_offset);
  JERRY_UNUSED (length);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_typed_array_not_supported_p)));
#endif /* JERRY_BUILTIN_TYPEDARRAY */
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

#if JERRY_BUILTIN_TYPEDARRAY
  if (ecma_is_value_error_reference (arraybuffer))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
  }

  jerry_length_t byteLength = jerry_get_arraybuffer_byte_length (arraybuffer);
  return jerry_create_typedarray_for_arraybuffer_sz (type_name, arraybuffer, 0, byteLength);
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (type_name);
  JERRY_UNUSED (arraybuffer);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_typed_array_not_supported_p)));
#endif /* JERRY_BUILTIN_TYPEDARRAY */
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

#if JERRY_BUILTIN_TYPEDARRAY
  if (!ecma_is_typedarray (value))
  {
    return JERRY_TYPEDARRAY_INVALID;
  }

  ecma_object_t *array_p = ecma_get_object_from_value (value);
  ecma_typedarray_type_t class_type = ecma_get_typedarray_id (array_p);

  for (uint32_t i = 0; i < sizeof (jerry_typedarray_mappings) / sizeof (jerry_typedarray_mappings[0]); i++)
  {
    if (class_type == jerry_typedarray_mappings[i].id)
    {
      return jerry_typedarray_mappings[i].api_type;
    }
  }
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (value);
#endif /* JERRY_BUILTIN_TYPEDARRAY */

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

#if JERRY_BUILTIN_TYPEDARRAY
  if (ecma_is_typedarray (value))
  {
    ecma_object_t *array_p = ecma_get_object_from_value (value);
    return ecma_typedarray_get_length (array_p);
  }
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (value);
#endif /* JERRY_BUILTIN_TYPEDARRAY */

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

#if JERRY_BUILTIN_TYPEDARRAY
  if (!ecma_is_typedarray (value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Object is not a TypedArray")));
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
#else /* !JERRY_BUILTIN_TYPEDARRAY */
  JERRY_UNUSED (value);
  JERRY_UNUSED (byte_length);
  JERRY_UNUSED (byte_offset);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_typed_array_not_supported_p)));
#endif /* JERRY_BUILTIN_TYPEDARRAY */
} /* jerry_get_typedarray_buffer */

/**
 * Parse the given JSON string to create a jerry_value_t.
 *
 * The behaviour is equivalent with the "JSON.parse(string)" JS call.
 *
 * Note:
 *      The returned value must be freed with jerry_release_value.
 *
 * @return - jerry_value_t containing a JavaScript value.
 *         - Error value if there was problems during the parse.
 */
jerry_value_t
jerry_json_parse (const jerry_char_t *string_p, /**< json string */
                  jerry_size_t string_size) /**< json string size */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_JSON
  ecma_value_t ret_value = ecma_builtin_json_parse_buffer (string_p, string_size);

  if (ecma_is_value_undefined (ret_value))
  {
    ret_value = jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG ("JSON string parse error")));
  }

  return jerry_return (ret_value);
#else /* !JERRY_BUILTIN_JSON */
  JERRY_UNUSED (string_p);
  JERRY_UNUSED (string_size);

  return jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG (ecma_error_json_not_supported_p)));
#endif /* JERRY_BUILTIN_JSON */
} /* jerry_json_parse */

/**
 * Create a JSON string from a JavaScript value.
 *
 * The behaviour is equivalent with the "JSON.stringify(input_value)" JS call.
 *
 * Note:
 *      The returned value must be freed with jerry_release_value,
 *
 * @return - jerry_value_t containing a JSON string.
 *         - Error value if there was a problem during the stringification.
 */
jerry_value_t
jerry_json_stringify (const jerry_value_t input_value) /**< a value to stringify */
{
  jerry_assert_api_available ();
#if JERRY_BUILTIN_JSON
  if (ecma_is_value_error_reference (input_value))
  {
    return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
  }

  ecma_value_t ret_value = ecma_builtin_json_stringify_no_opts (input_value);

  if (ecma_is_value_undefined (ret_value))
  {
    ret_value = jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG ("JSON stringify error")));
  }

  return jerry_return (ret_value);
#else /* JERRY_BUILTIN_JSON */
  JERRY_UNUSED (input_value);

  return jerry_throw (ecma_raise_syntax_error (ECMA_ERR_MSG (ecma_error_json_not_supported_p)));
#endif /* JERRY_BUILTIN_JSON */
} /* jerry_json_stringify */

/**
 * Create a container type specified in jerry_container_type_t.
 * The container can be created with a list of arguments, which will be passed to the container constructor to be
 * inserted to the container.
 *
 * Note:
 *      The returned value must be freed with jerry_release_value
 * @return jerry_value_t representing a container with the given type.
 */
jerry_value_t
jerry_create_container (jerry_container_type_t container_type, /**< Type of the container */
                        const jerry_value_t *arguments_list_p, /**< arguments list */
                        jerry_length_t arguments_list_len) /**< Length of arguments list */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_CONTAINER
  for (jerry_length_t i = 0; i < arguments_list_len; i++)
  {
    if (ecma_is_value_error_reference (arguments_list_p[i]))
    {
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_value_msg_p)));
    }
  }

  lit_magic_string_id_t lit_id;
  ecma_builtin_id_t proto_id;
  ecma_builtin_id_t ctor_id;

  switch (container_type)
  {
    case JERRY_CONTAINER_TYPE_MAP:
    {
      lit_id = LIT_MAGIC_STRING_MAP_UL;
      proto_id = ECMA_BUILTIN_ID_MAP_PROTOTYPE;
      ctor_id = ECMA_BUILTIN_ID_MAP;
      break;
    }
    case JERRY_CONTAINER_TYPE_SET:
    {
      lit_id = LIT_MAGIC_STRING_SET_UL;
      proto_id = ECMA_BUILTIN_ID_SET_PROTOTYPE;
      ctor_id = ECMA_BUILTIN_ID_SET;
      break;
    }
    case JERRY_CONTAINER_TYPE_WEAKMAP:
    {
      lit_id = LIT_MAGIC_STRING_WEAKMAP_UL;
      proto_id = ECMA_BUILTIN_ID_WEAKMAP_PROTOTYPE;
      ctor_id = ECMA_BUILTIN_ID_WEAKMAP;
      break;
    }
    case JERRY_CONTAINER_TYPE_WEAKSET:
    {
      lit_id = LIT_MAGIC_STRING_WEAKSET_UL;
      proto_id = ECMA_BUILTIN_ID_WEAKSET_PROTOTYPE;
      ctor_id = ECMA_BUILTIN_ID_WEAKSET;
      break;
    }
    default:
    {
      return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG ("Invalid container type")));
    }
  }
  ecma_object_t * old_new_target_p = JERRY_CONTEXT (current_new_target_p);

  if (old_new_target_p == NULL)
  {
    JERRY_CONTEXT (current_new_target_p) = ecma_builtin_get (ctor_id);
  }

  ecma_value_t container_value = ecma_op_container_create (arguments_list_p,
                                                           arguments_list_len,
                                                           lit_id,
                                                           proto_id);

  JERRY_CONTEXT (current_new_target_p) = old_new_target_p;
  return container_value;
#else /* !JERRY_BUILTIN_CONTAINER */
  JERRY_UNUSED (arguments_list_p);
  JERRY_UNUSED (arguments_list_len);
  JERRY_UNUSED (container_type);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_container_not_supported_p)));
#endif /* JERRY_BUILTIN_CONTAINER */
} /* jerry_create_container */

/**
 * Get the type of the given container object.
 *
 * @return Corresponding type to the given container object.
 */
jerry_container_type_t
jerry_get_container_type (const jerry_value_t value) /**< the container object */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_CONTAINER
  if (ecma_is_value_object (value))
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (value);

    if (ecma_object_class_is (obj_p, ECMA_OBJECT_CLASS_CONTAINER))
    {
      switch (((ecma_extended_object_t *) obj_p)->u.cls.u2.container_id)
      {
        case LIT_MAGIC_STRING_MAP_UL:
        {
          return JERRY_CONTAINER_TYPE_MAP;
        }
        case LIT_MAGIC_STRING_SET_UL:
        {
          return JERRY_CONTAINER_TYPE_SET;
        }
        case LIT_MAGIC_STRING_WEAKMAP_UL:
        {
          return JERRY_CONTAINER_TYPE_WEAKMAP;
        }
        case LIT_MAGIC_STRING_WEAKSET_UL:
        {
          return JERRY_CONTAINER_TYPE_WEAKSET;
        }
        default:
        {
          return JERRY_CONTAINER_TYPE_INVALID;
        }
      }
    }
  }

#else /* !JERRY_BUILTIN_CONTAINER */
  JERRY_UNUSED (value);
#endif /* JERRY_BUILTIN_CONTAINER */
  return JERRY_CONTAINER_TYPE_INVALID;
} /* jerry_get_container_type */

/**
 * Return a new array containing elements from a Container or a Container Iterator.
 * Sets the boolean input value to `true` if the container object has key/value pairs.
 *
 * Note:
 *     the returned value must be freed with a jerry_release_value call
 *
 * @return an array of items for maps/sets or their iterators, error otherwise
 */
jerry_value_t
jerry_get_array_from_container (jerry_value_t value, /**< the container or iterator object */
                                bool *is_key_value_p) /**< [out] is key-value structure */
{
  jerry_assert_api_available ();

#if JERRY_BUILTIN_CONTAINER
  const char *container_needed = ECMA_ERR_MSG ("Value is not a Container or Iterator");

  if (!ecma_is_value_object (value))
  {
    return jerry_throw (ecma_raise_type_error (container_needed));
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (value);

  if (ecma_get_object_type (obj_p) != ECMA_OBJECT_TYPE_CLASS)
  {
    return jerry_throw (ecma_raise_type_error (container_needed));
  }

  ecma_extended_object_t *ext_obj_p = (ecma_extended_object_t *) obj_p;

  uint32_t entry_count;
  uint8_t entry_size;

  uint32_t index = 0;
  uint8_t iterator_kind = ECMA_ITERATOR__COUNT;
  ecma_value_t *start_p;

  *is_key_value_p = false;

  if (ext_obj_p->u.cls.type == ECMA_OBJECT_CLASS_MAP_ITERATOR
      || ext_obj_p->u.cls.type == ECMA_OBJECT_CLASS_SET_ITERATOR)
  {
    ecma_value_t iterated_value = ext_obj_p->u.cls.u3.iterated_value;

    if (ecma_is_value_empty (iterated_value))
    {
      return ecma_op_new_array_object_from_collection (ecma_new_collection (), false);
    }

    ecma_extended_object_t *map_object_p = (ecma_extended_object_t *) (ecma_get_object_from_value (iterated_value));

    ecma_collection_t *container_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_collection_t, map_object_p->u.cls.u3.value);
    entry_count = ECMA_CONTAINER_ENTRY_COUNT (container_p);
    index = ext_obj_p->u.cls.u2.iterator_index;

    entry_size = ecma_op_container_entry_size (map_object_p->u.cls.u2.container_id);
    start_p = ECMA_CONTAINER_START (container_p);

    iterator_kind = ext_obj_p->u.cls.u1.iterator_kind;
  }
  else if (jerry_get_container_type (value) != JERRY_CONTAINER_TYPE_INVALID)
  {
    ecma_collection_t *container_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_collection_t, ext_obj_p->u.cls.u3.value);
    entry_count = ECMA_CONTAINER_ENTRY_COUNT (container_p);
    entry_size = ecma_op_container_entry_size (ext_obj_p->u.cls.u2.container_id);

    index = 0;
    iterator_kind = ECMA_ITERATOR_KEYS;
    start_p = ECMA_CONTAINER_START (container_p);

    if (ext_obj_p->u.cls.u2.container_id == LIT_MAGIC_STRING_MAP_UL
        || ext_obj_p->u.cls.u2.container_id == LIT_MAGIC_STRING_WEAKMAP_UL)
    {
      iterator_kind = ECMA_ITERATOR_ENTRIES;
    }
  }
  else
  {
    return jerry_throw (ecma_raise_type_error (container_needed));
  }

  *is_key_value_p = (iterator_kind == ECMA_ITERATOR_ENTRIES);
  ecma_collection_t *collection_buffer = ecma_new_collection ();

  for (uint32_t i = index; i < entry_count; i += entry_size)
  {
    ecma_value_t *entry_p = start_p + i;

    if (ecma_is_value_empty (*entry_p))
    {
      continue;
    }

    if (iterator_kind != ECMA_ITERATOR_VALUES)
    {
      ecma_collection_push_back (collection_buffer, ecma_copy_value_if_not_object (entry_p[0]));
    }

    if (iterator_kind != ECMA_ITERATOR_KEYS)
    {
      ecma_collection_push_back (collection_buffer, ecma_copy_value_if_not_object (entry_p[1]));
    }
  }
  return ecma_op_new_array_object_from_collection (collection_buffer, false);
#else /* JERRY_BUILTIN_CONTAINER */
  JERRY_UNUSED (value);
  JERRY_UNUSED (is_key_value_p);
  return jerry_throw (ecma_raise_type_error (ECMA_ERR_MSG (ecma_error_container_not_supported_p)));
#endif
} /* jerry_get_array_from_container */

/**
 * @}
 */

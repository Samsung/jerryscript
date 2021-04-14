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

#include "ecma-alloc.h"
#include "ecma-builtin-helpers.h"
#include "ecma-builtin-handlers.h"
#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-helpers.h"
#include "ecma-promise-object.h"
#include "lit-char-helpers.h"
#include "ecma-lex-env.h"
#include "ecma-objects.h"
#include "ecma-objects-general.h"
#include "ecma-proxy-object.h"
#include "ecma-symbol-object.h"
#include "jcontext.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmafunctionobject ECMA Function object related routines
 * @{
 */

#if JERRY_ESNEXT
/**
 * SetFunctionName operation
 *
 * See also: ECMAScript v6, 9.2.1.1
 *
 * @return resource name as ecma-string
 */
ecma_value_t
ecma_op_function_form_name (ecma_string_t *prop_name_p, /**< property name */
                            char *prefix_p, /**< prefix */
                            lit_utf8_size_t prefix_size) /**< prefix length */
{
  /* 4. */
  if (ecma_prop_name_is_symbol (prop_name_p))
  {
    /* .a */
    ecma_value_t string_desc = ecma_get_symbol_description (prop_name_p);

    /* .b */
    if (ecma_is_value_undefined (string_desc))
    {
      prop_name_p = ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY);
    }
    /* .c */
    else
    {
      ecma_string_t *string_desc_p = ecma_get_string_from_value (string_desc);
      ecma_stringbuilder_t builder = ecma_stringbuilder_create_raw ((lit_utf8_byte_t *) "[", 1);
      ecma_stringbuilder_append (&builder, string_desc_p);
      ecma_stringbuilder_append_byte (&builder, (lit_utf8_byte_t) LIT_CHAR_RIGHT_SQUARE);
      prop_name_p = ecma_stringbuilder_finalize (&builder);
    }
  }
  else
  {
    ecma_ref_ecma_string (prop_name_p);
  }

  /* 5. */
  if (JERRY_UNLIKELY (prefix_p != NULL))
  {
    ecma_stringbuilder_t builder = ecma_stringbuilder_create_raw ((lit_utf8_byte_t *) prefix_p, prefix_size);
    ecma_stringbuilder_append (&builder, prop_name_p);
    ecma_deref_ecma_string (prop_name_p);
    prop_name_p = ecma_stringbuilder_finalize (&builder);
  }

  return ecma_make_string_value (prop_name_p);
} /* ecma_op_function_form_name */
#endif /* JERRY_ESNEXT */

/**
 * IsCallable operation.
 *
 * See also: ECMA-262 v5, 9.11
 *
 * @return true - if the given object is callable;
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_op_object_is_callable (ecma_object_t *obj_p) /**< ecma object */
{
  JERRY_ASSERT (!ecma_is_lexical_environment (obj_p));

  const ecma_object_type_t type = ecma_get_object_type (obj_p);

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_TYPE_IS_PROXY (type))
  {
    return (obj_p->u2.prototype_cp & ECMA_PROXY_IS_CALLABLE) != 0;
  }
#endif /* JERRY_BUILTIN_PROXY */

  return type >= ECMA_OBJECT_TYPE_FUNCTION;
} /* ecma_op_object_is_callable */

/**
 * IsCallable operation.
 *
 * See also: ECMA-262 v5, 9.11
 *
 * @return true - if value is callable object;
 *         false - otherwise
 */
bool
ecma_op_is_callable (ecma_value_t value) /**< ecma value */
{
  return (ecma_is_value_object (value)
          && ecma_op_object_is_callable (ecma_get_object_from_value (value)));
} /* ecma_op_is_callable */

/**
 * Implement IsConstructor abstract operation.
 *
 *
 * @return ECMA_IS_VALID_CONSTRUCTOR - if object is a valid for constructor call
 *         any other value - if object is not a valid constructor, the pointer contains the error message.
 */
char *
ecma_object_check_constructor (ecma_object_t *obj_p) /**< ecma object */
{
  JERRY_ASSERT (!ecma_is_lexical_environment (obj_p));

  ecma_object_type_t type = ecma_get_object_type (obj_p);

  if (JERRY_UNLIKELY (type < ECMA_OBJECT_TYPE_PROXY))
  {
    return ECMA_ERR_MSG ("Invalid type for constructor call");
  }

  while (JERRY_UNLIKELY (type == ECMA_OBJECT_TYPE_BOUND_FUNCTION))
  {
    ecma_bound_function_t *bound_func_p = (ecma_bound_function_t *) obj_p;

    obj_p = ECMA_GET_NON_NULL_POINTER_FROM_POINTER_TAG (ecma_object_t,
                                                        bound_func_p->header.u.bound_function.target_function);

    type = ecma_get_object_type (obj_p);
  }

  if (JERRY_LIKELY (type == ECMA_OBJECT_TYPE_FUNCTION))
  {
    JERRY_ASSERT (!ecma_get_object_is_builtin (obj_p));

#if JERRY_ESNEXT
    const ecma_compiled_code_t *byte_code_p = ecma_op_function_get_compiled_code ((ecma_extended_object_t *) obj_p);

    if (!CBC_FUNCTION_IS_CONSTRUCTABLE (byte_code_p->status_flags))
    {
#if JERRY_ERROR_MESSAGES
      switch (CBC_FUNCTION_GET_TYPE (byte_code_p->status_flags))
      {
        case CBC_FUNCTION_SCRIPT:
        {
          return "Script (global) functions cannot be invoked with 'new'";
        }
        case CBC_FUNCTION_GENERATOR:
        {
          return "Generator functions cannot be invoked with 'new'";
        }
        case CBC_FUNCTION_ASYNC:
        {
          return "Async functions cannot be invoked with 'new'";
        }
        case CBC_FUNCTION_ASYNC_GENERATOR:
        {
          return "Async generator functions cannot be invoked with 'new'";
        }
        case CBC_FUNCTION_ACCESSOR:
        {
          return "Accessor functions cannot be invoked with 'new'";
        }
        case CBC_FUNCTION_METHOD:
        {
          return "Methods cannot be invoked with 'new'";
        }
        case CBC_FUNCTION_ARROW:
        {
          return "Arrow functions cannot be invoked with 'new'";
        }
        default:
        {
          JERRY_ASSERT (CBC_FUNCTION_GET_TYPE (byte_code_p->status_flags) == CBC_FUNCTION_ASYNC_ARROW);
          return "Async arrow functions cannot be invoked with 'new'";
        }
      }
#else /* !JERRY_ERROR_MESSAGES */
      return NULL;
#endif /* JERRY_ERROR_MESSAGES */
    }
#endif /* JERRY_NEXT */

    return ECMA_IS_VALID_CONSTRUCTOR;
  }

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_TYPE_IS_PROXY (type))
  {
    if (!(obj_p->u2.prototype_cp & ECMA_PROXY_IS_CONSTRUCTABLE))
    {
      return ECMA_ERR_MSG ("Proxy target is not a constructor");
    }

    return ECMA_IS_VALID_CONSTRUCTOR;
  }
#endif /* JERRY_BUILTIN_PROXY */

  JERRY_ASSERT (type == ECMA_OBJECT_TYPE_NATIVE_FUNCTION);

  if (ecma_get_object_is_builtin (obj_p))
  {
    if (ecma_builtin_function_is_routine (obj_p))
    {
      return ECMA_ERR_MSG ("Built-in routines have no constructor");
    }

#if JERRY_ESNEXT
    JERRY_ASSERT (((ecma_extended_object_t *) obj_p)->u.built_in.id != ECMA_BUILTIN_ID_HANDLER);
#endif /* !JERRY_ESNEXT */
  }

  return ECMA_IS_VALID_CONSTRUCTOR;
} /* ecma_object_check_constructor */

/**
 * Implement IsConstructor abstract operation.
 *
 * @return ECMA_IS_VALID_CONSTRUCTOR - if the input value is a constructor.
 *         any other value - if the input value is not a valid constructor, the pointer contains the error message.
 */
extern inline char *JERRY_ATTR_ALWAYS_INLINE
ecma_check_constructor (ecma_value_t value) /**< ecma object */
{
  if (!ecma_is_value_object (value))
  {
    return ECMA_ERR_MSG ("Invalid type for constructor call");
  }

  return ecma_object_check_constructor (ecma_get_object_from_value (value));
} /* ecma_check_constructor */

/**
 * Checks whether the given object implements [[Construct]].
 *
 * @return true - if the given object is constructor;
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_object_is_constructor (ecma_object_t *obj_p) /**< ecma object */
{
  return ecma_object_check_constructor (obj_p) == ECMA_IS_VALID_CONSTRUCTOR;
} /* ecma_object_is_constructor */

/**
 * Checks whether the value is Object that implements [[Construct]].
 *
 * @return true - if value is constructor object;
 *         false - otherwise
 */
bool
ecma_is_constructor (ecma_value_t value) /**< ecma value */
{
  return (ecma_is_value_object (value)
          && ecma_object_is_constructor (ecma_get_object_from_value (value)));
} /* ecma_is_constructor */

/**
 * Helper method to count and convert the arguments for the Function/GeneratorFunction constructor call.
 *
 * See also:
 *          ECMA 262 v5.1 15.3.2.1 steps 5.a-d
 *          ECMA 262 v6 19.2.1.1.1 steps 8
 *
 * @return ecma value - concatenated arguments as a string.
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_string_t *
ecma_op_create_dynamic_function_arguments_helper (const ecma_value_t *arguments_list_p, /**< arguments list */
                                                  uint32_t arguments_list_len) /**< number of arguments */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  if (arguments_list_len <= 1)
  {
    return ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY);
  }

  ecma_string_t *str_p = ecma_op_to_string (arguments_list_p[0]);

  if (JERRY_UNLIKELY (str_p == NULL))
  {
    return str_p;
  }

  if (arguments_list_len == 2)
  {
    return str_p;
  }

  ecma_stringbuilder_t builder = ecma_stringbuilder_create_from (str_p);
  ecma_deref_ecma_string (str_p);

  for (uint32_t idx = 1; idx < arguments_list_len - 1; idx++)
  {
    str_p = ecma_op_to_string (arguments_list_p[idx]);

    if (JERRY_UNLIKELY (str_p == NULL))
    {
      ecma_stringbuilder_destroy (&builder);
      return str_p;
    }

    ecma_stringbuilder_append_char (&builder, LIT_CHAR_COMMA);
    ecma_stringbuilder_append (&builder, str_p);
    ecma_deref_ecma_string (str_p);
  }

  return ecma_stringbuilder_finalize (&builder);
} /* ecma_op_create_dynamic_function_arguments_helper */

/**
 * Function object creation operation.
 *
 * See also: ECMA-262 v5, 13.2
 *
 * @return pointer to newly created Function object
 */
static ecma_object_t *
ecma_op_create_function_object (ecma_object_t *scope_p, /**< function's scope */
                                const ecma_compiled_code_t *bytecode_data_p, /**< byte-code array */
                                ecma_builtin_id_t proto_id) /**< builtin id of the prototype object */
{
  JERRY_ASSERT (ecma_is_lexical_environment (scope_p));

  /* 1., 4., 13. */
  ecma_object_t *prototype_obj_p = ecma_builtin_get (proto_id);

  size_t function_object_size = sizeof (ecma_extended_object_t);

#if JERRY_SNAPSHOT_EXEC
  if (bytecode_data_p->status_flags & CBC_CODE_FLAGS_STATIC_FUNCTION)
  {
    function_object_size = sizeof (ecma_static_function_t);
  }
#endif /* JERRY_SNAPSHOT_EXEC */

  ecma_object_t *func_p = ecma_create_object (prototype_obj_p,
                                              function_object_size,
                                              ECMA_OBJECT_TYPE_FUNCTION);

  /* 2., 6., 7., 8. */
  /*
   * We don't setup [[Get]], [[Call]], [[Construct]], [[HasInstance]] for each function object.
   * Instead we set the object's type to ECMA_OBJECT_TYPE_FUNCTION
   * that defines which version of the routine should be used on demand.
   */

  /* 3. */
  /*
   * [[Class]] property is not stored explicitly for objects of ECMA_OBJECT_TYPE_FUNCTION type.
   *
   * See also: ecma_object_get_class_name
   */

  ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) func_p;

  /* 9. */
  ECMA_SET_NON_NULL_POINTER_TAG (ext_func_p->u.function.scope_cp, scope_p, 0);

  /* 10., 11., 12. */

#if JERRY_SNAPSHOT_EXEC
  if (bytecode_data_p->status_flags & CBC_CODE_FLAGS_STATIC_FUNCTION)
  {
    ext_func_p->u.function.bytecode_cp = JMEM_CP_NULL;
    ((ecma_static_function_t *) func_p)->bytecode_p = bytecode_data_p;
  }
  else
#endif /* JERRY_SNAPSHOT_EXEC */
  {
    ECMA_SET_INTERNAL_VALUE_POINTER (ext_func_p->u.function.bytecode_cp, bytecode_data_p);
    ecma_bytecode_ref ((ecma_compiled_code_t *) bytecode_data_p);
  }

  /* 14., 15., 16., 17., 18. */
  /*
   * 'length' and 'prototype' properties are instantiated lazily
   *
   * See also: ecma_op_function_try_to_lazy_instantiate_property
   */

  return func_p;
} /* ecma_op_create_function_object */

/**
 * CreateDynamicFunction operation
 *
 * See also:
 *          ECMA-262 v5, 15.3.
 *          ECMA-262 v6, 19.2.1.1
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         constructed function object - otherwise
 */
ecma_value_t
ecma_op_create_dynamic_function (const ecma_value_t *arguments_list_p, /**< arguments list */
                                 uint32_t arguments_list_len, /**< number of arguments */
                                 ecma_parse_opts_t parse_opts) /**< parse options */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  ecma_string_t *arguments_str_p = ecma_op_create_dynamic_function_arguments_helper (arguments_list_p,
                                                                                     arguments_list_len);

  if (JERRY_UNLIKELY (arguments_str_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }

  ecma_string_t *function_body_str_p;

  if (arguments_list_len > 0)
  {
    function_body_str_p = ecma_op_to_string (arguments_list_p[arguments_list_len - 1]);

    if (JERRY_UNLIKELY (function_body_str_p == NULL))
    {
      ecma_deref_ecma_string (arguments_str_p);
      return ECMA_VALUE_ERROR;
    }
  }
  else
  {
    /* Very unlikely code path, not optimized. */
    function_body_str_p = ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY);
  }

  ECMA_STRING_TO_UTF8_STRING (arguments_str_p, arguments_buffer_p, arguments_buffer_size);
  ECMA_STRING_TO_UTF8_STRING (function_body_str_p, function_body_buffer_p, function_body_buffer_size);

  ecma_compiled_code_t *bytecode_p = parser_parse_script (arguments_buffer_p,
                                                          arguments_buffer_size,
                                                          function_body_buffer_p,
                                                          function_body_buffer_size,
                                                          parse_opts,
                                                          NULL);

  ECMA_FINALIZE_UTF8_STRING (function_body_buffer_p, function_body_buffer_size);
  ECMA_FINALIZE_UTF8_STRING (arguments_buffer_p, arguments_buffer_size);
  ecma_deref_ecma_string (arguments_str_p);
  ecma_deref_ecma_string (function_body_str_p);

  if (JERRY_UNLIKELY (bytecode_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }

#if JERRY_ESNEXT
  ecma_value_t *func_name_p;
  func_name_p = ecma_compiled_code_resolve_function_name ((const ecma_compiled_code_t *) bytecode_p);
  *func_name_p = ecma_make_magic_string_value (LIT_MAGIC_STRING_ANONYMOUS);
#endif /* JERRY_ESNEXT */

  ecma_object_t *global_object_p = ecma_builtin_get_global ();

#if JERRY_BUILTIN_REALMS
  JERRY_ASSERT (global_object_p == (ecma_object_t *) ecma_op_function_get_realm (bytecode_p));
#endif /* JERRY_BUILTIN_REALMS */

  ecma_object_t *global_env_p = ecma_get_global_environment (global_object_p);
  ecma_builtin_id_t fallback_proto = ECMA_BUILTIN_ID_FUNCTION_PROTOTYPE;

#if JERRY_ESNEXT
  ecma_object_t *new_target_p = JERRY_CONTEXT (current_new_target_p);
  ecma_builtin_id_t fallback_ctor = ECMA_BUILTIN_ID_FUNCTION;

  if (JERRY_UNLIKELY (parse_opts & (ECMA_PARSE_GENERATOR_FUNCTION | ECMA_PARSE_ASYNC_FUNCTION)))
  {
    fallback_proto = ECMA_BUILTIN_ID_ASYNC_GENERATOR;
    fallback_ctor = ECMA_BUILTIN_ID_ASYNC_GENERATOR_FUNCTION;

    if (!(parse_opts & ECMA_PARSE_GENERATOR_FUNCTION))
    {
      fallback_proto = ECMA_BUILTIN_ID_ASYNC_FUNCTION_PROTOTYPE;
      fallback_ctor = ECMA_BUILTIN_ID_ASYNC_FUNCTION;
    }
    else if (!(parse_opts & ECMA_PARSE_ASYNC_FUNCTION))
    {
      fallback_proto = ECMA_BUILTIN_ID_GENERATOR;
      fallback_ctor = ECMA_BUILTIN_ID_GENERATOR_FUNCTION;
    }
  }

  if (new_target_p == NULL)
  {
    new_target_p = ecma_builtin_get (fallback_ctor);
  }

  ecma_object_t *proto = ecma_op_get_prototype_from_constructor (new_target_p, fallback_proto);

  if (JERRY_UNLIKELY (proto == NULL))
  {
    ecma_bytecode_deref (bytecode_p);
    return ECMA_VALUE_ERROR;
  }
#endif /* JERRY_ESNEXT */

  ecma_object_t *func_obj_p = ecma_op_create_function_object (global_env_p, bytecode_p, fallback_proto);

#if JERRY_ESNEXT
  ECMA_SET_NON_NULL_POINTER (func_obj_p->u2.prototype_cp, proto);
  ecma_deref_object (proto);
#endif /* JERRY_ESNEXT */

  ecma_bytecode_deref (bytecode_p);
  return ecma_make_object_value (func_obj_p);
} /* ecma_op_create_dynamic_function */

/**
 * Function object creation operation.
 *
 * See also: ECMA-262 v5, 13.2
 *
 * @return pointer to newly created Function object
 */
ecma_object_t *
ecma_op_create_simple_function_object (ecma_object_t *scope_p, /**< function's scope */
                                       const ecma_compiled_code_t *bytecode_data_p) /**< byte-code array */
{
  return ecma_op_create_function_object (scope_p, bytecode_data_p, ECMA_BUILTIN_ID_FUNCTION_PROTOTYPE);
} /* ecma_op_create_simple_function_object */

#if JERRY_ESNEXT

/**
 * Create a function object with the appropriate prototype.
 *
 * @return pointer to newly created Function object
 */
ecma_object_t *
ecma_op_create_any_function_object (ecma_object_t *scope_p, /**< function's scope */
                                    const ecma_compiled_code_t *bytecode_data_p) /**< byte-code array */
{
  ecma_builtin_id_t proto_id;

  switch (CBC_FUNCTION_GET_TYPE (bytecode_data_p->status_flags))
  {
    case CBC_FUNCTION_GENERATOR:
    {
      proto_id = ECMA_BUILTIN_ID_GENERATOR;
      break;
    }
    case CBC_FUNCTION_ASYNC:
    {
      proto_id = ECMA_BUILTIN_ID_ASYNC_FUNCTION_PROTOTYPE;
      break;
    }
    case CBC_FUNCTION_ASYNC_GENERATOR:
    {
      proto_id = ECMA_BUILTIN_ID_ASYNC_GENERATOR;
      break;
    }
    default:
    {
      proto_id = ECMA_BUILTIN_ID_FUNCTION_PROTOTYPE;
      break;
    }
  }

  return ecma_op_create_function_object (scope_p, bytecode_data_p, proto_id);
} /* ecma_op_create_any_function_object */

/**
 * Arrow function object creation operation.
 *
 * See also: ES2015, 9.2.12
 *
 * @return pointer to newly created Function object
 */
ecma_object_t *
ecma_op_create_arrow_function_object (ecma_object_t *scope_p, /**< function's scope */
                                      const ecma_compiled_code_t *bytecode_data_p, /**< byte-code array */
                                      ecma_value_t this_binding) /**< value of 'this' binding */
{
  ecma_object_t *prototype_obj_p;

  if (CBC_FUNCTION_GET_TYPE (bytecode_data_p->status_flags) == CBC_FUNCTION_ARROW)
  {
    prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_FUNCTION_PROTOTYPE);
  }
  else
  {
    JERRY_ASSERT (CBC_FUNCTION_GET_TYPE (bytecode_data_p->status_flags) == CBC_FUNCTION_ASYNC_ARROW);
    prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_ASYNC_FUNCTION_PROTOTYPE);
  }

  size_t arrow_function_object_size = sizeof (ecma_arrow_function_t);

#if JERRY_SNAPSHOT_EXEC
  if (bytecode_data_p->status_flags & CBC_CODE_FLAGS_STATIC_FUNCTION)
  {
    arrow_function_object_size = sizeof (ecma_static_arrow_function_t);
  }
#endif /* JERRY_SNAPSHOT_EXEC */

  ecma_object_t *func_p = ecma_create_object (prototype_obj_p,
                                              arrow_function_object_size,
                                              ECMA_OBJECT_TYPE_FUNCTION);

  ecma_arrow_function_t *arrow_func_p = (ecma_arrow_function_t *) func_p;

  ECMA_SET_NON_NULL_POINTER_TAG (arrow_func_p->header.u.function.scope_cp, scope_p, 0);

#if JERRY_SNAPSHOT_EXEC
  if ((bytecode_data_p->status_flags & CBC_CODE_FLAGS_STATIC_FUNCTION))
  {
    arrow_func_p->header.u.function.bytecode_cp = ECMA_NULL_POINTER;
    ((ecma_static_arrow_function_t *) func_p)->bytecode_p = bytecode_data_p;
  }
  else
  {
#endif /* JERRY_SNAPSHOT_EXEC */
    ECMA_SET_INTERNAL_VALUE_POINTER (arrow_func_p->header.u.function.bytecode_cp, bytecode_data_p);
    ecma_bytecode_ref ((ecma_compiled_code_t *) bytecode_data_p);
#if JERRY_SNAPSHOT_EXEC
  }
#endif /* JERRY_SNAPSHOT_EXEC */

  arrow_func_p->this_binding = ecma_copy_value_if_not_object (this_binding);
  arrow_func_p->new_target = ECMA_VALUE_UNDEFINED;

  if (JERRY_CONTEXT (current_new_target_p) != NULL)
  {
    arrow_func_p->new_target = ecma_make_object_value (JERRY_CONTEXT (current_new_target_p));
  }
  return func_p;
} /* ecma_op_create_arrow_function_object */

#endif /* JERRY_ESNEXT */

/**
 * External function object creation operation.
 *
 * Note:
 *      external function object is implementation-defined object type
 *      that represent functions implemented in native code, using Embedding API
 *
 * @return pointer to newly created external function object
 */
ecma_object_t *
ecma_op_create_external_function_object (ecma_native_handler_t handler_cb) /**< pointer to external native handler */
{
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_FUNCTION_PROTOTYPE);

  ecma_object_t *function_obj_p = ecma_create_object (prototype_obj_p,
                                                      sizeof (ecma_native_function_t),
                                                      ECMA_OBJECT_TYPE_NATIVE_FUNCTION);

  /*
   * [[Class]] property is not stored explicitly for objects of ECMA_OBJECT_TYPE_NATIVE_FUNCTION type.
   *
   * See also: ecma_object_get_class_name
   */

  ecma_native_function_t *native_function_p = (ecma_native_function_t *) function_obj_p;
#if JERRY_BUILTIN_REALMS
  ECMA_SET_INTERNAL_VALUE_POINTER (native_function_p->realm_value,
                                   ecma_builtin_get_global ());
#endif /* JERRY_BUILTIN_REALMS */
  native_function_p->native_handler_cb = handler_cb;

  return function_obj_p;
} /* ecma_op_create_external_function_object */

#if JERRY_ESNEXT

/**
 * Create built-in native handler object.
 *
 * @return pointer to newly created native handler object
 */
ecma_object_t *
ecma_op_create_native_handler (ecma_native_handler_id_t id, /**< handler id */
                               size_t object_size) /**< created object size */
{
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_FUNCTION_PROTOTYPE);

  ecma_object_t *function_obj_p = ecma_create_object (prototype_obj_p,
                                                      object_size,
                                                      ECMA_OBJECT_TYPE_NATIVE_FUNCTION);
  ecma_set_object_is_builtin (function_obj_p);

  ecma_extended_object_t *ext_func_obj_p = (ecma_extended_object_t *) function_obj_p;
  ext_func_obj_p->u.built_in.id = ECMA_BUILTIN_ID_HANDLER;
  ext_func_obj_p->u.built_in.routine_id = (uint8_t) id;
  ext_func_obj_p->u.built_in.u2.routine_flags = ECMA_NATIVE_HANDLER_FLAGS_NONE;

#if JERRY_BUILTIN_REALMS
  ECMA_SET_INTERNAL_VALUE_POINTER (ext_func_obj_p->u.built_in.realm_value,
                                   ecma_builtin_get_global ());
#endif /* JERRY_BUILTIN_REALMS */

  return function_obj_p;
} /* ecma_op_create_native_handler */

#endif /* JERRY_ESNEXT */

/**
 * Get compiled code of a function object.
 *
 * @return compiled code
 */
extern inline const ecma_compiled_code_t * JERRY_ATTR_ALWAYS_INLINE
ecma_op_function_get_compiled_code (ecma_extended_object_t *function_p) /**< function pointer */
{
#if JERRY_SNAPSHOT_EXEC
  if (JERRY_LIKELY (function_p->u.function.bytecode_cp != ECMA_NULL_POINTER))
  {
    return ECMA_GET_INTERNAL_VALUE_POINTER (const ecma_compiled_code_t,
                                            function_p->u.function.bytecode_cp);
  }

  return ((ecma_static_function_t *) function_p)->bytecode_p;
#else /* !JERRY_SNAPSHOT_EXEC */
  return ECMA_GET_INTERNAL_VALUE_POINTER (const ecma_compiled_code_t,
                                          function_p->u.function.bytecode_cp);
#endif /* JERRY_SNAPSHOT_EXEC */
} /* ecma_op_function_get_compiled_code */

#if JERRY_BUILTIN_REALMS

/**
 * Get realm from a byte code.
 *
 * Note:
 *   Does not increase the reference counter.
 *
 * @return pointer to realm (global) object
 */
extern inline ecma_global_object_t * JERRY_ATTR_ALWAYS_INLINE
ecma_op_function_get_realm (const ecma_compiled_code_t *bytecode_header_p) /**< byte code header */
{
  ecma_value_t realm_value;

  if (bytecode_header_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
  {
    cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) bytecode_header_p;
    realm_value = args_p->realm_value;
  }
  else
  {
    cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) bytecode_header_p;
    realm_value = args_p->realm_value;
  }

#if JERRY_SNAPSHOT_EXEC
  if (JERRY_LIKELY (realm_value != JMEM_CP_NULL))
  {
    return ECMA_GET_INTERNAL_VALUE_POINTER (ecma_global_object_t, realm_value);
  }

  return (ecma_global_object_t *) ecma_builtin_get_global ();
#else /* !JERRY_SNAPSHOT_EXEC */
  return ECMA_GET_INTERNAL_VALUE_POINTER (ecma_global_object_t, realm_value);
#endif /* JERRY_SNAPSHOT_EXEC */
} /* ecma_op_function_get_realm */

/**
 * Get realm from a function
 *
 * Note:
 *   Does not increase the reference counter.
 *
 * @return realm (global) object
 */
ecma_global_object_t *
ecma_op_function_get_function_realm (ecma_object_t *func_obj_p) /**< function object */
{
  while (true)
  {
    if (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_FUNCTION)
    {
      ecma_extended_object_t *ext_function_obj_p = (ecma_extended_object_t *) func_obj_p;
      const ecma_compiled_code_t *bytecode_data_p = ecma_op_function_get_compiled_code (ext_function_obj_p);
      return ecma_op_function_get_realm (bytecode_data_p);
    }

    if (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_NATIVE_FUNCTION)
    {
      if (ecma_get_object_is_builtin (func_obj_p))
      {
        ecma_extended_object_t *ext_function_obj_p = (ecma_extended_object_t *) func_obj_p;
        return ECMA_GET_INTERNAL_VALUE_POINTER (ecma_global_object_t,
                                                ext_function_obj_p->u.built_in.realm_value);
      }
      ecma_native_function_t *native_function_p = (ecma_native_function_t *) func_obj_p;
      return ECMA_GET_INTERNAL_VALUE_POINTER (ecma_global_object_t,
                                              native_function_p->realm_value);
    }

  #if JERRY_BUILTIN_PROXY
    if (ECMA_OBJECT_IS_PROXY (func_obj_p))
    {
      ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) func_obj_p;
      if (ecma_is_value_null (proxy_obj_p->handler))
      {
        ecma_raise_type_error (ECMA_ERR_MSG ("Prototype from revoked Proxy is invalid"));
        return NULL;
      }
      func_obj_p = ecma_get_object_from_value (proxy_obj_p->target);
      continue;
    }
  #endif /* JERRY_BUILTIN_PROXY */

    JERRY_ASSERT (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_BOUND_FUNCTION);
    ecma_bound_function_t *bound_func_p = (ecma_bound_function_t *) func_obj_p;
    func_obj_p = ECMA_GET_NON_NULL_POINTER_FROM_POINTER_TAG (ecma_object_t,
                                                             bound_func_p->header.u.bound_function.target_function);
  }
} /* ecma_op_function_get_function_realm */

#endif /* JERRY_BUILTIN_REALMS */

/**
 * 15.3.5.3 implementation of [[HasInstance]] for Function objects
 *
 * @return true/false - if arguments are valid
 *         error - otherwise
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_function_has_instance (ecma_object_t *func_obj_p, /**< Function object */
                               ecma_value_t value) /**< argument 'V' */
{
  JERRY_ASSERT (func_obj_p != NULL
                && !ecma_is_lexical_environment (func_obj_p));

  if (!ecma_is_value_object (value))
  {
    return ECMA_VALUE_FALSE;
  }

  while (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_BOUND_FUNCTION)
  {
    JERRY_ASSERT (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_BOUND_FUNCTION);

    /* 1. 3. */
    ecma_bound_function_t *bound_func_p = (ecma_bound_function_t *) func_obj_p;

    func_obj_p = ECMA_GET_NON_NULL_POINTER_FROM_POINTER_TAG (ecma_object_t,
                                                             bound_func_p->header.u.bound_function.target_function);
  }

  JERRY_ASSERT (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_FUNCTION
                || ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_NATIVE_FUNCTION
                || ECMA_OBJECT_IS_PROXY (func_obj_p));

  ecma_object_t *v_obj_p = ecma_get_object_from_value (value);

  ecma_value_t prototype_obj_value = ecma_op_object_get_by_magic_id (func_obj_p,
                                                                     LIT_MAGIC_STRING_PROTOTYPE);

  if (ECMA_IS_VALUE_ERROR (prototype_obj_value))
  {
    return prototype_obj_value;
  }

  if (!ecma_is_value_object (prototype_obj_value))
  {
    ecma_free_value (prototype_obj_value);
    return ecma_raise_type_error (ECMA_ERR_MSG ("Object expected"));
  }

  ecma_object_t *prototype_obj_p = ecma_get_object_from_value (prototype_obj_value);
  JERRY_ASSERT (prototype_obj_p != NULL);

#if JERRY_BUILTIN_PROXY
  ecma_value_t result = ECMA_VALUE_ERROR;
#else /* !JERRY_BUILTIN_PROXY */
  ecma_value_t result = ECMA_VALUE_FALSE;
#endif /* JERRY_BUILTIN_PROXY */

  ecma_ref_object (v_obj_p);

  while (true)
  {
    ecma_object_t *current_proto_p = ecma_op_object_get_prototype_of (v_obj_p);
    ecma_deref_object (v_obj_p);

    if (current_proto_p == NULL)
    {
#if JERRY_BUILTIN_PROXY
      result = ECMA_VALUE_FALSE;
#endif /* JERRY_BUILTIN_PROXY */
      break;
    }
    else if (current_proto_p == ECMA_OBJECT_POINTER_ERROR)
    {
      break;
    }

    if (current_proto_p == prototype_obj_p)
    {
      ecma_deref_object (current_proto_p);
      result = ECMA_VALUE_TRUE;
      break;
    }

    /* Advance up on prototype chain. */
    v_obj_p = current_proto_p;
  }

  ecma_deref_object (prototype_obj_p);
  return result;
} /* ecma_op_function_has_instance */

#if JERRY_ESNEXT

/**
 * GetSuperConstructor operation for class methods
 *
 * See also: ECMAScript v6, 12.3.5.2
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         super constructor - otherwise
 */
ecma_value_t
ecma_op_function_get_super_constructor (ecma_object_t *func_obj_p) /**< function object */
{
  ecma_object_t *super_ctor_p = ecma_op_object_get_prototype_of (func_obj_p);

  if (JERRY_UNLIKELY (super_ctor_p == ECMA_OBJECT_POINTER_ERROR))
  {
    return ECMA_VALUE_ERROR;
  }
  else if (super_ctor_p == NULL || !ecma_object_is_constructor (super_ctor_p))
  {
    if (super_ctor_p != NULL)
    {
      ecma_deref_object (super_ctor_p);
    }
    return ecma_raise_type_error (ECMA_ERR_MSG ("Super binding must be a constructor"));
  }

  return ecma_make_object_value (super_ctor_p);
} /* ecma_op_function_get_super_constructor */
#endif /* JERRY_ESNEXT */

/**
 * Ordinary internal method: GetPrototypeFromConstructor (constructor, intrinsicDefaultProto)
 *
 * See also:
 *   - ECMAScript v6, 9.1.15
 *   - ECMAScript v10, 9.1.14
 *
 * @return NULL - if the operation fail (exception on the global context is raised)
 *         pointer to the prototype object - otherwise
 */
ecma_object_t *
ecma_op_get_prototype_from_constructor (ecma_object_t *ctor_obj_p, /**< constructor to get prototype from  */
                                        ecma_builtin_id_t default_proto_id) /**< intrinsicDefaultProto */
{
  JERRY_ASSERT (ecma_op_object_is_callable (ctor_obj_p));
  JERRY_ASSERT (default_proto_id < ECMA_BUILTIN_ID__COUNT);

  ecma_value_t proto = ecma_op_object_get_by_magic_id (ctor_obj_p, LIT_MAGIC_STRING_PROTOTYPE);

  if (ECMA_IS_VALUE_ERROR (proto))
  {
    return NULL;
  }

  ecma_object_t *proto_obj_p;

  if (!ecma_is_value_object (proto))
  {
    ecma_free_value (proto);

#if JERRY_BUILTIN_PROXY
    if (ECMA_OBJECT_IS_PROXY (ctor_obj_p))
    {
      ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) ctor_obj_p;
      if (ecma_is_value_null (proxy_obj_p->handler))
      {
        ecma_raise_type_error (ECMA_ERR_MSG ("Prototype from revoked Proxy is invalid"));
        return NULL;
      }
    }
#endif /* JERRY_BUILTIN_PROXY */

#if JERRY_BUILTIN_REALMS
    proto_obj_p = ecma_builtin_get_from_realm (ecma_op_function_get_function_realm (ctor_obj_p), default_proto_id);
#else /* !JERRY_BUILTIN_REALMS */
    proto_obj_p = ecma_builtin_get (default_proto_id);
#endif /* JERRY_BUILTIN_REALMS */
    ecma_ref_object (proto_obj_p);
  }
  else
  {
    proto_obj_p = ecma_get_object_from_value (proto);
  }

  return proto_obj_p;
} /* ecma_op_get_prototype_from_constructor */

/**
 * Perform a JavaScript function object method call.
 *
 * The input function object should be a pure JavaScript method
 *
 * @return the result of the function call.
 */
static ecma_value_t
ecma_op_function_call_simple (ecma_object_t *func_obj_p, /**< Function object */
                              ecma_value_t this_arg_value, /**< 'this' argument's value */
                              const ecma_value_t *arguments_list_p, /**< arguments list */
                              uint32_t arguments_list_len) /**< length of arguments list */
{
  JERRY_ASSERT (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_FUNCTION);
  JERRY_ASSERT (!ecma_get_object_is_builtin (func_obj_p));

  vm_frame_ctx_shared_args_t shared_args;
  shared_args.header.status_flags = VM_FRAME_CTX_SHARED_HAS_ARG_LIST;
  shared_args.header.called_object_p = func_obj_p;
  shared_args.arg_list_p = arguments_list_p;
  shared_args.arg_list_len = arguments_list_len;

  /* Entering Function Code (ECMA-262 v5, 10.4.3) */
  ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) func_obj_p;

  ecma_object_t *scope_p = ECMA_GET_NON_NULL_POINTER_FROM_POINTER_TAG (ecma_object_t,
                                                                       ext_func_p->u.function.scope_cp);

  /* 8. */
  ecma_value_t this_binding = this_arg_value;

  const ecma_compiled_code_t *bytecode_data_p = ecma_op_function_get_compiled_code (ext_func_p);
  uint16_t status_flags = bytecode_data_p->status_flags;

  shared_args.header.bytecode_header_p = bytecode_data_p;

#if JERRY_BUILTIN_REALMS
  ecma_global_object_t *realm_p = ecma_op_function_get_realm (bytecode_data_p);
#endif /* JERRY_BUILTIN_REALMS */

  /* 1. */
#if JERRY_ESNEXT
  if (JERRY_UNLIKELY (CBC_FUNCTION_IS_ARROW (status_flags)))
  {
    ecma_arrow_function_t *arrow_func_p = (ecma_arrow_function_t *) func_obj_p;

    if (ecma_is_value_undefined (arrow_func_p->new_target))
    {
      JERRY_CONTEXT (current_new_target_p) = NULL;
    }
    else
    {
      JERRY_CONTEXT (current_new_target_p) = ecma_get_object_from_value (arrow_func_p->new_target);
    }
    this_binding = arrow_func_p->this_binding;
  }
  else
  {
    shared_args.header.status_flags |= VM_FRAME_CTX_SHARED_NON_ARROW_FUNC;
#endif /* JERRY_ESNEXT */

    if (!(status_flags & CBC_CODE_FLAGS_STRICT_MODE))
    {
      if (ecma_is_value_undefined (this_binding)
          || ecma_is_value_null (this_binding))
      {
        /* 2. */
#if JERRY_BUILTIN_REALMS
        this_binding = realm_p->this_binding;
#else /* !JERRY_BUILTIN_REALMS */
        this_binding = ecma_make_object_value (ecma_builtin_get_global ());
#endif /* JERRY_BUILTIN_REALMS */
      }
      else if (!ecma_is_value_object (this_binding))
      {
        /* 3., 4. */
        this_binding = ecma_op_to_object (this_binding);
        shared_args.header.status_flags |= VM_FRAME_CTX_SHARED_FREE_THIS;

        JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (this_binding));
      }
    }
#if JERRY_ESNEXT
  }
#endif /* JERRY_ESNEXT */

  /* 5. */
  if (!(status_flags & CBC_CODE_FLAGS_LEXICAL_ENV_NOT_NEEDED))
  {
    shared_args.header.status_flags |= VM_FRAME_CTX_SHARED_FREE_LOCAL_ENV;
    scope_p = ecma_create_decl_lex_env (scope_p);
  }

  ecma_value_t ret_value;

#if JERRY_ESNEXT
  if (JERRY_UNLIKELY (CBC_FUNCTION_GET_TYPE (status_flags) == CBC_FUNCTION_CONSTRUCTOR))
  {
    if (JERRY_CONTEXT (current_new_target_p) == NULL)
    {
      ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Class constructor requires 'new'"));
      goto exit;
    }

    ecma_value_t lexical_this = this_binding;

    if (ECMA_GET_THIRD_BIT_FROM_POINTER_TAG (ext_func_p->u.function.scope_cp))
    {
      shared_args.header.status_flags |= VM_FRAME_CTX_SHARED_HERITAGE_PRESENT;
      lexical_this = ECMA_VALUE_UNINITIALIZED;
    }

    ecma_op_create_environment_record (scope_p, lexical_this, func_obj_p);
  }
#endif /* JERRY_ESNEXT */

#if JERRY_BUILTIN_REALMS
  ecma_global_object_t *saved_global_object_p = JERRY_CONTEXT (global_object_p);
  JERRY_CONTEXT (global_object_p) = realm_p;
#endif /* JERRY_BUILTIN_REALMS */

  ret_value = vm_run (&shared_args.header, this_binding, scope_p);

#if JERRY_BUILTIN_REALMS
  JERRY_CONTEXT (global_object_p) = saved_global_object_p;
#endif /* JERRY_BUILTIN_REALMS */

#if JERRY_ESNEXT
  /* ECMAScript v6, 9.2.2.13 */
  if (JERRY_UNLIKELY (shared_args.header.status_flags & VM_FRAME_CTX_SHARED_HERITAGE_PRESENT))
  {
    if (!ECMA_IS_VALUE_ERROR (ret_value) && !ecma_is_value_object (ret_value))
    {
      if (!ecma_is_value_undefined (ret_value))
      {
        ecma_free_value (ret_value);
        ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Derived constructors may only return object or undefined"));
      }
      else
      {
        ret_value = ecma_op_get_this_binding (scope_p);
      }
    }
  }

exit:
#endif /* JERRY_ESNEXT */

  if (JERRY_UNLIKELY (shared_args.header.status_flags & VM_FRAME_CTX_SHARED_FREE_LOCAL_ENV))
  {
    ecma_deref_object (scope_p);
  }

  if (JERRY_UNLIKELY (shared_args.header.status_flags & VM_FRAME_CTX_SHARED_FREE_THIS))
  {
    ecma_free_value (this_binding);
  }

  return ret_value;
} /* ecma_op_function_call_simple */

/**
 * Perform a native C method call which was registered via the API.
 *
 * @return the result of the function call.
 */
static ecma_value_t JERRY_ATTR_NOINLINE
ecma_op_function_call_native (ecma_object_t *func_obj_p, /**< Function object */
                              ecma_value_t this_arg_value, /**< 'this' argument's value */
                              const ecma_value_t *arguments_list_p, /**< arguments list */
                              uint32_t arguments_list_len) /**< length of arguments list */

{
  JERRY_ASSERT (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_NATIVE_FUNCTION);

  if (ecma_get_object_is_builtin (func_obj_p))
  {
#if JERRY_BUILTIN_REALMS
    ecma_global_object_t *saved_global_object_p = JERRY_CONTEXT (global_object_p);

    ecma_extended_object_t *ext_func_obj_p = (ecma_extended_object_t *) func_obj_p;
    JERRY_CONTEXT (global_object_p) = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_global_object_t,
                                                                       ext_func_obj_p->u.built_in.realm_value);
#endif /* JERRY_BUILTIN_REALMS */

    ecma_value_t ret_value = ecma_builtin_dispatch_call (func_obj_p,
                                                         this_arg_value,
                                                         arguments_list_p,
                                                         arguments_list_len);

#if JERRY_BUILTIN_REALMS
    JERRY_CONTEXT (global_object_p) = saved_global_object_p;
#endif /* JERRY_BUILTIN_REALMS */
    return ret_value;
  }

  ecma_native_function_t *native_function_p = (ecma_native_function_t *) func_obj_p;

#if JERRY_BUILTIN_REALMS
  ecma_global_object_t *saved_global_object_p = JERRY_CONTEXT (global_object_p);
  JERRY_CONTEXT (global_object_p) = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_global_object_t,
                                                                     native_function_p->realm_value);
#endif /* JERRY_BUILTIN_REALMS */

  jerry_call_info_t call_info;
  call_info.function = ecma_make_object_value (func_obj_p);
  call_info.this_value = this_arg_value;

#if JERRY_ESNEXT
  ecma_object_t *new_target_p = JERRY_CONTEXT (current_new_target_p);
  call_info.new_target = (new_target_p == NULL) ? ECMA_VALUE_UNDEFINED : ecma_make_object_value (new_target_p);
#else /* JERRY_ESNEXT */
  call_info.new_target = ECMA_VALUE_UNDEFINED;
#endif /* JERRY_ESNEXT */

  JERRY_ASSERT (native_function_p->native_handler_cb != NULL);
  ecma_value_t ret_value = native_function_p->native_handler_cb (&call_info,
                                                                 arguments_list_p,
                                                                 arguments_list_len);
#if JERRY_BUILTIN_REALMS
  JERRY_CONTEXT (global_object_p) = saved_global_object_p;
#endif /* JERRY_BUILTIN_REALMS */

  if (JERRY_UNLIKELY (ecma_is_value_error_reference (ret_value)))
  {
    ecma_raise_error_from_error_reference (ret_value);
    return ECMA_VALUE_ERROR;
  }

#if JERRY_DEBUGGER
  JERRY_DEBUGGER_CLEAR_FLAGS (JERRY_DEBUGGER_VM_EXCEPTION_THROWN);
#endif /* JERRY_DEBUGGER */
  return ret_value;
} /* ecma_op_function_call_native */

/**
 * Append the bound arguments into the given collection
 *
 * Note:
 *       - The whole bound chain is resolved
 *       - The first element of the collection contains the bounded this value
 *
 * @return target function of the bound function
 */
JERRY_ATTR_NOINLINE static ecma_object_t *
ecma_op_bound_function_get_argument_list (ecma_object_t *func_obj_p, /**< bound bunction object */
                                          ecma_collection_t *list_p) /**< list of arguments */
{
  JERRY_ASSERT (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_BOUND_FUNCTION);

  ecma_bound_function_t *bound_func_p = (ecma_bound_function_t *) func_obj_p;

  func_obj_p = ECMA_GET_NON_NULL_POINTER_FROM_POINTER_TAG (ecma_object_t,
                                                           bound_func_p->header.u.bound_function.target_function);

  ecma_value_t args_len_or_this = bound_func_p->header.u.bound_function.args_len_or_this;

  uint32_t args_length = 1;

  if (ecma_is_value_integer_number (args_len_or_this))
  {
    args_length = (uint32_t) ecma_get_integer_from_value (args_len_or_this);
  }

  /* 5. */
  if (args_length != 1)
  {
    const ecma_value_t *args_p = (const ecma_value_t *) (bound_func_p + 1);
    list_p->buffer_p[0] = *args_p;

    if (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_BOUND_FUNCTION)
    {
      func_obj_p = ecma_op_bound_function_get_argument_list (func_obj_p, list_p);
    }
    ecma_collection_append (list_p, args_p + 1, args_length - 1);
  }
  else
  {
    list_p->buffer_p[0] = args_len_or_this;
  }

  return func_obj_p;
} /* ecma_op_bound_function_get_argument_list */

/**
 * [[Call]] internal method for bound function objects
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
static ecma_value_t JERRY_ATTR_NOINLINE
ecma_op_function_call_bound (ecma_object_t *func_obj_p, /**< Function object */
                             const ecma_value_t *arguments_list_p, /**< arguments list */
                             uint32_t arguments_list_len) /**< length of arguments list */
{
  JERRY_ASSERT (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_BOUND_FUNCTION);

  JERRY_CONTEXT (status_flags) &= (uint32_t) ~ECMA_STATUS_DIRECT_EVAL;

  ecma_collection_t *bound_arg_list_p = ecma_new_collection ();
  ecma_collection_push_back (bound_arg_list_p, ECMA_VALUE_EMPTY);

  ecma_object_t *target_obj_p = ecma_op_bound_function_get_argument_list (func_obj_p, bound_arg_list_p);

  ecma_collection_append (bound_arg_list_p, arguments_list_p, arguments_list_len);

  JERRY_ASSERT (!ecma_is_value_empty (bound_arg_list_p->buffer_p[0]));

  ecma_value_t ret_value = ecma_op_function_call (target_obj_p,
                                                  bound_arg_list_p->buffer_p[0],
                                                  bound_arg_list_p->buffer_p + 1,
                                                  (uint32_t) (bound_arg_list_p->item_count - 1));

  ecma_collection_destroy (bound_arg_list_p);

  return ret_value;
} /* ecma_op_function_call_bound */

/**
 * [[Call]] implementation for Function objects,
 * created through 13.2 (ECMA_OBJECT_TYPE_FUNCTION)
 * or 15.3.4.5 (ECMA_OBJECT_TYPE_BOUND_FUNCTION),
 * and for built-in Function objects
 * from section 15 (ECMA_OBJECT_TYPE_FUNCTION).
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_function_call (ecma_object_t *func_obj_p, /**< Function object */
                       ecma_value_t this_arg_value, /**< 'this' argument's value */
                       const ecma_value_t *arguments_list_p, /**< arguments list */
                       uint32_t arguments_list_len) /**< length of arguments list */
{
  JERRY_ASSERT (func_obj_p != NULL
                && !ecma_is_lexical_environment (func_obj_p));
  JERRY_ASSERT (ecma_op_object_is_callable (func_obj_p));

  ECMA_CHECK_STACK_USAGE ();

  const ecma_object_type_t type = ecma_get_object_type (func_obj_p);

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_TYPE_IS_PROXY (type))
  {
    return ecma_proxy_object_call (func_obj_p, this_arg_value, arguments_list_p, arguments_list_len);
  }
#endif /* JERRY_BUILTIN_PROXY */

#if JERRY_ESNEXT
  ecma_object_t *old_new_target_p = JERRY_CONTEXT (current_new_target_p);
  if (JERRY_UNLIKELY (!(JERRY_CONTEXT (status_flags) & ECMA_STATUS_DIRECT_EVAL)))
  {
    JERRY_CONTEXT (current_new_target_p) = NULL;
  }
#endif /* JERRY_ESNEXT */

  ecma_value_t result;

  if (JERRY_LIKELY (type == ECMA_OBJECT_TYPE_FUNCTION))
  {
    result = ecma_op_function_call_simple (func_obj_p, this_arg_value, arguments_list_p, arguments_list_len);
  }
  else if (type == ECMA_OBJECT_TYPE_NATIVE_FUNCTION)
  {
    result = ecma_op_function_call_native (func_obj_p, this_arg_value, arguments_list_p, arguments_list_len);
  }
  else
  {
    result = ecma_op_function_call_bound (func_obj_p, arguments_list_p, arguments_list_len);
  }

#if JERRY_ESNEXT
  JERRY_CONTEXT (current_new_target_p) = old_new_target_p;
#endif /* JERRY_ESNEXT */

  return result;
} /* ecma_op_function_call */

/**
 * [[Construct]] internal method for bound function objects
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
static ecma_value_t JERRY_ATTR_NOINLINE
ecma_op_function_construct_bound (ecma_object_t *func_obj_p, /**< Function object */
                                  ecma_object_t *new_target_p, /**< new target */
                                  const ecma_value_t *arguments_list_p, /**< arguments list */
                                  uint32_t arguments_list_len) /**< length of arguments list */
{
  JERRY_ASSERT (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_BOUND_FUNCTION);

  ecma_collection_t *bound_arg_list_p = ecma_new_collection ();
  ecma_collection_push_back (bound_arg_list_p, ECMA_VALUE_EMPTY);

  ecma_object_t *target_obj_p = ecma_op_bound_function_get_argument_list (func_obj_p, bound_arg_list_p);

  ecma_collection_append (bound_arg_list_p, arguments_list_p, arguments_list_len);

  if (func_obj_p == new_target_p)
  {
    new_target_p = target_obj_p;
  }

  ecma_value_t ret_value = ecma_op_function_construct (target_obj_p,
                                                       new_target_p,
                                                       bound_arg_list_p->buffer_p + 1,
                                                       (uint32_t) (bound_arg_list_p->item_count - 1));

  ecma_collection_destroy (bound_arg_list_p);

  return ret_value;
} /* ecma_op_function_construct_bound */

/**
 * [[Construct]] internal method for external function objects
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
static ecma_value_t
ecma_op_function_construct_native (ecma_object_t *func_obj_p, /**< Function object */
                                   ecma_object_t *new_target_p, /**< new target */
                                   const ecma_value_t *arguments_list_p, /**< arguments list */
                                   uint32_t arguments_list_len) /**< length of arguments list */
{
  JERRY_ASSERT (ecma_get_object_type (func_obj_p) == ECMA_OBJECT_TYPE_NATIVE_FUNCTION);

  ecma_object_t *proto_p = ecma_op_get_prototype_from_constructor (new_target_p, ECMA_BUILTIN_ID_OBJECT_PROTOTYPE);

  if (JERRY_UNLIKELY (proto_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }

  ecma_object_t *new_this_obj_p = ecma_create_object (proto_p, 0, ECMA_OBJECT_TYPE_GENERAL);
  ecma_value_t this_arg = ecma_make_object_value (new_this_obj_p);
  ecma_deref_object (proto_p);

#if JERRY_ESNEXT
  ecma_object_t *old_new_target_p = JERRY_CONTEXT (current_new_target_p);
  JERRY_CONTEXT (current_new_target_p) = new_target_p;
#endif /* JERRY_ESNEXT */

  ecma_value_t ret_value = ecma_op_function_call_native (func_obj_p, this_arg, arguments_list_p, arguments_list_len);

#if JERRY_ESNEXT
  JERRY_CONTEXT (current_new_target_p) = old_new_target_p;
#endif /* JERRY_ESNEXT */

  if (ECMA_IS_VALUE_ERROR (ret_value) || ecma_is_value_object (ret_value))
  {
    ecma_deref_object (new_this_obj_p);
    return ret_value;
  }

  ecma_free_value (ret_value);

  return this_arg;
} /* ecma_op_function_construct_native */

/**
 * General [[Construct]] implementation function objects
 *
 * See also: ECMAScript v6, 9.2.2
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_function_construct (ecma_object_t *func_obj_p, /**< Function object */
                            ecma_object_t *new_target_p, /**< new target */
                            const ecma_value_t *arguments_list_p, /**< arguments list */
                            uint32_t arguments_list_len) /**< length of arguments list */
{
  JERRY_ASSERT (func_obj_p != NULL
                && !ecma_is_lexical_environment (func_obj_p));

  const ecma_object_type_t type = ecma_get_object_type (func_obj_p);

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_TYPE_IS_PROXY (type))
  {
    return ecma_proxy_object_construct (func_obj_p,
                                        new_target_p,
                                        arguments_list_p,
                                        arguments_list_len);
  }
#endif /* JERRY_BUILTIN_PROXY */

  if (JERRY_UNLIKELY (type == ECMA_OBJECT_TYPE_BOUND_FUNCTION))
  {
    return ecma_op_function_construct_bound (func_obj_p, new_target_p, arguments_list_p, arguments_list_len);
  }

  if (JERRY_UNLIKELY (type == ECMA_OBJECT_TYPE_NATIVE_FUNCTION))
  {
    if (JERRY_UNLIKELY (ecma_get_object_is_builtin (func_obj_p)))
    {
#if JERRY_BUILTIN_REALMS
      ecma_global_object_t *saved_global_object_p = JERRY_CONTEXT (global_object_p);
      ecma_value_t realm_value = ((ecma_extended_object_t *) func_obj_p)->u.built_in.realm_value;
      JERRY_CONTEXT (global_object_p) = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_global_object_t, realm_value);
#endif /* JERRY_BUILTIN_REALMS */

#if JERRY_ESNEXT
      ecma_object_t *old_new_target = JERRY_CONTEXT (current_new_target_p);
      JERRY_CONTEXT (current_new_target_p) = new_target_p;
#endif /* JERRY_ESNEXT */

      ecma_value_t ret_value = ecma_builtin_dispatch_construct (func_obj_p, arguments_list_p, arguments_list_len);

#if JERRY_ESNEXT
      JERRY_CONTEXT (current_new_target_p) = old_new_target;
#endif /* JERRY_ESNEXT */

#if JERRY_BUILTIN_REALMS
      JERRY_CONTEXT (global_object_p) = saved_global_object_p;
#endif /* JERRY_BUILTIN_REALMS */
      return ret_value;
    }

    return ecma_op_function_construct_native (func_obj_p, new_target_p, arguments_list_p, arguments_list_len);
  }

  JERRY_ASSERT (type == ECMA_OBJECT_TYPE_FUNCTION);
  JERRY_ASSERT (!ecma_get_object_is_builtin (func_obj_p));

  ecma_object_t *new_this_obj_p = NULL;
  ecma_value_t this_arg;

#if JERRY_ESNEXT
  ecma_extended_object_t *ext_func_obj_p = (ecma_extended_object_t *) func_obj_p;

  /* 5. */
  if (!ECMA_GET_THIRD_BIT_FROM_POINTER_TAG (ext_func_obj_p->u.function.scope_cp))
  {
#endif /* JERRY_ESNEXT */
    /* 5.a */
    ecma_object_t *proto_p = ecma_op_get_prototype_from_constructor (new_target_p, ECMA_BUILTIN_ID_OBJECT_PROTOTYPE);

    /* 5.b */
    if (JERRY_UNLIKELY (proto_p == NULL))
    {
      return ECMA_VALUE_ERROR;
    }

    new_this_obj_p = ecma_create_object (proto_p, 0, ECMA_OBJECT_TYPE_GENERAL);
    ecma_deref_object (proto_p);
    this_arg = ecma_make_object_value (new_this_obj_p);
#if JERRY_ESNEXT
  }
  else
  {
    this_arg = ECMA_VALUE_UNDEFINED;
  }

  /* 6. */
  ecma_object_t *old_new_target_p = JERRY_CONTEXT (current_new_target_p);
  JERRY_CONTEXT (current_new_target_p) = new_target_p;
#endif /* JERRY_ESNEXT */

  ecma_value_t ret_value = ecma_op_function_call_simple (func_obj_p, this_arg, arguments_list_p, arguments_list_len);

#if JERRY_ESNEXT
  JERRY_CONTEXT (current_new_target_p) = old_new_target_p;
#endif /* JERRY_ESNEXT */

  /* 13.a */
  if (ECMA_IS_VALUE_ERROR (ret_value) || ecma_is_value_object (ret_value))
  {
#if JERRY_ESNEXT
    if (new_this_obj_p != NULL)
    {
      ecma_deref_object (new_this_obj_p);
    }
#else /* !JERRY_ESNEXT */
    ecma_deref_object (new_this_obj_p);
#endif /* JERRY_ESNEXT */
    return ret_value;
  }

  /* 13.b */
  ecma_free_value (ret_value);
  return this_arg;
} /* ecma_op_function_construct */

/**
 * Lazy instantiation of 'prototype' property for non-builtin and external functions
 *
 * @return pointer to newly instantiated property
 */
static ecma_property_t *
ecma_op_lazy_instantiate_prototype_object (ecma_object_t *object_p) /**< the function object */
{
  JERRY_ASSERT (ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_FUNCTION
                || ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_NATIVE_FUNCTION);

#if JERRY_BUILTIN_REALMS
  ecma_global_object_t *global_object_p;

  if (ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_FUNCTION)
  {
    const ecma_compiled_code_t *bytecode_data_p;
    bytecode_data_p = ecma_op_function_get_compiled_code ((ecma_extended_object_t *) object_p);

    global_object_p = ecma_op_function_get_realm (bytecode_data_p);
  }
  else
  {
    ecma_native_function_t *native_function_p = (ecma_native_function_t *) object_p;

    global_object_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_global_object_t,
                                                       native_function_p->realm_value);
  }
#endif /* JERRY_BUILTIN_REALMS */

  /* ECMA-262 v5, 13.2, 16-18 */

  ecma_object_t *proto_object_p = NULL;
  bool init_constructor = true;

#if JERRY_ESNEXT
  if (ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_FUNCTION)
  {
    const ecma_compiled_code_t *byte_code_p = ecma_op_function_get_compiled_code ((ecma_extended_object_t *) object_p);

    if (!CBC_FUNCTION_HAS_PROTOTYPE (byte_code_p->status_flags))
    {
      return NULL;
    }

    if (CBC_FUNCTION_GET_TYPE (byte_code_p->status_flags) == CBC_FUNCTION_GENERATOR)
    {
      ecma_object_t *prototype_p;

#if JERRY_BUILTIN_REALMS
      prototype_p = ecma_builtin_get_from_realm (global_object_p, ECMA_BUILTIN_ID_GENERATOR_PROTOTYPE);
#else /* !JERRY_BUILTIN_REALMS */
      prototype_p = ecma_builtin_get (ECMA_BUILTIN_ID_GENERATOR_PROTOTYPE);
#endif /* JERRY_BUILTIN_REALMS */

      proto_object_p = ecma_create_object (prototype_p, 0, ECMA_OBJECT_TYPE_GENERAL);
      init_constructor = false;
    }

    if (CBC_FUNCTION_GET_TYPE (byte_code_p->status_flags) == CBC_FUNCTION_ASYNC_GENERATOR)
    {
      ecma_object_t *prototype_p;

#if JERRY_BUILTIN_REALMS
      prototype_p = ecma_builtin_get_from_realm (global_object_p, ECMA_BUILTIN_ID_ASYNC_GENERATOR_PROTOTYPE);
#else /* !JERRY_BUILTIN_REALMS */
      prototype_p = ecma_builtin_get (ECMA_BUILTIN_ID_ASYNC_GENERATOR_PROTOTYPE);
#endif /* JERRY_BUILTIN_REALMS */

      proto_object_p = ecma_create_object (prototype_p, 0, ECMA_OBJECT_TYPE_GENERAL);
      init_constructor = false;
    }
  }
#endif /* JERRY_ESNEXT */

#if JERRY_ESNEXT
  if (proto_object_p == NULL)
#endif /* JERRY_ESNEXT */
  {
    ecma_object_t *prototype_p;

#if JERRY_BUILTIN_REALMS
    prototype_p = ecma_builtin_get_from_realm (global_object_p, ECMA_BUILTIN_ID_OBJECT_PROTOTYPE);
#else /* !JERRY_BUILTIN_REALMS */
    prototype_p = ecma_builtin_get (ECMA_BUILTIN_ID_OBJECT_PROTOTYPE);
#endif /* JERRY_BUILTIN_REALMS */

    proto_object_p = ecma_op_create_object_object_noarg_and_set_prototype (prototype_p);
  }

  /* 17. */
  if (init_constructor)
  {
    ecma_property_value_t *constructor_prop_value_p;
    constructor_prop_value_p = ecma_create_named_data_property (proto_object_p,
                                                                ecma_get_magic_string (LIT_MAGIC_STRING_CONSTRUCTOR),
                                                                ECMA_PROPERTY_CONFIGURABLE_WRITABLE,
                                                                NULL);

    constructor_prop_value_p->value = ecma_make_object_value (object_p);
  }

  /* 18. */
  ecma_property_t *prototype_prop_p;
  ecma_property_value_t *prototype_prop_value_p;
  prototype_prop_value_p = ecma_create_named_data_property (object_p,
                                                            ecma_get_magic_string (LIT_MAGIC_STRING_PROTOTYPE),
                                                            ECMA_PROPERTY_FLAG_WRITABLE,
                                                            &prototype_prop_p);

  prototype_prop_value_p->value = ecma_make_object_value (proto_object_p);

  ecma_deref_object (proto_object_p);

  return prototype_prop_p;
} /* ecma_op_lazy_instantiate_prototype_object */

/**
 * Lazy instantiation of non-builtin ecma function object's properties
 *
 * Warning:
 *         Only non-configurable properties could be instantiated lazily in this function,
 *         as configurable properties could be deleted and it would be incorrect
 *         to reinstantiate them in the function in second time.
 *
 * @return pointer to newly instantiated property, if a property was instantiated,
 *         NULL - otherwise
 */
ecma_property_t *
ecma_op_function_try_to_lazy_instantiate_property (ecma_object_t *object_p, /**< the function object */
                                                   ecma_string_t *property_name_p) /**< property name */
{
  JERRY_ASSERT (!ecma_get_object_is_builtin (object_p));

#if JERRY_ESNEXT
  if (ecma_compare_ecma_string_to_magic_id (property_name_p, LIT_MAGIC_STRING_LENGTH))
  {
    ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) object_p;

    if (!ECMA_GET_FIRST_BIT_FROM_POINTER_TAG (ext_func_p->u.function.scope_cp))
    {
      /* Initialize 'length' property */
      const ecma_compiled_code_t *bytecode_data_p = ecma_op_function_get_compiled_code (ext_func_p);
      uint32_t len;

      if (bytecode_data_p->status_flags & CBC_CODE_FLAGS_HAS_EXTENDED_INFO)
      {
        len = CBC_EXTENDED_INFO_GET_LENGTH (ecma_compiled_code_resolve_extended_info (bytecode_data_p));
      }
      else if (bytecode_data_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
      {
        cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) bytecode_data_p;
        len = args_p->argument_end;
      }
      else
      {
        cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) bytecode_data_p;
        len = args_p->argument_end;
      }

      /* Set tag bit to represent initialized 'length' property */
      ECMA_SET_FIRST_BIT_TO_POINTER_TAG (ext_func_p->u.function.scope_cp);
      ecma_property_t *value_prop_p;
      ecma_property_value_t *value_p = ecma_create_named_data_property (object_p,
                                                                        property_name_p,
                                                                        ECMA_PROPERTY_FLAG_CONFIGURABLE,
                                                                        &value_prop_p);
      value_p->value = ecma_make_uint32_value (len);
      return value_prop_p;
    }

    return NULL;
  }

  if (ecma_compare_ecma_string_to_magic_id (property_name_p, LIT_MAGIC_STRING_NAME))
  {
    ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) object_p;
    if (!ECMA_GET_SECOND_BIT_FROM_POINTER_TAG (ext_func_p->u.function.scope_cp))
    {
      /* Set tag bit to represent initialized 'name' property */
      ECMA_SET_SECOND_BIT_TO_POINTER_TAG (ext_func_p->u.function.scope_cp);
      const ecma_compiled_code_t *bytecode_data_p = ecma_op_function_get_compiled_code (ext_func_p);

      if (CBC_FUNCTION_GET_TYPE (bytecode_data_p->status_flags) != CBC_FUNCTION_CONSTRUCTOR)
      {
        ecma_value_t value = *ecma_compiled_code_resolve_function_name (bytecode_data_p);
        JERRY_ASSERT (ecma_is_value_string (value));

        /* Initialize 'name' property */
        ecma_property_t *value_prop_p;
        ecma_property_value_t *value_p = ecma_create_named_data_property (object_p,
                                                                          property_name_p,
                                                                          ECMA_PROPERTY_FLAG_CONFIGURABLE,
                                                                          &value_prop_p);
        value_p->value = ecma_copy_value (value);
        return value_prop_p;
      }
    }

    return NULL;
  }
#endif /* JERRY_ESNEXT */

  if (ecma_compare_ecma_string_to_magic_id (property_name_p, LIT_MAGIC_STRING_PROTOTYPE)
      && ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_FUNCTION)
  {
    return ecma_op_lazy_instantiate_prototype_object (object_p);
  }

  const bool is_arguments = ecma_compare_ecma_string_to_magic_id (property_name_p, LIT_MAGIC_STRING_ARGUMENTS);

  if (is_arguments
      || ecma_compare_ecma_string_to_magic_id (property_name_p, LIT_MAGIC_STRING_CALLER))
  {
    const ecma_compiled_code_t *bytecode_data_p;
    bytecode_data_p = ecma_op_function_get_compiled_code ((ecma_extended_object_t *) object_p);

#if JERRY_ESNEXT
    if (!(bytecode_data_p->status_flags & CBC_CODE_FLAGS_STRICT_MODE)
        && CBC_FUNCTION_GET_TYPE (bytecode_data_p->status_flags) == CBC_FUNCTION_NORMAL)
    {
      ecma_property_t *value_prop_p;
      /* The property_name_p argument contains the name. */
      ecma_property_value_t *value_p = ecma_create_named_data_property (object_p,
                                                                        property_name_p,
                                                                        ECMA_PROPERTY_FIXED,
                                                                        &value_prop_p);
      value_p->value = is_arguments ? ECMA_VALUE_NULL : ECMA_VALUE_UNDEFINED;
      return value_prop_p;
    }
#else /* !JERRY_ESNEXT */
    if (bytecode_data_p->status_flags & CBC_CODE_FLAGS_STRICT_MODE)
    {
      ecma_object_t *thrower_p = ecma_builtin_get (ECMA_BUILTIN_ID_TYPE_ERROR_THROWER);

      ecma_property_t *caller_prop_p;
      /* The property_name_p argument contains the name. */
      ecma_create_named_accessor_property (object_p,
                                           property_name_p,
                                           thrower_p,
                                           thrower_p,
                                           ECMA_PROPERTY_FIXED,
                                           &caller_prop_p);
      return caller_prop_p;
    }
#endif /* JERRY_ESNEXT */
  }

  return NULL;
} /* ecma_op_function_try_to_lazy_instantiate_property */

/**
 * Create specification defined non-configurable properties for external functions.
 *
 * See also:
 *          ECMA-262 v5, 15.3.4.5
 *
 * @return pointer property, if one was instantiated,
 *         NULL - otherwise.
 */
ecma_property_t *
ecma_op_external_function_try_to_lazy_instantiate_property (ecma_object_t *object_p, /**< object */
                                                            ecma_string_t *property_name_p) /**< property's name */
{
  JERRY_ASSERT (ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_NATIVE_FUNCTION);

  if (ecma_compare_ecma_string_to_magic_id (property_name_p, LIT_MAGIC_STRING_PROTOTYPE))
  {
    return ecma_op_lazy_instantiate_prototype_object (object_p);
  }

  return NULL;
} /* ecma_op_external_function_try_to_lazy_instantiate_property */

/**
 * Create specification defined non-configurable properties for bound functions.
 *
 * See also:
 *          ECMA-262 v5, 15.3.4.5
 *
 * @return pointer property, if one was instantiated,
 *         NULL - otherwise.
 */
ecma_property_t *
ecma_op_bound_function_try_to_lazy_instantiate_property (ecma_object_t *object_p, /**< object */
                                                         ecma_string_t *property_name_p) /**< property's name */
{
  JERRY_ASSERT (ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_BOUND_FUNCTION);

  if (ecma_string_is_length (property_name_p))
  {
    ecma_bound_function_t *bound_func_p = (ecma_bound_function_t *) object_p;
    ecma_value_t args_len_or_this = bound_func_p->header.u.bound_function.args_len_or_this;
    ecma_number_t length = 0;
    ecma_integer_value_t args_length = 1;
    uint8_t length_attributes;

    if (ecma_is_value_integer_number (args_len_or_this))
    {
      args_length = ecma_get_integer_from_value (args_len_or_this);
    }

#if JERRY_ESNEXT
    if (ECMA_GET_FIRST_BIT_FROM_POINTER_TAG (bound_func_p->header.u.bound_function.target_function))
    {
      return NULL;
    }

    length_attributes = ECMA_PROPERTY_FLAG_CONFIGURABLE;
    length = ecma_get_number_from_value (bound_func_p->target_length) - (args_length - 1);

    /* Set tag bit to represent initialized 'length' property */
    ECMA_SET_FIRST_BIT_TO_POINTER_TAG (bound_func_p->header.u.bound_function.target_function);
#else /* !JERRY_ESNEXT */
    length_attributes = ECMA_PROPERTY_FIXED;

    ecma_object_t *target_func_p;
    target_func_p = ECMA_GET_NON_NULL_POINTER_FROM_POINTER_TAG (ecma_object_t,
                                                                bound_func_p->header.u.bound_function.target_function);

    if (ecma_object_get_class_name (target_func_p) == LIT_MAGIC_STRING_FUNCTION_UL)
    {
      /* The property_name_p argument contains the 'length' string. */
      ecma_value_t get_len_value = ecma_op_object_get (target_func_p, property_name_p);

      JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (get_len_value));
      JERRY_ASSERT (ecma_is_value_integer_number (get_len_value));

      length = (ecma_number_t) (ecma_get_integer_from_value (get_len_value) - (args_length - 1));
    }
#endif /* JERRY_ESNEXT */

    if (length < 0)
    {
      length = 0;
    }

    ecma_property_t *len_prop_p;
    ecma_property_value_t *len_prop_value_p = ecma_create_named_data_property (object_p,
                                                                               property_name_p,
                                                                               length_attributes,
                                                                               &len_prop_p);

    len_prop_value_p->value = ecma_make_number_value (length);
    return len_prop_p;
  }

#if !JERRY_ESNEXT
  if (ecma_compare_ecma_string_to_magic_id (property_name_p, LIT_MAGIC_STRING_CALLER)
      || ecma_compare_ecma_string_to_magic_id (property_name_p, LIT_MAGIC_STRING_ARGUMENTS))
  {
    ecma_object_t *thrower_p = ecma_builtin_get (ECMA_BUILTIN_ID_TYPE_ERROR_THROWER);

    ecma_property_t *caller_prop_p;
    /* The string_p argument contans the name. */
    ecma_create_named_accessor_property (object_p,
                                         property_name_p,
                                         thrower_p,
                                         thrower_p,
                                         ECMA_PROPERTY_FIXED,
                                         &caller_prop_p);
    return caller_prop_p;
  }
#endif /* !JERRY_ESNEXT */

  return NULL;
} /* ecma_op_bound_function_try_to_lazy_instantiate_property */

/**
 * List names of a Function object's lazy instantiated properties,
 * adding them to corresponding string collections
 *
 * See also:
 *          ecma_op_function_try_to_lazy_instantiate_property
 */
void
ecma_op_function_list_lazy_property_names (ecma_object_t *object_p, /**< functionobject */
                                           ecma_collection_t *prop_names_p, /**< prop name collection */
                                           ecma_property_counter_t *prop_counter_p)  /**< prop counter */
{
#if JERRY_ESNEXT
  ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) object_p;
  if (!ECMA_GET_FIRST_BIT_FROM_POINTER_TAG (ext_func_p->u.function.scope_cp))
  {
    /* Unintialized 'length' property is non-enumerable (ECMA-262 v6, 19.2.4.1) */
    ecma_collection_push_back (prop_names_p, ecma_make_magic_string_value (LIT_MAGIC_STRING_LENGTH));
    prop_counter_p->string_named_props++;
  }
#else /* !JERRY_ESNEXT */
  /* 'length' property is non-enumerable (ECMA-262 v5, 13.2.5) */
  ecma_collection_push_back (prop_names_p, ecma_make_magic_string_value (LIT_MAGIC_STRING_LENGTH));
  prop_counter_p->string_named_props++;
#endif /* JERRY_ESNEXT */

  const ecma_compiled_code_t *bytecode_data_p;
  bytecode_data_p = ecma_op_function_get_compiled_code ((ecma_extended_object_t *) object_p);

#if JERRY_ESNEXT
  if (!CBC_FUNCTION_HAS_PROTOTYPE (bytecode_data_p->status_flags))
  {
    return;
  }
#endif /* JERRY_ESNEXT */

  /* 'prototype' property is non-enumerable (ECMA-262 v5, 13.2.18) */
  ecma_collection_push_back (prop_names_p, ecma_make_magic_string_value (LIT_MAGIC_STRING_PROTOTYPE));
  prop_counter_p->string_named_props++;

#if JERRY_ESNEXT
  bool append_caller_and_arguments = !(bytecode_data_p->status_flags & CBC_CODE_FLAGS_STRICT_MODE);
#else /* !JERRY_ESNEXT */
  bool append_caller_and_arguments = (bytecode_data_p->status_flags & CBC_CODE_FLAGS_STRICT_MODE);
#endif /* JERRY_ESNEXT */

  if (append_caller_and_arguments)
  {
    /* 'caller' property is non-enumerable (ECMA-262 v5, 13.2.5) */
    ecma_collection_push_back (prop_names_p, ecma_make_magic_string_value (LIT_MAGIC_STRING_CALLER));

    /* 'arguments' property is non-enumerable (ECMA-262 v5, 13.2.5) */
    ecma_collection_push_back (prop_names_p, ecma_make_magic_string_value (LIT_MAGIC_STRING_ARGUMENTS));

    prop_counter_p->string_named_props += 2;
  }
} /* ecma_op_function_list_lazy_property_names */

/**
 * List names of an External Function object's lazy instantiated properties,
 * adding them to corresponding string collections
 *
 * See also:
 *          ecma_op_external_function_try_to_lazy_instantiate_property
 */
void
ecma_op_external_function_list_lazy_property_names (ecma_object_t *object_p, /**< function object */
                                                    ecma_collection_t *prop_names_p, /**< prop name collection */
                                                    ecma_property_counter_t *prop_counter_p)  /**< prop counter */
{
#if !JERRY_ESNEXT
  JERRY_UNUSED (object_p);
#else /* JERRY_ESNEXT */
  if (!ecma_op_ordinary_object_has_own_property (object_p, ecma_get_magic_string (LIT_MAGIC_STRING_PROTOTYPE)))
#endif /* !JERRY_ESNEXT */
  {
    /* 'prototype' property is non-enumerable (ECMA-262 v5, 13.2.18) */
    ecma_collection_push_back (prop_names_p, ecma_make_magic_string_value (LIT_MAGIC_STRING_PROTOTYPE));
    prop_counter_p->string_named_props++;
  }
} /* ecma_op_external_function_list_lazy_property_names */

/**
 * List names of a Bound Function object's lazy instantiated properties,
 * adding them to corresponding string collections
 *
 * See also:
 *          ecma_op_bound_function_try_to_lazy_instantiate_property
 */
void
ecma_op_bound_function_list_lazy_property_names (ecma_object_t *object_p, /**< bound function object*/
                                                 ecma_collection_t *prop_names_p, /**< prop name collection */
                                                 ecma_property_counter_t *prop_counter_p)  /**< prop counter */
{
#if JERRY_ESNEXT
  /* Unintialized 'length' property is non-enumerable (ECMA-262 v6, 19.2.4.1) */
  ecma_bound_function_t *bound_func_p = (ecma_bound_function_t *) object_p;
  if (!ECMA_GET_FIRST_BIT_FROM_POINTER_TAG (bound_func_p->header.u.bound_function.target_function))
  {
    ecma_collection_push_back (prop_names_p, ecma_make_magic_string_value (LIT_MAGIC_STRING_LENGTH));
    prop_counter_p->string_named_props++;
  }
#else /* !JERRY_ESNEXT */
  JERRY_UNUSED (object_p);
  /* 'length' property is non-enumerable (ECMA-262 v5, 13.2.5) */
  ecma_collection_push_back (prop_names_p, ecma_make_magic_string_value (LIT_MAGIC_STRING_LENGTH));
  prop_counter_p->string_named_props++;
#endif /* JERRY_ESNEXT */

  /* 'caller' property is non-enumerable (ECMA-262 v5, 13.2.5) */
  ecma_collection_push_back (prop_names_p, ecma_make_magic_string_value (LIT_MAGIC_STRING_CALLER));

  /* 'arguments' property is non-enumerable (ECMA-262 v5, 13.2.5) */
  ecma_collection_push_back (prop_names_p, ecma_make_magic_string_value (LIT_MAGIC_STRING_ARGUMENTS));

  prop_counter_p->string_named_props += 2;
} /* ecma_op_bound_function_list_lazy_property_names */

/**
 * @}
 * @}
 */

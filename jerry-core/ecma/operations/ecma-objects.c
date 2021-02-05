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

#include "ecma-objects.h"
#include "ecma-arguments-object.h"
#include "ecma-array-object.h"
#include "ecma-builtin-helpers.h"
#include "ecma-builtins.h"
#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-lcache.h"
#include "ecma-lex-env.h"
#include "ecma-objects-general.h"
#include "ecma-proxy-object.h"
#include "ecma-string-object.h"
#include "jcontext.h"

#if JERRY_BUILTIN_TYPEDARRAY
#include "ecma-arraybuffer-object.h"
#include "ecma-typedarray-object.h"
#endif /* JERRY_BUILTIN_TYPEDARRAY */

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmaobjectsinternalops ECMA objects' operations
 * @{
 */

/**
 * Hash bitmap size for ecma objects
 */
#define ECMA_OBJECT_HASH_BITMAP_SIZE 256

/**
 * Assert that specified object type value is valid
 *
 * @param type object's implementation-defined type
 */
#ifndef JERRY_NDEBUG
#define JERRY_ASSERT_OBJECT_TYPE_IS_VALID(type) JERRY_ASSERT (type < ECMA_OBJECT_TYPE__MAX);
#else /* JERRY_NDEBUG */
#define JERRY_ASSERT_OBJECT_TYPE_IS_VALID(type)
#endif /* !JERRY_NDEBUG */

/**
 * [[GetOwnProperty]] ecma object's operation
 *
 * See also:
 *          ECMA-262 v5, 8.6.2; ECMA-262 v5, Table 8
 *
 * @return pointer to a property - if it exists,
 *         NULL (i.e. ecma-undefined) - otherwise.
 */
ecma_property_t
ecma_op_object_get_own_property (ecma_object_t *object_p, /**< the object */
                                 ecma_string_t *property_name_p, /**< property name */
                                 ecma_property_ref_t *property_ref_p, /**< property reference */
                                 uint32_t options) /**< option bits */
{
  JERRY_ASSERT (object_p != NULL && !ecma_is_lexical_environment (object_p));
#if JERRY_BUILTIN_PROXY
  JERRY_ASSERT (!ECMA_OBJECT_IS_PROXY (object_p));
#endif /* JERRY_BUILTIN_PROXY */
  JERRY_ASSERT (property_name_p != NULL);
  JERRY_ASSERT (options == ECMA_PROPERTY_GET_NO_OPTIONS || property_ref_p != NULL);

  ecma_object_type_t type = ecma_get_object_type (object_p);

  switch (type)
  {
    case ECMA_OBJECT_TYPE_CLASS:
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      if (ext_object_p->u.class_prop.class_id == LIT_MAGIC_STRING_STRING_UL)
      {
        if (ecma_string_is_length (property_name_p))
        {
          if (options & ECMA_PROPERTY_GET_VALUE)
          {
            ecma_value_t prim_value_p = ext_object_p->u.class_prop.u.value;
            ecma_string_t *prim_value_str_p = ecma_get_string_from_value (prim_value_p);

            lit_utf8_size_t length = ecma_string_get_length (prim_value_str_p);
            property_ref_p->virtual_value = ecma_make_uint32_value (length);
          }

          return ECMA_PROPERTY_VIRTUAL;
        }

        uint32_t index = ecma_string_get_array_index (property_name_p);

        if (index != ECMA_STRING_NOT_ARRAY_INDEX)
        {
          ecma_value_t prim_value_p = ext_object_p->u.class_prop.u.value;
          ecma_string_t *prim_value_str_p = ecma_get_string_from_value (prim_value_p);

          if (index < ecma_string_get_length (prim_value_str_p))
          {
            if (options & ECMA_PROPERTY_GET_VALUE)
            {
              ecma_char_t char_at_idx = ecma_string_get_char_at_pos (prim_value_str_p, index);
              ecma_string_t *char_str_p = ecma_new_ecma_string_from_code_unit (char_at_idx);
              property_ref_p->virtual_value = ecma_make_string_value (char_str_p);
            }

            return ECMA_PROPERTY_FLAG_ENUMERABLE | ECMA_PROPERTY_VIRTUAL;
          }
        }
      }
      break;
    }
    case ECMA_OBJECT_TYPE_ARRAY:
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      if (ecma_string_is_length (property_name_p))
      {
        if (options & ECMA_PROPERTY_GET_VALUE)
        {
          property_ref_p->virtual_value = ecma_make_uint32_value (ext_object_p->u.array.length);
        }

        uint32_t length_prop = ext_object_p->u.array.length_prop_and_hole_count;
        return length_prop & (ECMA_PROPERTY_FLAG_WRITABLE | ECMA_PROPERTY_VIRTUAL);
      }

      if (ecma_op_array_is_fast_array (ext_object_p))
      {
        uint32_t index = ecma_string_get_array_index (property_name_p);

        if (index != ECMA_STRING_NOT_ARRAY_INDEX)
        {
          if (JERRY_LIKELY (index < ext_object_p->u.array.length))
          {
            ecma_value_t *values_p = ECMA_GET_NON_NULL_POINTER (ecma_value_t, object_p->u1.property_list_cp);

            if (ecma_is_value_array_hole (values_p[index]))
            {
              return ECMA_PROPERTY_TYPE_NOT_FOUND;
            }

            if (options & ECMA_PROPERTY_GET_VALUE)
            {
              property_ref_p->virtual_value = ecma_fast_copy_value (values_p[index]);
            }

            return ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE | ECMA_PROPERTY_VIRTUAL;
          }
        }

        return ECMA_PROPERTY_TYPE_NOT_FOUND;
      }

      break;
    }
#if JERRY_BUILTIN_TYPEDARRAY
    case ECMA_OBJECT_TYPE_PSEUDO_ARRAY:
    {
      /* ES2015 9.4.5.1 */
      if (ecma_object_is_typedarray (object_p))
      {
        if (ecma_prop_name_is_symbol (property_name_p))
        {
          break;
        }

        uint32_t array_index = ecma_string_get_array_index (property_name_p);

        if (array_index != ECMA_STRING_NOT_ARRAY_INDEX)
        {
          ecma_typedarray_info_t info = ecma_typedarray_get_info (object_p);
          ecma_value_t value = ECMA_VALUE_UNDEFINED;

          if (array_index < info.length)
          {
            uint32_t byte_pos = array_index << info.shift;
            value = ecma_get_typedarray_element (info.buffer_p + byte_pos, info.id);
          }

          if (!ecma_is_value_undefined (value))
          {
            if (options & ECMA_PROPERTY_GET_VALUE)
            {
              property_ref_p->virtual_value = value;
            }
            else
            {
              ecma_fast_free_value (value);
            }

            return ECMA_PROPERTY_ENUMERABLE_WRITABLE | ECMA_PROPERTY_VIRTUAL;
          }

          return ECMA_PROPERTY_TYPE_NOT_FOUND_AND_STOP;
        }

        ecma_number_t num = ecma_string_to_number (property_name_p);
        ecma_string_t *num_to_str = ecma_new_ecma_string_from_number (num);

        if (ecma_compare_ecma_strings (property_name_p, num_to_str))
        {
          ecma_deref_ecma_string (num_to_str);

          return ECMA_PROPERTY_TYPE_NOT_FOUND_AND_STOP;
        }

        ecma_deref_ecma_string (num_to_str);
      }

      break;
    }
#endif /* JERRY_BUILTIN_TYPEDARRAY */
    default:
    {
      break;
    }
  }

  ecma_property_t *property_p = ecma_find_named_property (object_p, property_name_p);

  if (property_p == NULL)
  {
    if (ecma_get_object_is_builtin (object_p))
    {
      if (type == ECMA_OBJECT_TYPE_NATIVE_FUNCTION && ecma_builtin_function_is_routine (object_p))
      {
        property_p = ecma_builtin_routine_try_to_instantiate_property (object_p, property_name_p);
      }
      else
      {
        property_p = ecma_builtin_try_to_instantiate_property (object_p, property_name_p);
      }
    }
    else
    {
      switch (type)
      {
        case ECMA_OBJECT_TYPE_FUNCTION:
        {
#if !JERRY_ESNEXT
          if (ecma_string_is_length (property_name_p))
          {
            if (options & ECMA_PROPERTY_GET_VALUE)
            {
              /* Get length virtual property. */
              ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) object_p;
              const ecma_compiled_code_t *bytecode_data_p = ecma_op_function_get_compiled_code (ext_func_p);

              uint32_t len;
              if (bytecode_data_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
              {
                cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) bytecode_data_p;
                len = args_p->argument_end;
              }
              else
              {
                cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) bytecode_data_p;
                len = args_p->argument_end;
              }

              property_ref_p->virtual_value = ecma_make_uint32_value (len);
            }

            return ECMA_PROPERTY_VIRTUAL;
          }
#endif /* !JERRY_ESNEXT */

          /* Get prototype physical property. */
          property_p = ecma_op_function_try_to_lazy_instantiate_property (object_p, property_name_p);
          break;
        }
        case ECMA_OBJECT_TYPE_NATIVE_FUNCTION:
        {
          property_p = ecma_op_external_function_try_to_lazy_instantiate_property (object_p, property_name_p);
          break;
        }
        case ECMA_OBJECT_TYPE_BOUND_FUNCTION:
        {
          property_p = ecma_op_bound_function_try_to_lazy_instantiate_property (object_p, property_name_p);
          break;
        }
        case ECMA_OBJECT_TYPE_PSEUDO_ARRAY:
        {
          if (((ecma_extended_object_t *) object_p)->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS)
          {
            property_p = ecma_op_arguments_object_try_to_lazy_instantiate_property (object_p, property_name_p);
          }
          break;
        }
        default:
        {
          break;
        }
      }
    }

    if (property_p == NULL)
    {
      return ECMA_PROPERTY_TYPE_NOT_FOUND;
    }
  }
  else if (type == ECMA_OBJECT_TYPE_PSEUDO_ARRAY
           && ((ecma_extended_object_t *) object_p)->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS
           && (((ecma_extended_object_t *) object_p)->u.pseudo_array.extra_info & ECMA_ARGUMENTS_OBJECT_MAPPED))
  {
    ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

    uint32_t index = ecma_string_get_array_index (property_name_p);

    if (index < ext_object_p->u.pseudo_array.u1.formal_params_number)
    {
      ecma_mapped_arguments_t *mapped_arguments_p = (ecma_mapped_arguments_t *) ext_object_p;

      ecma_value_t *argv_p = (ecma_value_t *) (mapped_arguments_p + 1);

      if (!ecma_is_value_empty (argv_p[index]))
      {
#if JERRY_LCACHE
        /* Mapped arguments initialized properties MUST not be lcached */
        if (ecma_is_property_lcached (property_p))
        {
          jmem_cpointer_t prop_name_cp;

          if (JERRY_UNLIKELY (ECMA_IS_DIRECT_STRING (property_name_p)))
          {
            prop_name_cp = (jmem_cpointer_t) ECMA_GET_DIRECT_STRING_VALUE (property_name_p);
          }
          else
          {
            ECMA_SET_NON_NULL_POINTER (prop_name_cp, property_name_p);
          }
          ecma_lcache_invalidate (object_p, prop_name_cp, property_p);
        }
#endif /* JERRY_LCACHE */
        ecma_string_t *name_p = ecma_op_arguments_object_get_formal_parameter (mapped_arguments_p, index);
        ecma_object_t *lex_env_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_object_t, mapped_arguments_p->lex_env);

        ecma_value_t binding_value = ecma_op_get_binding_value (lex_env_p, name_p, true);

        ecma_named_data_property_assign_value (object_p, ECMA_PROPERTY_VALUE_PTR (property_p), binding_value);
        ecma_free_value (binding_value);
      }
    }
  }

  if (options & ECMA_PROPERTY_GET_EXT_REFERENCE)
  {
    ((ecma_extended_property_ref_t *) property_ref_p)->property_p = property_p;
  }

  if (property_ref_p != NULL)
  {
    property_ref_p->value_p = ECMA_PROPERTY_VALUE_PTR (property_p);
  }

  return *property_p;
} /* ecma_op_object_get_own_property */

/**
 * Generic [[HasProperty]] operation
 *
 * See also:
 *          ECMAScript v6, 9.1.7.1
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_{TRUE_FALSE} - whether the property is found
 */
extern inline ecma_value_t JERRY_ATTR_ALWAYS_INLINE
ecma_op_object_has_property (ecma_object_t *object_p, /**< the object */
                             ecma_string_t *property_name_p) /**< property name */
{
  while (true)
  {
#if JERRY_BUILTIN_PROXY
    if (ECMA_OBJECT_IS_PROXY (object_p))
    {
      return ecma_proxy_object_has (object_p, property_name_p);
    }
#endif /* JERRY_BUILTIN_PROXY */

    /* 2 - 3. */
    if (ecma_op_ordinary_object_has_own_property (object_p, property_name_p))
    {
      return ECMA_VALUE_TRUE;
    }

    jmem_cpointer_t proto_cp = ecma_op_ordinary_object_get_prototype_of (object_p);

    /* 7. */
    if (proto_cp == JMEM_CP_NULL)
    {
      return ECMA_VALUE_FALSE;
    }

    object_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, proto_cp);
  }
} /* ecma_op_object_has_property */

/**
 * Search the value corresponding to a property name
 *
 * Note: search includes prototypes
 *
 * @return ecma value if property is found
 *         ECMA_VALUE_NOT_FOUND if property is not found
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_object_find_own (ecma_value_t base_value, /**< base value */
                         ecma_object_t *object_p, /**< target object */
                         ecma_string_t *property_name_p) /**< property name */
{
  JERRY_ASSERT (object_p != NULL && !ecma_is_lexical_environment (object_p));
  JERRY_ASSERT (property_name_p != NULL);
  JERRY_ASSERT (!ECMA_OBJECT_IS_PROXY (object_p));

  ecma_object_type_t type = ecma_get_object_type (object_p);

  switch (type)
  {
    case ECMA_OBJECT_TYPE_CLASS:
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      if (ext_object_p->u.class_prop.class_id == LIT_MAGIC_STRING_STRING_UL)
      {
        if (ecma_string_is_length (property_name_p))
        {
          ecma_value_t prim_value_p = ext_object_p->u.class_prop.u.value;

          ecma_string_t *prim_value_str_p = ecma_get_string_from_value (prim_value_p);
          lit_utf8_size_t length = ecma_string_get_length (prim_value_str_p);

          return ecma_make_uint32_value (length);
        }

        uint32_t index = ecma_string_get_array_index (property_name_p);

        if (index != ECMA_STRING_NOT_ARRAY_INDEX)
        {
          ecma_value_t prim_value_p = ext_object_p->u.class_prop.u.value;

          ecma_string_t *prim_value_str_p = ecma_get_string_from_value (prim_value_p);

          if (index < ecma_string_get_length (prim_value_str_p))
          {
            ecma_char_t char_at_idx = ecma_string_get_char_at_pos (prim_value_str_p, index);
            return ecma_make_string_value (ecma_new_ecma_string_from_code_unit (char_at_idx));
          }
        }
      }

      break;
    }
    case ECMA_OBJECT_TYPE_ARRAY:
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      if (ecma_string_is_length (property_name_p))
      {
        return ecma_make_uint32_value (ext_object_p->u.array.length);
      }

      if (JERRY_LIKELY (ecma_op_array_is_fast_array (ext_object_p)))
      {
        uint32_t index = ecma_string_get_array_index (property_name_p);

        if (JERRY_LIKELY (index != ECMA_STRING_NOT_ARRAY_INDEX))
        {
          if (JERRY_LIKELY (index < ext_object_p->u.array.length))
          {
            ecma_value_t *values_p = ECMA_GET_NON_NULL_POINTER (ecma_value_t, object_p->u1.property_list_cp);

            return (ecma_is_value_array_hole (values_p[index]) ? ECMA_VALUE_NOT_FOUND
                                                               : ecma_fast_copy_value (values_p[index]));
          }
        }
        return ECMA_VALUE_NOT_FOUND;
      }

      break;
    }
    case ECMA_OBJECT_TYPE_PSEUDO_ARRAY:
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      if (ext_object_p->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS
          && ext_object_p->u.pseudo_array.extra_info & ECMA_ARGUMENTS_OBJECT_MAPPED)
      {
        uint32_t index = ecma_string_get_array_index (property_name_p);

        if (index < ext_object_p->u.pseudo_array.u1.formal_params_number)
        {
          ecma_mapped_arguments_t *mapped_arguments_p = (ecma_mapped_arguments_t *) ext_object_p;

          ecma_value_t *argv_p = (ecma_value_t *) (mapped_arguments_p + 1);

          if (!ecma_is_value_empty (argv_p[index]))
          {
            ecma_string_t *name_p = ecma_op_arguments_object_get_formal_parameter (mapped_arguments_p, index);
            ecma_object_t *lex_env_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_object_t, mapped_arguments_p->lex_env);

            return ecma_op_get_binding_value (lex_env_p, name_p, true);
          }
        }
      }
#if JERRY_BUILTIN_TYPEDARRAY
      /* ES2015 9.4.5.4 */
      if (ecma_object_is_typedarray (object_p))
      {
        if (ecma_prop_name_is_symbol (property_name_p))
        {
          break;
        }

        uint32_t array_index = ecma_string_get_array_index (property_name_p);

        if (array_index != ECMA_STRING_NOT_ARRAY_INDEX)
        {
          ecma_typedarray_info_t info = ecma_typedarray_get_info (object_p);

          if (array_index >= info.length)
          {
            return ECMA_VALUE_UNDEFINED;
          }

          uint32_t byte_pos = array_index << info.shift;
          return ecma_get_typedarray_element (info.buffer_p + byte_pos, info.id);
        }

        ecma_number_t num = ecma_string_to_number (property_name_p);
        ecma_string_t *num_to_str = ecma_new_ecma_string_from_number (num);

        if (ecma_compare_ecma_strings (property_name_p, num_to_str))
        {
          ecma_deref_ecma_string (num_to_str);

          return ECMA_VALUE_UNDEFINED;
        }

        ecma_deref_ecma_string (num_to_str);
      }
#endif /* JERRY_BUILTIN_TYPEDARRAY */

      break;
    }
    default:
    {
      break;
    }
  }

  ecma_property_t *property_p = ecma_find_named_property (object_p, property_name_p);

  if (property_p == NULL)
  {
    if (ecma_get_object_is_builtin (object_p))
    {
      if (type == ECMA_OBJECT_TYPE_NATIVE_FUNCTION && ecma_builtin_function_is_routine (object_p))
      {
        property_p = ecma_builtin_routine_try_to_instantiate_property (object_p, property_name_p);
      }
      else
      {
        property_p = ecma_builtin_try_to_instantiate_property (object_p, property_name_p);
      }
    }
    else
    {
      switch (type)
      {
        case ECMA_OBJECT_TYPE_FUNCTION:
        {
#if !JERRY_ESNEXT
          if (ecma_string_is_length (property_name_p))
          {
            /* Get length virtual property. */
            ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) object_p;
            const ecma_compiled_code_t *bytecode_data_p = ecma_op_function_get_compiled_code (ext_func_p);

            uint32_t len;
            if (bytecode_data_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
            {
              cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) bytecode_data_p;
              len = args_p->argument_end;
            }
            else
            {
              cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) bytecode_data_p;
              len = args_p->argument_end;
            }

            return ecma_make_uint32_value (len);
          }
#endif /* !JERRY_ESNEXT */

          /* Get prototype physical property. */
          property_p = ecma_op_function_try_to_lazy_instantiate_property (object_p, property_name_p);
          break;
        }
        case ECMA_OBJECT_TYPE_NATIVE_FUNCTION:
        {
          property_p = ecma_op_external_function_try_to_lazy_instantiate_property (object_p, property_name_p);
          break;
        }
        case ECMA_OBJECT_TYPE_BOUND_FUNCTION:
        {
          property_p = ecma_op_bound_function_try_to_lazy_instantiate_property (object_p, property_name_p);
          break;
        }
        case ECMA_OBJECT_TYPE_PSEUDO_ARRAY:
        {
          if (((ecma_extended_object_t *) object_p)->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS)
          {
            property_p = ecma_op_arguments_object_try_to_lazy_instantiate_property (object_p, property_name_p);
          }
          break;
        }
        default:
        {
          break;
        }
      }
    }

    if (property_p == NULL)
    {
      return ECMA_VALUE_NOT_FOUND;
    }
  }

  JERRY_ASSERT (ECMA_PROPERTY_IS_RAW (*property_p));

  ecma_property_value_t *prop_value_p = ECMA_PROPERTY_VALUE_PTR (property_p);

  if (*property_p & ECMA_PROPERTY_FLAG_DATA)
  {
    return ecma_fast_copy_value (prop_value_p->value);
  }

  ecma_getter_setter_pointers_t *get_set_pair_p = ecma_get_named_accessor_property (prop_value_p);

  if (get_set_pair_p->getter_cp == JMEM_CP_NULL)
  {
    return ECMA_VALUE_UNDEFINED;
  }

  ecma_object_t *getter_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, get_set_pair_p->getter_cp);

  return ecma_op_function_call (getter_p, base_value, NULL, 0);
} /* ecma_op_object_find_own */

/**
 * Search the value corresponding to a property index
 *
 * Note: this method falls back to the general ecma_op_object_find
 *
 * @return ecma value if property is found
 *         ECMA_VALUE_NOT_FOUND if property is not found
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_object_find_by_index (ecma_object_t *object_p, /**< the object */
                              ecma_length_t index) /**< property index */
{
  if (JERRY_LIKELY (index <= ECMA_DIRECT_STRING_MAX_IMM))
  {
    return ecma_op_object_find (object_p, ECMA_CREATE_DIRECT_UINT32_STRING (index));
  }

  ecma_string_t *index_str_p = ecma_new_ecma_string_from_length (index);
  ecma_value_t ret_value = ecma_op_object_find (object_p, index_str_p);
  ecma_deref_ecma_string (index_str_p);

  return ret_value;
} /* ecma_op_object_find_by_index */

/**
 * Search the value corresponding to a property name
 *
 * Note: search includes prototypes
 *
 * @return ecma value if property is found
 *         ECMA_VALUE_NOT_FOUND if property is not found
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_object_find (ecma_object_t *object_p, /**< the object */
                     ecma_string_t *property_name_p) /**< property name */
{
  ecma_value_t base_value = ecma_make_object_value (object_p);

  while (true)
  {
#if JERRY_BUILTIN_PROXY
    if (ECMA_OBJECT_IS_PROXY (object_p))
    {
      return ecma_proxy_object_find (object_p, property_name_p);
    }
#endif /* JERRY_BUILTIN_PROXY */

    ecma_value_t value = ecma_op_object_find_own (base_value, object_p, property_name_p);

    if (ecma_is_value_found (value))
    {
      return value;
    }

    if (object_p->u2.prototype_cp == JMEM_CP_NULL)
    {
      break;
    }

    object_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, object_p->u2.prototype_cp);
  }

  return ECMA_VALUE_NOT_FOUND;
} /* ecma_op_object_find */

/**
 * [[Get]] operation of ecma object
 *
 * This function returns the value of a named property, or undefined
 * if the property is not found in the prototype chain. If the property
 * is an accessor, it calls the "get" callback function and returns
 * with its result (including error throws).
 *
 * See also:
 *          ECMA-262 v5, 8.6.2; ECMA-262 v5, Table 8
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
extern inline ecma_value_t JERRY_ATTR_ALWAYS_INLINE
ecma_op_object_get (ecma_object_t *object_p, /**< the object */
                    ecma_string_t *property_name_p) /**< property name */
{
  return ecma_op_object_get_with_receiver (object_p, property_name_p, ecma_make_object_value (object_p));
} /* ecma_op_object_get */

/**
 * [[Get]] operation of ecma object with the specified receiver
 *
 * This function returns the value of a named property, or undefined
 * if the property is not found in the prototype chain. If the property
 * is an accessor, it calls the "get" callback function and returns
 * with its result (including error throws).
 *
 * See also:
 *          ECMA-262 v5, 8.6.2; ECMA-262 v5, Table 8
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_object_get_with_receiver (ecma_object_t *object_p, /**< the object */
                                  ecma_string_t *property_name_p, /**< property name */
                                  ecma_value_t receiver) /**< receiver to invoke getter function */
{
  while (true)
  {
#if JERRY_BUILTIN_PROXY
    if (ECMA_OBJECT_IS_PROXY (object_p))
    {
      return ecma_proxy_object_get (object_p, property_name_p, receiver);
    }
#endif /* JERRY_BUILTIN_PROXY */

    ecma_value_t value = ecma_op_object_find_own (receiver, object_p, property_name_p);

    if (ecma_is_value_found (value))
    {
      return value;
    }

    jmem_cpointer_t proto_cp = ecma_op_ordinary_object_get_prototype_of (object_p);

    if (proto_cp == JMEM_CP_NULL)
    {
      break;
    }

    object_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, proto_cp);
  }

  return ECMA_VALUE_UNDEFINED;
} /* ecma_op_object_get_with_receiver */

/**
 * [[Get]] operation of ecma object specified for property index
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_object_get_by_index (ecma_object_t *object_p, /**< the object */
                             ecma_length_t index) /**< property index */
{
  if (JERRY_LIKELY (index <= ECMA_DIRECT_STRING_MAX_IMM))
  {
    return ecma_op_object_get (object_p, ECMA_CREATE_DIRECT_UINT32_STRING (index));
  }

  ecma_string_t *index_str_p = ecma_new_ecma_string_from_length (index);
  ecma_value_t ret_value = ecma_op_object_get (object_p, index_str_p);
  ecma_deref_ecma_string (index_str_p);

  return ret_value;
} /* ecma_op_object_get_by_index */

/**
 * Perform ToLength(O.[[Get]]("length")) operation
 *
 * The property is converted to uint32 during the operation
 *
 * @return ECMA_VALUE_ERROR - if there was any error during the operation
 *         ECMA_VALUE_EMPTY - otherwise
 */
ecma_value_t
ecma_op_object_get_length (ecma_object_t *object_p, /**< the object */
                           ecma_length_t *length_p) /**< [out] length value converted to uint32 */
{
  if (JERRY_LIKELY (ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_ARRAY))
  {
    *length_p = (ecma_length_t) ecma_array_get_length (object_p);
    return ECMA_VALUE_EMPTY;
  }

  ecma_value_t len_value = ecma_op_object_get_by_magic_id (object_p, LIT_MAGIC_STRING_LENGTH);
  ecma_value_t len_number = ecma_op_to_length (len_value, length_p);
  ecma_free_value (len_value);

  JERRY_ASSERT (ECMA_IS_VALUE_ERROR (len_number) || ecma_is_value_empty (len_number));

  return len_number;
} /* ecma_op_object_get_length */

/**
 * [[Get]] operation of ecma object where the property name is a magic string
 *
 * This function returns the value of a named property, or undefined
 * if the property is not found in the prototype chain. If the property
 * is an accessor, it calls the "get" callback function and returns
 * with its result (including error throws).
 *
 * See also:
 *          ECMA-262 v5, 8.6.2; ECMA-262 v5, Table 8
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
extern inline ecma_value_t JERRY_ATTR_ALWAYS_INLINE
ecma_op_object_get_by_magic_id (ecma_object_t *object_p, /**< the object */
                                lit_magic_string_id_t property_id) /**< property magic string id */
{
  return ecma_op_object_get (object_p, ecma_get_magic_string (property_id));
} /* ecma_op_object_get_by_magic_id */

#if JERRY_ESNEXT

/**
 * Descriptor string for each global symbol
 */
static const uint16_t ecma_global_symbol_descriptions[] = {
  LIT_MAGIC_STRING_ASYNC_ITERATOR, LIT_MAGIC_STRING_HAS_INSTANCE,  LIT_MAGIC_STRING_IS_CONCAT_SPREADABLE,
  LIT_MAGIC_STRING_ITERATOR,       LIT_MAGIC_STRING_MATCH,         LIT_MAGIC_STRING_REPLACE,
  LIT_MAGIC_STRING_SEARCH,         LIT_MAGIC_STRING_SPECIES,       LIT_MAGIC_STRING_SPLIT,
  LIT_MAGIC_STRING_TO_PRIMITIVE,   LIT_MAGIC_STRING_TO_STRING_TAG, LIT_MAGIC_STRING_UNSCOPABLES,
  LIT_MAGIC_STRING_MATCH_ALL,
};

JERRY_STATIC_ASSERT (sizeof (ecma_global_symbol_descriptions) / sizeof (uint16_t) == ECMA_BUILTIN_GLOBAL_SYMBOL_COUNT,
                     ecma_global_symbol_descriptions_must_have_global_symbol_count_elements);

/**
 * [[Get]] a well-known symbol by the given property id
 *
 * @return pointer to the requested well-known symbol
 */
ecma_string_t *
ecma_op_get_global_symbol (lit_magic_string_id_t property_id) /**< property symbol id */
{
  JERRY_ASSERT (LIT_IS_GLOBAL_SYMBOL (property_id));

  uint32_t symbol_index = (uint32_t) property_id - (uint32_t) LIT_GLOBAL_SYMBOL__FIRST;
  jmem_cpointer_t symbol_cp = JERRY_CONTEXT (global_symbols_cp)[symbol_index];

  if (symbol_cp != JMEM_CP_NULL)
  {
    ecma_string_t *symbol_p = ECMA_GET_NON_NULL_POINTER (ecma_string_t, symbol_cp);
    ecma_ref_ecma_string (symbol_p);
    return symbol_p;
  }

  ecma_string_t *symbol_dot_p = ecma_get_magic_string (LIT_MAGIC_STRING_SYMBOL_DOT_UL);
  uint16_t description = ecma_global_symbol_descriptions[symbol_index];
  ecma_string_t *name_p = ecma_get_magic_string ((lit_magic_string_id_t) description);
  ecma_string_t *descriptor_p = ecma_concat_ecma_strings (symbol_dot_p, name_p);

  ecma_string_t *symbol_p = ecma_new_symbol_from_descriptor_string (ecma_make_string_value (descriptor_p));
  symbol_p->u.hash = (uint16_t) ((property_id << ECMA_GLOBAL_SYMBOL_SHIFT) | ECMA_GLOBAL_SYMBOL_FLAG);

  ECMA_SET_NON_NULL_POINTER (JERRY_CONTEXT (global_symbols_cp)[symbol_index], symbol_p);

  ecma_ref_ecma_string (symbol_p);
  return symbol_p;
} /* ecma_op_get_global_symbol */

/**
 * [[Get]] operation of ecma object where the property is a well-known symbol
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_object_get_by_symbol_id (ecma_object_t *object_p, /**< the object */
                                 lit_magic_string_id_t property_id) /**< property symbol id */
{
  ecma_string_t *symbol_p = ecma_op_get_global_symbol (property_id);
  ecma_value_t ret_value = ecma_op_object_get (object_p, symbol_p);
  ecma_deref_ecma_string (symbol_p);

  return ret_value;
} /* ecma_op_object_get_by_symbol_id */

/**
 * GetMethod operation
 *
 * See also: ECMA-262 v6, 7.3.9
 *
 * Note:
 *      Returned value must be freed with ecma_free_value.
 *
 * @return iterator function object - if success
 *         raised error - otherwise
 */
static ecma_value_t
ecma_op_get_method (ecma_value_t value, /**< ecma value */
                    ecma_string_t *prop_name_p) /** property name */
{
  /* 2. */
  ecma_value_t obj_value = ecma_op_to_object (value);

  if (ECMA_IS_VALUE_ERROR (obj_value))
  {
    return obj_value;
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (obj_value);
  ecma_value_t func;

  func = ecma_op_object_get (obj_p, prop_name_p);
  ecma_deref_object (obj_p);

  /* 3. */
  if (ECMA_IS_VALUE_ERROR (func))
  {
    return func;
  }

  /* 4. */
  if (ecma_is_value_undefined (func) || ecma_is_value_null (func))
  {
    return ECMA_VALUE_UNDEFINED;
  }

  /* 5. */
  if (!ecma_op_is_callable (func))
  {
    ecma_free_value (func);
    return ecma_raise_type_error (ECMA_ERR_MSG ("Iterator is not callable"));
  }

  /* 6. */
  return func;
} /* ecma_op_get_method */

/**
 * GetMethod operation when the property is a well-known symbol
 *
 * See also: ECMA-262 v6, 7.3.9
 *
 * Note:
 *      Returned value must be freed with ecma_free_value.
 *
 * @return iterator function object - if success
 *         raised error - otherwise
 */
ecma_value_t
ecma_op_get_method_by_symbol_id (ecma_value_t value, /**< ecma value */
                                 lit_magic_string_id_t symbol_id) /**< property symbol id */
{
  ecma_string_t *prop_name_p = ecma_op_get_global_symbol (symbol_id);
  ecma_value_t ret_value = ecma_op_get_method (value, prop_name_p);
  ecma_deref_ecma_string (prop_name_p);

  return ret_value;
} /* ecma_op_get_method_by_symbol_id */

/**
 * GetMethod operation when the property is a magic string
 *
 * See also: ECMA-262 v6, 7.3.9
 *
 * Note:
 *      Returned value must be freed with ecma_free_value.
 *
 * @return iterator function object - if success
 *         raised error - otherwise
 */
ecma_value_t
ecma_op_get_method_by_magic_id (ecma_value_t value, /**< ecma value */
                                lit_magic_string_id_t magic_id) /**< property magic id */
{
  return ecma_op_get_method (value, ecma_get_magic_string (magic_id));
} /* ecma_op_get_method_by_magic_id */
#endif /* JERRY_ESNEXT */

/**
 * [[Put]] ecma general object's operation specialized for property index
 *
 * Note: This function falls back to the general ecma_op_object_put
 *
 * @return ecma value
 *         The returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_op_object_put_by_index (ecma_object_t *object_p, /**< the object */
                             ecma_length_t index, /**< property index */
                             ecma_value_t value, /**< ecma value */
                             bool is_throw) /**< flag that controls failure handling */
{
  if (JERRY_LIKELY (index <= ECMA_DIRECT_STRING_MAX_IMM))
  {
    return ecma_op_object_put (object_p, ECMA_CREATE_DIRECT_UINT32_STRING (index), value, is_throw);
  }

  ecma_string_t *index_str_p = ecma_new_ecma_string_from_length (index);
  ecma_value_t ret_value = ecma_op_object_put (object_p, index_str_p, value, is_throw);
  ecma_deref_ecma_string (index_str_p);

  return ret_value;
} /* ecma_op_object_put_by_index */

/**
 * [[Put]] ecma general object's operation
 *
 * See also:
 *          ECMA-262 v5, 8.6.2; ECMA-262 v5, Table 8
 *          ECMA-262 v5, 8.12.5
 *          Also incorporates [[CanPut]] ECMA-262 v5, 8.12.4
 *
 * @return ecma value
 *         The returned value must be freed with ecma_free_value.
 *
 *         Returns with ECMA_VALUE_TRUE if the operation is
 *         successful. Otherwise it returns with an error object
 *         or ECMA_VALUE_FALSE.
 *
 *         Note: even if is_throw is false, the setter can throw an
 *         error, and this function returns with that error.
 */
extern inline ecma_value_t JERRY_ATTR_ALWAYS_INLINE
ecma_op_object_put (ecma_object_t *object_p, /**< the object */
                    ecma_string_t *property_name_p, /**< property name */
                    ecma_value_t value, /**< ecma value */
                    bool is_throw) /**< flag that controls failure handling */
{
  return ecma_op_object_put_with_receiver (object_p,
                                           property_name_p,
                                           value,
                                           ecma_make_object_value (object_p),
                                           is_throw);
} /* ecma_op_object_put */

#if JERRY_ESNEXT
/**
 * [[Set]] ( P, V, Receiver) operation part for ordinary objects
 *
 * See also: ECMAScript v6, 9.19.9
 *
 * @return ecma value
 *         The returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_op_object_put_apply_receiver (ecma_value_t receiver, /**< receiver */
                                   ecma_string_t *property_name_p, /**< property name */
                                   ecma_value_t value, /**< value to set */
                                   bool is_throw) /**< flag that controls failure handling */
{
  /* 5.b */
  if (!ecma_is_value_object (receiver))
  {
    return ecma_reject (is_throw);
  }

  ecma_object_t *receiver_obj_p = ecma_get_object_from_value (receiver);

  ecma_property_descriptor_t prop_desc;
  /* 5.c */
  ecma_value_t status = ecma_op_object_get_own_property_descriptor (receiver_obj_p, property_name_p, &prop_desc);

  /* 5.d */
  if (ECMA_IS_VALUE_ERROR (status))
  {
    return status;
  }

  /* 5.e */
  if (ecma_is_value_true (status))
  {
    ecma_value_t result;

    /* 5.e.i - 5.e.ii */
    if (prop_desc.flags & (ECMA_PROP_IS_GET_DEFINED | ECMA_PROP_IS_SET_DEFINED)
        || !(prop_desc.flags & ECMA_PROP_IS_WRITABLE))
    {
      result = ecma_reject (is_throw);
    }
    else
    {
      /* 5.e.iii */
      JERRY_ASSERT (prop_desc.flags & ECMA_PROP_IS_VALUE_DEFINED);
      ecma_free_value (prop_desc.value);
      prop_desc.value = ecma_copy_value (value);

      /* 5.e.iv */
      result = ecma_op_object_define_own_property (receiver_obj_p, property_name_p, &prop_desc);

      if (JERRY_UNLIKELY (ecma_is_value_false (result)) && is_throw)
      {
        result = ecma_raise_type_error (ECMA_ERR_MSG ("Proxy trap returned falsish"));
      }
    }

    ecma_free_property_descriptor (&prop_desc);

    return result;
  }

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_IS_PROXY (receiver_obj_p))
  {
    ecma_property_descriptor_t desc;
    /* Based on: ES6 9.1.9 [[Set]] 4.d.i. / ES11 9.1.9.2 OrdinarySetWithOwnDescriptor 2.c.i. */
    desc.flags = (ECMA_PROP_IS_CONFIGURABLE | ECMA_PROP_IS_CONFIGURABLE_DEFINED | ECMA_PROP_IS_ENUMERABLE
                  | ECMA_PROP_IS_ENUMERABLE_DEFINED | ECMA_PROP_IS_WRITABLE | ECMA_PROP_IS_WRITABLE_DEFINED
                  | ECMA_PROP_IS_VALUE_DEFINED);
    desc.value = value;
    ecma_value_t ret_value = ecma_proxy_object_define_own_property (receiver_obj_p, property_name_p, &desc);

    if (JERRY_UNLIKELY (ecma_is_value_false (ret_value)) && is_throw)
    {
      ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Proxy trap returned falsish"));
    }

    return ret_value;
  }
#endif /* JERRY_BUILTIN_PROXY */

  if (JERRY_UNLIKELY (ecma_op_object_is_fast_array (receiver_obj_p)))
  {
    ecma_fast_array_convert_to_normal (receiver_obj_p);
  }

  /* 5.f.i */
  ecma_property_value_t *new_prop_value_p =
    ecma_create_named_data_property (receiver_obj_p,
                                     property_name_p,
                                     ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE,
                                     NULL);
  JERRY_ASSERT (ecma_is_value_undefined (new_prop_value_p->value));
  new_prop_value_p->value = ecma_copy_value_if_not_object (value);

  return ECMA_VALUE_TRUE;
} /* ecma_op_object_put_apply_receiver */
#endif /* JERRY_ESNEXT */

/**
 * [[Put]] ecma general object's operation with given receiver
 *
 * See also:
 *          ECMA-262 v5, 8.6.2; ECMA-262 v5, Table 8
 *          ECMA-262 v5, 8.12.5
 *          ECMA-262 v6, 9.1.9
 *          Also incorporates [[CanPut]] ECMA-262 v5, 8.12.4
 *
 * @return ecma value
 *         The returned value must be freed with ecma_free_value.
 *
 *         Returns with ECMA_VALUE_TRUE if the operation is
 *         successful. Otherwise it returns with an error object
 *         or ECMA_VALUE_FALSE.
 *
 *         Note: even if is_throw is false, the setter can throw an
 *         error, and this function returns with that error.
 */
ecma_value_t
ecma_op_object_put_with_receiver (ecma_object_t *object_p, /**< the object */
                                  ecma_string_t *property_name_p, /**< property name */
                                  ecma_value_t value, /**< ecma value */
                                  ecma_value_t receiver, /**< receiver */
                                  bool is_throw) /**< flag that controls failure handling */
{
  JERRY_ASSERT (object_p != NULL && !ecma_is_lexical_environment (object_p));
  JERRY_ASSERT (property_name_p != NULL);

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_IS_PROXY (object_p))
  {
    return ecma_proxy_object_set (object_p, property_name_p, value, receiver, is_throw);
  }
#endif /* JERRY_BUILTIN_PROXY */

  ecma_object_type_t type = ecma_get_object_type (object_p);

  switch (type)
  {
    case ECMA_OBJECT_TYPE_ARRAY:
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      if (ecma_string_is_length (property_name_p))
      {
        if (ecma_is_property_writable ((ecma_property_t) ext_object_p->u.array.length_prop_and_hole_count))
        {
          return ecma_op_array_object_set_length (object_p, value, 0);
        }

        return ecma_reject (is_throw);
      }

      if (JERRY_LIKELY (ecma_op_array_is_fast_array (ext_object_p)))
      {
        uint32_t index = ecma_string_get_array_index (property_name_p);

        if (JERRY_UNLIKELY (index == ECMA_STRING_NOT_ARRAY_INDEX))
        {
          ecma_fast_array_convert_to_normal (object_p);
        }
        else if (ecma_fast_array_set_property (object_p, index, value))
        {
          return ECMA_VALUE_TRUE;
        }
      }

      JERRY_ASSERT (!ecma_op_object_is_fast_array (object_p));

      break;
    }
    case ECMA_OBJECT_TYPE_PSEUDO_ARRAY:
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      if (ext_object_p->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS
          && (ext_object_p->u.pseudo_array.extra_info & ECMA_ARGUMENTS_OBJECT_MAPPED))
      {
        uint32_t index = ecma_string_get_array_index (property_name_p);

        if (index < ext_object_p->u.pseudo_array.u1.formal_params_number)
        {
          ecma_mapped_arguments_t *mapped_arguments_p = (ecma_mapped_arguments_t *) ext_object_p;

          ecma_value_t *argv_p = (ecma_value_t *) (mapped_arguments_p + 1);

          if (!ecma_is_value_empty (argv_p[index]))
          {
            ecma_string_t *name_p = ecma_op_arguments_object_get_formal_parameter (mapped_arguments_p, index);
            ecma_object_t *lex_env_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_object_t, mapped_arguments_p->lex_env);
            ecma_op_set_mutable_binding (lex_env_p, name_p, value, true);
            return ECMA_VALUE_TRUE;
          }
        }
      }
#if JERRY_BUILTIN_TYPEDARRAY
      /* ES2015 9.4.5.5 */
      if (ecma_object_is_typedarray (object_p))
      {
        if (ecma_prop_name_is_symbol (property_name_p))
        {
          break;
        }

        uint32_t array_index = ecma_string_get_array_index (property_name_p);

        if (array_index != ECMA_STRING_NOT_ARRAY_INDEX)
        {
          ecma_typedarray_info_t info = ecma_typedarray_get_info (object_p);

          if (array_index >= info.length)
          {
            return ECMA_VALUE_FALSE;
          }

          uint32_t byte_pos = array_index << info.shift;
          return ecma_set_typedarray_element (info.buffer_p + byte_pos, value, info.id);
        }

        ecma_number_t num = ecma_string_to_number (property_name_p);
        ecma_string_t *num_to_str = ecma_new_ecma_string_from_number (num);
        bool is_same = ecma_compare_ecma_strings (property_name_p, num_to_str);
        ecma_deref_ecma_string (num_to_str);

        if (is_same)
        {
          return ECMA_VALUE_FALSE;
        }
      }
#endif /* JERRY_BUILTIN_TYPEDARRAY */
      break;
    }
    default:
    {
      break;
    }
  }

  ecma_property_t *property_p = ecma_find_named_property (object_p, property_name_p);

  if (property_p == NULL)
  {
    if (ecma_get_object_is_builtin (object_p))
    {
      if (type == ECMA_OBJECT_TYPE_NATIVE_FUNCTION && ecma_builtin_function_is_routine (object_p))
      {
        property_p = ecma_builtin_routine_try_to_instantiate_property (object_p, property_name_p);
      }
      else
      {
        property_p = ecma_builtin_try_to_instantiate_property (object_p, property_name_p);
      }
    }
    else
    {
      switch (type)
      {
        case ECMA_OBJECT_TYPE_CLASS:
        {
          ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

          if (ext_object_p->u.class_prop.class_id == LIT_MAGIC_STRING_STRING_UL)
          {
            uint32_t index = ecma_string_get_array_index (property_name_p);

            if (index != ECMA_STRING_NOT_ARRAY_INDEX)
            {
              ecma_value_t prim_value_p = ext_object_p->u.class_prop.u.value;
              ecma_string_t *prim_value_str_p = ecma_get_string_from_value (prim_value_p);

              if (index < ecma_string_get_length (prim_value_str_p))
              {
                return ecma_reject (is_throw);
              }
            }
          }
          break;
        }
        case ECMA_OBJECT_TYPE_FUNCTION:
        {
#if JERRY_ESNEXT
          /* Uninitialized 'length' property is non-writable (ECMA-262 v6, 19.2.4.1) */
          if ((ecma_string_is_length (property_name_p))
              && (!ECMA_GET_FIRST_BIT_FROM_POINTER_TAG (((ecma_extended_object_t *) object_p)->u.function.scope_cp)))
          {
            return ecma_reject (is_throw);
          }
#else /* !JERRY_ESNEXT */
          if (ecma_string_is_length (property_name_p))
          {
            return ecma_reject (is_throw);
          }
#endif /* JERRY_ESNEXT */

          /* Get prototype physical property. */
          property_p = ecma_op_function_try_to_lazy_instantiate_property (object_p, property_name_p);
          break;
        }
        case ECMA_OBJECT_TYPE_NATIVE_FUNCTION:
        {
          property_p = ecma_op_external_function_try_to_lazy_instantiate_property (object_p, property_name_p);
          break;
        }
        case ECMA_OBJECT_TYPE_BOUND_FUNCTION:
        {
          property_p = ecma_op_bound_function_try_to_lazy_instantiate_property (object_p, property_name_p);
          break;
        }
        case ECMA_OBJECT_TYPE_PSEUDO_ARRAY:
        {
          if (((ecma_extended_object_t *) object_p)->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS)
          {
            property_p = ecma_op_arguments_object_try_to_lazy_instantiate_property (object_p, property_name_p);
          }
          break;
        }
        default:
        {
          break;
        }
      }
    }
  }

  jmem_cpointer_t setter_cp = JMEM_CP_NULL;

  if (property_p != NULL)
  {
    JERRY_ASSERT (ECMA_PROPERTY_IS_RAW (*property_p));

    if (*property_p & ECMA_PROPERTY_FLAG_DATA)
    {
      if (ecma_is_property_writable (*property_p))
      {
#if JERRY_ESNEXT
        if (ecma_make_object_value (object_p) != receiver)
        {
          return ecma_op_object_put_apply_receiver (receiver, property_name_p, value, is_throw);
        }
#endif /* JERRY_ESNEXT */

        /* There is no need for special casing arrays here because changing the
         * value of an existing property never changes the length of an array. */
        ecma_named_data_property_assign_value (object_p, ECMA_PROPERTY_VALUE_PTR (property_p), value);
        return ECMA_VALUE_TRUE;
      }
    }
    else
    {
      ecma_getter_setter_pointers_t *get_set_pair_p =
        ecma_get_named_accessor_property (ECMA_PROPERTY_VALUE_PTR (property_p));
      setter_cp = get_set_pair_p->setter_cp;
    }
  }
  else
  {
    bool create_new_property = true;

    jmem_cpointer_t obj_cp;
    ECMA_SET_NON_NULL_POINTER (obj_cp, object_p);
    ecma_object_t *proto_p = object_p;

    while (true)
    {
      obj_cp = ecma_op_ordinary_object_get_prototype_of (proto_p);

      if (obj_cp == JMEM_CP_NULL)
      {
        break;
      }

      ecma_property_ref_t property_ref = { NULL };
      proto_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, obj_cp);

#if JERRY_BUILTIN_PROXY
      if (ECMA_OBJECT_IS_PROXY (proto_p))
      {
        return ecma_op_object_put_with_receiver (proto_p, property_name_p, value, receiver, is_throw);
      }
#endif /* JERRY_BUILTIN_PROXY */

      ecma_property_t inherited_property =
        ecma_op_object_get_own_property (proto_p, property_name_p, &property_ref, ECMA_PROPERTY_GET_NO_OPTIONS);

      if (inherited_property != ECMA_PROPERTY_TYPE_NOT_FOUND
          && inherited_property != ECMA_PROPERTY_TYPE_NOT_FOUND_AND_STOP)
      {
        JERRY_ASSERT (ECMA_PROPERTY_IS_NAMED_PROPERTY (inherited_property));

        if (!(inherited_property & ECMA_PROPERTY_FLAG_DATA))
        {
          setter_cp = ecma_get_named_accessor_property (property_ref.value_p)->setter_cp;
          create_new_property = false;
          break;
        }

        create_new_property = ecma_is_property_writable (inherited_property);
        break;
      }
    }

#if JERRY_BUILTIN_PROXY
    if (create_new_property && ecma_is_value_object (receiver)
        && ECMA_OBJECT_IS_PROXY (ecma_get_object_from_value (receiver)))
    {
      return ecma_op_object_put_apply_receiver (receiver, property_name_p, value, is_throw);
    }
#endif /* JERRY_BUILTIN_PROXY */

    if (create_new_property && ecma_op_ordinary_object_is_extensible (object_p))
    {
      const ecma_object_type_t obj_type = ecma_get_object_type (object_p);

      if (obj_type == ECMA_OBJECT_TYPE_PSEUDO_ARRAY)
      {
        ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

        if (ext_object_p->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS
            && ext_object_p->u.pseudo_array.extra_info & ECMA_ARGUMENTS_OBJECT_MAPPED)
        {
          return ecma_builtin_helper_def_prop (object_p,
                                               property_name_p,
                                               value,
                                               ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE | ECMA_IS_THROW);
        }
      }

      uint32_t index = ecma_string_get_array_index (property_name_p);

      if (obj_type == ECMA_OBJECT_TYPE_ARRAY && index != ECMA_STRING_NOT_ARRAY_INDEX)
      {
        ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

        if (index < UINT32_MAX && index >= ext_object_p->u.array.length)
        {
          if (!ecma_is_property_writable ((ecma_property_t) ext_object_p->u.array.length_prop_and_hole_count))
          {
            return ecma_reject (is_throw);
          }

          ext_object_p->u.array.length = index + 1;
        }
      }

#if JERRY_ESNEXT
      return ecma_op_object_put_apply_receiver (receiver, property_name_p, value, is_throw);
#endif /* JERRY_ESNEXT */

      ecma_property_value_t *new_prop_value_p =
        ecma_create_named_data_property (object_p,
                                         property_name_p,
                                         ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE,
                                         NULL);

      JERRY_ASSERT (ecma_is_value_undefined (new_prop_value_p->value));
      new_prop_value_p->value = ecma_copy_value_if_not_object (value);
      return ECMA_VALUE_TRUE;
    }
  }

  if (setter_cp == JMEM_CP_NULL)
  {
    return ecma_reject (is_throw);
  }

  ecma_value_t ret_value =
    ecma_op_function_call (ECMA_GET_NON_NULL_POINTER (ecma_object_t, setter_cp), receiver, &value, 1);

  if (!ECMA_IS_VALUE_ERROR (ret_value))
  {
    ecma_fast_free_value (ret_value);
    ret_value = ECMA_VALUE_TRUE;
  }

  return ret_value;
} /* ecma_op_object_put_with_receiver */

/**
 * [[Delete]] ecma object's operation specialized for property index
 *
 * Note:
 *      This method falls back to the general ecma_op_object_delete
 *
 * @return true - if deleted successfully
 *         false - or type error otherwise (based in 'is_throw')
 */
ecma_value_t
ecma_op_object_delete_by_index (ecma_object_t *obj_p, /**< the object */
                                ecma_length_t index, /**< property index */
                                bool is_throw) /**< flag that controls failure handling */
{
  if (JERRY_LIKELY (index <= ECMA_DIRECT_STRING_MAX_IMM))
  {
    return ecma_op_object_delete (obj_p, ECMA_CREATE_DIRECT_UINT32_STRING (index), is_throw);
    ;
  }

  ecma_string_t *index_str_p = ecma_new_ecma_string_from_length (index);
  ecma_value_t ret_value = ecma_op_object_delete (obj_p, index_str_p, is_throw);
  ecma_deref_ecma_string (index_str_p);

  return ret_value;
} /* ecma_op_object_delete_by_index */

/**
 * [[Delete]] ecma object's operation
 *
 * See also:
 *          ECMA-262 v5, 8.6.2; ECMA-262 v5, Table 8
 *
 * Note:
 *      returned value must be freed with ecma_free_value
 *
 * @return true - if deleted successfully
 *         false - or type error otherwise (based in 'is_throw')
 */
ecma_value_t
ecma_op_object_delete (ecma_object_t *obj_p, /**< the object */
                       ecma_string_t *property_name_p, /**< property name */
                       bool is_strict) /**< flag that controls failure handling */
{
  JERRY_ASSERT (obj_p != NULL && !ecma_is_lexical_environment (obj_p));
  JERRY_ASSERT (property_name_p != NULL);

  if (ecma_get_object_type (obj_p) == ECMA_OBJECT_TYPE_PSEUDO_ARRAY)
  {
    ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) obj_p;

    if (ext_object_p->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS)
    {
      return ecma_op_arguments_object_delete (obj_p, property_name_p, is_strict);
    }
  }

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_IS_PROXY (obj_p))
  {
    return ecma_proxy_object_delete_property (obj_p, property_name_p, is_strict);
  }
#endif /* JERRY_BUILTIN_PROXY */

  JERRY_ASSERT_OBJECT_TYPE_IS_VALID (ecma_get_object_type (obj_p));

  return ecma_op_general_object_delete (obj_p, property_name_p, is_strict);
} /* ecma_op_object_delete */

/**
 * [[DefaultValue]] ecma object's operation
 *
 * See also:
 *          ECMA-262 v5, 8.6.2; ECMA-262 v5, Table 8
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_object_default_value (ecma_object_t *obj_p, /**< the object */
                              ecma_preferred_type_hint_t hint) /**< hint on preferred result type */
{
  JERRY_ASSERT (obj_p != NULL && !ecma_is_lexical_environment (obj_p));

  JERRY_ASSERT_OBJECT_TYPE_IS_VALID (ecma_get_object_type (obj_p));

  /*
   * typedef ecma_property_t * (*default_value_ptr_t) (ecma_object_t *, ecma_string_t *);
   * static const default_value_ptr_t default_value [ECMA_OBJECT_TYPE__COUNT] =
   * {
   *   [ECMA_OBJECT_TYPE_GENERAL]           = &ecma_op_general_object_default_value,
   *   [ECMA_OBJECT_TYPE_CLASS]             = &ecma_op_general_object_default_value,
   *   [ECMA_OBJECT_TYPE_FUNCTION]          = &ecma_op_general_object_default_value,
   *   [ECMA_OBJECT_TYPE_NATIVE_FUNCTION]   = &ecma_op_general_object_default_value,
   *   [ECMA_OBJECT_TYPE_ARRAY]             = &ecma_op_general_object_default_value,
   *   [ECMA_OBJECT_TYPE_BOUND_FUNCTION]    = &ecma_op_general_object_default_value,
   *   [ECMA_OBJECT_TYPE_PSEUDO_ARRAY]      = &ecma_op_general_object_default_value
   * };
   *
   * return default_value[type] (obj_p, property_name_p);
   */

  return ecma_op_general_object_default_value (obj_p, hint);
} /* ecma_op_object_default_value */

/**
 * [[DefineOwnProperty]] ecma object's operation
 *
 * See also:
 *          ECMA-262 v5, 8.6.2; ECMA-262 v5, Table 8
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_object_define_own_property (ecma_object_t *obj_p, /**< the object */
                                    ecma_string_t *property_name_p, /**< property name */
                                    const ecma_property_descriptor_t *property_desc_p) /**< property
                                                                                        *   descriptor */
{
  JERRY_ASSERT (obj_p != NULL && !ecma_is_lexical_environment (obj_p));
  JERRY_ASSERT (property_name_p != NULL);

  const ecma_object_type_t type = ecma_get_object_type (obj_p);

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_IS_PROXY (obj_p))
  {
    return ecma_proxy_object_define_own_property (obj_p, property_name_p, property_desc_p);
  }
#endif /* JERRY_BUILTIN_PROXY */

  switch (type)
  {
    case ECMA_OBJECT_TYPE_GENERAL:
    case ECMA_OBJECT_TYPE_CLASS:
    case ECMA_OBJECT_TYPE_FUNCTION:
    case ECMA_OBJECT_TYPE_NATIVE_FUNCTION:
    case ECMA_OBJECT_TYPE_BOUND_FUNCTION:
    {
      return ecma_op_general_object_define_own_property (obj_p, property_name_p, property_desc_p);
    }

    case ECMA_OBJECT_TYPE_ARRAY:
    {
      return ecma_op_array_object_define_own_property (obj_p, property_name_p, property_desc_p);
    }

    default:
    {
      JERRY_ASSERT (type == ECMA_OBJECT_TYPE_PSEUDO_ARRAY);

      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) obj_p;

#if JERRY_BUILTIN_TYPEDARRAY
      if (ext_object_p->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS)
      {
#else /* !JERRY_BUILTIN_TYPEDARRAY */
      JERRY_ASSERT (ext_object_p->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS);
#endif /* JERRY_BUILTIN_TYPEDARRAY */
        return ecma_op_arguments_object_define_own_property (obj_p, property_name_p, property_desc_p);
#if JERRY_BUILTIN_TYPEDARRAY
      }
      /* ES2015 9.4.5.3 */
      if (ecma_object_is_typedarray (obj_p))
      {
        if (ecma_prop_name_is_symbol (property_name_p))
        {
          return ecma_op_general_object_define_own_property (obj_p, property_name_p, property_desc_p);
        }
        uint32_t array_index = ecma_string_get_array_index (property_name_p);

        if (array_index != ECMA_STRING_NOT_ARRAY_INDEX)
        {
          ecma_value_t define_status = ecma_op_typedarray_define_index_prop (obj_p, array_index, property_desc_p);

          if (ECMA_IS_VALUE_ERROR (define_status))
          {
            return define_status;
          }

          if (ecma_is_value_true (define_status))
          {
            return ECMA_VALUE_TRUE;
          }

          return ecma_reject (property_desc_p->flags & ECMA_PROP_IS_THROW);
        }

        ecma_number_t num = ecma_string_to_number (property_name_p);
        ecma_string_t *num_to_str = ecma_new_ecma_string_from_number (num);
        bool is_same = ecma_compare_ecma_strings (property_name_p, num_to_str);
        ecma_deref_ecma_string (num_to_str);

        if (is_same)
        {
          return ecma_reject (property_desc_p->flags & ECMA_PROP_IS_THROW);
        }
      }

      return ecma_op_general_object_define_own_property (obj_p, property_name_p, property_desc_p);
#else /* !JERRY_BUILTIN_TYPEDARRAY */
      break;
#endif /* JERRY_BUILTIN_TYPEDARRAY */
    }
  }
} /* ecma_op_object_define_own_property */

/**
 * Get property descriptor from specified property
 *
 * depending on the property type the following fields are set:
 *   - for named data properties: { [Value], [Writable], [Enumerable], [Configurable] };
 *   - for named accessor properties: { [Get] - if defined,
 *                                      [Set] - if defined,
 *                                      [Enumerable], [Configurable]
 *                                    }.
 *
 * The output property descriptor will always be initialized to an empty descriptor.
 *
 * @return ECMA_VALUE_ERROR - if the Proxy.[[GetOwnProperty]] operation raises error
 *         ECMA_VALUE_{TRUE, FALSE} - if property found or not
 */
ecma_value_t
ecma_op_object_get_own_property_descriptor (ecma_object_t *object_p, /**< the object */
                                            ecma_string_t *property_name_p, /**< property name */
                                            ecma_property_descriptor_t *prop_desc_p) /**< property descriptor */
{
  *prop_desc_p = ecma_make_empty_property_descriptor ();

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_IS_PROXY (object_p))
  {
    return ecma_proxy_object_get_own_property_descriptor (object_p, property_name_p, prop_desc_p);
  }
#endif /* JERRY_BUILTIN_PROXY */

  ecma_property_ref_t property_ref;

  ecma_property_t property =
    ecma_op_object_get_own_property (object_p, property_name_p, &property_ref, ECMA_PROPERTY_GET_VALUE);

  if (property == ECMA_PROPERTY_TYPE_NOT_FOUND || property == ECMA_PROPERTY_TYPE_NOT_FOUND_AND_STOP)
  {
    return ECMA_VALUE_FALSE;
  }

  uint32_t flags = ecma_is_property_enumerable (property) ? ECMA_PROP_IS_ENUMERABLE : ECMA_PROP_NO_OPTS;
  flags |= ecma_is_property_configurable (property) ? ECMA_PROP_IS_CONFIGURABLE : ECMA_PROP_NO_OPTS;

  prop_desc_p->flags = (uint16_t) (ECMA_PROP_IS_ENUMERABLE_DEFINED | ECMA_PROP_IS_CONFIGURABLE_DEFINED | flags);

  if (property & ECMA_PROPERTY_FLAG_DATA)
  {
    if (!ECMA_PROPERTY_IS_VIRTUAL (property))
    {
      prop_desc_p->value = ecma_copy_value (property_ref.value_p->value);
    }
    else
    {
      prop_desc_p->value = property_ref.virtual_value;
    }

    prop_desc_p->flags |= (ECMA_PROP_IS_VALUE_DEFINED | ECMA_PROP_IS_WRITABLE_DEFINED);
    const uint8_t writable = ecma_is_property_writable (property) ? ECMA_PROP_IS_WRITABLE : ECMA_PROP_NO_OPTS;
    prop_desc_p->flags = (uint16_t) (prop_desc_p->flags | writable);
  }
  else
  {
    ecma_getter_setter_pointers_t *get_set_pair_p = ecma_get_named_accessor_property (property_ref.value_p);
    prop_desc_p->flags |= (ECMA_PROP_IS_GET_DEFINED | ECMA_PROP_IS_SET_DEFINED);

    if (get_set_pair_p->getter_cp == JMEM_CP_NULL)
    {
      prop_desc_p->get_p = NULL;
    }
    else
    {
      prop_desc_p->get_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, get_set_pair_p->getter_cp);
      ecma_ref_object (prop_desc_p->get_p);
    }

    if (get_set_pair_p->setter_cp == JMEM_CP_NULL)
    {
      prop_desc_p->set_p = NULL;
    }
    else
    {
      prop_desc_p->set_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, get_set_pair_p->setter_cp);
      ecma_ref_object (prop_desc_p->set_p);
    }
  }

  return ECMA_VALUE_TRUE;
} /* ecma_op_object_get_own_property_descriptor */

#if JERRY_BUILTIN_PROXY
/**
 * Get property descriptor from a target value for a specified property.
 *
 * For more details see ecma_op_object_get_own_property_descriptor
 *
 * @return ECMA_VALUE_ERROR - if the Proxy.[[GetOwnProperty]] operation raises error
 *         ECMA_VALUE_{TRUE, FALSE} - if property found or not
 */
ecma_value_t
ecma_op_get_own_property_descriptor (ecma_value_t target, /**< target value */
                                     ecma_string_t *property_name_p, /**< property name */
                                     ecma_property_descriptor_t *prop_desc_p) /**< property descriptor */
{
  if (!ecma_is_value_object (target))
  {
    return ECMA_VALUE_FALSE;
  }

  return ecma_op_object_get_own_property_descriptor (ecma_get_object_from_value (target), property_name_p, prop_desc_p);
} /* ecma_op_get_own_property_descriptor */
#endif /* JERRY_BUILTIN_PROXY */

/**
 * [[HasInstance]] ecma object's operation
 *
 * See also:
 *          ECMA-262 v5, 8.6.2; ECMA-262 v5, Table 9
 *
 * @return ecma value containing a boolean value or an error
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_object_has_instance (ecma_object_t *obj_p, /**< the object */
                             ecma_value_t value) /**< argument 'V' */
{
  JERRY_ASSERT (obj_p != NULL && !ecma_is_lexical_environment (obj_p));

  JERRY_ASSERT_OBJECT_TYPE_IS_VALID (ecma_get_object_type (obj_p));

  if (ecma_op_object_is_callable (obj_p))
  {
    return ecma_op_function_has_instance (obj_p, value);
  }

  return ecma_raise_type_error (ECMA_ERR_MSG ("Expected a function object"));
} /* ecma_op_object_has_instance */

/**
 * General [[GetPrototypeOf]] abstract operation
 *
 * Note: returned valid object must be freed.
 *
 * @return ecma_object_t * - prototype of the input object.
 *         ECMA_OBJECT_POINTER_ERROR - error reported during Proxy resolve.
 *         NULL - the input object does not have a prototype.
 */
ecma_object_t *
ecma_op_object_get_prototype_of (ecma_object_t *obj_p) /**< input object */
{
  JERRY_ASSERT (obj_p != NULL);

#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_IS_PROXY (obj_p))
  {
    ecma_value_t proto = ecma_proxy_object_get_prototype_of (obj_p);

    if (ECMA_IS_VALUE_ERROR (proto))
    {
      return ECMA_OBJECT_POINTER_ERROR;
    }
    if (ecma_is_value_null (proto))
    {
      return NULL;
    }

    JERRY_ASSERT (ecma_is_value_object (proto));

    return ecma_get_object_from_value (proto);
  }
  else
#endif /* JERRY_BUILTIN_PROXY */
  {
    jmem_cpointer_t proto_cp = ecma_op_ordinary_object_get_prototype_of (obj_p);

    if (proto_cp == JMEM_CP_NULL)
    {
      return NULL;
    }

    ecma_object_t *proto_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, proto_cp);
    ecma_ref_object (proto_p);

    return proto_p;
  }
} /* ecma_op_object_get_prototype_of */

/**
 * Object's isPrototypeOf operation
 *
 * See also:
 *          ECMA-262 v5, 15.2.4.6; 3
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_TRUE - if the target object is prototype of the base object
 *         ECMA_VALUE_FALSE - if the target object is not prototype of the base object
 */
ecma_value_t
ecma_op_object_is_prototype_of (ecma_object_t *base_p, /**< base object */
                                ecma_object_t *target_p) /**< target object */
{
  ecma_ref_object (target_p);

  do
  {
    ecma_object_t *proto_p = ecma_op_object_get_prototype_of (target_p);
    ecma_deref_object (target_p);

    if (proto_p == NULL)
    {
      return ECMA_VALUE_FALSE;
    }
    else if (proto_p == ECMA_OBJECT_POINTER_ERROR)
    {
      return ECMA_VALUE_ERROR;
    }
    else if (proto_p == base_p)
    {
      ecma_deref_object (proto_p);
      return ECMA_VALUE_TRUE;
    }

    /* Advance up on prototype chain. */
    target_p = proto_p;
  } while (true);
} /* ecma_op_object_is_prototype_of */

/**
 * Object's EnumerableOwnPropertyNames operation
 *
 * See also:
 *          ECMA-262 v11, 7.3.23
 *
 * @return NULL - if operation fails
 *         collection of property names / values / name-value pairs - otherwise
 */
ecma_collection_t *
ecma_op_object_get_enumerable_property_names (ecma_object_t *obj_p, /**< routine's first argument */
                                              ecma_enumerable_property_names_options_t option) /**< listing option */
{
  /* 2. */
  ecma_collection_t *prop_names_p = ecma_op_object_own_property_keys (obj_p);

#if JERRY_BUILTIN_PROXY
  if (JERRY_UNLIKELY (prop_names_p == NULL))
  {
    return prop_names_p;
  }
#endif /* JERRY_BUILTIN_PROXY */

  ecma_value_t *names_buffer_p = prop_names_p->buffer_p;
  /* 3. */
  ecma_collection_t *properties_p = ecma_new_collection ();

  /* 4. */
  for (uint32_t i = 0; i < prop_names_p->item_count; i++)
  {
    /* 4.a */
    if (ecma_is_value_string (names_buffer_p[i]))
    {
      ecma_string_t *key_p = ecma_get_string_from_value (names_buffer_p[i]);

      /* 4.a.i */
      ecma_property_descriptor_t prop_desc;
      ecma_value_t status = ecma_op_object_get_own_property_descriptor (obj_p, key_p, &prop_desc);

      if (ECMA_IS_VALUE_ERROR (status))
      {
        ecma_collection_free (prop_names_p);
        ecma_collection_free (properties_p);

        return NULL;
      }

      const bool is_enumerable = (prop_desc.flags & ECMA_PROP_IS_ENUMERABLE) != 0;
      ecma_free_property_descriptor (&prop_desc);
      /* 4.a.ii */
      if (is_enumerable)
      {
        /* 4.a.ii.1 */
        if (option == ECMA_ENUMERABLE_PROPERTY_KEYS)
        {
          ecma_collection_push_back (properties_p, ecma_copy_value (names_buffer_p[i]));
        }
        else
        {
          /* 4.a.ii.2.a */
          ecma_value_t value = ecma_op_object_get (obj_p, key_p);

          if (ECMA_IS_VALUE_ERROR (value))
          {
            ecma_collection_free (prop_names_p);
            ecma_collection_free (properties_p);

            return NULL;
          }

          /* 4.a.ii.2.b */
          if (option == ECMA_ENUMERABLE_PROPERTY_VALUES)
          {
            ecma_collection_push_back (properties_p, value);
          }
          else
          {
            /* 4.a.ii.2.c.i */
            JERRY_ASSERT (option == ECMA_ENUMERABLE_PROPERTY_ENTRIES);

            /* 4.a.ii.2.c.ii */
            ecma_object_t *entry_p = ecma_op_new_array_object (2);

            ecma_builtin_helper_def_prop_by_index (entry_p,
                                                   0,
                                                   names_buffer_p[i],
                                                   ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE);
            ecma_builtin_helper_def_prop_by_index (entry_p, 1, value, ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE);
            ecma_free_value (value);

            /* 4.a.ii.2.c.iii */
            ecma_collection_push_back (properties_p, ecma_make_object_value (entry_p));
          }
        }
      }
    }
  }

  ecma_collection_free (prop_names_p);

  return properties_p;
} /* ecma_op_object_get_enumerable_property_names */

/**
 * Helper method to check if a given property is already in the collection or not
 *
 * @return true - if the property is already in the collection
 *         false - otherwise
 */
static bool
ecma_object_prop_name_is_duplicated (ecma_collection_t *prop_names_p, /**< prop name collection */
                                     ecma_string_t *name_p) /**< property name */
{
  for (uint64_t i = 0; i < prop_names_p->item_count; i++)
  {
    if (ecma_compare_ecma_strings (ecma_get_prop_name_from_value (prop_names_p->buffer_p[i]), name_p))
    {
      return true;
    }
  }

  return false;
} /* ecma_object_prop_name_is_duplicated */

/**
 * Helper method for getting lazy instantiated properties for [[OwnPropertyKeys]]
 */
static void
ecma_object_list_lazy_property_names (ecma_object_t *obj_p, /**< object */
                                      ecma_collection_t *prop_names_p, /**< prop name collection */
                                      ecma_property_counter_t *prop_counter_p) /**< prop counter */
{
  const ecma_object_type_t type = ecma_get_object_type (obj_p);
  const bool obj_is_builtin = ecma_get_object_is_builtin (obj_p);

  if (obj_is_builtin)
  {
    if (type == ECMA_OBJECT_TYPE_NATIVE_FUNCTION && ecma_builtin_function_is_routine (obj_p))
    {
      ecma_builtin_routine_list_lazy_property_names (obj_p, prop_names_p, prop_counter_p);
    }
    else
    {
      ecma_builtin_list_lazy_property_names (obj_p, prop_names_p, prop_counter_p);
    }
  }
  else
  {
    switch (type)
    {
      case ECMA_OBJECT_TYPE_PSEUDO_ARRAY:
      {
        if (((ecma_extended_object_t *) obj_p)->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS)
        {
          ecma_op_arguments_object_list_lazy_property_names (obj_p, prop_names_p, prop_counter_p);
        }
#if JERRY_BUILTIN_TYPEDARRAY
        if (ecma_object_is_typedarray (obj_p))
        {
          ecma_op_typedarray_list_lazy_property_names (obj_p, prop_names_p, prop_counter_p);
        }
#endif /* JERRY_BUILTIN_TYPEDARRAY */
        break;
      }
      case ECMA_OBJECT_TYPE_FUNCTION:
      {
        ecma_op_function_list_lazy_property_names (obj_p, prop_names_p, prop_counter_p);
        break;
      }
      case ECMA_OBJECT_TYPE_NATIVE_FUNCTION:
      {
        ecma_op_external_function_list_lazy_property_names (obj_p, prop_names_p, prop_counter_p);
        break;
      }
      case ECMA_OBJECT_TYPE_BOUND_FUNCTION:
      {
        ecma_op_bound_function_list_lazy_property_names (obj_p, prop_names_p, prop_counter_p);
        break;
      }
      case ECMA_OBJECT_TYPE_CLASS:
      {
        ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) obj_p;

        if (ext_object_p->u.class_prop.class_id == LIT_MAGIC_STRING_STRING_UL)
        {
          ecma_op_string_list_lazy_property_names (obj_p, prop_names_p, prop_counter_p);
        }

        break;
      }
      case ECMA_OBJECT_TYPE_ARRAY:
      {
        ecma_collection_push_back (prop_names_p, ecma_make_magic_string_value (LIT_MAGIC_STRING_LENGTH));
        prop_counter_p->string_named_props++;
        break;
      }
      default:
      {
        JERRY_ASSERT (type == ECMA_OBJECT_TYPE_GENERAL);
        break;
      }
    }
  }
} /* ecma_object_list_lazy_property_names */

/**
 * Helper method for sorting the given property names based on [[OwnPropertyKeys]]
 */
static void
ecma_object_sort_property_names (ecma_collection_t *prop_names_p, /**< prop name collection */
                                 ecma_property_counter_t *prop_counter) /**< prop counter */
{
  uint32_t lazy_string_prop_name_count = prop_counter->lazy_string_named_props;
#if JERRY_ESNEXT
  uint32_t lazy_symbol_prop_name_count = prop_counter->lazy_symbol_named_props;
#endif /* JERRY_ESNEXT */

  uint32_t string_name_pos = prop_counter->string_named_props;
#if JERRY_ESNEXT
  uint32_t symbol_name_pos = prop_counter->symbol_named_props;
#endif /* JERRY_ESNEXT */

  uint32_t all_prop_count = (prop_counter->array_index_named_props) + (string_name_pos);
#if JERRY_ESNEXT
  all_prop_count += symbol_name_pos;
#endif /* JERRY_ESNEXT */

  ecma_value_t *names_p = jmem_heap_alloc_block (all_prop_count * sizeof (ecma_value_t));

  ecma_value_t *string_names_p = names_p + prop_counter->array_index_named_props;
#if JERRY_ESNEXT
  ecma_value_t *symbol_names_p = string_names_p + string_name_pos;
#endif /* JERRY_ESNEXT */

  uint32_t array_index_name_pos = 0;
  uint32_t lazy_string_name_pos = 0;
#if JERRY_ESNEXT
  uint32_t lazy_symbol_name_pos = 0;
#endif /* JERRY_ESNEXT */

  for (uint32_t i = 0; i < prop_names_p->item_count; i++)
  {
    ecma_value_t prop_name = prop_names_p->buffer_p[i];
    ecma_string_t *name_p = ecma_get_prop_name_from_value (prop_name);
    uint32_t index = ecma_string_get_array_index (name_p);

    /* sort array index named properties in ascending order */
    if (index != ECMA_STRING_NOT_ARRAY_INDEX)
    {
      JERRY_ASSERT (array_index_name_pos < prop_counter->array_index_named_props);

      uint32_t insert_pos = 0;
      while (insert_pos < array_index_name_pos
             && index > ecma_string_get_array_index (ecma_get_string_from_value (names_p[insert_pos])))
      {
        insert_pos++;
      }

      if (insert_pos == array_index_name_pos)
      {
        names_p[array_index_name_pos++] = prop_name;
      }
      else
      {
        JERRY_ASSERT (insert_pos < array_index_name_pos);
        JERRY_ASSERT (index <= ecma_string_get_array_index (ecma_get_string_from_value (names_p[insert_pos])));

        uint32_t move_pos = array_index_name_pos++;

        while (move_pos > insert_pos)
        {
          names_p[move_pos] = names_p[move_pos - 1u];

          move_pos--;
        }

        names_p[insert_pos] = prop_name;
      }
    }
#if JERRY_ESNEXT
    /* sort symbol named properites in creation order */
    else if (ecma_prop_name_is_symbol (name_p))
    {
      JERRY_ASSERT (symbol_name_pos > 0);
      JERRY_ASSERT (symbol_name_pos <= prop_counter->symbol_named_props);

      if (i < lazy_symbol_prop_name_count)
      {
        symbol_names_p[lazy_symbol_name_pos++] = prop_name;
      }
      else
      {
        symbol_names_p[--symbol_name_pos] = prop_name;
      }
    }
#endif /* JERRY_ESNEXT */
    /* sort string named properties in creation order */
    else
    {
      JERRY_ASSERT (string_name_pos > 0);
      JERRY_ASSERT (string_name_pos <= prop_counter->string_named_props);

      if (i < lazy_string_prop_name_count)
      {
        string_names_p[lazy_string_name_pos++] = prop_name;
      }
      else
      {
        string_names_p[--string_name_pos] = prop_name;
      }
    }
  }

  /* Free the unsorted buffer and copy the sorted one in its place */
  jmem_heap_free_block (prop_names_p->buffer_p, ECMA_COLLECTION_ALLOCATED_SIZE (prop_names_p->capacity));
  prop_names_p->buffer_p = names_p;
  prop_names_p->item_count = all_prop_count;
  prop_names_p->capacity = all_prop_count;
} /* ecma_object_sort_property_names */

/**
 * Object's [[OwnPropertyKeys]] internal method
 *
 * Order of names in the collection:
 *  - integer indices in ascending order
 *  - other indices in creation order (for built-ins: the order of the properties are listed in specification).
 *
 * Note:
 *      Implementation of the routine assumes that new properties are appended to beginning of corresponding object's
 *      property list, and the list is not reordered (in other words, properties are stored in order that is reversed
 *      to the properties' addition order).
 *
 * @return NULL - if the Proxy.[[OwnPropertyKeys]] operation raises error
 *         collection of property names - otherwise
 */
ecma_collection_t *
ecma_op_object_own_property_keys (ecma_object_t *obj_p) /**< object */
{
#if JERRY_BUILTIN_PROXY
  if (ECMA_OBJECT_IS_PROXY (obj_p))
  {
    return ecma_proxy_object_own_property_keys (obj_p);
  }
#endif /* JERRY_BUILTIN_PROXY */

  if (ecma_op_object_is_fast_array (obj_p))
  {
    return ecma_fast_array_object_own_property_keys (obj_p);
  }

  ecma_collection_t *prop_names_p = ecma_new_collection ();
  ecma_property_counter_t prop_counter = { 0, 0, 0, 0, 0 };

  ecma_object_list_lazy_property_names (obj_p, prop_names_p, &prop_counter);

  prop_counter.lazy_string_named_props = prop_names_p->item_count - prop_counter.symbol_named_props;
  prop_counter.lazy_symbol_named_props = prop_counter.symbol_named_props;

  jmem_cpointer_t prop_iter_cp = obj_p->u1.property_list_cp;

#if JERRY_PROPRETY_HASHMAP
  if (prop_iter_cp != JMEM_CP_NULL)
  {
    ecma_property_header_t *prop_iter_p = ECMA_GET_NON_NULL_POINTER (ecma_property_header_t, prop_iter_cp);

    if (prop_iter_p->types[0] == ECMA_PROPERTY_TYPE_HASHMAP)
    {
      prop_iter_cp = prop_iter_p->next_property_cp;
    }
  }
#endif /* JERRY_PROPRETY_HASHMAP */

  while (prop_iter_cp != JMEM_CP_NULL)
  {
    ecma_property_header_t *prop_iter_p = ECMA_GET_NON_NULL_POINTER (ecma_property_header_t, prop_iter_cp);
    JERRY_ASSERT (ECMA_PROPERTY_IS_PROPERTY_PAIR (prop_iter_p));

    for (int i = 0; i < ECMA_PROPERTY_PAIR_ITEM_COUNT; i++)
    {
      ecma_property_t *property_p = prop_iter_p->types + i;

      if (ECMA_PROPERTY_IS_RAW (*property_p))
      {
        ecma_property_pair_t *prop_pair_p = (ecma_property_pair_t *) prop_iter_p;

        if (ECMA_PROPERTY_GET_NAME_TYPE (*property_p) == ECMA_DIRECT_STRING_MAGIC
            && prop_pair_p->names_cp[i] >= LIT_NON_INTERNAL_MAGIC_STRING__COUNT
            && prop_pair_p->names_cp[i] < LIT_MAGIC_STRING__COUNT)
        {
          continue;
        }

        ecma_string_t *name_p = ecma_string_from_property_name (*property_p, prop_pair_p->names_cp[i]);

        if (!ecma_object_prop_name_is_duplicated (prop_names_p, name_p))
        {
          if (ecma_string_get_array_index (name_p) != ECMA_STRING_NOT_ARRAY_INDEX)
          {
            prop_counter.array_index_named_props++;
          }
#if JERRY_ESNEXT
          else if (ecma_prop_name_is_symbol (name_p))
          {
            prop_counter.symbol_named_props++;
          }
#endif /* JERRY_ESNEXT */
          else
          {
            prop_counter.string_named_props++;
          }

          ecma_collection_push_back (prop_names_p, ecma_make_prop_name_value (name_p));
        }
        else
        {
          ecma_deref_ecma_string (name_p);
        }
      }
    }

    prop_iter_cp = prop_iter_p->next_property_cp;
  }

  if (prop_names_p->item_count != 0)
  {
    ecma_object_sort_property_names (prop_names_p, &prop_counter);
  }

  return prop_names_p;
} /* ecma_op_object_own_property_keys */

/**
 * EnumerateObjectProperties abstract method
 *
 * See also:
 *          ECMA-262 v11, 13.7.5.15
 *
 * @return NULL - if the Proxy.[[OwnPropertyKeys]] operation raises error
 *         collection of enumerable property names - otherwise
 */
ecma_collection_t *
ecma_op_object_enumerate (ecma_object_t *obj_p) /**< object */
{
  ecma_collection_t *visited_names_p = ecma_new_collection ();
  ecma_collection_t *return_names_p = ecma_new_collection ();

  ecma_ref_object (obj_p);

  while (true)
  {
    ecma_collection_t *keys = ecma_op_object_own_property_keys (obj_p);

#if JERRY_ESNEXT
    if (JERRY_UNLIKELY (keys == NULL))
    {
      ecma_collection_free (return_names_p);
      ecma_collection_free (visited_names_p);
      ecma_deref_object (obj_p);
      return keys;
    }
#endif /* JERRY_ESNEXT */

    for (uint32_t i = 0; i < keys->item_count; i++)
    {
      ecma_value_t prop_name = keys->buffer_p[i];
      ecma_string_t *name_p = ecma_get_prop_name_from_value (prop_name);

#if JERRY_ESNEXT
      if (ecma_prop_name_is_symbol (name_p))
      {
        continue;
      }
#endif /* JERRY_ESNEXT */

      ecma_property_descriptor_t prop_desc;
      ecma_value_t get_desc = ecma_op_object_get_own_property_descriptor (obj_p, name_p, &prop_desc);

      if (ECMA_IS_VALUE_ERROR (get_desc))
      {
        ecma_collection_free (keys);
        ecma_collection_free (return_names_p);
        ecma_collection_free (visited_names_p);
        ecma_deref_object (obj_p);
        return NULL;
      }

      if (ecma_is_value_true (get_desc))
      {
        bool is_enumerable = (prop_desc.flags & ECMA_PROP_IS_ENUMERABLE) != 0;
        ecma_free_property_descriptor (&prop_desc);

        if (ecma_collection_has_string_value (visited_names_p, name_p)
            || ecma_collection_has_string_value (return_names_p, name_p))
        {
          continue;
        }

        ecma_ref_ecma_string (name_p);

        if (is_enumerable)
        {
          ecma_collection_push_back (return_names_p, prop_name);
        }
        else
        {
          ecma_collection_push_back (visited_names_p, prop_name);
        }
      }
    }

    ecma_collection_free (keys);

    /* Query the prototype. */
    ecma_object_t *proto_p = ecma_op_object_get_prototype_of (obj_p);
    ecma_deref_object (obj_p);

    if (proto_p == NULL)
    {
      break;
    }
    else if (JERRY_UNLIKELY (proto_p == ECMA_OBJECT_POINTER_ERROR))
    {
      ecma_collection_free (return_names_p);
      ecma_collection_free (visited_names_p);
      return NULL;
    }

    /* Advance up on prototype chain. */
    obj_p = proto_p;
  }

  ecma_collection_free (visited_names_p);

  return return_names_p;
} /* ecma_op_object_enumerate */

/**
 * The function is used in the assert of ecma_object_get_class_name
 *
 * @return true  - if class name is an object
 *         false - otherwise
 */
inline static bool
ecma_object_check_class_name_is_object (ecma_object_t *obj_p) /**< object */
{
#ifndef JERRY_NDEBUG
  return (ecma_builtin_is_global (obj_p)
#if JERRY_BUILTIN_PROMISE
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_PROMISE_PROTOTYPE)
#endif /* JERRY_BUILTIN_PROMISE */
#if JERRY_BUILTIN_TYPEDARRAY
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_ARRAYBUFFER_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_TYPEDARRAY_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_INT8ARRAY_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_UINT8ARRAY_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_INT16ARRAY_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_UINT16ARRAY_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_INT32ARRAY_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_UINT32ARRAY_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_FLOAT32ARRAY_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_UINT8CLAMPEDARRAY_PROTOTYPE)
#if JERRY_NUMBER_TYPE_FLOAT64
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_FLOAT64ARRAY_PROTOTYPE)
#endif /* JERRY_NUMBER_TYPE_FLOAT64 */
#if JERRY_BUILTIN_BIGINT
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_BIGINT64ARRAY_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_BIGUINT64ARRAY_PROTOTYPE)
#endif /* JERRY_BUILTIN_BIGINT */
#endif /* JERRY_BUILTIN_TYPEDARRAY */
#if JERRY_ESNEXT
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_ARRAY_PROTOTYPE_UNSCOPABLES)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_ARRAY_ITERATOR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_ITERATOR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_STRING_ITERATOR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_REGEXP_STRING_ITERATOR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_EVAL_ERROR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_RANGE_ERROR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_REFERENCE_ERROR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_SYNTAX_ERROR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_GENERATOR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_TYPE_ERROR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_URI_ERROR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_ERROR_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_DATE_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_REGEXP_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_SYMBOL_PROTOTYPE)
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_ASYNC_FUNCTION_PROTOTYPE)
#endif /* JERRY_ESNEXT */
#if JERRY_BUILTIN_MAP
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_MAP_PROTOTYPE)
#if JERRY_ESNEXT
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_MAP_ITERATOR_PROTOTYPE)
#endif /* JERRY_ESNEXT */
#endif /* JERRY_BUILTIN_MAP */
#if JERRY_BUILTIN_SET
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_SET_PROTOTYPE)
#if JERRY_ESNEXT
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_SET_ITERATOR_PROTOTYPE)
#endif /* JERRY_ESNEXT */
#endif /* JERRY_BUILTIN_SET */
#if JERRY_BUILTIN_WEAKMAP
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_WEAKMAP_PROTOTYPE)
#endif /* JERRY_BUILTIN_WEAKMAP */
#if JERRY_BUILTIN_WEAKSET
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_WEAKSET_PROTOTYPE)
#endif /* JERRY_BUILTIN_WEAKSET */
#if JERRY_BUILTIN_DATAVIEW
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_DATAVIEW_PROTOTYPE)
#endif /* JERRY_BUILTIN_DATAVIEW */
          || ecma_builtin_is (obj_p, ECMA_BUILTIN_ID_OBJECT_PROTOTYPE));
#else /* JERRY_NDEBUG */
  JERRY_UNUSED (obj_p);
  return true;
#endif /* !JERRY_NDEBUG */
} /* ecma_object_check_class_name_is_object */

/**
 * Get [[Class]] string of specified object
 *
 * @return class name magic string
 */
lit_magic_string_id_t
ecma_object_get_class_name (ecma_object_t *obj_p) /**< object */
{
  ecma_object_type_t type = ecma_get_object_type (obj_p);

  switch (type)
  {
    case ECMA_OBJECT_TYPE_ARRAY:
    {
      return LIT_MAGIC_STRING_ARRAY_UL;
    }
    case ECMA_OBJECT_TYPE_CLASS:
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) obj_p;

      return (lit_magic_string_id_t) ext_object_p->u.class_prop.class_id;
    }
    case ECMA_OBJECT_TYPE_PSEUDO_ARRAY:
    {
      ecma_extended_object_t *ext_obj_p = (ecma_extended_object_t *) obj_p;

      switch (ext_obj_p->u.pseudo_array.type)
      {
#if JERRY_BUILTIN_TYPEDARRAY
        case ECMA_PSEUDO_ARRAY_TYPEDARRAY:
        case ECMA_PSEUDO_ARRAY_TYPEDARRAY_WITH_INFO:
        {
          return (lit_magic_string_id_t) ext_obj_p->u.pseudo_array.u1.class_id;
        }
#endif /* JERRY_BUILTIN_TYPEDARRAY */
#if JERRY_ESNEXT
        case ECMA_PSEUDO_ARRAY_ITERATOR:
        {
          return LIT_MAGIC_STRING_ARRAY_ITERATOR_UL;
        }
        case ECMA_PSEUDO_SET_ITERATOR:
        {
          return LIT_MAGIC_STRING_SET_ITERATOR_UL;
        }
        case ECMA_PSEUDO_MAP_ITERATOR:
        {
          return LIT_MAGIC_STRING_MAP_ITERATOR_UL;
        }
#endif /* JERRY_ESNEXT */
#if JERRY_ESNEXT
        case ECMA_PSEUDO_STRING_ITERATOR:
        {
          return LIT_MAGIC_STRING_STRING_ITERATOR_UL;
        }
        case ECMA_PSEUDO_REGEXP_STRING_ITERATOR:
        {
          return LIT_MAGIC_STRING_REGEXP_STRING_ITERATOR_UL;
        }
#endif /* JERRY_ESNEXT */
        default:
        {
          JERRY_ASSERT (ext_obj_p->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS);

          return LIT_MAGIC_STRING_ARGUMENTS_UL;
        }
      }

      break;
    }
    case ECMA_OBJECT_TYPE_FUNCTION:
    case ECMA_OBJECT_TYPE_NATIVE_FUNCTION:
    case ECMA_OBJECT_TYPE_BOUND_FUNCTION:
    {
      return LIT_MAGIC_STRING_FUNCTION_UL;
    }
#if JERRY_BUILTIN_PROXY
    case ECMA_OBJECT_TYPE_PROXY:
    {
      ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) obj_p;

      if (!ecma_is_value_null (proxy_obj_p->target) && ecma_is_value_object (proxy_obj_p->target))
      {
        ecma_object_t *target_obj_p = ecma_get_object_from_value (proxy_obj_p->target);
        return ecma_object_get_class_name (target_obj_p);
      }
      return LIT_MAGIC_STRING_OBJECT_UL;
    }
#endif /* JERRY_BUILTIN_PROXY */
    default:
    {
      JERRY_ASSERT (type == ECMA_OBJECT_TYPE_GENERAL || type == ECMA_OBJECT_TYPE_PROXY);

      if (ecma_get_object_is_builtin (obj_p))
      {
        ecma_extended_object_t *ext_obj_p = (ecma_extended_object_t *) obj_p;

        switch (ext_obj_p->u.built_in.id)
        {
#if JERRY_BUILTIN_MATH
          case ECMA_BUILTIN_ID_MATH:
          {
            return LIT_MAGIC_STRING_MATH_UL;
          }
#endif /* JERRY_BUILTIN_MATH */
#if JERRY_BUILTIN_REFLECT
          case ECMA_BUILTIN_ID_REFLECT:
          {
            return LIT_MAGIC_STRING_REFLECT_UL;
          }
#endif /* JERRY_BUILTIN_REFLECT */
#if JERRY_ESNEXT
          case ECMA_BUILTIN_ID_GENERATOR:
          {
            return LIT_MAGIC_STRING_GENERATOR_UL;
          }
          case ECMA_BUILTIN_ID_ASYNC_GENERATOR:
          {
            return LIT_MAGIC_STRING_ASYNC_GENERATOR_UL;
          }
#endif /* JERRY_ESNEXT */
#if JERRY_BUILTIN_JSON
          case ECMA_BUILTIN_ID_JSON:
          {
            return LIT_MAGIC_STRING_JSON_U;
          }
#endif /* JERRY_BUILTIN_JSON */
#if !JERRY_ESNEXT
#if JERRY_BUILTIN_ERRORS
          case ECMA_BUILTIN_ID_EVAL_ERROR_PROTOTYPE:
          case ECMA_BUILTIN_ID_RANGE_ERROR_PROTOTYPE:
          case ECMA_BUILTIN_ID_REFERENCE_ERROR_PROTOTYPE:
          case ECMA_BUILTIN_ID_SYNTAX_ERROR_PROTOTYPE:
          case ECMA_BUILTIN_ID_TYPE_ERROR_PROTOTYPE:
          case ECMA_BUILTIN_ID_URI_ERROR_PROTOTYPE:
#endif /* JERRY_BUILTIN_ERRORS */
          case ECMA_BUILTIN_ID_ERROR_PROTOTYPE:
          {
            return LIT_MAGIC_STRING_ERROR_UL;
          }
#endif /* !JERRY_ESNEXT */
#if JERRY_BUILTIN_PROXY
          case ECMA_BUILTIN_ID_PROXY:
          {
            return LIT_MAGIC_STRING_FUNCTION_UL;
          }
#endif /* JERRY_BUILTIN_PROXY */
#if JERRY_BUILTIN_BIGINT
          case ECMA_BUILTIN_ID_BIGINT:
          {
            return LIT_MAGIC_STRING_FUNCTION_UL;
          }
#endif /* JERRY_BUILTIN_BIGINT */
          default:
          {
            JERRY_ASSERT (ecma_object_check_class_name_is_object (obj_p));

            return LIT_MAGIC_STRING_OBJECT_UL;
          }
        }
      }
      else
      {
        return LIT_MAGIC_STRING_OBJECT_UL;
      }
    }
  }
} /* ecma_object_get_class_name */

/**
 * Get value of an object if the class matches
 *
 * @return value of the object if the class matches
 *         ECMA_VALUE_NOT_FOUND otherwise
 */
extern inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_object_class_is (ecma_object_t *object_p, /**< object */
                      uint32_t class_id) /**< class id */
{
  if (ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_CLASS)
  {
    ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

    if (ext_object_p->u.class_prop.class_id == class_id)
    {
      return true;
    }
  }

  return false;
} /* ecma_object_class_is */

/**
 * Checks if the given argument has [[RegExpMatcher]] internal slot
 *
 * @return true - if the given argument is a regexp
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_object_is_regexp_object (ecma_value_t arg) /**< argument */
{
  return (ecma_is_value_object (arg)
          && ecma_object_class_is (ecma_get_object_from_value (arg), LIT_MAGIC_STRING_REGEXP_UL));
} /* ecma_object_is_regexp_object */

#if JERRY_ESNEXT
/**
 * Object's IsConcatSpreadable operation, used for Array.prototype.concat
 * It checks the argument's [Symbol.isConcatSpreadable] property value
 *
 * See also:
 *          ECMA-262 v6, 22.1.3.1.1;
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_TRUE - if the argument is concatSpreadable
 *         ECMA_VALUE_FALSE - otherwise
 */
ecma_value_t
ecma_op_is_concat_spreadable (ecma_value_t arg) /**< argument */
{
  if (!ecma_is_value_object (arg))
  {
    return ECMA_VALUE_FALSE;
  }

  ecma_value_t spreadable =
    ecma_op_object_get_by_symbol_id (ecma_get_object_from_value (arg), LIT_GLOBAL_SYMBOL_IS_CONCAT_SPREADABLE);

  if (ECMA_IS_VALUE_ERROR (spreadable))
  {
    return spreadable;
  }

  if (!ecma_is_value_undefined (spreadable))
  {
    const bool to_bool = ecma_op_to_boolean (spreadable);
    ecma_free_value (spreadable);
    return ecma_make_boolean_value (to_bool);
  }

  return ecma_is_value_array (arg);
} /* ecma_op_is_concat_spreadable */

/**
 * IsRegExp operation
 *
 * See also:
 *          ECMA-262 v6, 22.1.3.1.1;
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_TRUE - if the argument is regexp
 *         ECMA_VALUE_FALSE - otherwise
 */
ecma_value_t
ecma_op_is_regexp (ecma_value_t arg) /**< argument */
{
  if (!ecma_is_value_object (arg))
  {
    return ECMA_VALUE_FALSE;
  }

  ecma_value_t is_regexp = ecma_op_object_get_by_symbol_id (ecma_get_object_from_value (arg), LIT_GLOBAL_SYMBOL_MATCH);

  if (ECMA_IS_VALUE_ERROR (is_regexp))
  {
    return is_regexp;
  }

  if (!ecma_is_value_undefined (is_regexp))
  {
    const bool to_bool = ecma_op_to_boolean (is_regexp);
    ecma_free_value (is_regexp);
    return ecma_make_boolean_value (to_bool);
  }

  return ecma_make_boolean_value (ecma_object_is_regexp_object (arg));
} /* ecma_op_is_regexp */

/**
 * SpeciesConstructor operation
 * See also:
 *          ECMA-262 v6, 7.3.20;
 *
 * @return ecma_value
 *         returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_species_constructor (ecma_object_t *this_value, /**< This Value */
                             ecma_builtin_id_t default_constructor_id) /**< Builtin ID of default constructor */
{
  ecma_object_t *default_constructor_p = ecma_builtin_get (default_constructor_id);
  ecma_value_t constructor = ecma_op_object_get_by_magic_id (this_value, LIT_MAGIC_STRING_CONSTRUCTOR);
  if (ECMA_IS_VALUE_ERROR (constructor))
  {
    return constructor;
  }

  if (ecma_is_value_undefined (constructor))
  {
    ecma_ref_object (default_constructor_p);
    return ecma_make_object_value (default_constructor_p);
  }

  if (!ecma_is_value_object (constructor))
  {
    ecma_free_value (constructor);
    return ecma_raise_type_error (ECMA_ERR_MSG ("Constructor must be an object"));
  }

  ecma_object_t *ctor_object_p = ecma_get_object_from_value (constructor);
  ecma_value_t species = ecma_op_object_get_by_symbol_id (ctor_object_p, LIT_GLOBAL_SYMBOL_SPECIES);
  ecma_deref_object (ctor_object_p);

  if (ECMA_IS_VALUE_ERROR (species))
  {
    return species;
  }

  if (ecma_is_value_undefined (species) || ecma_is_value_null (species))
  {
    ecma_ref_object (default_constructor_p);
    return ecma_make_object_value (default_constructor_p);
  }

  if (!ecma_is_constructor (species))
  {
    ecma_free_value (species);
    return ecma_raise_type_error (ECMA_ERR_MSG ("Species must be a constructor"));
  }

  return species;
} /* ecma_op_species_constructor */

/**
 * 7.3.18 Abstract operation Invoke when property name is a magic string
 *
 * @return ecma_value result of the invoked function or raised error
 *         note: returned value must be freed with ecma_free_value
 */
extern inline ecma_value_t JERRY_ATTR_ALWAYS_INLINE
ecma_op_invoke_by_symbol_id (ecma_value_t object, /**< Object value */
                             lit_magic_string_id_t symbol_id, /**< Symbol ID */
                             ecma_value_t *args_p, /**< Argument list */
                             uint32_t args_len) /**< Argument list length */
{
  ecma_string_t *symbol_p = ecma_op_get_global_symbol (symbol_id);
  ecma_value_t ret_value = ecma_op_invoke (object, symbol_p, args_p, args_len);
  ecma_deref_ecma_string (symbol_p);

  return ret_value;
} /* ecma_op_invoke_by_symbol_id */
#endif /* JERRY_ESNEXT */

/**
 * 7.3.18 Abstract operation Invoke when property name is a magic string
 *
 * @return ecma_value result of the invoked function or raised error
 *         note: returned value must be freed with ecma_free_value
 */
extern inline ecma_value_t JERRY_ATTR_ALWAYS_INLINE
ecma_op_invoke_by_magic_id (ecma_value_t object, /**< Object value */
                            lit_magic_string_id_t magic_string_id, /**< Magic string ID */
                            ecma_value_t *args_p, /**< Argument list */
                            uint32_t args_len) /**< Argument list length */
{
  return ecma_op_invoke (object, ecma_get_magic_string (magic_string_id), args_p, args_len);
} /* ecma_op_invoke_by_magic_id */

/**
 * 7.3.18 Abstract operation Invoke
 *
 * @return ecma_value result of the invoked function or raised error
 *         note: returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_invoke (ecma_value_t object, /**< Object value */
                ecma_string_t *property_name_p, /**< Property name */
                ecma_value_t *args_p, /**< Argument list */
                uint32_t args_len) /**< Argument list length */
{
  /* 3. */
  ecma_value_t object_value = ecma_op_to_object (object);
  if (ECMA_IS_VALUE_ERROR (object_value))
  {
    return object_value;
  }

  ecma_object_t *object_p = ecma_get_object_from_value (object_value);

#if JERRY_ESNEXT
  ecma_value_t this_arg = object;
#else /* !JERRY_ESNEXT */
  ecma_value_t this_arg = object_value;
#endif /* JERRY_ESNEXT */

  ecma_value_t func = ecma_op_object_get_with_receiver (object_p, property_name_p, this_arg);

  if (ECMA_IS_VALUE_ERROR (func))
  {
    ecma_deref_object (object_p);
    return func;
  }

  /* 4. */
  if (!ecma_op_is_callable (func))
  {
    ecma_free_value (func);
    ecma_deref_object (object_p);
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not callable"));
  }

  ecma_object_t *func_obj_p = ecma_get_object_from_value (func);
  ecma_value_t call_result = ecma_op_function_call (func_obj_p, this_arg, args_p, args_len);

  ecma_deref_object (object_p);
  ecma_deref_object (func_obj_p);

  return call_result;
} /* ecma_op_invoke */

/**
 * Ordinary object [[GetPrototypeOf]] operation
 *
 * See also:
 *          ECMAScript v6, 9.1.1
 *
 * @return the value of the [[Prototype]] internal slot of the given object.
 */
extern inline jmem_cpointer_t JERRY_ATTR_ALWAYS_INLINE
ecma_op_ordinary_object_get_prototype_of (ecma_object_t *obj_p) /**< object */
{
  JERRY_ASSERT (!ecma_is_lexical_environment (obj_p));
  JERRY_ASSERT (!ECMA_OBJECT_IS_PROXY (obj_p));

  return obj_p->u2.prototype_cp;
} /* ecma_op_ordinary_object_get_prototype_of */

/**
 * Ordinary object [[SetPrototypeOf]] operation
 *
 * See also:
 *          ECMAScript v6, 9.1.2
 *
 * @return ECMA_VALUE_FALSE - if the operation fails
 *         ECMA_VALUE_TRUE - otherwise
 */
extern inline ecma_value_t JERRY_ATTR_ALWAYS_INLINE
ecma_op_ordinary_object_set_prototype_of (ecma_object_t *obj_p, /**< base object */
                                          ecma_value_t proto) /**< prototype object */
{
  JERRY_ASSERT (!ecma_is_lexical_environment (obj_p));
  JERRY_ASSERT (!ECMA_OBJECT_IS_PROXY (obj_p));

  /* 1. */
  JERRY_ASSERT (ecma_is_value_object (proto) || ecma_is_value_null (proto));

  /* 3. */
  ecma_object_t *current_proto_p = ECMA_GET_POINTER (ecma_object_t, ecma_op_ordinary_object_get_prototype_of (obj_p));
  ecma_object_t *new_proto_p = ecma_is_value_null (proto) ? NULL : ecma_get_object_from_value (proto);

  /* 4. */
  if (new_proto_p == current_proto_p)
  {
    return ECMA_VALUE_TRUE;
  }

  /* 2 - 5. */
  if (!ecma_op_ordinary_object_is_extensible (obj_p))
  {
    return ECMA_VALUE_FALSE;
  }

  /**
   * When the prototype of a fast array changes, it is required to convert the
   * array to a "normal" array. This ensures that all [[Get]]/[[Set]]/etc.
   * calls works as expected.
   */
  if (ecma_op_object_is_fast_array (obj_p))
  {
    ecma_fast_array_convert_to_normal (obj_p);
  }

  /* 6. */
  ecma_object_t *iter_p = new_proto_p;

  /* 7 - 8. */
  while (true)
  {
    /* 8.a */
    if (iter_p == NULL)
    {
      break;
    }

    /* 8.b */
    if (obj_p == iter_p)
    {
      return ECMA_VALUE_FALSE;
    }

    /* 8.c.i */
#if JERRY_BUILTIN_PROXY
    if (ECMA_OBJECT_IS_PROXY (iter_p))
    {
      break;
    }
#endif /* JERRY_BUILTIN_PROXY */

    /* 8.c.ii */
    iter_p = ECMA_GET_POINTER (ecma_object_t, ecma_op_ordinary_object_get_prototype_of (iter_p));
  }

  /* 9. */
  ECMA_SET_POINTER (obj_p->u2.prototype_cp, new_proto_p);

  /* 10. */
  return ECMA_VALUE_TRUE;
} /* ecma_op_ordinary_object_set_prototype_of */

/**
 * [[IsExtensible]] operation for Ordinary object.
 *
 * See also:
 *          ECMAScript v6, 9.1.2
 *
 * @return true  - if object is extensible
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_PURE
ecma_op_ordinary_object_is_extensible (ecma_object_t *object_p) /**< object */
{
  JERRY_ASSERT (!ECMA_OBJECT_IS_PROXY (object_p));

  bool is_extensible = (object_p->type_flags_refs & ECMA_OBJECT_FLAG_EXTENSIBLE) != 0;

  JERRY_ASSERT (!ecma_op_object_is_fast_array (object_p) || is_extensible);

  return is_extensible;
} /* ecma_op_ordinary_object_is_extensible */

/**
 * Set value of [[Extensible]] object's internal property.
 */
void JERRY_ATTR_NOINLINE
ecma_op_ordinary_object_prevent_extensions (ecma_object_t *object_p) /**< object */
{
  JERRY_ASSERT (!ECMA_OBJECT_IS_PROXY (object_p));

  if (JERRY_UNLIKELY (ecma_op_object_is_fast_array (object_p)))
  {
    ecma_fast_array_convert_to_normal (object_p);
  }

  object_p->type_flags_refs &= (ecma_object_descriptor_t) ~ECMA_OBJECT_FLAG_EXTENSIBLE;
} /* ecma_op_ordinary_object_prevent_extensions */

/**
 * Checks whether an object (excluding prototypes) has a named property
 *
 * @return true - if property is found
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_op_ordinary_object_has_own_property (ecma_object_t *object_p, /**< the object */
                                          ecma_string_t *property_name_p) /**< property name */
{
  JERRY_ASSERT (!ECMA_OBJECT_IS_PROXY (object_p));

  ecma_property_t property =
    ecma_op_object_get_own_property (object_p, property_name_p, NULL, ECMA_PROPERTY_GET_NO_OPTIONS);

  return property != ECMA_PROPERTY_TYPE_NOT_FOUND && property != ECMA_PROPERTY_TYPE_NOT_FOUND_AND_STOP;
} /* ecma_op_ordinary_object_has_own_property */

/**
 * @}
 * @}
 */

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

#ifndef JERRYX_ARG_IMPL_H
#define JERRYX_ARG_IMPL_H

/* transform functions for each type. */

#define JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL(type)                                                               \
  jerry_value_t jerryx_arg_transform_##type (jerryx_arg_js_iterator_t *js_arg_iter_p, const jerryx_arg_t *c_arg_p); \
  jerry_value_t jerryx_arg_transform_##type##_optional (jerryx_arg_js_iterator_t *js_arg_iter_p,                    \
                                                        const jerryx_arg_t *c_arg_p);

#define JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT(type) \
  JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL (type)                 \
  JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL (type##_strict)

JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT (uint8)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT (int8)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT (uint16)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT (int16)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT (uint32)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT (int32)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT (number)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT (string)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT (utf8_string)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT (boolean)

JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL (function)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL (native_pointer)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL (object_props)
JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL (array_items)

jerry_value_t jerryx_arg_transform_ignore (jerryx_arg_js_iterator_t *js_arg_iter_p, const jerryx_arg_t *c_arg_p);

#undef JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL
#undef JERRYX_ARG_TRANSFORM_FUNC_WITH_OPTIONAL_AND_STRICT

/**
 * The structure indicates the options used to transform integer argument.
 * It will be passed into jerryx_arg_t's  extra_info field.
 */
typedef struct
{
  uint8_t round; /**< rounding policy */
  uint8_t clamp; /**< clamping policy */
} jerryx_arg_int_option_t;

/**
 * The macro used to generate jerryx_arg_xxx for int type.
 */
#define JERRYX_ARG_INT(type)                                                                  \
  static inline jerryx_arg_t jerryx_arg_##type (type##_t *dest,                               \
                                                jerryx_arg_round_t round_flag,                \
                                                jerryx_arg_clamp_t clamp_flag,                \
                                                jerryx_arg_coerce_t coerce_flag,              \
                                                jerryx_arg_optional_t opt_flag)               \
  {                                                                                           \
    jerryx_arg_transform_func_t func;                                                         \
    if (coerce_flag == JERRYX_ARG_NO_COERCE)                                                  \
    {                                                                                         \
      if (opt_flag == JERRYX_ARG_OPTIONAL)                                                    \
      {                                                                                       \
        func = jerryx_arg_transform_##type##_strict_optional;                                 \
      }                                                                                       \
      else                                                                                    \
      {                                                                                       \
        func = jerryx_arg_transform_##type##_strict;                                          \
      }                                                                                       \
    }                                                                                         \
    else                                                                                      \
    {                                                                                         \
      if (opt_flag == JERRYX_ARG_OPTIONAL)                                                    \
      {                                                                                       \
        func = jerryx_arg_transform_##type##_optional;                                        \
      }                                                                                       \
      else                                                                                    \
      {                                                                                       \
        func = jerryx_arg_transform_##type;                                                   \
      }                                                                                       \
    }                                                                                         \
    union                                                                                     \
    {                                                                                         \
      jerryx_arg_int_option_t int_option;                                                     \
      uintptr_t extra_info;                                                                   \
    } u = { .int_option = { .round = (uint8_t) round_flag, .clamp = (uint8_t) clamp_flag } }; \
    return (jerryx_arg_t){ .func = func, .dest = (void *) dest, .extra_info = u.extra_info }; \
  }

JERRYX_ARG_INT (uint8)
JERRYX_ARG_INT (int8)
JERRYX_ARG_INT (uint16)
JERRYX_ARG_INT (int16)
JERRYX_ARG_INT (uint32)
JERRYX_ARG_INT (int32)

#undef JERRYX_ARG_INT

/**
 * Create a validation/transformation step (`jerryx_arg_t`) that expects to
 * consume one `number` JS argument and stores it into a C `double`.
 *
 * @return a jerryx_arg_t instance.
 */
static inline jerryx_arg_t
jerryx_arg_number (double *dest, /**< pointer to the double where the result should be stored */
                   jerryx_arg_coerce_t coerce_flag, /**< whether type coercion is allowed */
                   jerryx_arg_optional_t opt_flag) /**< whether the argument is optional */
{
  jerryx_arg_transform_func_t func;

  if (coerce_flag == JERRYX_ARG_NO_COERCE)
  {
    if (opt_flag == JERRYX_ARG_OPTIONAL)
    {
      func = jerryx_arg_transform_number_strict_optional;
    }
    else
    {
      func = jerryx_arg_transform_number_strict;
    }
  }
  else
  {
    if (opt_flag == JERRYX_ARG_OPTIONAL)
    {
      func = jerryx_arg_transform_number_optional;
    }
    else
    {
      func = jerryx_arg_transform_number;
    }
  }

  return (jerryx_arg_t){ .func = func, .dest = (void *) dest };
} /* jerryx_arg_number */

/**
 * Create a validation/transformation step (`jerryx_arg_t`) that expects to
 * consume one `boolean` JS argument and stores it into a C `bool`.
 *
 * @return a jerryx_arg_t instance.
 */
static inline jerryx_arg_t
jerryx_arg_boolean (bool *dest, /**< points to the native bool */
                    jerryx_arg_coerce_t coerce_flag, /**< whether type coercion is allowed */
                    jerryx_arg_optional_t opt_flag) /**< whether the argument is optional */
{
  jerryx_arg_transform_func_t func;

  if (coerce_flag == JERRYX_ARG_NO_COERCE)
  {
    if (opt_flag == JERRYX_ARG_OPTIONAL)
    {
      func = jerryx_arg_transform_boolean_strict_optional;
    }
    else
    {
      func = jerryx_arg_transform_boolean_strict;
    }
  }
  else
  {
    if (opt_flag == JERRYX_ARG_OPTIONAL)
    {
      func = jerryx_arg_transform_boolean_optional;
    }
    else
    {
      func = jerryx_arg_transform_boolean;
    }
  }

  return (jerryx_arg_t){ .func = func, .dest = (void *) dest };
} /* jerryx_arg_boolean */

/**
 * Create a validation/transformation step (`jerryx_arg_t`) that expects to
 * consume one `string` JS argument and stores it into a C `char` array.
 *
 * @return a jerryx_arg_t instance.
 */
static inline jerryx_arg_t
jerryx_arg_string (char *dest, /**< pointer to the native char array where the result should be stored */
                   uint32_t size, /**< the size of native char array */
                   jerryx_arg_coerce_t coerce_flag, /**< whether type coercion is allowed */
                   jerryx_arg_optional_t opt_flag) /**< whether the argument is optional */
{
  jerryx_arg_transform_func_t func;

  if (coerce_flag == JERRYX_ARG_NO_COERCE)
  {
    if (opt_flag == JERRYX_ARG_OPTIONAL)
    {
      func = jerryx_arg_transform_string_strict_optional;
    }
    else
    {
      func = jerryx_arg_transform_string_strict;
    }
  }
  else
  {
    if (opt_flag == JERRYX_ARG_OPTIONAL)
    {
      func = jerryx_arg_transform_string_optional;
    }
    else
    {
      func = jerryx_arg_transform_string;
    }
  }

  return (jerryx_arg_t){ .func = func, .dest = (void *) dest, .extra_info = (uintptr_t) size };
} /* jerryx_arg_string */

/**
 * Create a validation/transformation step (`jerryx_arg_t`) that expects to
 * consume one `string` JS argument and stores it into a C utf8 `char` array.
 *
 * @return a jerryx_arg_t instance.
 */
static inline jerryx_arg_t
jerryx_arg_utf8_string (char *dest, /**< [out] pointer to the native char array where the result should be stored */
                        uint32_t size, /**< the size of native char array */
                        jerryx_arg_coerce_t coerce_flag, /**< whether type coercion is allowed */
                        jerryx_arg_optional_t opt_flag) /**< whether the argument is optional */
{
  jerryx_arg_transform_func_t func;

  if (coerce_flag == JERRYX_ARG_NO_COERCE)
  {
    if (opt_flag == JERRYX_ARG_OPTIONAL)
    {
      func = jerryx_arg_transform_utf8_string_strict_optional;
    }
    else
    {
      func = jerryx_arg_transform_utf8_string_strict;
    }
  }
  else
  {
    if (opt_flag == JERRYX_ARG_OPTIONAL)
    {
      func = jerryx_arg_transform_utf8_string_optional;
    }
    else
    {
      func = jerryx_arg_transform_utf8_string;
    }
  }

  return (jerryx_arg_t){ .func = func, .dest = (void *) dest, .extra_info = (uintptr_t) size };
} /* jerryx_arg_utf8_string */

/**
 * Create a validation/transformation step (`jerryx_arg_t`) that expects to
 * consume one `function` JS argument and stores it into a C `jerry_value_t`.
 *
 * @return a jerryx_arg_t instance.
 */
static inline jerryx_arg_t
jerryx_arg_function (jerry_value_t *dest, /**< pointer to the jerry_value_t where the result should be stored */
                     jerryx_arg_optional_t opt_flag) /**< whether the argument is optional */
{
  jerryx_arg_transform_func_t func;

  if (opt_flag == JERRYX_ARG_OPTIONAL)
  {
    func = jerryx_arg_transform_function_optional;
  }
  else
  {
    func = jerryx_arg_transform_function;
  }

  return (jerryx_arg_t){ .func = func, .dest = (void *) dest };
} /* jerryx_arg_function */

/**
 * Create a validation/transformation step (`jerryx_arg_t`) that expects to
 * consume one `object` JS argument that is 'backed' with a native pointer with
 * a given type info. In case the native pointer info matches, the transform
 * will succeed and the object's native pointer will be assigned to *dest.
 *
 * @return a jerryx_arg_t instance.
 */
static inline jerryx_arg_t
jerryx_arg_native_pointer (void **dest, /**< pointer to where the resulting native pointer should be stored */
                           const jerry_object_native_info_t *info_p, /**< expected the type info */
                           jerryx_arg_optional_t opt_flag) /**< whether the argument is optional */
{
  jerryx_arg_transform_func_t func;

  if (opt_flag == JERRYX_ARG_OPTIONAL)
  {
    func = jerryx_arg_transform_native_pointer_optional;
  }
  else
  {
    func = jerryx_arg_transform_native_pointer;
  }

  return (jerryx_arg_t){ .func = func, .dest = (void *) dest, .extra_info = (uintptr_t) info_p };
} /* jerryx_arg_native_pointer */

/**
 * Create a jerryx_arg_t instance for ignored argument.
 *
 * @return a jerryx_arg_t instance.
 */
static inline jerryx_arg_t
jerryx_arg_ignore (void)
{
  return (jerryx_arg_t){ .func = jerryx_arg_transform_ignore };
} /* jerryx_arg_ignore */

/**
 * Create a jerryx_arg_t instance with custom transform.
 *
 * @return a jerryx_arg_t instance.
 */
static inline jerryx_arg_t
jerryx_arg_custom (void *dest, /**< pointer to the native argument where the result should be stored */
                   uintptr_t extra_info, /**< the extra parameter, specific to the transform function */
                   jerryx_arg_transform_func_t func) /**< the custom transform function */
{
  return (jerryx_arg_t){ .func = func, .dest = dest, .extra_info = extra_info };
} /* jerryx_arg_custom */

/**
 * Create a jerryx_arg_t instance for object properties.
 *
 * @return a jerryx_arg_t instance.
 */
static inline jerryx_arg_t
jerryx_arg_object_properties (const jerryx_arg_object_props_t *object_props, /**< pointer to object property mapping */
                              jerryx_arg_optional_t opt_flag) /**< whether the argument is optional */
{
  jerryx_arg_transform_func_t func;

  if (opt_flag == JERRYX_ARG_OPTIONAL)
  {
    func = jerryx_arg_transform_object_props_optional;
  }
  else
  {
    func = jerryx_arg_transform_object_props;
  }

  return (jerryx_arg_t){ .func = func, .dest = NULL, .extra_info = (uintptr_t) object_props };
} /* jerryx_arg_object_properties */

/**
 * Create a jerryx_arg_t instance for array.
 *
 * @return a jerryx_arg_t instance.
 */
static inline jerryx_arg_t
jerryx_arg_array (const jerryx_arg_array_items_t *array_items_p, /**< pointer to array items mapping */
                  jerryx_arg_optional_t opt_flag) /**< whether the argument is optional */
{
  jerryx_arg_transform_func_t func;

  if (opt_flag == JERRYX_ARG_OPTIONAL)
  {
    func = jerryx_arg_transform_array_items_optional;
  }
  else
  {
    func = jerryx_arg_transform_array_items;
  }

  return (jerryx_arg_t){ .func = func, .dest = NULL, .extra_info = (uintptr_t) array_items_p };
} /* jerryx_arg_array */

#endif /* !JERRYX_ARG_IMPL_H */

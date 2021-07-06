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

#include <math.h>

#include "jerryscript-ext/arg.h"
#include "jerryscript.h"

/**
 * The common function to deal with optional arguments.
 * The core transform function is provided by argument `func`.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_optional (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                               const jerryx_arg_t *c_arg_p, /**< native arg */
                               jerryx_arg_transform_func_t func) /**< the core transform function */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_peek (js_arg_iter_p);

  if (jerry_value_is_undefined (js_arg))
  {
    return jerryx_arg_js_iterator_pop (js_arg_iter_p);
  }

  return func (js_arg_iter_p, c_arg_p);
} /* jerryx_arg_transform_optional */

/**
 * The common part in transforming a JS argument to a number (double or certain int) type.
 * Type coercion is not allowed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
static jerry_value_t
jerryx_arg_transform_number_strict_common (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                           double *number_p) /**< [out] the number in JS arg */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_pop (js_arg_iter_p);

  if (!jerry_value_is_number (js_arg))
  {
    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "It is not a number.");
  }

  *number_p = jerry_get_number_value (js_arg);

  return jerry_create_undefined ();
} /* jerryx_arg_transform_number_strict_common */

/**
 * The common part in transforming a JS argument to a number (double or certain int) type.
 * Type coercion is allowed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
static jerry_value_t
jerryx_arg_transform_number_common (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                    double *number_p) /**< [out] the number in JS arg */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_pop (js_arg_iter_p);

  jerry_value_t to_number = jerry_value_to_number (js_arg);

  if (jerry_value_is_error (to_number))
  {
    jerry_release_value (to_number);

    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "It can not be converted to a number.");
  }

  *number_p = jerry_get_number_value (to_number);
  jerry_release_value (to_number);

  return jerry_create_undefined ();
} /* jerryx_arg_transform_number_common */

/**
 * Transform a JS argument to a double. Type coercion is not allowed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_number_strict (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                    const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  return jerryx_arg_transform_number_strict_common (js_arg_iter_p, c_arg_p->dest);
} /* jerryx_arg_transform_number_strict */

/**
 * Transform a JS argument to a double. Type coercion is allowed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_number (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                             const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  return jerryx_arg_transform_number_common (js_arg_iter_p, c_arg_p->dest);
} /* jerryx_arg_transform_number */

/**
 * Helper function to process a double number before converting it
 * to an integer.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
static jerry_value_t
jerryx_arg_helper_process_double (double *d, /**< [in, out] the number to be processed */
                                  double min, /**< the min value for clamping */
                                  double max, /**< the max value for clamping */
                                  jerryx_arg_int_option_t option) /**< the converting policies */
{
  if (*d != *d) /* isnan (*d) triggers conversion warning on clang<9 */
  {
    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "The number is NaN.");
  }

  if (option.clamp == JERRYX_ARG_NO_CLAMP)
  {
    if (*d > max || *d < min)
    {
      return jerry_create_error (JERRY_ERROR_TYPE,
                                 (jerry_char_t *) "The number is out of range.");
    }
  }
  else
  {
    *d = *d < min ? min : *d;
    *d = *d > max ? max : *d;
  }

  if (option.round == JERRYX_ARG_ROUND)
  {
    *d = (*d >= 0.0) ? floor (*d + 0.5) : ceil (*d - 0.5);
  }
  else if (option.round == JERRYX_ARG_FLOOR)
  {
    *d = floor (*d);
  }
  else
  {
    *d = ceil (*d);
  }

  return jerry_create_undefined ();
} /* jerryx_arg_helper_process_double */

/**
 * Use the macro to define thr transform functions for int type.
 */
#define JERRYX_ARG_TRANSFORM_FUNC_FOR_INT_TEMPLATE(type, suffix, min, max) \
  jerry_value_t jerryx_arg_transform_ ## type ## suffix (jerryx_arg_js_iterator_t *js_arg_iter_p, \
                                                         const jerryx_arg_t *c_arg_p) \
  { \
    double tmp = 0.0; \
    jerry_value_t rv = jerryx_arg_transform_number ## suffix ## _common (js_arg_iter_p, &tmp); \
    if (jerry_value_is_error (rv)) \
    { \
      return rv; \
    } \
    jerry_release_value (rv); \
    union \
    { \
      jerryx_arg_int_option_t int_option; \
      uintptr_t extra_info; \
    } u = { .extra_info = c_arg_p->extra_info }; \
    rv = jerryx_arg_helper_process_double (&tmp, min, max, u.int_option); \
    if (jerry_value_is_error (rv)) \
    { \
      return rv; \
    } \
    *(type ## _t *) c_arg_p->dest = (type ## _t) tmp; \
    return rv; \
  }

#define JERRYX_ARG_TRANSFORM_FUNC_FOR_INT(type, min, max) \
  JERRYX_ARG_TRANSFORM_FUNC_FOR_INT_TEMPLATE (type, _strict, min, max) \
  JERRYX_ARG_TRANSFORM_FUNC_FOR_INT_TEMPLATE (type, , min, max)

JERRYX_ARG_TRANSFORM_FUNC_FOR_INT (uint8, 0, UINT8_MAX)
JERRYX_ARG_TRANSFORM_FUNC_FOR_INT (int8, INT8_MIN, INT8_MAX)
JERRYX_ARG_TRANSFORM_FUNC_FOR_INT (uint16, 0, UINT16_MAX)
JERRYX_ARG_TRANSFORM_FUNC_FOR_INT (int16, INT16_MIN, INT16_MAX)
JERRYX_ARG_TRANSFORM_FUNC_FOR_INT (uint32, 0, UINT32_MAX)
JERRYX_ARG_TRANSFORM_FUNC_FOR_INT (int32, INT32_MIN, INT32_MAX)

#undef JERRYX_ARG_TRANSFORM_FUNC_FOR_INT_TEMPLATE
#undef JERRYX_ARG_TRANSFORM_FUNC_FOR_INT
/**
 * Transform a JS argument to a boolean. Type coercion is not allowed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_boolean_strict (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                     const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_pop (js_arg_iter_p);

  if (!jerry_value_is_boolean (js_arg))
  {
    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "It is not a boolean.");
  }

  bool *dest = c_arg_p->dest;
  *dest = jerry_value_is_true (js_arg);

  return jerry_create_undefined ();
} /* jerryx_arg_transform_boolean_strict */

/**
 * Transform a JS argument to a boolean. Type coercion is allowed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_boolean (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                              const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_pop (js_arg_iter_p);

  bool to_boolean = jerry_value_to_boolean (js_arg);

  bool *dest = c_arg_p->dest;
  *dest = to_boolean;

  return jerry_create_undefined ();
} /* jerryx_arg_transform_boolean */

/**
 * The common routine for string transformer.
 * It works for both CESU-8 and UTF-8 string.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
static jerry_value_t
jerryx_arg_string_to_buffer_common_routine (jerry_value_t js_arg, /**< JS arg */
                                            const jerryx_arg_t *c_arg_p, /**< native arg */
                                            bool is_utf8) /**< whether it is UTF-8 string */
{
  jerry_char_t *target_p = (jerry_char_t *) c_arg_p->dest;
  jerry_size_t target_buf_size = (jerry_size_t) c_arg_p->extra_info;
  jerry_size_t size;
  jerry_length_t len;

  if (!is_utf8)
  {
    size = jerry_string_to_char_buffer (js_arg,
                                        target_p,
                                        target_buf_size);
    len = jerry_get_string_length (js_arg);
  }
  else
  {
    size = jerry_string_to_utf8_char_buffer (js_arg,
                                             target_p,
                                             target_buf_size);
    len = jerry_get_utf8_string_length (js_arg);
  }

  if ((size == target_buf_size) || (size == 0 && len != 0))
  {
    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "Buffer size is not large enough.");
  }

  target_p[size] = '\0';

  return jerry_create_undefined ();
} /* jerryx_arg_string_to_buffer_common_routine */

/**
 * Transform a JS argument to a UTF-8/CESU-8 char array. Type coercion is not allowed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
static jerry_value_t
jerryx_arg_transform_string_strict_common (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                           const jerryx_arg_t *c_arg_p, /**< the native arg */
                                           bool is_utf8) /**< whether it is a UTF-8 string */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_pop (js_arg_iter_p);

  if (!jerry_value_is_string (js_arg))
  {
    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "It is not a string.");
  }

  return jerryx_arg_string_to_buffer_common_routine (js_arg, c_arg_p, is_utf8);
} /* jerryx_arg_transform_string_strict_common */

/**
 * Transform a JS argument to a UTF-8/CESU-8 char array. Type coercion is allowed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
static jerry_value_t
jerryx_arg_transform_string_common (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                    const jerryx_arg_t *c_arg_p, /**< the native arg */
                                    bool is_utf8) /**< whether it is a UTF-8 string */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_pop (js_arg_iter_p);

  jerry_value_t to_string = jerry_value_to_string (js_arg);

  if (jerry_value_is_error (to_string))
  {
    jerry_release_value (to_string);

    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "It can not be converted to a string.");
  }

  jerry_value_t ret = jerryx_arg_string_to_buffer_common_routine (to_string, c_arg_p, is_utf8);
  jerry_release_value (to_string);

  return ret;
} /* jerryx_arg_transform_string_common */

/**
 * Transform a JS argument to a cesu8 char array. Type coercion is not allowed.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_string_strict (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                    const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  return jerryx_arg_transform_string_strict_common (js_arg_iter_p, c_arg_p, false);
} /* jerryx_arg_transform_string_strict */

/**
 * Transform a JS argument to a utf8 char array. Type coercion is not allowed.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_utf8_string_strict (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                         const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  return jerryx_arg_transform_string_strict_common (js_arg_iter_p, c_arg_p, true);
} /* jerryx_arg_transform_utf8_string_strict */

/**
 * Transform a JS argument to a cesu8 char array. Type coercion is allowed.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_string (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                             const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  return jerryx_arg_transform_string_common (js_arg_iter_p, c_arg_p, false);
} /* jerryx_arg_transform_string */

/**
 * Transform a JS argument to a utf8 char array. Type coercion is allowed.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_utf8_string (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                  const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  return jerryx_arg_transform_string_common (js_arg_iter_p, c_arg_p, true);
} /* jerryx_arg_transform_utf8_string */

/**
 * Check whether the JS argument is jerry function, if so, assign to the native argument.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_function (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                               const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_pop (js_arg_iter_p);

  if (!jerry_value_is_function (js_arg))
  {
    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "It is not a function.");
  }

  jerry_value_t *func_p = c_arg_p->dest;
  *func_p = jerry_acquire_value (js_arg);

  return jerry_create_undefined ();
} /* jerryx_arg_transform_function */

/**
 * Check whether the native pointer has the expected type info.
 * If so, assign it to the native argument.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_native_pointer (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                     const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_pop (js_arg_iter_p);

  if (!jerry_value_is_object (js_arg))
  {
    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "It is not an object.");
  }

  const jerry_object_native_info_t *expected_info_p;
  expected_info_p = (const jerry_object_native_info_t *) c_arg_p->extra_info;
  void **ptr_p = (void **) c_arg_p->dest;
  bool is_ok = jerry_get_object_native_pointer (js_arg, ptr_p, expected_info_p);

  if (!is_ok)
  {
    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "The object has no native pointer or type does not match.");
  }

  return jerry_create_undefined ();
} /* jerryx_arg_transform_native_pointer */

/**
 * Check whether the JS object's properties have expected types, and transform them into native args.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_object_props (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                   const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_pop (js_arg_iter_p);

  const jerryx_arg_object_props_t *object_props = (const jerryx_arg_object_props_t *) c_arg_p->extra_info;

  return jerryx_arg_transform_object_properties (js_arg,
                                                 object_props->name_p,
                                                 object_props->name_cnt,
                                                 object_props->c_arg_p,
                                                 object_props->c_arg_cnt);
} /* jerryx_arg_transform_object_props */

/**
 * Check whether the JS array's items have expected types, and transform them into native args.
 *
 * @return jerry undefined: the transformer passes,
 *         jerry error: the transformer fails.
 */
jerry_value_t
jerryx_arg_transform_array_items (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                                  const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_pop (js_arg_iter_p);

  const jerryx_arg_array_items_t *array_items_p = (const jerryx_arg_array_items_t *) c_arg_p->extra_info;

  return jerryx_arg_transform_array (js_arg,
                                     array_items_p->c_arg_p,
                                     array_items_p->c_arg_cnt);
} /* jerryx_arg_transform_array_items */

/**
 * Define transformer for optional argument.
 */
#define JERRYX_ARG_TRANSFORM_OPTIONAL(type) \
  jerry_value_t \
  jerryx_arg_transform_ ## type ## _optional (jerryx_arg_js_iterator_t *js_arg_iter_p, \
                                              const jerryx_arg_t *c_arg_p) \
  { \
    return jerryx_arg_transform_optional (js_arg_iter_p, c_arg_p, jerryx_arg_transform_ ## type); \
  }

JERRYX_ARG_TRANSFORM_OPTIONAL (number)
JERRYX_ARG_TRANSFORM_OPTIONAL (number_strict)
JERRYX_ARG_TRANSFORM_OPTIONAL (boolean)
JERRYX_ARG_TRANSFORM_OPTIONAL (boolean_strict)
JERRYX_ARG_TRANSFORM_OPTIONAL (string)
JERRYX_ARG_TRANSFORM_OPTIONAL (string_strict)
JERRYX_ARG_TRANSFORM_OPTIONAL (utf8_string)
JERRYX_ARG_TRANSFORM_OPTIONAL (utf8_string_strict)
JERRYX_ARG_TRANSFORM_OPTIONAL (function)
JERRYX_ARG_TRANSFORM_OPTIONAL (native_pointer)
JERRYX_ARG_TRANSFORM_OPTIONAL (object_props)
JERRYX_ARG_TRANSFORM_OPTIONAL (array_items)

JERRYX_ARG_TRANSFORM_OPTIONAL (uint8)
JERRYX_ARG_TRANSFORM_OPTIONAL (uint16)
JERRYX_ARG_TRANSFORM_OPTIONAL (uint32)
JERRYX_ARG_TRANSFORM_OPTIONAL (int8)
JERRYX_ARG_TRANSFORM_OPTIONAL (int16)
JERRYX_ARG_TRANSFORM_OPTIONAL (int32)
JERRYX_ARG_TRANSFORM_OPTIONAL (int8_strict)
JERRYX_ARG_TRANSFORM_OPTIONAL (int16_strict)
JERRYX_ARG_TRANSFORM_OPTIONAL (int32_strict)
JERRYX_ARG_TRANSFORM_OPTIONAL (uint8_strict)
JERRYX_ARG_TRANSFORM_OPTIONAL (uint16_strict)
JERRYX_ARG_TRANSFORM_OPTIONAL (uint32_strict)

#undef JERRYX_ARG_TRANSFORM_OPTIONAL

/**
 * Ignore the JS argument.
 *
 * @return jerry undefined
 */
jerry_value_t
jerryx_arg_transform_ignore (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                             const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  (void) js_arg_iter_p; /* unused */
  (void) c_arg_p; /* unused */

  return jerry_create_undefined ();
} /* jerryx_arg_transform_ignore */

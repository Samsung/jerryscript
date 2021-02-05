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

#include "jerryscript-ext/arg.h"
#include "arg-internal.h"
#include "jerryscript.h"
#include "jext-common.h"

JERRYX_STATIC_ASSERT (sizeof (jerryx_arg_int_option_t) <= sizeof (((jerryx_arg_t *) 0)->extra_info),
                      jerryx_arg_number_options_t_must_fit_into_extra_info);

/**
 * Validate the JS arguments and assign them to the native arguments.
 *
 * @return jerry undefined: all validators passed,
 *         jerry error: a validator failed.
 */
jerry_value_t
jerryx_arg_transform_args (const jerry_value_t *js_arg_p, /**< points to the array with JS arguments */
                           const jerry_length_t js_arg_cnt, /**< the count of the `js_arg_p` array */
                           const jerryx_arg_t *c_arg_p, /**< points to the array of validation/transformation steps */
                           jerry_length_t c_arg_cnt) /**< the count of the `c_arg_p` array */
{
  jerry_value_t ret = jerry_create_undefined ();

  jerryx_arg_js_iterator_t iterator = { .js_arg_p = js_arg_p, .js_arg_cnt = js_arg_cnt, .js_arg_idx = 0 };

  for (; c_arg_cnt != 0 && !jerry_value_is_error (ret); c_arg_cnt--, c_arg_p++)
  {
    ret = c_arg_p->func (&iterator, c_arg_p);
  }

  return ret;
} /* jerryx_arg_transform_args */

/**
 * Validate the this value and the JS arguments,
 * and assign them to the native arguments.
 * This function is useful to perform input validation inside external
 * function handlers (see jerry_external_handler_t).
 * @note this_val is processed as the first value, before the array of arguments.
 *
 * @return jerry undefined: all validators passed,
 *         jerry error: a validator failed.
 */
jerry_value_t
jerryx_arg_transform_this_and_args (const jerry_value_t this_val, /**< the this_val for the external function */
                                    const jerry_value_t *js_arg_p, /**< points to the array with JS arguments */
                                    const jerry_length_t js_arg_cnt, /**< the count of the `js_arg_p` array */
                                    const jerryx_arg_t *c_arg_p, /**< points to the array of transformation steps */
                                    jerry_length_t c_arg_cnt) /**< the count of the `c_arg_p` array */
{
  if (c_arg_cnt == 0)
  {
    return jerry_create_undefined ();
  }

  jerryx_arg_js_iterator_t iterator = { .js_arg_p = &this_val, .js_arg_cnt = 1, .js_arg_idx = 0 };

  jerry_value_t ret = c_arg_p->func (&iterator, c_arg_p);

  if (jerry_value_is_error (ret))
  {
    jerry_release_value (ret);

    return jerry_create_error (JERRY_ERROR_TYPE, (jerry_char_t *) "'this' validation failed.");
  }

  return jerryx_arg_transform_args (js_arg_p, js_arg_cnt, c_arg_p + 1, c_arg_cnt - 1);
} /* jerryx_arg_transform_this_and_args */

/**
 * Validate the `obj_val`'s properties,
 * and assign them to the native arguments.
 *
 * @return jerry undefined: all validators passed,
 *         jerry error: a validator failed.
 */
jerry_value_t
jerryx_arg_transform_object_properties (const jerry_value_t obj_val, /**< the JS object */
                                        const jerry_char_t **name_p, /**< property name list of the JS object */
                                        const jerry_length_t name_cnt, /**< count of the name list */
                                        const jerryx_arg_t *c_arg_p, /**< points to the array of transformation steps */
                                        jerry_length_t c_arg_cnt) /**< the count of the `c_arg_p` array */
{
  if (!jerry_value_is_object (obj_val))
  {
    return jerry_create_error (JERRY_ERROR_TYPE, (jerry_char_t *) "Not an object.");
  }

  JERRY_VLA (jerry_value_t, prop, name_cnt);

  for (jerry_length_t i = 0; i < name_cnt; i++, name_p++)
  {
    const jerry_value_t name_str = jerry_create_string (*name_p);
    prop[i] = jerry_get_property (obj_val, name_str);
    jerry_release_value (name_str);

    if (jerry_value_is_error (prop[i]))
    {
      for (jerry_length_t j = 0; j < i; j++)
      {
        jerry_release_value (prop[j]);
      }

      return prop[i];
    }
  }

  const jerry_value_t ret = jerryx_arg_transform_args (prop, name_cnt, c_arg_p, c_arg_cnt);

  for (jerry_length_t i = 0; i < name_cnt; i++)
  {
    jerry_release_value (prop[i]);
  }

  return ret;
} /* jerryx_arg_transform_object_properties */

/**
 * Validate the items in the JS array and assign them to the native arguments.
 *
 * @return jerry undefined: all validators passed,
 *         jerry error: a validator failed.
 */
jerry_value_t
jerryx_arg_transform_array (const jerry_value_t array_val, /**< points to the JS array */
                            const jerryx_arg_t *c_arg_p, /**< points to the array of validation/transformation steps */
                            jerry_length_t c_arg_cnt) /**< the count of the `c_arg_p` array */
{
  if (!jerry_value_is_array (array_val))
  {
    return jerry_create_error (JERRY_ERROR_TYPE, (jerry_char_t *) "Not an array.");
  }

  JERRY_VLA (jerry_value_t, arr, c_arg_cnt);

  for (jerry_length_t i = 0; i < c_arg_cnt; i++)
  {
    arr[i] = jerry_get_property_by_index (array_val, i);

    if (jerry_value_is_error (arr[i]))
    {
      for (jerry_length_t j = 0; j < i; j++)
      {
        jerry_release_value (arr[j]);
      }

      return arr[i];
    }
  }

  const jerry_value_t ret = jerryx_arg_transform_args (arr, c_arg_cnt, c_arg_p, c_arg_cnt);

  for (jerry_length_t i = 0; i < c_arg_cnt; i++)
  {
    jerry_release_value (arr[i]);
  }

  return ret;
} /* jerryx_arg_transform_array */

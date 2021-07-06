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

#include "jerryscript-ext/handler.h"

/**
 * Register a JavaScript function in the global object.
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return true value - if the operation was successful,
 *         error - otherwise.
 */
jerry_value_t
jerryx_handler_register_global (const jerry_char_t *name_p, /**< name of the function */
                                jerry_external_handler_t handler_p) /**< function callback */
{
  jerry_value_t global_obj_val = jerry_get_global_object ();
  jerry_value_t function_name_val = jerry_create_string (name_p);
  jerry_value_t function_val = jerry_create_external_function (handler_p);

  jerry_value_t result_val = jerry_set_property (global_obj_val, function_name_val, function_val);

  jerry_release_value (function_val);
  jerry_release_value (function_name_val);
  jerry_release_value (global_obj_val);

  return result_val;
} /* jerryx_handler_register_global */

/**
 * Set multiple properties on a target object.
 *
 * The properties are an array of (name, property value) pairs and
 * this list must end with a (NULL, 0) entry.
 *
 * Notes:
 *  - Each property value in the input array is released after a successful property registration.
 *  - The property name must be a zero terminated UTF-8 string.
 *  - There should be no '\0' (NULL) character in the name excluding the string terminator.
 *  - The method `jerryx_release_property_entry` must be called if there is any failed registration
 *    to release the values in the entries array.
 *
 * @return `jerryx_register_result` struct - if everything is ok with the (undefined, property entry count) values.
 *         In case of error the (error object, registered property count) pair.
 */
jerryx_register_result
jerryx_set_properties (const jerry_value_t target_object, /**< target object */
                       const jerryx_property_entry entries[]) /**< array of method entries */
{
#define JERRYX_SET_PROPERTIES_RESULT(VALUE, IDX) ((jerryx_register_result) { VALUE, IDX })
  uint32_t idx = 0;

  if (entries == NULL)
  {
    return JERRYX_SET_PROPERTIES_RESULT (jerry_create_undefined (), 0);
  }

  for (; (entries[idx].name != NULL); idx++)
  {
    const jerryx_property_entry *entry = &entries[idx];

    jerry_value_t prop_name = jerry_create_string_from_utf8 ((const jerry_char_t *) entry->name);
    jerry_value_t result = jerry_set_property (target_object, prop_name, entry->value);

    jerry_release_value (prop_name);

    // By API definition:
    // The jerry_set_property returns TRUE if there is no problem
    // and error object if there is any problem.
    // Thus there is no need to check if the boolean value is false or not.
    if (!jerry_value_is_boolean (result))
    {
      return JERRYX_SET_PROPERTIES_RESULT (result, idx);
    }

    jerry_release_value (entry->value);
    jerry_release_value (result);
  }

  return JERRYX_SET_PROPERTIES_RESULT (jerry_create_undefined (), idx);
#undef JERRYX_SET_PROPERTIES_RESULT
} /* jerryx_set_properties */

/**
 * Release all jerry_value_t in a jerryx_property_entry array based on
 * a previous jerryx_set_properties call.
 *
 * In case of a successful registration it is safe to call this method.
 */
void
jerryx_release_property_entry (const jerryx_property_entry entries[], /**< list of property entries */
                               const jerryx_register_result register_result) /**< previous result of registration */
{
  if (entries == NULL)
  {
    return;
  }

  for (uint32_t idx = register_result.registered; entries[idx].name != NULL; idx++)
  {
    jerry_release_value (entries[idx].value);
  }
} /* jerryx_release_property_entry */

/**
 * Set a property to a specified value with a given name.
 *
 * Notes:
 *   - The operation performed is the same as what the `jerry_set_property` method.
 *   - The property name must be a zero terminated UTF-8 string.
 *   - There should be no '\0' (NULL) character in the name excluding the string terminator.
 *   - Returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return true value - if the operation was successful
 *         thrown error - otherwise
 */
jerry_value_t
jerryx_set_property_str (const jerry_value_t target_object, /**< target object */
                         const char *name, /**< property name */
                         const jerry_value_t value) /**< value to set */
{
  jerry_value_t property_name_val = jerry_create_string_from_utf8 ((const jerry_char_t *) name);
  jerry_value_t result_val = jerry_set_property (target_object, property_name_val, value);

  jerry_release_value (property_name_val);

  return result_val;
} /* jerryx_set_property_str */

/**
 * Get a property value of a specified object.
 *
 * Notes:
 *   - The operation performed is the same as what the `jerry_get_property` method.
 *   - The property name must be a zero terminated UTF-8 string.
 *   - There should be no '\0' (NULL) character in the name excluding the string terminator.
 *   - Returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return jerry_value_t - the property value
 */
jerry_value_t
jerryx_get_property_str (const jerry_value_t target_object, /**< target object */
                         const char *name) /**< property name */
{
  jerry_value_t prop_name = jerry_create_string_from_utf8 ((const jerry_char_t *) name);
  jerry_value_t result_val = jerry_get_property (target_object, prop_name);
  jerry_release_value (prop_name);

  return result_val;
} /* jerryx_get_property_str */

/**
 * Check if a property exists on an object.
 *
 * Notes:
 *   - The operation performed is the same as what the `jerry_has_property` method.
 *   - The property name must be a zero terminated UTF-8 string.
 *   - There should be no '\0' (NULL) character in the name excluding the string terminator.
 *
 * @return true - if the property exists on the given object.
 *         false - if there is no such property or there was an error accessing the property.
 */
bool
jerryx_has_property_str (const jerry_value_t target_object, /**< target object */
                         const char *name) /**< property name */
{
  bool has_property = false;

  jerry_value_t prop_name = jerry_create_string_from_utf8 ((const jerry_char_t *) name);
  jerry_value_t has_prop_val = jerry_has_property (target_object, prop_name);

  if (!jerry_value_is_error (has_prop_val))
  {
    has_property = jerry_value_is_true (has_prop_val);
  }

  jerry_release_value (has_prop_val);
  jerry_release_value (prop_name);

  return has_property;
} /* jerryx_has_property_str */

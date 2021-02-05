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

#include "jerryscript-port-default.h"
#include "jerryscript-port.h"
#include "jerryscript.h"
#include "test-common.h"

static const char *prop_names[] = { "val1", "val2", "val3", "val4", "val5", "37", "symbol" };

static jerry_char_t buffer[256] = { 0 };

static void
create_and_set_property (const jerry_value_t object, const char *prop_name)
{
  jerry_value_t jprop_name = jerry_create_string ((const jerry_char_t *) prop_name);
  jerry_value_t ret_val = jerry_set_property (object, jprop_name, jerry_create_undefined ());

  jerry_release_value (jprop_name);
  jerry_release_value (ret_val);
} /* create_and_set_property */

static void
compare_prop_name (const jerry_value_t object, const char *prop_name, uint32_t idx)
{
  jerry_value_t name = jerry_get_property_by_index (object, idx);
  TEST_ASSERT (jerry_value_is_string (name) || jerry_value_is_number (name));
  if (jerry_value_is_string (name))
  {
    jerry_size_t name_size = jerry_get_string_size (name);
    TEST_ASSERT (name_size < sizeof (buffer));
    jerry_size_t ret_size = jerry_string_to_char_buffer (name, buffer, sizeof (buffer));
    TEST_ASSERT (name_size == ret_size);
    buffer[name_size] = '\0';
    TEST_ASSERT (strcmp ((const char *) buffer, prop_name) == 0);
  }
  else
  {
    TEST_ASSERT ((int) jerry_get_number_value (name) == atoi (prop_name));
  }

  jerry_release_value (name);
} /* compare_prop_name */

static void
define_property (const jerry_value_t object,
                 const char *prop_name,
                 jerry_property_descriptor_t *prop_desc_p,
                 bool is_symbol)
{
  jerry_value_t jname = jerry_create_string ((const jerry_char_t *) prop_name);
  jerry_value_t ret_val;
  if (is_symbol)
  {
    jerry_value_t symbol = jerry_create_symbol (jname);
    ret_val = jerry_define_own_property (object, symbol, prop_desc_p);
    jerry_release_value (symbol);
  }
  else
  {
    ret_val = jerry_define_own_property (object, jname, prop_desc_p);
  }

  jerry_release_value (jname);
  jerry_release_value (ret_val);
} /* define_property */

int
main (void)
{
  if (!jerry_is_feature_enabled (JERRY_FEATURE_SYMBOL))
  {
    return 0;
  }

  TEST_INIT ();
  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t error_value = jerry_object_get_property_names (jerry_create_undefined (), JERRY_PROPERTY_FILTER_ALL);
  TEST_ASSERT (jerry_value_is_error (error_value) && jerry_get_error_type (error_value) == JERRY_ERROR_TYPE);
  jerry_release_value (error_value);

  jerry_value_t test_object = jerry_create_object ();
  create_and_set_property (test_object, prop_names[0]);
  create_and_set_property (test_object, prop_names[1]);

  jerry_value_t names;

  jerry_property_descriptor_t prop_desc;
  jerry_init_property_descriptor_fields (&prop_desc);
  prop_desc.is_configurable_defined = true;
  prop_desc.is_configurable = true;
  prop_desc.is_writable_defined = true;
  prop_desc.is_writable = true;
  prop_desc.is_enumerable_defined = true;

  // Test enumerable - non-enumerable filter
  prop_desc.is_enumerable = false;
  define_property (test_object, prop_names[2], &prop_desc, false);
  names = jerry_object_get_property_names (test_object,
                                           JERRY_PROPERTY_FILTER_ALL | JERRY_PROPERTY_FILTER_EXLCUDE_NON_ENUMERABLE);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 2);
  jerry_release_value (names);
  names = jerry_object_get_property_names (test_object, JERRY_PROPERTY_FILTER_ALL);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 3);
  compare_prop_name (names, prop_names[2], 2);
  jerry_release_value (names);
  prop_desc.is_enumerable = true;

  // Test configurable - non-configurable filter
  prop_desc.is_configurable = false;
  define_property (test_object, prop_names[3], &prop_desc, false);
  names = jerry_object_get_property_names (test_object,
                                           JERRY_PROPERTY_FILTER_ALL | JERRY_PROPERTY_FILTER_EXLCUDE_NON_CONFIGURABLE);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 3);
  jerry_release_value (names);
  names = jerry_object_get_property_names (test_object, JERRY_PROPERTY_FILTER_ALL);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 4);
  compare_prop_name (names, prop_names[3], 3);
  jerry_release_value (names);
  prop_desc.is_configurable = true;

  // Test writable - non-writable filter
  prop_desc.is_writable = false;
  define_property (test_object, prop_names[4], &prop_desc, false);
  names = jerry_object_get_property_names (test_object,
                                           JERRY_PROPERTY_FILTER_ALL | JERRY_PROPERTY_FILTER_EXLCUDE_NON_WRITABLE);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 4);
  jerry_release_value (names);
  names = jerry_object_get_property_names (test_object, JERRY_PROPERTY_FILTER_ALL);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 5);
  compare_prop_name (names, prop_names[4], 4);
  jerry_release_value (names);
  prop_desc.is_writable = true;

  // Test all property filter
  names = jerry_object_get_property_names (test_object, JERRY_PROPERTY_FILTER_ALL);
  jerry_length_t array_len = jerry_get_array_length (names);
  TEST_ASSERT (array_len == (uint32_t) 5);

  for (uint32_t i = 0; i < array_len; i++)
  {
    compare_prop_name (names, prop_names[i], i);
  }

  jerry_release_value (names);

  // Test number and string index exclusion
  define_property (test_object, prop_names[5], &prop_desc, false);
  names = jerry_object_get_property_names (test_object,
                                           JERRY_PROPERTY_FILTER_ALL | JERRY_PROPERTY_FILTER_EXLCUDE_STRINGS
                                             | JERRY_PROPERTY_FILTER_INTEGER_INDICES_AS_NUMBER);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 1);
  compare_prop_name (names, prop_names[5], 0);
  jerry_release_value (names);
  names = jerry_object_get_property_names (test_object,
                                           JERRY_PROPERTY_FILTER_ALL | JERRY_PROPERTY_FILTER_EXLCUDE_INTEGER_INDICES);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 5);
  jerry_release_value (names);

  // Test prototype chain traversion
  names = jerry_object_get_property_names (test_object, JERRY_PROPERTY_FILTER_ALL);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 6);
  jerry_release_value (names);
  names = jerry_object_get_property_names (test_object,
                                           JERRY_PROPERTY_FILTER_ALL | JERRY_PROPERTY_FILTER_TRAVERSE_PROTOTYPE_CHAIN);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 18);
  jerry_release_value (names);

  // Test symbol exclusion
  define_property (test_object, prop_names[6], &prop_desc, true);
  names =
    jerry_object_get_property_names (test_object, JERRY_PROPERTY_FILTER_ALL | JERRY_PROPERTY_FILTER_EXLCUDE_SYMBOLS);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 6);
  jerry_release_value (names);
  names = jerry_object_get_property_names (test_object, JERRY_PROPERTY_FILTER_ALL);
  TEST_ASSERT (jerry_get_array_length (names) == (uint32_t) 7);
  jerry_release_value (names);

  jerry_free_property_descriptor_fields (&prop_desc);
  jerry_release_value (test_object);
  jerry_cleanup ();
  return 0;
} /* main */

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

#include "jerryscript.h"
#include "test-common.h"

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  /* Test: init property descriptor */
  jerry_property_descriptor_t prop_desc;
  jerry_init_property_descriptor_fields (&prop_desc);
  TEST_ASSERT (prop_desc.is_value_defined == false);
  TEST_ASSERT (jerry_value_is_undefined (prop_desc.value));
  TEST_ASSERT (prop_desc.is_writable_defined == false);
  TEST_ASSERT (prop_desc.is_writable == false);
  TEST_ASSERT (prop_desc.is_enumerable_defined == false);
  TEST_ASSERT (prop_desc.is_enumerable == false);
  TEST_ASSERT (prop_desc.is_configurable_defined == false);
  TEST_ASSERT (prop_desc.is_configurable == false);
  TEST_ASSERT (prop_desc.is_get_defined == false);
  TEST_ASSERT (jerry_value_is_undefined (prop_desc.getter));
  TEST_ASSERT (prop_desc.is_set_defined == false);
  TEST_ASSERT (jerry_value_is_undefined (prop_desc.setter));

  /* Test: define own properties */
  jerry_value_t global_obj_val = jerry_get_global_object ();
  jerry_value_t prop_name = jerry_create_string ((const jerry_char_t *) "my_defined_property");
  prop_desc.is_value_defined = true;
  prop_desc.value = jerry_acquire_value (prop_name);
  jerry_value_t res = jerry_define_own_property (global_obj_val, prop_name, &prop_desc);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_boolean (res));
  TEST_ASSERT (jerry_get_boolean_value (res));
  jerry_release_value (res);
  jerry_free_property_descriptor_fields (&prop_desc);

  /* Test: get own property descriptor */
  bool is_ok = jerry_get_own_property_descriptor (global_obj_val, prop_name, &prop_desc);
  TEST_ASSERT (is_ok);
  TEST_ASSERT (prop_desc.is_value_defined == true);
  TEST_ASSERT (jerry_value_is_string (prop_desc.value));
  TEST_ASSERT (prop_desc.is_writable == false);
  TEST_ASSERT (prop_desc.is_enumerable == false);
  TEST_ASSERT (prop_desc.is_configurable == false);
  TEST_ASSERT (prop_desc.is_get_defined == false);
  TEST_ASSERT (jerry_value_is_undefined (prop_desc.getter));
  TEST_ASSERT (prop_desc.is_set_defined == false);
  TEST_ASSERT (jerry_value_is_undefined (prop_desc.setter));
  jerry_free_property_descriptor_fields (&prop_desc);

  if (jerry_is_feature_enabled (JERRY_FEATURE_PROXY))
  {
    /* Note: update this test when the internal method is implemented */
    jerry_value_t target = jerry_create_object ();
    jerry_value_t handler = jerry_create_object ();
    jerry_value_t proxy = jerry_create_proxy (target, handler);

    jerry_release_value (target);
    jerry_release_value (handler);
    is_ok = jerry_get_own_property_descriptor (proxy, prop_name, &prop_desc);
    TEST_ASSERT (!is_ok);
    jerry_release_value (proxy);
  }

  jerry_release_value (prop_name);

  /* Test: define and get own property descriptor */
  prop_desc.is_enumerable = true;
  prop_name = jerry_create_string ((const jerry_char_t *) "enumerable-property");
  res = jerry_define_own_property (global_obj_val, prop_name, &prop_desc);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_boolean (res));
  TEST_ASSERT (jerry_get_boolean_value (res));
  jerry_release_value (res);
  jerry_free_property_descriptor_fields (&prop_desc);
  is_ok = jerry_get_own_property_descriptor (global_obj_val, prop_name, &prop_desc);
  TEST_ASSERT (is_ok);
  TEST_ASSERT (prop_desc.is_writable == false);
  TEST_ASSERT (prop_desc.is_enumerable == true);
  TEST_ASSERT (prop_desc.is_configurable == false);

  jerry_release_value (prop_name);
  jerry_release_value (global_obj_val);

  /* Test: define own property descriptor error */
  jerry_init_property_descriptor_fields (&prop_desc);
  prop_desc.is_value_defined = true;
  prop_desc.value = jerry_create_number (11);

  jerry_value_t obj_val = jerry_create_object ();
  prop_name = jerry_create_string ((const jerry_char_t *) "property_key");
  res = jerry_define_own_property (obj_val, prop_name, &prop_desc);
  TEST_ASSERT (!jerry_value_is_error (res));
  jerry_release_value (res);

  jerry_release_value (prop_desc.value);
  prop_desc.value = jerry_create_number (22);
  res = jerry_define_own_property (obj_val, prop_name, &prop_desc);
  TEST_ASSERT (jerry_value_is_error (res));
  jerry_release_value (res);

  jerry_release_value (prop_name);
  jerry_release_value (obj_val);

  jerry_cleanup ();

  return 0;
} /* main */

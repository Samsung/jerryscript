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

#include "config.h"
#include "jerryscript.h"
#include "test-common.h"

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t object = jerry_create_object ();
  jerry_value_t prop_name = jerry_create_string_from_utf8 ((jerry_char_t *) "length");
  jerry_value_t value = jerry_create_boolean (true);

  TEST_ASSERT (jerry_set_property (object, prop_name, prop_name));
  TEST_ASSERT (jerry_has_property (object, prop_name));
  TEST_ASSERT (jerry_has_own_property (object, prop_name));

  jerry_property_descriptor_t prop_desc;
  TEST_ASSERT (jerry_get_own_property_descriptor (object, prop_name, &prop_desc));

  jerry_value_t from_object = jerry_from_property_descriptor (&prop_desc);

  prop_name = jerry_create_string_from_utf8 ((jerry_char_t *) "value");
  value = jerry_get_property (from_object, prop_name);
  TEST_ASSERT (value == prop_desc.value);

  prop_name = jerry_create_string_from_utf8 ((jerry_char_t *) "writable");
  value = jerry_get_property (from_object, prop_name);
  TEST_ASSERT (jerry_value_is_true (value) == ((prop_desc.flags & JERRY_PROP_IS_WRITABLE) != 0));

  prop_name = jerry_create_string_from_utf8 ((jerry_char_t *) "enumerable");
  value = jerry_get_property (from_object, prop_name);
  TEST_ASSERT (jerry_value_is_true (value) == ((prop_desc.flags & JERRY_PROP_IS_ENUMERABLE) != 0));

  prop_name = jerry_create_string_from_utf8 ((jerry_char_t *) "configurable");
  value = jerry_get_property (from_object, prop_name);
  TEST_ASSERT (jerry_value_is_true (value) == ((prop_desc.flags & JERRY_PROP_IS_CONFIGURABLE) != 0));

  jerry_release_value (object);
  jerry_release_value (prop_name);
  jerry_release_value (value);
  jerry_release_value (from_object);
  jerry_property_descriptor_free (&prop_desc);

  prop_desc.flags = JERRY_PROP_IS_CONFIGURABLE;
  from_object = jerry_from_property_descriptor (&prop_desc);
  TEST_ASSERT (jerry_value_is_error (from_object));
  jerry_release_value (from_object);

  prop_desc.flags = JERRY_PROP_IS_ENUMERABLE;
  from_object = jerry_from_property_descriptor (&prop_desc);
  TEST_ASSERT (jerry_value_is_error (from_object));
  jerry_release_value (from_object);

  prop_desc.flags = JERRY_PROP_IS_WRITABLE;
  from_object = jerry_from_property_descriptor (&prop_desc);
  TEST_ASSERT (jerry_value_is_error (from_object));
  jerry_release_value (from_object);

  jerry_cleanup ();
  return 0;
} /* main */

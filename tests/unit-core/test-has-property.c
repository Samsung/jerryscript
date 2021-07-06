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

static void
assert_boolean_and_release (jerry_value_t result, bool expected)
{
  TEST_ASSERT (jerry_value_is_boolean (result));
  TEST_ASSERT (jerry_value_is_true (result) == expected);
  jerry_release_value (result);
} /* assert_boolean_and_release */

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t object = jerry_create_object ();
  jerry_value_t prop_name = jerry_create_string_from_utf8 ((jerry_char_t *) "something");
  jerry_value_t prop_value = jerry_create_boolean (true);
  jerry_value_t proto_object = jerry_create_object ();

  /* Assert that an empty object does not have the property in question */
  assert_boolean_and_release (jerry_has_property (object, prop_name), false);
  assert_boolean_and_release (jerry_has_own_property (object, prop_name), false);

  assert_boolean_and_release (jerry_set_prototype (object, proto_object), true);

  /* If the object has a prototype, that still means it doesn't have the property */
  assert_boolean_and_release (jerry_has_property (object, prop_name), false);
  assert_boolean_and_release (jerry_has_own_property (object, prop_name), false);

  assert_boolean_and_release (jerry_set_property (proto_object, prop_name, prop_value), true);

  /* After setting the property on the prototype, it must be there, but not on the object */
  assert_boolean_and_release (jerry_has_property (object, prop_name), true);
  assert_boolean_and_release (jerry_has_own_property (object, prop_name), false);

  TEST_ASSERT (jerry_delete_property (proto_object, prop_name));
  assert_boolean_and_release (jerry_set_property (object, prop_name, prop_value), true);

  /* After relocating the property onto the object, it must be there */
  assert_boolean_and_release (jerry_has_property (object, prop_name), true);
  assert_boolean_and_release (jerry_has_own_property (object, prop_name), true);

  jerry_release_value (object);
  jerry_release_value (prop_name);
  jerry_release_value (prop_value);
  jerry_release_value (proto_object);

  jerry_cleanup ();

  return 0;
} /* main */

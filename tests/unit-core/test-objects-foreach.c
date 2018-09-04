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

static int test_data = 1;

static void free_test_data (void *data_p)
{
  TEST_ASSERT ((int *) data_p == &test_data);
} /* free_test_data */

static const jerry_object_native_info_t test_info =
{
  .free_cb = free_test_data
};

static const char *strict_equal_source = "var x = function(a, b) {return a === b;}; x";

static bool
find_test_object_by_data (const jerry_value_t candidate,
                          void *object_data_p,
                          void *context_p)
{
  if (object_data_p == &test_data)
  {
    *((jerry_value_t *) context_p) = jerry_acquire_value (candidate);
    return false;
  }
  return true;
} /* find_test_object_by_data */

static bool
find_test_object_by_property (const jerry_value_t candidate,
                              void *context_p)
{
  jerry_value_t *args_p = (jerry_value_t *) context_p;
  jerry_value_t result = jerry_has_property (candidate, args_p[0]);

  bool has_property = (!jerry_value_is_error (result) && jerry_get_boolean_value (result));

  /* If the object has the desired property, store a new reference to it in args_p[1]. */
  if (has_property)
  {
    args_p[1] = jerry_acquire_value (candidate);
  }

  jerry_release_value (result);

  /* Stop iterating if we've found our object. */
  return !has_property;
} /* find_test_object_by_property */

int
main (void)
{
  jerry_init (JERRY_INIT_DEFAULT);

  /* Render strict-equal as a function. */
  jerry_value_t parse_result = jerry_parse (NULL,
                                            0,
                                            (jerry_char_t *) strict_equal_source,
                                            strlen (strict_equal_source),
                                            JERRY_PARSE_STRICT_MODE);
  TEST_ASSERT (!jerry_value_is_error (parse_result));
  jerry_value_t strict_equal = jerry_run (parse_result);
  TEST_ASSERT (!jerry_value_is_error (strict_equal));
  jerry_release_value (parse_result);

  /* Create an object and associate some native data with it. */
  jerry_value_t object = jerry_create_object ();
  jerry_set_object_native_pointer (object, &test_data, &test_info);

  /* Retrieve the object by its native pointer. */

  jerry_value_t found_object;
  TEST_ASSERT (jerry_objects_foreach_by_native_info (&test_info, find_test_object_by_data, &found_object));
  jerry_value_t args[2] = {object, found_object};

  /* Assert that the correct object was retrieved. */
  jerry_value_t undefined = jerry_create_undefined ();
  jerry_value_t strict_equal_result = jerry_call_function (strict_equal, undefined, args, 2);
  TEST_ASSERT (jerry_value_is_boolean (strict_equal_result) && jerry_get_boolean_value (strict_equal_result));
  jerry_release_value (strict_equal_result);
  jerry_release_value (found_object);
  jerry_release_value (object);

  /* Collect garbage. */
  jerry_gc (JERRY_GC_SEVERITY_LOW);

  /* Attempt to retrieve the object by its native pointer again. */
  TEST_ASSERT (!jerry_objects_foreach_by_native_info (&test_info, find_test_object_by_data, &found_object));

  /* Create an object and set a property on it. */
  object = jerry_create_object ();
  jerry_value_t property_name = jerry_create_string ((jerry_char_t *) "xyzzy");
  jerry_value_t property_value = jerry_create_number (42);
  jerry_release_value (jerry_set_property (object, property_name, property_value));
  jerry_release_value (property_value);

  /* Retrieve the object by the presence of its property, placing it at args[1]. */
  args[0] = property_name;
  TEST_ASSERT (jerry_objects_foreach (find_test_object_by_property, args));

  /* Assert that the right object was retrieved and release both the original reference to it and the retrieved one. */
  args[0] = object;
  strict_equal_result = jerry_call_function (strict_equal, undefined, args, 2);
  TEST_ASSERT (jerry_value_is_boolean (strict_equal_result) && jerry_get_boolean_value (strict_equal_result));
  jerry_release_value (strict_equal_result);
  jerry_release_value (args[0]);
  jerry_release_value (args[1]);

  /* Collect garbage. */
  jerry_gc (JERRY_GC_SEVERITY_LOW);

  /* Attempt to retrieve the object by the presence of its property again. */
  args[0] = property_name;
  TEST_ASSERT (!jerry_objects_foreach (find_test_object_by_property, args));

  jerry_release_value (property_name);
  jerry_release_value (undefined);
  jerry_release_value (strict_equal);
  jerry_cleanup ();
} /* main */

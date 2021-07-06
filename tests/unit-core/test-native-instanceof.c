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

static const char instanceof_source[] = "var x = function(o, c) {return (o instanceof c);}; x";

static jerry_value_t
external_function (const jerry_call_info_t *call_info_p,
                   const jerry_value_t args_p[],
                   const jerry_size_t args_count)
{
  (void) call_info_p;
  (void) args_p;
  (void) args_count;

  return jerry_create_undefined ();
} /* external_function */

static void
test_instanceof (jerry_value_t instanceof,
                 jerry_value_t constructor)
{
  jerry_value_t instance = jerry_construct_object (constructor, NULL, 0);
  jerry_value_t args[2] =
  {
    instance, constructor
  };

  jerry_value_t undefined = jerry_create_undefined ();
  jerry_value_t result = jerry_call_function (instanceof, undefined, args, 2);
  jerry_release_value (undefined);

  TEST_ASSERT (!jerry_value_is_error (result));
  TEST_ASSERT (jerry_value_is_boolean (result));

  TEST_ASSERT (jerry_value_is_true (result));

  jerry_release_value (instance);
  jerry_release_value (result);
} /* test_instanceof */

int
main (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t instanceof = jerry_eval ((jerry_char_t *) instanceof_source, sizeof (instanceof_source) - 1, true);

  /* Test for a native-backed function. */
  jerry_value_t constructor = jerry_create_external_function (external_function);

  test_instanceof (instanceof, constructor);
  jerry_release_value (constructor);

  /* Test for a JS constructor. */
  jerry_value_t global = jerry_get_global_object ();
  jerry_value_t object_name = jerry_create_string ((jerry_char_t *) "Object");
  constructor = jerry_get_property (global, object_name);
  jerry_release_value (object_name);
  jerry_release_value (global);

  test_instanceof (instanceof, constructor);
  jerry_release_value (constructor);

  jerry_release_value (instanceof);

  jerry_cleanup ();

  return 0;
} /* main */

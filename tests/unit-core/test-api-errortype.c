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

  jerry_error_t errors[] = { JERRY_ERROR_COMMON, JERRY_ERROR_EVAL, JERRY_ERROR_RANGE, JERRY_ERROR_REFERENCE,
                             JERRY_ERROR_SYNTAX, JERRY_ERROR_TYPE, JERRY_ERROR_URI };

  for (size_t idx = 0; idx < sizeof (errors) / sizeof (errors[0]); idx++)
  {
    jerry_value_t error_obj = jerry_create_error (errors[idx], (const jerry_char_t *) "test");
    TEST_ASSERT (jerry_value_is_error (error_obj));
    TEST_ASSERT (jerry_get_error_type (error_obj) == errors[idx]);

    error_obj = jerry_get_value_from_error (error_obj, true);

    TEST_ASSERT (jerry_get_error_type (error_obj) == errors[idx]);

    jerry_release_value (error_obj);
  }

  jerry_value_t test_values[] = {
    jerry_create_number (11),
    jerry_create_string ((const jerry_char_t *) "message"),
    jerry_create_boolean (true),
    jerry_create_object (),
  };

  for (size_t idx = 0; idx < sizeof (test_values) / sizeof (test_values[0]); idx++)
  {
    jerry_error_t error_type = jerry_get_error_type (test_values[idx]);
    TEST_ASSERT (error_type == JERRY_ERROR_NONE);
    jerry_release_value (test_values[idx]);
  }

  char test_source[] = "\xF0\x9D\x84\x9E";

  jerry_value_t result =
    jerry_parse (NULL, 0, (const jerry_char_t *) test_source, sizeof (test_source) - 1, JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (jerry_value_is_error (result));
  TEST_ASSERT (jerry_get_error_type (result) == JERRY_ERROR_SYNTAX);

  jerry_release_value (result);

  jerry_cleanup ();
} /* main */

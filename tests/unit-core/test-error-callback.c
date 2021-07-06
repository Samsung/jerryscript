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

static bool error_object_created_callback_is_running = false;
static int error_object_created_callback_count = 0;

static void
error_object_created_callback (const jerry_value_t error_object_t, /**< new error object */
                              void *user_p) /**< user pointer */
{
  TEST_ASSERT (!error_object_created_callback_is_running);
  TEST_ASSERT (user_p == (void *) &error_object_created_callback_count);

  error_object_created_callback_is_running = true;
  error_object_created_callback_count++;

  jerry_value_t name = jerry_create_string ((const jerry_char_t *) "message");
  jerry_value_t message = jerry_create_string ((const jerry_char_t *) "Replaced message!");

  jerry_value_t result = jerry_set_property (error_object_t, name, message);
  TEST_ASSERT (jerry_value_is_boolean (result) && jerry_value_is_true (result));
  jerry_release_value (result);

  /* This SyntaxError must not trigger a recusrsive call of the this callback. */
  const char *source_p = "Syntax Error in JS!";
  result = jerry_eval ((const jerry_char_t *) source_p, strlen (source_p), 0);
  TEST_ASSERT (jerry_value_is_error (result));

  jerry_release_value (result);
  jerry_release_value (message);
  jerry_release_value (name);

  error_object_created_callback_is_running = false;
} /* error_object_created_callback */

static void
run_test (const char *source_p)
{
  /* Run the code 5 times. */
  for (int i = 0; i < 5; i++)
  {
    jerry_value_t result = jerry_eval ((const jerry_char_t *) source_p, strlen (source_p), 0);
    TEST_ASSERT (jerry_value_is_boolean (result) && jerry_value_is_true (result));
    jerry_release_value (result);
  }
} /* run_test */

/**
 * Unit test's main function.
 */
int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  jerry_set_error_object_created_callback (error_object_created_callback,
                                          (void *) &error_object_created_callback_count);

  run_test ("var result = false\n"
            "try {\n"
            "  ref_error;\n"
            "} catch(e) {\n"
            "  result = (e.message === 'Replaced message!')\n"
            "}\n"
            "result\n");

  run_test ("var error = new Error()\n"
            "error.message === 'Replaced message!'\n");

  jerry_release_value (jerry_create_error (JERRY_ERROR_COMMON, (const jerry_char_t *) "Message"));

  TEST_ASSERT (error_object_created_callback_count == 11);

  jerry_cleanup ();
  return 0;
} /* main */

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
#include "jerryscript-port.h"
#include "jerryscript-port-default.h"
#include "test-common.h"

/**
 * Empty constructor
 */
static jerry_value_t
construct_handler (const jerry_call_info_t *call_info_p, /**< call information */
                   const jerry_value_t args_p[], /**< function arguments */
                   const jerry_length_t args_cnt) /**< number of function arguments */
{
  JERRY_UNUSED (call_info_p);

  TEST_ASSERT (args_cnt == 1);
  TEST_ASSERT (jerry_get_number_value (args_p[0]) == 1.0);

  return jerry_create_undefined ();
} /* construct_handler */

int
main (void)
{
  /* Test JERRY_FEATURE_SYMBOL feature as it is a must-have in ES.next */
  if (!jerry_is_feature_enabled (JERRY_FEATURE_SYMBOL))
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Skipping test, ES.next support is disabled.\n");
    return 0;
  }

  jerry_init (JERRY_INIT_EMPTY);

  {
    jerry_value_t global_obj_val = jerry_get_global_object ();

    jerry_value_t function_val = jerry_create_external_function (construct_handler);
    jerry_value_t function_name_val = jerry_create_string ((const jerry_char_t *) "Demo");
    jerry_value_t result_val = jerry_set_property (global_obj_val, function_name_val, function_val);
    TEST_ASSERT (!jerry_value_is_error (result_val));
    TEST_ASSERT (jerry_value_is_true (result_val));
    jerry_release_value (result_val);
    jerry_release_value (function_name_val);
    jerry_release_value (global_obj_val);
    jerry_release_value (function_val);
  }

  {
    static const jerry_char_t test_source[] = TEST_STRING_LITERAL (
        "class Sub1 extends Demo { constructor () { super (1); } };"
        "new Sub1 ()"
    );

    jerry_value_t parsed_code_val = jerry_parse (test_source,
                                                 sizeof (test_source) - 1,
                                                 NULL);
    TEST_ASSERT (!jerry_value_is_error (parsed_code_val));

    jerry_value_t result = jerry_run (parsed_code_val);
    TEST_ASSERT (!jerry_value_is_error (result));

    jerry_release_value (result);
    jerry_release_value (parsed_code_val);
  }

  {
    static const jerry_char_t test_source[] = TEST_STRING_LITERAL (
      "class Sub2 extends Demo { };"
      "new Sub2 (1)"
    );

    jerry_value_t parsed_code_val = jerry_parse (test_source,
                                                 sizeof (test_source) - 1,
                                                 NULL);
    TEST_ASSERT (!jerry_value_is_error (parsed_code_val));

    jerry_value_t result = jerry_run (parsed_code_val);
    TEST_ASSERT (!jerry_value_is_error (result));

    jerry_release_value (result);
    jerry_release_value (parsed_code_val);
  }

  jerry_cleanup ();
  return 0;
} /* main */

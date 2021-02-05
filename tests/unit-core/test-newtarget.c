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

/**
 * Register a JavaScript function in the global object.
 */
static jerry_value_t
register_js_function (const char *name_p, /**< name of the function */
                      jerry_external_handler_t handler_p) /**< function callback */
{
  jerry_value_t global_obj_val = jerry_get_global_object ();

  jerry_value_t function_val = jerry_create_external_function (handler_p);
  jerry_value_t function_name_val = jerry_create_string ((const jerry_char_t *) name_p);
  jerry_value_t result_val = jerry_set_property (global_obj_val, function_name_val, function_val);

  jerry_release_value (function_name_val);
  jerry_release_value (global_obj_val);

  jerry_release_value (result_val);

  return function_val;
} /* register_js_function */

enum
{
  TEST_ID_SIMPLE_CONSTRUCT = 1,
  TEST_ID_SIMPLE_CALL = 2,
  TEST_ID_CONSTRUCT_AND_CALL_SUB = 3,
};

static jerry_value_t
construct_handler (const jerry_value_t func_obj_val, /**< function object */
                   const jerry_value_t this_val, /**< this arg */
                   const jerry_value_t args_p[], /**< function arguments */
                   const jerry_length_t args_cnt) /**< number of function arguments */
{
  JERRY_UNUSED (func_obj_val);
  JERRY_UNUSED (this_val);
  JERRY_UNUSED (args_p);

  if (args_cnt != 1 || !jerry_value_is_number (args_p[0]))
  {
    TEST_ASSERT (0 && "Invalid arguments for demo method");
  }

  int test_id = (int) jerry_get_number_value (args_p[0]);

  switch (test_id)
  {
    case TEST_ID_SIMPLE_CONSTRUCT:
    {
      /* Method was called with "new": new.target should be equal to the function object. */
      jerry_value_t target = jerry_get_new_target ();
      TEST_ASSERT (!jerry_value_is_undefined (target));
      TEST_ASSERT (target == func_obj_val);
      jerry_release_value (target);
      break;
    }
    case TEST_ID_SIMPLE_CALL:
    {
      /* Method was called directly without "new": new.target should be equal undefined. */
      jerry_value_t target = jerry_get_new_target ();
      TEST_ASSERT (jerry_value_is_undefined (target));
      TEST_ASSERT (target != func_obj_val);
      jerry_release_value (target);
      break;
    }
    case TEST_ID_CONSTRUCT_AND_CALL_SUB:
    {
      /* Method was called with "new": new.target should be equal to the function object. */
      jerry_value_t target = jerry_get_new_target ();
      TEST_ASSERT (!jerry_value_is_undefined (target));
      TEST_ASSERT (target == func_obj_val);
      jerry_release_value (target);

      /* Calling a function should hide the old "new.target". */
      jerry_value_t sub_arg = jerry_create_number (TEST_ID_SIMPLE_CALL);
      jerry_value_t func_call_result = jerry_call_function (func_obj_val, this_val, &sub_arg, 1);
      TEST_ASSERT (!jerry_value_is_error (func_call_result));
      TEST_ASSERT (jerry_value_is_undefined (func_call_result));
      break;
    }

    default:
    {
      TEST_ASSERT (0 && "Incorrect test ID");
      break;
    }
  }

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

  jerry_value_t demo_func = register_js_function ("Demo", construct_handler);

  {
    jerry_value_t test_arg = jerry_create_number (TEST_ID_SIMPLE_CONSTRUCT);
    jerry_value_t constructed = jerry_construct_object (demo_func, &test_arg, 1);
    TEST_ASSERT (!jerry_value_is_error (constructed));
    TEST_ASSERT (jerry_value_is_object (constructed));
    jerry_release_value (test_arg);
    jerry_release_value (constructed);
  }

  {
    jerry_value_t test_arg = jerry_create_number (TEST_ID_SIMPLE_CALL);
    jerry_value_t this_arg = jerry_create_undefined ();
    jerry_value_t constructed = jerry_call_function (demo_func, this_arg, &test_arg, 1);
    TEST_ASSERT (jerry_value_is_undefined (constructed));
    jerry_release_value (constructed);
    jerry_release_value (constructed);
    jerry_release_value (test_arg);
  }

  {
    jerry_value_t test_arg = jerry_create_number (TEST_ID_CONSTRUCT_AND_CALL_SUB);
    jerry_value_t constructed = jerry_construct_object (demo_func, &test_arg, 1);
    TEST_ASSERT (!jerry_value_is_error (constructed));
    TEST_ASSERT (jerry_value_is_object (constructed));
    jerry_release_value (test_arg);
    jerry_release_value (constructed);
  }

  {
    static const jerry_char_t test_source[] = TEST_STRING_LITERAL ("new Demo (1)");

    jerry_value_t parsed_code_val = jerry_parse (NULL, 0, test_source, sizeof (test_source) - 1, JERRY_PARSE_NO_OPTS);
    TEST_ASSERT (!jerry_value_is_error (parsed_code_val));

    jerry_value_t res = jerry_run (parsed_code_val);
    TEST_ASSERT (!jerry_value_is_error (res));

    jerry_release_value (res);
    jerry_release_value (parsed_code_val);
  }

  {
    static const jerry_char_t test_source[] = TEST_STRING_LITERAL ("Demo (2)");

    jerry_value_t parsed_code_val = jerry_parse (NULL, 0, test_source, sizeof (test_source) - 1, JERRY_PARSE_NO_OPTS);
    TEST_ASSERT (!jerry_value_is_error (parsed_code_val));

    jerry_value_t res = jerry_run (parsed_code_val);
    TEST_ASSERT (!jerry_value_is_error (res));

    jerry_release_value (res);
    jerry_release_value (parsed_code_val);
  }

  {
    static const jerry_char_t test_source[] = TEST_STRING_LITERAL ("function base(arg) { new Demo (arg); };"
                                                                   "base (1);"
                                                                   "new base(1);"
                                                                   "new base(3);");

    jerry_value_t parsed_code_val = jerry_parse (NULL, 0, test_source, sizeof (test_source) - 1, JERRY_PARSE_NO_OPTS);
    TEST_ASSERT (!jerry_value_is_error (parsed_code_val));

    jerry_value_t res = jerry_run (parsed_code_val);
    TEST_ASSERT (!jerry_value_is_error (res));

    jerry_release_value (res);
    jerry_release_value (parsed_code_val);
  }

  jerry_release_value (demo_func);
  jerry_cleanup ();
  return 0;
} /* main */

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

#define T(lhs, rhs, res) \
  { lhs, rhs, res }

typedef struct
{
  jerry_value_t lhs;
  jerry_value_t rhs;
  bool expected;
} test_entry_t;

static jerry_value_t
my_constructor (const jerry_call_info_t *call_info_p, /**< call information */
                const jerry_value_t argv[], /**< arguments */
                const jerry_length_t argc) /**< number of arguments */
{
  (void) call_info_p;
  (void) argv;
  (void) argc;
  return jerry_create_undefined ();
} /* my_constructor */

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t base_obj = jerry_create_object ();
  jerry_value_t constructor = jerry_create_external_function (my_constructor);

  jerry_value_t no_proto_instance_val = jerry_construct_object (constructor, NULL, 0);

  jerry_value_t prototype_str = jerry_create_string ((const jerry_char_t *) "prototype");
  jerry_value_t res = jerry_set_property (constructor, prototype_str, base_obj);
  jerry_release_value (prototype_str);
  TEST_ASSERT (!jerry_value_is_error (res));
  jerry_release_value (res);

  jerry_value_t instance_val = jerry_construct_object (constructor, NULL, 0);

  jerry_value_t error = jerry_create_error_from_value (base_obj, false);

  test_entry_t bool_tests[] =
  {
    T (jerry_acquire_value (instance_val), jerry_acquire_value (constructor), true),
    T (jerry_acquire_value (no_proto_instance_val), jerry_acquire_value (constructor), false),
    T (jerry_acquire_value (base_obj), jerry_acquire_value (constructor), false)
  };

  for (uint32_t idx = 0; idx < sizeof (bool_tests) / sizeof (test_entry_t); idx++)
  {
    jerry_value_t result = jerry_binary_operation (JERRY_BIN_OP_INSTANCEOF,
                                                   bool_tests[idx].lhs,
                                                   bool_tests[idx].rhs);
    TEST_ASSERT (!jerry_value_is_error (result));
    TEST_ASSERT (jerry_value_is_true (result) == bool_tests[idx].expected);
    jerry_release_value (bool_tests[idx].lhs);
    jerry_release_value (bool_tests[idx].rhs);
    jerry_release_value (result);
  }

  test_entry_t error_tests[] =
  {
    T (jerry_acquire_value (constructor), jerry_acquire_value (instance_val), true),
    T (jerry_create_undefined (), jerry_acquire_value (constructor), true),
    T (jerry_acquire_value (instance_val), jerry_create_undefined (), true),
    T (jerry_acquire_value (instance_val), jerry_acquire_value (base_obj), true),
    T (jerry_acquire_value (error), jerry_acquire_value (constructor), true),
    T (jerry_acquire_value (instance_val), jerry_acquire_value (error), true),
    T (jerry_create_string ((const jerry_char_t *) ""), jerry_create_string ((const jerry_char_t *) ""), true),
    T (jerry_create_string ((const jerry_char_t *) ""), jerry_create_number (5.0), true),
    T (jerry_create_number (5.0), jerry_create_string ((const jerry_char_t *) ""), true),
    T (jerry_create_array (1), jerry_create_array (1), true),
    T (jerry_create_array (1), jerry_create_object (), true),
    T (jerry_create_object (), jerry_create_array (1), true),
    T (jerry_create_null (), jerry_create_object (), true),
    T (jerry_create_object (), jerry_create_string ((const jerry_char_t *) ""), true)
  };

  for (uint32_t idx = 0; idx < sizeof (error_tests) / sizeof (test_entry_t); idx++)
  {
    jerry_value_t result = jerry_binary_operation (JERRY_BIN_OP_INSTANCEOF,
                                                   error_tests[idx].lhs,
                                                   error_tests[idx].rhs);
    TEST_ASSERT (jerry_value_is_error (result) == error_tests[idx].expected);
    jerry_release_value (error_tests[idx].lhs);
    jerry_release_value (error_tests[idx].rhs);
    jerry_release_value (result);
  }

  jerry_release_value (base_obj);
  jerry_release_value (constructor);
  jerry_release_value (error);
  jerry_release_value (instance_val);
  jerry_release_value (no_proto_instance_val);

  jerry_cleanup ();

  return 0;
} /* main */

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

#define T(op, lhs, rhs, res) \
  { op, lhs, rhs, res }

typedef struct
{
  jerry_binary_operation_t op;
  jerry_value_t lhs;
  jerry_value_t rhs;
  bool expected;
} test_entry_t;

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t obj1 = jerry_eval ((const jerry_char_t *) "o={x:1};o", 9, JERRY_PARSE_NO_OPTS);
  jerry_value_t obj2 = jerry_eval ((const jerry_char_t *) "o={x:1};o", 9, JERRY_PARSE_NO_OPTS);
  jerry_value_t err1 = jerry_create_error (JERRY_ERROR_SYNTAX, (const jerry_char_t *) "error");

  test_entry_t tests[] =
  {
    /* Testing strict equal comparison */
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_number (5.0), jerry_create_number (5.0), true),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_number (3.1), jerry_create_number (10), false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_number (3.1), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_number (3.1), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_STRICT_EQUAL,
       jerry_create_string ((const jerry_char_t *) "example string"),
       jerry_create_string ((const jerry_char_t *) "example string"),
       true),
    T (JERRY_BIN_OP_STRICT_EQUAL,
       jerry_create_string ((const jerry_char_t *) "example string"),
       jerry_create_undefined (),
       false),
    T (JERRY_BIN_OP_STRICT_EQUAL,
       jerry_create_string ((const jerry_char_t *) "example string"),
       jerry_create_null (),
       false),
    T (JERRY_BIN_OP_STRICT_EQUAL,
       jerry_create_string ((const jerry_char_t *) "example string"),
       jerry_create_number (5.0),
       false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_undefined (), jerry_create_undefined (), true),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_undefined (), jerry_create_null (), false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_null (), jerry_create_null (), true),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_boolean (true), jerry_create_boolean (true), true),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_boolean (true), jerry_create_boolean (false), false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_boolean (false), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_boolean (false), jerry_create_boolean (false), true),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_acquire_value (obj1), jerry_acquire_value (obj1), true),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_acquire_value (obj1), jerry_acquire_value (obj2), false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_acquire_value (obj2), jerry_acquire_value (obj1), false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_acquire_value (obj1), jerry_create_null (), false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_acquire_value (obj1), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_acquire_value (obj1), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_acquire_value (obj1), jerry_create_boolean (false), false),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_acquire_value (obj1), jerry_create_number (5.0), false),
    T (JERRY_BIN_OP_STRICT_EQUAL,
       jerry_acquire_value (obj1),
       jerry_create_string ((const jerry_char_t *) "example string"),
       false),

    /* Testing equal comparison */
    T (JERRY_BIN_OP_EQUAL, jerry_create_number (5.0), jerry_create_number (5.0), true),
    T (JERRY_BIN_OP_EQUAL, jerry_create_number (3.1), jerry_create_number (10), false),
    T (JERRY_BIN_OP_EQUAL, jerry_create_number (3.1), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_EQUAL, jerry_create_number (3.1), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_EQUAL,
       jerry_create_string ((const jerry_char_t *) "example string"),
       jerry_create_string ((const jerry_char_t *) "example string"),
       true),
    T (JERRY_BIN_OP_EQUAL,
       jerry_create_string ((const jerry_char_t *) "example string"),
       jerry_create_undefined (),
       false),
    T (JERRY_BIN_OP_EQUAL,
       jerry_create_string ((const jerry_char_t *) "example string"),
       jerry_create_null (),
       false),
    T (JERRY_BIN_OP_EQUAL,
       jerry_create_string ((const jerry_char_t *) "example string"),
       jerry_create_number (5.0),
       false),
    T (JERRY_BIN_OP_EQUAL, jerry_create_undefined (), jerry_create_undefined (), true),
    T (JERRY_BIN_OP_EQUAL, jerry_create_undefined (), jerry_create_null (), true),
    T (JERRY_BIN_OP_EQUAL, jerry_create_null (), jerry_create_null (), true),
    T (JERRY_BIN_OP_EQUAL, jerry_create_boolean (true), jerry_create_boolean (true), true),
    T (JERRY_BIN_OP_EQUAL, jerry_create_boolean (true), jerry_create_boolean (false), false),
    T (JERRY_BIN_OP_EQUAL, jerry_create_boolean (false), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_EQUAL, jerry_create_boolean (false), jerry_create_boolean (false), true),
    T (JERRY_BIN_OP_EQUAL, jerry_acquire_value (obj1), jerry_acquire_value (obj1), true),
    T (JERRY_BIN_OP_EQUAL, jerry_acquire_value (obj1), jerry_acquire_value (obj2), false),
    T (JERRY_BIN_OP_EQUAL, jerry_acquire_value (obj2), jerry_acquire_value (obj1), false),
    T (JERRY_BIN_OP_EQUAL, jerry_acquire_value (obj1), jerry_create_null (), false),
    T (JERRY_BIN_OP_EQUAL, jerry_acquire_value (obj1), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_EQUAL, jerry_acquire_value (obj1), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_EQUAL, jerry_acquire_value (obj1), jerry_create_boolean (false), false),
    T (JERRY_BIN_OP_EQUAL, jerry_acquire_value (obj1), jerry_create_number (5.0), false),
    T (JERRY_BIN_OP_EQUAL,
       jerry_acquire_value (obj1),
       jerry_create_string ((const jerry_char_t *) "example string"),
       false),

    /* Testing less comparison */
    T (JERRY_BIN_OP_LESS, jerry_create_number (5.0), jerry_create_number (5.0), false),
    T (JERRY_BIN_OP_LESS, jerry_create_number (3.1), jerry_create_number (10), true),
    T (JERRY_BIN_OP_LESS, jerry_create_number (3.1), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_LESS, jerry_create_number (3.1), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_LESS,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_string ((const jerry_char_t *) "2"),
       true),
    T (JERRY_BIN_OP_LESS,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_undefined (),
       false),
    T (JERRY_BIN_OP_LESS,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_null (),
       false),
    T (JERRY_BIN_OP_LESS,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_number (5.0),
       true),
    T (JERRY_BIN_OP_LESS, jerry_create_undefined (), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_LESS, jerry_create_undefined (), jerry_create_null (), false),
    T (JERRY_BIN_OP_LESS, jerry_create_null (), jerry_create_null (), false),
    T (JERRY_BIN_OP_LESS, jerry_create_boolean (true), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_LESS, jerry_create_boolean (true), jerry_create_boolean (false), false),
    T (JERRY_BIN_OP_LESS, jerry_create_boolean (false), jerry_create_boolean (true), true),
    T (JERRY_BIN_OP_LESS, jerry_create_boolean (false), jerry_create_boolean (false), false),

    /* Testing less or equal comparison */
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_number (5.0), jerry_create_number (5.0), true),
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_number (5.1), jerry_create_number (5.0), false),
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_number (3.1), jerry_create_number (10), true),
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_number (3.1), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_number (3.1), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_LESS_EQUAL,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_string ((const jerry_char_t *) "2"),
       true),
    T (JERRY_BIN_OP_LESS_EQUAL,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_string ((const jerry_char_t *) "1"),
       true),
    T (JERRY_BIN_OP_LESS_EQUAL,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_undefined (),
       false),
    T (JERRY_BIN_OP_LESS_EQUAL,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_null (),
       false),
    T (JERRY_BIN_OP_LESS_EQUAL,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_number (5.0),
       true),
    T (JERRY_BIN_OP_LESS_EQUAL,
       jerry_create_string ((const jerry_char_t *) "5.0"),
       jerry_create_number (5.0),
       true),
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_undefined (), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_undefined (), jerry_create_null (), false),
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_null (), jerry_create_null (), true),
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_boolean (true), jerry_create_boolean (true), true),
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_boolean (true), jerry_create_boolean (false), false),
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_boolean (false), jerry_create_boolean (true), true),
    T (JERRY_BIN_OP_LESS_EQUAL, jerry_create_boolean (false), jerry_create_boolean (false), true),

    /* Testing greater comparison */
    T (JERRY_BIN_OP_GREATER, jerry_create_number (5.0), jerry_create_number (5.0), false),
    T (JERRY_BIN_OP_GREATER, jerry_create_number (10), jerry_create_number (3.1), true),
    T (JERRY_BIN_OP_GREATER, jerry_create_number (3.1), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_GREATER, jerry_create_number (3.1), jerry_create_boolean (true), true),
    T (JERRY_BIN_OP_GREATER,
       jerry_create_string ((const jerry_char_t *) "2"),
       jerry_create_string ((const jerry_char_t *) "1"),
       true),
    T (JERRY_BIN_OP_GREATER,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_string ((const jerry_char_t *) "2"),
       false),
    T (JERRY_BIN_OP_GREATER,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_undefined (),
       false),
    T (JERRY_BIN_OP_GREATER,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_null (),
       true),
    T (JERRY_BIN_OP_GREATER,
       jerry_create_number (5.0),
       jerry_create_string ((const jerry_char_t *) "1"),
       true),
    T (JERRY_BIN_OP_GREATER, jerry_create_undefined (), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_GREATER, jerry_create_undefined (), jerry_create_null (), false),
    T (JERRY_BIN_OP_GREATER, jerry_create_null (), jerry_create_null (), false),
    T (JERRY_BIN_OP_GREATER, jerry_create_boolean (true), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_GREATER, jerry_create_boolean (true), jerry_create_boolean (false), true),
    T (JERRY_BIN_OP_GREATER, jerry_create_boolean (false), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_GREATER, jerry_create_boolean (false), jerry_create_boolean (false), false),

    /* Testing greater or equal comparison */
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_number (5.0), jerry_create_number (5.0), true),
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_number (5.0), jerry_create_number (5.1), false),
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_number (10), jerry_create_number (3.1), true),
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_number (3.1), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_number (3.1), jerry_create_boolean (true), true),
    T (JERRY_BIN_OP_GREATER_EQUAL,
       jerry_create_string ((const jerry_char_t *) "2"),
       jerry_create_string ((const jerry_char_t *) "1"),
       true),
    T (JERRY_BIN_OP_GREATER_EQUAL,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_string ((const jerry_char_t *) "1"),
       true),
    T (JERRY_BIN_OP_GREATER_EQUAL,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_undefined (),
       false),
    T (JERRY_BIN_OP_GREATER_EQUAL,
       jerry_create_string ((const jerry_char_t *) "1"),
       jerry_create_null (),
       true),
    T (JERRY_BIN_OP_GREATER_EQUAL,
       jerry_create_number (5.0),
       jerry_create_string ((const jerry_char_t *) "1"),
       true),
    T (JERRY_BIN_OP_GREATER_EQUAL,
       jerry_create_string ((const jerry_char_t *) "5.0"),
       jerry_create_number (5.0),
       true),
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_undefined (), jerry_create_undefined (), false),
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_undefined (), jerry_create_null (), false),
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_null (), jerry_create_null (), true),
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_boolean (true), jerry_create_boolean (true), true),
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_boolean (true), jerry_create_boolean (false), true),
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_boolean (false), jerry_create_boolean (true), false),
    T (JERRY_BIN_OP_GREATER_EQUAL, jerry_create_boolean (false), jerry_create_boolean (false), true),
  };

  for (uint32_t idx = 0; idx < sizeof (tests) / sizeof (test_entry_t); idx++)
  {
    jerry_value_t result = jerry_binary_operation (tests[idx].op, tests[idx].lhs, tests[idx].rhs);
    TEST_ASSERT (!jerry_value_is_error (result));
    TEST_ASSERT (jerry_value_is_true (result) == tests[idx].expected);
    jerry_release_value (tests[idx].lhs);
    jerry_release_value (tests[idx].rhs);
    jerry_release_value (result);
  }

  test_entry_t error_tests[] =
  {
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_acquire_value (err1), jerry_acquire_value (err1), true),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_acquire_value (err1), jerry_create_undefined (), true),
    T (JERRY_BIN_OP_STRICT_EQUAL, jerry_create_undefined (), jerry_acquire_value (err1), true),
  };

  for (uint32_t idx = 0; idx < sizeof (error_tests) / sizeof (test_entry_t); idx++)
  {
    jerry_value_t result = jerry_binary_operation (tests[idx].op, error_tests[idx].lhs, error_tests[idx].rhs);
    TEST_ASSERT (jerry_value_is_error (result) == error_tests[idx].expected);
    jerry_release_value (error_tests[idx].lhs);
    jerry_release_value (error_tests[idx].rhs);
    jerry_release_value (result);
  }

  jerry_release_value (obj1);
  jerry_release_value (obj2);
  jerry_release_value (err1);

  jerry_cleanup ();

  return 0;
} /* main */

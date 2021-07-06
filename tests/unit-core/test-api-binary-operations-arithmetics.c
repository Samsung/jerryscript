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

#define T_NAN(op, lhs, rhs) \
  { op, lhs, rhs }

#define T_ERR(op, lsh, rhs) \
  T_NAN (op, lsh, rhs)

#define T_ARI(lhs, rhs) \
  T_NAN (JERRY_BIN_OP_SUB, lhs, rhs), \
  T_NAN (JERRY_BIN_OP_MUL, lhs, rhs), \
  T_NAN (JERRY_BIN_OP_DIV, lhs, rhs), \
  T_NAN (JERRY_BIN_OP_REM, lhs, rhs)

typedef struct
{
  jerry_binary_operation_t op;
  jerry_value_t lhs;
  jerry_value_t rhs;
  jerry_value_t expected;
} test_entry_t;

typedef struct
{
  jerry_binary_operation_t op;
  jerry_value_t lhs;
  jerry_value_t rhs;
} test_nan_entry_t;

typedef test_nan_entry_t test_error_entry_t;

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t obj1 = jerry_eval ((const jerry_char_t *) "o={x:1};o", 9, JERRY_PARSE_NO_OPTS);
  jerry_value_t obj2 = jerry_eval ((const jerry_char_t *) "o={x:1};o", 9, JERRY_PARSE_NO_OPTS);
  jerry_value_t err1 = jerry_create_error (JERRY_ERROR_SYNTAX, (const jerry_char_t *) "error");

  test_nan_entry_t test_nans[] =
  {
    /* Testing addition (+) */
    T_NAN (JERRY_BIN_OP_ADD, jerry_create_number (3.1), jerry_create_undefined ()),
    T_NAN (JERRY_BIN_OP_ADD, jerry_create_undefined (), jerry_create_undefined ()),
    T_NAN (JERRY_BIN_OP_ADD, jerry_create_undefined (), jerry_create_null ()),

    /* Testing subtraction (-), multiplication (*), division (/), remainder (%) */
    T_ARI (jerry_create_number (3.1), jerry_create_undefined ()),
    T_ARI (jerry_create_string ((const jerry_char_t *) "foo"), jerry_create_string ((const jerry_char_t *) "bar")),
    T_ARI (jerry_create_string ((const jerry_char_t *) "foo"), jerry_create_undefined ()),
    T_ARI (jerry_create_string ((const jerry_char_t *) "foo"), jerry_create_null ()),
    T_ARI (jerry_create_string ((const jerry_char_t *) "foo"), jerry_create_number (5.0)),
    T_ARI (jerry_create_undefined (), jerry_create_string ((const jerry_char_t *) "foo")),
    T_ARI (jerry_create_null (), jerry_create_string ((const jerry_char_t *) "foo")),
    T_ARI (jerry_create_number (5.0), jerry_create_string ((const jerry_char_t *) "foo")),
    T_ARI (jerry_create_undefined (), jerry_create_undefined ()),
    T_ARI (jerry_create_undefined (), jerry_create_null ()),
    T_ARI (jerry_create_null (), jerry_create_undefined ()),
    T_ARI (jerry_acquire_value (obj1), jerry_acquire_value (obj1)),
    T_ARI (jerry_acquire_value (obj1), jerry_acquire_value (obj2)),
    T_ARI (jerry_acquire_value (obj2), jerry_acquire_value (obj1)),
    T_ARI (jerry_acquire_value (obj2), jerry_create_undefined ()),
    T_ARI (jerry_acquire_value (obj1), jerry_create_string ((const jerry_char_t *) "foo")),
    T_ARI (jerry_acquire_value (obj1), jerry_create_null ()),
    T_ARI (jerry_acquire_value (obj1), jerry_create_boolean (true)),
    T_ARI (jerry_acquire_value (obj1), jerry_create_boolean (false)),
    T_ARI (jerry_acquire_value (obj1), jerry_create_number (5.0)),

    /* Testing division (/) */
    T_NAN (JERRY_BIN_OP_DIV, jerry_create_boolean (false), jerry_create_boolean (false)),
    T_NAN (JERRY_BIN_OP_DIV, jerry_create_number (0.0), jerry_create_number (0.0)),
    T_NAN (JERRY_BIN_OP_DIV, jerry_create_null (), jerry_create_null ()),

    /* Testing remainder (%) */
    T_NAN (JERRY_BIN_OP_REM, jerry_create_boolean (true), jerry_create_boolean (false)),
    T_NAN (JERRY_BIN_OP_REM, jerry_create_boolean (false), jerry_create_boolean (false)),
    T_NAN (JERRY_BIN_OP_REM, jerry_create_number (0.0), jerry_create_number (0.0)),
    T_NAN (JERRY_BIN_OP_REM, jerry_create_null (), jerry_create_null ()),
  };

  for (uint32_t idx = 0; idx < sizeof (test_nans) / sizeof (test_nan_entry_t); idx++)
  {
    jerry_value_t result = jerry_binary_operation (test_nans[idx].op, test_nans[idx].lhs, test_nans[idx].rhs);
    TEST_ASSERT (jerry_value_is_number (result));

    double num = jerry_get_number_value (result);

    TEST_ASSERT (num != num);

    jerry_release_value (test_nans[idx].lhs);
    jerry_release_value (test_nans[idx].rhs);
    jerry_release_value (result);
  }

  test_entry_t tests[] =
  {
    /* Testing addition (+) */
    T (JERRY_BIN_OP_ADD, jerry_create_number (5.0), jerry_create_number (5.0), jerry_create_number (10.0)),
    T (JERRY_BIN_OP_ADD, jerry_create_number (3.1), jerry_create_number (10), jerry_create_number (13.1)),
    T (JERRY_BIN_OP_ADD, jerry_create_number (3.1), jerry_create_boolean (true), jerry_create_number (4.1)),
    T (JERRY_BIN_OP_ADD,
       jerry_create_string ((const jerry_char_t *) "foo"),
       jerry_create_string ((const jerry_char_t *) "bar"),
       jerry_create_string ((const jerry_char_t *) "foobar")),
    T (JERRY_BIN_OP_ADD,
       jerry_create_string ((const jerry_char_t *) "foo"),
       jerry_create_undefined (),
       jerry_create_string ((const jerry_char_t *) "fooundefined")),
    T (JERRY_BIN_OP_ADD,
       jerry_create_string ((const jerry_char_t *) "foo"),
       jerry_create_null (),
       jerry_create_string ((const jerry_char_t *) "foonull")),
    T (JERRY_BIN_OP_ADD,
       jerry_create_string ((const jerry_char_t *) "foo"),
       jerry_create_number (5.0),
       jerry_create_string ((const jerry_char_t *) "foo5")),

    T (JERRY_BIN_OP_ADD, jerry_create_null (), jerry_create_null (), jerry_create_number (0.0)),
    T (JERRY_BIN_OP_ADD, jerry_create_boolean (true), jerry_create_boolean (true), jerry_create_number (2.0)),
    T (JERRY_BIN_OP_ADD, jerry_create_boolean (true), jerry_create_boolean (false), jerry_create_number (1.0)),
    T (JERRY_BIN_OP_ADD, jerry_create_boolean (false), jerry_create_boolean (true), jerry_create_number (1.0)),
    T (JERRY_BIN_OP_ADD, jerry_create_boolean (false), jerry_create_boolean (false), jerry_create_number (0.0)),
    T (JERRY_BIN_OP_ADD,
       jerry_acquire_value (obj1),
       jerry_acquire_value (obj1),
       jerry_create_string ((const jerry_char_t *) "[object Object][object Object]")),
    T (JERRY_BIN_OP_ADD,
       jerry_acquire_value (obj1),
       jerry_acquire_value (obj2),
       jerry_create_string ((const jerry_char_t *) "[object Object][object Object]")),
    T (JERRY_BIN_OP_ADD,
       jerry_acquire_value (obj2),
       jerry_acquire_value (obj1),
       jerry_create_string ((const jerry_char_t *) "[object Object][object Object]")),
    T (JERRY_BIN_OP_ADD,
       jerry_acquire_value (obj1),
       jerry_create_null (),
       jerry_create_string ((const jerry_char_t *) "[object Object]null")),
    T (JERRY_BIN_OP_ADD,
       jerry_acquire_value (obj1),
       jerry_create_undefined (),
       jerry_create_string ((const jerry_char_t *) "[object Object]undefined")),
    T (JERRY_BIN_OP_ADD,
       jerry_acquire_value (obj1),
       jerry_create_boolean (true),
       jerry_create_string ((const jerry_char_t *) "[object Object]true")),
    T (JERRY_BIN_OP_ADD,
       jerry_acquire_value (obj1),
       jerry_create_boolean (false),
       jerry_create_string ((const jerry_char_t *) "[object Object]false")),
    T (JERRY_BIN_OP_ADD,
       jerry_acquire_value (obj1),
       jerry_create_number (5.0),
       jerry_create_string ((const jerry_char_t *) "[object Object]5")),
    T (JERRY_BIN_OP_ADD,
       jerry_acquire_value (obj1),
       jerry_create_string ((const jerry_char_t *) "foo"),
       jerry_create_string ((const jerry_char_t *) "[object Object]foo")),

    /* Testing subtraction (-) */
    T (JERRY_BIN_OP_SUB, jerry_create_number (5.0), jerry_create_number (5.0), jerry_create_number (0.0)),
    T (JERRY_BIN_OP_SUB, jerry_create_number (3.1), jerry_create_number (10), jerry_create_number (-6.9)),
    T (JERRY_BIN_OP_SUB, jerry_create_number (3.1), jerry_create_boolean (true), jerry_create_number (2.1)),
    T (JERRY_BIN_OP_SUB, jerry_create_boolean (true), jerry_create_boolean (true), jerry_create_number (0.0)),
    T (JERRY_BIN_OP_SUB, jerry_create_boolean (true), jerry_create_boolean (false), jerry_create_number (1.0)),
    T (JERRY_BIN_OP_SUB, jerry_create_boolean (false), jerry_create_boolean (true), jerry_create_number (-1.0)),
    T (JERRY_BIN_OP_SUB, jerry_create_boolean (false), jerry_create_boolean (false), jerry_create_number (0.0)),
    T (JERRY_BIN_OP_SUB, jerry_create_null (), jerry_create_null (), jerry_create_number (-0.0)),

    /* Testing multiplication (*) */
    T (JERRY_BIN_OP_MUL, jerry_create_number (5.0), jerry_create_number (5.0), jerry_create_number (25.0)),
    T (JERRY_BIN_OP_MUL, jerry_create_number (3.1), jerry_create_number (10), jerry_create_number (31)),
    T (JERRY_BIN_OP_MUL, jerry_create_number (3.1), jerry_create_boolean (true), jerry_create_number (3.1)),
    T (JERRY_BIN_OP_MUL, jerry_create_boolean (true), jerry_create_boolean (true), jerry_create_number (1.0)),
    T (JERRY_BIN_OP_MUL, jerry_create_boolean (true), jerry_create_boolean (false), jerry_create_number (0.0)),
    T (JERRY_BIN_OP_MUL, jerry_create_boolean (false), jerry_create_boolean (true), jerry_create_number (0.0)),
    T (JERRY_BIN_OP_MUL, jerry_create_boolean (false), jerry_create_boolean (false), jerry_create_number (0.0)),
    T (JERRY_BIN_OP_MUL, jerry_create_null (), jerry_create_null (), jerry_create_number (0.0)),

    /* Testing division (/) */
    T (JERRY_BIN_OP_DIV, jerry_create_number (5.0), jerry_create_number (5.0), jerry_create_number (1.0)),
    T (JERRY_BIN_OP_DIV, jerry_create_number (3.1), jerry_create_number (10), jerry_create_number (0.31)),
    T (JERRY_BIN_OP_DIV, jerry_create_number (3.1), jerry_create_boolean (true), jerry_create_number (3.1)),
    T (JERRY_BIN_OP_DIV, jerry_create_boolean (true), jerry_create_boolean (true), jerry_create_number (1.0)),
    T (JERRY_BIN_OP_DIV,
       jerry_create_boolean (true),
       jerry_create_boolean (false),
       jerry_create_number_infinity (false)),
    T (JERRY_BIN_OP_DIV, jerry_create_boolean (false), jerry_create_boolean (true), jerry_create_number (0.0)),

    /* Testing remainder (%) */
    T (JERRY_BIN_OP_REM, jerry_create_number (5.0), jerry_create_number (5.0), jerry_create_number (0.0)),
    T (JERRY_BIN_OP_REM, jerry_create_number (5.0), jerry_create_number (2.0), jerry_create_number (1.0)),
    T (JERRY_BIN_OP_REM, jerry_create_number (3.1), jerry_create_number (10), jerry_create_number (3.1)),
    T (JERRY_BIN_OP_REM,
       jerry_create_number (3.1),
       jerry_create_boolean (true),
       jerry_create_number (0.10000000000000009)),
    T (JERRY_BIN_OP_REM, jerry_create_boolean (true), jerry_create_boolean (true), jerry_create_number (0.0)),
    T (JERRY_BIN_OP_REM, jerry_create_boolean (false), jerry_create_boolean (true), jerry_create_number (0.0)),

  };

  for (uint32_t idx = 0; idx < sizeof (tests) / sizeof (test_entry_t); idx++)
  {
    jerry_value_t result = jerry_binary_operation (tests[idx].op, tests[idx].lhs, tests[idx].rhs);
    TEST_ASSERT (!jerry_value_is_error (result));

    jerry_value_t equals = jerry_binary_operation (JERRY_BIN_OP_STRICT_EQUAL, result, tests[idx].expected);
    TEST_ASSERT (jerry_value_is_boolean (equals) && jerry_value_is_true (equals));
    jerry_release_value (equals);

    jerry_release_value (tests[idx].lhs);
    jerry_release_value (tests[idx].rhs);
    jerry_release_value (tests[idx].expected);
    jerry_release_value (result);
  }

  jerry_value_t obj3 = jerry_eval ((const jerry_char_t *) "o={valueOf:function(){throw 5}};o", 33, JERRY_PARSE_NO_OPTS);

  test_error_entry_t error_tests[] =
  {
    /* Testing addition (+) */
    T_ERR (JERRY_BIN_OP_ADD, jerry_acquire_value (err1), jerry_acquire_value (err1)),
    T_ERR (JERRY_BIN_OP_ADD, jerry_acquire_value (err1), jerry_create_undefined ()),
    T_ERR (JERRY_BIN_OP_ADD, jerry_create_undefined (), jerry_acquire_value (err1)),

    /* Testing subtraction (-), multiplication (*), division (/), remainder (%) */
    T_ARI (jerry_acquire_value (err1), jerry_acquire_value (err1)),
    T_ARI (jerry_acquire_value (err1), jerry_create_undefined ()),
    T_ARI (jerry_create_undefined (), jerry_acquire_value (err1)),

    /* Testing addition (+) */
    T_ERR (JERRY_BIN_OP_ADD, jerry_acquire_value (obj3), jerry_create_undefined ()),
    T_ERR (JERRY_BIN_OP_ADD, jerry_acquire_value (obj3), jerry_create_null ()),
    T_ERR (JERRY_BIN_OP_ADD, jerry_acquire_value (obj3), jerry_create_boolean (true)),
    T_ERR (JERRY_BIN_OP_ADD, jerry_acquire_value (obj3), jerry_create_boolean (false)),
    T_ERR (JERRY_BIN_OP_ADD, jerry_acquire_value (obj3), jerry_acquire_value (obj2)),
    T_ERR (JERRY_BIN_OP_ADD, jerry_acquire_value (obj3), jerry_create_string ((const jerry_char_t *) "foo")),

    /* Testing subtraction (-), multiplication (*), division (/), remainder (%) */
    T_ARI (jerry_acquire_value (obj3), jerry_create_undefined ()),
    T_ARI (jerry_acquire_value (obj3), jerry_create_null ()),
    T_ARI (jerry_acquire_value (obj3), jerry_create_boolean (true)),
    T_ARI (jerry_acquire_value (obj3), jerry_create_boolean (false)),
    T_ARI (jerry_acquire_value (obj3), jerry_acquire_value (obj2)),
    T_ARI (jerry_acquire_value (obj3), jerry_create_string ((const jerry_char_t *) "foo")),
  };

  for (uint32_t idx = 0; idx < sizeof (error_tests) / sizeof (test_error_entry_t); idx++)
  {
    jerry_value_t result = jerry_binary_operation (tests[idx].op, error_tests[idx].lhs, error_tests[idx].rhs);
    TEST_ASSERT (jerry_value_is_error (result));
    jerry_release_value (error_tests[idx].lhs);
    jerry_release_value (error_tests[idx].rhs);
    jerry_release_value (result);
  }

  jerry_release_value (obj1);
  jerry_release_value (obj2);
  jerry_release_value (obj3);
  jerry_release_value (err1);

  jerry_cleanup ();

  return 0;
} /* main */

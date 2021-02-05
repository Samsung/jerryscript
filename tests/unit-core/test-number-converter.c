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

#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "test-common.h"

// basic toUint32 tester method
static void
test_to_uint32 (double input, uint32_t test_number)
{
  jerry_value_t number_val = jerry_create_number (input);
  uint32_t uint_number = jerry_value_as_uint32 (number_val);
  TEST_ASSERT (uint_number == test_number);
  jerry_release_value (number_val);
} /* test_to_uint32 */

// basic toInt32 tester method
static void
test_to_int32 (double input, int32_t test_number)
{
  jerry_value_t number_val = jerry_create_number (input);
  int32_t int_number = jerry_value_as_int32 (number_val);
  TEST_ASSERT (int_number == test_number);
  jerry_release_value (number_val);
} /* test_to_int32 */

// basic toInteger tester method
static void
test_to_interger (double input, double test_number)
{
  jerry_value_t number_val = jerry_create_number (input);
  double double_number = jerry_value_as_integer (number_val);
  TEST_ASSERT (double_number == test_number);
  jerry_release_value (number_val);
} /* test_to_interger */

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  // few toUint32 test-cases
  test_to_uint32 (1.0, 1);
  test_to_uint32 (0.0, 0);
  test_to_uint32 (NAN, 0);
  test_to_uint32 (-NAN, 0);
  test_to_uint32 (INFINITY, 0);
  test_to_uint32 (-INFINITY, 0);
  test_to_uint32 (0.1, 0);
  test_to_uint32 (-0.1, 0);
  test_to_uint32 (1.1, 1);
  test_to_uint32 (-1.1, 4294967295);
  test_to_uint32 (4294967295, 4294967295);
  test_to_uint32 (-4294967295, 1);
  test_to_uint32 (4294967296, 0);
  test_to_uint32 (-4294967296, 0);
  test_to_uint32 (4294967297, 1);
  test_to_uint32 (-4294967297, 4294967295);

  // few toint32 test-cases
  test_to_int32 (1.0, 1);
  test_to_int32 (0.0, 0);
  test_to_int32 (NAN, 0);
  test_to_int32 (-NAN, 0);
  test_to_int32 (INFINITY, 0);
  test_to_int32 (-INFINITY, 0);
  test_to_int32 (0.1, 0);
  test_to_int32 (-0.1, 0);
  test_to_int32 (1.1, 1);
  test_to_int32 (-1.1, -1);
  test_to_int32 (4294967295, -1);
  test_to_int32 (-4294967295, 1);
  test_to_int32 (4294967296, 0);
  test_to_int32 (-4294967296, 0);
  test_to_int32 (4294967297, 1);
  test_to_int32 (-4294967297, -1);
  test_to_int32 (2147483648, -2147483648);
  test_to_int32 (-2147483648, -2147483648);
  test_to_int32 (2147483647, 2147483647);
  test_to_int32 (-2147483647, -2147483647);
  test_to_int32 (-2147483649, 2147483647);
  test_to_int32 (2147483649, -2147483647);

  // few toInteger test-cases
  test_to_interger (1.0, 1.0);
  test_to_interger (0.0, 0.0);
  test_to_interger (NAN, 0);
  test_to_interger (-NAN, 0);
  test_to_interger (INFINITY, INFINITY);
  test_to_interger (-INFINITY, -INFINITY);
  test_to_interger (0.1, 0);
  test_to_interger (-0.1, -0);
  test_to_interger (1.1, 1);
  test_to_interger (-1.1, -1);
  test_to_interger (4294967295, 4294967295);
  test_to_interger (-4294967295, -4294967295);
  test_to_interger (4294967295, 4294967295);
  test_to_interger (-4294967296, -4294967296);
  test_to_interger (4294967297, 4294967297);
  test_to_interger (-4294967297, -4294967297);

  // few test-cases which return with error
  jerry_value_t error_val = jerry_create_error (JERRY_ERROR_TYPE, (const jerry_char_t *) "error");
  double number = jerry_value_as_integer (error_val);
  jerry_release_value (error_val);
  TEST_ASSERT (number == 0);

  error_val = jerry_create_symbol (error_val);
  number = jerry_value_as_integer (error_val);
  TEST_ASSERT (number == 0);
  jerry_release_value (error_val);

  error_val =
    jerry_eval ((const jerry_char_t *) "({ valueOf() { throw new TypeError('foo')}})", 44, JERRY_PARSE_NO_OPTS);
  number = jerry_value_as_integer (error_val);
  TEST_ASSERT (number == 0);
  jerry_release_value (error_val);

  jerry_cleanup ();
  return 0;
} /* main */

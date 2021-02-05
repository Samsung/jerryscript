/*
 * Copyright JS Foundation and other contributors, http://js.foundation
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

#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-init-finalize.h"
#include "jcontext.h"
#include "jerryscript.h"
#include "test-common.h"

/**
 * Unit test's main function.
 */
int
main (void)
{
  TEST_INIT ();

  jmem_init ();
  ecma_init ();

  ecma_number_t num;

  ecma_value_t int_num = ecma_make_int32_value (123);

  ecma_number_t result = ecma_op_to_integer (int_num, &num);

  ecma_free_value (int_num);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (num == 123);

  /* 2 */
  ecma_value_t error = ecma_raise_type_error (ECMA_ERR_MSG ("I am a neat little error message"));

  result = ecma_op_to_integer (error, &num);

  jcontext_release_exception ();

  TEST_ASSERT (ECMA_IS_VALUE_ERROR (result));

  /* 3 */
  ecma_value_t nan = ecma_make_nan_value ();

  result = ecma_op_to_integer (nan, &num);

  ecma_free_value (nan);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (num == 0);

  /* 4 */
  /* -0 */
  ecma_value_t negative_zero = ecma_make_number_value (-0.0f);

  result = ecma_op_to_integer (negative_zero, &num);

  ecma_free_value (negative_zero);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (1.0f / num == ecma_number_make_infinity (true));

  /* +0 */
  ecma_value_t positive_zero = ecma_make_number_value (+0.0f);

  result = ecma_op_to_integer (positive_zero, &num);

  ecma_free_value (positive_zero);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (1.0f / num == ecma_number_make_infinity (false));

  /* -infinity */
  ecma_value_t negative_infinity = ecma_make_number_value (ecma_number_make_infinity (true));

  result = ecma_op_to_integer (negative_infinity, &num);

  ecma_free_value (negative_infinity);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (num == ecma_number_make_infinity (true));

  /* +infinity */
  ecma_value_t positive_infinity = ecma_make_number_value (ecma_number_make_infinity (false));

  result = ecma_op_to_integer (positive_infinity, &num);

  ecma_free_value (positive_infinity);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (num == ecma_number_make_infinity (false));

  /* 5 */
  ecma_value_t floor = ecma_make_number_value (3.001f);

  result = ecma_op_to_integer (floor, &num);

  ecma_free_value (floor);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (num == 3);

  ecma_value_t floor2 = ecma_make_number_value (-26.5973);

  result = ecma_op_to_integer (floor2, &num);

  ecma_free_value (floor2);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (num == -26);

  ecma_finalize ();
  jmem_finalize ();

  return 0;
} /* main */

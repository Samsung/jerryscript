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

  ecma_length_t num;

  ecma_value_t int_num = ecma_make_int32_value (123);

  ecma_value_t result = ecma_op_to_length (int_num, &num);

  ecma_free_value (int_num);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (num == 123);

  /* 1, 3 */
  ecma_value_t error_throw = ecma_raise_type_error (ECMA_ERR_MSG ("I'm an error"));

  result = ecma_op_to_length (error_throw, &num);

  jcontext_release_exception ();

  TEST_ASSERT (ECMA_IS_VALUE_ERROR (result));

  /* zero */
  ecma_value_t zero = ecma_make_int32_value (0);

  result = ecma_op_to_length (zero, &num);

  ecma_free_value (zero);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (num == 0);

  /* negative */
  ecma_value_t negative = ecma_make_number_value (-26.5973f);

  result = ecma_op_to_length (negative, &num);

  ecma_free_value (negative);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
#if JERRY_ESNEXT
  TEST_ASSERT (num == 0);
#else /* !JERRY_ESNEXT */
  TEST_ASSERT (num == 4294967270);
#endif /* JERRY_ESNEXT */

  /* +infinity */
  ecma_value_t positive_infinity = ecma_make_number_value (ecma_number_make_infinity (false));

  result = ecma_op_to_length (positive_infinity, &num);

  ecma_free_value (positive_infinity);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
#if JERRY_ESNEXT
  TEST_ASSERT ((ecma_number_t) num == ECMA_NUMBER_MAX_SAFE_INTEGER);
#else /* !JERRY_ESNEXT */
  TEST_ASSERT (num == 0);
#endif /* JERRY_ESNEXT */

  /* -infinity */
  ecma_value_t negative_infinity = ecma_make_number_value (ecma_number_make_infinity (true));

  result = ecma_op_to_length (negative_infinity, &num);

  ecma_free_value (negative_infinity);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (num == 0);

  /* NaN */
  ecma_value_t nan = ecma_make_nan_value ();

  result = ecma_op_to_length (nan, &num);

  ecma_free_value (nan);

  TEST_ASSERT (!ECMA_IS_VALUE_ERROR (result));
  TEST_ASSERT (num == 0);

  ecma_finalize ();
  jmem_finalize ();

  return 0;
} /* main */

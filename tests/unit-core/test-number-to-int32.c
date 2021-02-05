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

typedef struct
{
  ecma_number_t num;
  uint32_t uint32_num;
} uint32_test_case_t;

typedef struct
{
  ecma_number_t num;
  int32_t int32_num;
} int32_test_case_t;

/**
 * Unit test's main function.
 */
int
main (void)
{
  TEST_INIT ();

  const uint32_test_case_t test_cases_uint32[] = {
#define TEST_CASE(num, uint32) { num, uint32 }
    TEST_CASE (1.0, 1),
    TEST_CASE (0.0, 0),
    TEST_CASE (NAN, 0),
    TEST_CASE (-NAN, 0),
    TEST_CASE (INFINITY, 0),
    TEST_CASE (-INFINITY, 0),
    TEST_CASE (0.1, 0),
    TEST_CASE (-0.1, 0),
    TEST_CASE (1.1, 1),
    TEST_CASE (-1.1, 4294967295),
    TEST_CASE (4294967295, 4294967295),
    TEST_CASE (-4294967295, 1),
    TEST_CASE (4294967296, 0),
    TEST_CASE (-4294967296, 0),
    TEST_CASE (4294967297, 1),
    TEST_CASE (-4294967297, 4294967295)
#undef TEST_CASE
  };

  for (uint32_t i = 0; i < sizeof (test_cases_uint32) / sizeof (test_cases_uint32[0]); i++)
  {
    TEST_ASSERT (ecma_number_to_uint32 (test_cases_uint32[i].num) == test_cases_uint32[i].uint32_num);
  }

  int32_test_case_t test_cases_int32[] = {
#define TEST_CASE(num, int32) { num, int32 }
    TEST_CASE (1.0, 1),
    TEST_CASE (0.0, 0),
    TEST_CASE (NAN, 0),
    TEST_CASE (-NAN, 0),
    TEST_CASE (INFINITY, 0),
    TEST_CASE (-INFINITY, 0),
    TEST_CASE (0.1, 0),
    TEST_CASE (-0.1, 0),
    TEST_CASE (1.1, 1),
    TEST_CASE (-1.1, -1),
    TEST_CASE (4294967295, -1),
    TEST_CASE (-4294967295, 1),
    TEST_CASE (4294967296, 0),
    TEST_CASE (-4294967296, 0),
    TEST_CASE (4294967297, 1),
    TEST_CASE (-4294967297, -1),
    TEST_CASE (2147483648, -2147483648),
    TEST_CASE (-2147483648, -2147483648),
    TEST_CASE (2147483647, 2147483647),
    TEST_CASE (-2147483647, -2147483647),
    TEST_CASE (-2147483649, 2147483647),
    TEST_CASE (2147483649, -2147483647)
#undef TEST_CASE
  };

  for (uint32_t i = 0; i < sizeof (test_cases_int32) / sizeof (test_cases_int32[0]); i++)
  {
    TEST_ASSERT (ecma_number_to_int32 (test_cases_int32[i].num) == test_cases_int32[i].int32_num);
  }

  return 0;
} /* main */

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

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "jerryscript-port.h"
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JERRY_UNUSED(x) ((void) (x))

#define TEST_ASSERT(x)                                                \
  do                                                                  \
  {                                                                   \
    if (JERRY_UNLIKELY (!(x)))                                        \
    {                                                                 \
      jerry_port_log (JERRY_LOG_LEVEL_ERROR,                          \
                      "TEST: Assertion '%s' failed at %s(%s):%lu.\n", \
                      #x,                                             \
                      __FILE__,                                       \
                      __func__,                                       \
                      (unsigned long) __LINE__);                      \
      jerry_port_fatal (ERR_FAILED_INTERNAL_ASSERTION);               \
    }                                                                 \
  } while (0)

#define TEST_ASSERT_STR(EXPECTED, RESULT)                               \
  do                                                                    \
  {                                                                     \
    const char* __expected = (const char*) (EXPECTED);                  \
    const char* __result = (const char*) (RESULT);                      \
    if (strcmp (__expected, __result) != 0)                             \
    {                                                                   \
      jerry_port_log (JERRY_LOG_LEVEL_ERROR,                            \
                      "TEST: String comparison failed at %s(%s):%lu.\n" \
                      " Expected: '%s'\n Got: '%s'\n",                  \
                      __FILE__,                                         \
                      __func__,                                         \
                      (unsigned long) __LINE__,                         \
                      __expected,                                       \
                      __result);                                        \
      jerry_port_fatal (ERR_FAILED_INTERNAL_ASSERTION);                 \
    }                                                                   \
  } while (0)

/**
 * Test initialization statement that should be included
 * at the beginning of main function in every unit test.
 */
#define TEST_INIT()                                  \
  do                                                 \
  {                                                  \
    union                                            \
    {                                                \
      double d;                                      \
      unsigned u;                                    \
    } now = { .d = jerry_port_get_current_time () }; \
    srand (now.u);                                   \
  } while (0)

/**
 * Dummy macro to enable the breaking of long string literals into multiple
 * substrings on separate lines. (Style checker doesn't allow it without putting
 * the whole literal into parentheses but the compiler warns about parenthesized
 * string constants.)
 */
#define TEST_STRING_LITERAL(x) x

#endif /* !TEST_COMMON_H */

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

#define ARRAY_SIZE(array) ((unsigned long) (sizeof (array) / sizeof ((array)[0])))

#define JERRY_UNUSED(x) ((void) (x))

#define TEST_ASSERT(x)                                                \
  do                                                                  \
  {                                                                   \
    if (!(x))                                                         \
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

#define TEST_STRING_LITERAL(x) x

#endif /* !TEST_COMMON_H */

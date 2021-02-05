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

#ifndef JEXT_COMMON_H
#define JEXT_COMMON_H

#include "jerryscript-port.h"
#include "jerryscript.h"
#include <stdio.h>
#include <string.h>

/*
 * Make sure unused parameters, variables, or expressions trigger no compiler warning.
 */
#define JERRYX_UNUSED(x) ((void) (x))

/*
 * Asserts
 *
 * Warning:
 *         Don't use JERRY_STATIC_ASSERT in headers, because
 *         __LINE__ may be the same for asserts in a header
 *         and in an implementation file.
 */
#define JERRYX_STATIC_ASSERT_GLUE_(a, b, c) a##b##_##c
#define JERRYX_STATIC_ASSERT_GLUE(a, b, c)  JERRYX_STATIC_ASSERT_GLUE_ (a, b, c)
#define JERRYX_STATIC_ASSERT(x, msg)                                                  \
  enum                                                                                \
  {                                                                                   \
    JERRYX_STATIC_ASSERT_GLUE (static_assertion_failed_, __LINE__, msg) = 1 / (!!(x)) \
  }

#ifndef JERRY_NDEBUG
void JERRY_ATTR_NORETURN jerry_assert_fail (const char *assertion,
                                            const char *file,
                                            const char *function,
                                            const uint32_t line);
void JERRY_ATTR_NORETURN jerry_unreachable (const char *file, const char *function, const uint32_t line);

#define JERRYX_ASSERT(x)                                    \
  do                                                        \
  {                                                         \
    if (JERRY_UNLIKELY (!(x)))                              \
    {                                                       \
      jerry_assert_fail (#x, __FILE__, __func__, __LINE__); \
    }                                                       \
  } while (0)

#define JERRYX_UNREACHABLE()                          \
  do                                                  \
  {                                                   \
    jerry_unreachable (__FILE__, __func__, __LINE__); \
  } while (0)
#else /* JERRY_NDEBUG */
#define JERRYX_ASSERT(x) \
  do                     \
  {                      \
    if (false)           \
    {                    \
      JERRYX_UNUSED (x); \
    }                    \
  } while (0)

#ifdef __GNUC__
#define JERRYX_UNREACHABLE() __builtin_unreachable ()
#endif /* __GNUC__ */

#ifdef _MSC_VER
#define JERRYX_UNREACHABLE() _assume (0)
#endif /* _MSC_VER */

#ifndef JERRYX_UNREACHABLE
#define JERRYX_UNREACHABLE()
#endif /* !JERRYX_UNREACHABLE */

#endif /* !JERRY_NDEBUG */

/*
 * Logging
 */
#define JERRYX_ERROR_MSG(...)   jerry_port_log (JERRY_LOG_LEVEL_ERROR, __VA_ARGS__)
#define JERRYX_WARNING_MSG(...) jerry_port_log (JERRY_LOG_LEVEL_WARNING, __VA_ARGS__)
#define JERRYX_DEBUG_MSG(...)   jerry_port_log (JERRY_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define JERRYX_TRACE_MSG(...)   jerry_port_log (JERRY_LOG_LEVEL_TRACE, __VA_ARGS__)

#endif /* !JEXT_COMMON_H */

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

#if !defined(_XOPEN_SOURCE) || _XOPEN_SOURCE < 500
#undef _XOPEN_SOURCE
/* Required macro for sleep functions (nanosleep or usleep) */
#define _XOPEN_SOURCE 500
#endif

#ifdef _WIN32
#include <windows.h>
#else /* !_WIN32 */
#include <unistd.h>
#endif /* _WIN32 */

#include "jerryscript-port-default.h"
#include "jerryscript-port.h"

/**
 * Default implementation of jerry_port_sleep. Uses 'usleep' if available on the
 * system, does nothing otherwise.
 */
void
jerry_port_sleep (uint32_t sleep_time) /**< milliseconds to sleep */
{
#ifdef _WIN32
  Sleep (sleep_time);
#else /* !_WIN32 */
  usleep ((useconds_t) sleep_time * 1000);
#endif /* _WIN32 */
} /* jerry_port_sleep */

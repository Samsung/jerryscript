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

#ifndef JERRYX_AUTORELEASE_IMPL_H
#define JERRYX_AUTORELEASE_IMPL_H

#include "jerryscript.h"

#ifdef __GNUC__
/*
 * Calls jerry_release_value (*value).
 * The GCC __cleanup__ function must take a pointer to the variable to clean up.
 *
 * @return void
 */
static inline void
jerryx_autorelease_cleanup (const jerry_value_t *value) /**< jerry value */
{
  jerry_release_value (*value);
} /* jerryx_autorelease_cleanup */

#define __JERRYX_AR_VALUE_T_IMPL const jerry_value_t __attribute__ ((__cleanup__ (jerryx_autorelease_cleanup)))
#else /* !__GNUC__ */
/* TODO: for other compilers */
#error "No autorelease implementation for your compiler!"
#endif /* __GNUC__ */

#endif /* !JERRYX_AUTORELEASE_IMPL_H */

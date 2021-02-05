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

#ifndef JERRYX_AUTORELEASE_H
#define JERRYX_AUTORELEASE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "autorelease.impl.h"

/*
 * Macro for `const jerry_value_t` for which jerry_release_value () is
 * automatically called when the variable goes out of scope.
 *
 * Example usage:
 * static void foo (bool enable)
 * {
 *   JERRYX_AR_VALUE_T bar = jerry_create_string (...);
 *
 *   if (enable) {
 *     JERRYX_AR_VALUE_T baz = jerry_get_global_object ();
 *
 *     // ...
 *
 *     // jerry_release_value (baz) and jerry_release_value (bar) is called automatically before
 *     // returning, because `baz` and `bar` go out of scope.
 *     return;
 *   }
 *
 *   // jerry_release_value (bar) is called automatically when the function returns,
 *   // because `bar` goes out of scope.
 * }
 */
#define JERRYX_AR_VALUE_T __JERRYX_AR_VALUE_T_IMPL

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYX_AUTORELEASE_H */

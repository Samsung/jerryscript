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

#include "jerryscript-ext/handler.h"

/**
 * Expose garbage collector to scripts.
 *
 * @return undefined.
 */
jerry_value_t
jerryx_handler_gc (const jerry_value_t func_obj_val, /**< function object */
                   const jerry_value_t this_p, /**< this arg */
                   const jerry_value_t args_p[], /**< function arguments */
                   const jerry_length_t args_cnt) /**< number of function arguments */
{
  (void) func_obj_val; /* unused */
  (void) this_p; /* unused */

  jerry_gc_mode_t mode =
    ((args_cnt > 0 && jerry_value_to_boolean (args_p[0])) ? JERRY_GC_PRESSURE_HIGH : JERRY_GC_PRESSURE_LOW);

  jerry_gc (mode);
  return jerry_create_undefined ();
} /* jerryx_handler_gc */

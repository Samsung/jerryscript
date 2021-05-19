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
#include "register-drivers.h"

/**
 * Register all jerry-ext handlers, and released their returned value
 *   - print
 *   - resource
 *   - gc
 *   - assert
*/
void register_jerryx ()
{
  jerryx_handler_register_global ((const jerry_char_t *) "print",
                                  jerryx_handler_print);

  jerryx_handler_register_global ((const jerry_char_t *) "resource",
                                  jerryx_handler_resource_name);

  jerryx_handler_register_global ((const jerry_char_t *) "gc",
                                  jerryx_handler_gc);

  jerryx_handler_register_global ((const jerry_char_t *) "assert",
                                  jerryx_handler_assert);
}

void register_all ()
{
  register_DigitalOut ();
  register_jerryx ();
  register_MbedThread ();
}

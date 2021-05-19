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
#include "setup.h"

extern uint32_t jsmbed_js_magic_string_count;
extern uint32_t jsmbed_js_magic_string_values[];

extern const jerry_char_t *jsmbed_js_magic_strings[];
extern const jerry_length_t jsmbed_js_magic_string_lengths[];

void jsmbed_js_load_magic_strings ()
{
  if (jsmbed_js_magic_string_count == 0)
  {
    return;
  }

  jerry_register_magic_strings (jsmbed_js_magic_strings,
                                jsmbed_js_magic_string_count,
                                jsmbed_js_magic_string_lengths);

  jerry_value_t global = jerry_get_global_object ();

  for (unsigned int idx = 0; idx < jsmbed_js_magic_string_count; idx++)
  {
    jerry_value_t constant_value = jerry_create_number (jsmbed_js_magic_string_values[idx]);
    jerry_value_t magic_string = jerry_create_string (jsmbed_js_magic_strings[idx]);

    jerry_release_value(jerry_set_property (global, magic_string, constant_value));

    jerry_release_value (constant_value);
    jerry_release_value (magic_string);
  }

  jerry_release_value (global);
}

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

#include "jerryscript-ext/module.h"
#include "jerryscript.h"
#include "test-common.h"
#include <string.h>

int
main (int argc, char **argv)
{
  (void) argc;
  (void) argv;
  jerry_char_t buffer[256];
  jerry_size_t bytes_copied;
  const jerryx_module_resolver_t *resolver = &jerryx_module_native_resolver;
  jerry_value_t module_name;

  jerry_init (JERRY_INIT_EMPTY);

  /* Attempt to load a non-existing module. */
  module_name = jerry_create_string ((jerry_char_t *) "some-unknown-module-name");
  jerry_value_t module = jerryx_module_resolve (module_name, &resolver, 1);
  jerry_release_value (module_name);

  TEST_ASSERT (jerry_value_is_error (module));

  /* Retrieve the error message. */
  module = jerry_get_value_from_error (module, true);
  jerry_value_t prop_name = jerry_create_string ((const jerry_char_t *) "message");
  jerry_value_t prop = jerry_get_property (module, prop_name);

  /* Assert that the error message is a string with specific contents. */
  TEST_ASSERT (jerry_value_is_string (prop));

  bytes_copied = jerry_substring_to_utf8_char_buffer (prop, 0, 254, buffer, 256);
  buffer[bytes_copied] = 0;
  TEST_ASSERT (!strcmp ((const char *) buffer, "Module not found"));

  /* Release the error message property name and value. */
  jerry_release_value (prop);
  jerry_release_value (prop_name);

  /* Retrieve the moduleName property. */
  prop_name = jerry_create_string ((const jerry_char_t *) "moduleName");
  prop = jerry_get_property (module, prop_name);

  /* Assert that the moduleName property is a string containing the requested module name. */
  TEST_ASSERT (jerry_value_is_string (prop));

  bytes_copied = jerry_substring_to_utf8_char_buffer (prop, 0, 254, buffer, 256);
  buffer[bytes_copied] = 0;
  TEST_ASSERT (!strcmp ((const char *) buffer, "some-unknown-module-name"));

  /* Release everything. */
  jerry_release_value (prop);
  jerry_release_value (prop_name);
  jerry_release_value (module);

  return 0;
} /* main */

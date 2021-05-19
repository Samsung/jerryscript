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
#include "launcher.h"
#include "setup.h"
#include "jerry-targetjs.h"
#include "register-drivers.h"
#include "rtos.h"
#include "jerryscript.h"
#include "js_source.h"
#include "logging.h"

DECLARE_JS_CODES;

/**
 * run js
 *
 * Merges javascript files into one code, then jerry_parse and run them
*/
int run_js ()
{
  /*merge*/
  size_t length = 0;
  for (int src = 0; js_codes[src].source; src++)
  {
    length += js_codes[src].length;
  }

  char* merged_code = (char*) malloc (length);
  memcpy (merged_code, js_codes[0].source, js_codes[0].length);

  for (int src = 1; js_codes[src].source; src++)
  {
    memcpy (&merged_code[js_codes[src-1].length], js_codes[src].source, js_codes[src].length);
  }

  /*parse and run*/
  const jerry_char_t* code = reinterpret_cast<const jerry_char_t*>(merged_code);
  jerry_value_t parsed_code = jerry_parse (NULL, 0, code, length, JERRY_PARSE_NO_OPTS);
  free (merged_code);
  if (jerry_value_is_error (parsed_code))
  {
    LOG_PRINT_ALWAYS("jerry_parse failed\r\n");
    jerry_release_value (parsed_code);
    return -1;
  }
  jerry_value_t ret_val = jerry_run (parsed_code);
  if (jerry_value_is_error (ret_val))
  {
    LOG_PRINT_ALWAYS("jerry_run failed\r\n");
    jerry_release_value (ret_val);
    jerry_release_value (parsed_code);
    return -1;
  }
  jerry_release_value (ret_val);
  jerry_release_value (parsed_code);
  return 0;
}

/**
 * jsmbed_init
 * 
 * Init jerry, register drivers
*/
void jsmbed_init ()
{
  /*Get current date*/
  union { double d; unsigned u; } now = { .d = jerry_port_get_current_time () };
  srand (now.u);

  jerry_init (JERRY_INIT_EMPTY);
  LOG_PRINT("jerry_init done \r\n");

  /*Register drivers/functions*/
  register_all ();
  LOG_PRINT("drivers are ready \r\n");

  jsmbed_js_load_magic_strings ();
  LOG_PRINT("magic strings are loaded\r\njsmbed_init done\r\n\n\n");
}

/**
 * jsmbed_launch
 *
 * Run the loaded Js code

*/
void jsmbed_launch ()
{
  puts("   JerryScript in mbed\r\n");
  puts("   build date:  " __DATE__ " \r\n");
  run_js ();
}

/**
 * jsmbed_exit
 * 
 * Run jerry_cleanup
*/
void jsmbed_exit() {
  LOG_PRINT("running jerry_cleanup... \r\n");
  jerry_cleanup ();
  LOG_PRINT("jerry_cleanup is done \r\n");
}

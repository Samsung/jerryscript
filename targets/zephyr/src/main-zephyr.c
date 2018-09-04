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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <zephyr.h>
#include <misc/printk.h>
#include "getline-zephyr.h"

#include "jerryscript.h"
#include "jerryscript-port.h"
#include "jerryscript-ext/handler.h"

static jerry_value_t print_function;

/**
 * Register a JavaScript function in the global object.
 */
static void
register_js_function (const char *name_p, /**< name of the function */
                      jerry_external_handler_t handler_p) /**< function callback */
{
  jerry_value_t result_val = jerryx_handler_register_global ((const jerry_char_t *) name_p, handler_p);

  if (jerry_value_is_error (result_val))
  {
    jerry_port_log (JERRY_LOG_LEVEL_WARNING, "Warning: failed to register '%s' method.", name_p);
  }

  jerry_release_value (result_val);
} /* register_js_function */

static int shell_cmd_handler (char *source_buffer)
{
  jerry_value_t ret_val;

  ret_val = jerry_eval ((jerry_char_t *) source_buffer,
    strlen (source_buffer),
    JERRY_PARSE_NO_OPTS);

  if (jerry_value_is_error (ret_val))
  {
    /* User-friendly error messages require at least "cp" JerryScript
       profile. Include a message prefix in case "cp_minimal" profile
       is used. */
    printf ("Error executing statement: ");
    /* Clear error flag, otherwise print call below won't produce any
       output. */
    ret_val = jerry_get_value_from_error (ret_val, true);
  }

  if (!jerry_value_is_error (print_function))
  {
    jerry_value_t ret_val_print = jerry_call_function (print_function,
      jerry_create_undefined (),
      &ret_val,
      1);
    jerry_release_value (ret_val_print);
  }

  jerry_release_value (ret_val);

  return 0;
} /* shell_cmd_handler */

void main (void)
{
  srand ((unsigned) jerry_port_get_current_time ());
  uint32_t zephyr_ver = sys_kernel_version_get ();
  printf ("JerryScript build: " __DATE__ " " __TIME__ "\n");
  printf ("JerryScript API %d.%d\n", JERRY_API_MAJOR_VERSION, JERRY_API_MINOR_VERSION);
  printf ("Zephyr version %d.%d.%d\n", (int)SYS_KERNEL_VER_MAJOR (zephyr_ver),
    (int)SYS_KERNEL_VER_MINOR (zephyr_ver),
    (int)SYS_KERNEL_VER_PATCHLEVEL (zephyr_ver));

  zephyr_getline_init ();
  jerry_init (JERRY_INIT_DEFAULT);
  register_js_function ("print", jerryx_handler_print);
  jerry_value_t global_obj_val = jerry_get_global_object ();

  jerry_value_t print_func_name_val = jerry_create_string ((jerry_char_t *) "print");
  print_function = jerry_get_property (global_obj_val, print_func_name_val);
  jerry_release_value (print_func_name_val);
  jerry_release_value (global_obj_val);
  if (jerry_value_is_error (print_function))
  {
    printf ("Error: could not look up print function, expression results won't be printed\n");
  }

  while (1)
  {
    char *s;
    printf("js> ");
    fflush(stdout);
    s = zephyr_getline ();
    if (*s)
    {
      shell_cmd_handler (s);
    }
  }

  /* As we never retturn from REPL above, don't call jerry_cleanup() here. */
} /* main */

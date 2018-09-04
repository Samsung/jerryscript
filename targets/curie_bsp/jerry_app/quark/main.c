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
#include <stdint.h>
#include <string.h>

/* Infra */
#include "infra/bsp.h"
#include "infra/reboot.h"
#include "infra/log.h"
#include "infra/time.h"
#include "infra/system_events.h"
#include "infra/tcmd/handler.h"

#include "cfw/cfw.h"
/* Watchdog helper */
#include "infra/wdt_helper.h"

#include "jerryscript.h"
#include "jerryscript-port.h"
#include "string.h"

#include "zephyr.h"
#include "microkernel/task.h"
#include "os/os.h"
#include "misc/printk.h"

static T_QUEUE queue;

jerry_value_t print_function;

void jerry_resolve_error (jerry_value_t ret_value)
{
  if (jerry_value_is_error (ret_value))
  {
    ret_value = jerry_get_value_from_error (ret_value, true);
    jerry_value_t err_str_val = jerry_value_to_string (ret_value);
    jerry_size_t err_str_size = jerry_get_string_size (err_str_val);
    jerry_char_t *err_str_buf = (jerry_char_t *) balloc (err_str_size, NULL);
    jerry_size_t sz = jerry_string_to_char_buffer (err_str_val, err_str_buf, err_str_size);
    err_str_buf[sz] = 0;
    printk ("Script Error: unhandled exception: %s\n", err_str_buf);
    bfree(err_str_buf);
    jerry_release_value (err_str_val);
  }
}

void help ()
{
  printk ("Usage:\n");
  printk ("js e 'JavaScript Command'\n");
  printk ("eg. js e print ('Hello World');\n");
}

void eval_jerry_script (int argc, char *argv[], struct tcmd_handler_ctx *ctx)
{
  if (argc < 3)
  {
    TCMD_RSP_ERROR (ctx, NULL);
    help ();
    return;
  }
  else
  {
    OS_ERR_TYPE err;
    size_t str_total_length = 0;
    size_t *str_lens = (size_t *) balloc ((argc - 2) * sizeof(size_t), &err);
    if (str_lens == NULL || err != E_OS_OK)
    {
      printk ("%s: allocate memory failed!", __func__);
      TCMD_RSP_ERROR (ctx, NULL);
      return;
    }
    for (int i = 2; i < argc; ++i)
    {
      str_lens[i - 2] = strlen (argv[i]);
      str_total_length += str_lens[i - 2] + 1;
    }
    err = E_OS_OK;
    char *buffer = (char *) balloc (str_total_length, &err);
    if (buffer == NULL || err != E_OS_OK)
    {
      printk ("%s: allocate memory failed!", __func__);
      TCMD_RSP_ERROR (ctx, NULL);
      return;
    }

    char *p = buffer;
    for (int i = 2; i < argc; ++i)
    {
      for (int j =0; j < str_lens[i - 2]; ++j)
      {
        *p = argv[i][j];
        ++p;
      }
      *p = ' ';
      ++p;
    }
    *p = '\0';

    jerry_value_t eval_ret = jerry_eval (buffer, str_total_length - 1, JERRY_PARSE_NO_OPTS);

    if (jerry_value_is_error (eval_ret))
    {
      jerry_resolve_error (eval_ret);
      TCMD_RSP_ERROR (ctx, NULL);
    }
    else
    {
      jerry_value_t args[] = {eval_ret};
      jerry_value_t ret_val_print = jerry_call_function (print_function,
                                                         jerry_create_undefined (),
                                                         args,
                                                         1);
      jerry_release_value (ret_val_print);
      TCMD_RSP_FINAL (ctx, NULL);
    }
    jerry_release_value (eval_ret);
    bfree (buffer);
    bfree (str_lens);
  }
}

void jerry_start ()
{
  srand ((unsigned) jerry_port_get_current_time ());
  jerry_init (JERRY_INIT_DEFAULT);
  jerry_value_t global_obj_val = jerry_get_global_object ();
  jerry_value_t print_func_name_val = jerry_create_string ((jerry_char_t *) "print");
  print_function = jerry_get_property (global_obj_val, print_func_name_val);
  jerry_release_value (print_func_name_val);
  jerry_release_value (global_obj_val);
}

/* Application main entry point */
void main_task (void *param)
{
  /* Init BSP (also init BSP on ARC core) */
  queue = bsp_init ();
  /* start Quark watchdog */
  wdt_start (WDT_MAX_TIMEOUT_MS);
  /* Init the CFW */
  cfw_init (queue);
  jerry_start ();
  /* Loop to process message queue */
  while (1)
  {
    OS_ERR_TYPE err = E_OS_OK;
    /* Process message with a given timeout */
    queue_process_message_wait (queue, 5000, &err);
    /* Acknowledge the system watchdog to prevent panic and reset */
    wdt_keepalive ();
  }
}

DECLARE_TEST_COMMAND (js, e, eval_jerry_script);

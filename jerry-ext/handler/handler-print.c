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

#include "jerryscript-debugger.h"
#include "jerryscript-ext/handler.h"
#include "jerryscript-port.h"

/**
 * Provide a 'print' implementation for scripts.
 *
 * The routine converts all of its arguments to strings and outputs them
 * char-by-char using jerry_port_print_char.
 *
 * The NUL character is output as "\u0000", other characters are output
 * bytewise.
 *
 * Note:
 *      This implementation does not use standard C `printf` to print its
 *      output. This allows more flexibility but also extends the core
 *      JerryScript engine port API. Applications that want to use
 *      `jerryx_handler_print` must ensure that their port implementation also
 *      provides `jerry_port_print_char`.
 *
 * @return undefined - if all arguments could be converted to strings,
 *         error - otherwise.
 */
jerry_value_t
jerryx_handler_print (const jerry_value_t func_obj_val, /**< function object */
                      const jerry_value_t this_p, /**< this arg */
                      const jerry_value_t args_p[], /**< function arguments */
                      const jerry_length_t args_cnt) /**< number of function arguments */
{
  (void) func_obj_val; /* unused */
  (void) this_p; /* unused */

  const char *const null_str = "\\u0000";

  jerry_value_t ret_val = jerry_create_undefined ();

  for (jerry_length_t arg_index = 0; arg_index < args_cnt; arg_index++)
  {
    jerry_value_t str_val;

    if (jerry_value_is_symbol (args_p[arg_index]))
    {
      str_val = jerry_get_symbol_descriptive_string (args_p[arg_index]);
    }
    else
    {
      str_val = jerry_value_to_string (args_p[arg_index]);
    }

    if (jerry_value_is_error (str_val))
    {
      /* There is no need to free the undefined value. */
      ret_val = str_val;
      break;
    }

    jerry_length_t length = jerry_get_utf8_string_length (str_val);
    jerry_length_t substr_pos = 0;
    jerry_char_t substr_buf[256];

    do
    {
      jerry_size_t substr_size = jerry_substring_to_utf8_char_buffer (str_val, substr_pos, length, substr_buf, 256 - 1);

      jerry_char_t *buf_end_p = substr_buf + substr_size;

      /* Update start position by the number of utf-8 characters. */
      for (jerry_char_t *buf_p = substr_buf; buf_p < buf_end_p; buf_p++)
      {
        /* Skip intermediate utf-8 octets. */
        if ((*buf_p & 0xc0) != 0x80)
        {
          substr_pos++;
        }
      }

      if (substr_pos == length)
      {
        *buf_end_p++ = (arg_index < args_cnt - 1) ? ' ' : '\n';
      }

      for (jerry_char_t *buf_p = substr_buf; buf_p < buf_end_p; buf_p++)
      {
        char chr = (char) *buf_p;

        if (chr != '\0')
        {
          jerry_port_print_char (chr);
          continue;
        }

        for (jerry_size_t null_index = 0; null_str[null_index] != '\0'; null_index++)
        {
          jerry_port_print_char (null_str[null_index]);
        }
      }
    } while (substr_pos < length);

    jerry_release_value (str_val);
  }

  if (args_cnt == 0 || jerry_value_is_error (ret_val))
  {
    jerry_port_print_char ('\n');
  }

  return ret_val;
} /* jerryx_handler_print */

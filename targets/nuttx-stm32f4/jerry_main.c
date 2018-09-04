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

#include "jerryscript.h"
#include "jerryscript-ext/debugger.h"
#include "jerryscript-ext/handler.h"
#include "jerryscript-port.h"
#include "setjmp.h"

/**
 * Maximum command line arguments number.
 */
#define JERRY_MAX_COMMAND_LINE_ARGS (16)

/**
 * Standalone Jerry exit codes.
 */
#define JERRY_STANDALONE_EXIT_CODE_OK   (0)
#define JERRY_STANDALONE_EXIT_CODE_FAIL (1)

/**
 * Context size of the SYNTAX_ERROR
 */
#define SYNTAX_ERROR_CONTEXT_SIZE 2

/**
 * Print usage and available options
 */
static void
print_help (char *name)
{
  printf ("Usage: %s [OPTION]... [FILE]...\n"
          "\n"
          "Options:\n"
          "  --log-level [0-3]\n"
          "  --mem-stats\n"
          "  --mem-stats-separate\n"
          "  --show-opcodes\n"
          "  --start-debug-server\n"
          "  --debug-server-port [port]\n"
          "\n",
          name);
} /* print_help */

/**
 * Read source code into buffer.
 *
 * Returned value must be freed with jmem_heap_free_block if it's not NULL.
 * @return NULL, if read or allocation has failed
 *         pointer to the allocated memory block, otherwise
 */
static const uint8_t *
read_file (const char *file_name, /**< source code */
           size_t *out_size_p) /**< [out] number of bytes successfully read from source */
{
  FILE *file = fopen (file_name, "r");
  if (file == NULL)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: cannot open file: %s\n", file_name);
    return NULL;
  }

  int fseek_status = fseek (file, 0, SEEK_END);
  if (fseek_status != 0)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Failed to seek (error: %d)\n", fseek_status);
    fclose (file);
    return NULL;
  }

  long script_len = ftell (file);
  if (script_len < 0)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Failed to get the file size(error %ld)\n", script_len);
    fclose (file);
    return NULL;
  }

  rewind (file);

  uint8_t *buffer = (uint8_t *) malloc (script_len);

  if (buffer == NULL)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Out of memory error\n");
    fclose (file);
    return NULL;
  }

  size_t bytes_read = fread (buffer, 1u, script_len, file);

  if (!bytes_read || bytes_read != script_len)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: failed to read file: %s\n", file_name);
    free ((void*) buffer);

    fclose (file);
    return NULL;
  }

  fclose (file);

  *out_size_p = bytes_read;
  return (const uint8_t *) buffer;
} /* read_file */

/**
 * Convert string into unsigned integer
 *
 * @return converted number
 */
static uint32_t
str_to_uint (const char *num_str_p) /**< string to convert */
{
  assert (jerry_is_feature_enabled (JERRY_FEATURE_ERROR_MESSAGES));

  uint32_t result = 0;

  while (*num_str_p != '\0')
  {
    assert (*num_str_p >= '0' && *num_str_p <= '9');

    result *= 10;
    result += (uint32_t) (*num_str_p - '0');
    num_str_p++;
  }

  return result;
} /* str_to_uint */

/**
 * Print error value
 */
static void
print_unhandled_exception (jerry_value_t error_value, /**< error value */
                           const jerry_char_t *source_p) /**< source_p */
{
  assert (jerry_value_is_error (error_value));

  error_value = jerry_get_value_from_error (error_value, false);
  jerry_value_t err_str_val = jerry_value_to_string (error_value);
  jerry_size_t err_str_size = jerry_get_string_size (err_str_val);
  jerry_char_t err_str_buf[256];

  jerry_release_value (error_value);

  if (err_str_size >= 256)
  {
    const char msg[] = "[Error message too long]";
    err_str_size = sizeof (msg) / sizeof (char) - 1;
    memcpy (err_str_buf, msg, err_str_size);
  }
  else
  {
    jerry_size_t sz = jerry_string_to_char_buffer (err_str_val, err_str_buf, err_str_size);
    assert (sz == err_str_size);
    err_str_buf[err_str_size] = 0;

    if (jerry_is_feature_enabled (JERRY_FEATURE_ERROR_MESSAGES)
        && jerry_get_error_type (error_value) == JERRY_ERROR_SYNTAX)
    {
      uint32_t err_line = 0;
      uint32_t err_col = 0;

      /* 1. parse column and line information */
      for (uint32_t i = 0; i < sz; i++)
      {
        if (!strncmp ((char *) (err_str_buf + i), "[line: ", 7))
        {
          i += 7;

          char num_str[8];
          uint32_t j = 0;

          while (i < sz && err_str_buf[i] != ',')
          {
            num_str[j] = (char) err_str_buf[i];
            j++;
            i++;
          }
          num_str[j] = '\0';

          err_line = str_to_uint (num_str);

          if (strncmp ((char *) (err_str_buf + i), ", column: ", 10))
          {
            break; /* wrong position info format */
          }

          i += 10;
          j = 0;

          while (i < sz && err_str_buf[i] != ']')
          {
            num_str[j] = (char) err_str_buf[i];
            j++;
            i++;
          }
          num_str[j] = '\0';

          err_col = str_to_uint (num_str);
          break;
        }
      } /* for */

      if (err_line != 0 && err_col != 0)
      {
        uint32_t curr_line = 1;

        bool is_printing_context = false;
        uint32_t pos = 0;

        /* 2. seek and print */
        while (source_p[pos] != '\0')
        {
          if (source_p[pos] == '\n')
          {
            curr_line++;
          }

          if (err_line < SYNTAX_ERROR_CONTEXT_SIZE
              || (err_line >= curr_line
                  && (err_line - curr_line) <= SYNTAX_ERROR_CONTEXT_SIZE))
          {
            /* context must be printed */
            is_printing_context = true;
          }

          if (curr_line > err_line)
          {
            break;
          }

          if (is_printing_context)
          {
            jerry_port_log (JERRY_LOG_LEVEL_ERROR, "%c", source_p[pos]);
          }

          pos++;
        }

        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "\n");

        while (--err_col)
        {
          jerry_port_log (JERRY_LOG_LEVEL_ERROR, "~");
        }

        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "^\n");
      }
    }
  }

  jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Script Error: %s\n", err_str_buf);
  jerry_release_value (err_str_val);
} /* print_unhandled_exception */

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

/**
 * JerryScript log level
 */
static jerry_log_level_t jerry_log_level = JERRY_LOG_LEVEL_ERROR;

/**
 * Main program.
 *
 * @return 0 if success, error code otherwise
 */
#ifdef CONFIG_BUILD_KERNEL
int main (int argc, FAR char *argv[])
#else
int jerry_main (int argc, char *argv[])
#endif
{
  if (argc > JERRY_MAX_COMMAND_LINE_ARGS)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR,
                    "Too many command line arguments. Current maximum is %d\n",
                    JERRY_MAX_COMMAND_LINE_ARGS);

    return JERRY_STANDALONE_EXIT_CODE_FAIL;
  }

  const char *file_names[JERRY_MAX_COMMAND_LINE_ARGS];
  int i;
  int files_counter = 0;
  bool start_debug_server = false;
  uint16_t debug_port = 5001;

  jerry_init_flag_t flags = JERRY_INIT_EMPTY;

  for (i = 1; i < argc; i++)
  {
    if (!strcmp ("-h", argv[i]) || !strcmp ("--help", argv[i]))
    {
      print_help (argv[0]);
      return JERRY_STANDALONE_EXIT_CODE_OK;
    }
    else if (!strcmp ("--mem-stats", argv[i]))
    {
      flags |= JERRY_INIT_MEM_STATS;
      jerry_log_level = JERRY_LOG_LEVEL_DEBUG;
    }
    else if (!strcmp ("--mem-stats-separate", argv[i]))
    {
      flags |= JERRY_INIT_MEM_STATS_SEPARATE;
      jerry_log_level = JERRY_LOG_LEVEL_DEBUG;
    }
    else if (!strcmp ("--show-opcodes", argv[i]))
    {
      flags |= JERRY_INIT_SHOW_OPCODES | JERRY_INIT_SHOW_REGEXP_OPCODES;
      jerry_log_level = JERRY_LOG_LEVEL_DEBUG;
    }
    else if (!strcmp ("--log-level", argv[i]))
    {
      if (++i < argc && strlen (argv[i]) == 1 && argv[i][0] >='0' && argv[i][0] <= '3')
      {
        jerry_log_level = argv[i][0] - '0';
      }
      else
      {
        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: wrong format or invalid argument\n");
        return JERRY_STANDALONE_EXIT_CODE_FAIL;
      }
    }
    else if (!strcmp ("--start-debug-server", argv[i]))
    {
      start_debug_server = true;
    }
    else if (!strcmp ("--debug-server-port", argv[i]))
    {
      if (++i < argc)
      {
        debug_port = str_to_uint (argv[i]);
      }
      else
      {
        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: wrong format or invalid argument\n");
        return JERRY_STANDALONE_EXIT_CODE_FAIL;
      }
    }
    else
    {
      file_names[files_counter++] = argv[i];
    }
  }

  jerry_init (JERRY_INIT_FLAGS (flags));

  if (start_debug_server)
  {
    jerryx_debugger_after_connect (jerryx_debugger_tcp_create (debug_port)
                                   && jerryx_debugger_ws_create ());
  }

  register_js_function ("assert", jerryx_handler_assert);
  register_js_function ("gc", jerryx_handler_gc);
  register_js_function ("print", jerryx_handler_print);

  jerry_value_t ret_value = jerry_create_undefined ();

  if (files_counter == 0)
  {
    printf ("No input files, running a hello world demo:\n");
    const jerry_char_t script[] = "var str = 'Hello World'; print(str + ' from JerryScript')";
    size_t script_size = strlen ((const char *) script);

    ret_value = jerry_parse (NULL, 0, script, script_size, JERRY_PARSE_NO_OPTS);

    if (!jerry_value_is_error (ret_value))
    {
      ret_value = jerry_run (ret_value);
    }
  }
  else
  {
    for (i = 0; i < files_counter; i++)
    {
      size_t source_size;
      const jerry_char_t *source_p = read_file (file_names[i], &source_size);

      if (source_p == NULL)
      {
        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Source file load error\n");
        return JERRY_STANDALONE_EXIT_CODE_FAIL;
      }

      ret_value = jerry_parse ((jerry_char_t *) file_names[i],
                               strlen (file_names[i]),
                               source_p,
                               source_size,
                               JERRY_PARSE_NO_OPTS);

      if (!jerry_value_is_error (ret_value))
      {
        jerry_value_t func_val = ret_value;
        ret_value = jerry_run (func_val);
        jerry_release_value (func_val);
      }

      if (jerry_value_is_error (ret_value))
      {
        print_unhandled_exception (ret_value, source_p);
        free ((void*) source_p);

        break;
      }

      free ((void*) source_p);

      jerry_release_value (ret_value);
      ret_value = jerry_create_undefined ();
    }
  }

  int ret_code = JERRY_STANDALONE_EXIT_CODE_OK;

  if (jerry_value_is_error (ret_value))
  {
    ret_code = JERRY_STANDALONE_EXIT_CODE_FAIL;
  }

  jerry_release_value (ret_value);

  ret_value = jerry_run_all_enqueued_jobs ();

  if (jerry_value_is_error (ret_value))
  {
    ret_code = JERRY_STANDALONE_EXIT_CODE_FAIL;
  }

  jerry_release_value (ret_value);
  jerry_cleanup ();

  return ret_code;
} /* main */

/**
 * Aborts the program.
 */
void jerry_port_fatal (jerry_fatal_code_t code)
{
  exit (1);
} /* jerry_port_fatal */

/**
 * Provide log message implementation for the engine.
 */
void
jerry_port_log (jerry_log_level_t level, /**< log level */
                const char *format, /**< format string */
                ...)  /**< parameters */
{
  if (level <= jerry_log_level)
  {
    va_list args;
    va_start (args, format);
    vfprintf (stderr, format, args);
    va_end (args);
  }
} /* jerry_port_log */

/**
 * Dummy function to get the time zone.
 *
 * @return true
 */
bool
jerry_port_get_time_zone (jerry_time_zone_t *tz_p)
{
  /* We live in UTC. */
  tz_p->offset = 0;
  tz_p->daylight_saving_time = 0;

  return true;
} /* jerry_port_get_time_zone */

/**
 * Dummy function to get the current time.
 *
 * @return 0
 */
double
jerry_port_get_current_time (void)
{
  return 0;
} /* jerry_port_get_current_time */

/**
 * Provide the implementation of jerryx_port_handler_print_char.
 * Uses 'printf' to print a single character to standard output.
 */
void
jerryx_port_handler_print_char (char c) /**< the character to print */
{
  printf ("%c", c);
} /* jerryx_port_handler_print_char */

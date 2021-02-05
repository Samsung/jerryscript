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

#include "jerryscript-ext/debugger.h"
#include "jerryscript-ext/handler.h"
#include "jerryscript-port-default.h"
#include "jerryscript-port.h"
#include "jerryscript.h"
#include "main-options.h"
#include "main-utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Temporal buffer size.
 */
#define JERRY_BUFFER_SIZE 256u

#if defined(JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1)
/**
 * The alloc function passed to jerry_create_context
 */
static void *
context_alloc (size_t size, void *cb_data_p)
{
  (void) cb_data_p; /* unused */
  return malloc (size);
} /* context_alloc */
#endif /* defined (JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1) */

int
main (int argc, char **argv)
{
  union
  {
    double d;
    unsigned u;
  } now = { .d = jerry_port_get_current_time () };
  srand (now.u);

  JERRY_VLA (main_source_t, sources_p, argc);

  main_args_t arguments;
  arguments.sources_p = sources_p;

  main_parse_args (argc, argv, &arguments);

#if defined(JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1)
  jerry_context_t *context_p = jerry_create_context (JERRY_GLOBAL_HEAP_SIZE * 1024, context_alloc, NULL);
  jerry_port_default_set_current_context (context_p);
#endif /* defined (JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1) */

restart:
  main_init_engine (&arguments);
  int return_code = JERRY_STANDALONE_EXIT_CODE_FAIL;
  jerry_value_t ret_value;

  for (uint32_t source_index = 0; source_index < arguments.source_count; source_index++)
  {
    main_source_t *source_file_p = sources_p + source_index;
    const char *file_path_p = argv[source_file_p->path_index];

    size_t source_size;
    uint8_t *source_p = jerry_port_read_source (file_path_p, &source_size);

    if (source_p == NULL)
    {
      goto exit;
    }

    uint32_t parse_opts = JERRY_PARSE_NO_OPTS;

    switch (source_file_p->type)
    {
      case SOURCE_SNAPSHOT:
      {
        ret_value = jerry_exec_snapshot ((uint32_t *) source_p,
                                         source_size,
                                         source_file_p->snapshot_index,
                                         JERRY_SNAPSHOT_EXEC_COPY_DATA);

        jerry_port_release_source (source_p);
        break;
      }
      case SOURCE_MODULE:
      {
        parse_opts = JERRY_PARSE_MODULE;
        /* FALLTHRU */
      }
      default:
      {
        assert (source_file_p->type == SOURCE_SCRIPT || source_file_p->type == SOURCE_MODULE);

        if (!jerry_is_valid_utf8_string ((jerry_char_t *) source_p, (jerry_size_t) source_size))
        {
          jerry_port_release_source (source_p);
          jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: Input must be a valid UTF-8 string.");
          goto exit;
        }

        ret_value = jerry_parse ((jerry_char_t *) file_path_p, strlen (file_path_p), source_p, source_size, parse_opts);

        jerry_port_release_source (source_p);

        if (!jerry_value_is_error (ret_value) && !(arguments.option_flags & OPT_FLAG_PARSE_ONLY))
        {
          jerry_value_t func_val = ret_value;
          ret_value = jerry_run (func_val);
          jerry_release_value (func_val);
        }

        break;
      }
    }

    if (jerry_value_is_error (ret_value))
    {
      if (main_is_value_reset (ret_value))
      {
        jerry_cleanup ();

        goto restart;
      }

      main_print_unhandled_exception (ret_value);
      goto exit;
    }

    jerry_release_value (ret_value);
  }

  if (arguments.option_flags & OPT_FLAG_WAIT_SOURCE)
  {
    while (true)
    {
      jerry_debugger_wait_for_source_status_t receive_status;
      receive_status = jerry_debugger_wait_for_client_source (main_wait_for_source_callback, NULL, &ret_value);

      if (receive_status == JERRY_DEBUGGER_SOURCE_RECEIVE_FAILED)
      {
        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Connection aborted before source arrived.");
        goto exit;
      }

      if (receive_status == JERRY_DEBUGGER_SOURCE_END)
      {
        jerry_port_log (JERRY_LOG_LEVEL_DEBUG, "No more client source.\n");
        break;
      }

      assert (receive_status == JERRY_DEBUGGER_CONTEXT_RESET_RECEIVED
              || receive_status == JERRY_DEBUGGER_SOURCE_RECEIVED);

      if (receive_status == JERRY_DEBUGGER_CONTEXT_RESET_RECEIVED || main_is_value_reset (ret_value))
      {
        jerry_cleanup ();
        goto restart;
      }

      assert (receive_status == JERRY_DEBUGGER_SOURCE_RECEIVED);
      jerry_release_value (ret_value);
    }
  }
  else if (arguments.option_flags & OPT_FLAG_USE_STDIN)
  {
    char buffer[JERRY_BUFFER_SIZE];
    char *source_p = NULL;
    size_t source_size = 0;

    while (!feof (stdin))
    {
      size_t read_bytes = fread (buffer, 1u, JERRY_BUFFER_SIZE, stdin);

      size_t new_size = source_size + read_bytes;
      source_p = realloc (source_p, new_size);

      memcpy (source_p + source_size, buffer, read_bytes);
      source_size = new_size;
    }

    ret_value = jerry_parse (NULL, 0, (jerry_char_t *) source_p, source_size, JERRY_PARSE_NO_OPTS);
    free (source_p);

    if (jerry_value_is_error (ret_value))
    {
      main_print_unhandled_exception (ret_value);
      goto exit;
    }

    jerry_value_t func_val = ret_value;
    ret_value = jerry_run (func_val);
    jerry_release_value (func_val);

    if (jerry_value_is_error (ret_value))
    {
      main_print_unhandled_exception (ret_value);
      goto exit;
    }

    jerry_release_value (ret_value);
  }
  else if (arguments.source_count == 0)
  {
    const char *prompt = (arguments.option_flags & OPT_FLAG_NO_PROMPT) ? "" : "jerry> ";
    char buffer[JERRY_BUFFER_SIZE];

    while (true)
    {
      printf ("%s", prompt);
      char *str_p = fgets (buffer, JERRY_BUFFER_SIZE, stdin);

      if (str_p == NULL)
      {
        printf ("\n");
        break;
      }

      size_t len = strlen (str_p);

      if (len == 0)
      {
        continue;
      }

      if (!jerry_is_valid_utf8_string ((jerry_char_t *) str_p, (jerry_size_t) len))
      {
        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: Input must be a valid UTF-8 string.\n");
        continue;
      }

      ret_value = jerry_parse (NULL, 0, (jerry_char_t *) str_p, len, JERRY_PARSE_NO_OPTS);

      if (jerry_value_is_error (ret_value))
      {
        main_print_unhandled_exception (ret_value);
        continue;
      }

      jerry_value_t func_val = ret_value;
      ret_value = jerry_run (func_val);
      jerry_release_value (func_val);

      if (jerry_value_is_error (ret_value))
      {
        main_print_unhandled_exception (ret_value);
        continue;
      }

      const jerry_value_t args[] = { ret_value };
      jerry_value_t ret_val_print =
        jerryx_handler_print (jerry_create_undefined (), jerry_create_undefined (), args, 1);
      jerry_release_value (ret_val_print);
      jerry_release_value (ret_value);
      ret_value = jerry_run_all_enqueued_jobs ();

      if (jerry_value_is_error (ret_value))
      {
        main_print_unhandled_exception (ret_value);
        continue;
      }

      jerry_release_value (ret_value);
    }
  }

  ret_value = jerry_run_all_enqueued_jobs ();

  if (jerry_value_is_error (ret_value))
  {
    main_print_unhandled_exception (ret_value);
    goto exit;
  }

  jerry_release_value (ret_value);

  if (arguments.exit_cb_name_p != NULL)
  {
    jerry_value_t global = jerry_get_global_object ();
    jerry_value_t name_str = jerry_create_string ((jerry_char_t *) arguments.exit_cb_name_p);
    jerry_value_t callback_fn = jerry_get_property (global, name_str);

    jerry_release_value (global);
    jerry_release_value (name_str);

    if (jerry_value_is_function (callback_fn))
    {
      ret_value = jerry_call_function (callback_fn, jerry_create_undefined (), NULL, 0);

      if (jerry_value_is_error (ret_value))
      {
        main_print_unhandled_exception (ret_value);
        goto exit;
      }

      jerry_release_value (ret_value);
    }

    jerry_release_value (callback_fn);
  }

  return_code = JERRY_STANDALONE_EXIT_CODE_OK;

exit:
  jerry_cleanup ();

#if defined(JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1)
  free (context_p);
#endif /* defined (JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1) */

  return return_code;
} /* main */

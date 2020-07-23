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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jerryscript.h"
#include "jerryscript-ext/debugger.h"
#include "jerryscript-ext/handler.h"
#include "jerryscript-port.h"
#include "jerryscript-port-default.h"

#include "cli.h"

/**
 * Maximum size of source code
 */
#define JERRY_BUFFER_SIZE (1048576)

/**
 * Maximum size of snapshots buffer
 */
#define JERRY_SNAPSHOT_BUFFER_SIZE (JERRY_BUFFER_SIZE / sizeof (uint32_t))

/**
 * Standalone Jerry exit codes
 */
#define JERRY_STANDALONE_EXIT_CODE_OK   (0)
#define JERRY_STANDALONE_EXIT_CODE_FAIL (1)

/**
 * Context size of the SYNTAX_ERROR
 */
#define SYNTAX_ERROR_CONTEXT_SIZE 2

static uint8_t buffer[ JERRY_BUFFER_SIZE ];

static const uint32_t *
read_file (const char *file_name,
           size_t *out_size_p)
{
  if (file_name == NULL)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: failed to open file, missing filename\n");
    return NULL;
  }

  FILE *file;
  if (!strcmp ("-", file_name))
  {
    file = stdin;
  }
  else
  {
    file = fopen (file_name, "rb");
    if (file == NULL)
    {
      jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: failed to open file: %s\n", file_name);
      return NULL;
    }
  }

  size_t bytes_read = fread (buffer, 1u, sizeof (buffer), file);
  if (!bytes_read)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: failed to read file: %s\n", file_name);
    fclose (file);
    return NULL;
  }

  fclose (file);

  *out_size_p = bytes_read;
  return (const uint32_t *) buffer;
} /* read_file */

/**
 * Print error value
 */
static void
print_unhandled_exception (jerry_value_t error_value) /**< error value */
{
  assert (!jerry_value_is_error (error_value));

  jerry_char_t err_str_buf[256];

  if (jerry_value_is_object (error_value))
  {
    jerry_value_t stack_str = jerry_create_string ((const jerry_char_t *) "stack");
    jerry_value_t backtrace_val = jerry_get_property (error_value, stack_str);
    jerry_release_value (stack_str);

    if (!jerry_value_is_error (backtrace_val)
        && jerry_value_is_array (backtrace_val))
    {
      printf ("Exception backtrace:\n");

      uint32_t length = jerry_get_array_length (backtrace_val);

      /* This length should be enough. */
      if (length > 32)
      {
        length = 32;
      }

      for (uint32_t i = 0; i < length; i++)
      {
        jerry_value_t item_val = jerry_get_property_by_index (backtrace_val, i);

        if (!jerry_value_is_error (item_val)
            && jerry_value_is_string (item_val))
        {
          jerry_size_t str_size = jerry_get_utf8_string_size (item_val);

          if (str_size >= 256)
          {
            printf ("%3u: [Backtrace string too long]\n", i);
          }
          else
          {
            jerry_size_t string_end = jerry_string_to_utf8_char_buffer (item_val, err_str_buf, str_size);
            assert (string_end == str_size);
            err_str_buf[string_end] = 0;

            printf ("%3u: %s\n", i, err_str_buf);
          }
        }

        jerry_release_value (item_val);
      }
    }
    jerry_release_value (backtrace_val);
  }

  jerry_value_t err_str_val = jerry_value_to_string (error_value);
  jerry_size_t err_str_size = jerry_get_utf8_string_size (err_str_val);

  if (err_str_size >= 256)
  {
    const char msg[] = "[Error message too long]";
    err_str_size = sizeof (msg) / sizeof (char) - 1;
    memcpy (err_str_buf, msg, err_str_size + 1);
  }
  else
  {
    jerry_size_t string_end = jerry_string_to_utf8_char_buffer (err_str_val, err_str_buf, err_str_size);
    assert (string_end == err_str_size);
    err_str_buf[string_end] = 0;

    if (jerry_is_feature_enabled (JERRY_FEATURE_ERROR_MESSAGES)
        && jerry_get_error_type (error_value) == JERRY_ERROR_SYNTAX)
    {
      jerry_char_t *string_end_p = err_str_buf + string_end;
      unsigned int err_line = 0;
      unsigned int err_col = 0;
      char *path_str_p = NULL;
      char *path_str_end_p = NULL;

      /* 1. parse column and line information */
      for (jerry_char_t *current_p = err_str_buf; current_p < string_end_p; current_p++)
      {
        if (*current_p == '[')
        {
          current_p++;

          if (*current_p == '<')
          {
            break;
          }

          path_str_p = (char *) current_p;
          while (current_p < string_end_p && *current_p != ':')
          {
            current_p++;
          }

          path_str_end_p = (char *) current_p++;

          err_line = (unsigned int) strtol ((char *) current_p, (char **) &current_p, 10);

          current_p++;

          err_col = (unsigned int) strtol ((char *) current_p, NULL, 10);
          break;
        }
      } /* for */

      if (err_line != 0 && err_col != 0)
      {
        unsigned int curr_line = 1;

        bool is_printing_context = false;
        unsigned int pos = 0;

        size_t source_size;

        /* Temporarily modify the error message, so we can use the path. */
        *path_str_end_p = '\0';

        read_file (path_str_p, &source_size);

        /* Revert the error message. */
        *path_str_end_p = ':';

        /* 2. seek and print */
        while ((pos < source_size) && (buffer[pos] != '\0'))
        {
          if (buffer[pos] == '\n')
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
            jerry_port_log (JERRY_LOG_LEVEL_ERROR, "%c", buffer[pos]);
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
    result_val = jerry_get_value_from_error (result_val, true);
    print_unhandled_exception (result_val);
  }

  jerry_release_value (result_val);
} /* register_js_function */

/**
 * Runs the source code received by jerry_debugger_wait_for_client_source.
 *
 * @return result fo the source code execution
 */
static jerry_value_t
wait_for_source_callback (const jerry_char_t *resource_name_p, /**< resource name */
                          size_t resource_name_size, /**< size of resource name */
                          const jerry_char_t *source_p, /**< source code */
                          size_t source_size, /**< source code size */
                          void *user_p) /**< user pointer */
{
  (void) user_p; /* unused */
  jerry_value_t ret_val = jerry_parse (resource_name_p,
                                       resource_name_size,
                                       source_p,
                                       source_size,
                                       JERRY_PARSE_NO_OPTS);

  if (!jerry_value_is_error (ret_val))
  {
    jerry_value_t func_val = ret_val;
    ret_val = jerry_run (func_val);
    jerry_release_value (func_val);
  }

  return ret_val;
} /* wait_for_source_callback */

/**
 * Command line option IDs
 */
typedef enum
{
  OPT_HELP,
  OPT_VERSION,
  OPT_MEM_STATS,
  OPT_PARSE_ONLY,
  OPT_SHOW_OP,
  OPT_SHOW_RE_OP,
  OPT_DEBUG_SERVER,
  OPT_DEBUG_PORT,
  OPT_DEBUG_CHANNEL,
  OPT_DEBUG_PROTOCOL,
  OPT_DEBUG_SERIAL_CONFIG,
  OPT_DEBUGGER_WAIT_SOURCE,
  OPT_EXEC_SNAP,
  OPT_EXEC_SNAP_FUNC,
  OPT_LOG_LEVEL,
  OPT_NO_PROMPT,
  OPT_CALL_ON_EXIT
} main_opt_id_t;

/**
 * Command line options
 */
static const cli_opt_t main_opts[] =
{
  CLI_OPT_DEF (.id = OPT_HELP, .opt = "h", .longopt = "help",
               .help = "print this help and exit"),
  CLI_OPT_DEF (.id = OPT_VERSION, .opt = "v", .longopt = "version",
               .help = "print tool and library version and exit"),
  CLI_OPT_DEF (.id = OPT_MEM_STATS, .longopt = "mem-stats",
               .help = "dump memory statistics"),
  CLI_OPT_DEF (.id = OPT_PARSE_ONLY, .longopt = "parse-only",
               .help = "don't execute JS input"),
  CLI_OPT_DEF (.id = OPT_SHOW_OP, .longopt = "show-opcodes",
               .help = "dump parser byte-code"),
  CLI_OPT_DEF (.id = OPT_SHOW_RE_OP, .longopt = "show-regexp-opcodes",
               .help = "dump regexp byte-code"),
  CLI_OPT_DEF (.id = OPT_DEBUG_SERVER, .longopt = "start-debug-server",
               .help = "start debug server and wait for a connecting client"),
  CLI_OPT_DEF (.id = OPT_DEBUG_PORT, .longopt = "debug-port", .meta = "NUM",
               .help = "debug server port (default: 5001)"),
  CLI_OPT_DEF (.id = OPT_DEBUG_CHANNEL, .longopt = "debug-channel", .meta = "[websocket|rawpacket]",
               .help = "Specify the debugger transmission channel (default: websocket)"),
  CLI_OPT_DEF (.id = OPT_DEBUG_PROTOCOL, .longopt = "debug-protocol", .meta = "PROTOCOL",
               .help = "Specify the transmission protocol over the communication channel (tcp|serial, default: tcp)"),
  CLI_OPT_DEF (.id = OPT_DEBUG_SERIAL_CONFIG, .longopt = "serial-config", .meta = "OPTIONS_STRING",
               .help = "Configure parameters for serial port (default: /dev/ttyS0,115200,8,N,1)"),
  CLI_OPT_DEF (.id = OPT_DEBUGGER_WAIT_SOURCE, .longopt = "debugger-wait-source",
               .help = "wait for an executable source from the client"),
  CLI_OPT_DEF (.id = OPT_EXEC_SNAP, .longopt = "exec-snapshot", .meta = "FILE",
               .help = "execute input snapshot file(s)"),
  CLI_OPT_DEF (.id = OPT_EXEC_SNAP_FUNC, .longopt = "exec-snapshot-func", .meta = "FILE NUM",
               .help = "execute specific function from input snapshot file(s)"),
  CLI_OPT_DEF (.id = OPT_LOG_LEVEL, .longopt = "log-level", .meta = "NUM",
               .help = "set log level (0-3)"),
  CLI_OPT_DEF (.id = OPT_NO_PROMPT, .longopt = "no-prompt",
               .help = "don't print prompt in REPL mode"),
  CLI_OPT_DEF (.id = OPT_CALL_ON_EXIT, .longopt = "call-on-exit", .meta = "STRING",
               .help = "invoke the specified function when the process is just about to exit"),
  CLI_OPT_DEF (.id = CLI_OPT_DEFAULT, .meta = "FILE",
               .help = "input JS file(s) (If file is -, read standard input.)")
};

/**
 * Check whether JerryScript has a requested feature enabled or not. If not,
 * print a warning message.
 *
 * @return the status of the feature.
 */
static bool
check_feature (jerry_feature_t feature, /**< feature to check */
               const char *option) /**< command line option that triggered this check */
{
  if (!jerry_is_feature_enabled (feature))
  {
    jerry_port_default_set_log_level (JERRY_LOG_LEVEL_WARNING);
    jerry_port_log (JERRY_LOG_LEVEL_WARNING, "Ignoring '%s' option because this feature is disabled!\n", option);
    return false;
  }
  return true;
} /* check_feature */

/**
 * Check whether a usage-related condition holds. If not, print an error
 * message, print the usage, and terminate the application.
 */
static void
check_usage (bool condition, /**< the condition that must hold */
             const char *name, /**< name of the application (argv[0]) */
             const char *msg, /**< error message to print if condition does not hold */
             const char *opt) /**< optional part of the error message */
{
  if (!condition)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "%s: %s%s\n", name, msg, opt != NULL ? opt : "");
    exit (JERRY_STANDALONE_EXIT_CODE_FAIL);
  }
} /* check_usage */

#if defined (JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1)

/**
 * The alloc function passed to jerry_create_context
 */
static void *
context_alloc (size_t size,
               void *cb_data_p)
{
  (void) cb_data_p; /* unused */
  return malloc (size);
} /* context_alloc */

#endif /* defined (JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1) */

/**
 * Inits the engine and the debugger
 */
static void
init_engine (jerry_init_flag_t flags, /**< initialized flags for the engine */
             char *debug_channel, /**< enable the debugger init or not */
             char *debug_protocol, /**< enable the debugger init or not */
             uint16_t debug_port, /**< the debugger port for tcp protocol */
             char *debug_serial_config) /**< configuration string for serial protocol */
{
  jerry_init (flags);
  if (strcmp (debug_channel, ""))
  {
    bool protocol = false;

    if (!strcmp (debug_protocol, "tcp"))
    {
      protocol = jerryx_debugger_tcp_create (debug_port);
    }
    else
    {
      assert (!strcmp (debug_protocol, "serial"));
      protocol = jerryx_debugger_serial_create (debug_serial_config);
    }

    if (!strcmp (debug_channel, "rawpacket"))
    {
      jerryx_debugger_after_connect (protocol && jerryx_debugger_rp_create ());
    }
    else
    {
      assert (!strcmp (debug_channel, "websocket"));
      jerryx_debugger_after_connect (protocol && jerryx_debugger_ws_create ());
    }
  }

  register_js_function ("assert", jerryx_handler_assert);
  register_js_function ("gc", jerryx_handler_gc);
  register_js_function ("print", jerryx_handler_print);
  register_js_function ("resourceName", jerryx_handler_resource_name);

  jerryx_register_assert_object ();
} /* init_engine */

int
main (int argc,
      char **argv)
{
  union
  {
    double d;
    unsigned u;
  } now = { .d = jerry_port_get_current_time () };
  srand (now.u);
  JERRY_VLA (const char *, file_names, argc);
  int files_counter = 0;

  jerry_init_flag_t flags = JERRY_INIT_EMPTY;

  JERRY_VLA (const char *, exec_snapshot_file_names, argc);
  JERRY_VLA (uint32_t, exec_snapshot_file_indices, argc);
  int exec_snapshots_count = 0;

  bool is_parse_only = false;

  bool start_debug_server = false;
  uint16_t debug_port = 5001;
  char *debug_channel = "websocket";
  char *debug_protocol = "tcp";
  char *debug_serial_config = "/dev/ttyS0,115200,8,N,1";

  bool is_repl_mode = false;
  bool is_wait_mode = false;
  bool no_prompt = false;

  const char *exit_cb = NULL;

  cli_state_t cli_state = cli_init (main_opts, argc - 1, argv + 1);
  for (int id = cli_consume_option (&cli_state); id != CLI_OPT_END; id = cli_consume_option (&cli_state))
  {
    switch (id)
    {
      case OPT_HELP:
      {
        cli_help (argv[0], NULL, main_opts);
        return JERRY_STANDALONE_EXIT_CODE_OK;
      }
      case OPT_VERSION:
      {
        printf ("Version: %d.%d.%d%s\n",
                JERRY_API_MAJOR_VERSION,
                JERRY_API_MINOR_VERSION,
                JERRY_API_PATCH_VERSION,
                JERRY_COMMIT_HASH);
        return JERRY_STANDALONE_EXIT_CODE_OK;
      }
      case OPT_MEM_STATS:
      {
        if (check_feature (JERRY_FEATURE_MEM_STATS, cli_state.arg))
        {
          jerry_port_default_set_log_level (JERRY_LOG_LEVEL_DEBUG);
          flags |= JERRY_INIT_MEM_STATS;
        }
        break;
      }
      case OPT_PARSE_ONLY:
      {
        is_parse_only = true;
        break;
      }
      case OPT_SHOW_OP:
      {
        if (check_feature (JERRY_FEATURE_PARSER_DUMP, cli_state.arg))
        {
          jerry_port_default_set_log_level (JERRY_LOG_LEVEL_DEBUG);
          flags |= JERRY_INIT_SHOW_OPCODES;
        }
        break;
      }
      case OPT_CALL_ON_EXIT:
      {
        exit_cb = cli_consume_string (&cli_state);
        break;
      }
      case OPT_SHOW_RE_OP:
      {
        if (check_feature (JERRY_FEATURE_REGEXP_DUMP, cli_state.arg))
        {
          jerry_port_default_set_log_level (JERRY_LOG_LEVEL_DEBUG);
          flags |= JERRY_INIT_SHOW_REGEXP_OPCODES;
        }
        break;
      }
      case OPT_DEBUG_SERVER:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          start_debug_server = true;
        }
        break;
      }
      case OPT_DEBUG_PORT:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          debug_port = (uint16_t) cli_consume_int (&cli_state);
        }
        break;
      }
      case OPT_DEBUG_CHANNEL:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          debug_channel = (char *) cli_consume_string (&cli_state);
          check_usage (!strcmp (debug_channel, "websocket") || !strcmp (debug_channel, "rawpacket"),
                       argv[0], "Error: invalid value for --debug-channel: ", cli_state.arg);
        }
        break;
      }
      case OPT_DEBUG_PROTOCOL:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          debug_protocol = (char *) cli_consume_string (&cli_state);
          check_usage (!strcmp (debug_protocol, "tcp") || !strcmp (debug_protocol, "serial"),
                       argv[0], "Error: invalid value for --debug-protocol: ", cli_state.arg);
        }
        break;
      }
      case OPT_DEBUG_SERIAL_CONFIG:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          debug_serial_config = (char *) cli_consume_string (&cli_state);
        }
        break;
      }
      case OPT_DEBUGGER_WAIT_SOURCE:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          is_wait_mode = true;
        }
        break;
      }
      case OPT_EXEC_SNAP:
      {
        if (check_feature (JERRY_FEATURE_SNAPSHOT_EXEC, cli_state.arg))
        {
          exec_snapshot_file_names[exec_snapshots_count] = cli_consume_string (&cli_state);
          exec_snapshot_file_indices[exec_snapshots_count++] = 0;
        }
        else
        {
          cli_consume_string (&cli_state);
        }
        break;
      }
      case OPT_EXEC_SNAP_FUNC:
      {
        if (check_feature (JERRY_FEATURE_SNAPSHOT_EXEC, cli_state.arg))
        {
          exec_snapshot_file_names[exec_snapshots_count] = cli_consume_string (&cli_state);
          exec_snapshot_file_indices[exec_snapshots_count++] = (uint32_t) cli_consume_int (&cli_state);
        }
        else
        {
          cli_consume_string (&cli_state);
        }
        break;
      }
      case OPT_LOG_LEVEL:
      {
        long int log_level = cli_consume_int (&cli_state);
        check_usage (log_level >= 0 && log_level <= 3,
                     argv[0], "Error: invalid value for --log-level: ", cli_state.arg);

        jerry_port_default_set_log_level ((jerry_log_level_t) log_level);
        break;
      }
      case OPT_NO_PROMPT:
      {
        no_prompt = true;
        break;
      }
      case CLI_OPT_DEFAULT:
      {
        file_names[files_counter++] = cli_consume_string (&cli_state);
        break;
      }
      default:
      {
        cli_state.error = "Internal error";
        break;
      }
    }
  }

  if (cli_state.error != NULL)
  {
    if (cli_state.arg != NULL)
    {
      jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: %s %s\n", cli_state.error, cli_state.arg);
    }
    else
    {
      jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: %s\n", cli_state.error);
    }

    return JERRY_STANDALONE_EXIT_CODE_FAIL;
  }

  if (files_counter == 0
      && exec_snapshots_count == 0)
  {
    is_repl_mode = true;
  }

#if defined (JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1)

  jerry_context_t *context_p = jerry_create_context (JERRY_GLOBAL_HEAP_SIZE * 1024, context_alloc, NULL);
  jerry_port_default_set_current_context (context_p);

#endif /* defined (JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1) */

  if (!start_debug_server)
  {
    debug_channel = "";
  }

  init_engine (flags, debug_channel, debug_protocol, debug_port, debug_serial_config);

  jerry_value_t ret_value = jerry_create_undefined ();

  if (jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_EXEC))
  {
    for (int i = 0; i < exec_snapshots_count; i++)
    {
      size_t snapshot_size;
      const uint32_t *snapshot_p = read_file (exec_snapshot_file_names[i], &snapshot_size);

      if (snapshot_p == NULL)
      {
        ret_value = jerry_create_error (JERRY_ERROR_COMMON, (jerry_char_t *) "Snapshot file load error");
      }
      else
      {
        ret_value = jerry_exec_snapshot (snapshot_p,
                                         snapshot_size,
                                         exec_snapshot_file_indices[i],
                                         JERRY_SNAPSHOT_EXEC_COPY_DATA);
      }

      if (jerry_value_is_error (ret_value))
      {
        break;
      }
    }
  }

  while (true)
  {

    if (!jerry_value_is_error (ret_value))
    {
      for (int i = 0; i < files_counter; i++)
      {
        size_t source_size;
        const jerry_char_t *source_p = (jerry_char_t *) read_file (file_names[i], &source_size);

        if (source_p == NULL)
        {
          ret_value = jerry_create_error (JERRY_ERROR_COMMON, (jerry_char_t *) "Source file load error");
          break;
        }

        if (!jerry_is_valid_utf8_string (source_p, (jerry_size_t) source_size))
        {
          ret_value = jerry_create_error (JERRY_ERROR_COMMON, (jerry_char_t *) ("Input must be a valid UTF-8 string."));
          break;
        }

        ret_value = jerry_parse ((jerry_char_t *) file_names[i],
                                 strlen (file_names[i]),
                                 source_p,
                                 source_size,
                                 JERRY_PARSE_NO_OPTS);

        if (!jerry_value_is_error (ret_value) && !is_parse_only)
        {
          jerry_value_t func_val = ret_value;
          ret_value = jerry_run (func_val);
          jerry_release_value (func_val);
        }

        if (jerry_value_is_error (ret_value))
        {
          break;
        }

        jerry_release_value (ret_value);
        ret_value = jerry_create_undefined ();
      }
    }

    if (is_wait_mode)
    {
      is_repl_mode = false;

      if (jerry_is_feature_enabled (JERRY_FEATURE_DEBUGGER))
      {
        while (true)
        {
          jerry_debugger_wait_for_source_status_t receive_status;

          do
          {
            jerry_value_t run_result;

            receive_status = jerry_debugger_wait_for_client_source (wait_for_source_callback,
                                                                    NULL,
                                                                    &run_result);

            if (receive_status == JERRY_DEBUGGER_SOURCE_RECEIVE_FAILED)
            {
              ret_value = jerry_create_error (JERRY_ERROR_COMMON,
                                              (jerry_char_t *) "Connection aborted before source arrived.");
            }

            if (receive_status == JERRY_DEBUGGER_SOURCE_END)
            {
              jerry_port_log (JERRY_LOG_LEVEL_DEBUG, "No more client source.\n");
            }

            if (jerry_value_is_abort (run_result))
            {
              ret_value = jerry_acquire_value (run_result);
            }

            jerry_release_value (run_result);
          }
          while (receive_status == JERRY_DEBUGGER_SOURCE_RECEIVED);

          if (receive_status != JERRY_DEBUGGER_CONTEXT_RESET_RECEIVED)
          {
            break;
          }

          init_engine (flags, debug_channel, debug_protocol, debug_port, debug_serial_config);

          ret_value = jerry_create_undefined ();
        }
      }

    }

    bool restart = false;

    if (jerry_is_feature_enabled (JERRY_FEATURE_DEBUGGER) && jerry_value_is_abort (ret_value))
    {
      jerry_value_t abort_value = jerry_get_value_from_error (ret_value, false);
      if (jerry_value_is_string (abort_value))
      {
        static const char restart_str[] = "r353t";

        jerry_value_t str_val = jerry_value_to_string (abort_value);
        jerry_size_t str_size = jerry_get_string_size (str_val);

        if (str_size == sizeof (restart_str) - 1)
        {
          JERRY_VLA (jerry_char_t, str_buf, str_size);
          jerry_string_to_char_buffer (str_val, str_buf, str_size);
          if (memcmp (restart_str, (char *) (str_buf), str_size) == 0)
          {
            jerry_release_value (ret_value);
            restart = true;
          }
        }

        jerry_release_value (str_val);
      }

      jerry_release_value (abort_value);
    }

    if (!restart)
    {
      break;
    }

    jerry_cleanup ();

    init_engine (flags, debug_channel, debug_protocol, debug_port, debug_serial_config);

    ret_value = jerry_create_undefined ();
  }

  if (is_repl_mode)
  {
    const char *prompt = !no_prompt ? "jerry> " : "";
    bool is_done = false;

    while (!is_done)
    {
      uint8_t *source_buffer_tail = buffer;
      size_t len = 0;

      printf ("%s", prompt);

      /* Read a line */
      while (true)
      {
        if (fread (source_buffer_tail, 1, 1, stdin) != 1)
        {
          is_done = true;
          break;
        }
        if (*source_buffer_tail == '\n')
        {
          break;
        }
        source_buffer_tail ++;
        len ++;
      }
      *source_buffer_tail = 0;

      if (len > 0)
      {
        if (!jerry_is_valid_utf8_string (buffer, (jerry_size_t) len))
        {
          jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: Input must be a valid UTF-8 string.\n");
          return JERRY_STANDALONE_EXIT_CODE_FAIL;
        }

        /* Evaluate the line */
        jerry_value_t ret_val = jerry_parse (NULL,
                                             0,
                                             buffer,
                                             len,
                                             JERRY_PARSE_NO_OPTS);

        if (!jerry_value_is_error (ret_val))
        {
          jerry_value_t func_val = ret_val;
          ret_val = jerry_run (func_val);
          jerry_release_value (func_val);
        }

        if (!jerry_value_is_error (ret_val))
        {
          /* Print return value */
          const jerry_value_t args[] = { ret_val };
          jerry_value_t ret_val_print = jerryx_handler_print (jerry_create_undefined (),
                                                              jerry_create_undefined (),
                                                              args,
                                                              1);
          jerry_release_value (ret_val_print);
          jerry_release_value (ret_val);
          ret_val = jerry_run_all_enqueued_jobs ();

          if (jerry_value_is_error (ret_val))
          {
            ret_val = jerry_get_value_from_error (ret_val, true);
            print_unhandled_exception (ret_val);
          }
        }
        else
        {
          ret_val = jerry_get_value_from_error (ret_val, true);
          print_unhandled_exception (ret_val);
        }

        jerry_release_value (ret_val);
      }
    }
  }

  int ret_code = JERRY_STANDALONE_EXIT_CODE_OK;

  if (jerry_value_is_error (ret_value))
  {
    ret_value = jerry_get_value_from_error (ret_value, true);
    print_unhandled_exception (ret_value);

    ret_code = JERRY_STANDALONE_EXIT_CODE_FAIL;
  }

  jerry_release_value (ret_value);

  ret_value = jerry_run_all_enqueued_jobs ();

  if (jerry_value_is_error (ret_value))
  {
    ret_value = jerry_get_value_from_error (ret_value, true);
    print_unhandled_exception (ret_value);
    ret_code = JERRY_STANDALONE_EXIT_CODE_FAIL;
  }

  jerry_release_value (ret_value);

  if (exit_cb != NULL)
  {
    jerry_value_t global = jerry_get_global_object ();
    jerry_value_t fn_str = jerry_create_string ((jerry_char_t *) exit_cb);
    jerry_value_t callback_fn = jerry_get_property (global, fn_str);

    jerry_release_value (global);
    jerry_release_value (fn_str);

    if (jerry_value_is_function (callback_fn))
    {
      jerry_value_t ret_val = jerry_call_function (callback_fn, jerry_create_undefined (), NULL, 0);

      if (jerry_value_is_error (ret_val))
      {
        ret_val = jerry_get_value_from_error (ret_val, true);
        print_unhandled_exception (ret_val);
        ret_code = JERRY_STANDALONE_EXIT_CODE_FAIL;
      }

      jerry_release_value (ret_val);
    }

    jerry_release_value (callback_fn);
  }

  jerry_cleanup ();
#if defined (JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1)
  free (context_p);
#endif /* defined (JERRY_EXTERNAL_CONTEXT) && (JERRY_EXTERNAL_CONTEXT == 1) */
  return ret_code;
} /* main */

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

#include "main-options.h"
#include "cli.h"
#include "jerryscript-port-default.h"
#include "jerryscript-port.h"
#include "main-utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Command line option IDs
 */
typedef enum
{
  OPT_HELP,
  OPT_VERSION,
  OPT_MEM_STATS,
  OPT_TEST262_OBJECT,
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
  OPT_MODULE,
  OPT_LOG_LEVEL,
  OPT_NO_PROMPT,
  OPT_CALL_ON_EXIT,
  OPT_USE_STDIN,
} main_opt_id_t;

/**
 * Command line options
 */
static const cli_opt_t main_opts[] = {
  CLI_OPT_DEF (.id = OPT_HELP, .opt = "h", .longopt = "help", .help = "print this help and exit"),
  CLI_OPT_DEF (.id = OPT_VERSION, .opt = "v", .longopt = "version", .help = "print tool and library version and exit"),
  CLI_OPT_DEF (.id = OPT_MEM_STATS, .longopt = "mem-stats", .help = "dump memory statistics"),
  CLI_OPT_DEF (.id = OPT_TEST262_OBJECT, .longopt = "test262-object", .help = "create test262 object"),
  CLI_OPT_DEF (.id = OPT_PARSE_ONLY, .longopt = "parse-only", .help = "don't execute JS input"),
  CLI_OPT_DEF (.id = OPT_SHOW_OP, .longopt = "show-opcodes", .help = "dump parser byte-code"),
  CLI_OPT_DEF (.id = OPT_SHOW_RE_OP, .longopt = "show-regexp-opcodes", .help = "dump regexp byte-code"),
  CLI_OPT_DEF (.id = OPT_DEBUG_SERVER,
               .longopt = "start-debug-server",
               .help = "start debug server and wait for a connecting client"),
  CLI_OPT_DEF (.id = OPT_DEBUG_PORT,
               .longopt = "debug-port",
               .meta = "NUM",
               .help = "debug server port (default: 5001)"),
  CLI_OPT_DEF (.id = OPT_DEBUG_CHANNEL,
               .longopt = "debug-channel",
               .meta = "[websocket|rawpacket]",
               .help = "Specify the debugger transmission channel (default: websocket)"),
  CLI_OPT_DEF (.id = OPT_DEBUG_PROTOCOL,
               .longopt = "debug-protocol",
               .meta = "PROTOCOL",
               .help = "Specify the transmission protocol over the communication channel (tcp|serial, default: tcp)"),
  CLI_OPT_DEF (.id = OPT_DEBUG_SERIAL_CONFIG,
               .longopt = "serial-config",
               .meta = "OPTIONS_STRING",
               .help = "Configure parameters for serial port (default: /dev/ttyS0,115200,8,N,1)"),
  CLI_OPT_DEF (.id = OPT_DEBUGGER_WAIT_SOURCE,
               .longopt = "debugger-wait-source",
               .help = "wait for an executable source from the client"),
  CLI_OPT_DEF (.id = OPT_EXEC_SNAP,
               .longopt = "exec-snapshot",
               .meta = "FILE",
               .help = "execute input snapshot file(s)"),
  CLI_OPT_DEF (.id = OPT_EXEC_SNAP_FUNC,
               .longopt = "exec-snapshot-func",
               .meta = "FILE NUM",
               .help = "execute specific function from input snapshot file(s)"),
  CLI_OPT_DEF (.id = OPT_MODULE, .opt = "m", .longopt = "module", .meta = "FILE", .help = "execute module file"),
  CLI_OPT_DEF (.id = OPT_LOG_LEVEL, .longopt = "log-level", .meta = "NUM", .help = "set log level (0-3)"),
  CLI_OPT_DEF (.id = OPT_NO_PROMPT, .longopt = "no-prompt", .help = "don't print prompt in REPL mode"),
  CLI_OPT_DEF (.id = OPT_CALL_ON_EXIT,
               .longopt = "call-on-exit",
               .meta = "STRING",
               .help = "invoke the specified function when the process is just about to exit"),
  CLI_OPT_DEF (.id = OPT_USE_STDIN, .opt = "", .help = "read from standard input"),
  CLI_OPT_DEF (.id = CLI_OPT_DEFAULT, .meta = "FILE", .help = "input JS file(s)")
};

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
 * parse input arguments
 */
void
main_parse_args (int argc, /**< argc */
                 char **argv, /**< argv */
                 main_args_t *arguments_p) /**< [in/out] arguments reference */
{
  arguments_p->source_count = 0;

  arguments_p->debug_channel = "websocket";
  arguments_p->debug_protocol = "tcp";
  arguments_p->debug_serial_config = "/dev/ttyS0,115200,8,N,1";
  arguments_p->debug_port = 5001;

  arguments_p->exit_cb_name_p = NULL;
  arguments_p->init_flags = JERRY_INIT_EMPTY;
  arguments_p->option_flags = OPT_FLAG_EMPTY;

  cli_state_t cli_state = cli_init (main_opts, argc, argv);
  for (int id = cli_consume_option (&cli_state); id != CLI_OPT_END; id = cli_consume_option (&cli_state))
  {
    switch (id)
    {
      case OPT_HELP:
      {
        cli_help (argv[0], NULL, main_opts);
        exit (JERRY_STANDALONE_EXIT_CODE_OK);

        break;
      }
      case OPT_VERSION:
      {
        printf ("Version: %d.%d.%d%s\n",
                JERRY_API_MAJOR_VERSION,
                JERRY_API_MINOR_VERSION,
                JERRY_API_PATCH_VERSION,
                JERRY_COMMIT_HASH);
        exit (JERRY_STANDALONE_EXIT_CODE_OK);

        break;
      }
      case OPT_MEM_STATS:
      {
        if (check_feature (JERRY_FEATURE_MEM_STATS, cli_state.arg))
        {
          jerry_port_default_set_log_level (JERRY_LOG_LEVEL_DEBUG);
          arguments_p->init_flags |= JERRY_INIT_MEM_STATS;
        }
        break;
      }
      case OPT_TEST262_OBJECT:
      {
        arguments_p->option_flags |= OPT_FLAG_TEST262_OBJECT;
        break;
      }
      case OPT_PARSE_ONLY:
      {
        arguments_p->option_flags |= OPT_FLAG_PARSE_ONLY;
        break;
      }
      case OPT_SHOW_OP:
      {
        if (check_feature (JERRY_FEATURE_PARSER_DUMP, cli_state.arg))
        {
          jerry_port_default_set_log_level (JERRY_LOG_LEVEL_DEBUG);
          arguments_p->init_flags |= JERRY_INIT_SHOW_OPCODES;
        }
        break;
      }
      case OPT_CALL_ON_EXIT:
      {
        arguments_p->exit_cb_name_p = cli_consume_string (&cli_state);
        break;
      }
      case OPT_SHOW_RE_OP:
      {
        if (check_feature (JERRY_FEATURE_REGEXP_DUMP, cli_state.arg))
        {
          jerry_port_default_set_log_level (JERRY_LOG_LEVEL_DEBUG);
          arguments_p->init_flags |= JERRY_INIT_SHOW_REGEXP_OPCODES;
        }
        break;
      }
      case OPT_DEBUG_SERVER:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          arguments_p->option_flags |= OPT_FLAG_DEBUG_SERVER;
        }
        break;
      }
      case OPT_DEBUG_PORT:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          arguments_p->debug_port = (uint16_t) cli_consume_int (&cli_state);
        }
        break;
      }
      case OPT_DEBUG_CHANNEL:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          const char *debug_channel = cli_consume_string (&cli_state);
          check_usage (!strcmp (debug_channel, "websocket") || !strcmp (debug_channel, "rawpacket"),
                       argv[0],
                       "Error: invalid value for --debug-channel: ",
                       cli_state.arg);

          arguments_p->debug_channel = debug_channel;
        }
        break;
      }
      case OPT_DEBUG_PROTOCOL:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          const char *debug_protocol = cli_consume_string (&cli_state);
          check_usage (!strcmp (debug_protocol, "tcp") || !strcmp (debug_protocol, "serial"),
                       argv[0],
                       "Error: invalid value for --debug-protocol: ",
                       cli_state.arg);

          arguments_p->debug_protocol = debug_protocol;
        }
        break;
      }
      case OPT_DEBUG_SERIAL_CONFIG:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          arguments_p->debug_serial_config = cli_consume_string (&cli_state);
        }
        break;
      }
      case OPT_DEBUGGER_WAIT_SOURCE:
      {
        if (check_feature (JERRY_FEATURE_DEBUGGER, cli_state.arg))
        {
          arguments_p->option_flags |= OPT_FLAG_WAIT_SOURCE;
        }
        break;
      }
      case OPT_EXEC_SNAP:
      {
        const bool is_enabled = check_feature (JERRY_FEATURE_SNAPSHOT_EXEC, cli_state.arg);
        const uint32_t path_index = cli_consume_path (&cli_state);

        if (is_enabled)
        {
          main_source_t *source_p = arguments_p->sources_p + arguments_p->source_count;
          arguments_p->source_count++;

          source_p->type = SOURCE_SNAPSHOT;
          source_p->path_index = path_index;
          source_p->snapshot_index = 0;
        }

        break;
      }
      case OPT_EXEC_SNAP_FUNC:
      {
        const bool is_enabled = check_feature (JERRY_FEATURE_SNAPSHOT_EXEC, cli_state.arg);
        const uint32_t path_index = cli_consume_path (&cli_state);
        const uint16_t snapshot_index = (uint16_t) cli_consume_int (&cli_state);

        if (is_enabled)
        {
          main_source_t *source_p = arguments_p->sources_p + arguments_p->source_count;
          arguments_p->source_count++;

          source_p->type = SOURCE_SNAPSHOT;
          source_p->path_index = path_index;
          source_p->snapshot_index = snapshot_index;
        }

        break;
      }
      case OPT_MODULE:
      {
        const uint32_t path_index = cli_consume_path (&cli_state);

        main_source_t *source_p = arguments_p->sources_p + arguments_p->source_count;
        arguments_p->source_count++;

        source_p->type = SOURCE_MODULE;
        source_p->path_index = path_index;
        source_p->snapshot_index = 0;

        break;
      }
      case OPT_LOG_LEVEL:
      {
        long int log_level = cli_consume_int (&cli_state);
        check_usage (log_level >= 0 && log_level <= 3,
                     argv[0],
                     "Error: invalid value for --log-level: ",
                     cli_state.arg);

        jerry_port_default_set_log_level ((jerry_log_level_t) log_level);
        break;
      }
      case OPT_NO_PROMPT:
      {
        arguments_p->option_flags |= OPT_FLAG_NO_PROMPT;
        break;
      }
      case OPT_USE_STDIN:
      {
        arguments_p->option_flags |= OPT_FLAG_USE_STDIN;
        break;
      }
      case CLI_OPT_DEFAULT:
      {
        main_source_t *source_p = arguments_p->sources_p + arguments_p->source_count;
        arguments_p->source_count++;

        source_p->type = SOURCE_SCRIPT;
        source_p->path_index = cli_consume_path (&cli_state);
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

    exit (JERRY_STANDALONE_EXIT_CODE_FAIL);
  }
} /* main_parse_args */

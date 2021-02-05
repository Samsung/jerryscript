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

#include "cli.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Fixed layout settings
 */

/**
 * Wrap lines at:
 */
#define CLI_LINE_LENGTH 80

/**
 * Indent various lines with:
 */
#define CLI_LINE_INDENT 2

/**
 * Tab stop (for multi-column display) at:
 */
#define CLI_LINE_TAB 24

/**
 * Declare a char VLA and concatenate a program name and a sub-command name
 * (separated by a single space) into the new array. Useful for printing command
 * line option usage summary for sub-commands.
 *
 * @param CMDNAME name of the new array variable.
 * @param PROGNAME string containing the name of the program.
 * @param CMD string continaing the name of the sub-command.
 */
#define CLI_CMD_NAME(CMDNAME, PROGNAME, CMD)              \
  char CMDNAME[strlen ((PROGNAME)) + strlen ((CMD)) + 2]; \
  strncpy (CMDNAME, (PROGNAME), strlen ((PROGNAME)));     \
  CMDNAME[strlen ((PROGNAME))] = ' ';                     \
  strncpy (CMDNAME + strlen ((PROGNAME)) + 1, (CMD), strlen ((CMD)) + 1)

/*
 * Command line option handling
 */

/**
 * Initialize a command line option processor.
 *
 * @return the state that should be passed to other cli_ functions.
 */
cli_state_t
cli_init (const cli_opt_t *options_p, /**< array of option definitions, terminated by CLI_OPT_DEFAULT */
          int argc, /**< number of command line arguments */
          char **argv) /**< array of command line arguments */
{
  return (cli_state_t){ .error = NULL, .arg = NULL, .index = 1, .argc = argc, .argv = argv, .opts = options_p };
} /* cli_init */

/**
 * Use another option list.
 */
void
cli_change_opts (cli_state_t *state_p, /**< state of the command line option processor */
                 const cli_opt_t *options_p) /**< array of option definitions, terminated by CLI_OPT_DEFAULT */
{
  state_p->opts = options_p;
} /* cli_change_opts */

/**
 * Checks whether the current argument is an option.
 *
 * Note:
 *   The state_p->error is not NULL on error and it contains the error message.
 *
 * @return the ID of the option that was found or a CLI_OPT_ constant otherwise.
 */
int
cli_consume_option (cli_state_t *state_p) /**< state of the command line option processor */
{
  if (state_p->error != NULL)
  {
    return CLI_OPT_END;
  }

  if (state_p->index >= state_p->argc)
  {
    state_p->arg = NULL;
    return CLI_OPT_END;
  }

  const char *arg = state_p->argv[state_p->index];

  state_p->arg = arg;

  if (arg[0] != '-')
  {
    return CLI_OPT_DEFAULT;
  }

  if (arg[1] == '-')
  {
    arg += 2;

    for (const cli_opt_t *opt = state_p->opts; opt->id != CLI_OPT_DEFAULT; opt++)
    {
      if (opt->longopt != NULL && strcmp (arg, opt->longopt) == 0)
      {
        state_p->index++;
        return opt->id;
      }
    }

    state_p->error = "Unknown long option";
    return CLI_OPT_END;
  }

  arg++;

  for (const cli_opt_t *opt = state_p->opts; opt->id != CLI_OPT_DEFAULT; opt++)
  {
    if (opt->opt != NULL && strcmp (arg, opt->opt) == 0)
    {
      state_p->index++;
      return opt->id;
    }
  }

  state_p->error = "Unknown option";
  return CLI_OPT_END;
} /* cli_consume_option */

/**
 * Returns the next argument as string.
 *
 * Note:
 *   The state_p->error is not NULL on error and it contains the error message.
 *
 * @return argument string
 */
const char *
cli_consume_string (cli_state_t *state_p) /**< state of the command line option processor */
{
  if (state_p->error != NULL)
  {
    return NULL;
  }

  if (state_p->index >= state_p->argc)
  {
    state_p->error = "Expected string argument";
    state_p->arg = NULL;
    return NULL;
  }

  state_p->arg = state_p->argv[state_p->index];

  state_p->index++;
  return state_p->arg;
} /* cli_consume_string */

/**
 * Returns the next argument as integer.
 *
 * Note:
 *   The state_p->error is not NULL on error and it contains the error message.
 *
 * @return argument integer
 */
int
cli_consume_int (cli_state_t *state_p) /**< state of the command line option processor */
{
  if (state_p->error != NULL)
  {
    return 0;
  }

  state_p->error = "Expected integer argument";

  if (state_p->index >= state_p->argc)
  {
    state_p->arg = NULL;
    return 0;
  }

  state_p->arg = state_p->argv[state_p->index];

  char *endptr;
  long int value = strtol (state_p->arg, &endptr, 10);

  if (*endptr != '\0')
  {
    return 0;
  }

  state_p->error = NULL;
  state_p->index++;
  return (int) value;
} /* cli_consume_int */

/**
 * Return next agument as path index.
 *
 * @return path index
 */
uint32_t
cli_consume_path (cli_state_t *state_p) /**< state of the command line option processor */
{
  if (state_p->error != NULL)
  {
    return 0;
  }

  uint32_t path_index = (uint32_t) state_p->index;
  cli_consume_string (state_p);

  return path_index;
} /* cli_consume_path */

/*
 * Print helper functions
 */

/**
 * Pad with spaces.
 */
static void
cli_print_pad (int cnt) /**< number of spaces to print */
{
  for (int i = 0; i < cnt; i++)
  {
    printf (" ");
  }
} /* cli_print_pad */

/**
 * Print the prefix of a string.
 */
static void
cli_print_prefix (const char *str, /**< string to print */
                  int len) /**< length of the prefix to print */
{
  for (int i = 0; i < len; i++)
  {
    printf ("%c", *str++);
  }
} /* cli_print_prefix */

/**
 * Print usage summary of options.
 */
static void
cli_opt_usage (const char *prog_name_p, /**< program name, typically argv[0] */
               const char *command_name_p, /**< command name if available */
               const cli_opt_t *opts_p) /**< array of command line option definitions, terminated by CLI_OPT_DEFAULT */
{
  int length = (int) strlen (prog_name_p);
  const cli_opt_t *current_opt_p = opts_p;

  printf ("%s", prog_name_p);

  if (command_name_p != NULL)
  {
    int command_length = (int) strlen (command_name_p);

    if (length + 1 + command_length > CLI_LINE_LENGTH)
    {
      length = CLI_LINE_INDENT - 1;
      printf ("\n");
      cli_print_pad (length);
    }

    printf (" %s", command_name_p);
  }

  while (current_opt_p->id != CLI_OPT_DEFAULT)
  {
    const char *opt_p = current_opt_p->opt;
    int opt_length = 2 + 1;

    if (opt_p == NULL)
    {
      opt_p = current_opt_p->longopt;
      opt_length++;
    }

    opt_length += (int) strlen (opt_p);

    if (length + 1 + opt_length >= CLI_LINE_LENGTH)
    {
      length = CLI_LINE_INDENT - 1;
      printf ("\n");
      cli_print_pad (length);
    }
    length += opt_length;

    printf (" [");

    if (current_opt_p->opt != NULL)
    {
      printf ("-%s", opt_p);
    }
    else
    {
      printf ("--%s", opt_p);
    }

    if (current_opt_p->meta != NULL)
    {
      printf (" %s", current_opt_p->meta);
    }

    printf ("]");

    current_opt_p++;
  }

  if (current_opt_p->meta != NULL)
  {
    const char *opt_p = current_opt_p->meta;
    int opt_length = (int) (2 + strlen (opt_p));

    if (length + 1 + opt_length >= CLI_LINE_LENGTH)
    {
      length = CLI_LINE_INDENT - 1;
      printf ("\n");
      cli_print_pad (length);
    }

    printf (" [%s]", opt_p);
  }

  printf ("\n\n");
} /* cli_opt_usage */

/**
 * Print a help message wrapped into the second column.
 */
static void
cli_print_help (const char *help) /**< the help message to print */
{
  while (help != NULL && *help != 0)
  {
    int length = -1;
    int i = 0;
    for (; i < CLI_LINE_LENGTH - CLI_LINE_TAB && help[i] != 0; i++)
    {
      if (help[i] == ' ')
      {
        length = i;
      }
    }
    if (length < 0 || i < CLI_LINE_LENGTH - CLI_LINE_TAB)
    {
      length = i;
    }

    cli_print_prefix (help, length);

    help += length;
    while (*help == ' ')
    {
      help++;
    }

    if (*help != 0)
    {
      printf ("\n");
      cli_print_pad (CLI_LINE_TAB);
    }
  }
} /* cli_print_help */

/**
 * Print detailed help for options.
 */
void
cli_help (const char *prog_name_p, /**< program name, typically argv[0] */
          const char *command_name_p, /**< command name if available */
          const cli_opt_t *options_p) /**< array of command line option definitions, terminated by CLI_OPT_DEFAULT */
{
  cli_opt_usage (prog_name_p, command_name_p, options_p);

  const cli_opt_t *opt_p = options_p;

  while (opt_p->id != CLI_OPT_DEFAULT)
  {
    int length = CLI_LINE_INDENT;
    cli_print_pad (CLI_LINE_INDENT);

    if (opt_p->opt != NULL)
    {
      printf ("-%s", opt_p->opt);
      length += (int) (strlen (opt_p->opt) + 1);
    }

    if (opt_p->opt != NULL && opt_p->longopt != NULL)
    {
      printf (", ");
      length += 2;
    }

    if (opt_p->longopt != NULL)
    {
      printf ("--%s", opt_p->longopt);
      length += (int) (strlen (opt_p->longopt) + 2);
    }

    if (opt_p->meta != NULL)
    {
      printf (" %s", opt_p->meta);
      length += 1 + (int) strlen (opt_p->meta);
    }

    if (opt_p->help != NULL)
    {
      if (length >= CLI_LINE_TAB)
      {
        printf ("\n");
        length = 0;
      }
      cli_print_pad (CLI_LINE_TAB - length);
      length = CLI_LINE_TAB;

      cli_print_help (opt_p->help);
    }

    printf ("\n");
    opt_p++;
  }

  if (opt_p->help != NULL)
  {
    int length = 0;

    if (opt_p->meta != NULL)
    {
      length = (int) (CLI_LINE_INDENT + strlen (opt_p->meta));

      cli_print_pad (CLI_LINE_INDENT);
      printf ("%s", opt_p->meta);
    }

    if (length >= CLI_LINE_TAB)
    {
      printf ("\n");
      length = 0;
    }

    cli_print_pad (CLI_LINE_TAB - length);

    cli_print_help (opt_p->help);
    printf ("\n");
  }
} /* cli_help */

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

#include "jerryscript-port.h"
#include "jerryscript.h"
#include <stdlib.h>
#include <string.h>

/**
 * Maximum size of source code / snapshots buffer
 */
#define JERRY_BUFFER_SIZE (1048576)

/**
 * Standalone Jerry exit codes
 */
#define JERRY_STANDALONE_EXIT_CODE_OK   (0)
#define JERRY_STANDALONE_EXIT_CODE_FAIL (1)

static uint8_t buffer[JERRY_BUFFER_SIZE];

static const uint8_t *
read_file (const char *file_name, size_t *out_size_p)
{
  FILE *file = fopen (file_name, "rb");
  if (file == NULL)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: failed to open file: %s\n", file_name);
    return NULL;
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
  return (const uint8_t *) buffer;
} /* read_file */

static void
print_help (char *name)
{
  printf ("Usage: %s [OPTION]... [FILE]...\n"
          "\n"
          "Options:\n"
          "  -h, --help\n"
          "\n",
          name);
} /* print_help */

/* Global "argc" and "argv" used to avoid argument passing via stack. */
static int argc;
static char **argv;

static JERRY_ATTR_NOINLINE int
run (void)
{
  jerry_init (JERRY_INIT_EMPTY);
  jerry_value_t ret_value = jerry_create_undefined ();

  for (int i = 1; i < argc; i++)
  {
    const char *file_name = argv[i];
    size_t source_size;

    const jerry_char_t *source_p = read_file (file_name, &source_size);

    if (source_p == NULL)
    {
      ret_value = jerry_create_error (JERRY_ERROR_COMMON, (jerry_char_t *) "");
      break;
    }
    else
    {
      ret_value = jerry_parse (NULL, 0, source_p, source_size, JERRY_PARSE_NO_OPTS);

      if (!jerry_value_is_error (ret_value))
      {
        jerry_value_t func_val = ret_value;
        ret_value = jerry_run (func_val);
        jerry_release_value (func_val);
      }
    }

    if (jerry_value_is_error (ret_value))
    {
      break;
    }

    jerry_release_value (ret_value);
    ret_value = jerry_create_undefined ();
  }

  int ret_code = !jerry_value_is_error (ret_value) ? JERRY_STANDALONE_EXIT_CODE_OK : JERRY_STANDALONE_EXIT_CODE_FAIL;

  jerry_release_value (ret_value);
  jerry_cleanup ();

  return ret_code;
} /* run */

#if defined(JERRY_TEST_STACK_MEASURE) && (JERRY_TEST_STACK_MEASURE)

/**
 * How this stack measuring works:
 *
 * 1) Get the current stack pointer before doing the test execution.
 *    This will be the "Stack bottom".
 * 2) Fill the stack towards lower addresses with a placeholder 32 bit value.
 *    A "STACK_MEASURE_RANGE" big area will be filled with the value starting
 *    from "Stack bottom".
 *    The "Stack bottom" + "STACK_MEASURE_RANGE" will be the "Stack top".
 * 3) Run the tests.
 * 4) Check the stack backwards from "Stack top" to see where the 32 bit placeholder
 *    value is not present. The point where the 32 bit value is not found is
 *    considered to be the "Stack max". The "Stack bottom" - "Stack max" substraction
 *    will give the stack usage in bytes.
 *
 *
 * Based on this the expected stack layout is:
 * The stack is expected to "grow" towards lower address.
 *
 * |-------|
 * |       |    <- low address - "Stack top"
 * |-------|
 * |       |
 * |-------|
 *    ....
 * |-------|
 * |       |    <- "Stack max"
 * |-------|
 *    ....
 * |-------|
 * |       |    <- high address - "Stack bottom"
 * |-------|
 *
 */

#if !(defined(__linux__) && __linux__)
#error "Unsupported stack measurement platform!"
#endif /* !(defined ( __linux__) && __linux__) */

#if defined(__x86_64__)
#define STACK_SAVE(TARGET)                          \
  {                                                 \
    __asm volatile("mov %%rsp, %0" : "=m"(TARGET)); \
  }
#elif defined(__i386__)
#define STACK_SAVE(TARGET)                          \
  {                                                 \
    __asm volatile("mov %%esp, %0" : "=m"(TARGET)); \
  }
#elif defined(__arm__)
#define STACK_SAVE(TARGET)                       \
  {                                              \
    __asm volatile("mov %0, sp" : "=r"(TARGET)); \
  }
#else /* !defined (__x86_64__) && !defined (__i386__) && !defined (__arm__) */
#error "Unsupported stack measurement target!"
#endif /* !defined (__x86_64__) && !defined (__i386__) && !defined (__arm__) */

static void *g_stack_bottom = 0x0;

#define STACK_MEASURE_RANGE ((2 * 1024 * 1024))
#define STACK_PATTERN       (0xDEADBEEF)
#define STACK_INIT(TARGET, SIZE)                                  \
  do                                                              \
  {                                                               \
    for (size_t idx = 0; idx < (SIZE / sizeof (uint32_t)); idx++) \
    {                                                             \
      ((uint32_t *) (TARGET))[idx] = STACK_PATTERN;               \
    }                                                             \
  } while (0)

#define STACK_USAGE(TARGET, SIZE)   stack_usage (TARGET, SIZE)
#define STACK_TOP_PTR(TARGET, SIZE) (uint32_t *) (((uint8_t *) TARGET) - SIZE)

static void
stack_usage (uint32_t *stack_top_p, size_t length_in_bytes)
{
  uint32_t *stack_bottom_p = stack_top_p + (length_in_bytes / sizeof (uint32_t));
  uint32_t *stack_p = stack_top_p;

  while (stack_p < stack_bottom_p)
  {
    if (*stack_p != STACK_PATTERN)
    {
      break;
    }
    stack_p++;
  }

  jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Used stack: %d\n", (int) ((uint8_t *) stack_bottom_p - (uint8_t *) stack_p));
} /* stack_usage */

#else /* (JERRY_TEST_STACK_MEASURE) && (JERRY_TEST_STACK_MEASURE) */
#define STACK_SAVE(TARGET)
#define STACK_INIT(TARGET, SIZE)
#define STACK_USAGE(TARGET, SIZE)
#endif /* #if defined (JERRY_TEST_STACK_MEASURE) && (JERRY_TEST_STACK_MEASURE) */

int
main (int main_argc, char **main_argv)
{
  union
  {
    double d;
    unsigned u;
  } now = { .d = jerry_port_get_current_time () };
  srand (now.u);

  argc = main_argc;
  argv = main_argv;

  if (argc <= 1 || (argc == 2 && (!strcmp ("-h", argv[1]) || !strcmp ("--help", argv[1]))))
  {
    print_help (argv[0]);
    return JERRY_STANDALONE_EXIT_CODE_OK;
  }

  STACK_SAVE (g_stack_bottom);
  STACK_INIT (STACK_TOP_PTR (g_stack_bottom, STACK_MEASURE_RANGE), STACK_MEASURE_RANGE);

  int result = run ();

  STACK_USAGE (STACK_TOP_PTR (g_stack_bottom, STACK_MEASURE_RANGE), STACK_MEASURE_RANGE);

  if (result == JERRY_STANDALONE_EXIT_CODE_FAIL)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Unhandled exception: Script Error!\n");
  }

  return result;
} /* main */

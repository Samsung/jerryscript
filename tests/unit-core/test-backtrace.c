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

#include "config.h"
#include "jerryscript.h"
#include "test-common.h"

static jerry_value_t
backtrace_handler (const jerry_value_t function_obj, /**< function object */
                   const jerry_value_t this_val, /**< this value */
                   const jerry_value_t args_p[], /**< argument list */
                   const jerry_length_t args_count) /**< argument count */
{
  JERRY_UNUSED (function_obj);
  JERRY_UNUSED (this_val);

  uint32_t max_depth = 0;

  if (args_count > 0 && jerry_value_is_number (args_p[0]))
  {
    max_depth = (uint32_t) jerry_get_number_value (args_p[0]);
  }

  return jerry_get_backtrace (max_depth);
} /* backtrace_handler */

static jerry_value_t
run (const char *resource_name_p, /**< resource name */
     const char *source_p) /**< source code */
{
  jerry_value_t code = jerry_parse ((const jerry_char_t *) resource_name_p,
                                    strlen (resource_name_p),
                                    (const jerry_char_t *) source_p,
                                    strlen (source_p),
                                    JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (!jerry_value_is_error (code));

  jerry_value_t result = jerry_run (code);
  jerry_release_value (code);

  return result;
} /* run */

static void
compare (jerry_value_t array, /**< array */
         uint32_t index, /**< item index */
         const char *str) /**< string to compare */
{
  jerry_char_t buf[64];

  size_t len = strlen (str);

  TEST_ASSERT (len < sizeof (buf));

  jerry_value_t value = jerry_get_property_by_index (array, index);

  TEST_ASSERT (!jerry_value_is_error (value)
               && jerry_value_is_string (value));

  TEST_ASSERT (jerry_get_string_size (value) == len);

  jerry_size_t str_len = jerry_string_to_char_buffer (value, buf, (jerry_size_t) len);
  TEST_ASSERT (str_len == len);

  jerry_release_value (value);

  TEST_ASSERT (memcmp (buf, str, len) == 0);
} /* compare */

static void
test_get_backtrace_api_call (void)
{
  jerry_init (JERRY_INIT_DEFAULT);

  jerry_value_t global = jerry_get_global_object ();

  jerry_value_t func = jerry_create_external_function (backtrace_handler);
  jerry_value_t name = jerry_create_string ((const jerry_char_t *) "backtrace");
  jerry_value_t result = jerry_set_property (global, name, func);
  TEST_ASSERT (!jerry_value_is_error (result));

  jerry_release_value (result);
  jerry_release_value (name);
  jerry_release_value (func);

  jerry_release_value (global);

  const char *source = ("function f() {\n"
                        "  return backtrace(0);\n"
                        "}\n"
                        "\n"
                        "function g() {\n"
                        "  return f();\n"
                        "}\n"
                        "\n"
                        "function h() {\n"
                        "  return g();\n"
                        "}\n"
                        "\n"
                        "h();\n");

  jerry_value_t backtrace = run ("something.js", source);

  TEST_ASSERT (!jerry_value_is_error (backtrace)
               && jerry_value_is_array (backtrace));

  TEST_ASSERT (jerry_get_array_length (backtrace) == 4);

  compare (backtrace, 0, "something.js:2");
  compare (backtrace, 1, "something.js:6");
  compare (backtrace, 2, "something.js:10");
  compare (backtrace, 3, "something.js:13");

  jerry_release_value (backtrace);

  /* Depth set to 2 this time. */

  source = ("function f() {\n"
            "  return backtrace(2);\n"
            "}\n"
            "\n"
            "function g() {\n"
            "  return f();\n"
            "}\n"
            "\n"
            "function h() {\n"
            "  return g();\n"
            "}\n"
            "\n"
            "h();\n");

  backtrace = run ("something_else.js", source);

  TEST_ASSERT (!jerry_value_is_error (backtrace)
               && jerry_value_is_array (backtrace));

  TEST_ASSERT (jerry_get_array_length (backtrace) == 2);

  compare (backtrace, 0, "something_else.js:2");
  compare (backtrace, 1, "something_else.js:6");

  jerry_release_value (backtrace);

  jerry_cleanup ();
} /* test_get_backtrace_api_call */

static void
test_exception_backtrace (void)
{
  jerry_init (JERRY_INIT_DEFAULT);

  const char *source = ("function f() {\n"
                        "  undef_reference;\n"
                        "}\n"
                        "\n"
                        "function g() {\n"
                        "  return f();\n"
                        "}\n"
                        "\n"
                        "g();\n");

  jerry_value_t error = run ("bad.js", source);

  TEST_ASSERT (jerry_value_is_error (error));

  error = jerry_get_value_from_error (error, true);

  TEST_ASSERT (jerry_value_is_object (error));

  jerry_value_t name = jerry_create_string ((const jerry_char_t *) "stack");
  jerry_value_t backtrace = jerry_get_property (error, name);

  jerry_release_value (name);
  jerry_release_value (error);

  TEST_ASSERT (!jerry_value_is_error (backtrace)
               && jerry_value_is_array (backtrace));

  TEST_ASSERT (jerry_get_array_length (backtrace) == 3);

  compare (backtrace, 0, "bad.js:2");
  compare (backtrace, 1, "bad.js:6");
  compare (backtrace, 2, "bad.js:9");

  jerry_release_value (backtrace);

  jerry_cleanup ();
} /* test_exception_backtrace */

static void
test_large_line_count (void)
{
  jerry_init (JERRY_INIT_DEFAULT);

  const char *source = ("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                        "g();\n");

  jerry_value_t error = run ("bad.js", source);

  TEST_ASSERT (jerry_value_is_error (error));

  error = jerry_get_value_from_error (error, true);

  TEST_ASSERT (jerry_value_is_object (error));

  jerry_value_t name = jerry_create_string ((const jerry_char_t *) "stack");
  jerry_value_t backtrace = jerry_get_property (error, name);

  jerry_release_value (name);
  jerry_release_value (error);

  TEST_ASSERT (!jerry_value_is_error (backtrace)
               && jerry_value_is_array (backtrace));

  TEST_ASSERT (jerry_get_array_length (backtrace) == 1);

  compare (backtrace, 0, "bad.js:385");

  jerry_release_value (backtrace);

  jerry_cleanup ();
} /* test_large_line_count */

int
main (void)
{
  TEST_INIT ();

  TEST_ASSERT (jerry_is_feature_enabled (JERRY_FEATURE_LINE_INFO));

  test_get_backtrace_api_call ();
  test_exception_backtrace ();
  test_large_line_count ();

  return 0;
} /* main */

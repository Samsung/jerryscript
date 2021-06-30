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
backtrace_handler (const jerry_call_info_t *call_info_p, /**< call information */
                   const jerry_value_t args_p[], /**< argument list */
                   const jerry_length_t args_count) /**< argument count */
{
  JERRY_UNUSED (call_info_p);

  uint32_t max_depth = 0;

  if (args_count >= 1 && jerry_value_is_number (args_p[0]))
  {
    max_depth = (uint32_t) jerry_get_number_value (args_p[0]);
  }

  return jerry_get_backtrace (max_depth);
} /* backtrace_handler */

static void
compare_string (jerry_value_t left_value, /* string value */
                const char *right_p) /* string to compare */
{
  jerry_char_t buffer[64];
  size_t length = strlen (right_p);

  TEST_ASSERT (length <= sizeof (buffer));
  TEST_ASSERT (jerry_value_is_string (left_value));
  TEST_ASSERT (jerry_get_string_size (left_value) == length);

  TEST_ASSERT (jerry_string_to_char_buffer (left_value, buffer, sizeof (buffer)) == length);
  TEST_ASSERT (memcmp (buffer, right_p, length) == 0);
} /* compare_string */

static const jerry_value_t *handler_args_p;
static int frame_index;

static bool
backtrace_callback (jerry_backtrace_frame_t *frame_p, /* frame information */
                    void *user_p) /* user data */
{
  TEST_ASSERT ((void *) handler_args_p == user_p);
  TEST_ASSERT (jerry_backtrace_get_frame_type (frame_p) == JERRY_BACKTRACE_FRAME_JS);

  const jerry_backtrace_location_t *location_p = jerry_backtrace_get_location (frame_p);
  const jerry_value_t *function_p = jerry_backtrace_get_function (frame_p);
  const jerry_value_t *this_p = jerry_backtrace_get_this (frame_p);

  TEST_ASSERT (location_p != NULL);
  TEST_ASSERT (function_p != NULL);
  TEST_ASSERT (this_p != NULL);

  compare_string (location_p->resource_name, "capture_test.js");

  ++frame_index;

  if (frame_index == 1)
  {
    TEST_ASSERT (!jerry_backtrace_is_strict (frame_p));
    TEST_ASSERT (location_p->line == 2);
    TEST_ASSERT (location_p->column == 3);
    TEST_ASSERT (handler_args_p[0] == *function_p);
    TEST_ASSERT (handler_args_p[1] == *this_p);
    return true;
  }

  if (frame_index == 2)
  {
    TEST_ASSERT (jerry_backtrace_is_strict (frame_p));
    TEST_ASSERT (location_p->line == 7);
    TEST_ASSERT (location_p->column == 6);
    TEST_ASSERT (handler_args_p[2] == *function_p);
    TEST_ASSERT (jerry_value_is_undefined (*this_p));
    return true;
  }

  jerry_value_t global = jerry_get_global_object ();

  TEST_ASSERT (frame_index == 3);
  TEST_ASSERT (!jerry_backtrace_is_strict (frame_p));
  TEST_ASSERT (location_p->line == 11);
  TEST_ASSERT (location_p->column == 3);
  TEST_ASSERT (handler_args_p[3] == *function_p);
  TEST_ASSERT (global == *this_p);

  jerry_release_value (global);
  return false;
} /* backtrace_callback */

static bool
async_backtrace_callback (jerry_backtrace_frame_t *frame_p, /* frame information */
                          void *user_p) /* user data */
{
  TEST_ASSERT ((void *) handler_args_p == user_p);
  TEST_ASSERT (jerry_backtrace_get_frame_type (frame_p) == JERRY_BACKTRACE_FRAME_JS);

  const jerry_backtrace_location_t *location_p = jerry_backtrace_get_location (frame_p);
  const jerry_value_t *function_p = jerry_backtrace_get_function (frame_p);

  TEST_ASSERT (location_p != NULL);
  TEST_ASSERT (function_p != NULL);

  compare_string (location_p->resource_name, "async_capture_test.js");

  ++frame_index;

  if (frame_index == 1)
  {
    TEST_ASSERT (jerry_backtrace_is_strict (frame_p));
    TEST_ASSERT (location_p->line == 3);
    TEST_ASSERT (location_p->column == 3);
    TEST_ASSERT (handler_args_p[0] == *function_p);
    return true;
  }

  TEST_ASSERT (frame_index == 2);
  TEST_ASSERT (!jerry_backtrace_is_strict (frame_p));
  TEST_ASSERT (location_p->line == 8);
  TEST_ASSERT (location_p->column == 3);
  TEST_ASSERT (handler_args_p[1] == *function_p);
  return true;
} /* async_backtrace_callback */

static bool
class_backtrace_callback (jerry_backtrace_frame_t *frame_p, /* frame information */
                          void *user_p) /* user data */
{
  TEST_ASSERT ((void *) handler_args_p == user_p);
  TEST_ASSERT (jerry_backtrace_get_frame_type (frame_p) == JERRY_BACKTRACE_FRAME_JS);

  const jerry_backtrace_location_t *location_p = jerry_backtrace_get_location (frame_p);
  const jerry_value_t *function_p = jerry_backtrace_get_function (frame_p);

  TEST_ASSERT (location_p != NULL);
  TEST_ASSERT (function_p != NULL);

  compare_string (location_p->resource_name, "class_capture_test.js");

  ++frame_index;

  if (frame_index == 1)
  {
    TEST_ASSERT (jerry_backtrace_is_strict (frame_p));
    TEST_ASSERT (location_p->line == 3);
    TEST_ASSERT (location_p->column == 12);
    return false;
  }

  TEST_ASSERT (frame_index == 2);
  TEST_ASSERT (jerry_backtrace_is_strict (frame_p));
  TEST_ASSERT (location_p->line == 2);
  TEST_ASSERT (location_p->column == 5);
  return false;
} /* class_backtrace_callback */

static jerry_value_t
capture_handler (const jerry_call_info_t *call_info_p, /**< call information */
                 const jerry_value_t args_p[], /**< argument list */
                 const jerry_length_t args_count) /**< argument count */
{
  JERRY_UNUSED (call_info_p);
  JERRY_UNUSED (args_p);
  JERRY_UNUSED (args_count);

  TEST_ASSERT (args_count == 0 || args_count == 2 || args_count == 4);
  TEST_ASSERT (args_count == 0 || frame_index == 0);

  jerry_backtrace_callback_t callback = backtrace_callback;

  if (args_count == 0)
  {
    callback = class_backtrace_callback;
  }
  else if (args_count == 2)
  {
    callback = async_backtrace_callback;
  }

  handler_args_p = args_p;
  jerry_backtrace_capture (callback, (void *) args_p);

  TEST_ASSERT (args_count == 0 || frame_index == (args_count == 4 ? 3 : 2));

  return jerry_create_undefined ();
} /* capture_handler */

static void
register_callback (jerry_external_handler_t handler_p, /**< callback function */
                   char *name_p) /**< name of the function */
{
  jerry_value_t global = jerry_get_global_object ();

  jerry_value_t func = jerry_create_external_function (handler_p);
  jerry_value_t name = jerry_create_string ((const jerry_char_t *) name_p);
  jerry_value_t result = jerry_set_property (global, name, func);
  TEST_ASSERT (!jerry_value_is_error (result));

  jerry_release_value (result);
  jerry_release_value (name);
  jerry_release_value (func);

  jerry_release_value (global);
} /* register_callback */

static jerry_value_t
run (const char *resource_name_p, /**< resource name */
     const char *source_p) /**< source code */
{
  jerry_parse_options_t parse_options;
  parse_options.options = JERRY_PARSE_HAS_RESOURCE;
  parse_options.resource_name_p = (const jerry_char_t *) resource_name_p;
  parse_options.resource_name_length = strlen (resource_name_p);

  jerry_value_t code = jerry_parse ((const jerry_char_t *) source_p,
                                    strlen (source_p),
                                    &parse_options);
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
  jerry_init (JERRY_INIT_EMPTY);

  register_callback (backtrace_handler, "backtrace");
  register_callback (capture_handler, "capture");

  const char *source_p = ("function f() {\n"
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

  jerry_value_t backtrace = run ("something.js", source_p);

  TEST_ASSERT (!jerry_value_is_error (backtrace)
               && jerry_value_is_array (backtrace));

  TEST_ASSERT (jerry_get_array_length (backtrace) == 4);

  compare (backtrace, 0, "something.js:2:3");
  compare (backtrace, 1, "something.js:6:3");
  compare (backtrace, 2, "something.js:10:3");
  compare (backtrace, 3, "something.js:13:1");

  jerry_release_value (backtrace);

  /* Depth set to 2 this time. */

  source_p = ("function f() {\n"
              "  1; return backtrace(2);\n"
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

  backtrace = run ("something_else.js", source_p);

  TEST_ASSERT (!jerry_value_is_error (backtrace)
               && jerry_value_is_array (backtrace));

  TEST_ASSERT (jerry_get_array_length (backtrace) == 2);

  compare (backtrace, 0, "something_else.js:2:6");
  compare (backtrace, 1, "something_else.js:6:3");

  jerry_release_value (backtrace);

  /* Test frame capturing. */

  frame_index = 0;
  source_p = ("var o = { f:function() {\n"
              "  return capture(o.f, o, g, h);\n"
              "} }\n"
              "\n"
              "function g() {\n"
              "  'use strict';\n"
              "  1; return o.f();\n"
              "}\n"
              "\n"
              "function h() {\n"
              "  return g();\n"
              "}\n"
              "\n"
              "h();\n");

  jerry_value_t result = run ("capture_test.js", source_p);

  TEST_ASSERT (jerry_value_is_undefined (result));
  jerry_release_value (result);

  TEST_ASSERT (frame_index == 3);

  /* Test async frame capturing. */
  source_p = "async function f() {}";
  result = jerry_eval ((const jerry_char_t *) source_p, strlen (source_p), JERRY_PARSE_NO_OPTS);

  if (!jerry_value_is_error (result))
  {
    jerry_release_value (result);

    frame_index = 0;
    source_p = ("function f() {\n"
                "  'use strict';\n"
                "  return capture(f, g);\n"
                "}\n"
                "\n"
                "async function g() {\n"
                "  await 0;\n"
                "  return f();\n"
                "}\n"
                "\n"
                "g();\n");

    result = run ("async_capture_test.js", source_p);

    TEST_ASSERT (jerry_value_is_promise (result));
    jerry_release_value (result);

    TEST_ASSERT (frame_index == 0);

    result = jerry_run_all_enqueued_jobs ();
    TEST_ASSERT (!jerry_value_is_error (result));

    TEST_ASSERT (frame_index == 2);
  }
  else
  {
    TEST_ASSERT (jerry_get_error_type (result) == JERRY_ERROR_SYNTAX);
  }

  jerry_release_value (result);

  /* Test class initializer frame capturing. */
  source_p = "class C {}";
  result = jerry_eval ((const jerry_char_t *) source_p, strlen (source_p), JERRY_PARSE_NO_OPTS);

  if (!jerry_value_is_error (result))
  {
    jerry_release_value (result);

    frame_index = 0;
    source_p = ("class C {\n"
                "  a = capture();\n"
                "  static b = capture();\n"
                "}\n"
                "new C;\n");

    result = run ("class_capture_test.js", source_p);

    TEST_ASSERT (!jerry_value_is_error (result));
    TEST_ASSERT (frame_index == 2);
  }
  else
  {
    TEST_ASSERT (jerry_get_error_type (result) == JERRY_ERROR_SYNTAX);
  }

  jerry_release_value (result);

  jerry_cleanup ();
} /* test_get_backtrace_api_call */

static void
test_exception_backtrace (void)
{
  jerry_init (JERRY_INIT_EMPTY);

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

  compare (backtrace, 0, "bad.js:2:3");
  compare (backtrace, 1, "bad.js:6:3");
  compare (backtrace, 2, "bad.js:9:1");

  jerry_release_value (backtrace);

  jerry_cleanup ();
} /* test_exception_backtrace */

static void
test_large_line_count (void)
{
  jerry_init (JERRY_INIT_EMPTY);

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

  compare (backtrace, 0, "bad.js:385:1");

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

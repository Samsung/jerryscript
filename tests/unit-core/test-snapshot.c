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

/**
 * Maximum size of snapshots buffer
 */
#define SNAPSHOT_BUFFER_SIZE (256)

/**
 * Magic strings
 */
static const jerry_char_t *magic_strings[] =
{
  (const jerry_char_t *) " ",
  (const jerry_char_t *) "a",
  (const jerry_char_t *) "b",
  (const jerry_char_t *) "c",
  (const jerry_char_t *) "from",
  (const jerry_char_t *) "func",
  (const jerry_char_t *) "string",
  (const jerry_char_t *) "snapshot"
};

/**
 * Magic string lengths
 */
static const jerry_length_t magic_string_lengths[] =
{
  1, 1, 1, 1, 4, 4, 6, 8
};

static void test_function_snapshot (void)
{
  /* function to snapshot */
  if (!jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_SAVE)
      || !jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_EXEC))
  {
    return;
  }

  const jerry_init_flag_t flags = JERRY_INIT_EMPTY;
  static uint32_t function_snapshot_buffer[SNAPSHOT_BUFFER_SIZE];

  const char *args_p = "a, b";
  const char *code_to_snapshot_p = "return a + b";

  jerry_init (JERRY_INIT_FLAGS (flags));
  jerry_value_t generate_result;
  generate_result = jerry_generate_function_snapshot (NULL,
                                                      0,
                                                      (const jerry_char_t *) code_to_snapshot_p,
                                                      strlen (code_to_snapshot_p),
                                                      (jerry_char_t *) args_p,
                                                      strlen (args_p),
                                                      0,
                                                      function_snapshot_buffer,
                                                      SNAPSHOT_BUFFER_SIZE);
  TEST_ASSERT (!jerry_value_is_error (generate_result)
               && jerry_value_is_number (generate_result));

  size_t function_snapshot_size = (size_t) jerry_get_number_value (generate_result);
  jerry_release_value (generate_result);

  jerry_cleanup ();

  jerry_init (JERRY_INIT_FLAGS (flags));

  jerry_value_t function_obj = jerry_load_function_snapshot (function_snapshot_buffer,
                                                             function_snapshot_size,
                                                             0,
                                                             0);

  TEST_ASSERT (!jerry_value_is_error (function_obj));
  TEST_ASSERT (jerry_value_is_function (function_obj));

  jerry_value_t this_val = jerry_create_undefined ();
  jerry_value_t args[2];
  args[0] = jerry_create_number (1.0);
  args[1] = jerry_create_number (2.0);

  jerry_value_t res = jerry_call_function (function_obj, this_val, args, 2);

  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_number (res));
  double num = jerry_get_number_value (res);
  TEST_ASSERT (num == 3);

  jerry_release_value (args[0]);
  jerry_release_value (args[1]);
  jerry_release_value (res);
  jerry_release_value (function_obj);

  jerry_cleanup ();
} /* test_function_snapshot */

static void arguments_test_exec_snapshot (uint32_t *snapshot_p, size_t snapshot_size, uint32_t exec_snapshot_flags)
{
  jerry_init (JERRY_INIT_DEFAULT);
  jerry_value_t res = jerry_exec_snapshot (snapshot_p, snapshot_size, 0, exec_snapshot_flags);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_number (res));
  double raw_value = jerry_get_number_value (res);
  TEST_ASSERT (raw_value == 15);
  jerry_release_value (res);

  jerry_cleanup ();
} /* arguments_test_exec_snapshot */

static void test_function_arguments_snapshot (void)
{
  if (jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_SAVE)
      && jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_EXEC))
  {
    static uint32_t arguments_snapshot_buffer[SNAPSHOT_BUFFER_SIZE];

    const char *code_to_snapshot_p = ("function f(a,b,c) {"
                                      "  arguments[0]++;"
                                      "  arguments[1]++;"
                                      "  arguments[2]++;"
                                      "  return a + b + c;"
                                      "}"
                                      "f(3,4,5);");
    jerry_init (JERRY_INIT_DEFAULT);

    jerry_value_t generate_result;
    generate_result = jerry_generate_snapshot (NULL,
                                               0,
                                               (jerry_char_t *) code_to_snapshot_p,
                                               strlen (code_to_snapshot_p),
                                               0,
                                               arguments_snapshot_buffer,
                                               SNAPSHOT_BUFFER_SIZE);

    TEST_ASSERT (!jerry_value_is_error (generate_result)
                 && jerry_value_is_number (generate_result));

    size_t snapshot_size = (size_t) jerry_get_number_value (generate_result);
    jerry_release_value (generate_result);

    jerry_cleanup ();

    arguments_test_exec_snapshot (arguments_snapshot_buffer, snapshot_size, 0);
    arguments_test_exec_snapshot (arguments_snapshot_buffer, snapshot_size, JERRY_SNAPSHOT_EXEC_COPY_DATA);
  }
} /* test_function_arguments_snapshot */

static void test_exec_snapshot (uint32_t *snapshot_p, size_t snapshot_size, uint32_t exec_snapshot_flags)
{
  char string_data[32];

  jerry_init (JERRY_INIT_DEFAULT);

  jerry_register_magic_strings (magic_strings,
                                sizeof (magic_string_lengths) / sizeof (jerry_length_t),
                                magic_string_lengths);

  jerry_value_t res = jerry_exec_snapshot (snapshot_p, snapshot_size, 0, exec_snapshot_flags);

  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_string (res));
  jerry_size_t sz = jerry_get_string_size (res);
  TEST_ASSERT (sz == 20);
  sz = jerry_string_to_char_buffer (res, (jerry_char_t *) string_data, sz);
  TEST_ASSERT (sz == 20);
  jerry_release_value (res);
  TEST_ASSERT (!strncmp (string_data, "string from snapshot", (size_t) sz));

  jerry_cleanup ();
} /* test_exec_snapshot */

int
main (void)
{
  static uint32_t snapshot_buffer[SNAPSHOT_BUFFER_SIZE];

  TEST_INIT ();

  /* Dump / execute snapshot */
  if (jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_SAVE)
      && jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_EXEC))
  {
    const char *code_to_snapshot_p = "(function () { return 'string from snapshot'; }) ();";

    jerry_init (JERRY_INIT_DEFAULT);
    jerry_value_t generate_result;
    generate_result = jerry_generate_snapshot (NULL,
                                               0,
                                               (const jerry_char_t *) code_to_snapshot_p,
                                               strlen (code_to_snapshot_p),
                                               0,
                                               snapshot_buffer,
                                               SNAPSHOT_BUFFER_SIZE);
    TEST_ASSERT (!jerry_value_is_error (generate_result)
                 && jerry_value_is_number (generate_result));

    size_t snapshot_size = (size_t) jerry_get_number_value (generate_result);
    jerry_release_value (generate_result);

    /* Check the snapshot data. Unused bytes should be filled with zeroes */
    const uint8_t expected_data[] =
    {
      0x4A, 0x52, 0x52, 0x59, 0x11, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
      0x03, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x00, 0x00, 0x00, 0x01, 0x18, 0x00, 0x00, 0x00,
      0x28, 0x00, 0xB8, 0x46, 0x00, 0x00, 0x00, 0x00,
      0x03, 0x00, 0x01, 0x00, 0x21, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x01, 0x01, 0x07, 0x00, 0x00, 0x00,
      0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x14, 0x00, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67,
      0x20, 0x66, 0x72, 0x6F, 0x6D, 0x20, 0x73, 0x6E,
      0x61, 0x70, 0x73, 0x68, 0x6F, 0x74
    };
    TEST_ASSERT (sizeof (expected_data) == snapshot_size);
    TEST_ASSERT (0 == memcmp (expected_data, snapshot_buffer, sizeof (expected_data)));

    jerry_cleanup ();

    test_exec_snapshot (snapshot_buffer, snapshot_size, 0);
    test_exec_snapshot (snapshot_buffer, snapshot_size, JERRY_SNAPSHOT_EXEC_COPY_DATA);
  }

  /* Static snapshot */
  if (jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_SAVE)
      && jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_EXEC))
  {
    const char *code_to_snapshot_p = ("function func(a, b, c) {"
                                      "  c = 'snapshot';"
                                      "  return arguments[0] + ' ' + b + ' ' + arguments[2];"
                                      "};"
                                      "func('string', 'from');");

    jerry_init (JERRY_INIT_DEFAULT);
    jerry_register_magic_strings (magic_strings,
                                  sizeof (magic_string_lengths) / sizeof (jerry_length_t),
                                  magic_string_lengths);

    jerry_value_t generate_result;
    generate_result = jerry_generate_snapshot (NULL,
                                               0,
                                               (jerry_char_t *) code_to_snapshot_p,
                                               strlen (code_to_snapshot_p),
                                               JERRY_SNAPSHOT_SAVE_STATIC,
                                               snapshot_buffer,
                                               SNAPSHOT_BUFFER_SIZE);
    TEST_ASSERT (!jerry_value_is_error (generate_result)
                 && jerry_value_is_number (generate_result));

    size_t snapshot_size = (size_t) jerry_get_number_value (generate_result);
    jerry_release_value (generate_result);

    /* Static snapshots are not supported by default. */
    jerry_value_t exec_result = jerry_exec_snapshot (snapshot_buffer, snapshot_size, 0, 0);
    TEST_ASSERT (jerry_value_is_error (exec_result));
    jerry_release_value (exec_result);

    jerry_cleanup ();

    test_exec_snapshot (snapshot_buffer, snapshot_size, JERRY_SNAPSHOT_EXEC_ALLOW_STATIC);
  }

  /* Merge snapshot */
  if (jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_SAVE)
      && jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_EXEC))
  {
    static uint32_t snapshot_buffer_0[SNAPSHOT_BUFFER_SIZE];
    static uint32_t snapshot_buffer_1[SNAPSHOT_BUFFER_SIZE];
    size_t snapshot_sizes[2];
    static uint32_t merged_snapshot_buffer[SNAPSHOT_BUFFER_SIZE];

    const char *code_to_snapshot_p = "var a = 'hello'; 123";

    jerry_init (JERRY_INIT_DEFAULT);
    jerry_value_t generate_result;
    generate_result = jerry_generate_snapshot (NULL,
                                               0,
                                               (const jerry_char_t *) code_to_snapshot_p,
                                               strlen (code_to_snapshot_p),
                                               0,
                                               snapshot_buffer_0,
                                               SNAPSHOT_BUFFER_SIZE);
    TEST_ASSERT (!jerry_value_is_error (generate_result)
                 && jerry_value_is_number (generate_result));

    snapshot_sizes[0] = (size_t) jerry_get_number_value (generate_result);
    jerry_release_value (generate_result);

    jerry_cleanup ();

    code_to_snapshot_p = "var b = 'hello'; 456";

    jerry_init (JERRY_INIT_DEFAULT);
    generate_result = jerry_generate_snapshot (NULL,
                                               0,
                                               (const jerry_char_t *) code_to_snapshot_p,
                                               strlen (code_to_snapshot_p),
                                               0,
                                               snapshot_buffer_1,
                                               SNAPSHOT_BUFFER_SIZE);
    TEST_ASSERT (!jerry_value_is_error (generate_result)
                 && jerry_value_is_number (generate_result));

    snapshot_sizes[1] = (size_t) jerry_get_number_value (generate_result);
    jerry_release_value (generate_result);

    jerry_cleanup ();

    jerry_init (JERRY_INIT_DEFAULT);

    const char *error_p;
    const uint32_t *snapshot_buffers[2];

    snapshot_buffers[0] = snapshot_buffer_0;
    snapshot_buffers[1] = snapshot_buffer_1;

    static uint32_t snapshot_buffer_0_bck[SNAPSHOT_BUFFER_SIZE];
    static uint32_t snapshot_buffer_1_bck[SNAPSHOT_BUFFER_SIZE];

    memcpy (snapshot_buffer_0_bck, snapshot_buffer_0, SNAPSHOT_BUFFER_SIZE);
    memcpy (snapshot_buffer_1_bck, snapshot_buffer_1, SNAPSHOT_BUFFER_SIZE);

    size_t merged_size = jerry_merge_snapshots (snapshot_buffers,
                                                snapshot_sizes,
                                                2,
                                                merged_snapshot_buffer,
                                                SNAPSHOT_BUFFER_SIZE,
                                                &error_p);

    jerry_cleanup ();

    TEST_ASSERT (0 == memcmp (snapshot_buffer_0_bck, snapshot_buffer_0, SNAPSHOT_BUFFER_SIZE));
    TEST_ASSERT (0 == memcmp (snapshot_buffer_1_bck, snapshot_buffer_1, SNAPSHOT_BUFFER_SIZE));

    jerry_init (JERRY_INIT_DEFAULT);

    jerry_value_t res = jerry_exec_snapshot (merged_snapshot_buffer, merged_size, 0, 0);
    TEST_ASSERT (!jerry_value_is_error (res));
    TEST_ASSERT (jerry_get_number_value (res) == 123);
    jerry_release_value (res);

    res = jerry_exec_snapshot (merged_snapshot_buffer, merged_size, 1, 0);
    TEST_ASSERT (!jerry_value_is_error (res));
    TEST_ASSERT (jerry_get_number_value (res) == 456);
    jerry_release_value (res);

    jerry_cleanup ();
  }

  /* Save literals */
  if (jerry_is_feature_enabled (JERRY_FEATURE_SNAPSHOT_SAVE))
  {
    /* C format generation */
    jerry_init (JERRY_INIT_DEFAULT);

    static uint32_t literal_buffer_c[SNAPSHOT_BUFFER_SIZE];
    static const char *code_for_c_format_p = "var object = { aa:'fo o', Bb:'max', aaa:'xzy0' };";

    size_t literal_sizes_c_format = jerry_parse_and_save_literals ((jerry_char_t *) code_for_c_format_p,
                                                                   strlen (code_for_c_format_p),
                                                                   false,
                                                                   literal_buffer_c,
                                                                   SNAPSHOT_BUFFER_SIZE,
                                                                   true);
    TEST_ASSERT (literal_sizes_c_format == 200);

    static const char *expected_c_format = (
                                            "jerry_length_t literal_count = 4;\n\n"
                                            "jerry_char_t *literals[4] =\n"
                                            "{\n"
                                            "  \"Bb\",\n"
                                            "  \"aa\",\n"
                                            "  \"aaa\",\n"
                                            "  \"xzy0\"\n"
                                            "};\n\n"
                                            "jerry_length_t literal_sizes[4] =\n"
                                            "{\n"
                                            "  2 /* Bb */,\n"
                                            "  2 /* aa */,\n"
                                            "  3 /* aaa */,\n"
                                            "  4 /* xzy0 */\n"
                                            "};\n"
                                            );

    TEST_ASSERT (!strncmp ((char *) literal_buffer_c, expected_c_format, literal_sizes_c_format));
    jerry_cleanup ();

    /* List format generation */
    jerry_init (JERRY_INIT_DEFAULT);

    static uint32_t literal_buffer_list[SNAPSHOT_BUFFER_SIZE];
    static const char *code_for_list_format_p = "var obj = { a:'aa', bb:'Bb' };";

    size_t literal_sizes_list_format = jerry_parse_and_save_literals ((jerry_char_t *) code_for_list_format_p,
                                                                      strlen (code_for_list_format_p),
                                                                      false,
                                                                      literal_buffer_list,
                                                                      SNAPSHOT_BUFFER_SIZE,
                                                                      false);

    TEST_ASSERT (literal_sizes_list_format == 25);
    TEST_ASSERT (!strncmp ((char *) literal_buffer_list, "1 a\n2 Bb\n2 aa\n2 bb\n3 obj\n", literal_sizes_list_format));

    jerry_cleanup ();
  }

  test_function_snapshot ();

  test_function_arguments_snapshot ();

  return 0;
} /* main */

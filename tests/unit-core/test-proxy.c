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

/** Test in Proxy on C side. Equivalent test code in JS:

var demo = 0.0;

var target = {};
var handler = {
    get: function (target, name, recv) {
        assert (typeof (target) === 'object');
        assert (name === 'value');
        assert (typeof (recv) === 'object');
        return demo++;
    }

    set: function (target, name, value, recv) {
        assert (typeof (target) === 'object');
        assert (name === 'value');
        assert (typeof (recv) === 'object');
        demo = 55;
        return demo;
    }
};

var pdemo = new Proxy(target, handler);

assert (pdemo.value === 1.0);
assert (pdemo.value === 1.0);
assert (pdemo.value === 2.0);

pdemo.value = 55;

assert (pdemo.value === 56);

pdemo.value = 12;

assert (pdemo.value === 13);
 */

static int demo_value = 0;

static jerry_value_t
handler_get (const jerry_call_info_t *call_info_p, /**< call information */
             const jerry_value_t args_p[], /**< function arguments */
             const jerry_length_t args_count) /**< number of function arguments */
{
  JERRY_UNUSED (call_info_p);

  TEST_ASSERT (args_count == 3);
  TEST_ASSERT (jerry_value_is_object (args_p[0])); /* target */
  TEST_ASSERT (jerry_value_is_string (args_p[1])); /* P */
  TEST_ASSERT (jerry_value_is_object (args_p[2])); /* receiver */

  const char expected[] = "value";
  char buffer[10];
  jerry_size_t copied = jerry_string_to_char_buffer (args_p[1], (jerry_char_t *) buffer, 10);

  TEST_ASSERT (copied == 5);
  TEST_ASSERT (strncmp (expected, buffer, 5) == 0);

  demo_value++;

  return jerry_create_number (demo_value);
} /* handler_get */

static jerry_value_t
handler_set (const jerry_call_info_t *call_info_p, /**< call information */
             const jerry_value_t args_p[], /**< function arguments */
             const jerry_length_t args_count) /**< number of function arguments */
{
  JERRY_UNUSED (call_info_p);
  JERRY_UNUSED (args_p);
  JERRY_UNUSED (args_count);

  TEST_ASSERT (args_count == 4);
  TEST_ASSERT (jerry_value_is_object (args_p[0])); /* target */
  TEST_ASSERT (jerry_value_is_string (args_p[1])); /* P */
  TEST_ASSERT (jerry_value_is_number (args_p[2])); /* V */
  TEST_ASSERT (jerry_value_is_object (args_p[3])); /* receiver */

  const char expected[] = "value";
  char buffer[10];
  jerry_size_t copied = jerry_string_to_char_buffer (args_p[1], (jerry_char_t *) buffer, 10);

  TEST_ASSERT (copied == 5);
  TEST_ASSERT (strncmp (expected, buffer, 5) == 0);

  TEST_ASSERT (jerry_value_is_number (args_p[2]));
  demo_value = (int) jerry_get_number_value (args_p[2]);

  return jerry_create_number (demo_value);
} /* handler_set */

static void
set_property (jerry_value_t target, /**< target object */
              const char *name_p, /**< name of the property */
              jerry_value_t value) /**< value of the property */
{
  jerry_value_t name_val = jerry_create_string ((const jerry_char_t *) name_p);
  jerry_value_t result_val = jerry_set_property (target, name_val, value);

  TEST_ASSERT (jerry_value_is_boolean (result_val));
  TEST_ASSERT (jerry_value_is_true (result_val));
  jerry_release_value (name_val);
} /* set_property */

static jerry_value_t
get_property (jerry_value_t target, /**< target object */
              const char *name_p) /**< name of the property */
{
  jerry_value_t name_val = jerry_create_string ((const jerry_char_t *) name_p);
  jerry_value_t result_val = jerry_get_property (target, name_val);

  TEST_ASSERT (!jerry_value_is_error (result_val));
  jerry_release_value (name_val);
  return result_val;
} /* get_property */

static void
set_function (jerry_value_t target, /**< target object */
              const char *name_p, /**< name of the function */
              jerry_external_handler_t handler_p) /**< function callback */
{
  jerry_value_t function_val = jerry_create_external_function (handler_p);
  set_property (target, name_p, function_val);
  jerry_release_value (function_val);
} /* set_function */

struct test_data
{
  int value;
};

static void
proxy_native_freecb (void *native_p, /**< native pointer */
                     jerry_object_native_info_t *info_p) /**< native info */
{
  TEST_ASSERT (native_p != NULL);
  TEST_ASSERT (info_p->free_cb == proxy_native_freecb);
  struct test_data *data_p = (struct test_data *) native_p;
  data_p->value = -1;
} /* proxy_native_freecb */

static const jerry_object_native_info_t proxy_native_info =
{
  .free_cb = proxy_native_freecb,
  .number_of_references = 0,
  .offset_of_references = 0,
};

static jerry_value_t
proxy_native_handler_get (const jerry_call_info_t *call_info_p, /**< call information */
                          const jerry_value_t args_p[], /**< function arguments */
                          const jerry_length_t args_count) /**< number of function arguments */
{
  JERRY_UNUSED (call_info_p);
  TEST_ASSERT (args_count == 3);

  /* 3rd argument (Receiver) should be the Proxy here. */
  jerry_value_t receiver = args_p[2];
  TEST_ASSERT (jerry_value_is_proxy (receiver));

  /* Check if proxy has the native ptr. */
  struct test_data *native_p;
  bool has_p = jerry_get_object_native_pointer (receiver, (void *) &native_p, &proxy_native_info);
  TEST_ASSERT (has_p == true);

  native_p->value <<= 1;
  return jerry_create_number (native_p->value);
} /* proxy_native_handler_get */

/**
 * Test Proxy with added native object.
 */
static void
test_proxy_native (void)
{
  jerry_value_t handler = jerry_create_object ();
  set_function (handler, "get", proxy_native_handler_get);

  jerry_value_t target = jerry_create_object ();
  jerry_value_t proxy = jerry_create_proxy (target, handler);

  struct test_data *data = (struct test_data *) malloc (sizeof (struct test_data));
  data->value = 2;
  jerry_set_object_native_pointer (proxy, data, &proxy_native_info);

  /* Call: proxy[10] */
  jerry_value_t result_for_10 = jerry_get_property_by_index (proxy, 10);
  TEST_ASSERT (jerry_value_is_number (result_for_10));
  TEST_ASSERT (jerry_get_number_value (result_for_10) == 4.0);

  /* Call: proxy[5] */
  data->value = 8;
  jerry_value_t result_for_5 = jerry_get_property_by_index (proxy, 5);
  TEST_ASSERT (jerry_value_is_number (result_for_5));
  TEST_ASSERT (jerry_get_number_value (result_for_5) == 16.0);

  jerry_release_value (handler);
  jerry_release_value (target);
  jerry_release_value (proxy);
} /* test_proxy_native */

int
main (void)
{
  TEST_INIT ();

  if (!jerry_is_feature_enabled (JERRY_FEATURE_PROXY))
  {
    printf ("Skipping test, Proxy not enabled\n");
    return 0;
  }

  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t handler = jerry_create_object ();
  {
    set_function (handler, "get", handler_get);
    set_function (handler, "set", handler_set);
  }

  jerry_value_t target = jerry_create_object ();
  jerry_value_t proxy = jerry_create_proxy (target, handler);
  {
    jerry_value_t global = jerry_get_global_object ();
    set_property (global, "pdemo", proxy);
    jerry_release_value (global);
  }

  const jerry_char_t get_value_src[] = TEST_STRING_LITERAL ("pdemo.value");
  jerry_value_t parsed_get_code_val = jerry_parse (get_value_src,
                                                   sizeof (get_value_src) - 1,
                                                   NULL);
  TEST_ASSERT (!jerry_value_is_error (parsed_get_code_val));

  {
    jerry_value_t res = jerry_run (parsed_get_code_val);
    TEST_ASSERT (jerry_value_is_number (res));
    TEST_ASSERT (jerry_get_number_value (res) == 1.0);
    jerry_release_value (res);
  }

  {
    jerry_value_t res = get_property (proxy, "value");
    TEST_ASSERT (jerry_value_is_number (res));
    TEST_ASSERT (jerry_get_number_value (res) == 2.0);
    jerry_release_value (res);
  }

  {
    jerry_value_t res = jerry_run (parsed_get_code_val);
    TEST_ASSERT (jerry_value_is_number (res));
    TEST_ASSERT (jerry_get_number_value (res) == 3.0);
    jerry_release_value (res);
  }

  const jerry_char_t set_value_src[] = TEST_STRING_LITERAL ("pdemo.value = 55");
  jerry_value_t parsed_set_code_val = jerry_parse (set_value_src,
                                                   sizeof (set_value_src) - 1,
                                                   NULL);
  TEST_ASSERT (!jerry_value_is_error (parsed_set_code_val));

  {
    jerry_value_t res = jerry_run (parsed_set_code_val);
    TEST_ASSERT (jerry_value_is_number (res));
    TEST_ASSERT (jerry_get_number_value (res) == 55);
    jerry_release_value (res);
  }

  {
    jerry_value_t res = jerry_run (parsed_get_code_val);
    TEST_ASSERT (jerry_value_is_number (res));
    TEST_ASSERT (jerry_get_number_value (res) == 56);
    jerry_release_value (res);
  }

  {
    jerry_value_t new_value = jerry_create_number (12);
    set_property (proxy, "value", new_value);
    jerry_release_value (new_value);
  }

  {
    jerry_value_t res = get_property (proxy, "value");
    TEST_ASSERT (jerry_value_is_number (res));
    TEST_ASSERT (jerry_get_number_value (res) == 13.0);
    jerry_release_value (res);
  }

  jerry_release_value (parsed_set_code_val);
  jerry_release_value (parsed_get_code_val);
  jerry_release_value (proxy);
  jerry_release_value (target);
  jerry_release_value (handler);

  {
    const jerry_char_t has_value_src[] = TEST_STRING_LITERAL ("new Proxy({}, {\n"
                                                              "  has: function(target, key) { throw 33 }\n"
                                                              "})");
    jerry_value_t parsed_has_code_val = jerry_parse (has_value_src,
                                                     sizeof (has_value_src) - 1,
                                                     NULL);
    TEST_ASSERT (!jerry_value_is_error (parsed_has_code_val));

    jerry_value_t res = jerry_run (parsed_has_code_val);
    jerry_release_value (parsed_has_code_val);
    TEST_ASSERT (jerry_value_is_proxy (res));

    jerry_value_t name = jerry_create_string ((const jerry_char_t *) "key");
    TEST_ASSERT (jerry_value_is_string (name));
    jerry_value_t property = jerry_has_property (res, name);
    jerry_release_value (name);
    jerry_release_value (res);

    TEST_ASSERT (jerry_value_is_error (property));
    property = jerry_get_value_from_error (property, true);
    TEST_ASSERT (jerry_get_number_value (property) == 33);
    jerry_release_value (property);
  }

  target = jerry_create_object ();
  handler = jerry_create_object ();
  proxy = jerry_create_proxy (target, handler);

  {
    jerry_value_t res = jerry_get_proxy_target (proxy);
    TEST_ASSERT (res == target);
    jerry_release_value (res);

    res = jerry_get_proxy_handler (proxy);
    TEST_ASSERT (res == handler);
    jerry_release_value (res);

    res = jerry_get_proxy_target (target);
    TEST_ASSERT (jerry_value_is_error (res));
    res = jerry_get_value_from_error (res, true);
    TEST_ASSERT (jerry_get_error_type (res) == JERRY_ERROR_TYPE);
    jerry_release_value (res);

    res = jerry_get_proxy_handler (handler);
    TEST_ASSERT (jerry_value_is_error (res));
    res = jerry_get_value_from_error (res, true);
    TEST_ASSERT (jerry_get_error_type (res) == JERRY_ERROR_TYPE);
    jerry_release_value (res);
  }

  jerry_release_value (proxy);
  jerry_release_value (handler);
  jerry_release_value (target);

  test_proxy_native ();

  jerry_cleanup ();
  return 0;
} /* main */

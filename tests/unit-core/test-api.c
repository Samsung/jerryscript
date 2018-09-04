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

const char *test_source = (
                           "function assert (arg) { "
                           "  if (!arg) { "
                           "    throw Error('Assert failed');"
                           "  } "
                           "} "
                           "this.t = 1; "
                           "function f () { "
                           "return this.t; "
                           "} "
                           "this.foo = f; "
                           "this.bar = function (a) { "
                           "return a + t; "
                           "}; "
                           "function A () { "
                           "this.t = 12; "
                           "} "
                           "this.A = A; "
                           "this.a = new A (); "
                           "function call_external () { "
                           "  return this.external ('1', true); "
                           "} "
                           "function call_throw_test() { "
                           "  var catched = false; "
                           "  try { "
                           "    this.throw_test(); "
                           "  } catch (e) { "
                           "    catched = true; "
                           "    assert(e.name == 'TypeError'); "
                           "    assert(e.message == 'error'); "
                           "  } "
                           "  assert(catched); "
                           "} "
                           "function throw_reference_error() { "
                           " throw new ReferenceError ();"
                           "} "
                           "p = {'alpha':32, 'bravo':false, 'charlie':{}, 'delta':123.45, 'echo':'foobar'};"
                           "np = {}; Object.defineProperty (np, 'foxtrot', { "
                           "get: function() { throw 'error'; }, enumerable: true }) "
                           );

bool test_api_is_free_callback_was_called = false;

static jerry_value_t
handler (const jerry_value_t func_obj_val, /**< function object */
         const jerry_value_t this_val, /**< this value */
         const jerry_value_t args_p[], /**< arguments list */
         const jerry_length_t args_cnt) /**< arguments length */
{
  char buffer[32];
  jerry_size_t sz;

  printf ("ok %u %u %p %u\n",
          (unsigned int) func_obj_val, (unsigned int) this_val, (void *) args_p, (unsigned int) args_cnt);

  TEST_ASSERT (args_cnt == 2);

  TEST_ASSERT (jerry_value_is_string (args_p[0]));
  sz = jerry_get_string_size (args_p[0]);
  TEST_ASSERT (sz == 1);
  sz = jerry_string_to_char_buffer (args_p[0],
                                    (jerry_char_t *) buffer,
                                    sz);
  TEST_ASSERT (sz == 1);
  TEST_ASSERT (!strncmp (buffer, "1", (size_t) sz));

  TEST_ASSERT (jerry_value_is_boolean (args_p[1]));

  return jerry_create_string ((jerry_char_t *) "string from handler");
} /* handler */

static jerry_value_t
handler_throw_test (const jerry_value_t func_obj_val, /**< function object */
                    const jerry_value_t this_val, /**< this value */
                    const jerry_value_t args_p[], /**< arguments list */
                    const jerry_length_t args_cnt) /**< arguments length */
{
  printf ("ok %u %u %p %u\n",
          (unsigned int) func_obj_val, (unsigned int) this_val, (void *) args_p, (unsigned int) args_cnt);

  return jerry_create_error (JERRY_ERROR_TYPE, (jerry_char_t *) "error");
} /* handler_throw_test */

static void
handler_construct_freecb (void *native_p)
{
  TEST_ASSERT ((uintptr_t) native_p == (uintptr_t) 0x0012345678abcdefull);
  printf ("ok object free callback\n");

  test_api_is_free_callback_was_called = true;
} /* handler_construct_freecb */


/**
 * The name of the jerry_object_native_info_t struct.
 */
#define JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE(c_type) _jerry_object_native_info_##c_type

/**
 * Define a native pointer's type based on the C type and free callback.
 */
#define JERRY_DEFINE_NATIVE_HANDLE_INFO(c_type, native_free_cb) \
  static const jerry_object_native_info_t JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (c_type) = \
  { \
    .free_cb = (jerry_object_native_free_callback_t) native_free_cb \
  }

JERRY_DEFINE_NATIVE_HANDLE_INFO (bind1, handler_construct_freecb);
JERRY_DEFINE_NATIVE_HANDLE_INFO (bind2, handler_construct_freecb);
JERRY_DEFINE_NATIVE_HANDLE_INFO (bind3, NULL);

static jerry_value_t
handler_construct (const jerry_value_t func_obj_val, /**< function object */
                   const jerry_value_t this_val, /**< this value */
                   const jerry_value_t args_p[], /**< arguments list */
                   const jerry_length_t args_cnt) /**< arguments length */
{
  printf ("ok construct %u %u %p %u\n",
          (unsigned int) func_obj_val, (unsigned int) this_val, (void *) args_p, (unsigned int) args_cnt);

  TEST_ASSERT (jerry_value_is_object (this_val));

  TEST_ASSERT (args_cnt == 1);
  TEST_ASSERT (jerry_value_is_boolean (args_p[0]));
  TEST_ASSERT (jerry_get_boolean_value (args_p[0]) == true);

  jerry_value_t field_name = jerry_create_string ((jerry_char_t *) "value_field");
  jerry_set_property (this_val, field_name, args_p[0]);
  jerry_release_value (field_name);

  jerry_set_object_native_pointer (this_val,
                                   (void *) 0x0000000000000000ull,
                                   &JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (bind1));

  void *ptr = NULL;
  const jerry_object_native_info_t *out_info_p;
  bool is_ok = jerry_get_object_native_pointer (this_val, &ptr, &out_info_p);
  TEST_ASSERT (is_ok
               && (uintptr_t) ptr == (uintptr_t) 0x0000000000000000ull
               && out_info_p == &JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (bind1));
  /* check if setting handle for second time is handled correctly */
  jerry_set_object_native_pointer (this_val,
                                   (void *) 0x0012345678abcdefull,
                                   &JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (bind2));

  return jerry_create_boolean (true);
} /* handler_construct */

/**
 * Extended Magic Strings
 */
#define JERRY_MAGIC_STRING_ITEMS \
  JERRY_MAGIC_STRING_DEF (GLOBAL, global) \
  JERRY_MAGIC_STRING_DEF (GREEK_ZERO_SIGN, \xed\xa0\x80\xed\xb6\x8a) \
  JERRY_MAGIC_STRING_DEF (CONSOLE, console)

#define JERRY_MAGIC_STRING_DEF(NAME, STRING) \
  static const char jerry_magic_string_ex_ ## NAME[] = # STRING;

JERRY_MAGIC_STRING_ITEMS

#undef JERRY_MAGIC_STRING_DEF

const jerry_length_t magic_string_lengths[] =
{
#define JERRY_MAGIC_STRING_DEF(NAME, STRING) \
    (jerry_length_t) (sizeof (jerry_magic_string_ex_ ## NAME) - 1u),

  JERRY_MAGIC_STRING_ITEMS

#undef JERRY_MAGIC_STRING_DEF
};

const jerry_char_t *magic_string_items[] =
{
#define JERRY_MAGIC_STRING_DEF(NAME, STRING) \
    (const jerry_char_t *) jerry_magic_string_ex_ ## NAME,

  JERRY_MAGIC_STRING_ITEMS

#undef JERRY_MAGIC_STRING_DEF
};

static bool
foreach (const jerry_value_t name, /**< field name */
         const jerry_value_t value, /**< field value */
         void *user_data) /**< user data */
{
  char str_buf_p[128];
  jerry_size_t sz = jerry_string_to_char_buffer (name, (jerry_char_t *) str_buf_p, 128);
  str_buf_p[sz] = '\0';

  TEST_ASSERT (!strncmp ((const char *) user_data, "user_data", 9));
  TEST_ASSERT (sz > 0);

  if (!strncmp (str_buf_p, "alpha", (size_t) sz))
  {
    TEST_ASSERT (jerry_value_is_number (value));
    TEST_ASSERT (jerry_get_number_value (value) == 32.0);
    return true;
  }
  else if (!strncmp (str_buf_p, "bravo", (size_t) sz))
  {
    TEST_ASSERT (jerry_value_is_boolean (value));
    TEST_ASSERT (jerry_get_boolean_value (value) == false);
    return true;
  }
  else if (!strncmp (str_buf_p, "charlie", (size_t) sz))
  {
    TEST_ASSERT (jerry_value_is_object (value));
    return true;
  }
  else if (!strncmp (str_buf_p, "delta", (size_t) sz))
  {
    TEST_ASSERT (jerry_value_is_number (value));
    TEST_ASSERT (jerry_get_number_value (value) == 123.45);
    return true;
  }
  else if (!strncmp (str_buf_p, "echo", (size_t) sz))
  {
    TEST_ASSERT (jerry_value_is_string (value));
    jerry_size_t echo_sz = jerry_string_to_char_buffer (value,
                                                        (jerry_char_t *) str_buf_p,
                                                        128);
    str_buf_p[echo_sz] = '\0';
    TEST_ASSERT (!strncmp (str_buf_p, "foobar", (size_t) echo_sz));
    return true;
  }

  TEST_ASSERT (false);
  return false;


} /* foreach */

static bool
foreach_exception (const jerry_value_t name, /**< field name */
                   const jerry_value_t value, /**< field value */
                   void *user_data) /**< user data */
{
  JERRY_UNUSED (value);
  JERRY_UNUSED (user_data);
  char str_buf_p[128];
  jerry_size_t sz = jerry_string_to_char_buffer (name, (jerry_char_t *) str_buf_p, 128);
  str_buf_p[sz] = '\0';

  if (!strncmp (str_buf_p, "foxtrot", (size_t) sz))
  {
    TEST_ASSERT (false);
  }

  return true;
} /* foreach_exception */

static bool
foreach_subset (const jerry_value_t name, /**< field name */
                const jerry_value_t value, /**< field value */
                void *user_data) /**< user data */
{
  JERRY_UNUSED (name);
  JERRY_UNUSED (value);
  int *count_p = (int *) (user_data);

  if (*count_p == 3)
  {
    return false;
  }
  (*count_p)++;
  return true;
} /* foreach_subset */

static jerry_value_t
get_property (const jerry_value_t obj_val, /**< object value */
              const char *str_p) /**< property name */
{
  jerry_value_t prop_name_val = jerry_create_string ((const jerry_char_t *) str_p);
  jerry_value_t ret_val = jerry_get_property (obj_val, prop_name_val);
  jerry_release_value (prop_name_val);
  return ret_val;
} /* get_property */

static jerry_value_t
set_property (const jerry_value_t obj_val, /**< object value */
              const char *str_p, /**< property name */
              const jerry_value_t val) /**< value to set */
{
  jerry_value_t prop_name_val = jerry_create_string ((const jerry_char_t *) str_p);
  jerry_value_t ret_val = jerry_set_property (obj_val, prop_name_val, val);
  jerry_release_value (prop_name_val);
  return ret_val;
} /* set_property */

static bool
test_run_simple (const char *script_p) /**< source code to run */
{
  size_t script_size = strlen (script_p);

  return jerry_run_simple ((const jerry_char_t *) script_p, script_size, JERRY_INIT_DEFAULT);
} /* test_run_simple */

static bool
strict_equals (jerry_value_t a,
               jerry_value_t b)
{
  bool is_strict_equal;
  const char *is_equal_src;
  jerry_value_t args[2];
  jerry_value_t is_equal_fn_val;
  jerry_value_t res;

  is_equal_src = "var isEqual = function(a, b) { return (a === b); }; isEqual";
  is_equal_fn_val = jerry_eval ((jerry_char_t *) is_equal_src, strlen (is_equal_src), JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (!jerry_value_is_error (is_equal_fn_val));
  args[0] = a;
  args[1] = b;
  res = jerry_call_function (is_equal_fn_val, jerry_create_undefined (), args, 2);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_boolean (res));
  is_strict_equal = jerry_get_boolean_value (res);
  jerry_release_value (res);
  jerry_release_value (is_equal_fn_val);
  return is_strict_equal;
} /* strict_equals */

int
main (void)
{
  TEST_INIT ();

  bool is_ok;
  jerry_size_t sz, utf8_sz, cesu8_sz;
  jerry_length_t cesu8_length, utf8_length;
  jerry_value_t val_t, val_foo, val_bar, val_A, val_A_prototype, val_a, val_a_foo, val_value_field, val_p, val_np;
  jerry_value_t val_call_external;
  jerry_value_t global_obj_val, obj_val;
  jerry_value_t external_func_val, external_construct_val;
  jerry_value_t throw_test_handler_val;
  jerry_value_t parsed_code_val, proto_val, prim_val;
  jerry_value_t res, args[2];
  double number_val;
  char buffer[32];

  is_ok = test_run_simple ("throw 'Hello World';");
  TEST_ASSERT (!is_ok);

  jerry_init (JERRY_INIT_DEFAULT);

  parsed_code_val = jerry_parse (NULL,
                                 0,
                                 (jerry_char_t *) test_source,
                                 strlen (test_source),
                                 JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (!jerry_value_is_error (parsed_code_val));

  res = jerry_run (parsed_code_val);
  TEST_ASSERT (!jerry_value_is_error (res));
  jerry_release_value (res);
  jerry_release_value (parsed_code_val);

  global_obj_val = jerry_get_global_object ();

  /* Test corner case for jerry_string_to_char_buffer */
  args[0] = jerry_create_string ((jerry_char_t *) "");
  sz = jerry_get_string_size (args[0]);
  TEST_ASSERT (sz == 0);
  jerry_release_value (args[0]);

  /* Test create_jerry_string_from_utf8 with 4-byte long unicode sequences,
   * test string: 'str: {DESERET CAPITAL LETTER LONG I}'
   */
  args[0] = jerry_create_string_from_utf8 ((jerry_char_t *) "\x73\x74\x72\x3a \xf0\x90\x90\x80");
  args[1] = jerry_create_string ((jerry_char_t *) "\x73\x74\x72\x3a \xed\xa0\x81\xed\xb0\x80");

  /* These sizes must be equal */
  utf8_sz = jerry_get_string_size (args[0]);
  cesu8_sz = jerry_get_string_size (args[1]);

  char string_from_utf8[utf8_sz];
  char string_from_cesu8[cesu8_sz];

  jerry_string_to_char_buffer (args[0], (jerry_char_t *) string_from_utf8, utf8_sz);
  jerry_string_to_char_buffer (args[1], (jerry_char_t *) string_from_cesu8, cesu8_sz);

  TEST_ASSERT (utf8_sz == cesu8_sz);
  TEST_ASSERT (!strncmp (string_from_utf8, string_from_cesu8, utf8_sz));
  jerry_release_value (args[0]);
  jerry_release_value (args[1]);

  /* Test jerry_string_to_utf8_char_buffer, test string: 'str: {DESERET CAPITAL LETTER LONG I}' */
  args[0] = jerry_create_string_from_utf8 ((jerry_char_t *) "\x73\x74\x72\x3a \xf0\x90\x90\x80");
  args[1] = jerry_create_string ((jerry_char_t *) "\x73\x74\x72\x3a \xed\xa0\x81\xed\xb0\x80");

  /* Test that the strings are equal / ensure hashes are equal */
  TEST_ASSERT (strict_equals (args[0], args[1]));

  /* These sizes must be equal */
  utf8_sz = jerry_get_utf8_string_size (args[0]);
  cesu8_sz = jerry_get_utf8_string_size (args[1]);

  TEST_ASSERT (utf8_sz == cesu8_sz);

  char string_from_utf8_string[utf8_sz];
  char string_from_cesu8_string[cesu8_sz];

  jerry_string_to_utf8_char_buffer (args[0], (jerry_char_t *) string_from_utf8_string, utf8_sz);
  jerry_string_to_utf8_char_buffer (args[1], (jerry_char_t *) string_from_cesu8_string, cesu8_sz);

  TEST_ASSERT (!strncmp (string_from_utf8_string, string_from_cesu8_string, utf8_sz));
  jerry_release_value (args[0]);
  jerry_release_value (args[1]);

  /* Test string: 'str: {MATHEMATICAL FRAKTUR SMALL F}{MATHEMATICAL FRAKTUR SMALL G}' */
  args[0] = jerry_create_string_from_utf8 ((jerry_char_t *) "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 \xf0\x9d\x94\xa4");

  cesu8_length = jerry_get_string_length (args[0]);
  utf8_length = jerry_get_utf8_string_length (args[0]);

  cesu8_sz = jerry_get_string_size (args[0]);
  utf8_sz =  jerry_get_utf8_string_size (args[0]);

  TEST_ASSERT (cesu8_length == 10 && utf8_length == 8);
  TEST_ASSERT (cesu8_sz != utf8_sz);
  TEST_ASSERT (utf8_sz == 14 && cesu8_sz == 18);

  char test_string[utf8_sz];

  TEST_ASSERT (jerry_string_to_utf8_char_buffer (args[0], (jerry_char_t *) test_string, utf8_sz) == 14);
  TEST_ASSERT (!strncmp (test_string, "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 \xf0\x9d\x94\xa4", utf8_sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0], 0, utf8_length, (jerry_char_t *) test_string, utf8_sz);
  TEST_ASSERT (sz == 14);
  TEST_ASSERT (!strncmp (test_string, "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 \xf0\x9d\x94\xa4", sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0], 0, utf8_length + 1, (jerry_char_t *) test_string, utf8_sz);
  TEST_ASSERT (sz == 14);
  TEST_ASSERT (!strncmp (test_string, "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 \xf0\x9d\x94\xa4", sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0], utf8_length, 0, (jerry_char_t *) test_string, utf8_sz);
  TEST_ASSERT (sz == 0);

  sz = jerry_substring_to_utf8_char_buffer (args[0], 0, utf8_length, (jerry_char_t *) test_string, utf8_sz - 1);
  TEST_ASSERT (sz == 10);
  TEST_ASSERT (!strncmp (test_string, "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 ", sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0], 0, utf8_length - 1, (jerry_char_t *) test_string, utf8_sz);
  TEST_ASSERT (sz == 10);
  TEST_ASSERT (!strncmp (test_string, "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 ", sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0],
                                            utf8_length - 2,
                                            utf8_length - 1,
                                            (jerry_char_t *) test_string,
                                            utf8_sz);
  TEST_ASSERT (sz == 1);
  TEST_ASSERT (!strncmp (test_string, " ", sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0],
                                            utf8_length - 3,
                                            utf8_length - 2,
                                            (jerry_char_t *) test_string,
                                            utf8_sz);
  TEST_ASSERT (sz == 4);
  TEST_ASSERT (!strncmp (test_string, "\xf0\x9d\x94\xa3", sz));

  jerry_release_value (args[0]);

  /* Test string: 'str: {DESERET CAPITAL LETTER LONG I}' */
  args[0] = jerry_create_string ((jerry_char_t *) "\x73\x74\x72\x3a \xed\xa0\x81\xed\xb0\x80");

  cesu8_length = jerry_get_string_length (args[0]);
  utf8_length = jerry_get_utf8_string_length (args[0]);

  cesu8_sz = jerry_get_string_size (args[0]);
  utf8_sz =  jerry_get_utf8_string_size (args[0]);

  TEST_ASSERT (cesu8_length == 7 && utf8_length == 6);
  TEST_ASSERT (cesu8_sz != utf8_sz);
  TEST_ASSERT (utf8_sz == 9 && cesu8_sz == 11);

  jerry_release_value (args[0]);

  /* Test string: 'price: 10{EURO SIGN}' */
  args[0] = jerry_create_string_from_utf8 ((jerry_char_t *) "\x70\x72\x69\x63\x65\x3a \x31\x30\xe2\x82\xac");

  cesu8_length = jerry_get_string_length (args[0]);
  utf8_length = jerry_get_utf8_string_length (args[0]);

  cesu8_sz = jerry_get_string_size (args[0]);
  utf8_sz =  jerry_get_utf8_string_size (args[0]);

  TEST_ASSERT (cesu8_length == utf8_length);
  TEST_ASSERT (cesu8_length == 10);
  TEST_ASSERT (cesu8_sz == utf8_sz);
  TEST_ASSERT (utf8_sz == 12);
  jerry_release_value (args[0]);

  /* Test string: '3' */
  {
    jerry_value_t test_str = jerry_create_string ((const jerry_char_t *) "3");
    char result_string[1] = { 'E' };
    jerry_size_t copied_utf8 = jerry_substring_to_utf8_char_buffer (test_str,
                                                                    0,
                                                                    1,
                                                                    (jerry_char_t *) result_string,
                                                                    sizeof (result_string));
    TEST_ASSERT (copied_utf8 == 1);
    TEST_ASSERT (result_string[0] == '3');

    result_string[0] = 'E';
    jerry_size_t copied = jerry_substring_to_char_buffer (test_str,
                                                          0,
                                                          1,
                                                          (jerry_char_t *) result_string,
                                                          sizeof (result_string));
    TEST_ASSERT (copied == 1);
    TEST_ASSERT (result_string[0] == '3');

    jerry_release_value (test_str);
  }

  /* Test jerry_substring_to_char_buffer */
  args[0] = jerry_create_string ((jerry_char_t *) "an ascii string");

  /* Buffer size */
  cesu8_sz = 5;

  char substring[cesu8_sz];
  sz = jerry_substring_to_char_buffer (args[0], 3, 8, (jerry_char_t *) substring, cesu8_sz);
  TEST_ASSERT (sz == 5);
  TEST_ASSERT (!strncmp (substring, "ascii", sz));

  /* Buffer size is 5, substring length is 11 => copied only the first 5 char */
  sz = jerry_substring_to_char_buffer (args[0], 0, 11, (jerry_char_t *) substring, cesu8_sz);

  TEST_ASSERT (sz == 5);
  TEST_ASSERT (!strncmp (substring, "an as", sz));

  /* Position of the first character is greater than the string length */
  sz = jerry_substring_to_char_buffer (args[0], 16, 21, (jerry_char_t *) substring, cesu8_sz);
  TEST_ASSERT (sz == 0);

  sz = jerry_substring_to_char_buffer (args[0], 14, 15, (jerry_char_t *) substring, cesu8_sz);
  TEST_ASSERT (sz == 1);
  TEST_ASSERT (!strncmp (substring, "g", sz));

  sz = jerry_substring_to_char_buffer (args[0], 0, 1, (jerry_char_t *) substring, cesu8_sz);
  TEST_ASSERT (sz == 1);
  TEST_ASSERT (!strncmp (substring, "a", sz));

  cesu8_length = jerry_get_string_length (args[0]);
  cesu8_sz = jerry_get_string_size (args[0]);
  TEST_ASSERT (cesu8_length == 15);
  TEST_ASSERT (cesu8_length == cesu8_sz);

  sz = jerry_substring_to_char_buffer (args[0], 0, cesu8_length, (jerry_char_t *) substring, cesu8_sz);
  TEST_ASSERT (sz = 15);
  TEST_ASSERT (!strncmp (substring, "an ascii string", sz));

  jerry_release_value (args[0]);

  /* Test jerry_substring_to_char_buffer: '0101' */
  args[0] = jerry_create_string ((jerry_char_t *) "0101");
  cesu8_sz = jerry_get_string_size (args[0]);

  char number_substring[cesu8_sz];

  sz = jerry_substring_to_char_buffer (args[0], 1, 3, (jerry_char_t *) number_substring, cesu8_sz);
  TEST_ASSERT (sz == 2);
  TEST_ASSERT (!strncmp (number_substring, "10", sz));

  jerry_release_value (args[0]);

  /* Test jerry_substring_to_char_buffer: 'str: {greek zero sign}' */
  args[0] = jerry_create_string ((jerry_char_t *) "\x73\x74\x72\x3a \xed\xa0\x80\xed\xb6\x8a");
  cesu8_sz = jerry_get_string_size (args[0]);
  cesu8_length = jerry_get_string_length (args[0]);
  TEST_ASSERT (cesu8_sz == 11);
  TEST_ASSERT (cesu8_length = 7);

  char supl_substring[cesu8_sz];

  sz = jerry_substring_to_char_buffer (args[0], 0, cesu8_length, (jerry_char_t *) supl_substring, cesu8_sz);
  TEST_ASSERT (sz == 11);
  TEST_ASSERT (!strncmp (supl_substring, "\x73\x74\x72\x3a \xed\xa0\x80\xed\xb6\x8a", sz));

  /* Decrease the buffer size => the low surrogate char will not fit into the buffer */
  cesu8_sz -= 1;
  sz = jerry_substring_to_char_buffer (args[0], 0, cesu8_length, (jerry_char_t *) supl_substring, cesu8_sz);
  TEST_ASSERT (sz == 8);
  TEST_ASSERT (!strncmp (supl_substring, "\x73\x74\x72\x3a \xed\xa0\x80", sz));

  sz = jerry_substring_to_char_buffer (args[0],
                                       cesu8_length - 1,
                                       cesu8_length,
                                       (jerry_char_t *) supl_substring,
                                       cesu8_sz);
  TEST_ASSERT (sz == 3);
  TEST_ASSERT (!strncmp (supl_substring, "\xed\xb6\x8a", sz));

  sz = jerry_substring_to_char_buffer (args[0],
                                       cesu8_length - 2,
                                       cesu8_length - 1,
                                       (jerry_char_t *) supl_substring,
                                       cesu8_sz);
  TEST_ASSERT (sz == 3);
  TEST_ASSERT (!strncmp (supl_substring, "\xed\xa0\x80", sz));

  jerry_release_value (args[0]);

  /* Get global.boo (non-existing field) */
  val_t = get_property (global_obj_val, "boo");
  TEST_ASSERT (!jerry_value_is_error (val_t));
  TEST_ASSERT (jerry_value_is_undefined (val_t));

  /* Get global.t */
  val_t = get_property (global_obj_val, "t");
  TEST_ASSERT (!jerry_value_is_error (val_t));
  TEST_ASSERT (jerry_value_is_number (val_t)
               && jerry_get_number_value (val_t) == 1.0);
  jerry_release_value (val_t);

  /* Get global.foo */
  val_foo = get_property (global_obj_val, "foo");
  TEST_ASSERT (!jerry_value_is_error (val_foo));
  TEST_ASSERT (jerry_value_is_object (val_foo));

  /* Call foo (4, 2) */
  args[0] = jerry_create_number (4);
  args[1] = jerry_create_number (2);
  res = jerry_call_function (val_foo, jerry_create_undefined (), args, 2);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_number (res)
               && jerry_get_number_value (res) == 1.0);
  jerry_release_value (res);

  /* Get global.bar */
  val_bar = get_property (global_obj_val, "bar");
  TEST_ASSERT (!jerry_value_is_error (val_bar));
  TEST_ASSERT (jerry_value_is_object (val_bar));

  /* Call bar (4, 2) */
  res = jerry_call_function (val_bar, jerry_create_undefined (), args, 2);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_number (res)
               && jerry_get_number_value (res) == 5.0);
  jerry_release_value (res);
  jerry_release_value (val_bar);

  /* Set global.t = "abcd" */
  jerry_release_value (args[0]);
  args[0] = jerry_create_string ((jerry_char_t *) "abcd");
  res = set_property (global_obj_val, "t", args[0]);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_get_boolean_value (res));
  jerry_release_value (res);

  /* Call foo (4, 2) */
  res = jerry_call_function (val_foo, jerry_create_undefined (), args, 2);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_string (res));
  sz = jerry_get_string_size (res);
  TEST_ASSERT (sz == 4);
  sz = jerry_string_to_char_buffer (res, (jerry_char_t *) buffer, sz);
  TEST_ASSERT (sz == 4);
  jerry_release_value (res);
  TEST_ASSERT (!strncmp (buffer, "abcd", (size_t) sz));
  jerry_release_value (args[0]);
  jerry_release_value (args[1]);

  /* Get global.A */
  val_A = get_property (global_obj_val, "A");
  TEST_ASSERT (!jerry_value_is_error (val_A));
  TEST_ASSERT (jerry_value_is_object (val_A));

  /* Get A.prototype */
  is_ok = jerry_value_is_constructor (val_A);
  TEST_ASSERT (is_ok);
  val_A_prototype = get_property (val_A, "prototype");
  TEST_ASSERT (!jerry_value_is_error (val_A_prototype));
  TEST_ASSERT (jerry_value_is_object (val_A_prototype));
  jerry_release_value (val_A);

  /* Set A.prototype.foo = global.foo */
  res = set_property (val_A_prototype, "foo", val_foo);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_get_boolean_value (res));
  jerry_release_value (res);
  jerry_release_value (val_A_prototype);
  jerry_release_value (val_foo);

  /* Get global.a */
  val_a = get_property (global_obj_val, "a");
  TEST_ASSERT (!jerry_value_is_error (val_a));
  TEST_ASSERT (jerry_value_is_object (val_a));

  /* Get a.t */
  res = get_property (val_a, "t");
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_number (res)
               && jerry_get_number_value (res) == 12.0);
  jerry_release_value (res);

  /* foreach properties */
  val_p = get_property (global_obj_val, "p");
  is_ok = jerry_foreach_object_property (val_p, foreach, (void *) "user_data");
  TEST_ASSERT (is_ok);

  /* break foreach at third element */
  int count = 0;
  is_ok = jerry_foreach_object_property (val_p, foreach_subset, &count);
  TEST_ASSERT (is_ok);
  TEST_ASSERT (count == 3);
  jerry_release_value (val_p);

  /* foreach with throw test */
  val_np = get_property (global_obj_val, "np");
  is_ok = !jerry_foreach_object_property (val_np, foreach_exception, NULL);
  TEST_ASSERT (is_ok);
  jerry_release_value (val_np);

  /* Get a.foo */
  val_a_foo = get_property (val_a, "foo");
  TEST_ASSERT (!jerry_value_is_error (val_a_foo));
  TEST_ASSERT (jerry_value_is_object (val_a_foo));

  /* Call a.foo () */
  res = jerry_call_function (val_a_foo, val_a, NULL, 0);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_number (res)
               && jerry_get_number_value (res) == 12.0);
  jerry_release_value (res);
  jerry_release_value (val_a_foo);

  jerry_release_value (val_a);

  /* Create native handler bound function object and set it to 'external' variable */
  external_func_val = jerry_create_external_function (handler);
  TEST_ASSERT (jerry_value_is_function (external_func_val)
               && jerry_value_is_constructor (external_func_val));

  res = set_property (global_obj_val, "external", external_func_val);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_get_boolean_value (res));
  jerry_release_value (external_func_val);

  /* Call 'call_external' function that should call external function created above */
  val_call_external = get_property (global_obj_val, "call_external");
  TEST_ASSERT (!jerry_value_is_error (val_call_external));
  TEST_ASSERT (jerry_value_is_object (val_call_external));
  res = jerry_call_function (val_call_external, global_obj_val, NULL, 0);
  jerry_release_value (val_call_external);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_string (res));
  sz = jerry_get_string_size (res);
  TEST_ASSERT (sz == 19);
  sz = jerry_string_to_char_buffer (res, (jerry_char_t *) buffer, sz);
  TEST_ASSERT (sz == 19);
  jerry_release_value (res);
  TEST_ASSERT (!strncmp (buffer, "string from handler", (size_t) sz));

  /* Create native handler bound function object and set it to 'external_construct' variable */
  external_construct_val = jerry_create_external_function (handler_construct);
  TEST_ASSERT (jerry_value_is_function (external_construct_val)
               && jerry_value_is_constructor (external_construct_val));

  res = set_property (global_obj_val, "external_construct", external_construct_val);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_get_boolean_value (res));
  jerry_release_value (res);

  /* Call external function created above, as constructor */
  args[0] = jerry_create_boolean (true);
  res = jerry_construct_object (external_construct_val, args, 1);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_object (res));
  val_value_field = get_property (res, "value_field");

  /* Get 'value_field' of constructed object */
  TEST_ASSERT (!jerry_value_is_error (val_value_field));
  TEST_ASSERT (jerry_value_is_boolean (val_value_field)
               && jerry_get_boolean_value (val_value_field));
  jerry_release_value (val_value_field);
  jerry_release_value (external_construct_val);

  void *ptr = NULL;
  const jerry_object_native_info_t *out_info_p;
  is_ok = jerry_get_object_native_pointer (res, &ptr, &out_info_p);
  TEST_ASSERT (is_ok
               && (uintptr_t) ptr == (uintptr_t) 0x0012345678abcdefull
               && out_info_p == &JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (bind2));

  /* Passing NULL for out_info_p is allowed. */
  is_ok = jerry_get_object_native_pointer (res, &ptr, NULL);
  TEST_ASSERT (is_ok
               && (uintptr_t) ptr == (uintptr_t) 0x0012345678abcdefull);

  jerry_release_value (res);

  /* Test: It is ok to set native pointer's free callback as NULL. */
  jerry_value_t obj_freecb = jerry_create_object ();
  jerry_set_object_native_pointer (obj_freecb,
                                   (void *) 0x1234,
                                   &JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (bind3));

  jerry_release_value (obj_freecb);

  /* Test: Throwing exception from native handler. */
  throw_test_handler_val = jerry_create_external_function (handler_throw_test);
  TEST_ASSERT (jerry_value_is_function (throw_test_handler_val));

  res = set_property (global_obj_val, "throw_test", throw_test_handler_val);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_get_boolean_value (res));
  jerry_release_value (res);
  jerry_release_value (throw_test_handler_val);

  val_t = get_property (global_obj_val, "call_throw_test");
  TEST_ASSERT (!jerry_value_is_error (val_t));
  TEST_ASSERT (jerry_value_is_object (val_t));

  res = jerry_call_function (val_t, global_obj_val, NULL, 0);
  TEST_ASSERT (!jerry_value_is_error (res));
  jerry_release_value (val_t);
  jerry_release_value (res);

  /* Test: Unhandled exception in called function */
  val_t = get_property (global_obj_val, "throw_reference_error");
  TEST_ASSERT (!jerry_value_is_error (val_t));
  TEST_ASSERT (jerry_value_is_object (val_t));

  res = jerry_call_function (val_t, global_obj_val, NULL, 0);

  TEST_ASSERT (jerry_value_is_error (res));
  jerry_release_value (val_t);

  /* 'res' should contain exception object */
  res = jerry_get_value_from_error (res, true);
  TEST_ASSERT (jerry_value_is_object (res));
  jerry_release_value (res);

  /* Test: Call of non-function */
  obj_val = jerry_create_object ();
  res = jerry_call_function (obj_val, global_obj_val, NULL, 0);
  TEST_ASSERT (jerry_value_is_error (res));

  /* 'res' should contain exception object */
  res = jerry_get_value_from_error (res, true);
  TEST_ASSERT (jerry_value_is_object (res));
  jerry_release_value (res);

  jerry_release_value (obj_val);

  /* Test: Unhandled exception in function called, as constructor */
  val_t = get_property (global_obj_val, "throw_reference_error");
  TEST_ASSERT (!jerry_value_is_error (val_t));
  TEST_ASSERT (jerry_value_is_object (val_t));

  res = jerry_construct_object (val_t, NULL, 0);
  TEST_ASSERT (jerry_value_is_error (res));
  jerry_release_value (val_t);

  /* 'res' should contain exception object */
  res = jerry_get_value_from_error (res, true);
  TEST_ASSERT (jerry_value_is_object (res));
  jerry_release_value (res);

  /* Test: Call of non-function as constructor */
  obj_val = jerry_create_object ();
  res = jerry_construct_object (obj_val, NULL, 0);
  TEST_ASSERT (jerry_value_is_error (res));

  /* 'res' should contain exception object */
  res = jerry_get_value_from_error (res, true);
  TEST_ASSERT (jerry_value_is_object (res));
  jerry_release_value (res);

  jerry_release_value (obj_val);

  /* Test: Array Object API */
  jerry_value_t array_obj_val = jerry_create_array (10);
  TEST_ASSERT (jerry_value_is_array (array_obj_val));
  TEST_ASSERT (jerry_get_array_length (array_obj_val) == 10);

  jerry_value_t v_in = jerry_create_number (10.5);
  jerry_set_property_by_index (array_obj_val, 5, v_in);
  jerry_value_t v_out = jerry_get_property_by_index (array_obj_val, 5);

  TEST_ASSERT (jerry_value_is_number (v_out)
               && jerry_get_number_value (v_out) == 10.5);

  jerry_delete_property_by_index (array_obj_val, 5);
  jerry_value_t v_und = jerry_get_property_by_index (array_obj_val, 5);

  TEST_ASSERT (jerry_value_is_undefined (v_und));

  jerry_release_value (v_in);
  jerry_release_value (v_out);
  jerry_release_value (v_und);
  jerry_release_value (array_obj_val);

  /* Test: init property descriptor */
  jerry_property_descriptor_t prop_desc;
  jerry_init_property_descriptor_fields (&prop_desc);
  TEST_ASSERT (prop_desc.is_value_defined == false);
  TEST_ASSERT (jerry_value_is_undefined (prop_desc.value));
  TEST_ASSERT (prop_desc.is_writable_defined == false);
  TEST_ASSERT (prop_desc.is_writable == false);
  TEST_ASSERT (prop_desc.is_enumerable_defined == false);
  TEST_ASSERT (prop_desc.is_enumerable == false);
  TEST_ASSERT (prop_desc.is_configurable_defined == false);
  TEST_ASSERT (prop_desc.is_configurable == false);
  TEST_ASSERT (prop_desc.is_get_defined == false);
  TEST_ASSERT (jerry_value_is_undefined (prop_desc.getter));
  TEST_ASSERT (prop_desc.is_set_defined == false);
  TEST_ASSERT (jerry_value_is_undefined (prop_desc.setter));

  /* Test: define own properties */
  jerry_value_t prop_name = jerry_create_string ((const jerry_char_t *) "my_defined_property");
  prop_desc.is_value_defined = true;
  prop_desc.value = jerry_acquire_value (prop_name);
  res = jerry_define_own_property (global_obj_val, prop_name, &prop_desc);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_boolean (res));
  TEST_ASSERT (jerry_get_boolean_value (res));
  jerry_release_value (res);
  jerry_free_property_descriptor_fields (&prop_desc);

  /* Test: get own property descriptor */
  is_ok = jerry_get_own_property_descriptor (global_obj_val, prop_name, &prop_desc);
  TEST_ASSERT (is_ok);
  TEST_ASSERT (prop_desc.is_value_defined == true);
  TEST_ASSERT (jerry_value_is_string (prop_desc.value));
  TEST_ASSERT (prop_desc.is_writable == false);
  TEST_ASSERT (prop_desc.is_enumerable == false);
  TEST_ASSERT (prop_desc.is_configurable == false);
  TEST_ASSERT (prop_desc.is_get_defined == false);
  TEST_ASSERT (jerry_value_is_undefined (prop_desc.getter));
  TEST_ASSERT (prop_desc.is_set_defined == false);
  TEST_ASSERT (jerry_value_is_undefined (prop_desc.setter));
  jerry_release_value (prop_name);
  jerry_free_property_descriptor_fields (&prop_desc);

  /* Test: object keys */
  res = jerry_get_object_keys (global_obj_val);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_array (res));
  jerry_release_value (res);

  /* Test: jerry_value_to_primitive */
  obj_val = jerry_eval ((jerry_char_t *) "new String ('hello')", 20, JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (!jerry_value_is_error (obj_val));
  TEST_ASSERT (jerry_value_is_object (obj_val));
  TEST_ASSERT (!jerry_value_is_string (obj_val));
  prim_val = jerry_value_to_primitive (obj_val);
  TEST_ASSERT (!jerry_value_is_error (prim_val));
  TEST_ASSERT (jerry_value_is_string (prim_val));
  jerry_release_value (prim_val);

  /* Test: jerry_get_prototype */
  proto_val = jerry_get_prototype (obj_val);
  TEST_ASSERT (!jerry_value_is_error (proto_val));
  TEST_ASSERT (jerry_value_is_object (proto_val));
  jerry_release_value (obj_val);

  /* Test: jerry_set_prototype */
  obj_val = jerry_create_object ();
  res = jerry_set_prototype (obj_val, jerry_create_null ());
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_boolean (res));
  TEST_ASSERT (jerry_get_boolean_value (res));

  res = jerry_set_prototype (obj_val, jerry_create_object ());
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_boolean (res));
  TEST_ASSERT (jerry_get_boolean_value (res));
  proto_val = jerry_get_prototype (obj_val);
  TEST_ASSERT (!jerry_value_is_error (proto_val));
  TEST_ASSERT (jerry_value_is_object (proto_val));
  jerry_release_value (proto_val);
  jerry_release_value (obj_val);

  /* Test: eval */
  const char *eval_code_src_p = "(function () { return 123; })";
  val_t = jerry_eval ((jerry_char_t *) eval_code_src_p, strlen (eval_code_src_p), JERRY_PARSE_STRICT_MODE);
  TEST_ASSERT (!jerry_value_is_error (val_t));
  TEST_ASSERT (jerry_value_is_object (val_t));
  TEST_ASSERT (jerry_value_is_function (val_t));

  res = jerry_call_function (val_t, jerry_create_undefined (), NULL, 0);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_number (res)
               && jerry_get_number_value (res) == 123.0);
  jerry_release_value (res);

  jerry_release_value (val_t);

  /* cleanup. */
  jerry_release_value (global_obj_val);

  /* Test: run gc. */
  jerry_gc (JERRY_GC_SEVERITY_LOW);

  /* Test: spaces */
  eval_code_src_p = "\x0a \x0b \x0c \xc2\xa0 \xe2\x80\xa8 \xe2\x80\xa9 \xef\xbb\xbf 4321";
  val_t = jerry_eval ((jerry_char_t *) eval_code_src_p, strlen (eval_code_src_p), JERRY_PARSE_STRICT_MODE);
  TEST_ASSERT (!jerry_value_is_error (val_t));
  TEST_ASSERT (jerry_value_is_number (val_t)
               && jerry_get_number_value (val_t) == 4321.0);
  jerry_release_value (val_t);

  /* Test: number */
  val_t = jerry_create_number (6.25);
  number_val = jerry_get_number_value (val_t);
  TEST_ASSERT (number_val * 3 == 18.75);
  jerry_release_value (val_t);

  val_t = jerry_create_number_infinity (true);
  number_val = jerry_get_number_value (val_t);
  TEST_ASSERT (number_val * 3 == number_val && number_val != 0.0);
  jerry_release_value (val_t);

  val_t = jerry_create_number_nan ();
  number_val = jerry_get_number_value (val_t);
  TEST_ASSERT (number_val != number_val);
  jerry_release_value (val_t);

  /* Test: create function */
  const char *func_resource = "unknown";
  const char *func_arg_list = "a , b,c";
  const char *func_src = "  return 5 +  a+\nb+c";

  jerry_value_t func_val = jerry_parse_function ((const jerry_char_t *) func_resource,
                                                 strlen (func_resource),
                                                 (const jerry_char_t *) func_arg_list,
                                                 strlen (func_arg_list),
                                                 (const jerry_char_t *) func_src,
                                                 strlen (func_src),
                                                 JERRY_PARSE_NO_OPTS);

  TEST_ASSERT (!jerry_value_is_error (func_val));

  jerry_value_t func_args[3] =
  {
    jerry_create_number (4),
    jerry_create_number (6),
    jerry_create_number (-2)
  };

  val_t = jerry_call_function (func_val, func_args[0], func_args, 3);
  number_val = jerry_get_number_value (val_t);
  TEST_ASSERT (number_val == 13.0);

  jerry_release_value (val_t);
  jerry_release_value (func_val);

  jerry_cleanup ();

  TEST_ASSERT (test_api_is_free_callback_was_called);

  /* Test: jerry_get_value_from_error */
  {
    jerry_init (JERRY_INIT_DEFAULT);
    jerry_value_t num_val = jerry_create_number (123);
    num_val = jerry_create_error_from_value (num_val, true);
    TEST_ASSERT (jerry_value_is_error (num_val));
    jerry_value_t num2_val = jerry_get_value_from_error (num_val, false);
    TEST_ASSERT (jerry_value_is_error (num_val));
    TEST_ASSERT (!jerry_value_is_error (num2_val));
    double num = jerry_get_number_value (num2_val);
    TEST_ASSERT (num == 123);
    num2_val = jerry_get_value_from_error (num_val, true);
    TEST_ASSERT (!jerry_value_is_error (num2_val));
    num = jerry_get_number_value (num2_val);
    TEST_ASSERT (num == 123);
    jerry_release_value (num2_val);
    jerry_cleanup ();
  }

  /* Test: parser error location */
  if (jerry_is_feature_enabled (JERRY_FEATURE_ERROR_MESSAGES))
  {
    jerry_init (JERRY_INIT_FLAGS (JERRY_INIT_SHOW_OPCODES));

    const char *parser_err_src_p = "b = 'hello';\nvar a = (;";
    parsed_code_val = jerry_parse (NULL,
                                   0,
                                   (jerry_char_t *) parser_err_src_p,
                                   strlen (parser_err_src_p),
                                   JERRY_PARSE_NO_OPTS);
    TEST_ASSERT (jerry_value_is_error (parsed_code_val));
    parsed_code_val = jerry_get_value_from_error (parsed_code_val, true);
    jerry_value_t err_str_val = jerry_value_to_string (parsed_code_val);
    jerry_size_t err_str_size = jerry_get_string_size (err_str_val);
    jerry_char_t err_str_buf[256];
    sz = jerry_string_to_char_buffer (err_str_val, err_str_buf, err_str_size);
    err_str_buf[sz] = 0;

    jerry_release_value (err_str_val);
    jerry_release_value (parsed_code_val);
    TEST_ASSERT (!strcmp ((char *) err_str_buf,
                          "SyntaxError: Primary expression expected. [line: 2, column: 10]"));

    jerry_cleanup ();
  }

  /* External Magic String */
  jerry_init (JERRY_INIT_FLAGS (JERRY_INIT_SHOW_OPCODES));

  uint32_t num_magic_string_items = (uint32_t) (sizeof (magic_string_items) / sizeof (jerry_char_t *));
  jerry_register_magic_strings (magic_string_items,
                                num_magic_string_items,
                                magic_string_lengths);

  const char *ms_code_src_p = "var global = {}; var console = [1]; var process = 1;";
  parsed_code_val = jerry_parse (NULL,
                                 0,
                                 (jerry_char_t *) ms_code_src_p,
                                 strlen (ms_code_src_p),
                                 JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (!jerry_value_is_error (parsed_code_val));

  res = jerry_run (parsed_code_val);
  TEST_ASSERT (!jerry_value_is_error (res));
  jerry_release_value (res);
  jerry_release_value (parsed_code_val);

  /* call jerry_create_string functions which will returns with the registered external magic strings */
  args[0] = jerry_create_string ((jerry_char_t *) "console");
  args[1] = jerry_create_string ((jerry_char_t *) "\xed\xa0\x80\xed\xb6\x8a"); /**< greek zero sign */

  cesu8_length = jerry_get_string_length (args[0]);
  cesu8_sz = jerry_get_string_size (args[0]);

  char string_console[cesu8_sz];
  jerry_string_to_char_buffer (args[0], (jerry_char_t *) string_console, cesu8_sz);

  TEST_ASSERT (!strncmp (string_console, "console", cesu8_sz));
  TEST_ASSERT (cesu8_length == 7);
  TEST_ASSERT (cesu8_length == cesu8_sz);

  jerry_release_value (args[0]);

  const char *test_magic_str_access_src_p = "'console'.charAt(6) == 'e'";
  res = jerry_eval ((const jerry_char_t *) test_magic_str_access_src_p,
                    strlen (test_magic_str_access_src_p),
                    JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (jerry_value_is_boolean (res));
  TEST_ASSERT (jerry_get_boolean_value (res) == true);

  jerry_release_value (res);

  cesu8_length = jerry_get_string_length (args[1]);
  cesu8_sz = jerry_get_string_size (args[1]);

  char string_greek_zero_sign[cesu8_sz];
  jerry_string_to_char_buffer (args[1], (jerry_char_t *) string_greek_zero_sign, cesu8_sz);

  TEST_ASSERT (!strncmp (string_greek_zero_sign, "\xed\xa0\x80\xed\xb6\x8a", cesu8_sz));
  TEST_ASSERT (cesu8_length == 2);
  TEST_ASSERT (cesu8_sz == 6);

  jerry_release_value (args[1]);

  {
    /*json parser check*/
    char data_check[]="John";
    jerry_value_t key = jerry_create_string ((const jerry_char_t *) "name");
    const char *data = "{\"name\": \"John\", \"age\": 5}";
    jerry_size_t str_length = (jerry_size_t) strlen (data);
    jerry_value_t parsed_json = jerry_json_parse ((jerry_char_t *) data, str_length);
    jerry_value_t has_prop_js = jerry_has_property (parsed_json, key);
    TEST_ASSERT (jerry_get_boolean_value (has_prop_js));
    jerry_release_value (has_prop_js);
    jerry_value_t parsed_data = jerry_get_property (parsed_json, key);
    TEST_ASSERT (jerry_value_is_string (parsed_data)== true);
    jerry_size_t buff_size = (jerry_size_t) jerry_get_string_length (parsed_data);
    char buff[jerry_get_string_length (parsed_data)];
    jerry_char_t *buff_p = (jerry_char_t *) buff;
    jerry_string_to_char_buffer (parsed_data, buff_p, buff_size);
    buff[buff_size] = '\0';
    TEST_ASSERT (strcmp ((const char *) data_check, (const char *) buff) == false);
    jerry_release_value (parsed_json);
    jerry_release_value (key);
    jerry_release_value (parsed_data);
  }

  /*json stringify test*/
  {
    jerry_value_t obj = jerry_create_object ();
    char check_value[] = "{\"name\":\"John\"}";
    jerry_value_t key = jerry_create_string ((const jerry_char_t *) "name");
    jerry_value_t value = jerry_create_string ((const jerry_char_t *) "John");
    jerry_set_property (obj, key, value);
    jerry_value_t stringified = jerry_json_stringfy (obj);
    TEST_ASSERT (jerry_value_is_string (stringified));
    char buff[jerry_get_string_length (stringified)];
    jerry_string_to_char_buffer (stringified, (jerry_char_t *) buff,
                                (jerry_size_t) jerry_get_string_length (stringified));
    buff[jerry_get_string_length (stringified)] = '\0';
    TEST_ASSERT (strcmp ((const char *) check_value, (const char *) buff)  == 0);
    jerry_release_value (stringified);
    jerry_release_value (obj);
    jerry_release_value (key);
    jerry_release_value (value);
  }
  jerry_cleanup ();

  return 0;
} /* main */

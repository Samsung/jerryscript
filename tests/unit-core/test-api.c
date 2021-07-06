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

const jerry_char_t test_source[] = TEST_STRING_LITERAL (
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
handler (const jerry_call_info_t *call_info_p, /**< call information */
         const jerry_value_t args_p[], /**< arguments list */
         const jerry_length_t args_cnt) /**< arguments length */
{
  char buffer[32];
  jerry_size_t sz;

  printf ("ok %u %u %p %u\n",
          (unsigned int) call_info_p->function,
          (unsigned int) call_info_p->this_value,
          (void *) args_p,
          (unsigned int) args_cnt);

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
handler_throw_test (const jerry_call_info_t *call_info_p, /**< call information */
                    const jerry_value_t args_p[], /**< arguments list */
                    const jerry_length_t args_cnt) /**< arguments length */
{
  printf ("ok %u %u %p %u\n",
          (unsigned int) call_info_p->function,
          (unsigned int) call_info_p->this_value,
          (void *) args_p,
          (unsigned int) args_cnt);

  return jerry_create_error (JERRY_ERROR_TYPE, (jerry_char_t *) "error");
} /* handler_throw_test */

static void
handler_construct_1_freecb (void *native_p, /**< native pointer */
                            jerry_object_native_info_t *info_p) /**< native info */
{
  TEST_ASSERT ((uintptr_t) native_p == (uintptr_t) 0x0000000000000000ull);
  TEST_ASSERT (info_p->free_cb == handler_construct_1_freecb);
  printf ("ok object free callback\n");

  test_api_is_free_callback_was_called = true;
} /* handler_construct_1_freecb */

static void
handler_construct_2_freecb (void *native_p, /**< native pointer */
                            jerry_object_native_info_t *info_p) /**< native info */
{
  TEST_ASSERT ((uintptr_t) native_p == (uintptr_t) 0x0012345678abcdefull);
  TEST_ASSERT (info_p->free_cb == handler_construct_2_freecb);
  printf ("ok object free callback\n");

  test_api_is_free_callback_was_called = true;
} /* handler_construct_2_freecb */

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
    .free_cb = (jerry_object_native_free_callback_t) native_free_cb, \
    .number_of_references = 0, \
    .offset_of_references = 0, \
  }

JERRY_DEFINE_NATIVE_HANDLE_INFO (bind1, handler_construct_1_freecb);
JERRY_DEFINE_NATIVE_HANDLE_INFO (bind2, handler_construct_2_freecb);
JERRY_DEFINE_NATIVE_HANDLE_INFO (bind3, NULL);

static jerry_value_t
handler_construct (const jerry_call_info_t *call_info_p, /**< call information */
                   const jerry_value_t args_p[], /**< arguments list */
                   const jerry_length_t args_cnt) /**< arguments length */
{
  printf ("ok construct %u %u %p %u\n",
          (unsigned int) call_info_p->function,
          (unsigned int) call_info_p->this_value,
          (void *) args_p,
          (unsigned int) args_cnt);

  TEST_ASSERT (jerry_value_is_object (call_info_p->this_value));

  TEST_ASSERT (args_cnt == 1);
  TEST_ASSERT (jerry_value_is_boolean (args_p[0]));
  TEST_ASSERT (jerry_value_is_true (args_p[0]));

  jerry_value_t this_value = call_info_p->this_value;
  jerry_value_t field_name = jerry_create_string ((jerry_char_t *) "value_field");
  jerry_value_t res = jerry_set_property (this_value, field_name, args_p[0]);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_true (res));
  jerry_release_value (res);
  jerry_release_value (field_name);

  /* Set a native pointer. */
  jerry_set_object_native_pointer (this_value,
                                   (void *) 0x0000000000000000ull,
                                   &JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (bind1));

  /* Check that the native pointer was set. */
  void *ptr = NULL;
  bool is_ok = jerry_get_object_native_pointer (this_value, &ptr, &JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (bind1));
  TEST_ASSERT (is_ok
               && (uintptr_t) ptr == (uintptr_t) 0x0000000000000000ull);

  /* Set a second native pointer. */
  jerry_set_object_native_pointer (this_value,
                                   (void *) 0x0012345678abcdefull,
                                   &JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (bind2));

  /* Check that a second native pointer was set. */
  is_ok = jerry_get_object_native_pointer (this_value, &ptr, &JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (bind2));
  TEST_ASSERT (is_ok
               && (uintptr_t) ptr == (uintptr_t) 0x0012345678abcdefull);

  /* Check that the first native pointer is still set. */
  is_ok = jerry_get_object_native_pointer (this_value, &ptr, &JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (bind1));
  TEST_ASSERT (is_ok
               && (uintptr_t) ptr == (uintptr_t) 0x0000000000000000ull);
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
    TEST_ASSERT (jerry_value_is_true (value) == false);
    TEST_ASSERT (jerry_value_is_false (value));
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

  return jerry_run_simple ((const jerry_char_t *) script_p, script_size, JERRY_INIT_EMPTY);
} /* test_run_simple */

static void
test_syntax_error (const char *script_p, /**< source code to run */
                   const jerry_parse_options_t *options_p, /**< additional parsing options */
                   const char *error_message_p, /**< error message */
                   bool run_script) /**< run script before checking the error message */
{
  jerry_value_t result_val = jerry_parse ((const jerry_char_t *) script_p,
                                          strlen (script_p),
                                          options_p);

  if (run_script)
  {
    TEST_ASSERT (!jerry_value_is_error (result_val));
    jerry_value_t script_val = result_val;

    result_val = jerry_run (script_val);
    jerry_release_value (script_val);
  }

  TEST_ASSERT (jerry_value_is_error (result_val));
  result_val = jerry_get_value_from_error (result_val, true);

  jerry_value_t err_str_val = jerry_value_to_string (result_val);
  jerry_size_t err_str_size = jerry_get_string_size (err_str_val);
  jerry_char_t err_str_buf[256];

  TEST_ASSERT (err_str_size <= sizeof (err_str_buf));
  TEST_ASSERT (err_str_size == strlen (error_message_p));

  TEST_ASSERT (jerry_string_to_char_buffer (err_str_val, err_str_buf, err_str_size) == err_str_size);

  jerry_release_value (err_str_val);
  jerry_release_value (result_val);
  TEST_ASSERT (memcmp ((char *) err_str_buf, error_message_p, err_str_size) == 0);
} /* test_syntax_error */

int
main (void)
{
  TEST_INIT ();

  bool is_ok;
  jerry_size_t sz, cesu8_sz;
  jerry_length_t cesu8_length;
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

  jerry_init (JERRY_INIT_EMPTY);

  parsed_code_val = jerry_parse (test_source,
                                 sizeof (test_source) - 1,
                                 NULL);
  TEST_ASSERT (!jerry_value_is_error (parsed_code_val));

  res = jerry_run (parsed_code_val);
  TEST_ASSERT (!jerry_value_is_error (res));
  jerry_release_value (res);
  jerry_release_value (parsed_code_val);

  global_obj_val = jerry_get_global_object ();

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
  TEST_ASSERT (jerry_value_is_true (res));
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
  TEST_ASSERT (jerry_value_is_true (res));
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
  TEST_ASSERT (jerry_value_is_true (res));
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
  TEST_ASSERT (jerry_value_is_true (res));
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
               && jerry_value_is_true (val_value_field));
  jerry_release_value (val_value_field);
  jerry_release_value (external_construct_val);

  void *ptr = NULL;
  is_ok = jerry_get_object_native_pointer (res, &ptr, &JERRY_NATIVE_HANDLE_INFO_FOR_CTYPE (bind2));
  TEST_ASSERT (is_ok
               && (uintptr_t) ptr == (uintptr_t) 0x0012345678abcdefull);

  /* Passing NULL for info_p is allowed. */
  is_ok = jerry_get_object_native_pointer (res, &ptr, NULL);
  TEST_ASSERT (!is_ok);

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
  TEST_ASSERT (jerry_value_is_true (res));
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
  res = jerry_set_property_by_index (array_obj_val, 5, v_in);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_boolean (res) && jerry_value_is_true (res));
  jerry_release_value (res);
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

  /* Test: object keys */
  res = jerry_get_object_keys (global_obj_val);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_array (res));
  TEST_ASSERT (jerry_get_array_length (res) == 15);
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
  proto_val = jerry_get_prototype (jerry_create_undefined ());
  TEST_ASSERT (jerry_value_is_error (proto_val));
  jerry_value_t error = jerry_get_value_from_error (proto_val, true);
  TEST_ASSERT (jerry_get_error_type (error) == JERRY_ERROR_TYPE);
  jerry_release_value (error);

  proto_val = jerry_get_prototype (obj_val);
  TEST_ASSERT (!jerry_value_is_error (proto_val));
  TEST_ASSERT (jerry_value_is_object (proto_val));
  jerry_release_value (proto_val);
  jerry_release_value (obj_val);

  if (jerry_is_feature_enabled (JERRY_FEATURE_PROXY))
  {
    jerry_value_t target = jerry_create_object ();
    jerry_value_t handler = jerry_create_object ();
    jerry_value_t proxy = jerry_create_proxy (target, handler);
    jerry_value_t obj_proto = jerry_eval ((jerry_char_t *) "Object.prototype", 16, JERRY_PARSE_NO_OPTS);

    jerry_release_value (target);
    jerry_release_value (handler);
    proto_val = jerry_get_prototype (proxy);
    TEST_ASSERT (!jerry_value_is_error (proto_val));
    TEST_ASSERT (proto_val == obj_proto);
    jerry_release_value (proto_val);
    jerry_release_value (obj_proto);
    jerry_release_value (proxy);
  }

  /* Test: jerry_set_prototype */
  obj_val = jerry_create_object ();
  res = jerry_set_prototype (obj_val, jerry_create_null ());
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_boolean (res));
  TEST_ASSERT (jerry_value_is_true (res));

  jerry_value_t new_proto = jerry_create_object ();
  res = jerry_set_prototype (obj_val, new_proto);
  jerry_release_value (new_proto);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_boolean (res));
  TEST_ASSERT (jerry_value_is_true (res));
  proto_val = jerry_get_prototype (obj_val);
  TEST_ASSERT (!jerry_value_is_error (proto_val));
  TEST_ASSERT (jerry_value_is_object (proto_val));
  jerry_release_value (proto_val);
  jerry_release_value (obj_val);

  if (jerry_is_feature_enabled (JERRY_FEATURE_PROXY))
  {
    jerry_value_t target = jerry_create_object ();
    jerry_value_t handler = jerry_create_object ();
    jerry_value_t proxy = jerry_create_proxy (target, handler);
    new_proto = jerry_eval ((jerry_char_t *) "Function.prototype", 18, JERRY_PARSE_NO_OPTS);

    res = jerry_set_prototype (proxy, new_proto);
    TEST_ASSERT (!jerry_value_is_error (res));
    jerry_value_t target_proto = jerry_get_prototype (target);
    TEST_ASSERT (target_proto == new_proto);

    jerry_release_value (target);
    jerry_release_value (handler);
    jerry_release_value (proxy);
    jerry_release_value (new_proto);
    jerry_release_value (target_proto);
  }

  /* Test: eval */
  const jerry_char_t eval_code_src1[] = "(function () { return 123; })";
  val_t = jerry_eval (eval_code_src1, sizeof (eval_code_src1) - 1, JERRY_PARSE_STRICT_MODE);
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
  jerry_gc (JERRY_GC_PRESSURE_LOW);

  /* Test: spaces */
  const jerry_char_t eval_code_src2[] = "\x0a \x0b \x0c \xc2\xa0 \xe2\x80\xa8 \xe2\x80\xa9 \xef\xbb\xbf 4321";
  val_t = jerry_eval (eval_code_src2, sizeof (eval_code_src2) - 1, JERRY_PARSE_STRICT_MODE);
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
  const jerry_char_t func_arg_list[] = "a , b,c";
  const jerry_char_t func_src[] = "  return 5 +  a+\nb+c";

  jerry_value_t func_val = jerry_parse_function (func_arg_list,
                                                 sizeof (func_arg_list) - 1,
                                                 func_src,
                                                 sizeof (func_src) - 1,
                                                 NULL);

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
    jerry_init (JERRY_INIT_EMPTY);
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

  /* Test parsing/executing scripts with lexically scoped global variables multiple times. */
  if (jerry_is_feature_enabled (JERRY_FEATURE_SYMBOL))
  {
    jerry_init (JERRY_INIT_EMPTY);
    const jerry_char_t scoped_src_p[] = "let a; this.b = 5";
    jerry_value_t parse_result = jerry_parse (scoped_src_p,
                                              sizeof (scoped_src_p) - 1,
                                              NULL);
    TEST_ASSERT (!jerry_value_is_error (parse_result));
    jerry_release_value (parse_result);

    parse_result = jerry_parse (scoped_src_p,
                                sizeof (scoped_src_p) - 1,
                                NULL);
    TEST_ASSERT (!jerry_value_is_error (parse_result));

    jerry_value_t run_result = jerry_run (parse_result);
    TEST_ASSERT (!jerry_value_is_error (run_result));
    jerry_release_value (run_result);

    /* Should be a syntax error due to redeclaration. */
    run_result = jerry_run (parse_result);
    TEST_ASSERT (jerry_value_is_error (run_result));
    jerry_release_value (run_result);
    jerry_release_value (parse_result);

    /* The variable should have no effect on parsing. */
    parse_result = jerry_parse (scoped_src_p,
                                sizeof (scoped_src_p) - 1,
                                NULL);
    TEST_ASSERT (!jerry_value_is_error (parse_result));
    jerry_release_value (parse_result);

    /* The already existing global binding should not affect a new lexical binding */
    const jerry_char_t scoped_src2_p[] = "let b = 6; this.b + b";
    parse_result = jerry_parse (scoped_src2_p,
                                sizeof (scoped_src2_p) - 1,
                                NULL);
    TEST_ASSERT (!jerry_value_is_error (parse_result));
    run_result = jerry_run (parse_result);
    TEST_ASSERT (jerry_value_is_number (run_result));
    TEST_ASSERT (jerry_get_number_value (run_result) == 11);
    jerry_release_value (run_result);
    jerry_release_value (parse_result);

    /* Check restricted global property */
    const jerry_char_t scoped_src3_p[] = "let undefined;";
    parse_result = jerry_parse (scoped_src3_p,
                                sizeof (scoped_src3_p) - 1,
                                NULL);
    TEST_ASSERT (!jerry_value_is_error (parse_result));
    run_result = jerry_run (parse_result);
    TEST_ASSERT (jerry_value_is_error (run_result));
    TEST_ASSERT (jerry_get_error_type (run_result) == JERRY_ERROR_SYNTAX);
    jerry_release_value (run_result);
    jerry_release_value (parse_result);

    jerry_value_t global_obj = jerry_get_global_object ();
    jerry_value_t prop_name = jerry_create_string ((const jerry_char_t *) "foo");

    jerry_property_descriptor_t prop_desc = jerry_property_descriptor_create ();
    prop_desc.flags |= JERRY_PROP_IS_VALUE_DEFINED;
    prop_desc.value = jerry_create_number (5.2);

    jerry_value_t define_result = jerry_define_own_property (global_obj, prop_name, &prop_desc);
    TEST_ASSERT (jerry_value_is_boolean (define_result) && jerry_value_is_true (define_result));
    jerry_release_value (define_result);

    jerry_property_descriptor_free (&prop_desc);
    jerry_release_value (prop_name);
    jerry_release_value (global_obj);

    const jerry_char_t scoped_src4_p[] = "let foo;";
    parse_result = jerry_parse (scoped_src4_p,
                                sizeof (scoped_src4_p) - 1,
                                NULL);
    TEST_ASSERT (!jerry_value_is_error (parse_result));
    run_result = jerry_run (parse_result);
    TEST_ASSERT (jerry_value_is_error (run_result));
    TEST_ASSERT (jerry_get_error_type (run_result) == JERRY_ERROR_SYNTAX);
    jerry_release_value (run_result);
    jerry_release_value (parse_result);

    if (jerry_is_feature_enabled (JERRY_FEATURE_REALM))
    {
      const jerry_char_t proxy_src_p[] = "new Proxy({}, { getOwnPropertyDescriptor() { throw 42.1 }})";
      jerry_value_t proxy = jerry_eval (proxy_src_p, sizeof (proxy_src_p) - 1, JERRY_PARSE_NO_OPTS);
      TEST_ASSERT (jerry_value_is_object (proxy));
      jerry_value_t new_realm_value = jerry_create_realm ();

      jerry_value_t set_realm_this_result = jerry_realm_set_this (new_realm_value, proxy);
      TEST_ASSERT (jerry_value_is_boolean (set_realm_this_result) && jerry_value_is_true (set_realm_this_result));
      jerry_release_value (set_realm_this_result);

      jerry_value_t old_realm = jerry_set_realm (new_realm_value);

      const jerry_char_t scoped_src5_p[] = "let a;";
      parse_result = jerry_parse (scoped_src5_p,
                                  sizeof (scoped_src5_p) - 1,
                                  NULL);
      TEST_ASSERT (!jerry_value_is_error (parse_result));
      run_result = jerry_run (parse_result);
      TEST_ASSERT (jerry_value_is_error (run_result));
      jerry_value_t error_value = jerry_get_value_from_error (run_result, false);
      TEST_ASSERT (jerry_value_is_number (error_value) && jerry_get_number_value (error_value) == 42.1);
      jerry_release_value (error_value);
      jerry_release_value (run_result);
      jerry_release_value (parse_result);

      jerry_set_realm (old_realm);

      jerry_release_value (new_realm_value);
      jerry_release_value (proxy);

      const jerry_char_t proxy_src2_p[] = "new Proxy(Object.defineProperty({}, 'b', {value: 5.2}), {})";
      proxy = jerry_eval (proxy_src2_p, sizeof (proxy_src2_p) - 1, JERRY_PARSE_NO_OPTS);
      TEST_ASSERT (jerry_value_is_object (proxy));
      new_realm_value = jerry_create_realm ();

      set_realm_this_result = jerry_realm_set_this (new_realm_value, proxy);
      TEST_ASSERT (jerry_value_is_boolean (set_realm_this_result) && jerry_value_is_true (set_realm_this_result));
      jerry_release_value (set_realm_this_result);

      old_realm = jerry_set_realm (new_realm_value);

      const jerry_char_t scoped_src6_p[] = "let b;";
      parse_result = jerry_parse (scoped_src6_p,
                                  sizeof (scoped_src6_p) - 1,
                                  NULL);
      TEST_ASSERT (!jerry_value_is_error (parse_result));
      run_result = jerry_run (parse_result);
      TEST_ASSERT (jerry_value_is_error (run_result));
      TEST_ASSERT (jerry_value_is_error (run_result));
      TEST_ASSERT (jerry_get_error_type (run_result) == JERRY_ERROR_SYNTAX);
      jerry_release_value (run_result);
      jerry_release_value (parse_result);

      jerry_set_realm (old_realm);

      jerry_release_value (new_realm_value);
      jerry_release_value (proxy);
    }

    jerry_cleanup ();
  }

  /* Test: parser error location */
  if (jerry_is_feature_enabled (JERRY_FEATURE_ERROR_MESSAGES))
  {
    jerry_init (JERRY_INIT_SHOW_OPCODES);

    test_syntax_error ("b = 'hello';\nvar a = (;",
                       NULL,
                       "SyntaxError: Primary expression expected [<anonymous>:2:10]",
                       false);

    const jerry_char_t file_str[] = "filename.js";
    jerry_parse_options_t parse_options;
    parse_options.options = JERRY_PARSE_HAS_RESOURCE;
    parse_options.resource_name_p = file_str;
    parse_options.resource_name_length = sizeof (file_str) - 1;

    test_syntax_error ("b = 'hello';\nvar a = (;",
                       &parse_options,
                       "SyntaxError: Primary expression expected [filename.js:2:10]",
                       false);

    test_syntax_error ("eval(\"var b;\\nfor (,); \");",
                       &parse_options,
                       "SyntaxError: Primary expression expected [<eval>:2:6]",
                       true);

    parse_options.options |= JERRY_PARSE_HAS_START;
    parse_options.start_line = 10;
    parse_options.start_column = 20;

    test_syntax_error ("for (var a in []",
                       &parse_options,
                       "SyntaxError: Expected ')' token [filename.js:10:36]",
                       false);

    jerry_cleanup ();
  }

  /* External Magic String */
  jerry_init (JERRY_INIT_SHOW_OPCODES);

  uint32_t num_magic_string_items = (uint32_t) (sizeof (magic_string_items) / sizeof (jerry_char_t *));
  jerry_register_magic_strings (magic_string_items,
                                num_magic_string_items,
                                magic_string_lengths);

  const jerry_char_t ms_code_src[] = "var global = {}; var console = [1]; var process = 1;";
  parsed_code_val = jerry_parse (ms_code_src,
                                 sizeof (ms_code_src) - 1,
                                 NULL);
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

  JERRY_VLA (char, string_console, cesu8_sz);
  jerry_string_to_char_buffer (args[0], (jerry_char_t *) string_console, cesu8_sz);

  TEST_ASSERT (!strncmp (string_console, "console", cesu8_sz));
  TEST_ASSERT (cesu8_length == 7);
  TEST_ASSERT (cesu8_length == cesu8_sz);

  jerry_release_value (args[0]);

  const jerry_char_t test_magic_str_access_src[] = "'console'.charAt(6) == 'e'";
  res = jerry_eval (test_magic_str_access_src,
                    sizeof (test_magic_str_access_src) - 1,
                    JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (jerry_value_is_boolean (res));
  TEST_ASSERT (jerry_value_is_true (res));

  jerry_release_value (res);

  cesu8_length = jerry_get_string_length (args[1]);
  cesu8_sz = jerry_get_string_size (args[1]);

  JERRY_VLA (char, string_greek_zero_sign, cesu8_sz);
  jerry_string_to_char_buffer (args[1], (jerry_char_t *) string_greek_zero_sign, cesu8_sz);

  TEST_ASSERT (!strncmp (string_greek_zero_sign, "\xed\xa0\x80\xed\xb6\x8a", cesu8_sz));
  TEST_ASSERT (cesu8_length == 2);
  TEST_ASSERT (cesu8_sz == 6);

  jerry_release_value (args[1]);

  jerry_cleanup ();

  return 0;
} /* main */

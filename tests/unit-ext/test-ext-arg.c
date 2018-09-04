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

/**
 * Unit test for jerry-ext/args.
 */

#include "jerryscript.h"
#include "jerryscript-ext/arg.h"
#include "test-common.h"

#include <string.h>
#include <jerryscript-ext/arg.h>

const char *test_source = (
                           "var arg1 = true;"
                           "var arg2 = 10.5;"
                           "var arg3 = 'abc';"
                           "var arg4 = function foo() {};"
                           "test_validator1(arg1, arg2, arg3, arg4);"
                           "arg1 = new Boolean(true);"
                           "arg3 = new String('abc');"
                           "test_validator1(arg1, arg2, arg3);"
                           "test_validator1(arg1, arg2, '');"
                           "arg2 = new Number(10.5);"
                           "test_validator1(arg1, arg2, arg3);"
                           "test_validator1(arg1, 10.5, 'abcdef');"
                           "var obj_a = new MyObjectA();"
                           "var obj_b = new MyObjectB();"
                           "test_validator2.call(obj_a, 5);"
                           "test_validator2.call(obj_b, 5);"
                           "test_validator2.call(obj_a, 1);"
                           "var obj1 = {prop1:true, prop2:'1.5'};"
                           "test_validator_prop1(obj1);"
                           "test_validator_prop2(obj1);"
                           "test_validator_prop2();"
                           "var obj2 = {prop1:true};"
                           "Object.defineProperty(obj2, 'prop2', {"
                           "  get: function() { throw new TypeError('prop2 error') }"
                           "});"
                           "test_validator_prop3(obj2);"
                           "test_validator_int1(-1000, 1000, 128, -1000, 1000, -127,"
                           "                    -1000, 4294967297, 65536, -2200000000, 4294967297, -2147483647);"
                           "test_validator_int2(-1.5, -1.5, -1.5, 1.5, 1.5, 1.5, Infinity, -Infinity, 300.5, 300.5);"
                           "test_validator_int3(NaN);"
                           "var arr = [1, 2];"
                           "test_validator_array1(arr);"
                           "test_validator_array1();"
                           "test_validator_array2(arr);"
                           );

static const jerry_object_native_info_t thing_a_info =
{
  .free_cb = NULL
};

static const jerry_object_native_info_t thing_b_info =
{
  .free_cb = NULL
};

typedef struct
{
  int x;
} my_type_a_t;

typedef struct
{
  bool x;
} my_type_b_t;

static my_type_a_t my_thing_a;
static my_type_b_t my_thing_b;

static int validator1_count = 0;
static int validator2_count = 0;
static int validator_int_count = 0;
static int validator_prop_count = 0;
static int validator_array_count = 0;

/**
 * The handler should have following arguments:
 *   this: Ignore.
 *   arg1: Bool.
 *   arg2: Number. It must be strict primitive number.
 *   arg3: String.
 *   arg4: function. It is an optional argument.
 *
 */
static jerry_value_t
test_validator1_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                         const jerry_value_t this_val, /**< this value */
                         const jerry_value_t args_p[], /**< arguments list */
                         const jerry_length_t args_cnt) /**< arguments length */
{

  bool arg1;
  double arg2 = 0.0;
  char arg3[5] = "1234";
  jerry_value_t arg4 = jerry_create_undefined ();

  jerryx_arg_t mapping[] =
  {
    /* ignore this */
    jerryx_arg_ignore (),
    /* 1st argument should be boolean */
    jerryx_arg_boolean (&arg1, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    /* 2nd argument should be strict number */
    jerryx_arg_number (&arg2, JERRYX_ARG_NO_COERCE, JERRYX_ARG_REQUIRED),
    /* 3th argument should be string */
    jerryx_arg_string (arg3, 5, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    /* 4th argument should be function, and it is optional */
    jerryx_arg_function (&arg4, JERRYX_ARG_OPTIONAL)
  };

  jerry_value_t is_ok = jerryx_arg_transform_this_and_args (this_val,
                                                            args_p,
                                                            args_cnt,
                                                            mapping,
                                                            ARRAY_SIZE (mapping));

  if (validator1_count == 0)
  {
    TEST_ASSERT (!jerry_value_is_error (is_ok));
    TEST_ASSERT (arg1);
    TEST_ASSERT (arg2 == 10.5);
    TEST_ASSERT (strcmp (arg3, "abc") == 0);
    TEST_ASSERT (jerry_value_is_function (arg4));
  }
  else if (validator1_count == 1)
  {
    TEST_ASSERT (!jerry_value_is_error (is_ok));
    TEST_ASSERT (arg1);
    TEST_ASSERT (arg2 == 10.5);
    TEST_ASSERT (strcmp (arg3, "abc") == 0);
    TEST_ASSERT (jerry_value_is_undefined (arg4));
  }
  else if (validator1_count == 2)
  {
    TEST_ASSERT (!jerry_value_is_error (is_ok));
    TEST_ASSERT (arg1);
    TEST_ASSERT (arg2 == 10.5);
    TEST_ASSERT (strcmp (arg3, "") == 0);
    TEST_ASSERT (jerry_value_is_undefined (arg4));
  }
  else
  {
    TEST_ASSERT (jerry_value_is_error (is_ok));
  }

  jerry_release_value (is_ok);
  jerry_release_value (arg4);
  validator1_count++;

  return jerry_create_undefined ();
} /* test_validator1_handler */

/**
 * The JS argument should be number, whose value is equal with the extra_info .
 */
static jerry_value_t
my_custom_transform (jerryx_arg_js_iterator_t *js_arg_iter_p, /**< available JS args */
                     const jerryx_arg_t *c_arg_p) /**< the native arg */
{
  jerry_value_t js_arg = jerryx_arg_js_iterator_pop (js_arg_iter_p);
  jerry_value_t to_number = jerry_value_to_number (js_arg);

  if (jerry_value_is_error (to_number))
  {
    jerry_release_value (to_number);

    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "It can not be converted to a number.");
  }

  int expected_num = (int) c_arg_p->extra_info;
  int get_num = (int) jerry_get_number_value (to_number);

  if (get_num != expected_num)
  {
    return jerry_create_error (JERRY_ERROR_TYPE,
                               (jerry_char_t *) "Number value is not expected.");
  }

  return jerry_create_undefined ();
} /* my_custom_transform */

/**
 * The handler should have following arguments:
 *   this: with native pointer whose type is bind_a_info.
 *   arg1: should pass the custom tranform function.
 */
static jerry_value_t
test_validator2_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                         const jerry_value_t this_val, /**< this value */
                         const jerry_value_t args_p[], /**< arguments list */
                         const jerry_length_t args_cnt) /**< arguments length */
{
  my_type_a_t *thing_p;

  jerryx_arg_t mapping[] =
  {
    /* this should has native pointer, whose type is thing_a_info */
    jerryx_arg_native_pointer ((void **) &thing_p, &thing_a_info, JERRYX_ARG_REQUIRED),
    /* custom tranform function */
    jerryx_arg_custom (NULL, 5, my_custom_transform)
  };

  jerry_value_t is_ok = jerryx_arg_transform_this_and_args (this_val,
                                                            args_p,
                                                            args_cnt,
                                                            mapping,
                                                            ARRAY_SIZE (mapping));

  if (validator2_count == 0)
  {
    TEST_ASSERT (!jerry_value_is_error (is_ok));
    TEST_ASSERT (thing_p == &my_thing_a);
    TEST_ASSERT (thing_p->x == 1);
  }
  else
  {
    TEST_ASSERT (jerry_value_is_error (is_ok));
  }

  jerry_release_value (is_ok);
  validator2_count++;

  return jerry_create_undefined ();
} /* test_validator2_handler */

/**
 * Calling jerryx_arg_transform_object_properties directly.
 */
static jerry_value_t
test_validator_prop1_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                              const jerry_value_t this_val __attribute__((unused)), /**< this value */
                              const jerry_value_t args_p[], /**< arguments list */
                              const jerry_length_t args_cnt __attribute__((unused))) /**< arguments length */
{
  bool native1 = false;
  double native2 = 0;
  double native3 = 3;

  const char *name_p[] = {"prop1", "prop2", "prop3"};

  jerryx_arg_t mapping[] =
  {
    jerryx_arg_boolean (&native1, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_number (&native2, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_number (&native3, JERRYX_ARG_COERCE, JERRYX_ARG_OPTIONAL)
  };

  jerry_value_t is_ok = jerryx_arg_transform_object_properties (args_p[0],
                                                                (const jerry_char_t **) name_p,
                                                                ARRAY_SIZE (name_p),
                                                                mapping,
                                                                ARRAY_SIZE (mapping));

  TEST_ASSERT (!jerry_value_is_error (is_ok));
  TEST_ASSERT (native1);
  TEST_ASSERT (native2 == 1.5);
  TEST_ASSERT (native3 == 3);

  validator_prop_count++;

  return jerry_create_undefined ();
} /* test_validator_prop1_handler */

/**
 * Calling jerryx_arg_transform_object_properties indirectly by
 * using jerryx_arg_object_properties.
 */
static jerry_value_t
test_validator_prop2_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                              const jerry_value_t this_val __attribute__((unused)), /**< this value */
                              const jerry_value_t args_p[], /**< arguments list */
                              const jerry_length_t args_cnt) /**< arguments length */
{
  bool native1 = false;
  double native2 = 0;
  double native3 = 3;

  jerryx_arg_object_props_t prop_info;

  const char *name_p[] = { "prop1", "prop2", "prop3" };

  jerryx_arg_t prop_mapping[] =
  {
    jerryx_arg_boolean (&native1, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_number (&native2, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_number (&native3, JERRYX_ARG_COERCE, JERRYX_ARG_OPTIONAL)
  };

  prop_info.name_p = (const jerry_char_t **) name_p;
  prop_info.name_cnt = 3;
  prop_info.c_arg_p = prop_mapping;
  prop_info.c_arg_cnt = 3;

  jerryx_arg_t mapping[] =
  {
    jerryx_arg_object_properties (&prop_info, JERRYX_ARG_OPTIONAL),
  };

  jerry_value_t is_ok = jerryx_arg_transform_args (args_p, args_cnt, mapping, ARRAY_SIZE (mapping));


  TEST_ASSERT (!jerry_value_is_error (is_ok));

  if (validator_prop_count == 1)
  {
    TEST_ASSERT (native1);
    TEST_ASSERT (native2 == 1.5);
    TEST_ASSERT (native3 == 3);
  }

  validator_prop_count++;

  return jerry_create_undefined ();
} /* test_validator_prop2_handler */

static jerry_value_t
test_validator_prop3_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                              const jerry_value_t this_val __attribute__((unused)), /**< this value */
                              const jerry_value_t args_p[], /**< arguments list */
                              const jerry_length_t args_cnt __attribute__((unused))) /**< arguments length */
{
  bool native1 = false;
  bool native2 = true;

  const char *name_p[] = { "prop1", "prop2" };

  jerryx_arg_t mapping[] =
  {
    jerryx_arg_boolean (&native1, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_boolean (&native2, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
  };

  jerry_value_t is_ok = jerryx_arg_transform_object_properties (args_p[0],
                                                                (const jerry_char_t **) name_p,
                                                                ARRAY_SIZE (name_p),
                                                                mapping,
                                                                ARRAY_SIZE (mapping));

  TEST_ASSERT (jerry_value_is_error (is_ok));
  TEST_ASSERT (!native1);
  TEST_ASSERT (native2);

  validator_prop_count++;
  jerry_release_value (is_ok);

  return jerry_create_undefined ();
} /* test_validator_prop3_handler */

/*
 * args_p[0-2] are uint8, args_p[3-5] are int8, args_p[6-8] are uint32, args_p[9-11] are int32.
 */
static jerry_value_t
test_validator_int1_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                             const jerry_value_t this_val __attribute__((unused)), /**< this value */
                             const jerry_value_t args_p[], /**< arguments list */
                             const jerry_length_t args_cnt) /**< arguments length */
{
  uint8_t num0, num1, num2;
  int8_t num3, num4, num5;
  uint32_t num6, num7, num8;
  int32_t num9, num10, num11;

  jerryx_arg_t mapping[] =
  {
    jerryx_arg_uint8 (&num0, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_uint8 (&num1, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_uint8 (&num2, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num3, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num4, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num5, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_uint32 (&num6, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_uint32 (&num7, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_uint32 (&num8, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int32 (&num9, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int32 (&num10, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int32 (&num11, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED)
  };

  jerry_value_t is_ok = jerryx_arg_transform_args (args_p,
                                                   args_cnt,
                                                   mapping,
                                                   ARRAY_SIZE (mapping));

  TEST_ASSERT (!jerry_value_is_error (is_ok));
  TEST_ASSERT (num0 == 0);
  TEST_ASSERT (num1 == 255);
  TEST_ASSERT (num2 == 128);
  TEST_ASSERT (num3 == -128);
  TEST_ASSERT (num4 == 127);
  TEST_ASSERT (num5 == -127);
  TEST_ASSERT (num6 == 0);
  TEST_ASSERT (num7 == 4294967295);
  TEST_ASSERT (num8 == 65536);
  TEST_ASSERT (num9 == -2147483648);
  TEST_ASSERT (num10 == 2147483647);
  TEST_ASSERT (num11 == -2147483647);

  jerry_release_value (is_ok);
  validator_int_count++;

  return jerry_create_undefined ();
} /* test_validator_int1_handler */

static jerry_value_t
test_validator_int2_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                             const jerry_value_t this_val __attribute__((unused)), /**< this value */
                             const jerry_value_t args_p[], /**< arguments list */
                             const jerry_length_t args_cnt) /**< arguments length */
{
  int8_t num0, num1, num2, num3, num4, num5, num6, num7, num8, num9;
  num8 = 123;
  num9 = 123;

  jerryx_arg_t mapping[] =
  {
    jerryx_arg_int8 (&num0, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num1, JERRYX_ARG_FLOOR, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num2, JERRYX_ARG_CEIL, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num3, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num4, JERRYX_ARG_FLOOR, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num5, JERRYX_ARG_CEIL, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num6, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num7, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num8, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_int8 (&num9, JERRYX_ARG_ROUND, JERRYX_ARG_NO_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
  };

  jerry_value_t is_ok = jerryx_arg_transform_args (args_p,
                                                   args_cnt,
                                                   mapping,
                                                   ARRAY_SIZE (mapping));

  TEST_ASSERT (jerry_value_is_error (is_ok));
  TEST_ASSERT (num0 == -2);
  TEST_ASSERT (num1 == -2);
  TEST_ASSERT (num2 == -1);
  TEST_ASSERT (num3 == 2);
  TEST_ASSERT (num4 == 1);
  TEST_ASSERT (num5 == 2);
  TEST_ASSERT (num6 == 127);
  TEST_ASSERT (num7 == -128);
  TEST_ASSERT (num8 == 127);
  TEST_ASSERT (num9 == 123);

  jerry_release_value (is_ok);
  validator_int_count++;

  return jerry_create_undefined ();
} /* test_validator_int2_handler */

static jerry_value_t
test_validator_int3_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                             const jerry_value_t this_val __attribute__((unused)), /**< this value */
                             const jerry_value_t args_p[], /**< arguments list */
                             const jerry_length_t args_cnt) /**< arguments length */
{
  int8_t num0;

  jerryx_arg_t mapping[] =
  {
    jerryx_arg_int8 (&num0, JERRYX_ARG_ROUND, JERRYX_ARG_CLAMP, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
  };

  jerry_value_t is_ok = jerryx_arg_transform_args (args_p,
                                                   args_cnt,
                                                   mapping,
                                                   ARRAY_SIZE (mapping));

  TEST_ASSERT (jerry_value_is_error (is_ok));

  jerry_release_value (is_ok);
  validator_int_count++;

  return jerry_create_undefined ();
} /* test_validator_int3_handler */

static jerry_value_t
test_validator_array1_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                               const jerry_value_t this_val __attribute__((unused)), /**< this value */
                               const jerry_value_t args_p[], /**< arguments list */
                               const jerry_length_t args_cnt) /**< arguments length */
{
  double native1 = 0;
  double native2 = 0;
  double native3 = 0;

  jerryx_arg_array_items_t arr_info;

  jerryx_arg_t item_mapping[] =
  {
    jerryx_arg_number (&native1, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_number (&native2, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_number (&native3, JERRYX_ARG_COERCE, JERRYX_ARG_OPTIONAL)
  };

  arr_info.c_arg_p = item_mapping;
  arr_info.c_arg_cnt = 3;

  jerryx_arg_t mapping[] =
  {
    jerryx_arg_array (&arr_info, JERRYX_ARG_OPTIONAL),
  };

  jerry_value_t is_ok = jerryx_arg_transform_args (args_p, args_cnt, mapping, ARRAY_SIZE (mapping));


  TEST_ASSERT (!jerry_value_is_error (is_ok));

  if (validator_array_count == 0)
  {
    TEST_ASSERT (native1 == 1);
    TEST_ASSERT (native2 == 2);
    TEST_ASSERT (native3 == 0);
  }

  validator_array_count++;

  return jerry_create_undefined ();
} /* test_validator_array1_handler */

static jerry_value_t
test_validator_array2_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                               const jerry_value_t this_val __attribute__((unused)), /**< this value */
                               const jerry_value_t args_p[], /**< arguments list */
                               const jerry_length_t args_cnt __attribute__((unused))) /**< arguments length */
{
  double native1 = 0;
  bool native2 = false;

  jerryx_arg_t item_mapping[] =
  {
    jerryx_arg_number (&native1, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
    jerryx_arg_boolean (&native2, JERRYX_ARG_NO_COERCE, JERRYX_ARG_REQUIRED)
  };

  jerry_value_t is_ok = jerryx_arg_transform_array (args_p[0], item_mapping, ARRAY_SIZE (item_mapping));

  TEST_ASSERT (jerry_value_is_error (is_ok));
  TEST_ASSERT (native1 == 1);
  TEST_ASSERT (!native2);

  validator_array_count++;
  jerry_release_value (is_ok);

  return jerry_create_undefined ();
} /* test_validator_array2_handler */

static void
test_utf8_string (void)
{
  /* test string: 'str: {DESERET CAPITAL LETTER LONG I}' */
  jerry_value_t str = jerry_create_string ((jerry_char_t *) "\x73\x74\x72\x3a \xed\xa0\x81\xed\xb0\x80");
  char expect_utf8_buf[] = "\x73\x74\x72\x3a \xf0\x90\x90\x80";
  size_t buf_len = strlen (expect_utf8_buf);
  char buf[buf_len+1];

  jerryx_arg_t mapping[] =
  {
    jerryx_arg_utf8_string (buf, (uint32_t) buf_len + 1, JERRYX_ARG_COERCE, JERRYX_ARG_REQUIRED),
  };

  jerry_value_t is_ok = jerryx_arg_transform_args (&str,
                                                   1,
                                                   mapping,
                                                   ARRAY_SIZE (mapping));

  TEST_ASSERT (!jerry_value_is_error (is_ok));
  TEST_ASSERT (!strcmp (buf, expect_utf8_buf));

  jerry_release_value (str);
} /* test_utf8_string */

static jerry_value_t
create_object_a_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                         const jerry_value_t this_val, /**< this value */
                         const jerry_value_t args_p[] __attribute__((unused)), /**< arguments list */
                         const jerry_length_t args_cnt __attribute__((unused))) /**< arguments length */
{
  TEST_ASSERT (jerry_value_is_object (this_val));

  my_thing_a.x = 1;
  jerry_set_object_native_pointer (this_val,
                                   &my_thing_a,
                                   &thing_a_info);

  return jerry_create_boolean (true);
} /* create_object_a_handler */

static jerry_value_t
create_object_b_handler (const jerry_value_t func_obj_val __attribute__((unused)), /**< function object */
                         const jerry_value_t this_val, /**< this value */
                         const jerry_value_t args_p[] __attribute__((unused)), /**< arguments list */
                         const jerry_length_t args_cnt __attribute__((unused))) /**< arguments length */
{
  TEST_ASSERT (jerry_value_is_object (this_val));

  my_thing_b.x = false;
  jerry_set_object_native_pointer (this_val,
                                   &my_thing_b,
                                   &thing_b_info);

  return jerry_create_boolean (true);
} /* create_object_b_handler */

/**
 * Register a JavaScript function in the global object.
 */
static void
register_js_function (const char *name_p, /**< name of the function */
                      jerry_external_handler_t handler_p) /**< function callback */
{
  jerry_value_t global_obj_val = jerry_get_global_object ();

  jerry_value_t function_val = jerry_create_external_function (handler_p);
  jerry_value_t function_name_val = jerry_create_string ((const jerry_char_t *) name_p);
  jerry_value_t result_val = jerry_set_property (global_obj_val, function_name_val, function_val);

  jerry_release_value (function_name_val);
  jerry_release_value (function_val);
  jerry_release_value (global_obj_val);

  jerry_release_value (result_val);
} /* register_js_function */

int
main (void)
{
  jerry_init (JERRY_INIT_DEFAULT);

  test_utf8_string ();

  register_js_function ("test_validator1", test_validator1_handler);
  register_js_function ("test_validator2", test_validator2_handler);
  register_js_function ("test_validator_int1", test_validator_int1_handler);
  register_js_function ("test_validator_int2", test_validator_int2_handler);
  register_js_function ("test_validator_int3", test_validator_int3_handler);
  register_js_function ("MyObjectA", create_object_a_handler);
  register_js_function ("MyObjectB", create_object_b_handler);
  register_js_function ("test_validator_prop1", test_validator_prop1_handler);
  register_js_function ("test_validator_prop2", test_validator_prop2_handler);
  register_js_function ("test_validator_prop3", test_validator_prop3_handler);
  register_js_function ("test_validator_array1", test_validator_array1_handler);
  register_js_function ("test_validator_array2", test_validator_array2_handler);

  jerry_value_t parsed_code_val = jerry_parse (NULL,
                                               0,
                                               (jerry_char_t *) test_source,
                                               strlen (test_source),
                                               JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (!jerry_value_is_error (parsed_code_val));

  jerry_value_t res = jerry_run (parsed_code_val);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (validator1_count == 5);
  TEST_ASSERT (validator2_count == 3);
  TEST_ASSERT (validator_prop_count == 4);
  TEST_ASSERT (validator_int_count == 3);
  TEST_ASSERT (validator_array_count == 3);

  jerry_release_value (res);
  jerry_release_value (parsed_code_val);

  jerry_cleanup ();
} /* main */

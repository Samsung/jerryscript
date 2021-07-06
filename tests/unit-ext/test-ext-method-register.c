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
 * Unit test for jerry-ext/handler property registration
 */

#include "jerryscript.h"
#include "jerryscript-ext/handler.h"
#include "test-common.h"

#include <string.h>

static jerry_value_t
method_hello (const jerry_call_info_t *call_info_p, /**< call information */
              const jerry_value_t jargv[], /**< arguments */
              const jerry_length_t jargc) /**< number of arguments */
{
  (void) call_info_p;
  (void) jargv;
  return jerry_create_number (jargc);
} /* method_hello */

/**
 * Helper method to create a non-configurable property on an object
 */
static void
freeze_property (jerry_value_t target_obj, /**< target object */
                 const char *target_prop) /**< target property name */
{
  // "freeze" property
  jerry_property_descriptor_t prop_desc = jerry_property_descriptor_create ();
  prop_desc.flags |= JERRY_PROP_IS_CONFIGURABLE_DEFINED;

  jerry_value_t prop_name = jerry_create_string ((const jerry_char_t *) target_prop);
  jerry_value_t return_value = jerry_define_own_property (target_obj, prop_name, &prop_desc);
  TEST_ASSERT (jerry_value_is_boolean (return_value));
  jerry_release_value (return_value);
  jerry_release_value (prop_name);

  jerry_property_descriptor_free (&prop_desc);
} /* freeze_property */

/**
 * Test registration of various property values.
 */
static void
test_simple_registration (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t target_object = jerry_create_object ();

  // Test simple registration
  jerryx_property_entry methods[] =
  {
    JERRYX_PROPERTY_FUNCTION ("hello", method_hello),
    JERRYX_PROPERTY_NUMBER ("my_number", 42.5),
    JERRYX_PROPERTY_STRING ("my_str", "super_str"),
    JERRYX_PROPERTY_STRING_SZ ("my_str_sz", "super_str", 6),
    JERRYX_PROPERTY_BOOLEAN ("my_bool", true),
    JERRYX_PROPERTY_BOOLEAN ("my_bool_false", false),
    JERRYX_PROPERTY_UNDEFINED ("my_non_value"),
    JERRYX_PROPERTY_LIST_END (),
  };

  jerryx_register_result register_result = jerryx_set_properties (target_object, methods);

  TEST_ASSERT (register_result.registered == 7);
  TEST_ASSERT (jerry_value_is_undefined (register_result.result));

  jerryx_release_property_entry (methods, register_result);
  jerry_release_value (register_result.result);

  jerry_value_t global_obj = jerry_get_global_object ();
  jerryx_set_property_str (global_obj, "test", target_object);
  jerry_release_value (target_object);
  jerry_release_value (global_obj);

  {
    const char *test_A = "test.my_number";
    jerry_value_t result = jerry_eval ((const jerry_char_t *) test_A, strlen (test_A), 0);
    TEST_ASSERT (jerry_value_is_number (result));
    TEST_ASSERT (jerry_get_number_value (result) == 42.5);
    jerry_release_value (result);
  }

  {
    const char *test_A = "test.my_str_sz === 'super_'";
    jerry_value_t result = jerry_eval ((const jerry_char_t *) test_A, strlen (test_A), 0);
    TEST_ASSERT (jerry_value_is_boolean (result));
    TEST_ASSERT (jerry_value_is_true (result));
    jerry_release_value (result);
  }

  {
    const char *test_A = "test.my_str === 'super_str'";
    jerry_value_t result = jerry_eval ((const jerry_char_t *) test_A, strlen (test_A), 0);
    TEST_ASSERT (jerry_value_is_boolean (result));
    TEST_ASSERT (jerry_value_is_true (result));
    jerry_release_value (result);
  }

  {
    const char *test_A = "test.my_bool";
    jerry_value_t result = jerry_eval ((const jerry_char_t *) test_A, strlen (test_A), 0);
    TEST_ASSERT (jerry_value_is_boolean (result));
    TEST_ASSERT (jerry_value_is_true (result));
    jerry_release_value (result);
  }

  {
    const char *test_A = "test.my_bool_false";
    jerry_value_t result = jerry_eval ((const jerry_char_t *) test_A, strlen (test_A), 0);
    TEST_ASSERT (jerry_value_is_boolean (result));
    TEST_ASSERT (jerry_value_is_true (result) == false);
    jerry_release_value (result);
  }

  {
    const char *test_A = "test.my_non_value";
    jerry_value_t result = jerry_eval ((const jerry_char_t *) test_A, strlen (test_A), 0);
    TEST_ASSERT (jerry_value_is_undefined (result));
    jerry_release_value (result);
  }

  {
    const char *test_A = "test.hello(33, 42, 2);";
    jerry_value_t result = jerry_eval ((const jerry_char_t *) test_A, strlen (test_A), 0);
    TEST_ASSERT (jerry_value_is_number (result));
    TEST_ASSERT ((uint32_t) jerry_get_number_value (result) == 3u);
    jerry_release_value (result);
  }

  {
    const char *test_A = "test.hello();";
    jerry_value_t result = jerry_eval ((const jerry_char_t *) test_A, strlen (test_A), 0);
    TEST_ASSERT (jerry_value_is_number (result));
    TEST_ASSERT ((uint32_t) jerry_get_number_value (result) == 0u);
    jerry_release_value (result);
  }

  jerry_cleanup ();
} /* test_simple_registration */

/**
 * Test registration error.
 *
 * Trying to register a property which is already a non-configurable property
 * should result in an error.
 */
static void
test_error_setvalue (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  const char *target_prop = "test_err";
  jerry_value_t global_obj = jerry_get_global_object ();
  freeze_property (global_obj, target_prop);

  jerry_value_t new_object = jerry_create_object ();
  jerry_value_t set_result = jerryx_set_property_str (global_obj, target_prop, new_object);
  TEST_ASSERT (jerry_value_is_error (set_result));

  jerry_release_value (set_result);
  jerry_release_value (new_object);
  jerry_release_value (global_obj);

  jerry_cleanup ();
} /* test_error_setvalue */

/**
 * Test registration error with jerryx_set_properties.
 *
 * Trying to register a property which is already a non-configurable property
 * should result in an error.
 */
static void
test_error_single_function (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  const char *target_prop = "test_err";
  jerry_value_t target_object = jerry_create_object ();
  freeze_property (target_object, target_prop);

  jerryx_property_entry methods[] =
  {
    JERRYX_PROPERTY_FUNCTION (target_prop, method_hello), // This registration should fail
    JERRYX_PROPERTY_LIST_END (),
  };

  jerryx_register_result register_result = jerryx_set_properties (target_object, methods);

  TEST_ASSERT (register_result.registered == 0);
  TEST_ASSERT (jerry_value_is_error (register_result.result));
  jerryx_release_property_entry (methods, register_result);
  jerry_release_value (register_result.result);

  jerry_release_value (target_object);

  jerry_cleanup ();
} /* test_error_single_function */

/**
 * Test to see if jerryx_set_properties exits at the first error.
 */
static void
test_error_multiple_functions (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  const char *prop_ok = "prop_ok";
  const char *prop_err = "prop_err";
  const char *prop_not = "prop_not";
  jerry_value_t target_object = jerry_create_object ();
  freeze_property (target_object, prop_err);

  jerryx_property_entry methods[] =
  {
    JERRYX_PROPERTY_FUNCTION (prop_ok, method_hello), // This registration is ok
    JERRYX_PROPERTY_FUNCTION (prop_err, method_hello), // This registration should fail
    JERRYX_PROPERTY_FUNCTION (prop_not, method_hello), // This registration is not done
    JERRYX_PROPERTY_LIST_END (),
  };

  jerryx_register_result register_result = jerryx_set_properties (target_object, methods);

  TEST_ASSERT (register_result.registered == 1);
  TEST_ASSERT (jerry_value_is_error (register_result.result));

  jerryx_release_property_entry (methods, register_result);
  jerry_release_value (register_result.result);

  {
    // Test if property "prop_ok" is correctly registered.
    jerry_value_t prop_ok_val = jerry_create_string ((const jerry_char_t *) prop_ok);
    jerry_value_t prop_ok_exists = jerry_has_own_property (target_object, prop_ok_val);
    TEST_ASSERT (jerry_value_is_true (prop_ok_exists));
    jerry_release_value (prop_ok_exists);

    // Try calling the method
    jerry_value_t prop_ok_func = jerry_get_property (target_object, prop_ok_val);
    TEST_ASSERT (jerry_value_is_function (prop_ok_func) == true);
    jerry_value_t args[2] =
    {
      jerry_create_number (22),
      jerry_create_number (-3),
    };
    jerry_size_t args_cnt = sizeof (args) / sizeof (jerry_value_t);
    jerry_value_t func_result = jerry_call_function (prop_ok_func,
                                                     jerry_create_undefined (),
                                                     args,
                                                     args_cnt);
    TEST_ASSERT (jerry_value_is_number (func_result) == true);
    TEST_ASSERT ((uint32_t) jerry_get_number_value (func_result) == 2u);
    jerry_release_value (func_result);
    for (jerry_size_t idx = 0; idx < args_cnt; idx++)
    {
      jerry_release_value (args[idx]);
    }
    jerry_release_value (prop_ok_func);
    jerry_release_value (prop_ok_val);
  }

  {
    // The "prop_err" should exist - as it was "freezed" - but it should not be a function
    jerry_value_t prop_err_val = jerry_create_string ((const jerry_char_t *) prop_err);
    jerry_value_t prop_err_exists = jerry_has_own_property (target_object, prop_err_val);
    TEST_ASSERT (jerry_value_is_true (prop_err_exists));
    jerry_release_value (prop_err_exists);

    jerry_value_t prop_err_func = jerry_value_is_function (prop_err_val);
    TEST_ASSERT (jerry_value_is_function (prop_err_func) == false);
    jerry_release_value (prop_err_val);
  }

  { // The "prop_not" is not available on the target object
    jerry_value_t prop_not_val = jerry_create_string ((const jerry_char_t *) prop_not);
    jerry_value_t prop_not_exists = jerry_has_own_property (target_object, prop_not_val);
    TEST_ASSERT (jerry_value_is_true (prop_not_exists) == false);
    jerry_release_value (prop_not_exists);
    jerry_release_value (prop_not_val);
  }

  jerry_release_value (target_object);

  jerry_cleanup ();
} /* test_error_multiple_functions */

int
main (void)
{
  test_simple_registration ();
  test_error_setvalue ();
  test_error_single_function ();
  test_error_multiple_functions ();
  return 0;
} /* main */

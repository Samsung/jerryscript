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

#include "jerryscript.h"

#include "test-common.h"

static void
create_number_property (jerry_value_t object_value, /**< object value */
                        char *name_p, /**< name */
                        double number) /**< value */
{
  jerry_value_t name_value = jerry_create_string ((const jerry_char_t *) name_p);
  jerry_value_t number_value = jerry_create_number (number);
  jerry_value_t result_value = jerry_set_property (object_value, name_value, number_value);
  TEST_ASSERT (!jerry_value_is_error (result_value));

  jerry_release_value (result_value);
  jerry_release_value (number_value);
  jerry_release_value (name_value);
} /* create_number_property */

static double
get_number_property (jerry_value_t object_value, /**< object value */
                     char *name_p) /**< name */
{
  jerry_value_t name_value = jerry_create_string ((const jerry_char_t *) name_p);
  jerry_value_t result_value = jerry_get_property (object_value, name_value);
  TEST_ASSERT (!jerry_value_is_error (result_value));
  TEST_ASSERT (jerry_value_is_number (result_value));

  double result = jerry_get_number_value (result_value);

  jerry_release_value (result_value);
  jerry_release_value (name_value);
  return result;
} /* get_number_property */

static double
eval_and_get_number (char *script_p) /**< script source */
{
  jerry_value_t result_value;
  result_value = jerry_eval ((const jerry_char_t *) script_p, strlen (script_p), JERRY_PARSE_NO_OPTS);

  TEST_ASSERT (jerry_value_is_number (result_value));
  double result = jerry_get_number_value (result_value);
  jerry_release_value (result_value);
  return result;
} /* eval_and_get_number */

static void
check_type_error (jerry_value_t result_value) /**< result value */
{
  TEST_ASSERT (jerry_value_is_error (result_value));
  result_value = jerry_get_value_from_error (result_value, true);
  TEST_ASSERT (jerry_get_error_type (result_value) == JERRY_ERROR_TYPE);
  jerry_release_value (result_value);
} /* check_type_error */

static void
check_array_prototype (jerry_value_t realm_value, jerry_value_t result_value)
{
  jerry_value_t name_value = jerry_create_string ((const jerry_char_t *) "Array");
  jerry_value_t array_value = jerry_get_property (realm_value, name_value);
  TEST_ASSERT (jerry_value_is_object (array_value));
  jerry_release_value (name_value);

  name_value = jerry_create_string ((const jerry_char_t *) "prototype");
  jerry_value_t prototype_value = jerry_get_property (array_value, name_value);
  TEST_ASSERT (jerry_value_is_object (prototype_value));
  jerry_release_value (name_value);
  jerry_release_value (array_value);

  jerry_value_t compare_value = jerry_binary_operation (JERRY_BIN_OP_STRICT_EQUAL, result_value, prototype_value);
  jerry_release_value (prototype_value);

  TEST_ASSERT (jerry_value_is_boolean (compare_value) && jerry_value_is_true (compare_value));
  jerry_release_value (compare_value);
} /* check_array_prototype */

/**
 * Unit test's main function.
 */
int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t global_value = jerry_get_global_object ();
  jerry_value_t result_value = jerry_realm_get_this (global_value);
  TEST_ASSERT (global_value == result_value);
  jerry_release_value (global_value);

  jerry_value_t number_value = jerry_create_number (3);
  check_type_error (jerry_realm_get_this (number_value));
  jerry_release_value (number_value);

  if (!jerry_is_feature_enabled (JERRY_FEATURE_REALM))
  {
    printf ("Skipping test, Realms not enabled\n");
    return 0;
  }

  jerry_value_t realm_value = jerry_create_realm ();

  create_number_property (global_value, "a", 3.5);
  create_number_property (global_value, "b", 7.25);
  create_number_property (realm_value, "a", -1.25);
  create_number_property (realm_value, "b", -6.75);

  TEST_ASSERT (eval_and_get_number ("a") == 3.5);

  result_value = jerry_set_realm (realm_value);
  TEST_ASSERT (result_value == global_value);
  TEST_ASSERT (eval_and_get_number ("a") == -1.25);

  result_value = jerry_set_realm (global_value);
  TEST_ASSERT (result_value == realm_value);
  TEST_ASSERT (eval_and_get_number ("b") == 7.25);

  result_value = jerry_set_realm (realm_value);
  TEST_ASSERT (result_value == global_value);
  TEST_ASSERT (eval_and_get_number ("b") == -6.75);

  result_value = jerry_set_realm (global_value);
  TEST_ASSERT (result_value == realm_value);

  jerry_value_t object_value = jerry_create_object ();
  check_type_error (jerry_set_realm (object_value));
  jerry_release_value (object_value);

  number_value = jerry_create_number (5);
  check_type_error (jerry_set_realm (number_value));
  jerry_release_value (number_value);

  jerry_release_value (global_value);
  jerry_release_value (realm_value);

  realm_value = jerry_create_realm ();

  result_value = jerry_realm_get_this (realm_value);
  TEST_ASSERT (result_value == realm_value);
  jerry_release_value (result_value);

  result_value = jerry_set_realm (realm_value);
  TEST_ASSERT (!jerry_value_is_error (result_value));
  object_value = jerry_create_object ();
  jerry_set_realm (result_value);

  number_value = jerry_create_number (7);
  check_type_error (jerry_realm_set_this (realm_value, number_value));
  check_type_error (jerry_realm_set_this (number_value, object_value));
  jerry_release_value (number_value);

  result_value = jerry_realm_set_this (realm_value, object_value);
  TEST_ASSERT (jerry_value_is_boolean (result_value) && jerry_value_is_true (result_value));
  jerry_release_value (result_value);

  create_number_property (object_value, "x", 7.25);
  create_number_property (object_value, "y", 1.25);

  result_value = jerry_set_realm (realm_value);
  TEST_ASSERT (!jerry_value_is_error (result_value));
  TEST_ASSERT (eval_and_get_number ("var z = -5.5; x + this.y") == 8.5);
  jerry_set_realm (result_value);

  TEST_ASSERT (get_number_property (object_value, "z") == -5.5);

  result_value = jerry_realm_get_this (realm_value);
  TEST_ASSERT (result_value == object_value);
  jerry_release_value (result_value);

  jerry_release_value (object_value);
  jerry_release_value (realm_value);

  if (jerry_is_feature_enabled (JERRY_FEATURE_PROXY))
  {
    /* Check property creation. */
    jerry_value_t handler_value = jerry_create_object ();
    jerry_value_t target_value = jerry_create_realm ();
    jerry_value_t proxy_value = jerry_create_proxy (target_value, handler_value);

    jerry_realm_set_this (target_value, proxy_value);
    jerry_release_value (proxy_value);
    jerry_release_value (handler_value);

    jerry_value_t old_realm_value = jerry_set_realm (target_value);
    TEST_ASSERT (!jerry_value_is_error (old_realm_value));
    TEST_ASSERT (eval_and_get_number ("var z = 1.5; z") == 1.5);
    jerry_set_realm (old_realm_value);

    TEST_ASSERT (get_number_property (target_value, "z") == 1.5);
    jerry_release_value (target_value);

    /* Check isExtensible error. */

    const char *script_p = "new Proxy({}, { isExtensible: function() { throw 42.5 } })";
    proxy_value = jerry_eval ((const jerry_char_t *) script_p, strlen (script_p), JERRY_PARSE_NO_OPTS);
    TEST_ASSERT (!jerry_value_is_error (proxy_value) && jerry_value_is_object (proxy_value));

    target_value = jerry_create_realm ();
    jerry_realm_set_this (target_value, proxy_value);
    jerry_release_value (proxy_value);

    old_realm_value = jerry_set_realm (target_value);
    TEST_ASSERT (!jerry_value_is_error (old_realm_value));
    script_p = "var z = 1.5";
    result_value = jerry_eval ((const jerry_char_t *) script_p, strlen (script_p), JERRY_PARSE_NO_OPTS);
    jerry_set_realm (old_realm_value);
    jerry_release_value (target_value);

    TEST_ASSERT (jerry_value_is_error (result_value));
    result_value = jerry_get_value_from_error (result_value, true);
    TEST_ASSERT (jerry_value_is_number (result_value) && jerry_get_number_value (result_value) == 42.5);
    jerry_release_value (result_value);
  }

  realm_value = jerry_create_realm ();

  result_value = jerry_set_realm (realm_value);
  TEST_ASSERT (!jerry_value_is_error (result_value));

  const char *script_p = "global2 = global1 - 1; Object.getPrototypeOf([])";
  jerry_value_t script_value = jerry_parse ((const jerry_char_t *) script_p,
                                            strlen (script_p),
                                            NULL);

  TEST_ASSERT (!jerry_value_is_error (script_value));
  jerry_set_realm (result_value);

  /* Script is compiled in another realm. */
  create_number_property (realm_value, "global1", 7.5);
  result_value = jerry_run (script_value);
  TEST_ASSERT (!jerry_value_is_error (result_value));

  check_array_prototype (realm_value, result_value);

  jerry_release_value (result_value);
  jerry_release_value (script_value);

  TEST_ASSERT (get_number_property (realm_value, "global2") == 6.5);

  jerry_release_value (realm_value);

  jerry_cleanup ();
  return 0;
} /* main */

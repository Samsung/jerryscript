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
#include "jerryscript-port.h"
#include "jerryscript-port-default.h"
#include "test-common.h"

int
main (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t object = jerry_create_object ();

  jerry_value_t prop_name_1 = jerry_create_string ((const jerry_char_t *) "foo");
  jerry_value_t prop_name_2 = jerry_create_string ((const jerry_char_t *) "non_hidden_prop");
  jerry_value_t prop_name_3;

  if (jerry_is_feature_enabled (JERRY_FEATURE_SYMBOL))
  {
    jerry_value_t prop_name_3_desc = jerry_create_string ((const jerry_char_t *) "bar");
    prop_name_3 = jerry_create_symbol (prop_name_3_desc);
    jerry_release_value (prop_name_3_desc);
  }
  else
  {
    prop_name_3 = jerry_create_string ((const jerry_char_t *) "non_hidden_string_prop");
  }

  jerry_value_t internal_prop_name_1 = jerry_create_string ((const jerry_char_t *) "hidden_foo");
  jerry_value_t internal_prop_name_2 = jerry_create_string ((const jerry_char_t *) "hidden_prop");
  jerry_value_t internal_prop_name_3;

  if (jerry_is_feature_enabled (JERRY_FEATURE_SYMBOL))
  {
    jerry_value_t internal_prop_name_3_desc = jerry_create_string ((const jerry_char_t *) "bar");
    internal_prop_name_3 = jerry_create_symbol (internal_prop_name_3_desc);
    jerry_release_value (internal_prop_name_3_desc);
  }
  else
  {
    internal_prop_name_3 = jerry_create_string ((const jerry_char_t *) "hidden_string_prop");
  }

  jerry_value_t prop_value_1 = jerry_create_number (5.5);
  jerry_value_t prop_value_2 = jerry_create_number (6.5);
  jerry_value_t prop_value_3 = jerry_create_number (7.5);

  jerry_value_t internal_prop_value_1 = jerry_create_number (8.5);
  jerry_value_t internal_prop_value_2 = jerry_create_number (9.5);
  jerry_value_t internal_prop_value_3 = jerry_create_number (10.5);

  /* Test the normal [[Set]] method */
  bool set_result_1 = jerry_set_property (object, prop_name_1, prop_value_1);
  bool set_result_2 = jerry_set_property (object, prop_name_2, prop_value_2);
  bool set_result_3 = jerry_set_property (object, prop_name_3, prop_value_3);

  TEST_ASSERT (set_result_1);
  TEST_ASSERT (set_result_2);
  TEST_ASSERT (set_result_3);

  /* Test the internal [[Set]] method */
  bool set_internal_result_1 = jerry_set_internal_property (object, internal_prop_name_1, internal_prop_value_1);
  bool set_internal_result_2 = jerry_set_internal_property (object, internal_prop_name_2, internal_prop_value_2);
  bool set_internal_result_3 = jerry_set_internal_property (object, internal_prop_name_3, internal_prop_value_3);

  TEST_ASSERT (set_internal_result_1);
  TEST_ASSERT (set_internal_result_2);
  TEST_ASSERT (set_internal_result_3);

  /* Test the normal [[Has]] method. */
  jerry_value_t has_result_1 = jerry_has_property (object, prop_name_1);
  jerry_value_t has_result_2 = jerry_has_property (object, prop_name_2);
  jerry_value_t has_result_3 = jerry_has_property (object, prop_name_3);
  jerry_value_t has_result_4 = jerry_has_property (object, internal_prop_name_1);
  jerry_value_t has_result_5 = jerry_has_property (object, internal_prop_name_2);
  jerry_value_t has_result_6 = jerry_has_property (object, internal_prop_name_3);

  TEST_ASSERT (jerry_value_is_boolean (has_result_1) && jerry_value_is_true (has_result_1));
  TEST_ASSERT (jerry_value_is_boolean (has_result_2) && jerry_value_is_true (has_result_2));
  TEST_ASSERT (jerry_value_is_boolean (has_result_3) && jerry_value_is_true (has_result_3));
  TEST_ASSERT (jerry_value_is_boolean (has_result_4) && !jerry_value_is_true (has_result_4));
  TEST_ASSERT (jerry_value_is_boolean (has_result_5) && !jerry_value_is_true (has_result_5));
  TEST_ASSERT (jerry_value_is_boolean (has_result_6) && !jerry_value_is_true (has_result_6));

  jerry_release_value (has_result_1);
  jerry_release_value (has_result_2);
  jerry_release_value (has_result_3);
  jerry_release_value (has_result_4);
  jerry_release_value (has_result_5);
  jerry_release_value (has_result_6);

  /* Test the internal [[Has]] method. */
  bool has_internal_result_1 = jerry_has_internal_property (object, prop_name_1);
  bool has_internal_result_2 = jerry_has_internal_property (object, prop_name_2);
  bool has_internal_result_3 = jerry_has_internal_property (object, prop_name_3);
  bool has_internal_result_4 = jerry_has_internal_property (object, internal_prop_name_1);
  bool has_internal_result_5 = jerry_has_internal_property (object, internal_prop_name_2);
  bool has_internal_result_6 = jerry_has_internal_property (object, internal_prop_name_3);

  TEST_ASSERT (!has_internal_result_1);
  TEST_ASSERT (!has_internal_result_2);
  TEST_ASSERT (!has_internal_result_3);
  TEST_ASSERT (has_internal_result_4);
  TEST_ASSERT (has_internal_result_5);
  TEST_ASSERT (has_internal_result_6);

  /* Test the normal [[Get]] method. */
  jerry_value_t get_result_1 = jerry_get_property (object, prop_name_1);
  jerry_value_t get_result_2 = jerry_get_property (object, prop_name_2);
  jerry_value_t get_result_3 = jerry_get_property (object, prop_name_3);
  jerry_value_t get_result_4 = jerry_get_property (object, internal_prop_name_1);
  jerry_value_t get_result_5 = jerry_get_property (object, internal_prop_name_2);
  jerry_value_t get_result_6 = jerry_get_property (object, internal_prop_name_3);

  TEST_ASSERT (jerry_value_is_number (get_result_1) && jerry_get_number_value (get_result_1) == 5.5);
  TEST_ASSERT (jerry_value_is_number (get_result_2) && jerry_get_number_value (get_result_2) == 6.5);
  TEST_ASSERT (jerry_value_is_number (get_result_3) && jerry_get_number_value (get_result_3) == 7.5);
  TEST_ASSERT (jerry_value_is_undefined (get_result_4));
  TEST_ASSERT (jerry_value_is_undefined (get_result_5));
  TEST_ASSERT (jerry_value_is_undefined (get_result_6));

  jerry_release_value (get_result_1);
  jerry_release_value (get_result_2);
  jerry_release_value (get_result_3);
  jerry_release_value (get_result_4);
  jerry_release_value (get_result_5);
  jerry_release_value (get_result_6);

  /* Test the internal [[Get]] method. */
  jerry_value_t get_internal_result_1 = jerry_get_internal_property (object, prop_name_1);
  jerry_value_t get_internal_result_2 = jerry_get_internal_property (object, prop_name_2);
  jerry_value_t get_internal_result_3 = jerry_get_internal_property (object, prop_name_3);
  jerry_value_t get_internal_result_4 = jerry_get_internal_property (object, internal_prop_name_1);
  jerry_value_t get_internal_result_5 = jerry_get_internal_property (object, internal_prop_name_2);
  jerry_value_t get_internal_result_6 = jerry_get_internal_property (object, internal_prop_name_3);

  TEST_ASSERT (jerry_value_is_undefined (get_internal_result_1));
  TEST_ASSERT (jerry_value_is_undefined (get_internal_result_2));
  TEST_ASSERT (jerry_value_is_undefined (get_internal_result_3));
  TEST_ASSERT (jerry_value_is_number (get_internal_result_4) && jerry_get_number_value (get_internal_result_4) == 8.5);
  TEST_ASSERT (jerry_value_is_number (get_internal_result_5) && jerry_get_number_value (get_internal_result_5) == 9.5);
  TEST_ASSERT (jerry_value_is_number (get_internal_result_6) && jerry_get_number_value (get_internal_result_6) == 10.5);

  jerry_release_value (get_internal_result_1);
  jerry_release_value (get_internal_result_2);
  jerry_release_value (get_internal_result_3);
  jerry_release_value (get_internal_result_4);
  jerry_release_value (get_internal_result_5);
  jerry_release_value (get_internal_result_6);

  /* Test the normal [[Delete]] method. */
  bool delete_result_1 = jerry_delete_property (object, prop_name_1);
  bool delete_result_2 = jerry_delete_property (object, prop_name_2);
  bool delete_result_3 = jerry_delete_property (object, prop_name_3);
  bool delete_result_4 = jerry_delete_property (object, internal_prop_name_1);
  bool delete_result_5 = jerry_delete_property (object, internal_prop_name_2);
  bool delete_result_6 = jerry_delete_property (object, internal_prop_name_3);

  TEST_ASSERT (delete_result_1);
  TEST_ASSERT (delete_result_2);
  TEST_ASSERT (delete_result_3);
  TEST_ASSERT (delete_result_4);
  TEST_ASSERT (delete_result_5);
  TEST_ASSERT (delete_result_6);

  jerry_value_t has_after_delete_result_1 = jerry_has_property (object, prop_name_1);
  jerry_value_t has_after_delete_result_2 = jerry_has_property (object, prop_name_2);
  jerry_value_t has_after_delete_result_3 = jerry_has_property (object, prop_name_3);
  bool has_after_delete_result_4 = jerry_has_internal_property (object, internal_prop_name_1);
  bool has_after_delete_result_5 = jerry_has_internal_property (object, internal_prop_name_2);
  bool has_after_delete_result_6 = jerry_has_internal_property (object, internal_prop_name_3);

  TEST_ASSERT (jerry_value_is_boolean (has_after_delete_result_1)
               && !jerry_value_is_true (has_after_delete_result_1));
  TEST_ASSERT (jerry_value_is_boolean (has_after_delete_result_2)
               && !jerry_value_is_true (has_after_delete_result_2));
  TEST_ASSERT (jerry_value_is_boolean (has_after_delete_result_3)
               && !jerry_value_is_true (has_after_delete_result_3));
  TEST_ASSERT (has_after_delete_result_4);
  TEST_ASSERT (has_after_delete_result_5);
  TEST_ASSERT (has_after_delete_result_6);

  jerry_release_value (has_after_delete_result_1);
  jerry_release_value (has_after_delete_result_2);
  jerry_release_value (has_after_delete_result_3);

  /* Test the internal [[Delete]] method. */
  bool delete_internal_result_4 = jerry_delete_internal_property (object, internal_prop_name_1);
  bool delete_internal_result_5 = jerry_delete_internal_property (object, internal_prop_name_2);
  bool delete_internal_result_6 = jerry_delete_internal_property (object, internal_prop_name_3);

  TEST_ASSERT (delete_internal_result_4);
  TEST_ASSERT (delete_internal_result_5);
  TEST_ASSERT (delete_internal_result_6);

  bool has_after_internal_delete_result_1 = jerry_has_internal_property (object, internal_prop_name_1);
  bool has_after_internal_delete_result_2 = jerry_has_internal_property (object, internal_prop_name_2);
  bool has_after_internal_delete_result_3 = jerry_has_internal_property (object, internal_prop_name_3);

  TEST_ASSERT (!has_after_internal_delete_result_1);
  TEST_ASSERT (!has_after_internal_delete_result_2);
  TEST_ASSERT (!has_after_internal_delete_result_3);

  /* Cleanup */
  jerry_release_value (prop_value_3);
  jerry_release_value (prop_value_2);
  jerry_release_value (prop_value_1);

  jerry_release_value (prop_name_3);
  jerry_release_value (prop_name_2);
  jerry_release_value (prop_name_1);

  jerry_release_value (internal_prop_value_3);
  jerry_release_value (internal_prop_value_2);
  jerry_release_value (internal_prop_value_1);

  jerry_release_value (internal_prop_name_3);
  jerry_release_value (internal_prop_name_2);
  jerry_release_value (internal_prop_name_1);

  jerry_release_value (object);

  jerry_cleanup ();

  return 0;
} /* main */

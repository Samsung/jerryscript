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

int
main (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  if (!jerry_is_feature_enabled (JERRY_FEATURE_MAP)
      || !jerry_is_feature_enabled (JERRY_FEATURE_SET)
      || !jerry_is_feature_enabled (JERRY_FEATURE_WEAKMAP)
      || !jerry_is_feature_enabled (JERRY_FEATURE_WEAKSET))
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Containers are disabled!\n");
    jerry_cleanup ();
    return 0;
  }

  // Map container tests
  jerry_value_t map = jerry_create_container (JERRY_CONTAINER_TYPE_MAP, NULL, 0);
  TEST_ASSERT (jerry_get_container_type (map) == JERRY_CONTAINER_TYPE_MAP);

  jerry_value_t key_str = jerry_create_string ((jerry_char_t *) "number");
  jerry_value_t number = jerry_create_number (10);
  jerry_value_t args[2] = {key_str, number};
  jerry_value_t result = jerry_container_operation (JERRY_CONTAINER_OP_SET, map, args, 2);
  TEST_ASSERT (!jerry_value_is_error (result));
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_GET, map, &key_str, 1);
  TEST_ASSERT (jerry_get_number_value (result) == 10);
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_HAS, map, &key_str, 1);
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_SIZE, map, NULL, 0);
  TEST_ASSERT (jerry_get_number_value (result) == 1);
  jerry_release_value (result);

  key_str = jerry_create_string ((jerry_char_t *) "number2");
  number = jerry_create_number (11);
  jerry_value_t args2[2] = {key_str, number};
  result = jerry_container_operation (JERRY_CONTAINER_OP_SET, map, args2, 2);
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_SIZE, map, NULL, 0);
  TEST_ASSERT (jerry_get_number_value (result) == 2);
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_DELETE, map, &key_str, 1);
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_SIZE, map, NULL, 0);
  TEST_ASSERT (jerry_get_number_value (result) == 1);
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_CLEAR, map, NULL, 0);
  TEST_ASSERT (jerry_value_is_undefined (result));
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_SIZE, map, NULL, 0);
  TEST_ASSERT (jerry_get_number_value (result) == 0);
  jerry_release_value (result);

  // Set container tests
  number = jerry_create_number (10);
  jerry_value_t set = jerry_create_container (JERRY_CONTAINER_TYPE_SET, NULL, 0);
  TEST_ASSERT (jerry_get_container_type (set) == JERRY_CONTAINER_TYPE_SET);
  result = jerry_container_operation (JERRY_CONTAINER_OP_ADD, set, &number, 1);
  TEST_ASSERT (!jerry_value_is_error (result));
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_HAS, set, &number, 1);
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_SIZE, set, NULL, 0);
  TEST_ASSERT (jerry_get_number_value (result) == 1);
  jerry_release_value (result);

  number = jerry_create_number (11);
  result = jerry_container_operation (JERRY_CONTAINER_OP_ADD, set, &number, 1);
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_SIZE, set, NULL, 0);
  TEST_ASSERT (jerry_get_number_value (result) == 2);
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_DELETE, set, &number, 1);
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_SIZE, set, NULL, 0);
  TEST_ASSERT (jerry_get_number_value (result) == 1);
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_CLEAR, set, NULL, 0);
  TEST_ASSERT (jerry_value_is_undefined (result));
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_SIZE, set, NULL, 0);
  TEST_ASSERT (jerry_get_number_value (result) == 0);
  jerry_release_value (result);
  jerry_release_value (set);

  // WeakMap contanier tests
  number = jerry_create_number (10);
  jerry_value_t weak_map = jerry_create_container (JERRY_CONTAINER_TYPE_WEAKMAP, NULL, 0);
  TEST_ASSERT (jerry_get_container_type (weak_map) == JERRY_CONTAINER_TYPE_WEAKMAP);

  jerry_value_t obj = jerry_create_object ();
  number = jerry_create_number (10);
  jerry_value_t args4[2] = {obj, number};
  result = jerry_container_operation (JERRY_CONTAINER_OP_SET, weak_map, args4, 2);
  TEST_ASSERT (!jerry_value_is_error (result));
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_HAS, weak_map, &obj, 1);
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_DELETE, weak_map, &obj, 1);
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);
  jerry_release_value (weak_map);

  // WeakSet contanier tests,
  jerry_value_t weak_set = jerry_create_container (JERRY_CONTAINER_TYPE_WEAKSET, NULL, 0);
  TEST_ASSERT (jerry_get_container_type (weak_set) == JERRY_CONTAINER_TYPE_WEAKSET);

  result = jerry_container_operation (JERRY_CONTAINER_OP_ADD, weak_set, &obj, 1);
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_HAS, weak_set, &obj, 1);
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  result = jerry_container_operation (JERRY_CONTAINER_OP_DELETE, weak_set, &obj, 1);
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);
  jerry_release_value (weak_set);

  // container is not a object
  jerry_value_t empty_val = jerry_create_undefined ();
  result = jerry_container_operation (JERRY_CONTAINER_OP_SET, empty_val, args, 2);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);

  // arguments is a error
  const char * const error_message_p = "Random error.";
  jerry_value_t error_val = jerry_create_error (JERRY_ERROR_RANGE, (const jerry_char_t *) error_message_p);
  jerry_value_t args3[2] = { error_val, error_val };
  result = jerry_container_operation (JERRY_CONTAINER_OP_SET, map, args3, 2);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);
  jerry_release_value (error_val);
  jerry_release_value (map);

  jerry_release_value (key_str);
  jerry_release_value (number);
  jerry_release_value (obj);
  jerry_cleanup ();
  return 0;

} /* main */

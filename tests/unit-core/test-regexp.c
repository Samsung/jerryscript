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
  TEST_INIT ();
  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t global_obj_val = jerry_get_global_object ();

  jerry_char_t pattern[] = "[^.]+";
  uint16_t flags = JERRY_REGEXP_FLAG_GLOBAL | JERRY_REGEXP_FLAG_MULTILINE;
  jerry_value_t regex_obj = jerry_create_regexp (pattern, flags);
  TEST_ASSERT (jerry_value_is_object (regex_obj));

  const jerry_char_t func_arg_list[] = "regex";
  const jerry_char_t func_src[] = "return [regex.exec('something.domain.com'), regex.multiline, regex.global];";
  jerry_value_t func_val = jerry_parse_function (func_arg_list,
                                                 sizeof (func_arg_list) - 1,
                                                 func_src,
                                                 sizeof (func_src) - 1,
                                                 NULL);

  jerry_value_t res = jerry_call_function (func_val, global_obj_val, &regex_obj, 1);
  jerry_value_t regex_res = jerry_get_property_by_index (res, 0);
  jerry_value_t regex_res_str = jerry_get_property_by_index (regex_res, 0);
  jerry_value_t is_multiline = jerry_get_property_by_index (res, 1);
  jerry_value_t is_global = jerry_get_property_by_index (res, 2);

  const char expected_result[] = "something";
  jerry_size_t str_size = jerry_get_string_size (regex_res_str);
  TEST_ASSERT (str_size == (sizeof (expected_result) - 1));

  JERRY_VLA (jerry_char_t, res_buff, str_size);
  jerry_size_t res_size = jerry_string_to_char_buffer (regex_res_str, res_buff, str_size);

  TEST_ASSERT (res_size == str_size);
  TEST_ASSERT (strncmp (expected_result, (const char *) res_buff, res_size) == 0);
  TEST_ASSERT (jerry_value_is_true (is_multiline));
  TEST_ASSERT (jerry_value_is_true (is_global));

  jerry_release_value (regex_obj);
  jerry_release_value (res);
  jerry_release_value (func_val);
  jerry_release_value (regex_res);
  jerry_release_value (regex_res_str);
  jerry_release_value (is_multiline);
  jerry_release_value (is_global);
  jerry_release_value (global_obj_val);

  jerry_cleanup ();
  return 0;
} /* main */

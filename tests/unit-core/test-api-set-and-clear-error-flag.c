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
compare_str (jerry_value_t value, const char *str_p, size_t str_len)
{
  jerry_size_t size = jerry_get_string_size (value);
  JERRY_ASSERT (str_len == size);
  JERRY_VLA (jerry_char_t, str_buff, size);
  jerry_string_to_utf8_char_buffer (value, str_buff, size);
  JERRY_ASSERT (!memcmp (str_p, str_buff, str_len));
} /* compare_str */

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_DEFAULT);

  jerry_value_t obj_val = jerry_create_object ();
  obj_val = jerry_create_error_from_value (obj_val, true);
  jerry_value_t err_val = jerry_acquire_value (obj_val);

  obj_val = jerry_get_value_from_error (err_val, true);

  JERRY_ASSERT (obj_val != err_val);
  jerry_release_value (err_val);
  jerry_release_value (obj_val);

  const char *pterodactylus_p = "Pterodactylus";
  const size_t pterodactylus_size = strlen (pterodactylus_p);

  jerry_value_t str = jerry_create_string ((jerry_char_t *) pterodactylus_p);
  jerry_value_t error = jerry_create_error_from_value (str, true);
  str = jerry_get_value_from_error (error, true);

  compare_str (str, pterodactylus_p, pterodactylus_size);
  jerry_release_value (str);

  str = jerry_create_string ((jerry_char_t *) pterodactylus_p);
  error = jerry_create_error_from_value (str, false);
  jerry_release_value (str);
  str = jerry_get_value_from_error (error, true);

  compare_str (str, pterodactylus_p, pterodactylus_size);
  jerry_release_value (str);

  str = jerry_create_string ((jerry_char_t *) pterodactylus_p);
  error = jerry_create_abort_from_value (str, true);
  str = jerry_get_value_from_error (error, true);

  compare_str (str, pterodactylus_p, pterodactylus_size);
  jerry_release_value (str);

  str = jerry_create_string ((jerry_char_t *) pterodactylus_p);
  error = jerry_create_abort_from_value (str, false);
  jerry_release_value (str);
  str = jerry_get_value_from_error (error, true);

  compare_str (str, pterodactylus_p, pterodactylus_size);
  jerry_release_value (str);

  str = jerry_create_string ((jerry_char_t *) pterodactylus_p);
  error = jerry_create_error_from_value (str, true);
  error = jerry_create_abort_from_value (error, true);
  JERRY_ASSERT (jerry_value_is_abort (error));
  str = jerry_get_value_from_error (error, true);

  compare_str (str, pterodactylus_p, pterodactylus_size);
  jerry_release_value (str);

  str = jerry_create_string ((jerry_char_t *) pterodactylus_p);
  error = jerry_create_error_from_value (str, true);
  jerry_value_t error2 = jerry_create_abort_from_value (error, false);
  JERRY_ASSERT (jerry_value_is_abort (error2));
  jerry_release_value (error);
  str = jerry_get_value_from_error (error2, true);

  compare_str (str, pterodactylus_p, pterodactylus_size);
  jerry_release_value (str);

  double test_num = 3.1415926;
  jerry_value_t num = jerry_create_number (test_num);
  jerry_value_t num2 = jerry_create_error_from_value (num, false);
  JERRY_ASSERT (jerry_value_is_error (num2));
  jerry_release_value (num);
  num2 = jerry_get_value_from_error (num2, true);
  JERRY_ASSERT (jerry_get_number_value (num2) == test_num);
  jerry_release_value (num2);

  num = jerry_create_number (test_num);
  num2 = jerry_create_error_from_value (num, true);
  JERRY_ASSERT (jerry_value_is_error (num2));
  num2 = jerry_get_value_from_error (num2, true);
  JERRY_ASSERT (jerry_get_number_value (num2) == test_num);
  jerry_release_value (num2);

  num = jerry_create_number (test_num);
  num2 = jerry_create_error_from_value (num, false);
  JERRY_ASSERT (jerry_value_is_error (num2));
  jerry_release_value (num);
  jerry_value_t num3 = jerry_create_error_from_value (num2, false);
  JERRY_ASSERT (jerry_value_is_error (num3));
  jerry_release_value (num2);
  num2 = jerry_get_value_from_error (num3, true);
  JERRY_ASSERT (jerry_get_number_value (num2) == test_num);
  jerry_release_value (num2);

  num = jerry_create_number (test_num);
  num2 = jerry_create_error_from_value (num, true);
  JERRY_ASSERT (jerry_value_is_error (num2));
  num3 = jerry_create_error_from_value (num2, true);
  JERRY_ASSERT (jerry_value_is_error (num3));
  num2 = jerry_get_value_from_error (num3, true);
  JERRY_ASSERT (jerry_get_number_value (num2) == test_num);
  jerry_release_value (num2);

  num = jerry_create_number (test_num);
  error = jerry_create_abort_from_value (num, true);
  JERRY_ASSERT (jerry_value_is_abort (error));
  num2 = jerry_create_error_from_value (error, true);
  JERRY_ASSERT (jerry_value_is_error (num2));
  num = jerry_get_value_from_error (num2, true);
  JERRY_ASSERT (jerry_get_number_value (num) == test_num);
  jerry_release_value (num);

  num = jerry_create_number (test_num);
  error = jerry_create_abort_from_value (num, false);
  jerry_release_value (num);
  JERRY_ASSERT (jerry_value_is_abort (error));
  num2 = jerry_create_error_from_value (error, true);
  JERRY_ASSERT (jerry_value_is_error (num2));
  num = jerry_get_value_from_error (num2, true);
  JERRY_ASSERT (jerry_get_number_value (num) == test_num);
  jerry_release_value (num);

  num = jerry_create_number (test_num);
  error = jerry_create_abort_from_value (num, true);
  JERRY_ASSERT (jerry_value_is_abort (error));
  num2 = jerry_create_error_from_value (error, false);
  jerry_release_value (error);
  JERRY_ASSERT (jerry_value_is_error (num2));
  num = jerry_get_value_from_error (num2, true);
  JERRY_ASSERT (jerry_get_number_value (num) == test_num);
  jerry_release_value (num);

  num = jerry_create_number (test_num);
  error = jerry_create_abort_from_value (num, false);
  jerry_release_value (num);
  JERRY_ASSERT (jerry_value_is_abort (error));
  num2 = jerry_create_error_from_value (error, false);
  jerry_release_value (error);
  JERRY_ASSERT (jerry_value_is_error (num2));
  num = jerry_get_value_from_error (num2, true);
  JERRY_ASSERT (jerry_get_number_value (num) == test_num);
  jerry_release_value (num);

  jerry_value_t value = jerry_create_number (42);
  value = jerry_get_value_from_error (value, true);
  jerry_release_value (value);

  value = jerry_create_number (42);
  jerry_value_t value2 = jerry_get_value_from_error (value, false);
  jerry_release_value (value);
  jerry_release_value (value2);

  value = jerry_create_number (42);
  error = jerry_create_error_from_value (value, true);
  error = jerry_create_error_from_value (error, true);
  jerry_release_value (error);

  value = jerry_create_number (42);
  error = jerry_create_abort_from_value (value, true);
  error = jerry_create_abort_from_value (error, true);
  jerry_release_value (error);

  value = jerry_create_number (42);
  error = jerry_create_error_from_value (value, true);
  error2 = jerry_create_error_from_value (error, false);
  jerry_release_value (error);
  jerry_release_value (error2);

  value = jerry_create_number (42);
  error = jerry_create_abort_from_value (value, true);
  error2 = jerry_create_abort_from_value (error, false);
  jerry_release_value (error);
  jerry_release_value (error2);

  jerry_cleanup ();
} /* main */

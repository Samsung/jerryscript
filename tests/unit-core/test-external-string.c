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

static int free_count = 0;

static const char *external_1 = "External string! External string! External string! External string!";
static const char *external_2 = "Object";
static const char *external_3 = "x!?:s";
static const char *external_4 = "Object property external string! Object property external string!";

static void
external_string_free_callback_1 (jerry_char_t *string_p, /**< string pointer */
                                 jerry_size_t string_size, /**< size of the string */
                                 void *user_p) /**< user pointer */
{
  TEST_ASSERT ((const char *) string_p == external_1);
  TEST_ASSERT (string_size == strlen (external_1));
  TEST_ASSERT (user_p == NULL);
  free_count++;
} /* external_string_free_callback_1 */

static void
external_string_free_callback_2 (jerry_char_t *string_p, /**< string pointer */
                                 jerry_size_t string_size, /**< size of the string */
                                 void *user_p) /**< user pointer */
{
  TEST_ASSERT ((const char *) string_p == external_2);
  TEST_ASSERT (string_size == strlen (external_2));
  TEST_ASSERT (user_p == (void *) &free_count);
  free_count++;
} /* external_string_free_callback_2 */

static void
external_string_free_callback_3 (jerry_char_t *string_p, /**< string pointer */
                                 jerry_size_t string_size, /**< size of the string */
                                 void *user_p) /**< user pointer */
{
  TEST_ASSERT ((const char *) string_p == external_3);
  TEST_ASSERT (string_size == strlen (external_3));
  TEST_ASSERT (user_p == (void *) string_p);
  free_count++;
} /* external_string_free_callback_3 */

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  /* Test external callback calls. */
  jerry_string_set_external_string_free_callback (external_string_free_callback_1);
  jerry_value_t external_string = jerry_create_external_string ((jerry_char_t *) external_1, NULL);
  TEST_ASSERT (free_count == 0);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 1);

  jerry_string_set_external_string_free_callback (NULL);
  external_string = jerry_create_external_string ((jerry_char_t *) external_1, NULL);
  TEST_ASSERT (free_count == 1);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 1);

  jerry_string_set_external_string_free_callback (external_string_free_callback_2);
  external_string = jerry_create_external_string ((jerry_char_t *) external_2, (void *) &free_count);
  TEST_ASSERT (free_count == 2);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 2);

  jerry_string_set_external_string_free_callback (NULL);
  external_string = jerry_create_external_string ((jerry_char_t *) external_2, (void *) &free_count);
  TEST_ASSERT (free_count == 2);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 2);

  jerry_string_set_external_string_free_callback (external_string_free_callback_3);
  external_string = jerry_create_external_string ((jerry_char_t *) external_3, (void *) external_3);
  TEST_ASSERT (free_count == 3);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 3);

  jerry_string_set_external_string_free_callback (NULL);
  external_string = jerry_create_external_string ((jerry_char_t *) external_3, (void *) external_3);
  TEST_ASSERT (free_count == 3);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 3);

  /* Test string comparison. */
  jerry_string_set_external_string_free_callback (external_string_free_callback_1);
  external_string = jerry_create_external_string ((jerry_char_t *) external_1, NULL);
  jerry_value_t other_string = jerry_create_string ((jerry_char_t *) external_1);

  jerry_value_t result = jerry_binary_operation (JERRY_BIN_OP_STRICT_EQUAL, external_string, other_string);
  TEST_ASSERT (jerry_value_is_boolean (result));
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  result = jerry_binary_operation (JERRY_BIN_OP_STRICT_EQUAL, external_string, external_string);
  TEST_ASSERT (jerry_value_is_boolean (result));
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  TEST_ASSERT (free_count == 3);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 4);
  jerry_release_value (other_string);

  /* Test getting string. */
  jerry_string_set_external_string_free_callback (external_string_free_callback_1);
  external_string = jerry_create_external_string ((jerry_char_t *) external_1, NULL);
  size_t length = strlen (external_1);

  TEST_ASSERT (jerry_value_is_string (external_string));
  TEST_ASSERT (jerry_get_string_size (external_string) == length);
  TEST_ASSERT (jerry_get_string_length (external_string) == length);

  jerry_char_t buf[128];
  jerry_string_to_char_buffer (external_string, buf, sizeof (buf));
  TEST_ASSERT (memcmp (buf, external_1, length) == 0);

  TEST_ASSERT (free_count == 4);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 5);

  /* Test property access. */
  jerry_string_set_external_string_free_callback (NULL);
  external_string = jerry_create_external_string ((jerry_char_t *) external_4, NULL);
  other_string = jerry_create_string ((jerry_char_t *) external_4);

  jerry_value_t obj = jerry_create_object ();
  result = jerry_set_property (obj, external_string, other_string);
  TEST_ASSERT (jerry_value_is_boolean (result));
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  jerry_value_t get_result = jerry_get_property (obj, other_string);
  TEST_ASSERT (jerry_value_is_string (get_result));

  result = jerry_binary_operation (JERRY_BIN_OP_STRICT_EQUAL, get_result, external_string);
  jerry_release_value (get_result);
  TEST_ASSERT (jerry_value_is_boolean (result));
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  result = jerry_set_property (obj, other_string, external_string);
  TEST_ASSERT (jerry_value_is_boolean (result));
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  get_result = jerry_get_property (obj, external_string);
  TEST_ASSERT (jerry_value_is_string (get_result));

  result = jerry_binary_operation (JERRY_BIN_OP_STRICT_EQUAL, get_result, other_string);
  jerry_release_value (get_result);
  TEST_ASSERT (jerry_value_is_boolean (result));
  TEST_ASSERT (jerry_value_is_true (result));
  jerry_release_value (result);

  jerry_release_value (obj);
  jerry_release_value (external_string);
  jerry_release_value (other_string);

  jerry_cleanup ();
  return 0;
} /* main */

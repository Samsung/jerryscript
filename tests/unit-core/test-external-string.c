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
free_external1 (void *ptr)
{
  TEST_ASSERT (ptr == external_1);
  free_count++;
} /* free_external1 */

static void
free_external2 (void *ptr)
{
  TEST_ASSERT (ptr == external_2);
  free_count++;
} /* free_external2 */

static void
free_external3 (void *ptr)
{
  TEST_ASSERT (ptr == external_3);
  free_count++;
} /* free_external3 */

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  /* Test external callback calls. */
  jerry_value_t external_string = jerry_create_external_string ((jerry_char_t *) external_1, free_external1);
  TEST_ASSERT (free_count == 0);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 1);

  external_string = jerry_create_external_string ((jerry_char_t *) external_1, NULL);
  TEST_ASSERT (free_count == 1);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 1);

  external_string = jerry_create_external_string ((jerry_char_t *) external_2, free_external2);
  TEST_ASSERT (free_count == 2);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 2);

  external_string = jerry_create_external_string ((jerry_char_t *) external_2, NULL);
  TEST_ASSERT (free_count == 2);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 2);

  external_string = jerry_create_external_string ((jerry_char_t *) external_3, free_external3);
  TEST_ASSERT (free_count == 3);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 3);

  external_string = jerry_create_external_string ((jerry_char_t *) external_3, NULL);
  TEST_ASSERT (free_count == 3);
  jerry_release_value (external_string);
  TEST_ASSERT (free_count == 3);

  /* Test string comparison. */
  external_string = jerry_create_external_string ((jerry_char_t *) external_1, free_external1);
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
  external_string = jerry_create_external_string ((jerry_char_t *) external_1, free_external1);
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

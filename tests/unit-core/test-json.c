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

static jerry_value_t
custom_to_json (const jerry_call_info_t *call_info_p, /**< call information */
                const jerry_value_t args_p[], /**< arguments list */
                const jerry_length_t args_cnt) /**< arguments length */
{
  JERRY_UNUSED (call_info_p);
  JERRY_UNUSED (args_p);
  JERRY_UNUSED (args_cnt);

  return jerry_create_error (JERRY_ERROR_URI, (const jerry_char_t *) "Error");
} /* custom_to_json */

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  {
    /* JSON.parse check */
    const jerry_char_t data[] = "{\"name\": \"John\", \"age\": 5}";
    jerry_value_t parsed_json = jerry_json_parse (data, sizeof (data) - 1);

    /* Check "name" property values */
    jerry_value_t name_key = jerry_create_string ((const jerry_char_t *) "name");

    jerry_value_t has_name = jerry_has_property (parsed_json, name_key);
    TEST_ASSERT (jerry_value_is_true (has_name));
    jerry_release_value (has_name);

    jerry_value_t name_value = jerry_get_property (parsed_json, name_key);
    TEST_ASSERT (jerry_value_is_string (name_value) == true);

    jerry_size_t name_size = jerry_get_string_size (name_value);
    TEST_ASSERT (name_size == 4);
    JERRY_VLA (jerry_char_t, name_data, name_size + 1);
    jerry_size_t copied = jerry_string_to_char_buffer (name_value, name_data, name_size);
    name_data[name_size] = '\0';

    jerry_release_value (name_value);

    TEST_ASSERT (copied == name_size);
    TEST_ASSERT_STR ("John", name_data);
    jerry_release_value (name_key);

    /* Check "age" property values */
    jerry_value_t age_key = jerry_create_string ((const jerry_char_t *) "age");

    jerry_value_t has_age = jerry_has_property (parsed_json, age_key);
    TEST_ASSERT (jerry_value_is_true (has_age));
    jerry_release_value (has_age);

    jerry_value_t age_value = jerry_get_property (parsed_json, age_key);
    TEST_ASSERT (jerry_value_is_number (age_value) == true);
    TEST_ASSERT (jerry_get_number_value (age_value) == 5.0);

    jerry_release_value (age_value);
    jerry_release_value (age_key);

    jerry_release_value (parsed_json);
  }

  /* JSON.parse error checks */
  {
    jerry_value_t parsed_json = jerry_json_parse ((const jerry_char_t *) "", 0);
    TEST_ASSERT (jerry_value_is_error (parsed_json));
    TEST_ASSERT (jerry_get_error_type (parsed_json) == JERRY_ERROR_SYNTAX);
    jerry_release_value (parsed_json);
  }

  {
    jerry_value_t parsed_json = jerry_json_parse ((const jerry_char_t *) "-", 1);
    TEST_ASSERT (jerry_value_is_error (parsed_json));
    TEST_ASSERT (jerry_get_error_type (parsed_json) == JERRY_ERROR_SYNTAX);
    jerry_release_value (parsed_json);
  }

  /* JSON.stringify check */
  {
    jerry_value_t obj = jerry_create_object ();
    /* Fill "obj" with data */
    {
      jerry_value_t name_key = jerry_create_string ((const jerry_char_t *) "name");
      jerry_value_t name_value = jerry_create_string ((const jerry_char_t *) "John");
      jerry_value_t name_set = jerry_set_property (obj, name_key, name_value);
      TEST_ASSERT (!jerry_value_is_error (name_set));
      TEST_ASSERT (jerry_value_is_boolean (name_set));
      TEST_ASSERT (jerry_value_is_true (name_set));
      jerry_release_value (name_key);
      jerry_release_value (name_value);
      jerry_release_value (name_set);
    }
    {
      jerry_value_t age_key = jerry_create_string ((const jerry_char_t *) "age");
      jerry_value_t age_value = jerry_create_number (32);
      jerry_value_t age_set = jerry_set_property (obj, age_key, age_value);
      TEST_ASSERT (!jerry_value_is_error (age_set));
      TEST_ASSERT (jerry_value_is_boolean (age_set));
      TEST_ASSERT (jerry_value_is_true (age_set));
      jerry_release_value (age_key);
      jerry_release_value (age_value);
      jerry_release_value (age_set);
    }

    jerry_value_t json_string = jerry_json_stringify (obj);
    TEST_ASSERT (jerry_value_is_string (json_string));

    jerry_release_value (obj);

    const char check_value[] = "{\"name\":\"John\",\"age\":32}";
    jerry_size_t json_size = jerry_get_string_size (json_string);
    TEST_ASSERT (json_size == strlen (check_value));
    JERRY_VLA (jerry_char_t, json_data, json_size + 1);
    jerry_string_to_char_buffer (json_string, json_data, json_size);
    json_data[json_size] = '\0';

    TEST_ASSERT_STR (check_value, json_data);

    jerry_release_value (json_string);
  }

  /* Custom "toJSON" invocation test */
  {
    jerry_value_t obj = jerry_create_object ();
    /* Fill "obj" with data */
    {
      jerry_value_t name_key = jerry_create_string ((const jerry_char_t *) "toJSON");
      jerry_value_t name_value = jerry_create_external_function (custom_to_json);
      jerry_value_t name_set = jerry_set_property (obj, name_key, name_value);
      TEST_ASSERT (!jerry_value_is_error (name_set));
      TEST_ASSERT (jerry_value_is_boolean (name_set));
      TEST_ASSERT (jerry_value_is_true (name_set));
      jerry_release_value (name_key);
      jerry_release_value (name_value);
      jerry_release_value (name_set);
    }

    jerry_value_t json_string = jerry_json_stringify (obj);
    TEST_ASSERT (jerry_value_is_error (json_string));
    TEST_ASSERT (jerry_get_error_type (json_string) == JERRY_ERROR_URI);

    jerry_release_value (json_string);
    jerry_release_value (obj);
  }

  jerry_cleanup ();

  return 0;
} /* main */

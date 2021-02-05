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

#include "jerryscript-port-default.h"
#include "jerryscript-port.h"
#include "jerryscript.h"
#include "test-common.h"

int
main (void)
{
  if (!jerry_is_feature_enabled (JERRY_FEATURE_DATAVIEW))
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "DataView support is disabled!\n");
    return 0;
  }

  /* DataView builtin requires the TypedArray builtin */
  TEST_ASSERT (jerry_is_feature_enabled (JERRY_FEATURE_TYPEDARRAY));

  jerry_init (JERRY_INIT_EMPTY);

  /* Test accessors */
  jerry_value_t arraybuffer = jerry_create_arraybuffer (16);
  jerry_value_t view1 = jerry_create_dataview (arraybuffer, 0, 16);
  TEST_ASSERT (!jerry_value_is_error (view1));
  TEST_ASSERT (jerry_value_is_dataview (view1));

  jerry_length_t byteOffset = 0;
  jerry_length_t byteLength = 0;
  ;
  jerry_value_t internal_buffer = jerry_get_dataview_buffer (view1, &byteOffset, &byteLength);
  TEST_ASSERT (jerry_binary_operation (JERRY_BIN_OP_STRICT_EQUAL, internal_buffer, arraybuffer));
  TEST_ASSERT (byteOffset == 0);
  TEST_ASSERT (byteLength == 16);
  jerry_release_value (internal_buffer);

  jerry_value_t view2 = jerry_create_dataview (arraybuffer, 12, 4);
  TEST_ASSERT (!jerry_value_is_error (view1));
  TEST_ASSERT (jerry_value_is_dataview (view2));
  internal_buffer = jerry_get_dataview_buffer (view2, &byteOffset, &byteLength);
  TEST_ASSERT (jerry_binary_operation (JERRY_BIN_OP_STRICT_EQUAL, internal_buffer, arraybuffer));
  TEST_ASSERT (byteOffset == 12);
  TEST_ASSERT (byteLength == 4);
  jerry_release_value (internal_buffer);

  /* Test invalid construction */
  jerry_value_t empty_object = jerry_create_object ();
  jerry_value_t view3 = jerry_create_dataview (empty_object, 20, 10);
  TEST_ASSERT (jerry_value_is_error (view3));
  jerry_value_t error_obj = jerry_get_value_from_error (view3, true);
  TEST_ASSERT (jerry_get_error_type (error_obj) == JERRY_ERROR_TYPE);
  jerry_release_value (error_obj);
  jerry_release_value (empty_object);

  jerry_value_t view4 = jerry_create_dataview (arraybuffer, 20, 10);
  TEST_ASSERT (jerry_value_is_error (view3));
  error_obj = jerry_get_value_from_error (view4, true);
  TEST_ASSERT (jerry_get_error_type (error_obj) == JERRY_ERROR_RANGE);
  jerry_release_value (error_obj);

  /* Test getting/setting values */
  jerry_value_t global_obj = jerry_get_global_object ();
  jerry_value_t view1_str = jerry_create_string ((const jerry_char_t *) "view1");
  jerry_value_t view2_str = jerry_create_string ((const jerry_char_t *) "view2");
  TEST_ASSERT (jerry_set_property (global_obj, view1_str, view1));
  TEST_ASSERT (jerry_set_property (global_obj, view2_str, view2));

  jerry_release_value (view1_str);
  jerry_release_value (view2_str);
  jerry_release_value (global_obj);

  const jerry_char_t set_src[] = "view1.setInt16 (12, 255)";
  jerry_value_t res = jerry_eval (set_src, sizeof (set_src) - 1, JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (jerry_value_is_undefined (res));
  jerry_release_value (res);

  const jerry_char_t get_src[] = "view2.getInt16 (0)";
  res = jerry_eval (get_src, sizeof (get_src) - 1, JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (jerry_get_number_value (res) == 255);
  jerry_release_value (res);

  const jerry_char_t get_src_little_endian[] = "view2.getInt16 (0, true)";
  res = jerry_eval (get_src_little_endian, sizeof (get_src_little_endian) - 1, JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (jerry_get_number_value (res) == -256);
  jerry_release_value (res);

  /* Cleanup */
  jerry_release_value (view2);
  jerry_release_value (view1);
  jerry_release_value (arraybuffer);

  jerry_cleanup ();

  return 0;
} /* main */

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

#include <stdio.h>

/**
 * Type to describe test cases.
 */
typedef struct
{
  jerry_typedarray_type_t typedarray_type; /**< what kind of TypedArray */
  char *constructor_name; /**< JS constructor name for TypedArray */
  uint32_t element_count; /**< number of elements for the TypedArray */
  uint32_t bytes_per_element; /**< bytes per elment of the given typedarray_type */
} test_entry_t;

/**
 * Register a JavaScript value in the global object.
 */
static void
register_js_value (const char *name_p, /**< name of the function */
                    jerry_value_t value) /**< function callback */
{
  jerry_value_t global_obj_val = jerry_get_global_object ();

  jerry_value_t name_val = jerry_create_string ((const jerry_char_t *) name_p);
  jerry_value_t result_val = jerry_set_property (global_obj_val, name_val, value);

  jerry_release_value (name_val);
  jerry_release_value (global_obj_val);

  jerry_release_value (result_val);
} /* register_js_value */

static jerry_value_t
assert_handler (const jerry_call_info_t *call_info_p, /**< call information */
                const jerry_value_t args_p[], /**< function arguments */
                const jerry_length_t args_cnt) /**< number of function arguments */
{
  JERRY_UNUSED (call_info_p);

  if (jerry_value_is_true (args_p[0]))
  {
    return jerry_create_boolean (true);
  }
  else
  {
    if (args_cnt > 1
        && jerry_value_is_string (args_p[1]))
    {
      jerry_length_t utf8_sz = jerry_get_string_size (args_p[1]);
      TEST_ASSERT (utf8_sz <= 127); /* 127 is the expected max assert fail message size. */
      JERRY_VLA (char, string_from_utf8, utf8_sz + 1);
      string_from_utf8[utf8_sz] = 0;

      jerry_string_to_char_buffer (args_p[1], (jerry_char_t *) string_from_utf8, utf8_sz);

      printf ("JS assert: %s\n", string_from_utf8);
    }
    TEST_ASSERT (false);
  }
} /* assert_handler */

/**
 * Do simple TypedArray property validation.
 */
static void
test_typedarray_info (jerry_value_t typedarray, /**< target TypedArray to query */
                      jerry_typedarray_type_t typedarray_type, /**< expected TypedArray type */
                      jerry_length_t element_count, /**< expected element count */
                      jerry_length_t bytes_per_element) /**< bytes per element for the given type */
{
  TEST_ASSERT (!jerry_value_is_error (typedarray));
  TEST_ASSERT (jerry_value_is_typedarray (typedarray));
  TEST_ASSERT (jerry_get_typedarray_type (typedarray) == typedarray_type);
  TEST_ASSERT (jerry_get_typedarray_length (typedarray) == element_count);

  jerry_length_t byte_length = (uint32_t) -1;
  jerry_length_t byte_offset = (uint32_t) -1;
  jerry_value_t arraybuffer = jerry_get_typedarray_buffer (typedarray, &byte_offset, &byte_length);
  TEST_ASSERT (jerry_value_is_arraybuffer (arraybuffer));

  TEST_ASSERT (byte_length == element_count * bytes_per_element);
  TEST_ASSERT (byte_offset == 0);

  jerry_release_value (arraybuffer);
} /* test_typedarray_info */

/**
 * Test construction of TypedArrays and validate properties.
 */
static void
test_typedarray_queries (test_entry_t test_entries[]) /**< test cases */
{
  jerry_value_t global_obj_val = jerry_get_global_object ();

  for (uint32_t i = 0; test_entries[i].constructor_name != NULL; i++)
  {
    /* Create TypedArray via construct call */
    {
      jerry_value_t prop_name = jerry_create_string ((const jerry_char_t *) test_entries[i].constructor_name);
      jerry_value_t prop_value = jerry_get_property (global_obj_val, prop_name);
      TEST_ASSERT (!jerry_value_is_error (prop_value));
      jerry_value_t length_arg = jerry_create_number (test_entries[i].element_count);

      jerry_value_t typedarray = jerry_construct_object (prop_value, &length_arg, 1);

      jerry_release_value (prop_name);
      jerry_release_value (prop_value);
      jerry_release_value (length_arg);

      test_typedarray_info (typedarray,
                            test_entries[i].typedarray_type,
                            test_entries[i].element_count,
                            test_entries[i].bytes_per_element);
      jerry_release_value (typedarray);
    }

    /* Create TypedArray via api call */
    {
      jerry_value_t typedarray = jerry_create_typedarray (test_entries[i].typedarray_type,
                                                          test_entries[i].element_count);
      test_typedarray_info (typedarray,
                            test_entries[i].typedarray_type,
                            test_entries[i].element_count,
                            test_entries[i].bytes_per_element);
      jerry_release_value (typedarray);
    }
  }

  jerry_release_value (global_obj_val);
} /* test_typedarray_queries */

/**
 * Test value at given position in the buffer based on TypedArray type.
 */
static
void test_buffer_value (uint64_t value, /**< value to test for */
                        const void *buffer, /**< buffer to read value from */
                        uint32_t start_offset, /**< start offset of the value */
                        jerry_typedarray_type_t typedarray_type, /**< type of TypedArray */
                        uint32_t bytes_per_element) /**< bytes per element for the given type */
{
  uint32_t offset = start_offset / bytes_per_element;

#define TEST_VALUE_AT(TYPE, BUFFER, OFFSET, VALUE) TEST_ASSERT (((TYPE *) BUFFER)[OFFSET] == (TYPE) (VALUE))

  switch (typedarray_type)
  {
    case JERRY_TYPEDARRAY_UINT8:      TEST_VALUE_AT (uint8_t,  buffer, offset, value); break;
    case JERRY_TYPEDARRAY_INT8:       TEST_VALUE_AT (int8_t,   buffer, offset, value); break;
    case JERRY_TYPEDARRAY_UINT16:     TEST_VALUE_AT (uint16_t, buffer, offset, value); break;
    case JERRY_TYPEDARRAY_INT16:      TEST_VALUE_AT (int16_t,  buffer, offset, value); break;
    case JERRY_TYPEDARRAY_UINT32:     TEST_VALUE_AT (uint32_t, buffer, offset, value); break;
    case JERRY_TYPEDARRAY_INT32:      TEST_VALUE_AT (int32_t,  buffer, offset, value); break;
    case JERRY_TYPEDARRAY_FLOAT32:    TEST_VALUE_AT (float,    buffer, offset, value); break;
    case JERRY_TYPEDARRAY_FLOAT64:    TEST_VALUE_AT (double,   buffer, offset, value); break;
    case JERRY_TYPEDARRAY_BIGINT64:   TEST_VALUE_AT (int64_t,  buffer, offset, value); break;
    case JERRY_TYPEDARRAY_BIGUINT64:  TEST_VALUE_AT (uint64_t, buffer, offset, value); break;

    case JERRY_TYPEDARRAY_UINT8CLAMPED:
    {
      int64_t signed_value = (int64_t) value;
      uint8_t expected = (uint8_t) value;

      /* clamp the value if required*/
      if (signed_value > 0xFF)
      {
        expected = 0xFF;
      }
      else if (signed_value < 0)
      {
        expected = 0;
      }

      TEST_VALUE_AT (uint8_t, buffer, offset, expected); break;
    }
    default: TEST_ASSERT (false); break;
  }

#undef TEST_VALUE_AT
} /* test_buffer_value */

static void
test_typedarray_complex_creation (test_entry_t test_entries[], /**< test cases */
                                  bool use_external_buffer) /**< run tests using arraybuffer with external memory */
{
  const uint32_t arraybuffer_size = 256;

  JERRY_VLA (uint8_t, buffer_ext, arraybuffer_size);
  memset (buffer_ext, 0, arraybuffer_size);

  for (uint32_t i = 0; test_entries[i].constructor_name != NULL; i++)
  {
    const uint32_t offset = 8;
    uint32_t element_count = test_entries[i].element_count;
    uint32_t bytes_per_element = test_entries[i].bytes_per_element;

    /* new %TypedArray% (buffer, offset, length); */
    jerry_value_t typedarray;
    {
      jerry_value_t arraybuffer;

      if (use_external_buffer)
      {
        arraybuffer = jerry_create_arraybuffer_external (arraybuffer_size, buffer_ext, NULL);
      }
      else
      {
        arraybuffer = jerry_create_arraybuffer (arraybuffer_size);
      }

      jerry_value_t js_offset = jerry_create_number (offset);
      jerry_value_t js_element_count = jerry_create_number (element_count);

      register_js_value ("expected_offset", js_offset);
      register_js_value ("expected_length", js_element_count);

      typedarray = jerry_create_typedarray_for_arraybuffer_sz (test_entries[i].typedarray_type,
                                                               arraybuffer,
                                                               offset,
                                                               element_count);
      TEST_ASSERT (!jerry_value_is_error (typedarray));

      jerry_release_value (js_offset);
      jerry_release_value (js_element_count);
      jerry_release_value (arraybuffer);
    }

    register_js_value ("array", typedarray);

    const jerry_char_t test_exptected_src[] = TEST_STRING_LITERAL (
      "assert (array.length == expected_length,"
      "        'expected length: ' + expected_length + ' got: ' + array.length);"
      "assert (array.byteOffset == expected_offset);"
    );
    jerry_value_t result = jerry_eval (test_exptected_src,
                                       sizeof (test_exptected_src) - 1,
                                       JERRY_PARSE_STRICT_MODE);
    TEST_ASSERT (!jerry_value_is_error (result));
    jerry_release_value (result);

    const jerry_char_t set_element_src[] = TEST_STRING_LITERAL (
      "array[0] = 0x11223344n"
    );

    /* crop the last 'n' character */
    size_t src_length = sizeof (set_element_src) - 2;

    if (test_entries[i].typedarray_type >= JERRY_TYPEDARRAY_BIGINT64)
    {
      /* use the last 'n' character */
      src_length++;
    }

    result = jerry_eval (set_element_src, src_length, JERRY_PARSE_STRICT_MODE);
    TEST_ASSERT (!jerry_value_is_error (result));
    jerry_release_value (result);

    {
      jerry_length_t byte_length = 0;
      jerry_length_t byte_offset = 0;
      jerry_value_t buffer = jerry_get_typedarray_buffer (typedarray, &byte_offset, &byte_length);
      TEST_ASSERT (byte_length == element_count * bytes_per_element);
      TEST_ASSERT (byte_offset == offset);

      JERRY_VLA (uint8_t, test_buffer, arraybuffer_size);

      jerry_typedarray_type_t type = jerry_get_typedarray_type (typedarray);
      jerry_value_t read_count = jerry_arraybuffer_read (buffer, 0, test_buffer, offset + byte_length);
      TEST_ASSERT (read_count == offset + byte_length);
      test_buffer_value (0x11223344, test_buffer, offset, type, bytes_per_element);

      if (use_external_buffer)
      {
        test_buffer_value (0x11223344, buffer_ext, offset, type, bytes_per_element);
        TEST_ASSERT (memcmp (buffer_ext, test_buffer, offset + byte_length) == 0);
      }

      jerry_release_value (buffer);
    }

    jerry_release_value (typedarray);
  }
} /* test_typedarray_complex_creation */

/**
 * Test get/set/delete property by index.
 */
static void test_property_by_index (test_entry_t test_entries[])
{
  int test_int_numbers[5] = {-5, -70, 13, 0, 56};
  double test_double_numbers[5] = {-83.153, -35.15, 0, 13.1, 89.8975};
  uint8_t test_uint_numbers[5] = {83, 15, 36, 0, 43};
  uint64_t test_uint64_numbers[5] = {83, 0, 1, UINT32_MAX, UINT64_MAX};
  int64_t test_int64_numbers[5] = {INT64_MAX, INT64_MIN, 0, INT32_MAX, INT32_MIN};

  for (uint32_t i = 0; test_entries[i].constructor_name != NULL; i++)
  {
    jerry_value_t test_number;
    uint32_t test_numbers_length = sizeof (test_int_numbers) / sizeof (int);
    jerry_value_t typedarray = jerry_create_typedarray (test_entries[i].typedarray_type, test_numbers_length);
    jerry_typedarray_type_t type = jerry_get_typedarray_type (typedarray);

    jerry_value_t set_result;
    jerry_value_t get_result;

    switch (type)
    {
      case JERRY_TYPEDARRAY_INT8:
      case JERRY_TYPEDARRAY_INT16:
      case JERRY_TYPEDARRAY_INT32:
      {
        for (uint8_t j = 0; j < test_numbers_length; j++)
        {
          test_number = jerry_create_number (test_int_numbers[j]);
          TEST_ASSERT (!jerry_delete_property_by_index (typedarray, j));
          set_result = jerry_set_property_by_index (typedarray, j, test_number);
          get_result = jerry_get_property_by_index (typedarray, j);

          TEST_ASSERT (jerry_value_is_boolean (set_result));
          TEST_ASSERT (jerry_value_is_true (set_result));
          TEST_ASSERT (!jerry_delete_property_by_index (typedarray, j));
          TEST_ASSERT (jerry_get_number_value (get_result) == test_int_numbers[j]);

          jerry_release_value (test_number);
          jerry_release_value (set_result);
          jerry_release_value (get_result);
        }
        break;
      }
      case JERRY_TYPEDARRAY_FLOAT32:
      case JERRY_TYPEDARRAY_FLOAT64:
      {
        for (uint8_t j = 0; j < test_numbers_length; j++)
        {
          test_number = jerry_create_number (test_double_numbers[j]);
          TEST_ASSERT (!jerry_delete_property_by_index (typedarray, j));
          set_result = jerry_set_property_by_index (typedarray, j, test_number);
          get_result = jerry_get_property_by_index (typedarray, j);

          TEST_ASSERT (jerry_value_is_boolean (set_result));
          TEST_ASSERT (jerry_value_is_true (set_result));
          TEST_ASSERT (!jerry_delete_property_by_index (typedarray, j));

          double epsilon = pow (10, -5);
          double get_abs = fabs (jerry_get_number_value (get_result) - test_double_numbers[j]);
          TEST_ASSERT (get_abs < epsilon);

          jerry_release_value (test_number);
          jerry_release_value (set_result);
          jerry_release_value (get_result);

          /* Testing positive and negative infinity */
          for (uint8_t k = 0; k < 2; k++)
          {
            jerry_value_t inf = jerry_create_number_infinity (k);
            jerry_value_t set_inf = jerry_set_property_by_index (typedarray, 0, inf);
            TEST_ASSERT (jerry_value_is_boolean (set_inf));
            TEST_ASSERT (jerry_value_is_true (set_inf));
            jerry_value_t get_inf = jerry_get_property_by_index (typedarray, 0);
            TEST_ASSERT (isinf (jerry_get_number_value (get_inf)));

            jerry_release_value (inf);
            jerry_release_value (set_inf);
            jerry_release_value (get_inf);
          }
        }
        break;
      }
      case JERRY_TYPEDARRAY_BIGINT64:
      {
        for (uint8_t j = 0; j < test_numbers_length; j++)
        {
          test_number = jerry_create_bigint ((uint64_t *) &test_int64_numbers[j], 1, true);
          TEST_ASSERT (!jerry_delete_property_by_index (typedarray, j));
          set_result = jerry_set_property_by_index (typedarray, j, test_number);
          get_result = jerry_get_property_by_index (typedarray, j);

          TEST_ASSERT (jerry_value_is_boolean (set_result));
          TEST_ASSERT (jerry_value_is_true (set_result));
          TEST_ASSERT (!jerry_delete_property_by_index (typedarray, j));
          int64_t get_number;
          bool sign;
          jerry_get_bigint_digits (get_result, (uint64_t *) &get_number, 1, &sign);

          TEST_ASSERT (sign ? get_number : -get_number == test_int64_numbers[j]);

          jerry_release_value (test_number);
          jerry_release_value (set_result);
          jerry_release_value (get_result);
        }
        break;
      }
      case JERRY_TYPEDARRAY_BIGUINT64:
      {
        for (uint8_t j = 0; j < test_numbers_length; j++)
        {
          test_number = jerry_create_bigint (&test_uint64_numbers[j], 1, false);
          TEST_ASSERT (!jerry_delete_property_by_index (typedarray, j));
          set_result = jerry_set_property_by_index (typedarray, j, test_number);
          get_result = jerry_get_property_by_index (typedarray, j);

          TEST_ASSERT (jerry_value_is_boolean (set_result));
          TEST_ASSERT (jerry_value_is_true (set_result));
          TEST_ASSERT (!jerry_delete_property_by_index (typedarray, j));
          uint64_t get_number;
          bool sign;
          jerry_get_bigint_digits (get_result, &get_number, 1, &sign);

          TEST_ASSERT (get_number == test_uint64_numbers[j]);

          jerry_release_value (test_number);
          jerry_release_value (set_result);
          jerry_release_value (get_result);
        }
        break;
      }
      default:
      {
        for (uint8_t j = 0; j < test_numbers_length; j++)
        {
          test_number = jerry_create_number (test_uint_numbers[j]);
          TEST_ASSERT (!jerry_delete_property_by_index (typedarray, j));
          set_result = jerry_set_property_by_index (typedarray, j, test_number);
          get_result = jerry_get_property_by_index (typedarray, j);

          TEST_ASSERT (jerry_value_is_boolean (set_result));
          TEST_ASSERT (jerry_value_is_true (set_result));
          TEST_ASSERT (!jerry_delete_property_by_index (typedarray, j));
          TEST_ASSERT (jerry_get_number_value (get_result) == test_uint_numbers[j]);

          jerry_release_value (test_number);
          jerry_release_value (set_result);
          jerry_release_value (get_result);
        }
        break;
      }
    }

    jerry_value_t set_undefined = jerry_set_property_by_index (typedarray, 100, jerry_create_number (50));

    if (type == JERRY_TYPEDARRAY_BIGINT64 || type == JERRY_TYPEDARRAY_BIGUINT64)
    {
      TEST_ASSERT (jerry_value_is_error (set_undefined));
    }
    else
    {
      TEST_ASSERT (jerry_value_is_boolean (set_undefined) && !jerry_value_is_true (set_undefined));
    }

    jerry_value_t get_undefined = jerry_get_property_by_index (typedarray, 100);

    if (type == JERRY_TYPEDARRAY_BIGINT64 || type == JERRY_TYPEDARRAY_BIGUINT64)
    {
      TEST_ASSERT (jerry_value_is_error (set_undefined));
    }
    else
    {
      TEST_ASSERT (jerry_value_is_undefined (get_undefined));
    }

    TEST_ASSERT (jerry_value_is_undefined (get_undefined));
    jerry_release_value (set_undefined);
    jerry_release_value (get_undefined);
    jerry_release_value (typedarray);
  }
} /* test_property_by_index */

static void
test_detached_arraybuffer (void)
{
  static jerry_typedarray_type_t types[] =
  {
    JERRY_TYPEDARRAY_UINT8,
    JERRY_TYPEDARRAY_UINT8CLAMPED,
    JERRY_TYPEDARRAY_INT8,
    JERRY_TYPEDARRAY_UINT16,
    JERRY_TYPEDARRAY_INT16,
    JERRY_TYPEDARRAY_UINT32,
    JERRY_TYPEDARRAY_INT32,
    JERRY_TYPEDARRAY_FLOAT32,
    JERRY_TYPEDARRAY_FLOAT64,
    JERRY_TYPEDARRAY_BIGINT64,
    JERRY_TYPEDARRAY_BIGUINT64,
  };

  /* Creating an TypedArray for a detached array buffer with a given length/offset is invalid */
  {
    uint8_t buf[1];
    const uint32_t length = 1;
    jerry_value_t arraybuffer = jerry_create_arraybuffer_external (length, buf, NULL);
    TEST_ASSERT (!jerry_value_is_error (arraybuffer));
    TEST_ASSERT (jerry_value_is_arraybuffer (arraybuffer));
    TEST_ASSERT (jerry_get_arraybuffer_byte_length (arraybuffer) == length);

    jerry_value_t is_detachable = jerry_is_arraybuffer_detachable (arraybuffer);
    TEST_ASSERT (!jerry_value_is_error (is_detachable));
    TEST_ASSERT (jerry_value_is_true (is_detachable));
    jerry_release_value (is_detachable);

    jerry_value_t res = jerry_detach_arraybuffer (arraybuffer);
    TEST_ASSERT (!jerry_value_is_error (res));
    jerry_release_value (res);

    for (size_t idx = 0; idx < (sizeof (types) / sizeof (types[0])); idx++)
    {
      jerry_value_t typedarray = jerry_create_typedarray_for_arraybuffer_sz (types[idx], arraybuffer, 0, 4);
      TEST_ASSERT (jerry_value_is_error (typedarray));
      TEST_ASSERT (jerry_get_error_type (typedarray) == JERRY_ERROR_TYPE);
      jerry_release_value (typedarray);
    }

    jerry_release_value (arraybuffer);
  }

  /* Creating an TypedArray for a detached array buffer without length/offset is valid */
  {
    uint8_t buf[1];
    const uint32_t length = 1;
    jerry_value_t arraybuffer = jerry_create_arraybuffer_external (length, buf, NULL);
    TEST_ASSERT (!jerry_value_is_error (arraybuffer));
    TEST_ASSERT (jerry_value_is_arraybuffer (arraybuffer));
    TEST_ASSERT (jerry_get_arraybuffer_byte_length (arraybuffer) == length);

    jerry_value_t is_detachable = jerry_is_arraybuffer_detachable (arraybuffer);
    TEST_ASSERT (!jerry_value_is_error (is_detachable));
    TEST_ASSERT (jerry_value_is_true (is_detachable));
    jerry_release_value (is_detachable);

    jerry_value_t res = jerry_detach_arraybuffer (arraybuffer);
    TEST_ASSERT (!jerry_value_is_error (res));
    jerry_release_value (res);

    for (size_t idx = 0; idx < (sizeof (types) / sizeof (types[0])); idx++)
    {
      jerry_value_t typedarray = jerry_create_typedarray_for_arraybuffer (types[idx], arraybuffer);
      TEST_ASSERT (jerry_value_is_error (typedarray));
      TEST_ASSERT (jerry_get_error_type (typedarray) == JERRY_ERROR_TYPE);
      jerry_release_value (typedarray);
    }

    jerry_release_value (arraybuffer);
  }
} /* test_detached_arraybuffer */

int
main (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  if (!jerry_is_feature_enabled (JERRY_FEATURE_TYPEDARRAY))
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "TypedArray is disabled!\n");
    jerry_cleanup ();
    return 0;
  }

  jerry_value_t function_val = jerry_create_external_function (assert_handler);
  register_js_value ("assert", function_val);
  jerry_release_value (function_val);

  test_entry_t test_entries[] =
  {
#define TEST_ENTRY(TYPE, CONSTRUCTOR, COUNT, BYTES_PER_ELEMENT) \
      { TYPE, CONSTRUCTOR, COUNT, BYTES_PER_ELEMENT }

    TEST_ENTRY (JERRY_TYPEDARRAY_UINT8,        "Uint8Array",        12, 1),
    TEST_ENTRY (JERRY_TYPEDARRAY_UINT8CLAMPED, "Uint8ClampedArray", 12, 1),
    TEST_ENTRY (JERRY_TYPEDARRAY_INT8,         "Int8Array",         12, 1),
    TEST_ENTRY (JERRY_TYPEDARRAY_UINT16,       "Uint16Array",       12, 2),
    TEST_ENTRY (JERRY_TYPEDARRAY_INT16,        "Int16Array",        12, 2),
    TEST_ENTRY (JERRY_TYPEDARRAY_UINT16,       "Uint16Array",       12, 2),
    TEST_ENTRY (JERRY_TYPEDARRAY_INT32,        "Int32Array",        12, 4),
    TEST_ENTRY (JERRY_TYPEDARRAY_UINT32,       "Uint32Array",       12, 4),
    TEST_ENTRY (JERRY_TYPEDARRAY_FLOAT32,      "Float32Array",      12, 4),
  /* TODO: add check if the float64 is supported */
    TEST_ENTRY (JERRY_TYPEDARRAY_FLOAT64,      "Float64Array",      12, 8),
    TEST_ENTRY (JERRY_TYPEDARRAY_BIGINT64,     "BigInt64Array",     12, 8),
    TEST_ENTRY (JERRY_TYPEDARRAY_BIGUINT64,    "BigUint64Array",    12, 8),

    TEST_ENTRY (JERRY_TYPEDARRAY_INVALID, NULL, 0, 0)
#undef TEST_ENTRY
  };

  /* Test TypedArray queries */
  test_typedarray_queries (test_entries);

  /* Test TypedArray operations in js */
  {
    const uint32_t element_count = 14;

    jerry_value_t array = jerry_create_typedarray (JERRY_TYPEDARRAY_UINT8, element_count);

    {
      uint8_t expected_value = 42;
      JERRY_VLA (uint8_t, expected_data, element_count);
      memset (expected_data, expected_value, element_count);

      jerry_length_t byte_length;
      jerry_length_t offset;
      jerry_value_t buffer = jerry_get_typedarray_buffer (array, &offset, &byte_length);
      TEST_ASSERT (byte_length == element_count);
      jerry_length_t written = jerry_arraybuffer_write (buffer, offset, expected_data, element_count);
      TEST_ASSERT (written == element_count);
      jerry_release_value (buffer);

      jerry_value_t js_element_count = jerry_create_number (element_count);
      jerry_value_t js_expected_value = jerry_create_number (expected_value);

      register_js_value ("array", array);
      register_js_value ("expected_length", js_element_count);
      register_js_value ("expected_value", js_expected_value);

      jerry_release_value (js_element_count);
      jerry_release_value (js_expected_value);
    }

    /* Check read and to write */
    const jerry_char_t eval_src[] = TEST_STRING_LITERAL (
      "assert (array.length == expected_length, 'expected length: ' + expected_length + ' got: ' + array.length);"
      "for (var i = 0; i < array.length; i++)"
      "{"
      "  assert (array[i] == expected_value);"
      "  array[i] = i;"
      "};"
    );
    jerry_value_t result = jerry_eval (eval_src,
                                       sizeof (eval_src) - 1,
                                       JERRY_PARSE_STRICT_MODE);

    TEST_ASSERT (!jerry_value_is_error (result));
    jerry_release_value (result);

    /* Check write results */
    {
      jerry_length_t byte_length;
      jerry_length_t offset;
      jerry_value_t buffer = jerry_get_typedarray_buffer (array, &offset, &byte_length);
      TEST_ASSERT (byte_length == element_count);

      JERRY_VLA (uint8_t, result_data, element_count);

      jerry_length_t read_count = jerry_arraybuffer_read (buffer, offset, result_data, byte_length);
      TEST_ASSERT (read_count == byte_length);

      for (uint8_t i = 0; i < read_count; i++)
      {
        TEST_ASSERT (result_data[i] == i);
      }

      jerry_release_value (buffer);
    }

    jerry_release_value (array);
  }

  test_typedarray_complex_creation (test_entries, false);
  test_typedarray_complex_creation (test_entries, true);

  test_property_by_index (test_entries);

  /* test invalid things */
  {
    jerry_value_t values[] =
    {
      jerry_create_number (11),
      jerry_create_boolean (false),
      jerry_create_string ((const jerry_char_t *) "test"),
      jerry_create_object (),
      jerry_create_null (),
      jerry_create_arraybuffer (16),
      jerry_create_error (JERRY_ERROR_TYPE, (const jerry_char_t *) "error"),
      jerry_create_undefined (),
      jerry_create_promise (),
    };

    for (size_t idx = 0; idx < sizeof (values) / sizeof (values[0]); idx++)
    {
      /* A non-TypedArray object should not be regarded a TypedArray. */
      bool is_typedarray = jerry_value_is_typedarray (values[idx]);
      TEST_ASSERT (is_typedarray == false);

      /* JERRY_TYPEDARRAY_INVALID should be returned for non-TypedArray objects */
      jerry_typedarray_type_t type = jerry_get_typedarray_type (values[idx]);
      TEST_ASSERT (type == JERRY_TYPEDARRAY_INVALID);

      /* Zero should be returned for non-TypedArray objects */
      jerry_length_t length = jerry_get_typedarray_length (values[idx]);
      TEST_ASSERT (length == 0);

      /**
       * Getting the ArrayBuffer from a non-TypedArray object(s) should return an error
       * and should not modify the output parameter values.
       */
      {
        jerry_length_t offset = 22;
        jerry_length_t byte_count = 23;
        jerry_value_t error = jerry_get_typedarray_buffer (values[idx], &offset, &byte_count);
        TEST_ASSERT (jerry_value_is_error (error));
        TEST_ASSERT (offset == 22);
        TEST_ASSERT (byte_count == 23);
        jerry_release_value (error);
      }

      /**
       * Creating a TypedArray from a non-ArrayBuffer should result an error.
       */
      if (!jerry_value_is_arraybuffer (values[idx]))
      {
        jerry_value_t error = jerry_create_typedarray_for_arraybuffer (JERRY_TYPEDARRAY_UINT8, values[idx]);
        TEST_ASSERT (jerry_value_is_error (error));
        jerry_release_value (error);
      }

      jerry_release_value (values[idx]);
    }
  }

  test_detached_arraybuffer ();

  jerry_cleanup ();

  return 0;
} /* main */

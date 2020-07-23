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
#include "jerryscript-core.h"
#include "jerryscript-ext/handler.h"
#include "jext-common.h"
#include "jerryscript-port.h"

#include <inttypes.h>

typedef enum
{
  JERRYX_OP_EQUAL = 0,
  JERRYX_OP_STRICT_EQUAL = 1,
  JERRYX_OP_NOT_EQUAL = 2,
  JERRYX_OP_NOT_STRICT_EQUAL = 3
} jerryx_equality_operation_t;

JERRYX_STATIC_ASSERT ((int) (JERRYX_OP_EQUAL) == (int) (JERRY_BIN_OP_EQUAL),
                      jerryx_op_equal_must_be_equal_to_jerry_bin_op_equal);

JERRYX_STATIC_ASSERT ((int) (JERRYX_OP_STRICT_EQUAL) == (int) (JERRY_BIN_OP_STRICT_EQUAL),
                      jerryx_op_strict_equal_must_be_equal_to_jerry_bin_op_strict_equal);

/**
 * Function to handler assertion failures, with an optional error message.
 */
static void
jerryx_fatal (const jerry_value_t message) /** < message to print */
{
  if (jerry_value_is_string (message))
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Assertion failed: ");
    jerry_length_t remaining_length = jerry_get_string_length (message);
    jerry_char_t buff[256];
    jerry_length_t current_index = 0;

    while (remaining_length > 255)
    {
      jerry_substring_to_char_buffer (message,
                                      current_index,
                                      current_index + 255,
                                      buff,
                                      sizeof (buff));
      buff[255] = '\0';
      jerry_port_log (JERRY_LOG_LEVEL_ERROR, "%s", buff);
      current_index += 255;
      remaining_length -= 255;
    }
    jerry_substring_to_char_buffer (message,
                                    current_index,
                                    current_index + remaining_length,
                                    buff,
                                    sizeof (buff));
    buff[remaining_length] = '\0';
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "%s\n", buff);
  }

  /* Assert failed, print a bit of JS backtrace */
  jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Script Error: assertion failed\n");

  if (jerry_is_feature_enabled (JERRY_FEATURE_LINE_INFO))
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Script backtrace (top 5):\n");

    /* If the line info feature is disabled an empty array will be returned. */
    jerry_value_t backtrace_array = jerry_get_backtrace (5);
    uint32_t array_length = jerry_get_array_length (backtrace_array);

    for (uint32_t idx = 0; idx < array_length; idx++)
    {
      jerry_value_t property = jerry_get_property_by_index (backtrace_array, idx);

      jerry_length_t total_size = jerry_get_utf8_string_size (property);
      jerry_length_t current_size = 0;
      jerry_char_t string_buffer[64];
      const jerry_length_t copy_size = (jerry_length_t) (sizeof (string_buffer) - 1);

      /* On some systems the uint32_t values can't be printed with "%u" and
       * on some systems it can be printed. To avoid differences in the uint32_t typdef
       * The "PRIu32" macro is used to correctly add the formatter.
       */
      jerry_port_log (JERRY_LOG_LEVEL_ERROR, " %"PRIu32": ", idx);
      do
      {
        jerry_size_t copied_bytes = jerry_substring_to_utf8_char_buffer (property,
                                                                         current_size,
                                                                         current_size + copy_size,
                                                                         string_buffer,
                                                                         copy_size);
        string_buffer[copied_bytes] = '\0';
        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "%s", string_buffer);

        current_size += copied_bytes;
      }
      while (total_size != current_size);
      jerry_port_log (JERRY_LOG_LEVEL_ERROR, "\n");

      jerry_release_value (property);
    }

    jerry_release_value (backtrace_array);
  }

  jerry_port_fatal (ERR_FAILED_INTERNAL_ASSERTION);
} /* jerryx_fatal */

/**
 * Hard assert for scripts. The routine calls jerry_port_fatal on assertion failure.
 *
 * Notes:
 *  * If the `JERRY_FEATURE_LINE_INFO` runtime feature is enabled (build option: `JERRY_LINE_INFO`)
 *    a backtrace is also printed out.
 *
 * @return true - if only one argument was passed and that argument was a boolean true.
 *         Note that the function does not return otherwise.
 */
jerry_value_t
jerryx_handler_assert_fatal (const jerry_value_t func_obj_val, /**< function object */
                             const jerry_value_t this_p, /**< this arg */
                             const jerry_value_t args_p[], /**< function arguments */
                             const jerry_length_t args_cnt) /**< number of function arguments */
{
  (void) func_obj_val; /* unused */
  (void) this_p; /* unused */

  if (args_cnt == 1
      && jerry_value_is_boolean (args_p[0])
      && jerry_get_boolean_value (args_p[0]))
  {
    return jerry_create_boolean (true);
  }

  jerryx_fatal (jerry_create_null ());

  return jerry_create_boolean (false);
} /* jerryx_handler_assert_fatal */

/**
 * Soft assert for scripts. The routine throws an error on assertion failure.
 *
 * @return true - if only one argument was passed and that argument was a boolean true.
 *         error - otherwise.
 */
jerry_value_t
jerryx_handler_assert_throw (const jerry_value_t func_obj_val, /**< function object */
                             const jerry_value_t this_p, /**< this arg */
                             const jerry_value_t args_p[], /**< function arguments */
                             const jerry_length_t args_cnt) /**< number of function arguments */
{
  (void) func_obj_val; /* unused */
  (void) this_p; /* unused */

  if (args_cnt == 1
      && jerry_value_is_boolean (args_p[0])
      && jerry_get_boolean_value (args_p[0]))
  {
    return jerry_create_boolean (true);
  }

  return jerry_create_error (JERRY_ERROR_COMMON, (jerry_char_t *) "assertion failed");
} /* jerryx_handler_assert_throw */

/**
 * An alias to `jerryx_handler_assert_fatal`.
 *
 * @return true - if only one argument was passed and that argument was a boolean true.
 *         Note that the function does not return otherwise.
 */
jerry_value_t
jerryx_handler_assert (const jerry_value_t func_obj_val, /**< function object */
                       const jerry_value_t this_p, /**< this arg */
                       const jerry_value_t args_p[], /**< function arguments */
                       const jerry_length_t args_cnt) /**< number of function arguments */
{
  return jerryx_handler_assert_fatal (func_obj_val, this_p, args_p, args_cnt);
} /* jerryx_handler_assert */

/**
 * Assert equal helper which includes the implementation of the equal, strictEqual,
 * notEqual, notStrictEqual functions.
 */
static jerry_value_t
jerryx_handler_assert_equal_helper (const jerry_value_t func_obj_val, /**< function object */
                                    const jerry_value_t this_p, /**< this arg */
                                    const jerry_value_t args_p[], /**< function arguments */
                                    const jerry_length_t args_cnt, /**< number of function arguments */
                                    jerryx_equality_operation_t type) /** < type of equality */
{
  (void) func_obj_val; /* unused */
  (void) this_p; /* unused */

  if (args_cnt < 2)
  {
    return jerry_create_error (JERRY_ERROR_TYPE, (const jerry_char_t *) "Expected at least 2 arguments.");
  }

  jerry_value_t actual = args_p[0];
  jerry_value_t expected = args_p[1];

  jerry_binary_operation_t op = JERRY_BIN_OP_EQUAL;
  switch (type)
  {
    case JERRYX_OP_EQUAL:
    case JERRYX_OP_NOT_EQUAL:
    {
      op = JERRY_BIN_OP_EQUAL;
      break;
    }
    case JERRYX_OP_STRICT_EQUAL:
    case JERRYX_OP_NOT_STRICT_EQUAL:
    {
      op = JERRY_BIN_OP_STRICT_EQUAL;
      break;
    }
    default:
    {
      JERRYX_UNREACHABLE ();
    }
  }

  jerry_value_t result = jerry_binary_operation (op, actual, expected);
  bool bool_result = jerry_get_boolean_value (result);

  if (!jerry_value_is_boolean (result) || type > JERRYX_OP_STRICT_EQUAL ? bool_result : !bool_result)
  {
    if (args_cnt > 2)
    {
      jerryx_fatal (args_p[2]);
    }

    jerryx_fatal (jerry_create_null ());
  }

  jerry_release_value (result);

  return jerry_create_boolean (true);
} /* jerryx_handler_assert_equal_helper */

/**
 * Handler for the assert equal operation.
 *
 * @return true - if the two arguments are equal.
 *         otherwise - this function calls jerryx_fatal.
 *         Note that in this case the function does not return.
 */
static jerry_value_t
jerryx_handler_assert_equal (const jerry_value_t func_obj_val, /**< function object */
                             const jerry_value_t this_p, /**< this arg */
                             const jerry_value_t args_p[], /**< function arguments */
                             const jerry_length_t args_cnt) /**< number of function arguments */
{
  return jerryx_handler_assert_equal_helper (func_obj_val, this_p, args_p, args_cnt, JERRYX_OP_EQUAL);
} /* jerryx_handler_assert_equal */

/**
 * Handler for the assert strict equal operation.
 *
 * @return true - if the two arguments are strict equal.
 *         otherwise - this function calls jerryx_fatal.
 *         Note that in this case the function does not return.
 */
static jerry_value_t
jerryx_handler_assert_strict_equal (const jerry_value_t func_obj_val, /**< function object */
                                    const jerry_value_t this_p, /**< this arg */
                                    const jerry_value_t args_p[], /**< function arguments */
                                    const jerry_length_t args_cnt) /**< number of function arguments */
{
  return jerryx_handler_assert_equal_helper (func_obj_val, this_p, args_p, args_cnt, JERRYX_OP_STRICT_EQUAL);
} /* jerryx_handler_assert_strict_equal */

/**
 * Handler for the assert not equal operation.
 *
 * @return true - if the two arguments are not equal.
 *         otherwise - this function calls jerryx_fatal.
 *         Note that in this case the function does not return.
 */
static jerry_value_t
jerryx_handler_assert_not_equal (const jerry_value_t func_obj_val, /**< function object */
                                 const jerry_value_t this_p, /**< this arg */
                                 const jerry_value_t args_p[], /**< function arguments */
                                 const jerry_length_t args_cnt) /**< number of function arguments */
{
  return jerryx_handler_assert_equal_helper (func_obj_val, this_p, args_p, args_cnt, JERRYX_OP_NOT_EQUAL);
} /* jerryx_handler_assert_not_equal */

/**
 * Handler for the assert not strict equal operation.
 *
 * @return true - if the two arguments are not strict equal.
 *         otherwise - this function calls jerryx_fatal.
 *         Note that in this case the function does not return.
 */
static jerry_value_t
jerryx_handler_assert_not_strict_equal (const jerry_value_t func_obj_val, /**< function object */
                                        const jerry_value_t this_p, /**< this arg */
                                        const jerry_value_t args_p[], /**< function arguments */
                                        const jerry_length_t args_cnt) /**< number of function arguments */
{
  return jerryx_handler_assert_equal_helper (func_obj_val, this_p, args_p, args_cnt, JERRYX_OP_NOT_STRICT_EQUAL);
} /* jerryx_handler_assert_not_strict_equal */

/**
 * Handler to cause an assertion failure.
 */
static jerry_value_t
jerryx_handler_assert_fail (const jerry_value_t func_obj_val, /**< function object */
                            const jerry_value_t this_p, /**< this arg */
                            const jerry_value_t args_p[], /**< function arguments */
                            const jerry_length_t args_cnt) /**< number of function arguments */
{
  (void) func_obj_val; /* unused */
  (void) this_p; /* unused */

  if (args_cnt > 0)
  {
    jerryx_fatal (args_p[0]);
  }

  jerryx_fatal (jerry_create_null ());
  return jerry_create_null ();
} /* jerryx_handler_assert_fail */

/**
 * Handler function to check if a function throws an error.
 */
static jerry_value_t
jerryx_handler_assert_throws (const jerry_value_t func_obj_val, /**< function object */
                              const jerry_value_t this_p, /**< this arg */
                              const jerry_value_t args_p[], /**< function arguments */
                              const jerry_length_t args_cnt) /**< number of function arguments */
{
  (void) func_obj_val; /* unused */
  (void) this_p; /* unused */

  if (args_cnt > 0 && jerry_value_is_function (args_p[0]))
  {
    jerry_value_t value = jerry_call_function (args_p[0], jerry_create_undefined (), NULL, 0);

    if (jerry_value_is_error (value))
    {
      if (args_cnt <= 1)
      {
        jerry_release_value (value);
        return jerry_create_boolean (true);
      }

      jerry_binary_operation_t operation;

      if (!jerry_value_is_object (args_p[1]))
      {
        operation = JERRY_BIN_OP_STRICT_EQUAL;
      }
      else
      {
        operation = JERRY_BIN_OP_INSTANCEOF;
      }
      jerry_value_t result = jerry_get_value_from_error (value, true);
      jerry_value_t comparison = jerry_binary_operation (operation, result, args_p[1]);
      jerry_release_value (result);
      if (jerry_get_boolean_value (comparison))
      {
        return jerry_create_boolean (true);
      }
    }
    else
    {
      jerry_release_value (value);
    }
  }

  if (args_cnt > 2)
  {
    jerryx_fatal (args_p[2]);
  }

  jerryx_fatal (jerry_create_null ());
  return jerry_create_null ();
} /* jerryx_handler_assert_throws */

/**
 * Create the Assert object and register it into the Global object.
 */
void
jerryx_register_assert_object (void)
{
  jerry_value_t global_object = jerry_get_global_object ();
  jerry_value_t assert_object = jerry_create_object ();

  jerryx_handler_register_object (assert_object, (const jerry_char_t *) "equal", jerryx_handler_assert_equal);
  jerryx_handler_register_object (assert_object, (const jerry_char_t *) "strictEqual",
                                  jerryx_handler_assert_strict_equal);
  jerryx_handler_register_object (assert_object, (const jerry_char_t *) "notEqual",
                                  jerryx_handler_assert_not_equal);
  jerryx_handler_register_object (assert_object, (const jerry_char_t *) "notStrictEqual",
                                  jerryx_handler_assert_not_strict_equal);
  jerryx_handler_register_object (assert_object, (const jerry_char_t *) "fail",
                                  jerryx_handler_assert_fail);
  jerryx_handler_register_object (assert_object, (const jerry_char_t *) "throws",
                                  jerryx_handler_assert_throws);

  jerry_value_t prop_name = jerry_create_string ((const jerry_char_t *) "Assert");
  jerry_set_property (global_object, prop_name, assert_object);

  jerry_release_value (prop_name);
  jerry_release_value (assert_object);
  jerry_release_value (global_object);
} /* jerryx_register_assert_object */

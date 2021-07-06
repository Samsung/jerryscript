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

#include <string.h>

#include "jerryscript.h"
#include "test-common.h"

static void
compare_specifier (jerry_value_t specifier, /* string value */
                   int id) /* module id */
{
  jerry_char_t string[] = "XX_module.mjs";

  TEST_ASSERT (id >= 1 && id <= 99 && string[0] == 'X' && string[1] == 'X');

  string[0] = (jerry_char_t) ((id / 10) + '0');
  string[1] = (jerry_char_t) ((id % 10) + '0');

  jerry_size_t length = (jerry_size_t) (sizeof (string) - 1);
  jerry_char_t buffer[sizeof (string) - 1];

  TEST_ASSERT (jerry_value_is_string (specifier));
  TEST_ASSERT (jerry_get_string_size (specifier) == length);

  TEST_ASSERT (jerry_string_to_char_buffer (specifier, buffer, length) == length);
  TEST_ASSERT (memcmp (buffer, string, length) == 0);
} /* compare_specifier */

static void
compare_property (jerry_value_t namespace_object, /**< namespace object */
                  const char *name_p, /**< property name */
                  double expected_value) /**< property value (number for simplicity) */
{
  jerry_value_t name = jerry_create_string ((const jerry_char_t *) name_p);
  jerry_value_t result = jerry_get_property (namespace_object, name);

  TEST_ASSERT (jerry_value_is_number (result));
  TEST_ASSERT (jerry_get_number_value (result) == expected_value);

  jerry_release_value (result);
  jerry_release_value (name);
} /* compare_property */

static jerry_value_t
create_module (int id) /**< module id */
{
  jerry_parse_options_t module_parse_options;
  module_parse_options.options = JERRY_PARSE_MODULE;

  jerry_value_t result;

  if (id == 0)
  {
    result = jerry_parse ((jerry_char_t *) "", 0, &module_parse_options);
  }
  else
  {
    jerry_char_t source[] = "import a from 'XX_module.mjs'";

    TEST_ASSERT (id >= 1 && id <= 99 && source[15] == 'X' && source[16] == 'X');

    source[15] = (jerry_char_t) ((id / 10) + '0');
    source[16] = (jerry_char_t) ((id % 10) + '0');

    result = jerry_parse (source, sizeof (source) - 1, &module_parse_options);
  }

  TEST_ASSERT (!jerry_value_is_error (result));
  return result;
} /* create_module */

static int counter = 0;
static jerry_value_t module;

static jerry_value_t
resolve_callback1 (const jerry_value_t specifier, /**< module specifier */
                   const jerry_value_t referrer, /**< parent module */
                   void *user_p) /**< user data */
{
  TEST_ASSERT (user_p == (void *) &module);
  TEST_ASSERT (referrer == module);
  compare_specifier (specifier, 1);

  counter++;
  return counter == 1 ? jerry_create_number (7) : jerry_create_object ();
} /* resolve_callback1 */

static jerry_value_t prev_module;
static bool terminate_with_error;

static jerry_value_t
resolve_callback2 (const jerry_value_t specifier, /**< module specifier */
                   const jerry_value_t referrer, /**< parent module */
                   void *user_p) /**< user data */
{
  TEST_ASSERT (prev_module == referrer);
  TEST_ASSERT (user_p == NULL);

  compare_specifier (specifier, ++counter);

  if (counter >= 32)
  {
    if (terminate_with_error)
    {
      return jerry_create_error (JERRY_ERROR_RANGE, (const jerry_char_t *) "Module not found");
    }

    return create_module (0);
  }

  prev_module = create_module (counter + 1);
  return prev_module;
} /* resolve_callback2 */

static jerry_value_t
resolve_callback3 (const jerry_value_t specifier, /**< module specifier */
                   const jerry_value_t referrer, /**< parent module */
                   void *user_p) /**< user data */
{
  (void) specifier;
  (void) referrer;
  (void) user_p;

  TEST_ASSERT (false);
} /* resolve_callback3 */

static jerry_value_t
native_module_evaluate (const jerry_value_t native_module) /**< native module */
{
  ++counter;

  TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_EVALUATING);

  jerry_value_t exp_val = jerry_create_string ((const jerry_char_t *) "exp");
  jerry_value_t other_exp_val = jerry_create_string ((const jerry_char_t *) "other_exp");
  /* The native module has no such export. */
  jerry_value_t no_exp_val = jerry_create_string ((const jerry_char_t *) "no_exp");

  jerry_value_t result = jerry_native_module_get_export (native_module, exp_val);
  TEST_ASSERT (jerry_value_is_undefined (result));
  jerry_release_value (result);

  result = jerry_native_module_get_export (native_module, other_exp_val);
  TEST_ASSERT (jerry_value_is_undefined (result));
  jerry_release_value (result);

  result = jerry_native_module_get_export (native_module, no_exp_val);
  TEST_ASSERT (jerry_value_is_error (result));
  TEST_ASSERT (jerry_get_error_type (result) == JERRY_ERROR_REFERENCE);
  jerry_release_value (result);

  jerry_value_t export = jerry_create_number (3.5);
  result = jerry_native_module_set_export (native_module, exp_val, export);
  TEST_ASSERT (jerry_value_is_boolean (result) && jerry_value_is_true (result));
  jerry_release_value (result);
  jerry_release_value (export);

  export = jerry_create_string ((const jerry_char_t *) "str");
  result = jerry_native_module_set_export (native_module, other_exp_val, export);
  TEST_ASSERT (jerry_value_is_boolean (result) && jerry_value_is_true (result));
  jerry_release_value (result);
  jerry_release_value (export);

  result = jerry_native_module_set_export (native_module, no_exp_val, no_exp_val);
  TEST_ASSERT (jerry_value_is_error (result));
  TEST_ASSERT (jerry_get_error_type (result) == JERRY_ERROR_REFERENCE);
  jerry_release_value (result);

  result = jerry_native_module_get_export (native_module, exp_val);
  TEST_ASSERT (jerry_value_is_number (result) && jerry_get_number_value (result) == 3.5);
  jerry_release_value (result);

  result = jerry_native_module_get_export (native_module, other_exp_val);
  TEST_ASSERT (jerry_value_is_string (result));
  jerry_release_value (result);

  jerry_release_value (exp_val);
  jerry_release_value (other_exp_val);
  jerry_release_value (no_exp_val);

  if (counter == 4)
  {
    ++counter;
    return jerry_create_error (JERRY_ERROR_COMMON, (const jerry_char_t *) "Ooops!");
  }

  return jerry_create_undefined ();
} /* native_module_evaluate */

static jerry_value_t
resolve_callback4 (const jerry_value_t specifier, /**< module specifier */
                   const jerry_value_t referrer, /**< parent module */
                   void *user_p) /**< user data */
{
  (void) specifier;
  (void) referrer;

  ++counter;

  jerry_value_t exports[2] =
  {
    jerry_create_string ((const jerry_char_t *) "exp"),
    jerry_create_string ((const jerry_char_t *) "other_exp")
  };

  jerry_value_t native_module = jerry_native_module_create (native_module_evaluate, exports, 2);
  TEST_ASSERT (!jerry_value_is_error (native_module));

  jerry_release_value (exports[0]);
  jerry_release_value (exports[1]);

  *((jerry_value_t *) user_p) = jerry_acquire_value (native_module);
  return native_module;
} /* resolve_callback4 */

static void
module_state_changed (jerry_module_state_t new_state, /**< new state of the module */
                      const jerry_value_t module_val, /**< a module whose state is changed */
                      const jerry_value_t value, /**< value argument */
                      void *user_p) /**< user pointer */
{
  TEST_ASSERT (jerry_module_get_state (module_val) == new_state);
  TEST_ASSERT (module_val == module);
  TEST_ASSERT (user_p == (void *) &counter);

  ++counter;

  switch (counter)
  {
    case 1:
    case 3:
    {
      TEST_ASSERT (new_state == JERRY_MODULE_STATE_LINKED);
      TEST_ASSERT (jerry_value_is_undefined (value));
      break;
    }
    case 2:
    {
      TEST_ASSERT (new_state == JERRY_MODULE_STATE_EVALUATED);
      TEST_ASSERT (jerry_value_is_number (value) && jerry_get_number_value (value) == 33.5);
      break;
    }
    default:
    {
      TEST_ASSERT (counter == 4);
      TEST_ASSERT (new_state == JERRY_MODULE_STATE_ERROR);
      TEST_ASSERT (jerry_value_is_number (value) && jerry_get_number_value (value) == -5.5);
      break;
    }
  }
} /* module_state_changed */

int
main (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  if (!jerry_is_feature_enabled (JERRY_FEATURE_MODULE))
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Module is disabled!\n");
    jerry_cleanup ();
    return 0;
  }

  jerry_value_t number = jerry_create_number (5);
  jerry_value_t object = jerry_create_object ();

  jerry_value_t result = jerry_module_link (number, resolve_callback1, NULL);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);

  result = jerry_module_link (object, resolve_callback1, NULL);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);

  module = create_module (1);

  /* After an error, module must remain in unlinked mode. */
  result = jerry_module_link (module, resolve_callback1, (void *) &module);
  TEST_ASSERT (jerry_value_is_error (result));
  TEST_ASSERT (counter == 1);
  jerry_release_value (result);

  result = jerry_module_link (module, resolve_callback1, (void *) &module);
  TEST_ASSERT (jerry_value_is_error (result));
  TEST_ASSERT (counter == 2);
  jerry_release_value (result);

  prev_module = module;
  counter = 0;
  terminate_with_error = true;
  result = jerry_module_link (module, resolve_callback2, NULL);
  TEST_ASSERT (jerry_value_is_error (result));
  TEST_ASSERT (counter == 32);
  jerry_release_value (result);

  /* The successfully resolved modules is kept around in unlinked state. */
  jerry_gc (JERRY_GC_PRESSURE_HIGH);

  counter = 31;
  terminate_with_error = false;
  result = jerry_module_link (module, resolve_callback2, NULL);
  TEST_ASSERT (jerry_value_is_boolean (result) && jerry_value_is_true (result));
  TEST_ASSERT (counter == 32);
  jerry_release_value (result);

  TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_LINKED);
  TEST_ASSERT (jerry_module_get_number_of_requests (module) == 1);
  result = jerry_module_get_request (module, 0);
  TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_LINKED);
  jerry_release_value (result);

  jerry_release_value (module);

  module = create_module (1);

  prev_module = module;
  counter = 0;
  terminate_with_error = false;
  result = jerry_module_link (module, resolve_callback2, NULL);
  TEST_ASSERT (jerry_value_is_boolean (result) && jerry_value_is_true (result));
  TEST_ASSERT (counter == 32);
  jerry_release_value (result);
  jerry_release_value (module);

  TEST_ASSERT (jerry_module_get_state (number) == JERRY_MODULE_STATE_INVALID);

  jerry_parse_options_t module_parse_options;
  module_parse_options.options = JERRY_PARSE_MODULE;

  jerry_char_t source1[] = TEST_STRING_LITERAL (
    "import a from '16_module.mjs'\n"
    "export * from '07_module.mjs'\n"
    "export * from '44_module.mjs'\n"
    "import * as b from '36_module.mjs'\n"
  );
  module = jerry_parse (source1, sizeof (source1) - 1, &module_parse_options);
  TEST_ASSERT (!jerry_value_is_error (module));
  TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_UNLINKED);

  TEST_ASSERT (jerry_module_get_number_of_requests (number) == 0);
  TEST_ASSERT (jerry_module_get_number_of_requests (module) == 4);

  result = jerry_module_get_request (object, 0);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);

  result = jerry_module_get_request (module, 0);
  compare_specifier (result, 16);
  jerry_release_value (result);

  result = jerry_module_get_request (module, 1);
  compare_specifier (result, 7);
  jerry_release_value (result);

  result = jerry_module_get_request (module, 2);
  compare_specifier (result, 44);
  jerry_release_value (result);

  result = jerry_module_get_request (module, 3);
  compare_specifier (result, 36);
  jerry_release_value (result);

  result = jerry_module_get_request (module, 4);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);

  jerry_release_value (module);

  result = jerry_module_get_namespace (number);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);

  jerry_char_t source2[] = TEST_STRING_LITERAL (
    "export let a = 6\n"
    "export let b = 8.5\n"
  );
  module = jerry_parse (source2, sizeof (source2) - 1, &module_parse_options);
  TEST_ASSERT (!jerry_value_is_error (module));
  TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_UNLINKED);

  result = jerry_module_link (module, resolve_callback3, NULL);
  TEST_ASSERT (!jerry_value_is_error (result));
  jerry_release_value (result);

  TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_LINKED);

  result = jerry_module_evaluate (module);
  TEST_ASSERT (!jerry_value_is_error (result));
  jerry_release_value (result);

  TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_EVALUATED);

  result = jerry_module_get_namespace (module);
  TEST_ASSERT (jerry_value_is_object (result));
  compare_property (result, "a", 6);
  compare_property (result, "b", 8.5);
  jerry_release_value (result);

  jerry_release_value (module);

  module = jerry_native_module_create (NULL, &object, 1);
  TEST_ASSERT (jerry_value_is_error (module));
  jerry_release_value (module);

  module = jerry_native_module_create (NULL, NULL, 0);
  TEST_ASSERT (!jerry_value_is_error (module));
  TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_UNLINKED);

  result = jerry_native_module_get_export (object, number);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);

  result = jerry_native_module_set_export (module, number, number);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);

  jerry_release_value (module);

  /* Valid identifier. */
  jerry_value_t export = jerry_create_string ((const jerry_char_t *) "\xed\xa0\x83\xed\xb2\x80");

  module = jerry_native_module_create (NULL, &export, 1);
  TEST_ASSERT (!jerry_value_is_error (module));
  TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_UNLINKED);

  result = jerry_module_link (module, NULL, NULL);
  TEST_ASSERT (jerry_value_is_boolean (result) && jerry_value_is_true (result));
  jerry_release_value (result);

  result = jerry_module_evaluate (module);
  TEST_ASSERT (jerry_value_is_undefined (result));
  jerry_release_value (result);

  jerry_release_value (module);
  jerry_release_value (export);

  /* Invalid identifiers. */
  export = jerry_create_string ((const jerry_char_t *) "a+");
  module = jerry_native_module_create (NULL, &export, 1);
  TEST_ASSERT (jerry_value_is_error (module));
  jerry_release_value (module);
  jerry_release_value (export);

  export = jerry_create_string ((const jerry_char_t *) "\xed\xa0\x80");
  module = jerry_native_module_create (NULL, &export, 1);
  TEST_ASSERT (jerry_value_is_error (module));
  jerry_release_value (module);
  jerry_release_value (export);

  counter = 0;

  for (int i = 0; i < 2; i++)
  {
    jerry_char_t source3[] = TEST_STRING_LITERAL (
      "import {exp, other_exp as other} from 'native.js'\n"
      "import * as namespace from 'native.js'\n"
      "if (exp !== 3.5 || other !== 'str') { throw 'Assertion failed!' }\n"
      "if (namespace.exp !== 3.5 || namespace.other_exp !== 'str') { throw 'Assertion failed!' }\n"
    );
    module = jerry_parse (source3, sizeof (source3) - 1, &module_parse_options);
    TEST_ASSERT (!jerry_value_is_error (module));
    TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_UNLINKED);

    jerry_value_t native_module;

    result = jerry_module_link (module, resolve_callback4, (void *) &native_module);
    TEST_ASSERT (!jerry_value_is_error (result));
    jerry_release_value (result);

    TEST_ASSERT (counter == i * 2 + 1);
    TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_LINKED);
    TEST_ASSERT (jerry_module_get_state (native_module) == JERRY_MODULE_STATE_LINKED);

    result = jerry_module_evaluate (module);

    if (i == 0)
    {
      TEST_ASSERT (!jerry_value_is_error (result));
      TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_EVALUATED);
      TEST_ASSERT (jerry_module_get_state (native_module) == JERRY_MODULE_STATE_EVALUATED);
      TEST_ASSERT (counter == 2);
    }
    else
    {
      TEST_ASSERT (jerry_value_is_error (result));
      TEST_ASSERT (jerry_module_get_state (module) == JERRY_MODULE_STATE_ERROR);
      TEST_ASSERT (jerry_module_get_state (native_module) == JERRY_MODULE_STATE_ERROR);
      TEST_ASSERT (counter == 5);
    }

    jerry_release_value (result);
    jerry_release_value (module);
    jerry_release_value (native_module);
  }

  jerry_release_value (object);
  jerry_release_value (number);

  counter = 0;
  jerry_module_set_state_changed_callback (module_state_changed, (void *) &counter);

  jerry_char_t source4[] = TEST_STRING_LITERAL (
    "33.5\n"
  );
  module = jerry_parse (source4, sizeof (source4) - 1, &module_parse_options);

  result = jerry_module_link (module, NULL, NULL);
  TEST_ASSERT (!jerry_value_is_error (result));
  jerry_release_value (result);

  result = jerry_module_evaluate (module);
  TEST_ASSERT (!jerry_value_is_error (result));
  jerry_release_value (result);

  jerry_release_value (module);

  jerry_char_t source5[] = TEST_STRING_LITERAL (
    "throw -5.5\n"
  );
  module = jerry_parse (source5, sizeof (source5) - 1, &module_parse_options);

  result = jerry_module_link (module, NULL, NULL);
  TEST_ASSERT (!jerry_value_is_error (result));
  jerry_release_value (result);

  result = jerry_module_evaluate (module);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);

  jerry_release_value (module);

  jerry_module_set_state_changed_callback (NULL, NULL);

  TEST_ASSERT (counter == 4);

  jerry_cleanup ();

  return 0;
} /* main */

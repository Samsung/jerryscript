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

typedef struct
{
  jerry_object_type_t type_info;
  jerry_value_t value;
  bool active;
} test_entry_t;

#define ENTRY(TYPE, VALUE) { TYPE, VALUE, true }
#define ENTRY_IF(TYPE, VALUE, FEATURE) { TYPE, VALUE, jerry_is_feature_enabled (FEATURE) }
#define EVALUATE(BUFF) (jerry_eval ((BUFF), sizeof ((BUFF)) - 1, JERRY_PARSE_NO_OPTS))
#define PARSE(OPTS) (jerry_parse ((const jerry_char_t *) "", 0, (OPTS)))
static jerry_value_t
test_ext_function (const jerry_call_info_t *call_info_p, /**< call information */
                   const jerry_value_t args_p[], /**< array of arguments */
                   const jerry_length_t args_cnt) /**< number of arguments */
{
  (void) call_info_p;
  (void) args_p;
  (void) args_cnt;
  return jerry_create_boolean (true);
} /* test_ext_function */

static jerry_object_type_t
test_namespace (const jerry_parse_options_t module_parse_options) /** module options */
{
  jerry_value_t module = jerry_parse ((const jerry_char_t *) "", 0, &module_parse_options);
  jerry_value_t module_linked = jerry_module_link (module, NULL, NULL);
  jerry_object_type_t namespace = jerry_module_get_namespace (module);
  jerry_release_value (module_linked);
  jerry_release_value (module);
  return namespace;
} /* test_namespace */

static jerry_value_t
test_dataview (void)
{
  jerry_value_t arraybuffer = jerry_create_arraybuffer (10);
  jerry_value_t dataview = jerry_create_dataview (arraybuffer, 0, 4);

  jerry_release_value (arraybuffer);

  return dataview;
} /* test_dataview */

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  const jerry_char_t proxy_object[] = "new Proxy({}, {})";
  const jerry_char_t typedarray_object[] = "new Uint8Array()";
  const jerry_char_t container_object[] = "new Map()";
  const jerry_char_t iterator_object[] = "[1, 2, 3].values()";
  const jerry_char_t arrow_function[] = "_ => 5";
  const jerry_char_t async_arrow_function[] = "async _ => 5";
  const jerry_char_t generator_function[] = "function *f() {}; f";
  const jerry_char_t async_generator_function[] = "async function *f() {}; f";
  const jerry_char_t getter_function[] = "Object.getOwnPropertyDescriptor({get a(){}}, 'a').get";
  const jerry_char_t setter_function[] = "Object.getOwnPropertyDescriptor({set a(b){}}, 'a').set";
  const jerry_char_t method_function[] = "Object.getOwnPropertyDescriptor({a(){}}, 'a').value";

  const jerry_char_t symbol_object[] = "new Object(Symbol('foo'))";
  const jerry_char_t generator_object[] = "function *f() { yield 5 }; f()";
  const jerry_char_t bigint_object[] = "Object(5n)";

  const jerry_char_t builtin_function[] = "Object";
  const jerry_char_t simple_function[] = "function f() {}; f";
  const jerry_char_t bound_function[] = "function f() {}; f.bind(1,2)";
  const jerry_char_t mapped_arguments[] = "function f(a, b) { return arguments; }; f()";
  const jerry_char_t unmapped_arguments[] = "function f(a, b) {'use strict'; return arguments; }; f()";
  const jerry_char_t boolean_object[] = "new Boolean(true)";
  const jerry_char_t date_object[] = "new Date()";
  const jerry_char_t number_object[] = "new Number(5)";
  const jerry_char_t regexp_object[] = "new RegExp()";
  const jerry_char_t string_object[] = "new String('foo')";
  const jerry_char_t weak_ref_object[] = "new WeakRef({})";
  const jerry_char_t error_object[] = "new Error()";

  jerry_parse_options_t module_parse_options;
  module_parse_options.options = JERRY_PARSE_MODULE;

  test_entry_t entries[] =
  {
    ENTRY (JERRY_OBJECT_TYPE_NONE, jerry_create_number (-33.0)),
    ENTRY (JERRY_OBJECT_TYPE_NONE, jerry_create_boolean (true)),
    ENTRY (JERRY_OBJECT_TYPE_NONE, jerry_create_undefined ()),
    ENTRY (JERRY_OBJECT_TYPE_NONE, jerry_create_null ()),
    ENTRY (JERRY_OBJECT_TYPE_NONE, jerry_create_string ((const jerry_char_t *) "foo")),
    ENTRY (JERRY_OBJECT_TYPE_NONE, jerry_create_error (JERRY_ERROR_TYPE, (const jerry_char_t *) "error")),

    ENTRY (JERRY_OBJECT_TYPE_GENERIC, jerry_create_object ()),
    ENTRY_IF (JERRY_OBJECT_TYPE_MODULE_NAMESPACE, test_namespace (module_parse_options), JERRY_FEATURE_MODULE),
    ENTRY (JERRY_OBJECT_TYPE_ARRAY, jerry_create_array (10)),

    ENTRY_IF (JERRY_OBJECT_TYPE_PROXY, EVALUATE (proxy_object), JERRY_FEATURE_PROXY),
    ENTRY_IF (JERRY_OBJECT_TYPE_TYPEDARRAY, EVALUATE (typedarray_object), JERRY_FEATURE_TYPEDARRAY),
    ENTRY_IF (JERRY_OBJECT_TYPE_CONTAINER, EVALUATE (container_object), JERRY_FEATURE_MAP),
    ENTRY_IF (JERRY_OBJECT_TYPE_ITERATOR, EVALUATE (iterator_object), JERRY_FEATURE_SYMBOL),

    ENTRY (JERRY_OBJECT_TYPE_SCRIPT, PARSE (NULL)),
    ENTRY_IF (JERRY_OBJECT_TYPE_MODULE, PARSE (&module_parse_options), JERRY_FEATURE_MODULE),
    ENTRY_IF (JERRY_OBJECT_TYPE_PROMISE, jerry_create_promise (), JERRY_FEATURE_PROMISE),
    ENTRY_IF (JERRY_OBJECT_TYPE_DATAVIEW, test_dataview (), JERRY_FEATURE_DATAVIEW),
    ENTRY_IF (JERRY_OBJECT_TYPE_FUNCTION, EVALUATE (arrow_function), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_OBJECT_TYPE_FUNCTION, EVALUATE (async_arrow_function), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_OBJECT_TYPE_FUNCTION, EVALUATE (generator_function), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_OBJECT_TYPE_FUNCTION, EVALUATE (async_generator_function), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_OBJECT_TYPE_FUNCTION, EVALUATE (method_function), JERRY_FEATURE_SYMBOL),
    ENTRY (JERRY_OBJECT_TYPE_FUNCTION, EVALUATE (builtin_function)),
    ENTRY (JERRY_OBJECT_TYPE_FUNCTION, EVALUATE (simple_function)),
    ENTRY (JERRY_OBJECT_TYPE_FUNCTION, EVALUATE (bound_function)),
    ENTRY (JERRY_OBJECT_TYPE_FUNCTION, jerry_create_external_function (test_ext_function)),
    ENTRY (JERRY_OBJECT_TYPE_FUNCTION, EVALUATE (getter_function)),
    ENTRY (JERRY_OBJECT_TYPE_FUNCTION, EVALUATE (setter_function)),
    ENTRY_IF (JERRY_OBJECT_TYPE_ERROR, EVALUATE (error_object), JERRY_FEATURE_ERROR_MESSAGES),
    ENTRY_IF (JERRY_OBJECT_TYPE_ARRAYBUFFER, jerry_create_arraybuffer (10), JERRY_FEATURE_TYPEDARRAY),

    ENTRY (JERRY_OBJECT_TYPE_ARGUMENTS, EVALUATE (mapped_arguments)),
    ENTRY (JERRY_OBJECT_TYPE_ARGUMENTS, EVALUATE (unmapped_arguments)),
    ENTRY (JERRY_OBJECT_TYPE_BOOLEAN, EVALUATE (boolean_object)),
    ENTRY (JERRY_OBJECT_TYPE_DATE, EVALUATE (date_object)),
    ENTRY (JERRY_OBJECT_TYPE_NUMBER, EVALUATE (number_object)),
    ENTRY (JERRY_OBJECT_TYPE_REGEXP, EVALUATE (regexp_object)),
    ENTRY (JERRY_OBJECT_TYPE_STRING, EVALUATE (string_object)),
    ENTRY_IF (JERRY_OBJECT_TYPE_SYMBOL, EVALUATE (symbol_object), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_OBJECT_TYPE_GENERATOR, EVALUATE (generator_object), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_OBJECT_TYPE_BIGINT, EVALUATE (bigint_object), JERRY_FEATURE_BIGINT),
    ENTRY_IF (JERRY_OBJECT_TYPE_WEAKREF, EVALUATE (weak_ref_object), JERRY_FEATURE_WEAKREF),
  };

  for (size_t idx = 0; idx < sizeof (entries) / sizeof (entries[0]); idx++)
  {
    jerry_object_type_t type_info = jerry_object_get_type (entries[idx].value);

    TEST_ASSERT (!entries[idx].active || type_info == entries[idx].type_info);
    jerry_release_value (entries[idx].value);
  }

  if (jerry_is_feature_enabled (JERRY_FEATURE_REALM))
  {
    jerry_value_t new_realm = jerry_create_realm ();
    jerry_object_type_t new_realm_object_type = jerry_object_get_type (new_realm);
    TEST_ASSERT (new_realm_object_type == JERRY_OBJECT_TYPE_GENERIC);

    jerry_value_t old_realm = jerry_set_realm (new_realm);
    jerry_object_type_t old_realm_object_type = jerry_object_get_type (old_realm);
    TEST_ASSERT (old_realm_object_type == JERRY_OBJECT_TYPE_GENERIC);

    jerry_set_realm (old_realm);

    jerry_release_value (new_realm);
  }

  jerry_cleanup ();

  return 0;
} /* main */

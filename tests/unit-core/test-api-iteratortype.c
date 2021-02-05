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

typedef struct
{
  jerry_iterator_type_t type_info;
  jerry_value_t value;
  bool active;
} test_entry_t;

#define ENTRY(TYPE, VALUE) \
  {                        \
    TYPE, VALUE, true      \
  }
#define ENTRY_IF(TYPE, VALUE, FEATURE)              \
  {                                                 \
    TYPE, VALUE, jerry_is_feature_enabled (FEATURE) \
  }
#define EVALUATE(BUFF) (jerry_eval ((BUFF), sizeof ((BUFF)) - 1, JERRY_PARSE_NO_OPTS))

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  const jerry_char_t array_iterator_keys[] = "[1, 2, 3].keys()";
  const jerry_char_t array_iterator_values[] = "[1, 2, 3].values()";
  const jerry_char_t array_iterator_entries[] = "[1, 2, 3].entries()";
  const jerry_char_t array_iterator_symbol_iterator[] = "([1, 2, 3])[Symbol.iterator]()";

  const jerry_char_t typedarray_iterator_keys[] = "new Uint8Array([1, 2, 3]).keys()";
  const jerry_char_t typedarray_iterator_values[] = "new Uint8Array([1, 2, 3]).values()";
  const jerry_char_t typedarray_iterator_entries[] = "new Uint8Array([1, 2, 3]).entries()";
  const jerry_char_t typedarray_iterator_symbol_iterator[] = "new Uint8Array([1, 2, 3])[Symbol.iterator]()";

  const jerry_char_t string_symbol_iterator[] = "('foo')[Symbol.iterator]()";

  const jerry_char_t map_iterator_keys[] = "new Map([1, 2, 3].entries()).keys()";
  const jerry_char_t map_iterator_values[] = "new Map([1, 2, 3].entries()).values()";
  const jerry_char_t map_iterator_entries[] = "new Map([1, 2, 3].entries()).entries()";
  const jerry_char_t map_iterator_symbol_iterator[] = "new Map([1, 2, 3].entries())[Symbol.iterator]()";

  const jerry_char_t set_iterator_keys[] = "new Set([1, 2, 3]).keys()";
  const jerry_char_t set_iterator_values[] = "new Set([1, 2, 3]).values()";
  const jerry_char_t set_iterator_entries[] = "new Set([1, 2, 3]).entries()";
  const jerry_char_t set_iterator_symbol_iterator[] = "new Set([1, 2, 3])[Symbol.iterator]()";

  test_entry_t entries[] = {
    ENTRY (JERRY_ITERATOR_TYPE_NONE, jerry_create_number (-33.0)),
    ENTRY (JERRY_ITERATOR_TYPE_NONE, jerry_create_boolean (true)),
    ENTRY (JERRY_ITERATOR_TYPE_NONE, jerry_create_undefined ()),
    ENTRY (JERRY_ITERATOR_TYPE_NONE, jerry_create_null ()),
    ENTRY (JERRY_ITERATOR_TYPE_NONE, jerry_create_string ((const jerry_char_t *) "foo")),
    ENTRY (JERRY_ITERATOR_TYPE_NONE, jerry_create_error (JERRY_ERROR_TYPE, (const jerry_char_t *) "error")),

    ENTRY (JERRY_ITERATOR_TYPE_NONE, jerry_create_object ()),
    ENTRY (JERRY_ITERATOR_TYPE_NONE, jerry_create_array (10)),

    ENTRY_IF (JERRY_ITERATOR_TYPE_ARRAY, EVALUATE (array_iterator_keys), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_ITERATOR_TYPE_ARRAY, EVALUATE (array_iterator_values), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_ITERATOR_TYPE_ARRAY, EVALUATE (array_iterator_entries), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_ITERATOR_TYPE_ARRAY, EVALUATE (array_iterator_symbol_iterator), JERRY_FEATURE_SYMBOL),

    ENTRY_IF (JERRY_ITERATOR_TYPE_ARRAY, EVALUATE (typedarray_iterator_keys), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_ITERATOR_TYPE_ARRAY, EVALUATE (typedarray_iterator_values), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_ITERATOR_TYPE_ARRAY, EVALUATE (typedarray_iterator_entries), JERRY_FEATURE_SYMBOL),
    ENTRY_IF (JERRY_ITERATOR_TYPE_ARRAY, EVALUATE (typedarray_iterator_symbol_iterator), JERRY_FEATURE_SYMBOL),

    ENTRY_IF (JERRY_ITERATOR_TYPE_STRING, EVALUATE (string_symbol_iterator), JERRY_FEATURE_SYMBOL),

    ENTRY_IF (JERRY_ITERATOR_TYPE_MAP, EVALUATE (map_iterator_keys), JERRY_FEATURE_MAP),
    ENTRY_IF (JERRY_ITERATOR_TYPE_MAP, EVALUATE (map_iterator_values), JERRY_FEATURE_MAP),
    ENTRY_IF (JERRY_ITERATOR_TYPE_MAP, EVALUATE (map_iterator_entries), JERRY_FEATURE_MAP),
    ENTRY_IF (JERRY_ITERATOR_TYPE_MAP, EVALUATE (map_iterator_symbol_iterator), JERRY_FEATURE_MAP),

    ENTRY_IF (JERRY_ITERATOR_TYPE_SET, EVALUATE (set_iterator_keys), JERRY_FEATURE_SET),
    ENTRY_IF (JERRY_ITERATOR_TYPE_SET, EVALUATE (set_iterator_values), JERRY_FEATURE_SET),
    ENTRY_IF (JERRY_ITERATOR_TYPE_SET, EVALUATE (set_iterator_entries), JERRY_FEATURE_SET),
    ENTRY_IF (JERRY_ITERATOR_TYPE_SET, EVALUATE (set_iterator_symbol_iterator), JERRY_FEATURE_SET),
  };

  for (size_t idx = 0; idx < sizeof (entries) / sizeof (entries[0]); idx++)
  {
    jerry_iterator_type_t type_info = jerry_iterator_get_type (entries[idx].value);
    TEST_ASSERT (!entries[idx].active || type_info == entries[idx].type_info);
    jerry_release_value (entries[idx].value);
  }

  jerry_cleanup ();

  return 0;
} /* main */

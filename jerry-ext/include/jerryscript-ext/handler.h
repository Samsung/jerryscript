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

#ifndef JERRYX_HANDLER_H
#define JERRYX_HANDLER_H

#include "jerryscript.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*
 * Handler registration helper
 */

jerry_value_t jerryx_handler_register_global (const jerry_char_t *name_p,
                                              jerry_external_handler_t handler_p);

jerry_value_t jerryx_handler_register_object (jerry_value_t object_value, const jerry_char_t *name_p,
                                              jerry_external_handler_t handler_p);

/*
 * Common external function handlers
 */

jerry_value_t jerryx_handler_assert_fatal (const jerry_value_t func_obj_val, const jerry_value_t this_p,
                                           const jerry_value_t args_p[], const jerry_length_t args_cnt);
jerry_value_t jerryx_handler_assert_throw (const jerry_value_t func_obj_val, const jerry_value_t this_p,
                                           const jerry_value_t args_p[], const jerry_length_t args_cnt);
jerry_value_t jerryx_handler_assert (const jerry_value_t func_obj_val, const jerry_value_t this_p,
                                     const jerry_value_t args_p[], const jerry_length_t args_cnt);
jerry_value_t jerryx_handler_gc (const jerry_value_t func_obj_val, const jerry_value_t this_p,
                                 const jerry_value_t args_p[], const jerry_length_t args_cnt);
jerry_value_t jerryx_handler_print (const jerry_value_t func_obj_val, const jerry_value_t this_p,
                                    const jerry_value_t args_p[], const jerry_length_t args_cnt);
jerry_value_t jerryx_handler_resource_name (const jerry_value_t func_obj_val, const jerry_value_t this_p,
                                            const jerry_value_t args_p[], const jerry_length_t args_cnt);

void jerryx_register_assert_object (void);

/**
 * Struct used by the `jerryx_set_functions` method to
 * register multiple methods for a given object.
 */
typedef struct
{
  const char *name; /**< name of the property to add */
  jerry_value_t value; /**< value of the property */
} jerryx_property_entry;

#define JERRYX_PROPERTY_NUMBER(NAME, NUMBER) (jerryx_property_entry) { NAME, jerry_create_number (NUMBER) }
#define JERRYX_PROPERTY_STRING(NAME, STR) \
  (jerryx_property_entry) { NAME, jerry_create_string_from_utf8 ((const jerry_char_t *) STR) }
#define JERRYX_PROPERTY_STRING_SZ(NAME, STR, SIZE) \
  (jerryx_property_entry) { NAME, jerry_create_string_sz_from_utf8 ((const jerry_char_t *) STR, SIZE) }
#define JERRYX_PROPERTY_BOOLEAN(NAME, VALUE) (jerryx_property_entry) { NAME, jerry_create_boolean (VALUE) }
#define JERRYX_PROPERTY_FUNCTION(NAME, FUNC) (jerryx_property_entry) { NAME, jerry_create_external_function (FUNC) }
#define JERRYX_PROPERTY_UNDEFINED(NAME) (jerryx_property_entry) { NAME, jerry_create_undefined() }
#define JERRYX_PROPERTY_LIST_END() (jerryx_property_entry) { NULL, 0 }

/**
 * Stores the result of property register operation.
 */
typedef struct
{
  jerry_value_t result; /**< result of property registraion (undefined or error object) */
  uint32_t registered; /**< number of successfully registered methods */
} jerryx_register_result;

jerryx_register_result
jerryx_set_properties (const jerry_value_t target_object,
                       const jerryx_property_entry entries[]);

void
jerryx_release_property_entry (const jerryx_property_entry entries[],
                               const jerryx_register_result register_result);

jerry_value_t
jerryx_set_property_str (const jerry_value_t target_object,
                         const char *name,
                         const jerry_value_t value);

jerry_value_t
jerryx_get_property_str (const jerry_value_t target_object,
                         const char *name);

bool
jerryx_has_property_str (const jerry_value_t target_object,
                         const char *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYX_HANDLER_H */

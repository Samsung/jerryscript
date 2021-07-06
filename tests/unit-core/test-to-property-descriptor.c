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
create_property_descriptor (const char *script_p) /**< source code */
{
  jerry_value_t result = jerry_eval ((const jerry_char_t *) script_p, strlen (script_p), 0);
  TEST_ASSERT (jerry_value_is_object (result));
  return result;
} /* create_property_descriptor */

static void
check_attribute (jerry_value_t attribute, /**< attribute to be checked */
                 jerry_value_t object, /**< original object */
                 const char *name_p) /**< name of the attribute */
{
  jerry_value_t prop_name = jerry_create_string_from_utf8 ((const jerry_char_t *) name_p);
  jerry_value_t value = jerry_get_property (object, prop_name);

  if (jerry_value_is_undefined (value))
  {
    TEST_ASSERT (jerry_value_is_null (attribute));
  }
  else
  {
    jerry_value_t result = jerry_binary_operation (JERRY_BIN_OP_STRICT_EQUAL, attribute, value);
    TEST_ASSERT (jerry_value_is_true (result));
    jerry_release_value (result);
  }

  jerry_release_value (value);
  jerry_release_value (prop_name);
} /* check_attribute */

static jerry_property_descriptor_t
to_property_descriptor (jerry_value_t object /**< object */)
{
  jerry_property_descriptor_t prop_desc = jerry_property_descriptor_create ();

  jerry_value_t result = jerry_to_property_descriptor (object, &prop_desc);
  TEST_ASSERT (jerry_value_is_boolean (result) && jerry_value_is_true (result));
  jerry_release_value (result);

  return prop_desc;
} /* to_property_descriptor */

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  /* Next test. */
  const char *source_p = "({ value:'X', writable:true, enumerable:true, configurable:true })";
  jerry_value_t object = create_property_descriptor (source_p);

  jerry_property_descriptor_t prop_desc = to_property_descriptor (object);

  check_attribute (prop_desc.value, object, "value");

  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_VALUE_DEFINED);
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_GET_DEFINED));
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_SET_DEFINED));
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_WRITABLE_DEFINED);
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_WRITABLE);
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_ENUMERABLE_DEFINED);
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_ENUMERABLE);
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_CONFIGURABLE_DEFINED);
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_CONFIGURABLE);

  jerry_release_value (object);
  jerry_property_descriptor_free (&prop_desc);

  /* Next test. */
  source_p = "({ writable:false, configurable:true })";
  object = create_property_descriptor (source_p);

  prop_desc = to_property_descriptor (object);

  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_VALUE_DEFINED));
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_GET_DEFINED));
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_SET_DEFINED));
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_WRITABLE_DEFINED);
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_WRITABLE));
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_ENUMERABLE_DEFINED));
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_CONFIGURABLE_DEFINED);
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_CONFIGURABLE);

  jerry_release_value (object);
  jerry_property_descriptor_free (&prop_desc);

  /* Next test. */
  /* Note: the 'set' property is defined, and it has a value of undefined.
   *       This is different from not having a 'set' property. */
  source_p = "({ get: function() {}, set:undefined, configurable:true })";
  object = create_property_descriptor (source_p);

  prop_desc = to_property_descriptor (object);

  check_attribute (prop_desc.getter, object, "get");
  check_attribute (prop_desc.setter, object, "set");

  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_VALUE_DEFINED));
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_WRITABLE_DEFINED));
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_GET_DEFINED);
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_SET_DEFINED);
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_ENUMERABLE_DEFINED));
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_CONFIGURABLE_DEFINED);
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_CONFIGURABLE);

  jerry_release_value (object);
  jerry_property_descriptor_free (&prop_desc);

  /* Next test. */
  source_p = "({ get: undefined, enumerable:false })";
  object = create_property_descriptor (source_p);

  prop_desc = to_property_descriptor (object);

  check_attribute (prop_desc.getter, object, "get");

  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_VALUE_DEFINED));
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_WRITABLE_DEFINED));
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_GET_DEFINED);
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_SET_DEFINED));
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_ENUMERABLE_DEFINED);
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_ENUMERABLE));
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_CONFIGURABLE_DEFINED));

  jerry_release_value (object);
  jerry_property_descriptor_free (&prop_desc);

  /* Next test. */
  source_p = "({ set: function(v) {}, enumerable:true, configurable:false })";
  object = create_property_descriptor (source_p);

  prop_desc = to_property_descriptor (object);

  check_attribute (prop_desc.setter, object, "set");

  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_VALUE_DEFINED));
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_WRITABLE_DEFINED));
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_GET_DEFINED));
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_SET_DEFINED);
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_ENUMERABLE_DEFINED);
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_ENUMERABLE);
  TEST_ASSERT (prop_desc.flags & JERRY_PROP_IS_CONFIGURABLE_DEFINED);
  TEST_ASSERT (!(prop_desc.flags & JERRY_PROP_IS_CONFIGURABLE));

  jerry_release_value (object);
  jerry_property_descriptor_free (&prop_desc);

  /* Next test. */
  source_p = "({ get: function(v) {}, writable:true })";
  object = create_property_descriptor (source_p);
  jerry_value_t result = jerry_to_property_descriptor (object, &prop_desc);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);
  jerry_release_value (object);

  /* Next test. */
  object = jerry_create_null ();
  result = jerry_to_property_descriptor (object, &prop_desc);
  TEST_ASSERT (jerry_value_is_error (result));
  jerry_release_value (result);
  jerry_release_value (object);

  jerry_cleanup ();
  return 0;
} /* main */

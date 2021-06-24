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
#include "jerryscript-ext/module.h"

#define MODULE_NAME my_custom_module

static void
jobject_set_property_jval (
    jerry_value_t jobj, const char *name,
    jerry_value_t value)
{
  jerry_value_t prop_name = jerry_create_string ((const jerry_char_t *) (name));
  jerry_value_t ret_val = jerry_set_property (jobj, prop_name, value);
  jerry_release_value (prop_name);
  jerry_release_value (ret_val);
} /* jobject_set_property_jval */

static jerry_value_t
call_function_with_callback (
    const jerry_value_t jfunc,
    const jerry_value_t jthis,
    const jerry_value_t jargv[],
    const jerry_length_t jargc)
{
  (void) jfunc;
  (void) jargc;
  jerry_value_t jval_func = jargv[0];
  return jerry_call_function (jval_func, jthis, NULL, 0);
} /* call_function_with_callback */

static jerry_value_t
my_custom_module_on_resolve (void)
{
  jerry_value_t mymodule = jerry_create_object ();
  jerry_value_t val = jerry_create_number (42);
  jobject_set_property_jval (mymodule, "number_value", val);
  jerry_release_value (val);

  jerry_value_t jfunc = jerry_create_external_function (call_function_with_callback);
  jobject_set_property_jval (mymodule, "call_function_with_callback", jfunc);
  jerry_release_value (jfunc);

  return mymodule;
} /* my_custom_module_on_resolve */

JERRYX_NATIVE_MODULE (MODULE_NAME, my_custom_module_on_resolve)

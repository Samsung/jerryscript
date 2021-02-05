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

static void
native_cb2 (void)
{
  jerry_value_t array = jerry_create_array (100);
  jerry_release_value (array);
} /* native_cb2 */

static const jerry_object_native_info_t native_info2 = { .free_cb = (jerry_object_native_free_callback_t) native_cb2 };

static void
native_cb (void)
{
  jerry_value_t array = jerry_create_array (100);

  jerry_set_object_native_pointer (array, NULL, &native_info2);

  jerry_release_value (array);
} /* native_cb */

static const jerry_object_native_info_t native_info = { .free_cb = (jerry_object_native_free_callback_t) native_cb };

static void *
context_alloc_fn (size_t size, void *cb_data)
{
  (void) cb_data;
  return malloc (size);
} /* context_alloc_fn */

int
main (void)
{
  jerry_context_t *ctx_p = jerry_create_context (1024, context_alloc_fn, NULL);
  jerry_port_default_set_current_context (ctx_p);
  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t obj = jerry_create_object ();

  jerry_set_object_native_pointer (obj, NULL, &native_info);
  jerry_release_value (obj);

  jerry_cleanup ();
  free (ctx_p);
  return 0;
} /* main */

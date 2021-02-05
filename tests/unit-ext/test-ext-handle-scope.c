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

/**
 * Unit test for jerry-ext/handle-scope.
 */

#include "jerryscript-ext/handle-scope.h"
#include "jerryscript.h"
#include "test-common.h"

static int native_free_cb_call_count;

static void
native_free_cb (void *native_p)
{
  ++native_free_cb_call_count;
  (void) native_p;
} /* native_free_cb */

static const jerry_object_native_info_t native_info = {
  .free_cb = native_free_cb,
};

static jerry_value_t
create_object (void)
{
  jerry_value_t obj = jerry_create_object ();
  jerry_set_object_native_pointer (obj, NULL, &native_info);
  return obj;
} /* create_object */

static void
test_handle_scope_val (void)
{
  jerryx_handle_scope scope;
  jerryx_open_handle_scope (&scope);
  jerry_value_t obj = jerryx_create_handle (create_object ());
  (void) obj;
  jerryx_close_handle_scope (scope);
} /* test_handle_scope_val */

int
main (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  native_free_cb_call_count = 0;
  test_handle_scope_val ();
  jerry_gc (JERRY_GC_PRESSURE_LOW);
  TEST_ASSERT (native_free_cb_call_count == 1);

  jerry_cleanup ();
} /* main */

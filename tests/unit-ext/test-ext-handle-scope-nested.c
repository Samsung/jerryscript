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
 *
 * Tests escaping jerry value that passed from scopes which are created on heap.
 * Also reallocates scopes for one times to test if reallocation works.
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
create_object_nested (int times)
{
  jerryx_escapable_handle_scope scope;
  jerryx_open_escapable_handle_scope (&scope);

  jerry_value_t obj;
  if (times == 0)
  {
    obj = jerryx_create_handle (jerry_create_object ());
    jerry_set_object_native_pointer (obj, NULL, &native_info);
  }
  else
  {
    obj = create_object_nested (times - 1);
  }
  TEST_ASSERT (jerryx_handle_scope_get_current () == scope);

  // If leaves `escaped` uninitialized, there will be a style error on linux thrown by compiler
  jerry_value_t escaped = 0;
  jerryx_handle_scope_status status = jerryx_escape_handle (scope, obj, &escaped);
  TEST_ASSERT (status == jerryx_handle_scope_ok);
  TEST_ASSERT (scope->prelist_handle_count == 0);
  TEST_ASSERT (scope->handle_ptr == NULL);

  jerryx_close_handle_scope (scope);
  return escaped;
} /* create_object_nested */

static void
test_handle_scope_val (void)
{
  jerryx_handle_scope scope;
  jerryx_open_handle_scope (&scope);

  for (int idx = 0; idx < 2; ++idx)
  {
    jerry_value_t obj = create_object_nested (JERRYX_SCOPE_PRELIST_SIZE * 2);
    (void) obj;
  }

  TEST_ASSERT (jerryx_handle_scope_get_current () == scope);

  jerry_gc (JERRY_GC_PRESSURE_LOW);
  TEST_ASSERT (native_free_cb_call_count == 0);

  jerryx_close_handle_scope (scope);
} /* test_handle_scope_val */

int
main (void)
{
  jerry_init (JERRY_INIT_EMPTY);

  native_free_cb_call_count = 0;
  test_handle_scope_val ();

  jerry_gc (JERRY_GC_PRESSURE_LOW);
  TEST_ASSERT (native_free_cb_call_count == 2);

  jerry_cleanup ();
} /* main */

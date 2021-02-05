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

static bool test_context_data1_new_called = false;
static bool test_context_data2_new_called = false;
static bool test_context_data3_new_called = false;
static bool test_context_data4_new_called = false;
static bool test_context_data1_free_called = false;
static bool test_context_data2_free_called = false;
static bool test_context_data4_free_called = false;
static bool test_context_data1_finalize_called = false;
static bool test_context_data4_finalize_called = false;

/* Context item 1 */
const char *string1 = "item1";

static void
test_context_data1_new (void *user_data_p)
{
  test_context_data1_new_called = true;
  *((const char **) user_data_p) = string1;
} /* test_context_data1_new */

static void
test_context_data1_free (void *user_data_p)
{
  test_context_data1_free_called = true;
  TEST_ASSERT ((*(const char **) user_data_p) == string1);
  TEST_ASSERT (!test_context_data1_finalize_called);
} /* test_context_data1_free */

static void
test_context_data1_finalize (void *user_data_p)
{
  TEST_ASSERT (test_context_data1_free_called);
  TEST_ASSERT (!test_context_data1_finalize_called);
  TEST_ASSERT ((*(const char **) user_data_p) == string1);
  test_context_data1_finalize_called = true;
} /* test_context_data1_finalize */

static const jerry_context_data_manager_t manager1 = { .init_cb = test_context_data1_new,
                                                       .deinit_cb = test_context_data1_free,
                                                       .finalize_cb = test_context_data1_finalize,
                                                       .bytes_needed = sizeof (const char *) };

/* Context item 2 */
const char *string2 = "item2";

static void
test_context_data2_new (void *user_data_p)
{
  test_context_data2_new_called = true;
  *((const char **) user_data_p) = string2;
} /* test_context_data2_new */

static void
test_context_data2_free (void *user_data_p)
{
  test_context_data2_free_called = true;
  TEST_ASSERT ((*(const char **) user_data_p) == string2);
} /* test_context_data2_free */

static const jerry_context_data_manager_t manager2 = { .init_cb = test_context_data2_new,
                                                       .deinit_cb = test_context_data2_free,
                                                       .bytes_needed = sizeof (const char *) };

/* Context item 3 */

static void
test_context_data3_new (void *user_data_p)
{
  JERRY_UNUSED (user_data_p);
  test_context_data3_new_called = true;
} /* test_context_data3_new */

static const jerry_context_data_manager_t manager3 = {
  .init_cb = test_context_data3_new,
  /* NULL is allowed: */
  .deinit_cb = NULL,
  .finalize_cb = NULL,
  .bytes_needed = 0,
};

/* Context item 4 */

static void
test_context_data4_new (void *user_data_p)
{
  test_context_data4_new_called = true;
  TEST_ASSERT (user_data_p == NULL);
} /* test_context_data4_new */

static void
test_context_data4_free (void *user_data_p)
{
  test_context_data4_free_called = true;
  TEST_ASSERT (user_data_p == NULL);
  TEST_ASSERT (!test_context_data4_finalize_called);
} /* test_context_data4_free */

static void
test_context_data4_finalize (void *user_data_p)
{
  TEST_ASSERT (!test_context_data4_finalize_called);
  test_context_data4_finalize_called = true;
  TEST_ASSERT (user_data_p == NULL);
} /* test_context_data4_finalize */

static const jerry_context_data_manager_t manager4 = { .init_cb = test_context_data4_new,
                                                       .deinit_cb = test_context_data4_free,
                                                       .finalize_cb = test_context_data4_finalize,
                                                       .bytes_needed = 0 };

int
main (void)
{
  TEST_INIT ();

  jerry_init (JERRY_INIT_EMPTY);

  TEST_ASSERT (!strcmp (*((const char **) jerry_get_context_data (&manager1)), "item1"));
  TEST_ASSERT (!strcmp (*((const char **) jerry_get_context_data (&manager2)), "item2"));
  TEST_ASSERT (jerry_get_context_data (&manager3) == NULL);
  TEST_ASSERT (jerry_get_context_data (&manager4) == NULL);

  TEST_ASSERT (test_context_data1_new_called);
  TEST_ASSERT (test_context_data2_new_called);
  TEST_ASSERT (test_context_data3_new_called);
  TEST_ASSERT (test_context_data4_new_called);

  TEST_ASSERT (!test_context_data1_free_called);
  TEST_ASSERT (!test_context_data2_free_called);
  TEST_ASSERT (!test_context_data4_free_called);

  jerry_cleanup ();

  TEST_ASSERT (test_context_data1_free_called);
  TEST_ASSERT (test_context_data2_free_called);
  TEST_ASSERT (test_context_data4_free_called);

  TEST_ASSERT (test_context_data1_finalize_called);
  TEST_ASSERT (test_context_data4_finalize_called);

  return 0;
} /* main */

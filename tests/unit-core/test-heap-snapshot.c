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

#define _XOPEN_SOURCE 700
#include "jerryscript.h"
#include "jerryscript-port.h"
#include "jerryscript-port-default.h"
#include "test-common.h"
#include <unistd.h>
#include <inttypes.h>

static const jerry_char_t test_source[] = TEST_STRING_LITERAL (
  "var user_object = function() {"
  "this.simple_attribute = true;"
  "this.complex_attribute = {key1: 'value1', key2: 'longer value2'};"
  "this.recycled_attribute = 'Date';" // Value is "Magic" string, not runtime literal.
  "};"
  "var instantiated_object = new user_object();"
);

static void
node_cb (jerry_heap_snapshot_node_id_t node,
         jerry_heap_snapshot_node_type_t type,
         size_t size,
         jerry_value_t representation,
         jerry_heap_snapshot_node_id_t representation_node,
         void *user_data_p)
{
  char repr_buf[128] = {0};
  if (!jerry_value_is_undefined (representation))
  {
    jerry_string_to_utf8_char_buffer (representation, (jerry_char_t *) repr_buf, sizeof (repr_buf));
  }

  char result[128] = {0};
  snprintf (result,
            sizeof (result) - 1,
            "NODE\t%" PRIdPTR "\t%d\t%zu\t%s\t%" PRIdPTR "\n",
            node, type, size, repr_buf, representation_node);
  ssize_t res = write ((int) (uintptr_t) user_data_p, result, strlen (result));
  if (res != (ssize_t) strlen (result))
  {
    fprintf (stderr, "Error writing heap snapshot\n");
    exit (1);
  }
} /* node_cb */

static void
edge_cb (jerry_heap_snapshot_node_id_t parent,
         jerry_heap_snapshot_node_id_t node,
         jerry_heap_snapshot_edge_type_t type,
         jerry_value_t name,
         jerry_heap_snapshot_node_id_t name_node,
         void *user_data_p)
{
  char name_buf[128] = {0};
  if (!jerry_value_is_undefined (name))
  {
    jerry_string_to_utf8_char_buffer (name, (jerry_char_t *) name_buf, sizeof (name_buf));
  }

  char result[128] = {0};
  snprintf (result,
            sizeof (result) - 1,
            "EDGE\t%" PRIdPTR "\t%" PRIdPTR "\t%d\t%s\t%" PRIdPTR "\n",
            parent, node, type, name_buf, name_node);
  ssize_t res = write ((int) (uintptr_t) user_data_p, result, strlen (result));
  if (res != (ssize_t) strlen (result))
  {
    fprintf (stderr, "Error writing heap snapshot\n");
    exit (1);
  }
} /* edge_cb */

int
main (void)
{
  // We write the heap snapshot to a temporary file for later inspection.
  char heap_snapshot_output_name[] = "test-heap-snapshot-heap.XXXXXX";
  int heap_snapshot_fd = mkstemp (heap_snapshot_output_name);
  TEST_ASSERT (heap_snapshot_fd);

  jerry_init (JERRY_INIT_EMPTY);

  jerry_value_t parsed_code_val = jerry_parse (NULL,
                                               0,
                                               test_source,
                                               sizeof (test_source) - 1,
                                               JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (!jerry_value_is_error (parsed_code_val));

  jerry_value_t res = jerry_run (parsed_code_val);
  TEST_ASSERT (!jerry_value_is_error (res));

  jerry_heap_snapshot_capture (node_cb, edge_cb, (void *) (uintptr_t) heap_snapshot_fd);

  jerry_release_value (res);
  jerry_release_value (parsed_code_val);


  jerry_cleanup ();

  // Hand off validation of heap snapshot to a python script.
  close (heap_snapshot_fd);
  const char *test_source_name = __FILE__;
  char verify_script_name[strlen (test_source_name)];
  sprintf (verify_script_name, "%.*s.py",
           (int) (strrchr (test_source_name, '.') - test_source_name), test_source_name);
  execlp ("/usr/bin/env", "/usr/bin/env", "python", verify_script_name, heap_snapshot_output_name, NULL);
  // Does not return!
} /* main */

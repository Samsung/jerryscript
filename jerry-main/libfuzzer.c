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
#include <stdlib.h>

int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
  srand (0);
  jerry_init (JERRY_INIT_EMPTY);

  if (jerry_is_valid_utf8_string ((jerry_char_t *) data, (jerry_size_t) size))
  {
    jerry_value_t parse_value = jerry_parse (NULL, 0, (jerry_char_t *) data, size, JERRY_PARSE_NO_OPTS);

    if (!jerry_value_is_error (parse_value))
    {
      jerry_value_t run_value = jerry_run (parse_value);
      jerry_release_value (run_value);

      jerry_value_t run_queue_value = jerry_run_all_enqueued_jobs ();
      jerry_release_value (run_queue_value);
    }

    jerry_release_value (parse_value);
  }

  jerry_cleanup ();

  return 0;
} /* LLVMFuzzerTestOneInput */

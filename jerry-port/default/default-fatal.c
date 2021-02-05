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
#include <stdlib.h>

/**
 * Default implementation of jerry_port_fatal. Calls 'abort' if exit code is
 * non-zero, 'exit' otherwise.
 */
void
jerry_port_fatal (jerry_fatal_code_t code) /**< cause of error */
{
  if (code != 0 && code != ERR_OUT_OF_MEMORY)
  {
    abort ();
  }

  exit ((int) code);
} /* jerry_port_fatal */

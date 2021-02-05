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

#include "jerryscript-ext/debugger.h"
#include "jext-common.h"

/**
 * Must be called after the connection has been initialized.
 */
void
jerryx_debugger_after_connect (bool success) /**< tells whether the connection
                                              *   has been successfully established */
{
#if defined(JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1)
  if (success)
  {
    jerry_debugger_transport_start ();
  }
  else
  {
    jerry_debugger_transport_close ();
  }
#else /* !(defined (JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1)) */
  JERRYX_UNUSED (success);
#endif /* defined (JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1) */
} /* jerryx_debugger_after_connect */

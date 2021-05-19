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
#include "MbedThread-js.h"

/**
 * thread_sleep_for aka 'delay'
 * 
 * Sleep for a specified time period in millisec
 * 
 * @param value time in ms
*/
DECLARE_FUNCTION(thread_sleep_for_handler)
{
  uint32_t value = static_cast<uint32_t>(jerry_get_number_value (args_p[0]));
  thread_sleep_for (value);
  return jerry_create_undefined ();
}

void register_MbedThread ()
{
  jerry_value_t global_obj = jerry_get_global_object ();
  register_handler (global_obj, thread_sleep_for_handler, "delay");
  jerry_release_value (global_obj);
}

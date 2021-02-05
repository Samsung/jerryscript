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

#ifndef DEBUGGER_SHA1_H
#define DEBUGGER_SHA1_H

#include "jerryscript-debugger-transport.h"

#if defined(JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1)

/* JerryScript debugger protocol is a simplified version of RFC-6455 (WebSockets). */

void jerryx_debugger_compute_sha1 (const uint8_t *input1,
                                   size_t input1_len,
                                   const uint8_t *input2,
                                   size_t input2_len,
                                   uint8_t output[20]);

#endif /* defined (JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1) */

#endif /* !DEBUGGER_SHA1_H */

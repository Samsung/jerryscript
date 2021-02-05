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

#ifndef JERRYX_DEBUGGER_H
#define JERRYX_DEBUGGER_H

#include "jerryscript-debugger-transport.h"
#include "jerryscript.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void jerryx_debugger_after_connect (bool success);

/*
 * Message transmission interfaces.
 */
bool jerryx_debugger_tcp_create (uint16_t port);
bool jerryx_debugger_serial_create (const char *config);

/*
 * Message encoding interfaces.
 */
bool jerryx_debugger_ws_create (void);
bool jerryx_debugger_rp_create (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYX_HANDLER_H */

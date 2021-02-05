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

#ifndef JERRYSCRIPT_PORT_DEFAULT_H
#define JERRYSCRIPT_PORT_DEFAULT_H

#include "jerryscript-port.h"
#include "jerryscript.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** \addtogroup jerry_port_default Default Jerry engine port API
 * These functions are only available if the default port of Jerry is used.
 * @{
 */

jerry_log_level_t jerry_port_default_get_log_level (void);
void jerry_port_default_set_log_level (jerry_log_level_t level);

void jerry_port_default_set_current_context (jerry_context_t *context_p);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYSCRIPT_PORT_DEFAULT_H */

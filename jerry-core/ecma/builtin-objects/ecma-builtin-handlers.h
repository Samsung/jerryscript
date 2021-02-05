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

#ifndef ECMA_BUILTIN_HANDLERS_H
#define ECMA_BUILTIN_HANDLERS_H

#include "ecma-globals.h"

#if JERRY_ESNEXT

#include "ecma-builtins.h"
#include "ecma-helpers.h"
#include "ecma-promise-object.h"
#include "ecma-proxy-object.h"

typedef enum
{
  ECMA_NATIVE_HANDLER_START = 0,
#define ECMA_NATIVE_HANDLER(id, handler, length) id,
#include "ecma-builtin-handlers.inc.h"
#undef ECMA_NATIVE_HANDLER
  ECMA_NATIVE_HANDLER__COUNT
} ecma_native_handler_id_t;

typedef enum
{
  ECMA_NATIVE_HANDLER_FLAGS_NONE = 0,
  ECMA_NATIVE_HANDLER_FLAGS_NAME_INITIALIZED = (1 << 0),
  ECMA_NATIVE_HANDLER_FLAGS_LENGTH_INITIALIZED = (1 << 1),
  ECMA_NATIVE_HANDLER_FLAGS_PROMISE_ALREADY_RESOLVED = (1 << 2),
} ecma_native_handler_flags_t;

ecma_native_handler_t ecma_builtin_handler_get (ecma_native_handler_id_t id);
uint8_t ecma_builtin_handler_get_length (ecma_native_handler_id_t id);

#endif /* JERRY_ESNEXT */

#endif /* !ECMA_BUILTIN_HANDLERS_H */

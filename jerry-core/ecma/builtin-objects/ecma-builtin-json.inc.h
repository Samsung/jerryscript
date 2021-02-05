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

/*
 * JSON built-in description
 */

#include "ecma-builtin-helpers-macro-defines.inc.h"

#if JERRY_BUILTIN_JSON

#if JERRY_ESNEXT
/* ECMA-262 v6, 24.3.3 */
STRING_VALUE (LIT_GLOBAL_SYMBOL_TO_STRING_TAG, LIT_MAGIC_STRING_JSON_U, ECMA_PROPERTY_FLAG_CONFIGURABLE)
#endif /* JERRY_ESNEXT */

/* Routine properties:
 *  (property name, C routine name, arguments number or NON_FIXED, value of the routine's length property) */
ROUTINE (LIT_MAGIC_STRING_PARSE, ecma_builtin_json_parse, 2, 2)
ROUTINE (LIT_MAGIC_STRING_STRINGIFY, ecma_builtin_json_stringify, 3, 3)

#endif /* JERRY_BUILTIN_JSON */

#include "ecma-builtin-helpers-macro-undefs.inc.h"

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
 * Date built-in description
 */

#include "ecma-builtin-helpers-macro-defines.inc.h"

#if JERRY_BUILTIN_DATE

/* ECMA-262 v5, 15.9.4.1 */
OBJECT_VALUE (LIT_MAGIC_STRING_PROTOTYPE, ECMA_BUILTIN_ID_DATE_PROTOTYPE, ECMA_PROPERTY_FIXED)

NUMBER_VALUE (LIT_MAGIC_STRING_LENGTH, 7, ECMA_PROPERTY_FLAG_DEFAULT_LENGTH)

ROUTINE (LIT_MAGIC_STRING_PARSE, ecma_builtin_date_parse, 1, 1)
ROUTINE (LIT_MAGIC_STRING_UTC_U, ecma_builtin_date_utc, NON_FIXED, 7)
ROUTINE (LIT_MAGIC_STRING_NOW, ecma_builtin_date_now, 0, 0)

#if JERRY_ESNEXT
STRING_VALUE (LIT_MAGIC_STRING_NAME, LIT_MAGIC_STRING_DATE_UL, ECMA_PROPERTY_FLAG_CONFIGURABLE)
#endif /* JERRY_ESNEXT */

#endif /* JERRY_BUILTIN_DATE */

#include "ecma-builtin-helpers-macro-undefs.inc.h"

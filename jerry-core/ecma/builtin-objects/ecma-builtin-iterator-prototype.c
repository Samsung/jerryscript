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

#include "ecma-builtin-helpers.h"
#include "ecma-builtins.h"
#include "ecma-iterator-object.h"

#if JERRY_ESNEXT

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-iterator-prototype.inc.h"
#define BUILTIN_UNDERSCORED_ID  iterator_prototype
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup %iteratorprototype% ECMA %IteratorPrototype% object built-in
 * @{
 */

/**
 * The %IteratorPrototype% object's '@@iterator' routine
 *
 * See also:
 *          ECMA-262 v6, 22.1.2.1
 *
 * Note:
 *     Returned value must be freed with ecma_free_value.
 *
 * @return the given this value
 */
static ecma_value_t
ecma_builtin_iterator_prototype_object_iterator (ecma_value_t this_val) /**< this argument */
{
  /* 1. */
  return ecma_copy_value (this_val);
} /* ecma_builtin_iterator_prototype_object_iterator */

/**
 * @}
 * @}
 * @}
 */

#endif /* JERRY_ESNEXT */

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

#include "ecma-bigint.h"
#include "ecma-exceptions.h"

#if JERRY_BUILTIN_BIGINT

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

/**
 * This object has a custom dispatch function.
 */
#define BUILTIN_INC_HEADER_NAME "ecma-builtin-bigint-prototype.inc.h"
#define BUILTIN_UNDERSCORED_ID  bigint_prototype
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup bigint ECMA BigInt object built-in
 * @{
 */

/**
 * The BigInt.prototype object's 'valueOf' routine
 *
 * See also:
 *          ECMA-262 v11, 20.2.3.4
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_bigint_prototype_object_value_of (ecma_value_t this_arg) /**< this argument */
{
  if (ecma_is_value_bigint (this_arg))
  {
    return ecma_copy_value (this_arg);
  }

  if (ecma_is_value_object (this_arg))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (this_arg);

    if (ecma_object_class_is (object_p, LIT_MAGIC_STRING_BIGINT_UL))
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      JERRY_ASSERT (ecma_is_value_bigint (ext_object_p->u.class_prop.u.value));

      return ecma_copy_value (ext_object_p->u.class_prop.u.value);
    }
  }

  return ecma_raise_type_error (ECMA_ERR_MSG ("BigInt value expected"));
} /* ecma_builtin_bigint_prototype_object_value_of */

/**
 * The BigInt.prototype object's 'toString' routine
 *
 * See also:
 *          ECMA-262 v11, 20.2.3.3
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_bigint_prototype_object_to_string (ecma_value_t this_arg, /**< this argument */
                                                const ecma_value_t *arguments_list_p, /**< arguments list */
                                                uint32_t arguments_list_len) /**< number of arguments */
{
  ecma_value_t bigint = ecma_builtin_bigint_prototype_object_value_of (this_arg);

  if (ECMA_IS_VALUE_ERROR (bigint))
  {
    return bigint;
  }

  uint32_t radix = 10;

  if (arguments_list_len > 0 && !ecma_is_value_undefined (arguments_list_p[0]))
  {
    ecma_number_t arg_num;

    if (ECMA_IS_VALUE_ERROR (ecma_op_to_integer (arguments_list_p[0], &arg_num)))
    {
      ecma_free_value (bigint);
      return ECMA_VALUE_ERROR;
    }

    if (arg_num < 2 || arg_num > 36)
    {
      ecma_free_value (bigint);
      return ecma_raise_range_error (ECMA_ERR_MSG ("Radix must be between 2 and 36"));
    }

    radix = (uint32_t) arg_num;
  }

  ecma_string_t *string_p = ecma_bigint_to_string (bigint, radix);
  ecma_free_value (bigint);

  if (string_p == NULL)
  {
    return ECMA_VALUE_ERROR;
  }

  return ecma_make_string_value (string_p);
} /* ecma_builtin_bigint_prototype_object_to_string */

/**
 * @}
 * @}
 * @}
 */

#endif /* JERRY_BUILTIN_BIGINT */

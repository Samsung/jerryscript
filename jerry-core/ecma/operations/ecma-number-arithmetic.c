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

#include "ecma-number-arithmetic.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup numberarithmetic ECMA number arithmetic operations
 * @{
 */

/**
 * ECMA-defined number remainder calculation.
 *
 * See also:
 *          ECMA-262 v5, 11.5.3
 *
 * @return number - calculated remainder.
 */
ecma_number_t
ecma_op_number_remainder (ecma_number_t left_num, /**< left operand */
                          ecma_number_t right_num) /**< right operand */
{
  if (ecma_number_is_nan (left_num) || ecma_number_is_nan (right_num) || ecma_number_is_infinity (left_num)
      || ecma_number_is_zero (right_num))
  {
    return ecma_number_make_nan ();
  }
  else if (ecma_number_is_infinity (right_num) || (ecma_number_is_zero (left_num) && !ecma_number_is_zero (right_num)))
  {
    return left_num;
  }

  return ecma_number_calc_remainder (left_num, right_num);
} /* ecma_op_number_remainder */

/**
 * @}
 * @}
 */

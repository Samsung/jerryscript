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

#include "ecma-alloc.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "ecma-string-object.h"
#include "jrt-libc-includes.h"
#include "jrt.h"
#include "lit-char-helpers.h"
#include <math.h>

#if JERRY_BUILTIN_NUMBER

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

/**
 * This object has a custom dispatch function.
 */
#define BUILTIN_CUSTOM_DISPATCH

/**
 * List of built-in routine identifiers.
 */
enum
{
  ECMA_NUMBER_PROTOTYPE_ROUTINE_START = 0,
  ECMA_NUMBER_PROTOTYPE_VALUE_OF,
  ECMA_NUMBER_PROTOTYPE_TO_STRING,
  ECMA_NUMBER_PROTOTYPE_TO_LOCALE_STRING,
  ECMA_NUMBER_PROTOTYPE_TO_FIXED,
  ECMA_NUMBER_PROTOTYPE_TO_EXPONENTIAL,
  ECMA_NUMBER_PROTOTYPE_TO_PRECISION,
};

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-number-prototype.inc.h"
#define BUILTIN_UNDERSCORED_ID  number_prototype
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup numberprototype ECMA Number.prototype object built-in
 * @{
 */

/**
 * Helper for rounding numbers
 *
 * @return rounded number
 */
static inline lit_utf8_size_t JERRY_ATTR_ALWAYS_INLINE
ecma_builtin_number_prototype_helper_round (lit_utf8_byte_t *digits_p, /**< [in,out] number as a string in decimal
                                                                        *   form */
                                            lit_utf8_size_t num_digits, /**< length of the string representation */
                                            int32_t round_num, /**< number of digits to keep */
                                            int32_t *exponent_p, /**< [in, out] decimal exponent */
                                            bool zero) /**< true if digits_p represents zero */
{
  if (round_num == 0 && *exponent_p == 0)
  {
    if (digits_p[0] >= 5)
    {
      digits_p[0] = '1';
    }
    else
    {
      digits_p[0] = '0';
    }

    return 1;
  }

  if (round_num < 1)
  {
    return 0;
  }

  if ((lit_utf8_size_t) round_num >= num_digits || zero)
  {
    return num_digits;
  }

  if (digits_p[round_num] >= '5')
  {
    digits_p[round_num] = '0';

    int i = 1;

    /* Handle carry number. */
    for (; i <= round_num; i++)
    {
      if (++digits_p[round_num - i] <= '9')
      {
        break;
      }
      digits_p[round_num - i] = '0';
    }

    /* Prepend highest digit */
    if (i > round_num)
    {
      memmove (digits_p + 1, digits_p, num_digits);
      digits_p[0] = '1';
      *exponent_p += 1;
    }
  }

  return (lit_utf8_size_t) round_num;
} /* ecma_builtin_number_prototype_helper_round */

/**
 * The Number.prototype object's 'toString' and 'toLocaleString' routines
 *
 * See also:
 *          ECMA-262 v5, 15.7.4.2
 *          ECMA-262 v5, 15.7.4.7
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_number_prototype_object_to_string (ecma_number_t this_arg_number, /**< this argument number */
                                                const ecma_value_t *arguments_list_p, /**< arguments list */
                                                uint32_t arguments_list_len) /**< number of arguments */
{
  static const lit_utf8_byte_t digit_chars[36] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
                                                   'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                                   'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z' };

  uint32_t radix = 10;
  if (arguments_list_len > 0 && !ecma_is_value_undefined (arguments_list_p[0]))
  {
    ecma_number_t arg_num;

    if (ECMA_IS_VALUE_ERROR (ecma_op_to_integer (arguments_list_p[0], &arg_num)))
    {
      return ECMA_VALUE_ERROR;
    }

    radix = ecma_number_to_uint32 (arg_num);

    if (radix < 2 || radix > 36)
    {
      return ecma_raise_range_error (ECMA_ERR_MSG ("Radix must be between 2 and 36"));
    }
  }

  if (ecma_number_is_nan (this_arg_number) || ecma_number_is_infinity (this_arg_number)
      || ecma_number_is_zero (this_arg_number) || radix == 10)
  {
    ecma_string_t *ret_str_p = ecma_new_ecma_string_from_number (this_arg_number);
    return ecma_make_string_value (ret_str_p);
  }

  int buff_size = 0;

  bool is_number_negative = false;
  if (ecma_number_is_negative (this_arg_number))
  {
    /* ecma_number_to_decimal can't handle negative numbers, so we get rid of the sign. */
    this_arg_number = -this_arg_number;
    is_number_negative = true;

    /* Add space for the sign in the result. */
    buff_size += 1;
  }

  /* Decompose the number. */
  lit_utf8_byte_t digits[ECMA_MAX_CHARS_IN_STRINGIFIED_NUMBER];
  int32_t exponent;
  lit_utf8_size_t digit_count = ecma_number_to_decimal (this_arg_number, digits, &exponent);

  /*
   * The 'exponent' given by 'ecma_number_to_decimal' specifies where the decimal point is located
   * compared to the first digit in 'digits'.
   * For example: 120 -> '12', exp: 3 and 0.012 -> '12', exp: -1
   * We convert it to be location of the decimal point compared to the last digit of 'digits':
   * 120 -> 12 * 10^1 and 0.012 -> 12 * 10^-3
   */
  exponent = exponent - (int32_t) digit_count;

  /* 'magnitude' will be the magnitude of the number in the specific radix. */
  int magnitude;
  int required_digits;
  if (exponent >= 0)
  {
    /*
     * If the exponent is non-negative that means we won't have a fractional part, and can calculate
     * exactly how many digits we will have. This could be done via a mathematic formula, but in rare
     * cases that can cause incorrect results due to precision issues, so we use a loop instead.
     */
    magnitude = 0;
    ecma_number_t counter = this_arg_number;
    while (counter >= radix)
    {
      counter /= radix;
      magnitude++;
    }

    /*
     * The magnitude will only tell us how many digits we have after the first one, so we add one extra.
     * In this case we won't be needing a radix point, so we don't need to worry about space for it.
     */
    required_digits = magnitude + 1;
  }
  else
  {
    /*
     * We can't know exactly how many digits we will need, since the number may be non-terminating in the
     * new radix, so we will have to estimate it. We do this by first calculating how many zeros we will
     * need in the specific radix before we hit a significant digit. This is calculated from the decimal
     * exponent, which we negate so that we get a positive number in the end.
     */
    magnitude = (int) floor ((log (10) / log (radix)) * -exponent);

    /*
     * We also need to add space for significant digits. The worst case is radix == 2, since this will
     * require the most digits. In this case, the upper limit to the number of significant digits we can have is
     * ECMA_NUMBER_FRACTION_WIDTH + 1. This should be sufficient for any number.
     */
    required_digits = magnitude + ECMA_NUMBER_FRACTION_WIDTH + 1;

    /*
     * We add an exta slot for the radix point. It is also likely that we will need extra space for a
     * leading zero before the radix point. It's better to add space for that here as well, even if we may not
     * need it, since later we won't be able to do so.
     */
    buff_size += 2;
  }

  /*
   * Here we normalize the number so that it is as close to 0 as possible, which will prevent us from losing
   * precision in case of extreme numbers when we later split the number into integer and fractional parts.
   * This has to be done in the specific radix, otherwise it messes up the result, so we use magnitude instead.
   */
  if (exponent > 0)
  {
    for (int i = 0; i < magnitude; i++)
    {
      this_arg_number /= radix;
    }
  }
  else if (exponent < 0)
  {
    for (int i = 0; i < magnitude; i++)
    {
      this_arg_number *= radix;
    }
  }

  /* Split the number into an integer and a fractional part, since we have to handle them separately. */
  uint64_t whole = (uint64_t) this_arg_number;
  ecma_number_t fraction = this_arg_number - (ecma_number_t) whole;

  bool should_round = false;
  if (!ecma_number_is_zero (fraction) && exponent >= 0)
  {
    /*
     * If the exponent is non-negative, and we get a non-zero fractional part, that means
     * the normalization might have introduced a small error, in which case we have to correct it by rounding.
     * We'll add one extra significant digit which we will later use to round.
     */
    required_digits += 1;
    should_round = true;
  }

  /* Get the total required buffer size and allocate the buffer. */
  buff_size += required_digits;
  ecma_value_t ret_value;
  JMEM_DEFINE_LOCAL_ARRAY (buff, buff_size, lit_utf8_byte_t);
  int buff_index = 0;

  /* Calculate digits for whole part. */
  while (whole > 0)
  {
    JERRY_ASSERT (buff_index < buff_size && buff_index < required_digits);
    buff[buff_index++] = (lit_utf8_byte_t) (whole % radix);
    whole /= radix;
  }

  /* The digits are backwards, we need to reverse them. */
  for (int i = 0; i < buff_index / 2; i++)
  {
    lit_utf8_byte_t swap = buff[i];
    buff[i] = buff[buff_index - i - 1];
    buff[buff_index - i - 1] = swap;
  }

  /*
   * Calculate where we have to put the radix point relative to the beginning of
   * the new digits. If the exponent is non-negative this will be right after the number.
   */
  int point = exponent >= 0 ? magnitude + 1 : buff_index - magnitude;

  if (point < 0)
  {
    /*
     * In this case the radix point will be before the first digit,
     * so we need to leave space for leading zeros.
     */
    JERRY_ASSERT (exponent < 0);
    required_digits += point;
  }

  JERRY_ASSERT (required_digits <= buff_size);

  /* Calculate digits for fractional part. */
  while (buff_index < required_digits)
  {
    fraction *= radix;
    lit_utf8_byte_t digit = (lit_utf8_byte_t) floor (fraction);

    buff[buff_index++] = digit;
    fraction -= (ecma_number_t) floor (fraction);
  }

  if (should_round)
  {
    /* Consume last digit for rounding. */
    buff_index--;
    if (buff[buff_index] > radix / 2)
    {
      /* We should be rounding up. */
      buff[buff_index - 1]++;

      /* Propagate carry forward in the digits. */
      for (int i = buff_index - 1; i > 0 && buff[i] >= radix; i--)
      {
        buff[i] = (lit_utf8_byte_t) (buff[i] - radix);
        buff[i - 1]++;
      }

      if (buff[0] >= radix)
      {
        /*
         * Carry propagated over the whole number, we need to add a new leading digit.
         * We can use the place of the original rounded digit, we just need to shift everything
         * right by one.
         */
        memmove (buff + 1, buff, (size_t) buff_index);
        buff_index++;
        buff[0] = 1;
      }
    }
  }

  /* Remove trailing zeros. */
  while (buff_index - 1 > point && buff[buff_index - 1] == 0)
  {
    buff_index--;
  }

  /* Add leading zeros in case place of radix point is negative. */
  if (point <= 0)
  {
    /* We will have 'point' amount of zeros after the radix point, and +1 before. */
    int zero_count = -point + 1;
    memmove (buff + zero_count, buff, (size_t) buff_index);
    buff_index += zero_count;

    for (int i = 0; i < zero_count; i++)
    {
      buff[i] = 0;
    }

    /* We now need to place the radix point after the first zero. */
    point = 1;
  }

  /* Convert digits to characters. */
  for (int i = 0; i < buff_index; i++)
  {
    buff[i] = digit_chars[buff[i]];
  }

  /* Place radix point to the required position. */
  if (point < buff_index)
  {
    memmove (buff + point + 1, buff + point, (size_t) (buff_index - point));
    buff[point] = '.';
    buff_index++;
  }

  /* Add negative sign if necessary. */
  if (is_number_negative)
  {
    memmove (buff + 1, buff, (size_t) buff_index);
    buff[0] = '-';
    buff_index++;
  }

  JERRY_ASSERT (buff_index <= buff_size);
  ecma_string_t *str_p = ecma_new_ecma_string_from_utf8 (buff, (lit_utf8_size_t) buff_index);
  ret_value = ecma_make_string_value (str_p);
  JMEM_FINALIZE_LOCAL_ARRAY (buff);

  return ret_value;
} /* ecma_builtin_number_prototype_object_to_string */

/**
 * The Number.prototype object's 'valueOf' routine
 *
 * See also:
 *          ECMA-262 v5, 15.7.4.4
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_number_prototype_object_value_of (ecma_value_t this_arg) /**< this argument */
{
  if (ecma_is_value_number (this_arg))
  {
    return this_arg;
  }
  else if (ecma_is_value_object (this_arg))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (this_arg);

    if (ecma_object_class_is (object_p, LIT_MAGIC_STRING_NUMBER_UL))
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      JERRY_ASSERT (ecma_is_value_number (ext_object_p->u.class_prop.u.value));

      return ext_object_p->u.class_prop.u.value;
    }
  }

  return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a number or a Number object"));
} /* ecma_builtin_number_prototype_object_value_of */

/**
 * Type of number routine
 */
typedef enum
{
  NUMBER_ROUTINE_TO_FIXED, /**< Number.prototype.toFixed: ECMA-262 v11, 20.1.3.3 */
  NUMBER_ROUTINE_TO_EXPONENTIAL, /**< Number.prototype.toExponential: ECMA-262 v11, 20.1.3.2 */
  NUMBER_ROUTINE_TO_PRECISION, /**< Number.prototype.toPrecision: ECMA-262 v11, 20.1.3.5 */
  NUMBER_ROUTINE__COUNT, /**< count of the modes */
} number_routine_mode_t;

/**
 * Helper method to convert a number based on the given routine.
 */
static ecma_value_t
ecma_builtin_number_prototype_object_to_number_convert (ecma_number_t this_num, /**< this argument number */
                                                        ecma_value_t arg, /**< routine's argument */
                                                        number_routine_mode_t mode) /**< number routine mode */
{
  if (ecma_is_value_undefined (arg) && mode == NUMBER_ROUTINE_TO_PRECISION)
  {
    return ecma_builtin_number_prototype_object_to_string (this_num, NULL, 0);
  }

  ecma_number_t arg_num;
  ecma_value_t to_integer = ecma_op_to_integer (arg, &arg_num);

  if (ECMA_IS_VALUE_ERROR (to_integer))
  {
    return to_integer;
  }

  /* Argument boundary check for toFixed method */
  if (mode == NUMBER_ROUTINE_TO_FIXED && (arg_num <= -1 || arg_num >= 101))
  {
    return ecma_raise_range_error (ECMA_ERR_MSG ("Fraction digits must be between 0 and 100"));
  }

  /* Handle NaN separately */
  if (ecma_number_is_nan (this_num))
  {
    return ecma_make_magic_string_value (LIT_MAGIC_STRING_NAN);
  }

  /* Get the parameters of the number */
  lit_utf8_byte_t digits[ECMA_MAX_CHARS_IN_STRINGIFIED_NUMBER];
  lit_utf8_size_t num_of_digits;
  int32_t exponent;
  int32_t arg_int = ecma_number_to_int32 (arg_num);
  bool is_zero = ecma_number_is_zero (this_num);
  bool is_negative = ecma_number_is_negative (this_num);

  ecma_stringbuilder_t builder = ecma_stringbuilder_create ();

  if (is_negative)
  {
    if (!is_zero)
    {
      ecma_stringbuilder_append_char (&builder, LIT_CHAR_MINUS);
    }

    this_num *= -1;
  }

  /* Handle zero separately */
  if (is_zero)
  {
    if (mode == NUMBER_ROUTINE_TO_PRECISION)
    {
      arg_int--;
    }

    ecma_stringbuilder_append_char (&builder, LIT_CHAR_0);

    if (arg_int > 0)
    {
      ecma_stringbuilder_append_char (&builder, LIT_CHAR_DOT);
    }

    for (int32_t i = 0; i < arg_int; i++)
    {
      ecma_stringbuilder_append_char (&builder, LIT_CHAR_0);
    }

    if (mode == NUMBER_ROUTINE_TO_EXPONENTIAL)
    {
      ecma_stringbuilder_append_raw (&builder, (const lit_utf8_byte_t *) "e+0", 3);
    }

    return ecma_make_string_value (ecma_stringbuilder_finalize (&builder));
  }

  /* Handle infinity separately */
  if (ecma_number_is_infinity (this_num))
  {
    ecma_stringbuilder_append_magic (&builder, LIT_MAGIC_STRING_INFINITY_UL);
    return ecma_make_string_value (ecma_stringbuilder_finalize (&builder));
  }

  /* Argument boundary check for toExponential and toPrecision methods */
  if (mode == NUMBER_ROUTINE_TO_EXPONENTIAL && (arg_num <= -1 || arg_num >= 101))
  {
    ecma_stringbuilder_destroy (&builder);
    return ecma_raise_range_error (ECMA_ERR_MSG ("Fraction digits must be between 0 and 100"));
  }
  else if (mode == NUMBER_ROUTINE_TO_PRECISION && (arg_num < 1 || arg_num > 100))
  {
    ecma_stringbuilder_destroy (&builder);
    return ecma_raise_range_error (ECMA_ERR_MSG ("Precision digits must be between 1 and 100"));
  }

  num_of_digits = ecma_number_to_decimal (this_num, digits, &exponent);

  /* Handle undefined argument */
  if (ecma_is_value_undefined (arg) && mode == NUMBER_ROUTINE_TO_EXPONENTIAL)
  {
    arg_int = (int32_t) num_of_digits - 1;
  }

  if (mode == NUMBER_ROUTINE_TO_FIXED && exponent > 21)
  {
    ecma_stringbuilder_destroy (&builder);

    if (is_negative)
    {
      this_num *= -1;
    }

    return ecma_builtin_number_prototype_object_to_string (this_num, NULL, 0);
  }

  int32_t digits_to_keep = arg_int;

  if (mode == NUMBER_ROUTINE_TO_FIXED)
  {
    digits_to_keep += exponent;
  }
  else if (mode == NUMBER_ROUTINE_TO_EXPONENTIAL)
  {
    digits_to_keep += 1;
  }

  num_of_digits = ecma_builtin_number_prototype_helper_round (digits, num_of_digits, digits_to_keep, &exponent, false);

  /* toExponent routine and toPrecision cases where the exponent > precision or exponent < -5 */
  if (mode == NUMBER_ROUTINE_TO_EXPONENTIAL
      || (mode == NUMBER_ROUTINE_TO_PRECISION && (exponent < -5 || exponent > arg_int)))
  {
    /* Append first digit */
    ecma_stringbuilder_append_byte (&builder, *digits);

    if (mode == NUMBER_ROUTINE_TO_PRECISION)
    {
      arg_int--;
    }

    if (arg_int > 0)
    {
      ecma_stringbuilder_append_char (&builder, LIT_CHAR_DOT);
    }

    /* Append significant fraction digits */
    ecma_stringbuilder_append_raw (&builder, digits + 1, num_of_digits - 1);

    /* Append leading zeros */
    for (int32_t i = (int32_t) (num_of_digits); i < arg_int + 1; i++)
    {
      ecma_stringbuilder_append_char (&builder, LIT_CHAR_0);
    }

    ecma_stringbuilder_append_char (&builder, LIT_CHAR_LOWERCASE_E);

    if (exponent <= 0)
    {
      exponent = (-exponent) + 1;
      ecma_stringbuilder_append_char (&builder, LIT_CHAR_MINUS);
    }
    else
    {
      exponent -= 1;
      ecma_stringbuilder_append_char (&builder, LIT_CHAR_PLUS);
    }

    /* Append exponent part */
    lit_utf8_size_t exp_size = ecma_uint32_to_utf8_string ((uint32_t) exponent, digits, 3);
    ecma_stringbuilder_append_raw (&builder, digits, exp_size);

    return ecma_make_string_value (ecma_stringbuilder_finalize (&builder));
  }

  /* toFixed routine and toPrecision cases where the exponent <= precision and exponent >= -5 */
  lit_utf8_size_t result_digits;

  if (mode == NUMBER_ROUTINE_TO_FIXED)
  {
    result_digits = ((exponent > 0) ? (lit_utf8_size_t) (exponent + arg_int) : (lit_utf8_size_t) (arg_int + 1));
  }
  else
  {
    result_digits = ((exponent <= 0) ? (lit_utf8_size_t) (1 - exponent + arg_int) : (lit_utf8_size_t) arg_int);
  }

  /* Number of digits we copied from digits array */
  lit_utf8_size_t copied_digits = 0;

  if (exponent == 0 && digits_to_keep == 0)
  {
    ecma_stringbuilder_append_char (&builder, *digits);
    return ecma_make_string_value (ecma_stringbuilder_finalize (&builder));
  }

  if (exponent <= 0)
  {
    ecma_stringbuilder_append_char (&builder, LIT_CHAR_0);
    result_digits--;

    if (result_digits > 0)
    {
      ecma_stringbuilder_append_char (&builder, LIT_CHAR_DOT);

      /* Append leading zeros to the fraction part */
      for (int32_t i = 0; i < -exponent && result_digits > 0; i++)
      {
        ecma_stringbuilder_append_char (&builder, LIT_CHAR_0);
        result_digits--;
      }
    }
  }
  else
  {
    /* Append significant digits of integer part */
    copied_digits = JERRY_MIN (JERRY_MIN (num_of_digits, result_digits), (lit_utf8_size_t) exponent);
    ecma_stringbuilder_append_raw (&builder, digits, copied_digits);

    result_digits -= copied_digits;
    num_of_digits -= copied_digits;
    exponent -= (int32_t) copied_digits;

    /* Append zeros before decimal point */
    while (exponent > 0 && result_digits > 0)
    {
      ecma_stringbuilder_append_char (&builder, LIT_CHAR_0);
      result_digits--;
      exponent--;
    }

    if (result_digits > 0)
    {
      ecma_stringbuilder_append_char (&builder, LIT_CHAR_DOT);
    }
  }

  if (result_digits > 0)
  {
    /* Append significant digits to the fraction part */
    lit_utf8_size_t to_copy = JERRY_MIN (num_of_digits, result_digits);
    ecma_stringbuilder_append_raw (&builder, digits + copied_digits, to_copy);
    result_digits -= to_copy;

    /* Append leading zeros */
    while (result_digits > 0)
    {
      ecma_stringbuilder_append_char (&builder, LIT_CHAR_0);
      result_digits--;
    }
  }

  return ecma_make_string_value (ecma_stringbuilder_finalize (&builder));
} /* ecma_builtin_number_prototype_object_to_number_convert */

/**
 * Dispatcher of the built-in's routines
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_builtin_number_prototype_dispatch_routine (uint8_t builtin_routine_id, /**< built-in wide routine identifier */
                                                ecma_value_t this_arg, /**< 'this' argument value */
                                                const ecma_value_t arguments_list_p[], /**< list of arguments
                                                                                        *   passed to routine */
                                                uint32_t arguments_number) /**< length of arguments' list */
{
  ecma_value_t this_value = ecma_builtin_number_prototype_object_value_of (this_arg);

  if (ECMA_IS_VALUE_ERROR (this_value))
  {
    return this_value;
  }

  if (builtin_routine_id == ECMA_NUMBER_PROTOTYPE_VALUE_OF)
  {
    return ecma_copy_value (this_value);
  }

  ecma_number_t this_arg_number = ecma_get_number_from_value (this_value);

  ecma_value_t routine_arg_1 = arguments_list_p[0];

  switch (builtin_routine_id)
  {
    case ECMA_NUMBER_PROTOTYPE_TO_STRING:
    {
      return ecma_builtin_number_prototype_object_to_string (this_arg_number, arguments_list_p, arguments_number);
    }
    case ECMA_NUMBER_PROTOTYPE_TO_LOCALE_STRING:
    {
      return ecma_builtin_number_prototype_object_to_string (this_arg_number, NULL, 0);
    }
    case ECMA_NUMBER_PROTOTYPE_TO_FIXED:
    case ECMA_NUMBER_PROTOTYPE_TO_EXPONENTIAL:
    case ECMA_NUMBER_PROTOTYPE_TO_PRECISION:
    {
      const int option = NUMBER_ROUTINE_TO_FIXED + (builtin_routine_id - ECMA_NUMBER_PROTOTYPE_TO_FIXED);
      return ecma_builtin_number_prototype_object_to_number_convert (this_arg_number,
                                                                     routine_arg_1,
                                                                     (number_routine_mode_t) option);
    }
    default:
    {
      JERRY_UNREACHABLE ();
    }
  }
} /* ecma_builtin_number_prototype_dispatch_routine */

/**
 * @}
 * @}
 * @}
 */

#endif /* JERRY_BUILTIN_NUMBER */

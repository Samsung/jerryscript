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
#include "ecma-builtin-helpers.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "jcontext.h"
#include "lit-char-helpers.h"
#include <math.h>

#if JERRY_BUILTIN_DATE

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-date.inc.h"
#define BUILTIN_UNDERSCORED_ID  date
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup date ECMA Date object built-in
 * @{
 */

/**
 * Helper function to try to parse a part of a date string
 *
 * @return NaN if cannot read from string, ToNumber() otherwise
 */
static ecma_number_t
ecma_date_parse_date_chars (const lit_utf8_byte_t **str_p, /**< pointer to the cesu8 string */
                            const lit_utf8_byte_t *str_end_p, /**< pointer to the end of the string */
                            uint32_t num_of_chars, /**< number of characters to read and convert */
                            uint32_t min, /**< minimum valid value */
                            uint32_t max) /**< maximum valid value */
{
  JERRY_ASSERT (num_of_chars > 0);
  const lit_utf8_byte_t *str_start_p = *str_p;

  while (num_of_chars--)
  {
    if (*str_p >= str_end_p || !lit_char_is_decimal_digit (lit_cesu8_read_next (str_p)))
    {
      return ecma_number_make_nan ();
    }
  }

  ecma_number_t parsed_number = ecma_utf8_string_to_number (str_start_p, (lit_utf8_size_t) (*str_p - str_start_p), 0);

  if (parsed_number < min || parsed_number > max)
  {
    return ecma_number_make_nan ();
  }

  return parsed_number;
} /* ecma_date_parse_date_chars */

/**
 * Helper function to try to parse a special chracter (+,-,T,Z,:,.) in a date string
 *
 * @return true if the first character is same as the expected, false otherwise
 */
static bool
ecma_date_parse_special_char (const lit_utf8_byte_t **str_p, /**< pointer to the cesu8 string */
                              const lit_utf8_byte_t *str_end_p, /**< pointer to the end of the string */
                              const lit_utf8_byte_t expected_char) /**< expected character */
{
  if ((*str_p < str_end_p) && (**str_p == expected_char))
  {
    (*str_p)++;
    return true;
  }

  return false;
} /* ecma_date_parse_special_char */

/**
 * Helper function to try to parse a 4-5-6 digit year with optional negative sign in a date string
 *
 * Date.prototype.toString() and Date.prototype.toUTCString() emits year
 * in this format and Date.parse() should parse this format too.
 *
 * @return the parsed year or NaN.
 */
static ecma_number_t
ecma_date_parse_year (const lit_utf8_byte_t **str_p, /**< pointer to the cesu8 string */
                      const lit_utf8_byte_t *str_end_p) /**< pointer to the end of the string */
{
  bool is_year_sign_negative = ecma_date_parse_special_char (str_p, str_end_p, '-');
  const lit_utf8_byte_t *str_start_p = *str_p;
  int32_t parsed_year = 0;

  while ((str_start_p - *str_p < 6) && (str_start_p < str_end_p) && lit_char_is_decimal_digit (*str_start_p))
  {
    parsed_year = 10 * parsed_year + *str_start_p - LIT_CHAR_0;
    str_start_p++;
  }

  if (str_start_p - *str_p >= 4)
  {
    *str_p = str_start_p;
    if (is_year_sign_negative)
    {
      return -parsed_year;
    }
    return parsed_year;
  }

  return ecma_number_make_nan ();
} /* ecma_date_parse_year */

/**
 * Helper function to try to parse a day name in a date string
 * Valid day names: Sun, Mon, Tue, Wed, Thu, Fri, Sat
 * See also:
 *          ECMA-262 v9, 20.3.4.41.2 Table 46
 *
 * @return true if the string starts with a valid day name, false otherwise
 */
static bool
ecma_date_parse_day_name (const lit_utf8_byte_t **str_p, /**< pointer to the cesu8 string */
                          const lit_utf8_byte_t *str_end_p) /**< pointer to the end of the string */
{
  if (*str_p + 3 < str_end_p)
  {
    for (uint32_t i = 0; i < 7; i++)
    {
      if (!memcmp (day_names_p[i], *str_p, 3))
      {
        (*str_p) += 3;
        return true;
      }
    }
  }
  return false;
} /* ecma_date_parse_day_name */

/**
 * Helper function to try to parse a month name in a date string
 * Valid month names: Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec
 * See also:
 *          ECMA-262 v9, 20.3.4.41.2 Table 47
 *
 * @return number of the month if the string starts with a valid month name, 0 otherwise
 */
static uint32_t
ecma_date_parse_month_name (const lit_utf8_byte_t **str_p, /**< pointer to the cesu8 string */
                            const lit_utf8_byte_t *str_end_p) /**< pointer to the end of the string */
{
  if (*str_p + 3 < str_end_p)
  {
    for (uint32_t i = 0; i < 12; i++)
    {
      if (!memcmp (month_names_p[i], *str_p, 3))
      {
        (*str_p) += 3;
        return (i + 1);
      }
    }
  }
  return 0;
} /* ecma_date_parse_month_name */

/**
 * Calculate MakeDate(MakeDay(yr, m, dt), MakeTime(h, min, s, milli)) for Date constructor and UTC
 *
 * See also:
 *          ECMA-262 v5, 15.9.3.1
 *          ECMA-262 v5, 15.9.4.3
 *
 * @return result of MakeDate(MakeDay(yr, m, dt), MakeTime(h, min, s, milli))
 */
static ecma_value_t
ecma_date_construct_helper (const ecma_value_t *args, /**< arguments passed to the Date constructor */
                            uint32_t args_len) /**< number of arguments */
{
  ecma_number_t date_nums[7] = {
    ECMA_NUMBER_ZERO, /* year */
    ECMA_NUMBER_ZERO, /* month */
    ECMA_NUMBER_ONE, /* date */
    ECMA_NUMBER_ZERO, /* hours */
    ECMA_NUMBER_ZERO, /* minutes */
    ECMA_NUMBER_ZERO, /* seconds */
    ECMA_NUMBER_ZERO /* miliseconds */
  };

  args_len = JERRY_MIN (args_len, sizeof (date_nums) / sizeof (date_nums[0]));

  /* 1-7. */
  for (uint32_t i = 0; i < args_len; i++)
  {
    ecma_value_t status = ecma_op_to_number (args[i], date_nums + i);

    if (ECMA_IS_VALUE_ERROR (status))
    {
      return status;
    }
  }

  ecma_number_t prim_value = ecma_number_make_nan ();

  if (!ecma_number_is_nan (date_nums[0]))
  {
    /* 8. */
    ecma_number_t y = ecma_number_trunc (date_nums[0]);

    if (y >= 0 && y <= 99)
    {
      date_nums[0] = 1900 + y;
    }
  }

  prim_value = ecma_date_make_date (ecma_date_make_day (date_nums[0], date_nums[1], date_nums[2]),
                                    ecma_date_make_time (date_nums[3], date_nums[4], date_nums[5], date_nums[6]));

  return ecma_make_number_value (prim_value);
} /* ecma_date_construct_helper */

/**
 * Helper function used by ecma_builtin_date_parse
 *
 * See also:
 *          ECMA-262 v5, 15.9.4.2  Date.parse (string)
 *          ECMA-262 v5, 15.9.1.15 Date Time String Format
 *
 * @return the parsed date as ecma_number_t or NaN otherwise
 */
static ecma_number_t
ecma_builtin_date_parse_ISO_string_format (const lit_utf8_byte_t *date_str_curr_p,
                                           const lit_utf8_byte_t *date_str_end_p)
{
  /* 1. read year */

  uint32_t year_digits = 4;

  bool is_year_sign_negative = ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, '-');
  if (is_year_sign_negative || ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, '+'))
  {
    year_digits = 6;
  }

  ecma_number_t year =
    ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, year_digits, 0, (year_digits == 4) ? 9999 : 999999);
  if (is_year_sign_negative)
  {
    year = -year;
  }

  if (!ecma_number_is_nan (year))
  {
    ecma_number_t month = ECMA_NUMBER_ONE;
    ecma_number_t day = ECMA_NUMBER_ONE;
    ecma_number_t time = ECMA_NUMBER_ZERO;

    /* 2. read month if any */
    if (ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, '-'))
    {
      month = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 1, 12);
    }

    /* 3. read day if any */
    if (ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, '-'))
    {
      day = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 1, 31);
    }

    bool is_utc = true;
    /* 4. read time if any */
    if (ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, 'T'))
    {
      ecma_number_t hours = ECMA_NUMBER_ZERO;
      ecma_number_t minutes = ECMA_NUMBER_ZERO;
      ecma_number_t seconds = ECMA_NUMBER_ZERO;
      ecma_number_t milliseconds = ECMA_NUMBER_ZERO;

      lit_utf8_size_t remaining_length =
        lit_utf8_string_length (date_str_curr_p, (lit_utf8_size_t) (date_str_end_p - date_str_curr_p));

      if (remaining_length >= 5)
      {
        /* 4.1 read hours and minutes */
        hours = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 24);

        if (ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ':'))
        {
          minutes = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 59);

          /* 4.2 read seconds if any */
          if (ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ':'))
          {
            seconds = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 59);

            /* 4.3 read milliseconds if any */
            if (ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, '.'))
            {
              milliseconds = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 3, 0, 999);
            }
          }
        }
        else
        {
          minutes = ecma_number_make_nan ();
        }

        if (hours == 24 && (minutes != 0 || seconds != 0 || milliseconds != 0))
        {
          hours = ecma_number_make_nan ();
        }

        time = ecma_date_make_time (hours, minutes, seconds, milliseconds);
      }
      else
      {
        time = ecma_number_make_nan ();
      }

      /* 4.4 read timezone if any */
      if (ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, 'Z') && !ecma_number_is_nan (time))
      {
        time = ecma_date_make_time (hours, minutes, seconds, milliseconds);
      }
      else
      {
        bool is_timezone_sign_negative;
        if ((lit_utf8_string_length (date_str_curr_p, (lit_utf8_size_t) (date_str_end_p - date_str_curr_p)) == 6)
            && ((is_timezone_sign_negative = ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, '-'))
                || ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, '+')))
        {
          /* read hours and minutes */
          hours = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 24);

          if (hours == 24)
          {
            hours = ECMA_NUMBER_ZERO;
          }

          ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ':');
          minutes = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 59);
          ecma_number_t timezone_offset = ecma_date_make_time (hours, minutes, ECMA_NUMBER_ZERO, ECMA_NUMBER_ZERO);
          time += is_timezone_sign_negative ? timezone_offset : -timezone_offset;
        }
        else
        {
          is_utc = false;
        }
      }
    }

    if (date_str_curr_p >= date_str_end_p)
    {
      ecma_number_t date = ecma_date_make_day (year, month - 1, day);

      ecma_number_t result_date = ecma_date_make_date (date, time);
      if (!is_utc)
      {
        result_date = ecma_date_utc (result_date);
      }

      return result_date;
    }
  }
  return ecma_number_make_nan ();
} /* ecma_builtin_date_parse_ISO_string_format */

/**
 * Helper function used by ecma_builtin_date_parse
 *
 * See also:
 *          ECMA-262 v5, 15.9.4.2  Date.parse (string)
 *          ECMA-262 v9, 20.3.4.41 Date.prototype.toString ()
 *          ECMA-262 v9, 20.3.4.43 Date.prototype.toUTCString ()
 *
 * Used by: ecma_builtin_date_parse
 *
 * @return the parsed date as ecma_number_t or NaN otherwise
 */
static ecma_number_t
ecma_builtin_date_parse_toString_formats (const lit_utf8_byte_t *date_str_curr_p, const lit_utf8_byte_t *date_str_end_p)
{
  const ecma_number_t nan = ecma_number_make_nan ();

  if (!ecma_date_parse_day_name (&date_str_curr_p, date_str_end_p))
  {
    return nan;
  }

  const bool is_toUTCString_format = ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ',');

  if (!ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ' '))
  {
    return nan;
  }

  ecma_number_t month = 0;
  ecma_number_t day = 0;
  if (is_toUTCString_format)
  {
    day = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 31);
    if (ecma_number_is_nan (day))
    {
      return nan;
    }

    if (!ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ' '))
    {
      return nan;
    }

    month = ecma_date_parse_month_name (&date_str_curr_p, date_str_end_p);
    if (!(int) month)
    {
      return nan;
    }
  }
  else
  {
    month = ecma_date_parse_month_name (&date_str_curr_p, date_str_end_p);
    if (!(int) month)
    {
      return nan;
    }

    if (!ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ' '))
    {
      return nan;
    }

    day = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 31);
    if (ecma_number_is_nan (day))
    {
      return nan;
    }
  }

  if (!ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ' '))
  {
    return nan;
  }

  ecma_number_t year = ecma_date_parse_year (&date_str_curr_p, date_str_end_p);
  if (ecma_number_is_nan (year))
  {
    return nan;
  }

  if (!ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ' '))
  {
    return nan;
  }

  ecma_number_t hours = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 24);
  if (ecma_number_is_nan (hours))
  {
    return nan;
  }

  if (!ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ':'))
  {
    return nan;
  }

  ecma_number_t minutes = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 59);
  if (ecma_number_is_nan (minutes))
  {
    return nan;
  }

  if (!ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ':'))
  {
    return nan;
  }

  ecma_number_t seconds = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 59);
  if (ecma_number_is_nan (seconds))
  {
    return nan;
  }

  if (hours == 24 && (minutes != 0 || seconds != 0))
  {
    return nan;
  }

  if (!ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, ' '))
  {
    return nan;
  }

  if (!ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, 'G'))
  {
    return nan;
  }

  if (!ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, 'M'))
  {
    return nan;
  }

  if (!ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, 'T'))
  {
    return nan;
  }

  ecma_number_t time = ecma_date_make_time (hours, minutes, seconds, 0);

  if (!is_toUTCString_format)
  {
    bool is_timezone_sign_negative = ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, '-');
    if (!is_timezone_sign_negative && !ecma_date_parse_special_char (&date_str_curr_p, date_str_end_p, '+'))
    {
      return nan;
    }

    hours = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 24);
    if (ecma_number_is_nan (hours))
    {
      return nan;
    }
    if (hours == 24)
    {
      hours = ECMA_NUMBER_ZERO;
    }

    minutes = ecma_date_parse_date_chars (&date_str_curr_p, date_str_end_p, 2, 0, 59);
    if (ecma_number_is_nan (minutes))
    {
      return nan;
    }

    ecma_number_t timezone_offset = ecma_date_make_time (hours, minutes, ECMA_NUMBER_ZERO, ECMA_NUMBER_ZERO);
    time += is_timezone_sign_negative ? timezone_offset : -timezone_offset;
  }

  if (date_str_curr_p >= date_str_end_p)
  {
    ecma_number_t date = ecma_date_make_day (year, month - 1, day);
    return ecma_date_make_date (date, time);
  }

  return nan;
} /* ecma_builtin_date_parse_toString_formats */

/**
 * The Date object's 'parse' routine
 *
 * See also:
 *          ECMA-262 v5, 15.9.4.2  Date.parse (string)
 *          ECMA-262 v5, 15.9.1.15 Date Time String Format
 *          ECMA-262 v9, 20.3.4.41 Date.prototype.toString ()
 *          ECMA-262 v9, 20.3.4.43 Date.prototype.toUTCString ()
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_date_parse (ecma_value_t this_arg, /**< this argument */
                         ecma_value_t arg) /**< string */
{
  JERRY_UNUSED (this_arg);

  /* Date Time String fromat (ECMA-262 v5, 15.9.1.15) */
  ecma_string_t *date_str_p = ecma_op_to_string (arg);
  if (JERRY_UNLIKELY (date_str_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }

  ECMA_STRING_TO_UTF8_STRING (date_str_p, date_start_p, date_start_size);
  const lit_utf8_byte_t *date_str_curr_p = date_start_p;
  const lit_utf8_byte_t *date_str_end_p = date_start_p + date_start_size;

  // try to parse date string as ISO string - ECMA-262 v5, 15.9.1.15
  ecma_number_t ret_value = ecma_builtin_date_parse_ISO_string_format (date_str_curr_p, date_str_end_p);

  if (ecma_number_is_nan (ret_value))
  {
    // try to parse date string in Date.prototype.toString() or toUTCString() format
    ret_value = ecma_builtin_date_parse_toString_formats (date_str_curr_p, date_str_end_p);
  }

  ECMA_FINALIZE_UTF8_STRING (date_start_p, date_start_size);
  ecma_deref_ecma_string (date_str_p);
  return ecma_make_number_value (ecma_date_time_clip (ret_value));
} /* ecma_builtin_date_parse */

/**
 * The Date object's 'UTC' routine
 *
 * See also:
 *          ECMA-262 v5, 15.9.4.3
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_date_utc (ecma_value_t this_arg, /**< this argument */
                       const ecma_value_t args[], /**< arguments list */
                       uint32_t args_number) /**< number of arguments */
{
  JERRY_UNUSED (this_arg);

#if JERRY_ESNEXT
  const uint32_t required_args_number = 1;
#else /* !JERRY_ESNEXT */
  const uint32_t required_args_number = 2;
#endif /* JERRY_ESNEXT */

  if (args_number < required_args_number)
  {
    /* Note:
     *      When the UTC function is called with fewer than two arguments,
     *      the behaviour is implementation-dependent, so just return NaN.
     */
    return ecma_make_number_value (ecma_number_make_nan ());
  }

  ecma_value_t time_value = ecma_date_construct_helper (args, args_number);

  if (ECMA_IS_VALUE_ERROR (time_value))
  {
    return time_value;
  }

  ecma_number_t time = ecma_get_number_from_value (time_value);

  ecma_free_value (time_value);

  return ecma_make_number_value (ecma_date_time_clip (time));
} /* ecma_builtin_date_utc */

/**
 * Helper method to get the current time
 *
 * @return ecma_number_t
 */
static ecma_number_t
ecma_builtin_date_now_helper (void)
{
  return floor (DOUBLE_TO_ECMA_NUMBER_T (jerry_port_get_current_time ()));
} /* ecma_builtin_date_now_helper */

/**
 * The Date object's 'now' routine
 *
 * See also:
 *          ECMA-262 v5, 15.9.4.4
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_date_now (ecma_value_t this_arg) /**< this argument */
{
  JERRY_UNUSED (this_arg);
  return ecma_make_number_value (ecma_builtin_date_now_helper ());
} /* ecma_builtin_date_now */

/**
 * Handle calling [[Call]] of built-in Date object
 *
 * See also:
 *          ECMA-262 v5, 15.9.2.1
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_date_dispatch_call (const ecma_value_t *arguments_list_p, /**< arguments list */
                                 uint32_t arguments_list_len) /**< number of arguments */
{
  JERRY_UNUSED (arguments_list_p);
  JERRY_UNUSED (arguments_list_len);

  ecma_number_t now_val_num = ecma_builtin_date_now_helper ();

  return ecma_date_value_to_string (now_val_num);
} /* ecma_builtin_date_dispatch_call */

/**
 * Handle calling [[Construct]] of built-in Date object
 *
 * See also:
 *          ECMA-262 v5, 15.9.3.1
 *          ECMA-262 v11, 20.4.2
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_date_dispatch_construct (const ecma_value_t *arguments_list_p, /**< arguments list */
                                      uint32_t arguments_list_len) /**< number of arguments */
{
#if JERRY_ESNEXT
  JERRY_ASSERT (JERRY_CONTEXT (current_new_target_p));

  ecma_object_t *prototype_obj_p =
    ecma_op_get_prototype_from_constructor (JERRY_CONTEXT (current_new_target_p), ECMA_BUILTIN_ID_DATE_PROTOTYPE);
  if (JERRY_UNLIKELY (prototype_obj_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }
#else /* !JERRY_ESNEXT */
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_DATE_PROTOTYPE);
#endif /* (JERRY_ESNEXT */

  ecma_object_t *obj_p = ecma_create_object (prototype_obj_p, sizeof (ecma_extended_object_t), ECMA_OBJECT_TYPE_CLASS);

#if JERRY_ESNEXT
  ecma_deref_object (prototype_obj_p);
#endif /* JERRY_ESNEXT */

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) obj_p;
  ext_object_p->u.class_prop.class_id = LIT_MAGIC_STRING_UNDEFINED;

  ecma_number_t prim_value_num = ECMA_NUMBER_ZERO;

  /* 20.4.2.3 */
  if (arguments_list_len == 0)
  {
    prim_value_num = ecma_builtin_date_now_helper ();
  }
  /* 20.4.2.2 */
  else if (arguments_list_len == 1)
  {
    ecma_value_t argument = arguments_list_p[0];
    ecma_object_t *arg_obj = NULL;

    /* 4.a */
    if (ecma_is_value_object (argument))
    {
      arg_obj = ecma_get_object_from_value (argument);
    }

    if (arg_obj && ecma_object_class_is (arg_obj, LIT_MAGIC_STRING_DATE_UL))
    {
      ecma_extended_object_t *arg_ext_object_p = (ecma_extended_object_t *) arg_obj;
      prim_value_num = *ECMA_GET_INTERNAL_VALUE_POINTER (ecma_number_t, arg_ext_object_p->u.class_prop.u.value);
    }
    /* 4.b */
    else
    {
      ecma_value_t prim_comp_value = ecma_op_to_primitive (argument, ECMA_PREFERRED_TYPE_NO);

      if (ECMA_IS_VALUE_ERROR (prim_comp_value))
      {
        ecma_deref_object (obj_p);
        return prim_comp_value;
      }

      if (ecma_is_value_string (prim_comp_value))
      {
        ecma_value_t parse_res_value = ecma_builtin_date_parse (ecma_make_object_value (obj_p), prim_comp_value);

        if (ECMA_IS_VALUE_ERROR (parse_res_value))
        {
          ecma_deref_object (obj_p);
          ecma_free_value (prim_comp_value);
          return parse_res_value;
        }

        prim_value_num = ecma_get_number_from_value (parse_res_value);

        ecma_free_value (parse_res_value);
      }
      else
      {
        ecma_number_t arg;
        ecma_value_t prim_value = ecma_op_to_number (prim_comp_value, &arg);

        if (ECMA_IS_VALUE_ERROR (prim_value))
        {
          ecma_deref_object (obj_p);
          ecma_free_value (prim_comp_value);
          return prim_value;
        }

        prim_value_num = ecma_date_time_clip (arg);
      }

      ecma_free_value (prim_comp_value);
    }
  }
  /* 20.4.2.1 */
  else
  {
    ecma_value_t time_value = ecma_date_construct_helper (arguments_list_p, arguments_list_len);

    if (ECMA_IS_VALUE_ERROR (time_value))
    {
      ecma_deref_object (obj_p);
      return time_value;
    }

    ecma_number_t time = ecma_get_number_from_value (time_value);
    prim_value_num = ecma_date_time_clip (ecma_date_utc (time));

    ecma_free_value (time_value);
  }

  if (!ecma_number_is_nan (prim_value_num) && ecma_number_is_infinity (prim_value_num))
  {
    prim_value_num = ecma_number_make_nan ();
  }

  ext_object_p->u.class_prop.class_id = LIT_MAGIC_STRING_DATE_UL;

  ecma_number_t *date_num_p = ecma_alloc_number ();
  *date_num_p = prim_value_num;
  ECMA_SET_INTERNAL_VALUE_POINTER (ext_object_p->u.class_prop.u.value, date_num_p);

  return ecma_make_object_value (obj_p);
} /* ecma_builtin_date_dispatch_construct */

/**
 * @}
 * @}
 * @}
 */

#undef BREAK_IF_FALSE
#undef BREAK_IF_NAN

#endif /* JERRY_BUILTIN_DATE */

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
#include "ecma-exceptions.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "lit-char-helpers.h"
#include <math.h>

#if JERRY_BUILTIN_DATE

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltinhelpers ECMA builtin helper operations
 * @{
 */

const char day_names_p[7][3] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

const char month_names_p[12][3] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/**
 * Helper function to get day number from time value.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.2
 *
 * @return time value for day number
 */
extern inline ecma_number_t JERRY_ATTR_ALWAYS_INLINE
ecma_date_day (ecma_number_t time) /**< time value */
{
  JERRY_ASSERT (!ecma_number_is_nan (time));

  return (ecma_number_t) floor (time / ECMA_DATE_MS_PER_DAY);
} /* ecma_date_day */

/**
 * Helper function to get time within day from time value.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.2
 *
 * @return time value within the day
 */
extern inline ecma_number_t JERRY_ATTR_ALWAYS_INLINE
ecma_date_time_within_day (ecma_number_t time) /**< time value */
{
  JERRY_ASSERT (!ecma_number_is_nan (time));

  ecma_number_t modulo = fmod (time, ECMA_DATE_MS_PER_DAY);
  if (modulo < 0)
  {
    modulo += ECMA_DATE_MS_PER_DAY;
  }

  return modulo;
} /* ecma_date_time_within_day */

/**
 * Helper function to get the day number of the first day of a year.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.3
 *
 * @return day number of the first day of a year
 */
static ecma_number_t
ecma_date_day_from_year (ecma_number_t year) /**< year value */
{
  JERRY_ASSERT (!ecma_number_is_nan (year));

  return (ecma_number_t) (365 * (year - 1970) + floor ((year - 1969) / 4) - floor ((year - 1901) / 100)
                          + floor ((year - 1601) / 400));
} /* ecma_date_day_from_year */

/**
 * Helper function to get the time value of the start of a year.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.3
 *
 * @return  time value of the start of a year
 */
static inline ecma_number_t JERRY_ATTR_ALWAYS_INLINE
ecma_date_time_from_year (ecma_number_t year) /**< year value */
{
  JERRY_ASSERT (!ecma_number_is_nan (year));

  return ECMA_DATE_MS_PER_DAY * ecma_date_day_from_year (year);
} /* ecma_date_time_from_year */

/**
 * Helper function to determine a year value from the time value.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.3
 *
 * @return year value
 */
ecma_number_t
ecma_date_year_from_time (ecma_number_t time) /**< time value */
{
  JERRY_ASSERT (!ecma_number_is_nan (time));

  /* ECMA-262 v5, 15.9.1.1 define the largest year that is
   * representable (285616) forward from 01 January, 1970 UTC.
   */
  ecma_number_t year = (ecma_number_t) (1970 + 285616);
  ecma_number_t lower_year_boundary = (ecma_number_t) (1970 - 285616);

  if (ecma_date_time_from_year (year) < time || ecma_date_time_from_year (lower_year_boundary) > time)
  {
    return ecma_number_make_nan ();
  }

  while (ecma_date_time_from_year (year) > time)
  {
    ecma_number_t year_boundary = (ecma_number_t) floor (lower_year_boundary + (year - lower_year_boundary) / 2);
    if (ecma_date_time_from_year (year_boundary) > time)
    {
      year = year_boundary;
    }
    else
    {
      lower_year_boundary = year_boundary;
    }

    year--;
  }

  return year;
} /* ecma_date_year_from_time */

/**
 * Helper function to decide if time value is in a leap-year.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.3
 *
 * @return 1 if time within a leap year
 *         0 otherwise
 */
static int
ecma_date_in_leap_year (ecma_number_t year) /**< time value */
{
  int mod_400 = (int) fmod (floor (year), 400);

  JERRY_ASSERT (mod_400 >= -399 && mod_400 <= 399);

  if ((mod_400 % 4) != 0)
  {
    return 0;
  }

  if ((mod_400 % 100) != 0)
  {
    return 1;
  }

  if (mod_400 != 0)
  {
    return 0;
  }

  return 1;
} /* ecma_date_in_leap_year */

/**
 * End day for the first 11 months.
 */
static const int16_t ecma_date_month_end_day[10] = { 58, 89, 119, 150, 180, 211, 242, 272, 303, 333 };

/**
 * Helper function to get month from time value.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.4
 *
 * @return month number
 */
ecma_number_t
ecma_date_month_from_time (ecma_number_t time) /**< time value */
{
  JERRY_ASSERT (!ecma_number_is_nan (time));

  ecma_number_t year = ecma_date_year_from_time (time);

  if (ecma_number_is_nan (year))
  {
    return ecma_number_make_nan ();
  }

  int day_within_year = (int) (ecma_date_day (time) - ecma_date_day_from_year (year));

  JERRY_ASSERT (day_within_year >= 0);

  if (day_within_year <= 30)
  {
    return 0;
  }

  day_within_year -= ecma_date_in_leap_year (year);

  JERRY_ASSERT (day_within_year < 365);

  for (int i = 0; i < 10; i++)
  {
    if (day_within_year <= ecma_date_month_end_day[i])
    {
      return i + 1;
    }
  }

  return 11;
} /* ecma_date_month_from_time */

/**
 * Helper function to get date number from time value.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.5
 *
 * @return date number
 */
ecma_number_t
ecma_date_date_from_time (ecma_number_t time) /**< time value */
{
  JERRY_ASSERT (!ecma_number_is_nan (time));

  ecma_number_t year = ecma_date_year_from_time (time);

  if (ecma_number_is_nan (year))
  {
    return ecma_number_make_nan ();
  }

  int day_within_year = (int) (ecma_date_day (time) - ecma_date_day_from_year (year));

  JERRY_ASSERT (day_within_year >= 0);

  if (day_within_year <= 30)
  {
    return day_within_year + 1;
  }

  int leap_year = ecma_date_in_leap_year (year);

  if (day_within_year <= 58 + leap_year)
  {
    return day_within_year - 30;
  }

  day_within_year -= leap_year;

  JERRY_ASSERT (day_within_year < 365);

  for (int i = 1; i < 10; i++)
  {
    if (day_within_year <= ecma_date_month_end_day[i])
    {
      return day_within_year - ecma_date_month_end_day[i - 1];
    }
  }

  return day_within_year - 333;
} /* ecma_date_date_from_time */

/**
 * Helper function to get weekday from time value.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.6
 *
 * Used by:
 *         - The Date.prototype.getDay routine. (Generated.)
 *         - The Date.prototype.getUTCDay routine. (Generated.)
 *
 * @return  weekday number
 */
ecma_number_t
ecma_date_week_day (ecma_number_t time) /**< time value */
{
  JERRY_ASSERT (!ecma_number_is_nan (time));

  ecma_number_t week_day = (ecma_number_t) fmod ((ecma_date_day (time) + 4), 7);

  return (week_day < 0) ? (7 + week_day) : week_day;
} /* ecma_date_week_day */

/**
 * Helper function to get the local time zone offset at a given UTC timestamp.
 * You can add this number to the given UTC timestamp to get local time.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.9
 *
 * @return local time zone adjustment
 */
extern inline ecma_number_t JERRY_ATTR_ALWAYS_INLINE
ecma_date_local_time_zone_adjustment (ecma_number_t time) /**< time value */
{
  return jerry_port_get_local_time_zone_adjustment (time, true);
} /* ecma_date_local_time_zone_adjustment */

/**
 * Helper function to get UTC time from local time.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.9
 *
 * @return UTC time
 */
ecma_number_t
ecma_date_utc (ecma_number_t time) /**< time value */
{
  return time - jerry_port_get_local_time_zone_adjustment (time, false);
} /* ecma_date_utc */

/**
 * Helper function to get hour from time value.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.10
 *
 * @return hour value
 */
ecma_number_t
ecma_date_hour_from_time (ecma_number_t time) /**< time value */
{
  JERRY_ASSERT (!ecma_number_is_nan (time));

  ecma_number_t hour = (ecma_number_t) fmod (floor (time / ECMA_DATE_MS_PER_HOUR), ECMA_DATE_HOURS_PER_DAY);
  return (hour < 0) ? ECMA_DATE_HOURS_PER_DAY + hour : hour;
} /* ecma_date_hour_from_time */

/**
 * Helper function to get minute from time value.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.10
 *
 * @return minute value
 */
ecma_number_t
ecma_date_min_from_time (ecma_number_t time) /**< time value */
{
  JERRY_ASSERT (!ecma_number_is_nan (time));

  ecma_number_t min = (ecma_number_t) fmod (floor (time / ECMA_DATE_MS_PER_MINUTE), ECMA_DATE_MINUTES_PER_HOUR);
  return (min < 0) ? ECMA_DATE_MINUTES_PER_HOUR + min : min;
} /* ecma_date_min_from_time */

/**
 * Helper function to get second from time value.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.10
 *
 * @return second value
 */
ecma_number_t
ecma_date_sec_from_time (ecma_number_t time) /**< time value */
{
  JERRY_ASSERT (!ecma_number_is_nan (time));

  ecma_number_t sec = (ecma_number_t) fmod (floor (time / ECMA_DATE_MS_PER_SECOND), ECMA_DATE_SECONDS_PER_MINUTE);
  return (sec < 0) ? ECMA_DATE_SECONDS_PER_MINUTE + sec : sec;
} /* ecma_date_sec_from_time */

/**
 * Helper function to get millisecond from time value.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.10
 *
 * @return millisecond value
 */
ecma_number_t
ecma_date_ms_from_time (ecma_number_t time) /**< time value */
{
  JERRY_ASSERT (!ecma_number_is_nan (time));

  ecma_number_t milli = (ecma_number_t) fmod (time, ECMA_DATE_MS_PER_SECOND);
  return (milli < 0) ? ECMA_DATE_MS_PER_SECOND + milli : milli;
} /* ecma_date_ms_from_time */

/**
 * Helper function to make time value from hour, min, sec and ms.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.11
 *
 * @return time value
 */
ecma_number_t
ecma_date_make_time (ecma_number_t hour, /**< hour value */
                     ecma_number_t min, /**< minute value */
                     ecma_number_t sec, /**< second value */
                     ecma_number_t ms) /**< millisecond value */
{
  if (ecma_number_is_nan (hour) || ecma_number_is_nan (min) || ecma_number_is_nan (sec) || ecma_number_is_nan (ms)
      || ecma_number_is_infinity (hour) || ecma_number_is_infinity (min) || ecma_number_is_infinity (sec)
      || ecma_number_is_infinity (ms))
  {
    return ecma_number_make_nan ();
  }

  /* Replaced toInteger to ecma_number_trunc because it does the same thing. */
  ecma_number_t h = ecma_number_trunc (hour);
  ecma_number_t m = ecma_number_trunc (min);
  ecma_number_t s = ecma_number_trunc (sec);
  ecma_number_t milli = ecma_number_trunc (ms);

  return (h * ECMA_DATE_MS_PER_HOUR + m * ECMA_DATE_MS_PER_MINUTE + s * ECMA_DATE_MS_PER_SECOND + milli);
} /* ecma_date_make_time */

/**
 * Helper function to make day value from year, month and date.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.12
 *
 * @return day value
 */
ecma_number_t
ecma_date_make_day (ecma_number_t year, /**< year value */
                    ecma_number_t month, /**< month value */
                    ecma_number_t date) /**< date value */
{
  /* 1. */
  if (ecma_number_is_nan (year) || ecma_number_is_nan (month) || ecma_number_is_nan (date)
      || ecma_number_is_infinity (year) || ecma_number_is_infinity (month) || ecma_number_is_infinity (date))
  {
    return ecma_number_make_nan ();
  }

  /* 2., 3., 4. */
  ecma_number_t y = ecma_number_trunc (year);
  ecma_number_t m = ecma_number_trunc (month);
  ecma_number_t dt = ecma_number_trunc (date);
  /* 5. */
  ecma_number_t ym = y + (ecma_number_t) floor (m / 12);
  /* 6. */
  ecma_number_t mn = (ecma_number_t) fmod (m, 12);
  mn = (mn < 0) ? 12 + mn : mn;

  /* 7. */
  ecma_number_t time = ecma_date_time_from_year (ym);

  /**
   * The algorithm below searches the following date: ym-mn-1
   * To find this time it starts from the beginning of the year (ym)
   * then find the first day of the month.
   */
  if (!ecma_number_is_nan (time) && ecma_date_year_from_time (time) == ym)
  {
    /* Get the month */
    time += 31 * mn * ECMA_DATE_MS_PER_DAY;

    /* Get the month's first day */
    time += ((ecma_number_t) 1.0 - ecma_date_date_from_time (time)) * ECMA_DATE_MS_PER_DAY;

    if (!ecma_number_is_nan (time) && ecma_date_month_from_time (time) == mn && ecma_date_date_from_time (time) == 1)
    {
      /* 8. */
      return ecma_date_day (time) + dt - ((ecma_number_t) 1.0);
    }
  }

  return ecma_number_make_nan ();
} /* ecma_date_make_day */

/**
 * Helper function to make date value from day and time.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.13
 *
 * @return date value
 */
ecma_number_t
ecma_date_make_date (ecma_number_t day, /**< day value */
                     ecma_number_t time) /**< time value */
{
  if (ecma_number_is_nan (day) || ecma_number_is_nan (time))
  {
    return ecma_number_make_nan ();
  }

  ecma_number_t result = day * ECMA_DATE_MS_PER_DAY + time;

  if (ecma_number_is_infinity (result))
  {
    return ecma_number_make_nan ();
  }

  return result;
} /* ecma_date_make_date */

/**
 * Helper function to calculate number of milliseconds from time value.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.14
 *
 * @return number of milliseconds
 */
ecma_number_t
ecma_date_time_clip (ecma_number_t time) /**< time value */
{
  if (ecma_number_is_nan (time) || ecma_number_is_infinity (time) || fabs (time) > ECMA_DATE_MAX_VALUE)
  {
    return ecma_number_make_nan ();
  }

  return ecma_number_trunc (time);
} /* ecma_date_time_clip */

/**
 * Helper function to calculate timezone offset.
 *
 * See also:
 *          ECMA-262 v5, 15.9.5.26
 *
 * @return timezone offset
 */
extern inline ecma_number_t JERRY_ATTR_ALWAYS_INLINE
ecma_date_timezone_offset (ecma_number_t time) /**< time value */
{
  JERRY_ASSERT (!ecma_number_is_nan (time));

  return (-ecma_date_local_time_zone_adjustment (time)) / ECMA_DATE_MS_PER_MINUTE;
} /* ecma_date_timezone_offset */

/**
 * Common function to convert date to string.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_date_to_string_format (ecma_number_t datetime_number, /**< datetime */
                            const char *format_p) /**< format buffer */
{
  const uint32_t date_buffer_length = 37;
  JERRY_VLA (lit_utf8_byte_t, date_buffer, date_buffer_length);

  lit_utf8_byte_t *dest_p = date_buffer;

  while (*format_p != LIT_CHAR_NULL)
  {
    if (*format_p != LIT_CHAR_DOLLAR_SIGN)
    {
      *dest_p++ = (lit_utf8_byte_t) *format_p++;
      continue;
    }

    format_p++;

    const char *str_p = NULL;
    int32_t number = 0;
    int32_t number_length = 0;

    switch (*format_p)
    {
      case LIT_CHAR_UPPERCASE_Y: /* Year. */
      {
        number = (int32_t) ecma_date_year_from_time (datetime_number);

        if (number >= 100000 || number <= -100000)
        {
          number_length = 6;
        }
        else if (number >= 10000 || number <= -10000)
        {
          number_length = 5;
        }
        else
        {
          number_length = 4;
        }
        break;
      }
      case LIT_CHAR_LOWERCASE_Y: /* ISO Year: -000001, 0000, 0001, 9999, +012345 */
      {
        number = (int32_t) ecma_date_year_from_time (datetime_number);
        if (0 <= number && number <= 9999)
        {
          number_length = 4;
        }
        else
        {
          number_length = 6;
        }
        break;
      }
      case LIT_CHAR_UPPERCASE_M: /* Month. */
      {
        int32_t month = (int32_t) ecma_date_month_from_time (datetime_number);

        JERRY_ASSERT (month >= 0 && month <= 11);

        str_p = month_names_p[month];
        break;
      }
      case LIT_CHAR_UPPERCASE_O: /* Month as number. */
      {
        /* The 'ecma_date_month_from_time' (ECMA 262 v5, 15.9.1.4) returns a
         * number from 0 to 11, but we have to print the month from 1 to 12
         * for ISO 8601 standard (ECMA 262 v5, 15.9.1.15). */
        number = ((int32_t) ecma_date_month_from_time (datetime_number)) + 1;
        number_length = 2;
        break;
      }
      case LIT_CHAR_UPPERCASE_D: /* Day. */
      {
        number = (int32_t) ecma_date_date_from_time (datetime_number);
        number_length = 2;
        break;
      }
      case LIT_CHAR_UPPERCASE_W: /* Day of week. */
      {
        int32_t day = (int32_t) ecma_date_week_day (datetime_number);

        JERRY_ASSERT (day >= 0 && day <= 6);

        str_p = day_names_p[day];
        break;
      }
      case LIT_CHAR_LOWERCASE_H: /* Hour. */
      {
        number = (int32_t) ecma_date_hour_from_time (datetime_number);
        number_length = 2;
        break;
      }
      case LIT_CHAR_LOWERCASE_M: /* Minutes. */
      {
        number = (int32_t) ecma_date_min_from_time (datetime_number);
        number_length = 2;
        break;
      }
      case LIT_CHAR_LOWERCASE_S: /* Seconds. */
      {
        number = (int32_t) ecma_date_sec_from_time (datetime_number);
        number_length = 2;
        break;
      }
      case LIT_CHAR_LOWERCASE_I: /* Milliseconds. */
      {
        number = (int32_t) ecma_date_ms_from_time (datetime_number);
        number_length = 3;
        break;
      }
      case LIT_CHAR_LOWERCASE_Z: /* Time zone hours part. */
      {
        int32_t time_zone = (int32_t) ecma_date_local_time_zone_adjustment (datetime_number);

        if (time_zone >= 0)
        {
          *dest_p++ = LIT_CHAR_PLUS;
        }
        else
        {
          *dest_p++ = LIT_CHAR_MINUS;
          time_zone = -time_zone;
        }

        number = time_zone / (int32_t) ECMA_DATE_MS_PER_HOUR;
        number_length = 2;
        break;
      }
      default:
      {
        JERRY_ASSERT (*format_p == LIT_CHAR_UPPERCASE_Z); /* Time zone minutes part. */

        int32_t time_zone = (int32_t) ecma_date_local_time_zone_adjustment (datetime_number);

        if (time_zone < 0)
        {
          time_zone = -time_zone;
        }

        number = time_zone % (int32_t) ECMA_DATE_MS_PER_HOUR / (int32_t) ECMA_DATE_MS_PER_MINUTE;
        number_length = 2;
        break;
      }
    }

    format_p++;

    if (str_p != NULL)
    {
      /* Print string values: month or day name which is always 3 characters */
      memcpy (dest_p, str_p, 3);
      dest_p += 3;
      continue;
    }

    /* Print right aligned number values. */
    JERRY_ASSERT (number_length > 0);

    if (number < 0)
    {
      number = -number;
      *dest_p++ = '-';
    }
    else if (*(format_p - 1) == LIT_CHAR_LOWERCASE_Y && number_length == 6)
    {
      /* positive sign is compulsory for extended years */
      *dest_p++ = '+';
    }

    dest_p += number_length;
    lit_utf8_byte_t *buffer_p = dest_p;

    do
    {
      buffer_p--;
      *buffer_p = (lit_utf8_byte_t) ((number % 10) + (int32_t) LIT_CHAR_0);
      number /= 10;
    } while (--number_length);
  }

  JERRY_ASSERT (dest_p <= date_buffer + date_buffer_length);
  ecma_string_t *string_p = ecma_new_ecma_string_from_utf8 (date_buffer, (lit_utf8_size_t) (dest_p - date_buffer));

  return ecma_make_string_value (string_p);
} /* ecma_date_to_string_format */

/**
 * Common function to create a time zone specific string from a numeric value.
 *
 * Used by:
 *        - The Date routine.
 *        - The Date.prototype.toString routine.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_date_value_to_string (ecma_number_t datetime_number) /**< datetime */
{
  datetime_number += ecma_date_local_time_zone_adjustment (datetime_number);
  return ecma_date_to_string_format (datetime_number, "$W $M $D $Y $h:$m:$s GMT$z$Z");
} /* ecma_date_value_to_string */

/**
 * Common function to create a time zone specific string from a numeric value.
 *
 * Used by:
 *        - The Date.prototype.toUTCString routine.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_date_value_to_utc_string (ecma_number_t datetime_number) /**< datetime */
{
  return ecma_date_to_string_format (datetime_number, "$W, $D $M $Y $h:$m:$s GMT");
} /* ecma_date_value_to_utc_string */

/**
 * Common function to create a ISO specific string from a numeric value.
 *
 * Used by:
 *        - The Date.prototype.toISOString routine.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_date_value_to_iso_string (ecma_number_t datetime_number) /**<datetime */
{
  return ecma_date_to_string_format (datetime_number, "$y-$O-$DT$h:$m:$s.$iZ");
} /* ecma_date_value_to_iso_string */

/**
 * Common function to create a date string from a numeric value.
 *
 * Used by:
 *        - The Date.prototype.toDateString routine.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_date_value_to_date_string (ecma_number_t datetime_number) /**<datetime */
{
  return ecma_date_to_string_format (datetime_number, "$Y-$O-$D");
} /* ecma_date_value_to_date_string */

/**
 * Common function to create a time string from a numeric value.
 *
 * Used by:
 *        - The Date.prototype.toTimeString routine.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_date_value_to_time_string (ecma_number_t datetime_number) /**<datetime */
{
  return ecma_date_to_string_format (datetime_number, "$h:$m:$s.$i");
} /* ecma_date_value_to_time_string */

/**
 * @}
 * @}
 */

#endif /* JERRY_BUILTIN_DATE */

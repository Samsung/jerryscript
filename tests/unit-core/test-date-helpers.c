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
#include "ecma-globals.h"
#include "ecma-helpers.h"

#include "test-common.h"

#define MS_PER_DAY ((ecma_number_t) 86400000)
#define MS_PER_YEAR ((ecma_number_t) 365 * MS_PER_DAY)
#define START_OF_GREGORIAN_CALENDAR ((ecma_number_t) (-1970 * MS_PER_YEAR \
  - (1970 / 4) * MS_PER_DAY \
  + (1970 / 100) * MS_PER_DAY \
  - (1970 / 400) * MS_PER_DAY \
  - MS_PER_DAY))

/**
 * Unit test's main function.
 */
int
main (void)
{
  /* int ecma_date_year_from_time (time) */

  TEST_ASSERT (ecma_date_year_from_time (0) == 1970);
  TEST_ASSERT (ecma_date_year_from_time (0) == 1970);
  TEST_ASSERT (ecma_date_year_from_time (MS_PER_DAY) == 1970);
  TEST_ASSERT (ecma_date_year_from_time ((MS_PER_DAY) * (ecma_number_t) 365 - 1) == 1970);
  TEST_ASSERT (ecma_date_year_from_time (MS_PER_DAY * (ecma_number_t) 365) == 1971);
  TEST_ASSERT (ecma_date_year_from_time (MS_PER_DAY * (ecma_number_t) (365 * (2015 - 1970)))
                == 2014);
  TEST_ASSERT (ecma_date_year_from_time (MS_PER_DAY * (ecma_number_t) (365.25 * (2015 - 1970)))
                == 2015);
  TEST_ASSERT (ecma_date_year_from_time (-MS_PER_YEAR) == 1969);
  TEST_ASSERT (ecma_date_year_from_time (-1970 * MS_PER_YEAR) == 1);
  TEST_ASSERT (ecma_date_year_from_time (START_OF_GREGORIAN_CALENDAR) == 0);
  TEST_ASSERT (ecma_date_year_from_time (START_OF_GREGORIAN_CALENDAR - 1) == -1);
  TEST_ASSERT (ecma_date_year_from_time (START_OF_GREGORIAN_CALENDAR - 3 * MS_PER_YEAR) == -3);

  /* int ecma_date_month_from_time  (time) */

  TEST_ASSERT (ecma_date_month_from_time (START_OF_GREGORIAN_CALENDAR) == 0);
  TEST_ASSERT (ecma_date_month_from_time (0) == 0);
  TEST_ASSERT (ecma_date_month_from_time (-MS_PER_DAY) == 11);
  TEST_ASSERT (ecma_date_month_from_time (31 * MS_PER_DAY) == 1);

  /* int ecma_date_date_from_time  (time) */

  TEST_ASSERT (ecma_date_date_from_time (START_OF_GREGORIAN_CALENDAR) == 1);
  TEST_ASSERT (ecma_date_date_from_time (0) == 1);
  TEST_ASSERT (ecma_date_date_from_time (-MS_PER_DAY) == 31);
  TEST_ASSERT (ecma_date_date_from_time (31 * MS_PER_DAY) == 1);

  /* int ecma_date_week_day (ecma_number_t time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_utc (time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_hour_from_time (time) */

  TEST_ASSERT (ecma_date_hour_from_time (START_OF_GREGORIAN_CALENDAR) == 0);
  TEST_ASSERT (ecma_date_hour_from_time (0) == 0);
  TEST_ASSERT (ecma_date_hour_from_time (-MS_PER_DAY) == 0);
  TEST_ASSERT (ecma_date_hour_from_time (-1) == 23);

  /* ecma_number_t ecma_date_min_from_time (time) */

  TEST_ASSERT (ecma_date_min_from_time (START_OF_GREGORIAN_CALENDAR) == 0);
  TEST_ASSERT (ecma_date_min_from_time (0) == 0);
  TEST_ASSERT (ecma_date_min_from_time (-MS_PER_DAY) == 0);
  TEST_ASSERT (ecma_date_min_from_time (-1) == 59);

  /* ecma_number_t ecma_date_sec_from_time (time) */

  TEST_ASSERT (ecma_date_sec_from_time (START_OF_GREGORIAN_CALENDAR) == 0);
  TEST_ASSERT (ecma_date_sec_from_time (0) == 0);
  TEST_ASSERT (ecma_date_sec_from_time (-MS_PER_DAY) == 0);
  TEST_ASSERT (ecma_date_sec_from_time (-1) == 59);

  /* ecma_number_t ecma_date_ms_from_time (time) */

  TEST_ASSERT (ecma_date_ms_from_time (START_OF_GREGORIAN_CALENDAR) == 0);
  TEST_ASSERT (ecma_date_ms_from_time (0) == 0);
  TEST_ASSERT (ecma_date_ms_from_time (-MS_PER_DAY) == 0);
  TEST_ASSERT (ecma_date_ms_from_time (-1) == 999);

  /* ecma_number_t ecma_date_make_time (hour, min, sec, ms) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_make_day (year, month, date) */

  TEST_ASSERT (ecma_date_make_day (1970, 0, 1) == 0LL);
  TEST_ASSERT (ecma_date_make_day (1970, -1, 1) == -2678400000LL);
  TEST_ASSERT (ecma_date_make_day (1970, 0, 2.5) == 86400000LL);
  TEST_ASSERT (ecma_date_make_day (1970, 1, 35) == 5616000000LL);
  TEST_ASSERT (ecma_date_make_day (1970, 13, 35) == 37152000000LL);
  TEST_ASSERT (ecma_date_make_day (2016, 2, 1) == 1456790400000LL);
  TEST_ASSERT (ecma_date_make_day (2016, 8, 31) == 1475280000000LL);
  TEST_ASSERT (ecma_date_make_day (2016, 9, 1) == 1475280000000LL);

  /* ecma_number_t ecma_date_make_date (day, time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_time_clip (year) */

  /* FIXME: Implement */

  return 0;
} /* main */

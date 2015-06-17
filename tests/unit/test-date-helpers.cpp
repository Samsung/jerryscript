/* Copyright 2015 Samsung Electronics Co., Ltd.
 * Copyright 2015 University of Szeged.
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

#include "ecma-globals.h"
#include "ecma-helpers.h"

/**
 * Unit test's main function.
 */
int
main (int __attr_unused___ argc,
      char __attr_unused___ **argv)
{
  /* int ecma_date_day (time)*/

  JERRY_ASSERT (ecma_date_day (0) == 0);
  JERRY_ASSERT (ecma_date_day (86400000.000000000000000000001) == 1);

  /* ecma_number_t ecma_date_time_within_day (time) */

  JERRY_ASSERT (ecma_date_time_within_day (0) == 0);
  JERRY_ASSERT (ecma_date_time_within_day (42) == 42);
  JERRY_ASSERT (ecma_date_time_within_day (42.51) == 42.51);
  JERRY_ASSERT (ecma_date_time_within_day (86400000 + 42) == 42);

  /* int ecma_date_days_in_year (year) */

  JERRY_ASSERT (ecma_date_days_in_year (0) == 366);
  JERRY_ASSERT (ecma_date_days_in_year (1600) == 366);
  JERRY_ASSERT (ecma_date_days_in_year (1603) == 365);
  JERRY_ASSERT (ecma_date_days_in_year (1900) == 365);
  JERRY_ASSERT (ecma_date_days_in_year (1970) == 365);
  JERRY_ASSERT (ecma_date_days_in_year (2000) == 366);
  JERRY_ASSERT (ecma_date_days_in_year (2000.45) == 366);
  JERRY_ASSERT (ecma_date_days_in_year (2012) == 366);
  JERRY_ASSERT (ecma_date_days_in_year (2015) == 365);
  JERRY_ASSERT (ecma_date_days_in_year (285616 + 1970) == 365);
  JERRY_ASSERT (ecma_date_days_in_year (-1970) == 365);

  /* int ecma_date_day_from_year (year) */

  JERRY_ASSERT (ecma_date_day_from_year (1969) == -365);
  JERRY_ASSERT (ecma_date_day_from_year (1970) == 0);
  JERRY_ASSERT (ecma_date_day_from_year (1971) == 365);
  JERRY_ASSERT (ecma_date_day_from_year (2000) == 10957);

  /* ecma_number_t ecma_date_time_from_year (year) */

  /* FIXME: Implement */

  /* int ecma_date_year_from_time (time) */

  /* FIXME: Implement */

  /* int ecma_date_in_leap_year (time) */

  /* FIXME: Implement */

  /* int ecma_date_day_within_year (time) */

  /* FIXME: Implement */

  /* int ecma_date_month_from_time  (time) */

  /* FIXME: Implement */

  /* int ecma_date_date_from_time  (time) */

  /* FIXME: Implement */

  /* int ecma_date_week_day (ecma_number_t time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_local_tza () */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_daylight_saving_ta (time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_local_time (time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_utc (time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_hour_from_time (time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_min_from_time (time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_sec_from_time (time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_ms_from_time (time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_make_time (hour, min, sec, ms) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_make_day (year, month, date) */

  JERRY_ASSERT (ecma_date_make_day (1970, 0, 1) == 0);
  JERRY_ASSERT (ecma_date_make_day (1970, 0, 2.5) == 1);
  JERRY_ASSERT (ecma_date_make_day (1970, 1, 35) == 65);
  JERRY_ASSERT (ecma_date_make_day (1970, 13, 35) == 430);
  JERRY_ASSERT (ecma_date_make_day (2016, 2, 1) == 16861);
  /* FIXME: More testcase */

  /* ecma_number_t ecma_date_make_date (day, time) */

  /* FIXME: Implement */

  /* ecma_number_t ecma_date_time_clip (year) */

  /* FIXME: Implement */

  return 0;
} /* main */

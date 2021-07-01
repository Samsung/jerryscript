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

#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <winbase.h>
#include <winnt.h>
#endif /* _WIN32 */

#if defined (__GNUC__) || defined (__clang__)
#include <sys/time.h>
#endif /* __GNUC__ || __clang__ */

#include "jerryscript-port.h"
#include "jerryscript-port-default.h"

#ifdef _WIN32
static const LONGLONG UnixEpochInTicks = 116444736000000000LL; /* difference between 1970 and 1601 */
static const LONGLONG TicksPerMs = 10000LL; /* 1 tick is 100 nanoseconds */

/*
 * If you take the limit of SYSTEMTIME (last millisecond in 30827) then you end up with
 * a FILETIME of 0x7fff35f4f06c58f0 by using SystemTimeToFileTime(). However, if you put
 * 0x7fffffffffffffff into FileTimeToSystemTime() then you will end up in the year 30828,
 * although this date is invalid for SYSTEMTIME. Any larger value (0x8000000000000000 and above)
 * causes FileTimeToSystemTime() to fail.
 * https://docs.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-systemtime
 * https://docs.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime
 */
static const LONGLONG UnixEpochOfDate_1601_01_02 = -11644387200000LL; /* unit: ms */
static const LONGLONG UnixEpochOfDate_30827_12_29 = 9106702560000000LL; /* unit: ms */

/* https://support.microsoft.com/en-us/help/167296/how-to-convert-a-unix-time-t-to-a-win32-filetime-or-systemtime */
static void UnixTimeMsToFileTime (double t, LPFILETIME pft)
{
  LONGLONG ll = (LONGLONG) t * TicksPerMs + UnixEpochInTicks;
  pft->dwLowDateTime = (DWORD) ll;
  pft->dwHighDateTime = (DWORD) (ll >> 32);
} /* UnixTimeMsToFileTime */

static double FileTimeToUnixTimeMs (FILETIME ft)
{
  ULARGE_INTEGER date;
  date.HighPart = ft.dwHighDateTime;
  date.LowPart = ft.dwLowDateTime;
  return (double) (((LONGLONG) date.QuadPart - UnixEpochInTicks) / TicksPerMs);
} /* FileTimeToUnixTimeMs */

#endif /* _WIN32 */

/**
 * Default implementation of jerry_port_get_local_time_zone_adjustment.
 *
 * @return offset between UTC and local time at the given unix timestamp, if
 *         available. Otherwise, returns 0, assuming UTC time.
 */
double jerry_port_get_local_time_zone_adjustment (double unix_ms,  /**< ms since unix epoch */
                                                  bool is_utc)  /**< is the time above in UTC? */
{
#if defined (HAVE_TM_GMTOFF)
  struct tm tm;
  time_t now = (time_t) (unix_ms / 1000);
  localtime_r (&now, &tm);

  if (!is_utc)
  {
    now -= tm.tm_gmtoff;
    localtime_r (&now, &tm);
  }

  return ((double) tm.tm_gmtoff) * 1000;
#elif defined (_WIN32)
  FILETIME utcFileTime, localFileTime;
  SYSTEMTIME utcSystemTime, localSystemTime;
  bool timeConverted = false;

  /*
   * If the time is earlier than the date 1601-01-02, then always using date 1601-01-02 to
   * query time zone adjustment. This date (1601-01-02) will make sure both UTC and local
   * time succeed with Win32 API. The date 1601-01-01 may lead to a win32 api failure, as
   * after converting between local time and utc time, the time may be earlier than 1601-01-01
   * in UTC time, that exceeds the FILETIME representation range.
   */
  if (unix_ms < (double) UnixEpochOfDate_1601_01_02)
  {
    unix_ms = (double) UnixEpochOfDate_1601_01_02;
  }

  /* Like above, do not use the last supported day */
  if (unix_ms > (double) UnixEpochOfDate_30827_12_29)
  {
    unix_ms = (double) UnixEpochOfDate_30827_12_29;
  }

  if (is_utc)
  {
    UnixTimeMsToFileTime (unix_ms, &utcFileTime);
    if (FileTimeToSystemTime (&utcFileTime, &utcSystemTime)
        && SystemTimeToTzSpecificLocalTime (0, &utcSystemTime, &localSystemTime)
        && SystemTimeToFileTime (&localSystemTime, &localFileTime))
    {
      timeConverted = true;
    }
  }
  else
  {
    UnixTimeMsToFileTime (unix_ms, &localFileTime);
    if (FileTimeToSystemTime (&localFileTime, &localSystemTime)
        && TzSpecificLocalTimeToSystemTime (0, &localSystemTime, &utcSystemTime)
        && SystemTimeToFileTime (&utcSystemTime, &utcFileTime))
    {
      timeConverted = true;
    }
  }
  if (timeConverted)
  {
    ULARGE_INTEGER utcTime, localTime;
    utcTime.LowPart = utcFileTime.dwLowDateTime;
    utcTime.HighPart = utcFileTime.dwHighDateTime;
    localTime.LowPart = localFileTime.dwLowDateTime;
    localTime.HighPart = localFileTime.dwHighDateTime;
    return (double) (((LONGLONG) localTime.QuadPart - (LONGLONG) utcTime.QuadPart) / TicksPerMs);
  }
  return 0.0;
#elif defined (__GNUC__) || defined (__clang__)
  time_t now_time = (time_t) (unix_ms / 1000);
  double tza_s = 0.0;

  while (true)
  {
    struct tm now_tm;
    if (!gmtime_r (&now_time, &now_tm))
    {
      break;
    }
    now_tm.tm_isdst = -1; /* if not overridden, DST will not be taken into account */
    time_t local_time = mktime (&now_tm);
    if (local_time == (time_t) -1)
    {
      break;
    }
    tza_s = difftime (now_time, local_time);

    if (is_utc)
    {
      break;
    }
    now_time -= (time_t) tza_s;
    is_utc = true;
  }

  return tza_s * 1000;
#else /* !HAVE_TM_GMTOFF && !_WIN32 && !__GNUC__ && !__clang__ */
  (void) unix_ms; /* unused */
  (void) is_utc; /* unused */
  return 0.0;
#endif /* HAVE_TM_GMTOFF */
} /* jerry_port_get_local_time_zone_adjustment */

/**
 * Default implementation of jerry_port_get_current_time. Uses 'gettimeofday' if
 * available on the system, does nothing otherwise.
 *
 * @return milliseconds since Unix epoch - if 'gettimeofday' is available and
 *                                         executed successfully,
 *         0 - otherwise.
 */
double jerry_port_get_current_time (void)
{
#ifdef _WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime (&ft);
  return FileTimeToUnixTimeMs (ft);
#elif defined (__GNUC__) || defined (__clang__)
  struct timeval tv;

  if (gettimeofday (&tv, NULL) == 0)
  {
    return ((double) tv.tv_sec) * 1000.0 + ((double) tv.tv_usec) / 1000.0;
  }
  return 0.0;
#else /* !_WIN32 && !__GNUC__ && !__clang__ */
  return 0.0;
#endif /* _WIN32 */
} /* jerry_port_get_current_time */

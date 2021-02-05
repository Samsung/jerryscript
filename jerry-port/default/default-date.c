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
#include <time.h>
#include <winbase.h>
#include <windows.h>
#include <winnt.h>
#endif /* _WIN32 */

#if defined(__GNUC__) || defined(__clang__)
#include <sys/time.h>
#endif /* __GNUC__ || __clang__ */

#include "jerryscript-port-default.h"
#include "jerryscript-port.h"

#ifdef _WIN32
static const LONGLONG UnixEpochInTicks = 116444736000000000; /* difference between 1970 and 1601 */
static const LONGLONG TicksPerMs = 10000; /* 1 tick is 100 nanoseconds */

/* https://support.microsoft.com/en-us/help/167296/how-to-convert-a-unix-time-t-to-a-win32-filetime-or-systemtime */
static void
UnixTimeMsToFileTime (double t, LPFILETIME pft)
{
  LONGLONG ll = (LONGLONG) t * TicksPerMs + UnixEpochInTicks;
  pft->dwLowDateTime = (DWORD) ll;
  pft->dwHighDateTime = (DWORD) (ll >> 32);
} /* UnixTimeMsToFileTime */

static double
FileTimeToUnixTimeMs (FILETIME ft)
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
double
jerry_port_get_local_time_zone_adjustment (double unix_ms, /**< ms since unix epoch */
                                           bool is_utc) /**< is the time above in UTC? */
{
#ifdef _WIN32
  FILETIME fileTime, localFileTime;
  SYSTEMTIME systemTime, localSystemTime;
  ULARGE_INTEGER time, localTime;

  UnixTimeMsToFileTime (unix_ms, &fileTime);

  if (FileTimeToSystemTime (&fileTime, &systemTime)
      && SystemTimeToTzSpecificLocalTime (0, &systemTime, &localSystemTime)
      && SystemTimeToFileTime (&localSystemTime, &localFileTime))
  {
    time.LowPart = fileTime.dwLowDateTime;
    time.HighPart = fileTime.dwHighDateTime;
    localTime.LowPart = localFileTime.dwLowDateTime;
    localTime.HighPart = localFileTime.dwHighDateTime;
    return (double) (((LONGLONG) localTime.QuadPart - (LONGLONG) time.QuadPart) / TicksPerMs);
  }
#elif defined(__GNUC__) || defined(__clang__)
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
#else /* !_WIN32 && !__GNUC__ && !__clang__ */
  (void) unix_ms; /* unused */
  (void) is_utc; /* unused */
  return 0.0;
#endif /* _WIN32 */
} /* jerry_port_get_local_time_zone_adjustment */

/**
 * Default implementation of jerry_port_get_current_time. Uses 'gettimeofday' if
 * available on the system, does nothing otherwise.
 *
 * @return milliseconds since Unix epoch - if 'gettimeofday' is available and
 *                                         executed successfully,
 *         0 - otherwise.
 */
double
jerry_port_get_current_time (void)
{
#ifdef _WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime (&ft);
  return FileTimeToUnixTimeMs (ft);
#elif defined(__GNUC__) || defined(__clang__)
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

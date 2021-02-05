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

/*
 * This source contains libc overrides for the sake of stable benchmarking. If
 * building a binary for the purpose of benchmarking, the object compiled from
 * this source is to be injected into the list of objects-to-be-linked before
 * the list of libraries-to-be-linked to ensure that the linker picks up these
 * implementations.
 */

#ifdef __GNUC__
/*
 * Note:
 *      This is nasty and dangerous. However, we only need the timeval structure
 *      from sys/time.h. Unfortunately, the same header also declares
 *      gettimeofday, which has different declarations on different platforms
 *      (e.g., macOS, Linux). So, instead of #ifdef'ing for platforms, we simply
 *      tweak the header to declare another function. Don't try this at home.
 */
#define gettimeofday __prevent_conflicting_gettimeofday_declarations__
#include <sys/time.h>
#undef gettimeofday

int gettimeofday (struct timeval *, void *);

/**
 * Useless but stable gettimeofday implementation. Returns Epoch. Ensures that
 * test cases relying on "now" yield stable results.
 */
int
gettimeofday (struct timeval *tv, void *tz)
{
  (void) tz;
  tv->tv_sec = 0;
  tv->tv_usec = 0;
  return 0;
} /* gettimeofday */
#endif /* __GNUC__ */

int rand (void);

/**
 * Useless but stable rand implementation. Returns 4. Ensures that test cases
 * relying on randomness yield stable results.
 */
int
rand (void)
{
  return 4; /* Chosen by fair dice roll. Guaranteed to be random. */
} /* rand */

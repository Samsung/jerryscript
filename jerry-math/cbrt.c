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
 *
 * This file is based on work under the following copyright and permission
 * notice:
 *
 *     Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 *     Developed at SunSoft, a Sun Microsystems, Inc. business.
 *     Permission to use, copy, modify, and distribute this
 *     software is freely granted, provided that this notice
 *     is preserved.
 *
 *     @(#)s_cbrt.c 1.3 95/01/18
 */

#include "jerry-math-internal.h"

/* cbrt(x)
 * Return cube root of x
 */

#define B1 715094163 /* B1 = (682 - 0.03306235651) * 2**20 */
#define B2 696219795 /* B2 = (664 - 0.03306235651) * 2**20 */
#define C  5.42857142857142815906e-01 /* 19/35     = 0x3FE15F15, 0xF15F15F1 */
#define D  -7.05306122448979611050e-01 /* -864/1225 = 0xBFE691DE, 0x2532C834 */
#define E  1.41428571428571436819e+00 /* 99/70     = 0x3FF6A0EA, 0x0EA0EA0F */
#define F  1.60714285714285720630e+00 /* 45/28     = 0x3FF9B6DB, 0x6DB6DB6E */
#define G  3.57142857142857150787e-01 /* 5/14      = 0x3FD6DB6D, 0xB6DB6DB7 */

double
cbrt (double x)
{
  double r, s, w;
  double_accessor t, temp;
  unsigned int sign;
  t.dbl = 0.0;
  temp.dbl = x;

  sign = temp.as_int.hi & 0x80000000; /* sign = sign(x) */
  temp.as_int.hi ^= sign;

  if (temp.as_int.hi >= 0x7ff00000)
  {
    /* cbrt(NaN, INF) is itself */
    return (x + x);
  }
  if ((temp.as_int.hi | temp.as_int.lo) == 0)
  {
    /* cbrt(0) is itself */
    return (x);
  }
  /* rough cbrt to 5 bits */
  if (temp.as_int.hi < 0x00100000) /* subnormal number */
  {
    t.as_int.hi = 0x43500000; /* set t= 2**54 */
    t.dbl *= temp.dbl;
    t.as_int.hi = t.as_int.hi / 3 + B2;
  }
  else
  {
    t.as_int.hi = temp.as_int.hi / 3 + B1;
  }

  /* new cbrt to 23 bits, may be implemented in single precision */
  r = t.dbl * t.dbl / temp.dbl;
  s = C + r * t.dbl;
  t.dbl *= G + F / (s + E + D / s);

  /* chopped to 20 bits and make it larger than cbrt(x) */
  t.as_int.lo = 0;
  t.as_int.hi += 0x00000001;

  /* one step newton iteration to 53 bits with error less than 0.667 ulps */
  s = t.dbl * t.dbl; /* t*t is exact */
  r = temp.dbl / s;
  w = t.dbl + t.dbl;
  r = (r - t.dbl) / (w + r); /* r-s is exact */
  t.dbl = t.dbl + (t.dbl * r);

  /* retore the sign bit */
  t.as_int.hi |= sign;
  return (t.dbl);
} /* cbrt */

#undef B1
#undef B2
#undef C
#undef D
#undef E
#undef F
#undef G

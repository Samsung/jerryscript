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
 *     @(#)s_tanh.c 1.3 95/01/18
 */

#include "jerry-math-internal.h"

/* tanh(x)
 * Return the Hyperbolic Tangent of x
 *
 * Method:
 *                                 x   -x
 *                                e -  e
 *  0. tanh(x) is defined to be -----------
 *                                 x    -x
 *                                e  +  e
 *
 *  1. reduce x to non-negative by tanh(-x) = -tanh(x).
 *  2.  0      <= x <= 2**-55 : tanh(x) := x * (one + x)
 *
 *                                          -t
 *      2**-55 <  x <=  1     : tanh(x) := -----; t = expm1(-2x)
 *                                         t + 2
 *
 *                                               2
 *      1      <= x <=  22.0  : tanh(x) := 1-  ----- ; t = expm1(2x)
 *                                             t + 2
 *
 *      22.0   <  x <= INF    : tanh(x) := 1.
 *
 * Special cases:
 *  tanh(NaN) is NaN;
 *  only tanh(0) = 0 is exact for finite x.
 */
#define one  1.0
#define two  2.0
#define tiny 1.0e-300

double
tanh (double x)
{
  double t, z;
  int jx, ix;

  /* High word of |x|. */
  jx = __HI (x);
  ix = jx & 0x7fffffff;

  /* x is INF or NaN */
  if (ix >= 0x7ff00000)
  {
    if (jx >= 0)
    {
      /* tanh(+-inf) = +-1 */
      return one / x + one;
    }
    else
    {
      /* tanh(NaN) = NaN */
      return one / x - one;
    }
  }

  /* |x| < 22 */
  if (ix < 0x40360000)
  {
    /* |x| < 2**-55 */
    if (ix < 0x3c800000)
    {
      /* tanh(small) = small */
      return x * (one + x);
    }
    if (ix >= 0x3ff00000)
    {
      /* |x| >= 1  */
      t = expm1 (two * fabs (x));
      z = one - two / (t + two);
    }
    else
    {
      t = expm1 (-two * fabs (x));
      z = -t / (t + two);
    }
  }
  else
  {
    /* |x| > 22, return +-1 */
    z = one - tiny; /* raised inexact flag */
  }
  return (jx >= 0) ? z : -z;
} /* tanh */

#undef one
#undef two
#undef tiny

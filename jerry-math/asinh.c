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
 *     @(#)s_asinh.c 1.3 95/01/18
 */

#include "jerry-math-internal.h"

/* asinh(x)
 * Method :
 *  Based on
 *    asinh(x) = sign(x) * log [ |x| + sqrt(x*x+1) ]
 *  we have
 *  asinh(x) := x  if  1 + x * x = 1,
 *           := sign(x) * (log(x)+ln2)) for large |x|, else
 *           := sign(x) * log(2|x| + 1 / (|x| + sqrt(x * x + 1))) if|x| > 2, else
 *           := sign(x) * log1p(|x| + x^2 / (1 + sqrt(1 + x^2)))
 */

#define one  1.0
#define ln2  6.93147180559945286227e-01 /* 0x3FE62E42, 0xFEFA39EF */
#define huge 1.0e+300

double
asinh (double x)
{
  double t, w;
  int hx, ix;
  hx = __HI (x);
  ix = hx & 0x7fffffff;
  if (ix >= 0x7ff00000)
  {
    /* x is inf or NaN */
    return x + x;
  }
  if (ix < 0x3e300000)
  {
    /* |x| < 2**-28 */
    if (huge + x > one)
    {
      /* return x inexact except 0 */
      return x;
    }
  }
  if (ix > 0x41b00000)
  {
    /* |x| > 2**28 */
    w = log (fabs (x)) + ln2;
  }
  else if (ix > 0x40000000)
  {
    /* 2**28 > |x| > 2.0 */
    t = fabs (x);
    w = log (2.0 * t + one / (sqrt (x * x + one) + t));
  }
  else
  {
    /* 2.0 > |x| > 2**-28 */
    t = x * x;
    w = log1p (fabs (x) + t / (one + sqrt (one + t)));
  }
  if (hx > 0)
  {
    return w;
  }
  else
  {
    return -w;
  }
} /* asinh */

#undef one
#undef ln2
#undef huge

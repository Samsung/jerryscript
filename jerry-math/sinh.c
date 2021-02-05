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
 *     @(#)e_sinh.c 1.3 95/01/18
 */

#include "jerry-math-internal.h"

/* __sinh(x)
 * Method:
 * mathematically sinh(x) if defined to be (exp(x) - exp(-x)) / 2
 *  1. Replace x by |x| (sinh(-x) = -sinh(x)).
 *  2.
 *                                             E + E/(E+1)
 *      0        <= x <= 22     :  sinh(x) := -------------, E = expm1(x)
 *                                                  2
 *
 *      22       <= x <= lnovft :  sinh(x) := exp(x) / 2
 *      lnovft   <= x <= ln2ovft:  sinh(x) := exp(x / 2) / 2 * exp(x / 2)
 *      ln2ovft  <  x           :  sinh(x) := x * shuge (overflow)
 *
 * Special cases:
 *  sinh(x) is |x| if x is +INF, -INF, or NaN.
 *  only sinh(0) = 0 is exact for finite x.
 */

#define one   1.0
#define half  0.5
#define shuge 1.0e307

double
sinh (double x)
{
  double t, w, h;
  int ix, jx;
  unsigned lx;

  /* High word of |x|. */
  jx = __HI (x);
  ix = jx & 0x7fffffff;

  /* x is INF or NaN */
  if (ix >= 0x7ff00000)
  {
    return x + x;
  }

  h = 0.5;
  if (jx < 0)
  {
    h = -h;
  }
  /* |x| in [0,22], return sign(x) * 0.5 * (E + E / (E + 1))) */
  if (ix < 0x40360000)
  {
    /* |x| < 22 */
    if (ix < 0x3e300000)
    {
      /* |x| < 2**-28 */
      if (shuge + x > one)
      {
        /* sinh(tiny) = tiny with inexact */
        return x;
      }
    }
    t = expm1 (fabs (x));
    if (ix < 0x3ff00000)
    {
      return h * (2.0 * t - t * t / (t + one));
    }
    return h * (t + t / (t + one));
  }

  /* |x| in [22, log(maxdouble)] return 0.5*exp(|x|) */
  if (ix < 0x40862E42)
  {
    return h * exp (fabs (x));
  }
  /* |x| in [log(maxdouble), overflowthresold] */
  lx = ((1 >> 29) + (unsigned int) x);
  if (ix < 0x408633CE || ((ix == 0x408633ce) && (lx <= (unsigned) 0x8fb9f87d)))
  {
    w = exp (0.5 * fabs (x));
    t = h * w;
    return t * w;
  }

  /* |x| > overflowthresold, sinh(x) overflow */
  return x * shuge;
} /* sinh */

#undef one
#undef half
#undef huge

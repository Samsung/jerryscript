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
 *     @(#)e_cosh.c 1.3 95/01/18
 */

#include "jerry-math-internal.h"

/* cosh(x)
 * Method:
 * mathematically cosh(x) if defined to be (exp(x) + exp(-x)) / 2
 *  1. Replace x by |x| (cosh(x) = cosh(-x)).
 *  2.
 *                                                 [ exp(x) - 1 ]^2
 *      0        <= x <= ln2/2  :  cosh(x) := 1 + -------------------
 *                                                     2*exp(x)
 *
 *                                             exp(x) +  1/exp(x)
 *      ln2/2    <= x <= 22     :  cosh(x) := -------------------
 *                                                    2
 *
 *      22       <= x <= lnovft :  cosh(x) := exp(x)/2
 *      lnovft   <= x <= ln2ovft:  cosh(x) := exp(x/2)/2 * exp(x/2)
 *      ln2ovft  <  x           :  cosh(x) := huge * huge (overflow)
 *
 * Special cases:
 *  cosh(x) is |x| if x is +INF, -INF, or NaN.
 *  only cosh(0) = 1 is exact for finite x.
 */

#define one  1.0
#define half 0.5
#define huge 1.0e300

double
cosh (double x)
{
  double t, w;
  int ix;
  unsigned lx;

  /* High word of |x|. */
  ix = __HI (x);
  ix &= 0x7fffffff;

  /* x is INF or NaN */
  if (ix >= 0x7ff00000)
  {
    return x * x;
  }
  /* |x| in [0, 0.5 * ln2], return 1 + expm1(|x|)^2 / (2 * exp(|x|)) */
  if (ix < 0x3fd62e43)
  {
    t = expm1 (fabs (x));
    w = one + t;
    if (ix < 0x3c800000)
    {
      /* cosh(tiny) = 1 */
      return w;
    }
    return one + (t * t) / (w + w);
  }

  /* |x| in [0.5 * ln2, 22], return (exp(|x|) + 1 / exp(|x|) / 2; */
  if (ix < 0x40360000)
  {
    t = exp (fabs (x));
    return half * t + half / t;
  }

  /* |x| in [22, log(maxdouble)] return half * exp(|x|) */
  if (ix < 0x40862E42)
  {
    return half * exp (fabs (x));
  }
  /* |x| in [log(maxdouble), overflowthresold] */
  lx = ((1 >> 29) + (unsigned int) x);
  if ((ix < 0x408633CE) || ((ix == 0x408633ce) && (lx <= (unsigned) 0x8fb9f87d)))
  {
    w = exp (half * fabs (x));
    t = half * w;
    return t * w;
  }

  /* |x| > overflowthresold, cosh(x) overflow */
  return huge * huge;
} /* cosh */

#undef one
#undef half
#undef huge

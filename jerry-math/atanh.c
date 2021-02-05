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
 *     @(#)e_atanh.c 1.3 95/01/18
 */

#include "jerry-math-internal.h"

/* atanh(x)
 * Method :
 *    1.Reduced x to positive by atanh(-x) = -atanh(x)
 *    2.For x >= 0.5
 *              1              2x                          x
 *  atanh(x) = --- * log(1 + -------) = 0.5 * log1p(2 * --------)
 *              2             1 - x                      1 - x
 *
 *   For x < 0.5
 *    atanh(x) = 0.5 * log1p(2x + 2x * x / (1 - x))
 *
 * Special cases:
 *  atanh(x) is NaN if |x| > 1 with signal;
 *  atanh(NaN) is that NaN with no signal;
 *  atanh(+-1) is +-INF with signal.
 *
 */

#define zero 0.0
#define one  1.0
#define huge 1.0e+300

double
atanh (double x)
{
  double t;
  int hx, ix;
  double_accessor temp;
  temp.dbl = x;
  hx = temp.as_int.hi;
  ix = hx & 0x7fffffff;

  /* |x| > 1 */
  if ((ix | ((unsigned int) (temp.as_int.lo | (-temp.as_int.lo)) >> 31)) > 0x3ff00000)
  {
    return NAN;
  }
  if (ix == 0x3ff00000)
  {
    return x / zero;
  }
  if (ix < 0x3e300000 && (huge + x) > zero)
  {
    return x; /* x<2**-28 */
  }

  /* x <- |x| */
  temp.as_int.hi = ix;
  if (ix < 0x3fe00000)
  {
    /* x < 0.5 */
    t = temp.dbl + temp.dbl;
    t = 0.5 * log1p (t + t * temp.dbl / (one - temp.dbl));
  }
  else
  {
    t = 0.5 * log1p ((temp.dbl + temp.dbl) / (one - temp.dbl));
  }
  if (hx >= 0)
  {
    return t;
  }
  else
  {
    return -t;
  }
} /* atanh */

#undef zero
#undef one
#undef huge

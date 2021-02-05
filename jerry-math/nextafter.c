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
 *     @(#)s_nextafter.c 1.3 95/01/18
 */

#include "jerry-math-internal.h"

double
nextafter (double x, double y)
{
  int hx, hy, ix, iy;
  unsigned lx, ly;
  double_accessor ret;

  hx = __HI (x); /* high word of x */
  lx = __LO (x); /* low  word of x */
  hy = __HI (y); /* high word of y */
  ly = __LO (y); /* low  word of y */
  ix = hx & 0x7fffffff; /* |x| */
  iy = hy & 0x7fffffff; /* |y| */

  if (((ix >= 0x7ff00000) && ((ix - 0x7ff00000) | lx) != 0) /* x is nan */
      || ((iy >= 0x7ff00000) && ((iy - 0x7ff00000) | ly) != 0)) /* y is nan */
  {
    return x + y;
  }

  if (x == y)
  {
    return x; /* x=y, return x */
  }

  if ((ix | lx) == 0)
  { /* x == 0 */
    ret.as_int.hi = hy & 0x80000000; /* return +-minsubnormal */
    ret.as_int.lo = 1;
    y = ret.dbl * ret.dbl;
    if (y == ret.dbl)
    {
      return y;
    }
    else
    {
      return ret.dbl; /* raise underflow flag */
    }
  }

  if (hx >= 0)
  { /* x > 0 */
    if (hx > hy || ((hx == hy) && (lx > ly)))
    { /* x > y, x -= ulp */
      if (lx == 0)
      {
        hx -= 1;
      }

      lx -= 1;
    }
    else
    { /* x < y, x += ulp */
      lx += 1;

      if (lx == 0)
      {
        hx += 1;
      }
    }
  }
  else
  { /* x < 0 */
    if (hy >= 0 || hx > hy || ((hx == hy) && (lx > ly)))
    { /* x < y, x -= ulp */
      if (lx == 0)
      {
        hx -= 1;
      }

      lx -= 1;
    }
    else
    { /* x > y, x += ulp */
      lx += 1;

      if (lx == 0)
      {
        hx += 1;
      }
    }
  }

  hy = hx & 0x7ff00000;

  if (hy >= 0x7ff00000)
  {
    return x + x; /* overflow */
  }

  if (hy < 0x00100000)
  { /* underflow */
    y = x * x;
    if (y != x)
    { /* raise underflow flag */
      ret.as_int.hi = hx;
      ret.as_int.lo = lx;
      return ret.dbl;
    }
  }

  ret.as_int.hi = hx;
  ret.as_int.lo = lx;
  return ret.dbl;
} /* nextafter */

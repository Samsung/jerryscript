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
 *     @(#)e_fmod.c 1.3 95/01/18
 */

#include "jerry-math-internal.h"

/* fmod(x,y)
 * Return x mod y in exact arithmetic
 *
 * Method: shift and subtract
 */

static const double Zero[] = {
  0.0,
  -0.0,
};

double
fmod (double x, double y)
{
  int n, hx, hy, hz, ix, iy, sx, i;
  unsigned lx, ly, lz;

  hx = __HI (x); /* high word of x */
  lx = __LO (x); /* low  word of x */
  hy = __HI (y); /* high word of y */
  ly = __LO (y); /* low  word of y */
  sx = hx & 0x80000000; /* sign of x */
  hx ^= sx; /* |x| */
  hy &= 0x7fffffff; /* |y| */

  /* purge off exception values */
  if ((hy | ly) == 0 || (hx >= 0x7ff00000) || /* y = 0, or x not finite */
      ((hy | ((ly | -ly) >> 31)) > 0x7ff00000)) /* or y is NaN */
  {
    return NAN;
  }
  if (hx <= hy)
  {
    if ((hx < hy) || (lx < ly)) /* |x| < |y| return x */
    {
      return x;
    }
    if (lx == ly) /* |x| = |y| return x * 0 */
    {
      return Zero[(unsigned) sx >> 31];
    }
  }

  /* determine ix = ilogb(x) */
  if (hx < 0x00100000) /* subnormal x */
  {
    if (hx == 0)
    {
      for (ix = -1043, i = lx; i > 0; i <<= 1)
      {
        ix -= 1;
      }
    }
    else
    {
      for (ix = -1022, i = (hx << 11); i > 0; i <<= 1)
      {
        ix -= 1;
      }
    }
  }
  else
  {
    ix = (hx >> 20) - 1023;
  }

  /* determine iy = ilogb(y) */
  if (hy < 0x00100000) /* subnormal y */
  {
    if (hy == 0)
    {
      for (iy = -1043, i = ly; i > 0; i <<= 1)
      {
        iy -= 1;
      }
    }
    else
    {
      for (iy = -1022, i = (hy << 11); i > 0; i <<= 1)
      {
        iy -= 1;
      }
    }
  }
  else
  {
    iy = (hy >> 20) - 1023;
  }

  /* set up {hx,lx}, {hy,ly} and align y to x */
  if (ix >= -1022)
  {
    hx = 0x00100000 | (0x000fffff & hx);
  }
  else /* subnormal x, shift x to normal */
  {
    n = -1022 - ix;
    if (n <= 31)
    {
      hx = (((unsigned int) hx) << n) | (lx >> (32 - n));
      lx <<= n;
    }
    else
    {
      hx = lx << (n - 32);
      lx = 0;
    }
  }
  if (iy >= -1022)
  {
    hy = 0x00100000 | (0x000fffff & hy);
  }
  else /* subnormal y, shift y to normal */
  {
    n = -1022 - iy;
    if (n <= 31)
    {
      hy = (((unsigned int) hy) << n) | (ly >> (32 - n));
      ly <<= n;
    }
    else
    {
      hy = ly << (n - 32);
      ly = 0;
    }
  }

  /* fix point fmod */
  n = ix - iy;
  while (n--)
  {
    hz = hx - hy;
    lz = lx - ly;
    if (lx < ly)
    {
      hz -= 1;
    }
    if (hz < 0)
    {
      hx = hx + hx + (lx >> 31);
      lx = lx + lx;
    }
    else
    {
      if ((hz | lz) == 0) /* return sign(x) * 0 */
      {
        return Zero[(unsigned) sx >> 31];
      }
      hx = hz + hz + (lz >> 31);
      lx = lz + lz;
    }
  }
  hz = hx - hy;
  lz = lx - ly;
  if (lx < ly)
  {
    hz -= 1;
  }
  if (hz >= 0)
  {
    hx = hz;
    lx = lz;
  }

  /* convert back to floating value and restore the sign */
  if ((hx | lx) == 0) /* return sign(x) * 0 */
  {
    return Zero[(unsigned) sx >> 31];
  }
  while (hx < 0x00100000) /* normalize x */
  {
    hx = hx + hx + (lx >> 31);
    lx = lx + lx;
    iy -= 1;
  }

  double_accessor ret;
  if (iy >= -1022) /* normalize output */
  {
    hx = ((hx - 0x00100000) | ((iy + 1023) << 20));
    ret.as_int.hi = hx | sx;
    ret.as_int.lo = lx;
  }
  else /* subnormal output */
  {
    n = -1022 - iy;
    if (n <= 20)
    {
      lx = (lx >> n) | ((unsigned) hx << (32 - n));
      hx >>= n;
    }
    else if (n <= 31)
    {
      lx = (hx << (32 - n)) | (lx >> n);
      hx = sx;
    }
    else
    {
      lx = hx >> (n - 32);
      hx = sx;
    }
    ret.as_int.hi = hx | sx;
    ret.as_int.lo = lx;
  }
  return ret.dbl; /* exact output */
} /* fmod */

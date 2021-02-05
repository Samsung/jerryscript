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
 *     Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 *     Permission to use, copy, modify, and distribute this
 *     software is freely granted, provided that this notice
 *     is preserved.
 *
 *     @(#)e_pow.c 1.5 04/04/22
 */

#include "jerry-math-internal.h"

/* pow(x,y) return x**y
 *
 *                    n
 * Method:  Let x =  2   * (1+f)
 *      1. Compute and return log2(x) in two pieces:
 *              log2(x) = w1 + w2,
 *         where w1 has 53-24 = 29 bit trailing zeros.
 *      2. Perform y*log2(x) = n+y' by simulating muti-precision
 *         arithmetic, where |y'|<=0.5.
 *      3. Return x**y = 2**n*exp(y'*log2)
 *
 * Special cases:
 *      0.  +1 ** (anything) is 1
 *      1.  (anything) ** 0  is 1
 *      2.  (anything) ** 1  is itself
 *      3.  (anything) ** NAN is NAN
 *      4.  NAN ** (anything except 0) is NAN
 *      5.  +-(|x| > 1) **  +INF is +INF
 *      6.  +-(|x| > 1) **  -INF is +0
 *      7.  +-(|x| < 1) **  +INF is +0
 *      8.  +-(|x| < 1) **  -INF is +INF
 *      9.  -1          ** +-INF is 1
 *      10. +0 ** (+anything except 0, NAN)               is +0
 *      11. -0 ** (+anything except 0, NAN, odd integer)  is +0
 *      12. +0 ** (-anything except 0, NAN)               is +INF
 *      13. -0 ** (-anything except 0, NAN, odd integer)  is +INF
 *      14. -0 ** (odd integer) = -( +0 ** (odd integer) )
 *      15. +INF ** (+anything except 0,NAN) is +INF
 *      16. +INF ** (-anything except 0,NAN) is +0
 *      17. -INF ** (anything)  = -0 ** (-anything)
 *      18. (-anything) ** (integer) is (-1)**(integer)*(+anything**integer)
 *      19. (-anything except 0 and inf) ** (non-integer) is NAN
 *
 * Accuracy:
 *      pow(x,y) returns x**y nearly rounded. In particular
 *                      pow(integer,integer)
 *      always returns the correct integer provided it is
 *      representable.
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */

static const double bp[] = {
  1.0,
  1.5,
};
static const double dp_h[] = {
  0.0,
  5.84962487220764160156e-01, /* 0x3FE2B803, 0x40000000 */
};
static const double dp_l[] = {
  0.0,
  1.35003920212974897128e-08, /* 0x3E4CFDEB, 0x43CFD006 */
};

#define zero  0.0
#define one   1.0
#define two   2.0
#define two53 9007199254740992.0 /* 0x43400000, 0x00000000 */
#define huge  1.0e300
#define tiny  1.0e-300
/* poly coefs for (3/2) * (log(x) - 2s - 2/3 * s**3 */
#define L1      5.99999999999994648725e-01 /* 0x3FE33333, 0x33333303 */
#define L2      4.28571428578550184252e-01 /* 0x3FDB6DB6, 0xDB6FABFF */
#define L3      3.33333329818377432918e-01 /* 0x3FD55555, 0x518F264D */
#define L4      2.72728123808534006489e-01 /* 0x3FD17460, 0xA91D4101 */
#define L5      2.30660745775561754067e-01 /* 0x3FCD864A, 0x93C9DB65 */
#define L6      2.06975017800338417784e-01 /* 0x3FCA7E28, 0x4A454EEF */
#define P1      1.66666666666666019037e-01 /* 0x3FC55555, 0x5555553E */
#define P2      -2.77777777770155933842e-03 /* 0xBF66C16C, 0x16BEBD93 */
#define P3      6.61375632143793436117e-05 /* 0x3F11566A, 0xAF25DE2C */
#define P4      -1.65339022054652515390e-06 /* 0xBEBBBD41, 0xC5D26BF1 */
#define P5      4.13813679705723846039e-08 /* 0x3E663769, 0x72BEA4D0 */
#define lg2     6.93147180559945286227e-01 /* 0x3FE62E42, 0xFEFA39EF */
#define lg2_h   6.93147182464599609375e-01 /* 0x3FE62E43, 0x00000000 */
#define lg2_l   -1.90465429995776804525e-09 /* 0xBE205C61, 0x0CA86C39 */
#define ovt     8.0085662595372944372e-0017 /* -(1024-log2(ovfl+.5ulp)) */
#define cp      9.61796693925975554329e-01 /* 0x3FEEC709, 0xDC3A03FD = 2 / (3 ln2) */
#define cp_h    9.61796700954437255859e-01 /* 0x3FEEC709, 0xE0000000 = (float) cp */
#define cp_l    -7.02846165095275826516e-09 /* 0xBE3E2FE0, 0x145B01F5 = tail of cp_h */
#define ivln2   1.44269504088896338700e+00 /* 0x3FF71547, 0x652B82FE = 1 / ln2 */
#define ivln2_h 1.44269502162933349609e+00 /* 0x3FF71547, 0x60000000 = 24b 1 / ln2 */
#define ivln2_l 1.92596299112661746887e-08 /* 0x3E54AE0B, 0xF85DDF44 = 1 / ln2 tail */

double
pow (double x, double y)
{
  double_accessor t1, ax, p_h, y1, t, z;
  double z_h, z_l, p_l;
  double t2, r, s, u, v, w;
  int i, j, k, yisint, n;
  int hx, hy, ix, iy;
  unsigned lx, ly;

  hx = __HI (x);
  lx = __LO (x);
  hy = __HI (y);
  ly = __LO (y);
  ix = hx & 0x7fffffff;
  iy = hy & 0x7fffffff;

  /* x == one: 1**y = 1 */
  if (((hx - 0x3ff00000) | lx) == 0)
  {
    return one;
  }

  /* y == zero: x**0 = 1 */
  if ((iy | ly) == 0)
  {
    return one;
  }

  /* +-NaN return x + y */
  if (ix > 0x7ff00000 || ((ix == 0x7ff00000) && (lx != 0)) || iy > 0x7ff00000 || ((iy == 0x7ff00000) && (ly != 0)))
  {
    return x + y;
  }

  /* determine if y is an odd int when x < 0
   * yisint = 0 ... y is not an integer
   * yisint = 1 ... y is an odd int
   * yisint = 2 ... y is an even int
   */
  yisint = 0;
  if (hx < 0)
  {
    if (iy >= 0x43400000) /* even integer y */
    {
      yisint = 2;
    }
    else if (iy >= 0x3ff00000)
    {
      k = (iy >> 20) - 0x3ff; /* exponent */
      if (k > 20)
      {
        j = ly >> (52 - k);
        if ((j << (52 - k)) == ly)
        {
          yisint = 2 - (j & 1);
        }
      }
      else if (ly == 0)
      {
        j = iy >> (20 - k);
        if ((j << (20 - k)) == iy)
        {
          yisint = 2 - (j & 1);
        }
      }
    }
  }

  /* special value of y */
  if (ly == 0)
  {
    if (iy == 0x7ff00000) /* y is +-inf */
    {
      if (((ix - 0x3ff00000) | lx) == 0) /* +-1**+-inf is 1 */
      {
        return one;
      }
      else if (ix >= 0x3ff00000) /* (|x|>1)**+-inf = inf,0 */
      {
        return (hy >= 0) ? y : zero;
      }
      else /* (|x|<1)**-,+inf = inf,0 */
      {
        return (hy < 0) ? -y : zero;
      }
    }
    if (iy == 0x3ff00000) /* y is +-1 */
    {
      if (hy < 0)
      {
        return one / x;
      }
      else
      {
        return x;
      }
    }
    if (hy == 0x40000000) /* y is 2 */
    {
      return x * x;
    }
    if (hy == 0x3fe00000) /* y is 0.5 */
    {
      if (hx >= 0) /* x >= +0 */
      {
        return sqrt (x);
      }
    }
  }

  ax.dbl = fabs (x);
  /* special value of x */
  if (lx == 0)
  {
    if (ix == 0x7ff00000 || ix == 0 || ix == 0x3ff00000)
    {
      z.dbl = ax.dbl; /* x is +-0,+-inf,+-1 */
      if (hy < 0)
      {
        z.dbl = one / z.dbl; /* z = (1 / |x|) */
      }
      if (hx < 0)
      {
        if (((ix - 0x3ff00000) | yisint) == 0)
        {
          z.dbl = NAN; /* (-1)**non-int is NaN */
        }
        else if (yisint == 1)
        {
          z.dbl = -z.dbl; /* (x<0)**odd = -(|x|**odd) */
        }
      }
      return z.dbl;
    }
  }

  n = (hx < 0) ? 0 : 1;

  /* (x<0)**(non-int) is NaN */
  if ((n | yisint) == 0)
  {
    return NAN;
  }

  s = one; /* s (sign of result -ve**odd) = -1 else = 1 */
  if ((n | (yisint - 1)) == 0)
  {
    s = -one; /* (-ve)**(odd int) */
  }

  /* |y| is huge */
  if (iy > 0x41e00000) /* if |y| > 2**31 */
  {
    if (iy > 0x43f00000) /* if |y| > 2**64, must o/uflow */
    {
      if (ix <= 0x3fefffff)
      {
        return (hy < 0) ? huge * huge : tiny * tiny;
      }
      if (ix >= 0x3ff00000)
      {
        return (hy > 0) ? huge * huge : tiny * tiny;
      }
    }
    /* over/underflow if x is not close to one */
    if (ix < 0x3fefffff)
    {
      return (hy < 0) ? s * huge * huge : s * tiny * tiny;
    }
    if (ix > 0x3ff00000)
    {
      return (hy > 0) ? s * huge * huge : s * tiny * tiny;
    }
    /* now |1 - x| is tiny <= 2**-20, suffice to compute
       log(x) by x - x^2 / 2 + x^3 / 3 - x^4 / 4 */
    t.dbl = ax.dbl - one; /* t has 20 trailing zeros */
    w = (t.dbl * t.dbl) * (0.5 - t.dbl * (0.3333333333333333333333 - t.dbl * 0.25));
    u = ivln2_h * t.dbl; /* ivln2_h has 21 sig. bits */
    v = t.dbl * ivln2_l - w * ivln2;
    t1.dbl = u + v;
    t1.as_int.lo = 0;
    t2 = v - (t1.dbl - u);
  }
  else
  {
    double_accessor s_h, t_h;
    double ss, s2, s_l, t_l;

    n = 0;
    /* take care subnormal number */
    if (ix < 0x00100000)
    {
      ax.dbl *= two53;
      n -= 53;
      ix = ax.as_int.hi;
    }
    n += ((ix) >> 20) - 0x3ff;
    j = ix & 0x000fffff;
    /* determine interval */
    ix = j | 0x3ff00000; /* normalize ix */
    if (j <= 0x3988E) /* |x| < sqrt(3/2) */
    {
      k = 0;
    }
    else if (j < 0xBB67A) /* |x| < sqrt(3) */
    {
      k = 1;
    }
    else
    {
      k = 0;
      n += 1;
      ix -= 0x00100000;
    }
    ax.as_int.hi = ix;

    /* compute ss = s_h + s_l = (x - 1) / (x + 1) or (x - 1.5) / (x + 1.5) */
    u = ax.dbl - bp[k]; /* bp[0] = 1.0, bp[1] = 1.5 */
    v = one / (ax.dbl + bp[k]);
    ss = u * v;
    s_h.dbl = ss;
    s_h.as_int.lo = 0;
    /* t_h = ax + bp[k] High */
    t_h.dbl = zero;
    t_h.as_int.hi = ((ix >> 1) | 0x20000000) + 0x00080000 + (k << 18);
    t_l = ax.dbl - (t_h.dbl - bp[k]);
    s_l = v * ((u - s_h.dbl * t_h.dbl) - s_h.dbl * t_l);
    /* compute log(ax) */
    s2 = ss * ss;
    r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
    r += s_l * (s_h.dbl + ss);
    s2 = s_h.dbl * s_h.dbl;
    t_h.dbl = 3.0 + s2 + r;
    t_h.as_int.lo = 0;
    t_l = r - ((t_h.dbl - 3.0) - s2);
    /* u + v = ss * (1 + ...) */
    u = s_h.dbl * t_h.dbl;
    v = s_l * t_h.dbl + t_l * ss;
    /* 2 / (3 * log2) * (ss + ...) */
    p_h.dbl = u + v;
    p_h.as_int.lo = 0;
    p_l = v - (p_h.dbl - u);
    z_h = cp_h * p_h.dbl; /* cp_h + cp_l = 2 / (3 * log2) */
    z_l = cp_l * p_h.dbl + p_l * cp + dp_l[k];
    /* log2(ax) = (ss + ...) * 2 / (3 * log2) = n + dp_h + z_h + z_l */
    t.dbl = (double) n;
    t1.dbl = (((z_h + z_l) + dp_h[k]) + t.dbl);
    t1.as_int.lo = 0;
    t2 = z_l - (((t1.dbl - t.dbl) - dp_h[k]) - z_h);
  }

  /* split up y into y1 + y2 and compute (y1 + y2) * (t1 + t2) */
  y1.dbl = y;
  y1.as_int.lo = 0;
  p_l = (y - y1.dbl) * t1.dbl + y * t2;
  p_h.dbl = y1.dbl * t1.dbl;
  z.dbl = p_l + p_h.dbl;
  j = z.as_int.hi;
  i = z.as_int.lo;
  if (j >= 0x40900000) /* z >= 1024 */
  {
    if (((j - 0x40900000) | i) != 0) /* if z > 1024 */
    {
      return s * huge * huge; /* overflow */
    }
    else
    {
      if (p_l + ovt > z.dbl - p_h.dbl)
      {
        return s * huge * huge; /* overflow */
      }
    }
  }
  else if ((j & 0x7fffffff) >= 0x4090cc00) /* z <= -1075 */
  {
    if (((j - 0xc090cc00) | i) != 0) /* z < -1075 */
    {
      return s * tiny * tiny; /* underflow */
    }
    else
    {
      if (p_l <= z.dbl - p_h.dbl)
      {
        return s * tiny * tiny; /* underflow */
      }
    }
  }
  /*
   * compute 2**(p_h + p_l)
   */
  i = j & 0x7fffffff;
  k = (i >> 20) - 0x3ff;
  n = 0;
  if (i > 0x3fe00000) /* if |z| > 0.5, set n = [z + 0.5] */
  {
    n = j + (0x00100000 >> (k + 1));
    k = ((n & 0x7fffffff) >> 20) - 0x3ff; /* new k for n */
    t.dbl = zero;
    t.as_int.hi = (n & ~(0x000fffff >> k));
    n = ((n & 0x000fffff) | 0x00100000) >> (20 - k);
    if (j < 0)
    {
      n = -n;
    }
    p_h.dbl -= t.dbl;
  }
  t.dbl = p_l + p_h.dbl;
  t.as_int.lo = 0;
  u = t.dbl * lg2_h;
  v = (p_l - (t.dbl - p_h.dbl)) * lg2 + t.dbl * lg2_l;
  z.dbl = u + v;
  w = v - (z.dbl - u);
  t.dbl = z.dbl * z.dbl;
  t1.dbl = z.dbl - t.dbl * (P1 + t.dbl * (P2 + t.dbl * (P3 + t.dbl * (P4 + t.dbl * P5))));
  r = (z.dbl * t1.dbl) / (t1.dbl - two) - (w + z.dbl * w);
  z.dbl = one - (r - z.dbl);
  j = z.as_int.hi;
  j += (n << 20);
  if ((j >> 20) <= 0) /* subnormal output */
  {
    z.dbl = scalbn (z.dbl, n);
  }
  else
  {
    z.as_int.hi += (n << 20);
  }
  return s * z.dbl;
} /* pow */

#undef zero
#undef one
#undef two
#undef two53
#undef huge
#undef tiny
#undef L1
#undef L2
#undef L3
#undef L4
#undef L5
#undef L6
#undef P1
#undef P2
#undef P3
#undef P4
#undef P5
#undef lg2
#undef lg2_h
#undef lg2_l
#undef ovt
#undef cp
#undef cp_h
#undef cp_l
#undef ivln2
#undef ivln2_h
#undef ivln2_l

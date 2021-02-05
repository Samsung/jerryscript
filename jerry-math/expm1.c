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
 *     Permission to use, copy, modify, and distribute this
 *     software is freely granted, provided that this notice
 *     is preserved.
 *
 *     @(#)s_expm1.c 5.1 93/09/24
 */

#include "jerry-math-internal.h"

/* expm1(x)
 * Returns exp(x)-1, the exponential of x minus 1.
 *
 * Method
 *   1. Argument reduction:
 *  Given x, find r and integer k such that
 *
 *               x = k*ln2 + r,  |r| <= 0.5*ln2 ~ 0.34658
 *
 *      Here a correction term c will be computed to compensate
 *  the error in r when rounded to a floating-point number.
 *
 *   2. Approximating expm1(r) by a special rational function on
 *  the interval [0,0.34658]:
 *  Since
 *      r*(exp(r)+1)/(exp(r)-1) = 2+ r^2/6 - r^4/360 + ...
 *  we define R1(r*r) by
 *      r*(exp(r)+1)/(exp(r)-1) = 2+ r^2/6 * R1(r*r)
 *  That is,
 *      R1(r**2) = 6/r *((exp(r)+1)/(exp(r)-1) - 2/r)
 *         = 6/r * ( 1 + 2.0*(1/(exp(r)-1) - 1/r))
 *         = 1 - r^2/60 + r^4/2520 - r^6/100800 + ...
 *      We use a special Reme algorithm on [0,0.347] to generate
 *   a polynomial of degree 5 in r*r to approximate R1. The
 *  maximum error of this polynomial approximation is bounded
 *  by 2**-61. In other words,
 *      R1(z) ~ 1.0 + Q1*z + Q2*z**2 + Q3*z**3 + Q4*z**4 + Q5*z**5
 *  where   Q1  =  -1.6666666666666567384E-2,
 *     Q2  =   3.9682539681370365873E-4,
 *     Q3  =  -9.9206344733435987357E-6,
 *     Q4  =   2.5051361420808517002E-7,
 *     Q5  =  -6.2843505682382617102E-9;
 *    z   =  r*r,
 *  with error bounded by
 *      |                  5           |     -61
 *      | 1.0+Q1*z+...+Q5*z   -  R1(z) | <= 2
 *      |                              |
 *
 *  expm1(r) = exp(r)-1 is then computed by the following
 *   specific way which minimize the accumulation rounding error:
 *                        2     3
 *                        r     r    [ 3 - (R1 + R1*r/2)  ]
 *        expm1(r) = r + --- + --- * [--------------------]
 *                        2     2    [ 6 - r*(3 - R1*r/2) ]
 *
 *  To compensate the error in the argument reduction, we use
 *    expm1(r+c) = expm1(r) + c + expm1(r)*c
 *         ~ expm1(r) + c + r*c
 *  Thus c+r*c will be added in as the correction terms for
 *  expm1(r+c). Now rearrange the term to avoid optimization
 *   screw up:
 *                  (      2                                    2 )
 *                  ({  ( r    [ R1 -  (3 - R1*r/2) ]  )  }    r  )
 *   expm1(r+c)~r - ({r*(--- * [--------------------]-c)-c} - --- )
 *                  ({  ( 2    [ 6 - r*(3 - R1*r/2) ]  )  }    2  )
 *                  (                                             )
 *
 *       = r - E
 *   3. Scale back to obtain expm1(x):
 *  From step 1, we have
 *     expm1(x) = either 2^k*[expm1(r)+1] - 1
 *              = or     2^k*[expm1(r) + (1-2^-k)]
 *   4. Implementation notes:
 *  (A). To save one multiplication, we scale the coefficient Qi
 *       to Qi*2^i, and replace z by (x^2)/2.
 *  (B). To achieve maximum accuracy, we compute expm1(x) by
 *    (i)   if x < -56*ln2, return -1.0, (raise inexact if x!=inf)
 *    (ii)  if k=0, return r-E
 *    (iii) if k=-1, return 0.5*(r-E)-0.5
 *    (iv)  if k=1 if r < -0.25, return 2*((r+0.5)- E)
 *                  else       return  1.0+2.0*(r-E);
 *    (v)   if (k<-2||k>56) return 2^k(1-(E-r)) - 1 (or exp(x)-1)
 *    (vi)  if k <= 20, return 2^k((1-2^-k)-(E-r)), else
 *    (vii) return 2^k(1-((E+2^-k)-r))
 *
 * Special cases:
 *  expm1(INF) is INF, expm1(NaN) is NaN;
 *  expm1(-INF) is -1, and
 *  for finite argument, only expm1(0)=0 is exact.
 *
 * Accuracy:
 *  according to an error analysis, the error is always less than
 *  1 ulp (unit in the last place).
 *
 * Misc. info.
 *  For IEEE double
 *      if x >  7.09782712893383973096e+02 then expm1(x) overflow
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */

#define one         1.0
#define huge        1.0e+300
#define tiny        1.0e-300
#define o_threshold 7.09782712893383973096e+02 /* 0x40862E42, 0xFEFA39EF */
#define ln2_hi      6.93147180369123816490e-01 /* 0x3fe62e42, 0xfee00000 */
#define ln2_lo      1.90821492927058770002e-10 /* 0x3dea39ef, 0x35793c76 */
#define invln2      1.44269504088896338700e+00 /* 0x3ff71547, 0x652b82fe */

/* Scaled Q's: Qn_here = 2**n * Qn_above, for R(2*z) where z = hxs = x*x/2: */
#define Q1 -3.33333333333331316428e-02 /* BFA11111 111110F4 */
#define Q2 1.58730158725481460165e-03 /* 3F5A01A0 19FE5585 */
#define Q3 -7.93650757867487942473e-05 /* BF14CE19 9EAADBB7 */
#define Q4 4.00821782732936239552e-06 /* 3ED0CFCA 86E65239 */
#define Q5 -2.01099218183624371326e-07 /* BE8AFDB7 6E09C32D */

double
expm1 (double x)
{
  double y, hi, lo, c, e, hxs, hfx, r1;
  double_accessor t, twopk;
  int k, xsb;
  unsigned int hx;

  hx = __HI (x);
  xsb = hx & 0x80000000; /* sign bit of x */
  hx &= 0x7fffffff; /* high word of |x| */

  /* filter out huge and non-finite argument */
  if (hx >= 0x4043687A)
  {
    /* if |x|>=56*ln2 */
    if (hx >= 0x40862E42)
    {
      /* if |x|>=709.78... */
      if (hx >= 0x7ff00000)
      {
        unsigned int low;
        low = __LO (x);
        if (((hx & 0xfffff) | low) != 0)
        {
          /* NaN */
          return x + x;
        }
        else
        {
          /* exp(+-inf)-1={inf,-1} */
          return (xsb == 0) ? x : -1.0;
        }
      }
      if (x > o_threshold)
      {
        /* overflow */
        return huge * huge;
      }
    }
    if (xsb != 0)
    {
      /* x < -56*ln2, return -1.0 with inexact */
      if (x + tiny < 0.0) /* raise inexact */
      {
        /* return -1 */
        return tiny - one;
      }
    }
  }

  /* argument reduction */
  if (hx > 0x3fd62e42)
  {
    /* if  |x| > 0.5 ln2 */
    if (hx < 0x3FF0A2B2)
    {
      /* and |x| < 1.5 ln2 */
      if (xsb == 0)
      {
        hi = x - ln2_hi;
        lo = ln2_lo;
        k = 1;
      }
      else
      {
        hi = x + ln2_hi;
        lo = -ln2_lo;
        k = -1;
      }
    }
    else
    {
      k = (int) (invln2 * x + ((xsb == 0) ? 0.5 : -0.5));
      t.dbl = k;
      hi = x - t.dbl * ln2_hi; /* t*ln2_hi is exact here */
      lo = t.dbl * ln2_lo;
    }
    x = hi - lo;
    c = (hi - x) - lo;
  }
  else if (hx < 0x3c900000)
  {
    /* when |x|<2**-54, return x */
    return x;
  }
  else
  {
    k = 0;
  }

  /* x is now in primary range */
  hfx = 0.5 * x;
  hxs = x * hfx;
  r1 = one + hxs * (Q1 + hxs * (Q2 + hxs * (Q3 + hxs * (Q4 + hxs * Q5))));
  t.dbl = 3.0 - r1 * hfx;
  e = hxs * ((r1 - t.dbl) / (6.0 - x * t.dbl));
  if (k == 0)
  {
    /* c is 0 */
    return x - (x * e - hxs);
  }
  else
  {
    twopk.as_int.hi = 0x3ff00000 + ((unsigned int) k << 20); /* 2^k */
    twopk.as_int.lo = 0;
    e = (x * (e - c) - c);
    e -= hxs;
    if (k == -1)
    {
      return 0.5 * (x - e) - 0.5;
    }
    if (k == 1)
    {
      if (x < -0.25)
      {
        return -2.0 * (e - (x + 0.5));
      }
      else
      {
        return one + 2.0 * (x - e);
      }
    }
    if ((k <= -2) || (k > 56))
    {
      /* suffice to return exp(x)-1 */
      y = one - (e - x);
      if (k == 1024)
      {
        const double twop1023 = ((double_accessor){ .as_int = { .hi = 0x7fe00000, .lo = 0 } }).dbl; /* 0x1p1023 */
        y = y * 2.0 * twop1023;
      }
      else
      {
        y = y * twopk.dbl;
      }
      return y - one;
    }
    t.dbl = one;
    if (k < 20)
    {
      t.as_int.hi = (0x3ff00000 - (0x200000 >> k)); /* t=1-2^-k */
      y = t.dbl - (e - x);
      y = y * twopk.dbl;
    }
    else
    {
      t.as_int.hi = ((0x3ff - k) << 20); /* 2^-k */
      y = x - (e + t.dbl);
      y += one;
      y = y * twopk.dbl;
    }
  }
  return y;
} /* expm1 */

#undef one
#undef huge
#undef tiny
#undef o_threshold
#undef ln2_hi
#undef ln2_lo
#undef invln2
#undef Q1
#undef Q2
#undef Q3
#undef Q4
#undef Q5

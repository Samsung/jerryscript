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
 *     @(#)e_log2.c 1.3 95/01/18
 */

#include "jerry-math-internal.h"

/* log2(x)
 * Return the base 2 logarithm of x.  See e_log.c and k_log.h for most
 * comments.
 *
 * This reduces x to {k, 1+f} exactly as in e_log.c, then calls the kernel,
 * then does the combining and scaling steps
 *    log2(x) = (f - 0.5*f*f + k_log1p(f)) / ln2 + k
 * in not-quite-routine extra precision.
 */

#define zero    0.0
#define two54   1.80143985094819840000e+16 /* 0x43500000, 0x00000000 */
#define ivln2hi 1.44269504072144627571e+00 /* 0x3FF71547, 0x65200000 */
#define ivln2lo 1.67517131648865118353e-10 /* 0x3DE705FC, 0x2EEFA200 */
#define Lg1     6.666666666666735130e-01 /* 0x3FE55555, 0x55555593 */
#define Lg2     3.999999999940941908e-01 /* 0x3FD99999, 0x9997FA04 */
#define Lg3     2.857142874366239149e-01 /* 0x3FD24924, 0x94229359 */
#define Lg4     2.222219843214978396e-01 /* 0x3FCC71C5, 0x1D8E78AF */
#define Lg5     1.818357216161805012e-01 /* 0x3FC74664, 0x96CB03DE */
#define Lg6     1.531383769920937332e-01 /* 0x3FC39A09, 0xD078C69F */
#define Lg7     1.479819860511658591e-01 /* 0x3FC2F112, 0xDF3E5244 */

double
log2 (double x)
{
  double f, hfsq, hi, lo, r, val_hi, val_lo, w, y;
  int i, k, hx;
  unsigned int lx;
  double_accessor temp;

  hx = __HI (x); /* high word of x */
  lx = __LO (x); /* low word of x */

  k = 0;
  if (hx < 0x00100000)
  { /* x < 2**-1022  */
    if (((hx & 0x7fffffff) | lx) == 0)
    {
      return -INFINITY; /* log(+-0)=-inf */
    }
    if (hx < 0)
    {
      return NAN; /* log(-#) = NaN */
    }
    k -= 54;
    x *= two54; /* subnormal number, scale up x */
    hx = __HI (x); /* high word of x */
  }
  if (hx >= 0x7ff00000)
  {
    return x + x;
  }
  if (hx == 0x3ff00000 && lx == 0)
  {
    return zero; /* log(1) = +0 */
  }
  k += (hx >> 20) - 1023;
  hx &= 0x000fffff;
  i = (hx + 0x95f64) & 0x100000;
  temp.dbl = x;
  temp.as_int.hi = hx | (i ^ 0x3ff00000); /* normalize x or x/2 */
  k += (i >> 20);
  y = (double) k;
  f = temp.dbl - 1.0;
  hfsq = 0.5 * f * f;
  double s, z, R, t1, t2;

  s = f / (2.0 + f);
  z = s * s;
  w = z * z;
  t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
  t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
  R = t2 + t1;
  r = s * (hfsq + R);
  /*
   * f-hfsq must (for args near 1) be evaluated in extra precision
   * to avoid a large cancellation when x is near sqrt(2) or 1/sqrt(2).
   * This is fairly efficient since f-hfsq only depends on f, so can
   * be evaluated in parallel with R.  Not combining hfsq with R also
   * keeps R small (though not as small as a true `lo' term would be),
   * so that extra precision is not needed for terms involving R.
   *
   * Compiler bugs involving extra precision used to break Dekker's
   * theorem for spitting f-hfsq as hi+lo, unless double_t was used
   * or the multi-precision calculations were avoided when double_t
   * has extra precision.  These problems are now automatically
   * avoided as a side effect of the optimization of combining the
   * Dekker splitting step with the clear-low-bits step.
   *
   * y must (for args near sqrt(2) and 1/sqrt(2)) be added in extra
   * precision to avoid a very large cancellation when x is very near
   * these values.  Unlike the above cancellations, this problem is
   * specific to base 2.  It is strange that adding +-1 is so much
   * harder than adding +-ln2 or +-log10_2.
   *
   * This uses Dekker's theorem to normalize y+val_hi, so the
   * compiler bugs are back in some configurations, sigh.  And I
   * don't want to used double_t to avoid them, since that gives a
   * pessimization and the support for avoiding the pessimization
   * is not yet available.
   *
   * The multi-precision calculations for the multiplications are
   * routine.
   */
  hi = f - hfsq;
  temp.dbl = hi;
  temp.as_int.lo = 0;

  lo = (f - hi) - hfsq + r;
  val_hi = hi * ivln2hi;
  val_lo = (lo + hi) * ivln2lo + lo * ivln2hi;

  /* spadd(val_hi, val_lo, y), except for not using double_t: */
  w = y + val_hi;
  val_lo += (y - w) + val_hi;
  val_hi = w;

  return val_lo + val_hi;
} /* log2 */

#undef zero
#undef two54
#undef ivln2hi
#undef ivln2lo
#undef Lg1
#undef Lg2
#undef Lg3
#undef Lg4
#undef Lg5
#undef Lg6
#undef Lg7

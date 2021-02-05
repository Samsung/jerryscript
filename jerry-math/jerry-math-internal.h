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
 *     @(#)fdlibm.h 1.5 04/04/22
 */

#ifndef JERRY_MATH_INTERNAL_H
#define JERRY_MATH_INTERNAL_H

/* Sometimes it's necessary to define __LITTLE_ENDIAN explicitly
   but these catch some common cases. */

#ifndef __LITTLE_ENDIAN
/* Check if compiler has byte order macro. Some older versions do not.
 * If byte order is supported and set to little or target is among common
 * cases checked define __LITTLE_ENDIAN.
 */
#if (defined(i386) || defined(__i386) || defined(__i386__) || defined(i486) || defined(__i486) || defined(__i486__)  \
     || defined(intel) || defined(x86) || defined(i86pc) || defined(_M_IX86) || defined(_M_AMD64) || defined(_M_X64) \
     || defined(__alpha) || defined(__osf__) || defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)      \
     || defined(_M_ARM) || defined(_M_ARM64) || defined(__xtensa__) || defined(__MIPSEL))                            \
  || (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
#define __LITTLE_ENDIAN
#endif
#endif /* !__LITTLE_ENDIAN */

#ifdef __LITTLE_ENDIAN
#define __HI(x) *(1 + (const int *) &x)
#define __LO(x) *(const int *) &x
typedef union
{
  double dbl;
  struct
  {
    int lo;
    int hi;
  } as_int;
} double_accessor;
#else /* !__LITTLE_ENDIAN */
#define __HI(x) *(const int *) &x
#define __LO(x) *(1 + (const int *) &x)

typedef union
{
  double dbl;
  struct
  {
    int hi;
    int lo;
  } as_int;
} double_accessor;
#endif /* __LITTLE_ENDIAN */

#ifndef NAN
#ifdef _MSC_VER
#define INFINITY ((float) (1e+300 * 1e+300)) /* 1e+300*1e+300 must overflow */
#define NAN      ((float) (INFINITY * 0.0f))
#else /* !_MSC_VER */
#define INFINITY ((float) (1.0 / 0.0))
#define NAN      ((float) (0.0 / 0.0))
#endif /* _MSC_VER */
#endif /* !NAN */

/*
 * ANSI/POSIX
 */
double acos (double x);
double asin (double x);
double atan (double x);
double atan2 (double y, double x);
double cos (double x);
double sin (double x);
double tan (double x);

double cosh (double x);
double sinh (double x);
double tanh (double x);

double acosh (double x);
double asinh (double x);
double atanh (double x);

double exp (double x);
double expm1 (double x);
double log (double x);
double log1p (double x);
double log2 (double x);
double log10 (double);

double pow (double x, double y);
double sqrt (double x);
double cbrt (double);

double ceil (double x);
double fabs (double x);
double floor (double x);
double fmod (double x, double y);

double nextafter (double x, double y);

/*
 * Functions callable from C, intended to support IEEE arithmetic.
 */
double copysign (double x, double y);
double scalbn (double x, int n);

#endif /* !JERRY_MATH_INTERNAL_H */

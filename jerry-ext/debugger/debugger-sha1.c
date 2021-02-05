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
 */

/*
 *  FIPS-180-1 compliant SHA-1 implementation
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

/*
 *  The SHA-1 standard was published by NIST in 1993.
 *
 *  http://www.itl.nist.gov/fipspubs/fip180-1.htm
 */

#include "debugger-sha1.h"
#include "jext-common.h"

#if defined(JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1)

/**
 * SHA-1 context structure.
 */
typedef struct
{
  uint32_t total[2]; /**< number of bytes processed */
  uint32_t state[5]; /**< intermediate digest state */
  uint8_t buffer[64]; /**< data block being processed */
} jerryx_sha1_context;

/* 32-bit integer manipulation macros (big endian). */

#define JERRYX_SHA1_GET_UINT32_BE(n, b, i)                                                                         \
  {                                                                                                                \
    (n) = (((uint32_t) (b)[(i) + 0]) << 24) | (((uint32_t) (b)[(i) + 1]) << 16) | (((uint32_t) (b)[(i) + 2]) << 8) \
          | ((uint32_t) (b)[(i) + 3]);                                                                             \
  }

#define JERRYX_SHA1_PUT_UINT32_BE(n, b, i) \
  {                                        \
    (b)[(i) + 0] = (uint8_t) ((n) >> 24);  \
    (b)[(i) + 1] = (uint8_t) ((n) >> 16);  \
    (b)[(i) + 2] = (uint8_t) ((n) >> 8);   \
    (b)[(i) + 3] = (uint8_t) ((n));        \
  }

/**
 * Initialize SHA-1 context.
 */
static void
jerryx_sha1_init (jerryx_sha1_context *sha1_context_p) /**< SHA-1 context */
{
  memset (sha1_context_p, 0, sizeof (jerryx_sha1_context));

  sha1_context_p->total[0] = 0;
  sha1_context_p->total[1] = 0;

  sha1_context_p->state[0] = 0x67452301;
  sha1_context_p->state[1] = 0xEFCDAB89;
  sha1_context_p->state[2] = 0x98BADCFE;
  sha1_context_p->state[3] = 0x10325476;
  sha1_context_p->state[4] = 0xC3D2E1F0;
} /* jerryx_sha1_init */

#define JERRYX_SHA1_P(a, b, c, d, e, x)                              \
  do                                                                 \
  {                                                                  \
    e += JERRYX_SHA1_SHIFT (a, 5) + JERRYX_SHA1_F (b, c, d) + K + x; \
    b = JERRYX_SHA1_SHIFT (b, 30);                                   \
  } while (0)

/**
 * Update SHA-1 internal buffer status.
 */
static void
jerryx_sha1_process (jerryx_sha1_context *sha1_context_p, /**< SHA-1 context */
                     const uint8_t data[64]) /**< data buffer */
{
  uint32_t temp, W[16], A, B, C, D, E;

  JERRYX_SHA1_GET_UINT32_BE (W[0], data, 0);
  JERRYX_SHA1_GET_UINT32_BE (W[1], data, 4);
  JERRYX_SHA1_GET_UINT32_BE (W[2], data, 8);
  JERRYX_SHA1_GET_UINT32_BE (W[3], data, 12);
  JERRYX_SHA1_GET_UINT32_BE (W[4], data, 16);
  JERRYX_SHA1_GET_UINT32_BE (W[5], data, 20);
  JERRYX_SHA1_GET_UINT32_BE (W[6], data, 24);
  JERRYX_SHA1_GET_UINT32_BE (W[7], data, 28);
  JERRYX_SHA1_GET_UINT32_BE (W[8], data, 32);
  JERRYX_SHA1_GET_UINT32_BE (W[9], data, 36);
  JERRYX_SHA1_GET_UINT32_BE (W[10], data, 40);
  JERRYX_SHA1_GET_UINT32_BE (W[11], data, 44);
  JERRYX_SHA1_GET_UINT32_BE (W[12], data, 48);
  JERRYX_SHA1_GET_UINT32_BE (W[13], data, 52);
  JERRYX_SHA1_GET_UINT32_BE (W[14], data, 56);
  JERRYX_SHA1_GET_UINT32_BE (W[15], data, 60);

#define JERRYX_SHA1_SHIFT(x, n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define JERRYX_SHA1_R(t)                                                            \
  (temp = W[(t - 3) & 0x0F] ^ W[(t - 8) & 0x0F] ^ W[(t - 14) & 0x0F] ^ W[t & 0x0F], \
   W[t & 0x0F] = JERRYX_SHA1_SHIFT (temp, 1))

  A = sha1_context_p->state[0];
  B = sha1_context_p->state[1];
  C = sha1_context_p->state[2];
  D = sha1_context_p->state[3];
  E = sha1_context_p->state[4];

  uint32_t K = 0x5A827999;

#define JERRYX_SHA1_F(x, y, z) (z ^ (x & (y ^ z)))

  JERRYX_SHA1_P (A, B, C, D, E, W[0]);
  JERRYX_SHA1_P (E, A, B, C, D, W[1]);
  JERRYX_SHA1_P (D, E, A, B, C, W[2]);
  JERRYX_SHA1_P (C, D, E, A, B, W[3]);
  JERRYX_SHA1_P (B, C, D, E, A, W[4]);
  JERRYX_SHA1_P (A, B, C, D, E, W[5]);
  JERRYX_SHA1_P (E, A, B, C, D, W[6]);
  JERRYX_SHA1_P (D, E, A, B, C, W[7]);
  JERRYX_SHA1_P (C, D, E, A, B, W[8]);
  JERRYX_SHA1_P (B, C, D, E, A, W[9]);
  JERRYX_SHA1_P (A, B, C, D, E, W[10]);
  JERRYX_SHA1_P (E, A, B, C, D, W[11]);
  JERRYX_SHA1_P (D, E, A, B, C, W[12]);
  JERRYX_SHA1_P (C, D, E, A, B, W[13]);
  JERRYX_SHA1_P (B, C, D, E, A, W[14]);
  JERRYX_SHA1_P (A, B, C, D, E, W[15]);
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (16));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (17));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (18));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (19));

#undef JERRYX_SHA1_F

  K = 0x6ED9EBA1;

#define JERRYX_SHA1_F(x, y, z) (x ^ y ^ z)

  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (20));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (21));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (22));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (23));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (24));
  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (25));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (26));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (27));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (28));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (29));
  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (30));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (31));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (32));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (33));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (34));
  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (35));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (36));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (37));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (38));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (39));

#undef JERRYX_SHA1_F

  K = 0x8F1BBCDC;

#define JERRYX_SHA1_F(x, y, z) ((x & y) | (z & (x | y)))

  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (40));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (41));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (42));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (43));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (44));
  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (45));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (46));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (47));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (48));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (49));
  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (50));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (51));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (52));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (53));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (54));
  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (55));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (56));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (57));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (58));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (59));

#undef JERRYX_SHA1_F

  K = 0xCA62C1D6;

#define JERRYX_SHA1_F(x, y, z) (x ^ y ^ z)

  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (60));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (61));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (62));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (63));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (64));
  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (65));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (66));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (67));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (68));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (69));
  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (70));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (71));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (72));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (73));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (74));
  JERRYX_SHA1_P (A, B, C, D, E, JERRYX_SHA1_R (75));
  JERRYX_SHA1_P (E, A, B, C, D, JERRYX_SHA1_R (76));
  JERRYX_SHA1_P (D, E, A, B, C, JERRYX_SHA1_R (77));
  JERRYX_SHA1_P (C, D, E, A, B, JERRYX_SHA1_R (78));
  JERRYX_SHA1_P (B, C, D, E, A, JERRYX_SHA1_R (79));

#undef JERRYX_SHA1_F

  sha1_context_p->state[0] += A;
  sha1_context_p->state[1] += B;
  sha1_context_p->state[2] += C;
  sha1_context_p->state[3] += D;
  sha1_context_p->state[4] += E;

#undef JERRYX_SHA1_SHIFT
#undef JERRYX_SHA1_R
} /* jerryx_sha1_process */

#undef JERRYX_SHA1_P

/**
 * SHA-1 update buffer.
 */
static void
jerryx_sha1_update (jerryx_sha1_context *sha1_context_p, /**< SHA-1 context */
                    const uint8_t *source_p, /**< source buffer */
                    size_t source_length) /**< length of source buffer */
{
  size_t fill;
  uint32_t left;

  if (source_length == 0)
  {
    return;
  }

  left = sha1_context_p->total[0] & 0x3F;
  fill = 64 - left;

  sha1_context_p->total[0] += (uint32_t) source_length;

  /* Check overflow. */
  if (sha1_context_p->total[0] < (uint32_t) source_length)
  {
    sha1_context_p->total[1]++;
  }

  if (left && source_length >= fill)
  {
    memcpy ((void *) (sha1_context_p->buffer + left), source_p, fill);
    jerryx_sha1_process (sha1_context_p, sha1_context_p->buffer);
    source_p += fill;
    source_length -= fill;
    left = 0;
  }

  while (source_length >= 64)
  {
    jerryx_sha1_process (sha1_context_p, source_p);
    source_p += 64;
    source_length -= 64;
  }

  if (source_length > 0)
  {
    memcpy ((void *) (sha1_context_p->buffer + left), source_p, source_length);
  }
} /* jerryx_sha1_update */

/**
 * SHA-1 final digest.
 */
static void
jerryx_sha1_finish (jerryx_sha1_context *sha1_context_p, /**< SHA-1 context */
                    uint8_t destination_p[20]) /**< result */
{
  uint8_t buffer[16];

  uint32_t high = (sha1_context_p->total[0] >> 29) | (sha1_context_p->total[1] << 3);
  uint32_t low = (sha1_context_p->total[0] << 3);

  uint32_t last = sha1_context_p->total[0] & 0x3F;
  uint32_t padn = (last < 56) ? (56 - last) : (120 - last);

  memset (buffer, 0, sizeof (buffer));
  buffer[0] = 0x80;

  while (padn > sizeof (buffer))
  {
    jerryx_sha1_update (sha1_context_p, buffer, sizeof (buffer));
    buffer[0] = 0;
    padn -= (uint32_t) sizeof (buffer);
  }

  jerryx_sha1_update (sha1_context_p, buffer, padn);

  JERRYX_SHA1_PUT_UINT32_BE (high, buffer, 0);
  JERRYX_SHA1_PUT_UINT32_BE (low, buffer, 4);

  jerryx_sha1_update (sha1_context_p, buffer, 8);

  JERRYX_SHA1_PUT_UINT32_BE (sha1_context_p->state[0], destination_p, 0);
  JERRYX_SHA1_PUT_UINT32_BE (sha1_context_p->state[1], destination_p, 4);
  JERRYX_SHA1_PUT_UINT32_BE (sha1_context_p->state[2], destination_p, 8);
  JERRYX_SHA1_PUT_UINT32_BE (sha1_context_p->state[3], destination_p, 12);
  JERRYX_SHA1_PUT_UINT32_BE (sha1_context_p->state[4], destination_p, 16);
} /* jerryx_sha1_finish */

#undef JERRYX_SHA1_GET_UINT32_BE
#undef JERRYX_SHA1_PUT_UINT32_BE

/**
 * Computes the SHA-1 value of the combination of the two input buffers.
 */
void
jerryx_debugger_compute_sha1 (const uint8_t *source1_p, /**< first part of the input */
                              size_t source1_length, /**< length of the first part */
                              const uint8_t *source2_p, /**< second part of the input */
                              size_t source2_length, /**< length of the second part */
                              uint8_t destination_p[20]) /**< result */
{
  jerryx_sha1_context sha1_context;

  jerryx_sha1_init (&sha1_context);
  jerryx_sha1_update (&sha1_context, source1_p, source1_length);
  jerryx_sha1_update (&sha1_context, source2_p, source2_length);
  jerryx_sha1_finish (&sha1_context, destination_p);
} /* jerryx_debugger_compute_sha1 */

#endif /* defined (JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1) */

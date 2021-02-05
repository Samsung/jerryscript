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

#include "ecma-helpers.h"
#include "ecma-init-finalize.h"
#include "lit-strings.h"
#include "test-common.h"

/* Iterations count. */
#define test_iters (1024)

/* Sub iterations count. */
#define test_subiters (128)

/* Max bytes in string. */
#define max_bytes_in_string      (65 * 1024)
#define max_code_units_in_string (max_bytes_in_string)

typedef enum
{
  CESU8_ANY_SIZE,
  CESU8_ONE_BYTE,
  CESU8_TWO_BYTES,
  CESU8_THREE_BYTES,
} utf8_char_size;

static lit_utf8_size_t
generate_cesu8_char (utf8_char_size char_size, lit_utf8_byte_t *buf)
{
  TEST_ASSERT (char_size >= 0 && char_size <= LIT_CESU8_MAX_BYTES_IN_CODE_UNIT);
  lit_code_point_t code_point = (lit_code_point_t) rand ();

  if (char_size == 1)
  {
    code_point %= LIT_UTF8_1_BYTE_CODE_POINT_MAX;
  }
  else if (char_size == 2)
  {
    code_point =
      LIT_UTF8_2_BYTE_CODE_POINT_MIN + code_point % (LIT_UTF8_2_BYTE_CODE_POINT_MAX - LIT_UTF8_2_BYTE_CODE_POINT_MIN);
  }
  else if (char_size == 3)
  {
    code_point =
      LIT_UTF8_3_BYTE_CODE_POINT_MIN + code_point % (LIT_UTF8_3_BYTE_CODE_POINT_MAX - LIT_UTF8_3_BYTE_CODE_POINT_MIN);
  }
  else
  {
    code_point %= LIT_UTF8_3_BYTE_CODE_POINT_MAX;
  }

  if (code_point >= LIT_UTF16_HIGH_SURROGATE_MIN && code_point <= LIT_UTF16_LOW_SURROGATE_MAX)
  {
    code_point = LIT_UTF16_HIGH_SURROGATE_MIN - 1;
  }

  return lit_code_unit_to_utf8 ((ecma_char_t) code_point, buf);
} /* generate_cesu8_char */

static lit_utf8_size_t
generate_cesu8_string (lit_utf8_byte_t *buf_p, lit_utf8_size_t buf_size)
{
  lit_utf8_size_t length = 0;

  lit_utf8_size_t size = 0;
  while (size < buf_size)
  {
    const utf8_char_size char_size =
      (((buf_size - size) > LIT_CESU8_MAX_BYTES_IN_CODE_UNIT) ? CESU8_ANY_SIZE : (utf8_char_size) (buf_size - size));

    lit_utf8_size_t bytes_generated = generate_cesu8_char (char_size, buf_p);

    TEST_ASSERT (lit_is_valid_cesu8_string (buf_p, bytes_generated));

    size += bytes_generated;
    buf_p += bytes_generated;
    length++;
  }

  TEST_ASSERT (size == buf_size);

  return length;
} /* generate_cesu8_string */

int
main (void)
{
  TEST_INIT ();

  jmem_init ();
  ecma_init ();

  lit_utf8_byte_t cesu8_string[max_bytes_in_string];
  ecma_char_t code_units[max_code_units_in_string];
  const lit_utf8_byte_t *saved_positions[max_code_units_in_string];

  for (int i = 0; i < test_iters; i++)
  {
    lit_utf8_size_t cesu8_string_size = (i == 0) ? 0 : (lit_utf8_size_t) (rand () % max_bytes_in_string);
    lit_utf8_size_t length = generate_cesu8_string (cesu8_string, cesu8_string_size);

    ecma_string_t *char_collection_string_p = ecma_new_ecma_string_from_utf8 (cesu8_string, cesu8_string_size);
    lit_utf8_size_t char_collection_len = ecma_string_get_length (char_collection_string_p);
    TEST_ASSERT (char_collection_len == length);
    ecma_deref_ecma_string (char_collection_string_p);

    TEST_ASSERT (lit_utf8_string_length (cesu8_string, cesu8_string_size) == length);

    const lit_utf8_byte_t *curr_p = cesu8_string;
    const lit_utf8_byte_t *end_p = cesu8_string + cesu8_string_size;

    lit_utf8_size_t calculated_length = 0;
    lit_utf8_size_t code_units_count = 0;

    while (curr_p < end_p)
    {
      code_units[code_units_count] = lit_cesu8_peek_next (curr_p);
      saved_positions[code_units_count] = curr_p;
      code_units_count++;
      calculated_length++;

      lit_utf8_incr (&curr_p);
    }

    TEST_ASSERT (length == calculated_length);

    if (code_units_count > 0)
    {
      for (int j = 0; j < test_subiters; j++)
      {
        lit_utf8_size_t index = (lit_utf8_size_t) rand () % code_units_count;
        curr_p = saved_positions[index];
        TEST_ASSERT (lit_cesu8_peek_next (curr_p) == code_units[index]);
      }
    }

    curr_p = (lit_utf8_byte_t *) end_p;
    while (curr_p > cesu8_string)
    {
      TEST_ASSERT (code_units_count > 0);
      calculated_length--;
      TEST_ASSERT (code_units[calculated_length] == lit_cesu8_peek_prev (curr_p));
      lit_utf8_decr (&curr_p);
    }

    TEST_ASSERT (calculated_length == 0);

    while (curr_p < end_p)
    {
      ecma_char_t code_unit = lit_cesu8_read_next (&curr_p);
      TEST_ASSERT (code_unit == code_units[calculated_length]);
      calculated_length++;
    }

    TEST_ASSERT (length == calculated_length);

    while (curr_p > cesu8_string)
    {
      TEST_ASSERT (code_units_count > 0);
      calculated_length--;
      TEST_ASSERT (code_units[calculated_length] == lit_cesu8_read_prev (&curr_p));
    }

    TEST_ASSERT (calculated_length == 0);
  }

  /* Overlong-encoded code point */
  lit_utf8_byte_t invalid_cesu8_string_1[] = { 0xC0, 0x82 };
  TEST_ASSERT (!lit_is_valid_cesu8_string (invalid_cesu8_string_1, sizeof (invalid_cesu8_string_1)));

  /* Overlong-encoded code point */
  lit_utf8_byte_t invalid_cesu8_string_2[] = { 0xE0, 0x80, 0x81 };
  TEST_ASSERT (!lit_is_valid_cesu8_string (invalid_cesu8_string_2, sizeof (invalid_cesu8_string_2)));

  /* Pair of surrogates: 0xD901 0xDFF0 which encode Unicode character 0x507F0 */
  lit_utf8_byte_t invalid_cesu8_string_3[] = { 0xED, 0xA4, 0x81, 0xED, 0xBF, 0xB0 };
  TEST_ASSERT (lit_is_valid_cesu8_string (invalid_cesu8_string_3, sizeof (invalid_cesu8_string_3)));

  /* Isolated high surrogate 0xD901 */
  lit_utf8_byte_t valid_utf8_string_1[] = { 0xED, 0xA4, 0x81 };
  TEST_ASSERT (lit_is_valid_cesu8_string (valid_utf8_string_1, sizeof (valid_utf8_string_1)));

  lit_utf8_byte_t res_buf[3];
  lit_utf8_size_t res_size;

  res_size = lit_code_unit_to_utf8 (0x73, res_buf);
  TEST_ASSERT (res_size == 1);
  TEST_ASSERT (res_buf[0] == 0x73);

  res_size = lit_code_unit_to_utf8 (0x41A, res_buf);
  TEST_ASSERT (res_size == 2);
  TEST_ASSERT (res_buf[0] == 0xD0);
  TEST_ASSERT (res_buf[1] == 0x9A);

  res_size = lit_code_unit_to_utf8 (0xD7FF, res_buf);
  TEST_ASSERT (res_size == 3);
  TEST_ASSERT (res_buf[0] == 0xED);
  TEST_ASSERT (res_buf[1] == 0x9F);
  TEST_ASSERT (res_buf[2] == 0xBF);

  ecma_finalize ();
  jmem_finalize ();

  return 0;
} /* main */

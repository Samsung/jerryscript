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
#include "ecma-line-info.h"

#if JERRY_LINE_INFO

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmalineinfo Line info
 * @{
 */

/* The layout of the structure is defined in js-parser-line-info-create.c */

JERRY_STATIC_ASSERT ((ECMA_LINE_INFO_COLUMN_DEFAULT - 1) == ((ECMA_LINE_INFO_ENCODE_TWO_BYTE >> 1) - 1),
                     ecma_line_info_column_1_must_be_accessible_with_the_highest_one_byte_negative_value);

/**
 * Decodes an uint32_t number, and updates the buffer position.
 * Numbers expected to be larger values.
 *
 * @return the decoded value
 */
uint32_t
ecma_line_info_decode (uint8_t **buffer_p) /**< [in/out] target buffer */
{
  uint8_t *source_p = *buffer_p;
  uint32_t value = 0;

  do
  {
    value = (value << ECMA_LINE_INFO_NUMBER_SHIFT) | (*source_p >> 1);
  }
  while (!(*source_p++ & ECMA_LINE_INFO_NUMBER_TERMINATOR));

  *buffer_p = source_p;
  return value;
} /* ecma_line_info_decode */

/**
 * Decodes an uint32_t number, and updates the buffer position.
 * Numbers expected to be smaller values.
 *
 * @return the decoded value
 */
static uint32_t
ecma_line_info_decode_small (uint8_t **buffer_p) /**< [in/out] target buffer */
{
  uint8_t *source_p = *buffer_p;

  uint32_t type = source_p[0];

  if (type < ECMA_LINE_INFO_ENCODE_TWO_BYTE_MIN)
  {
    *buffer_p = source_p + 1;
    return type;
  }

  uint32_t value = source_p[1];

  if (type == ECMA_LINE_INFO_ENCODE_TWO_BYTE)
  {
    *buffer_p = source_p + 2;
    return (uint32_t) (value + ECMA_LINE_INFO_ENCODE_TWO_BYTE_MIN);
  }

  value |= ((uint32_t) source_p[2]) << 8;

  if (type == ECMA_LINE_INFO_ENCODE_THREE_BYTE)
  {
    *buffer_p = source_p + 3;
    return (uint32_t) (value + ECMA_LINE_INFO_ENCODE_THREE_BYTE_MIN);
  }

  JERRY_ASSERT (type == ECMA_LINE_INFO_ENCODE_FIVE_BYTE);

  *buffer_p = source_p + 5;
  return value | (((uint32_t) source_p[3]) << 8) | (((uint32_t) source_p[4]) << 8);
} /* ecma_line_info_decode_small */

/**
 * Updates a value using an encoded difference.
 *
 * @return updated value
 */
extern inline uint32_t JERRY_ATTR_ALWAYS_INLINE
ecma_line_info_difference_update (uint32_t current_value, /**< current value */
                                  uint32_t difference_value) /**< encoded difference */
{
  if ((difference_value & 0x1) == ECMA_LINE_INFO_INCREASE)
  {
    return current_value + (difference_value >> 1) + 1;
  }

  return current_value - (difference_value >> 1);
} /* ecma_line_info_difference_update */

/**
 * Release line info data.
 */
void
ecma_line_info_free (uint8_t *line_info_p) /**< line info buffer */
{
  uint8_t *source_p = line_info_p;
  uint32_t total_length = ecma_line_info_decode (&source_p);

  jmem_heap_free_block (line_info_p, total_length + (uint32_t) (source_p - line_info_p));
} /* ecma_line_info_free */

/**
 * Returns the line/column information for a given byte code offset.
 */
void
ecma_line_info_get (uint8_t *line_info_p, /**< line info buffer */
                    uint32_t offset, /**< byte code offset */
                    jerry_backtrace_location_t *location_p) /**< [out] location */
{
  uint32_t line = 1;
  uint32_t column = ECMA_LINE_INFO_COLUMN_DEFAULT;
  uint32_t end_offset = 0;
  uint32_t end_offset_increase;
  uint32_t value;

  /* Skip total_length. */
  ecma_line_info_decode (&line_info_p);

  while (true)
  {
    value = ecma_line_info_decode (&line_info_p);
    line = ecma_line_info_difference_update (line, value);

    if (*line_info_p == 0)
    {
      break;
    }

    uint8_t *size_p = line_info_p + *line_info_p + (ECMA_LINE_INFO_STREAM_SIZE_MIN + 1);

    end_offset += ecma_line_info_decode (&size_p);

    if (offset < end_offset)
    {
      break;
    }

    line_info_p = size_p;
  }

  line_info_p++;

  do
  {
    end_offset_increase = ecma_line_info_decode_small (&line_info_p);

    if (end_offset_increase & ECMA_LINE_INFO_HAS_LINE)
    {
      value = ecma_line_info_decode_small (&line_info_p);
      line = ecma_line_info_difference_update (line, value);
      column = ECMA_LINE_INFO_COLUMN_DEFAULT;
    }

    end_offset_increase >>= 1;

    value = ecma_line_info_decode_small (&line_info_p);
    column = ecma_line_info_difference_update (column, value);

    end_offset += end_offset_increase;
  }
  while (end_offset_increase != 0 && end_offset <= offset);

  location_p->line = line;
  location_p->column = column;
} /* ecma_line_info_get */

#if JERRY_PARSER_DUMP_BYTE_CODE

/**
 * Dumps line info data.
 */
void
ecma_line_info_dump (uint8_t *line_info_p) /**< dumps line info data */
{
  bool block_last = false;
  uint32_t block_line = 1;
  uint32_t block_byte_code_offset = 0;
  uint32_t value;

  value = ecma_line_info_decode (&line_info_p);
  JERRY_DEBUG_MSG ("\nLine info size: %d bytes\n", (int) value);

  while (true)
  {
    value = ecma_line_info_decode (&line_info_p);
    block_line = ecma_line_info_difference_update (block_line, value);

    JERRY_DEBUG_MSG ("\nNew block: line: %d", (int) block_line);

    if (*line_info_p == 0)
    {
      JERRY_DEBUG_MSG (" StreamLength: [last]\n");
      block_last = true;
    }
    else
    {
      uint8_t *size_p = line_info_p + *line_info_p + (ECMA_LINE_INFO_STREAM_SIZE_MIN + 1);

      value = ecma_line_info_decode (&size_p);

      JERRY_DEBUG_MSG (" StreamLength: %d ByteCodeSize: %d\n",
                       (int) (*line_info_p + ECMA_LINE_INFO_STREAM_SIZE_MIN),
                       (int) value);
    }

    line_info_p++;

    uint32_t stream_line = block_line;
    uint32_t stream_column = ECMA_LINE_INFO_COLUMN_DEFAULT;
    uint32_t stream_end_offset = block_byte_code_offset;

    while (true)
    {
      uint32_t stream_end_offset_increase = ecma_line_info_decode_small (&line_info_p);

      if (stream_end_offset_increase & ECMA_LINE_INFO_HAS_LINE)
      {
        value = ecma_line_info_decode_small (&line_info_p);
        stream_line = ecma_line_info_difference_update (stream_line, value);
        stream_column = ECMA_LINE_INFO_COLUMN_DEFAULT;
      }

      stream_end_offset_increase >>= 1;

      value = ecma_line_info_decode_small (&line_info_p);
      stream_column = ecma_line_info_difference_update (stream_column, value);

      if (stream_end_offset_increase == 0)
      {
        JERRY_DEBUG_MSG ("  ByteCodeEndOffset: [unterminated] Line: %d Column: %d\n",
                         (int) stream_line,
                         (int) stream_column);
        break;
      }

      stream_end_offset += stream_end_offset_increase;

      JERRY_DEBUG_MSG ("  ByteCodeEndOffset: %d Line: %d Column: %d\n",
                       (int) stream_end_offset,
                       (int) stream_line,
                       (int) stream_column);
    }

    if (block_last)
    {
      break;
    }

    block_byte_code_offset += ecma_line_info_decode (&line_info_p);
  }
} /* ecma_line_info_dump */

#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

#endif /* JERRY_LINE_INFO */

/**
 * @}
 * @}
 */

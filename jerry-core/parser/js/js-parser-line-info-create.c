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

#include "ecma-line-info.h"
#include "js-parser-internal.h"

#if JERRY_PARSER

#if JERRY_LINE_INFO

/* \addtogroup parser Parser
 * @{
 *
 * \addtogroup jsparser JavaScript
 * @{
 *
 * \addtogroup jsparser_line_info_create Create line info data
 * @{
 */

/*
 * The line-info data structure uses two number encodings:
 *
 * Default:
 *    Each byte contains 7 bit data and 1 bit for continuation.
 *    Can encode signed values between 63 and -64 in 1 byte.
 *
 * Small:
 *    First byte can encode signed values between 125 and -125 in 1 byte.
 *    Larger values requires at least 3 bytes.
 *
 * The line-info data structure is a sequence of chunks:
 *
 *  +------+--------------+------------+----------------+
 *  | Line | StreamLength | StreamData | [ByteCodeSize] |
 *  +------+--------------+------------+----------------+
 *
 *  Line [Default encoding]:
 *      Specifies the start line of this chunk, relative to its previous value.
 *      The starting column is always ECMA_LINE_INFO_COLUMN_DEFAULT
 *
 *  StreamLength [uint8_t]:
 *      Length of the StreamData in bytes minus ECMA_LINE_INFO_STREAM_SIZE_MIN.
 *      The 0 value represents the last chunk, which size is not specified
 *      (Can be less than ECMA_LINE_INFO_STREAM_SIZE_MIN).
 *
 *  StreamData [sequence of bytes]:
 *      Sequence of the following items:
 *
 *      +-----------+--------+--------+
 *      | EndOffset | [Line] | Column |
 *      +-----------+--------+--------+
 *
 *      EndOffset [Small encoding]:
 *          Specifies the EndOffset in the byte code, relative to the previous EndOffset.
 *          The range of byte codes corresponding to the line/column position of this item
 *          is between the EndOffset of the previous item (inclusive) and the EndOffset
 *          of this item (exclusive). The last end offset of a stream is always 0, which
 *          represents an unterminated range.
 *
 *      Line [Small encoding] [Optional]:
 *          If bit 1 of end offset is set, this specifies the line position of this item,
 *          relative to the previous line position, and the column position is set to
 *          ECMA_LINE_INFO_COLUMN_DEFAULT.
 *
 *      Column [Small encoding]:
 *          Specifies the current column position relative to the previous column position.
 *
 *  ByteCodeSize [Default encoding] [Optional]:
 *      If StreamLength is not 0, this specifies the byte code size of the whole range.
 *      This value can be used to skip the byte codes which line info is stored
 *      in this chunk. This information is not available for the last chunk.
 */

/**
 * Maximum number of bytes requires to encode a number.
 */
#define PARSER_LINE_INFO_BUFFER_MAX_SIZE 5

/**
 * Stream generation ends after this size is reached,
 * since there might be not enough place for the next item.
 */
#define PARSER_LINE_INFO_STREAM_SIZE_LIMIT \
  (ECMA_LINE_INFO_STREAM_SIZE_MIN + UINT8_MAX - ((2 * PARSER_LINE_INFO_BUFFER_MAX_SIZE) + 1))

/**
 * Free line info temporary data collected during parsing.
 */
void
parser_line_info_free (parser_line_info_data_t *line_info_p)
{
  parser_mem_page_t *current_page_p = line_info_p->first_page_p;

  while (current_page_p != NULL)
  {
    parser_mem_page_t *next_p = current_page_p->next_p;

    parser_free (current_page_p, sizeof (parser_mem_page_t *) + PARSER_STACK_PAGE_SIZE);
    current_page_p = next_p;
  }
} /* parser_line_info_free */

/**
 * Encodes an uint32_t number into a buffer. Numbers expected to be larger values.
 *
 * @return the number of bytes written to the buffer
 */
static uint32_t
parser_line_info_encode (uint8_t *buffer_p, /**< target buffer */
                         uint32_t value) /**< encoded value */
{
  const uint32_t max_value = UINT8_MAX >> 1;

  if (value <= max_value)
  {
    *buffer_p = (uint8_t) ((value << 1) | ECMA_LINE_INFO_NUMBER_TERMINATOR);
    return 1;
  }

  uint32_t extra_length = 0;
  uint32_t shifted_value = value;

  do
  {
    shifted_value >>= ECMA_LINE_INFO_NUMBER_SHIFT;
    extra_length++;
  }
  while (shifted_value > max_value);

  *buffer_p++ = (uint8_t) (shifted_value << 1);

  uint32_t shift = extra_length * ECMA_LINE_INFO_NUMBER_SHIFT;

  do
  {
    shift -= ECMA_LINE_INFO_NUMBER_SHIFT;
    *buffer_p++ = (uint8_t) ((value >> shift) << 1);
  }
  while (shift > 0);

  buffer_p[-1] |= ECMA_LINE_INFO_NUMBER_TERMINATOR;
  return extra_length + 1;
} /* parser_line_info_encode */

/**
 * Encodes an uint32_t number into a buffer. Numbers expected to be smaller values.
 *
 * @return the number of bytes written to the buffer
 */
static uint32_t
parser_line_info_encode_small (uint8_t *buffer_p, /**< target buffer */
                               uint32_t value) /**< encoded value */
{
  if (value < ECMA_LINE_INFO_ENCODE_TWO_BYTE_MIN)
  {
    buffer_p[0] = (uint8_t) value;
    return 1;
  }

  uint32_t length;

  if (JERRY_LIKELY (value < ECMA_LINE_INFO_ENCODE_THREE_BYTE_MIN))
  {
    value -= ECMA_LINE_INFO_ENCODE_TWO_BYTE_MIN;
    buffer_p[0] = ECMA_LINE_INFO_ENCODE_TWO_BYTE;
    length = 2;
  }
  else
  {
    if (value <= (ECMA_LINE_INFO_ENCODE_THREE_BYTE_MIN + UINT16_MAX))
    {
      value -= ECMA_LINE_INFO_ENCODE_THREE_BYTE_MIN;
      buffer_p[0] = ECMA_LINE_INFO_ENCODE_THREE_BYTE;
      length = 3;
    }
    else
    {
      buffer_p[0] = ECMA_LINE_INFO_ENCODE_FIVE_BYTE;
      buffer_p[3] = (uint8_t) (value >> 16);
      buffer_p[4] = (uint8_t) (value >> 24);
      length = 5;
    }

    buffer_p[2] = (uint8_t) (value >> 8);
  }

  buffer_p[1] = (uint8_t) value;
  return length;
} /* parser_line_info_encode_small */

/**
 * Encodes the difference between two values.
 *
 * @return encoded difference
 */
static inline uint32_t JERRY_ATTR_ALWAYS_INLINE
parser_line_info_difference_get (uint32_t current_value, /**< current value */
                                 uint32_t prev_value) /**< previous value */
{
  uint32_t result = current_value - prev_value - 1;

  if (result <= (UINT32_MAX >> 1))
  {
    return (result << 1) | ECMA_LINE_INFO_INCREASE;
  }

  return ((UINT32_MAX - result) << 1) | ECMA_LINE_INFO_DECREASE;
} /* parser_line_info_difference_get */

/**
 * Appends a value at the end of the line info stream.
 */
static void
parser_line_info_append_number (parser_context_t *context_p, /**< context */
                                uint32_t value) /**< value to be encoded */
{
  uint8_t buffer[PARSER_LINE_INFO_BUFFER_MAX_SIZE];

  uint32_t length = parser_line_info_encode (buffer, value);

  if (context_p->line_info.last_page_p != NULL
      && context_p->line_info.last_page_p->bytes[0] + length <= PARSER_STACK_PAGE_SIZE)
  {
    memcpy (context_p->line_info.last_page_p->bytes + context_p->line_info.last_page_p->bytes[0],
            buffer,
            length);

    length += context_p->line_info.last_page_p->bytes[0];
    context_p->line_info.last_page_p->bytes[0] = (uint8_t) length;
    return;
  }

  size_t size = sizeof (parser_mem_page_t *) + PARSER_STACK_PAGE_SIZE;
  parser_mem_page_t *new_page_p = (parser_mem_page_t *) parser_malloc (context_p, size);

  new_page_p->next_p = NULL;

  if (context_p->line_info.first_page_p == NULL)
  {
    context_p->line_info.first_page_p = new_page_p;
    context_p->line_info.last_page_p = new_page_p;
  }
  else
  {
    context_p->line_info.last_page_p->next_p = new_page_p;
    context_p->line_info.last_page_p = new_page_p;
  }

  context_p->line_info.last_page_p->bytes[0] = (uint8_t) (length + 1);
  memcpy (new_page_p->bytes + 1, buffer, length);
} /* parser_line_info_append_number */

/**
 * Updates the current line information data.
 */
void
parser_line_info_append (parser_context_t *context_p, /**< context */
                         parser_line_counter_t line, /**< line */
                         parser_line_counter_t column) /**< column */
{
  if (context_p->line_info.first_page_p != NULL
      && (context_p->byte_code_size == context_p->line_info.byte_code_position
          || (context_p->line_info.line == line
              && context_p->line_info.column == column)))
  {
    return;
  }

  uint32_t value = (context_p->byte_code_size - context_p->line_info.byte_code_position) << 1;
  value |= (context_p->line_info.line != line);

  parser_line_info_append_number (context_p, value);
  context_p->line_info.byte_code_position = context_p->byte_code_size;

  if (value & 0x1)
  {
    value = parser_line_info_difference_get (line, context_p->line_info.line);
    parser_line_info_append_number (context_p, value);
    context_p->line_info.line = line;
  }

  value = parser_line_info_difference_get (column, context_p->line_info.column);
  parser_line_info_append_number (context_p, value);
  context_p->line_info.column = column;
} /* parser_line_info_append */

/**
 * Line info iterator structure
 */
typedef struct
{
  parser_mem_page_t *current_page_p; /**< current page */
  uint32_t offset; /**< current offset */
} parser_line_info_iterator_t;

/**
 * Decodes the next value from the iterator stream
 */
static uint32_t
parser_line_info_iterator_get (parser_line_info_iterator_t *iterator_p) /**< iterator */
{
  uint8_t *source_p = iterator_p->current_page_p->bytes + iterator_p->offset;
  uint32_t result = ecma_line_info_decode (&source_p);

  iterator_p->offset = (uint32_t) (source_p - iterator_p->current_page_p->bytes);

  JERRY_ASSERT (iterator_p->offset <= iterator_p->current_page_p->bytes[0]);

  if (iterator_p->offset < iterator_p->current_page_p->bytes[0])
  {
    return result;
  }

  iterator_p->current_page_p = iterator_p->current_page_p->next_p;
  iterator_p->offset = 1;
  return result;
} /* parser_line_info_iterator_get */

/**
 * Generate line info data
 *
 * @return generated line info data
 */
uint8_t *
parser_line_info_generate (parser_context_t *context_p) /**< context */
{
  parser_line_info_iterator_t iterator;
  uint8_t *line_info_p = NULL;
  uint8_t *dst_p = NULL;
  uint32_t total_length = 0;
  uint32_t total_length_size = 0;

  while (true)
  {
    /* The following code runs twice: first the size of the data,
     * is computed and the data is generated during the second run.
     * Note: line_info_p is NULL during the first run. */
    parser_mem_page_t *iterator_byte_code_page_p = context_p->byte_code.first_p;
    uint32_t iterator_byte_code_page_offset = 0;
    uint32_t iterator_byte_code_base = 0;
    uint32_t iterator_last_byte_code_offset = UINT32_MAX;
    uint32_t iterator_prev_line = 0;
    uint32_t iterator_prev_column = 0;
    uint32_t iterator_line = 1;
    uint32_t iterator_column = 1;
    uint8_t block_buffer[PARSER_LINE_INFO_BUFFER_MAX_SIZE];
    uint8_t line_column_buffer[PARSER_LINE_INFO_BUFFER_MAX_SIZE * 2];
    uint8_t *block_size_p = NULL;
    uint32_t block_byte_code_offset = 0;
    uint32_t block_prev_line = 1;
    uint32_t stream_byte_code_offset = 0;
    uint32_t stream_current_line = 1;
    uint32_t stream_current_column = ECMA_LINE_INFO_COLUMN_DEFAULT;
    uint32_t stream_prev_line = 1;
    uint32_t stream_prev_column = ECMA_LINE_INFO_COLUMN_DEFAULT;
    uint32_t stream_size = 0;
    uint32_t stream_value_count = 0;
    uint32_t value;

    iterator.current_page_p = context_p->line_info.first_page_p;
    iterator.offset = 1;

    do
    {
      /* Decode line information generated during parsing. */
      value = parser_line_info_iterator_get (&iterator);
      iterator_byte_code_page_offset += (value >> 1);

      if (value & 0x1)
      {
        value = parser_line_info_iterator_get (&iterator);
        JERRY_ASSERT (value != ((0 << 1) | ECMA_LINE_INFO_DECREASE));
        iterator_line = ecma_line_info_difference_update (iterator_line, value);
      }

      value = parser_line_info_iterator_get (&iterator);
      iterator_column = ecma_line_info_difference_update (iterator_column, value);

      while (iterator_byte_code_page_offset >= PARSER_CBC_STREAM_PAGE_SIZE)
      {
        iterator_byte_code_base += iterator_byte_code_page_p->bytes[PARSER_CBC_STREAM_PAGE_SIZE - 1];
        iterator_byte_code_page_offset -= PARSER_CBC_STREAM_PAGE_SIZE;
        iterator_byte_code_page_p = iterator_byte_code_page_p->next_p;
      }

      uint32_t iterator_byte_code_offset = iterator_byte_code_base;

      if (iterator_byte_code_page_offset > 0)
      {
        uint8_t relative_offset = iterator_byte_code_page_p->bytes[iterator_byte_code_page_offset - 1];
        iterator_byte_code_offset += relative_offset & CBC_LOWER_SEVEN_BIT_MASK;
      }

      /* Skip those line/column pairs which byte code was discarded during post processing
       * or does not change line/column (this is possible when multiple skips occures). */
      if (iterator_byte_code_offset == iterator_last_byte_code_offset
          || (iterator_line == iterator_prev_line && iterator_column == iterator_prev_column))
      {
        continue;
      }

      iterator_prev_line = iterator_line;
      iterator_prev_column = iterator_column;
      iterator_last_byte_code_offset = iterator_byte_code_offset;

      if (block_size_p != NULL)
      {
        value = ((uint32_t) (stream_prev_line != stream_current_line)
                 | ((iterator_byte_code_offset - stream_byte_code_offset) << 1));

        uint32_t line_column_size = 0;
        uint32_t offset_size = parser_line_info_encode_small (block_buffer, value);
        stream_byte_code_offset = iterator_byte_code_offset;

        if (value & 0x1)
        {
          value = parser_line_info_difference_get (stream_current_line, stream_prev_line);
          line_column_size = parser_line_info_encode_small (line_column_buffer, value);
          stream_prev_line = stream_current_line;
          stream_prev_column = ECMA_LINE_INFO_COLUMN_DEFAULT;
        }

        value = parser_line_info_difference_get (stream_current_column, stream_prev_column);
        line_column_size += parser_line_info_encode_small (line_column_buffer + line_column_size, value);

        stream_prev_column = stream_current_column;
        stream_current_line = iterator_line;
        stream_current_column = iterator_column;

        stream_value_count++;

        if (stream_value_count < ECMA_LINE_INFO_STREAM_VALUE_COUNT_MAX
            && (stream_size + offset_size + line_column_size <= PARSER_LINE_INFO_STREAM_SIZE_LIMIT))
        {
          stream_size += offset_size + line_column_size;

          if (line_info_p != NULL)
          {
            memcpy (dst_p, block_buffer, offset_size);
            dst_p += offset_size;
            memcpy (dst_p, line_column_buffer, line_column_size);
            dst_p += line_column_size;
          }
          continue;
        }

        /* Finalize the current chunk. The size of EndOffset is always 1. */
        stream_size += 1 + line_column_size;

        JERRY_ASSERT (stream_size > ECMA_LINE_INFO_STREAM_SIZE_MIN
                      && (stream_size - ECMA_LINE_INFO_STREAM_SIZE_MIN) <= UINT8_MAX);

        if (line_info_p != NULL)
        {
          *block_size_p = (uint8_t) (stream_size - ECMA_LINE_INFO_STREAM_SIZE_MIN);
          /* Set EndOffset to 0 and copy the has_line bit. */
          *dst_p++ = (uint8_t) (block_buffer[0] & ECMA_LINE_INFO_HAS_LINE);
          memcpy (dst_p, line_column_buffer, line_column_size);
          dst_p += line_column_size;
        }
        else
        {
          total_length += stream_size;
          dst_p = block_buffer;
        }

        uint32_t byte_code_diff = iterator_last_byte_code_offset - block_byte_code_offset;
        dst_p += parser_line_info_encode (dst_p, byte_code_diff);
        block_byte_code_offset = iterator_last_byte_code_offset;

        if (line_info_p == NULL)
        {
          total_length += (uint32_t) (dst_p - block_buffer);
        }
      }

      /* Start a new chunk. */
      if (line_info_p == NULL)
      {
        dst_p = block_buffer;
      }

      value = parser_line_info_difference_get (iterator_line, block_prev_line);

      dst_p += parser_line_info_encode (dst_p, value);
      block_size_p = dst_p;
      dst_p++;

      if (line_info_p == NULL)
      {
        total_length += (uint32_t) (dst_p - block_buffer);
      }

      block_prev_line = iterator_line;
      stream_current_line = iterator_line;
      stream_current_column = iterator_column;
      stream_prev_line = iterator_line;
      stream_prev_column = ECMA_LINE_INFO_COLUMN_DEFAULT;
      stream_size = 0;
      stream_value_count = 0;
    }
    while (iterator.current_page_p != NULL);

    value = (stream_prev_line != stream_current_line);

    /* Finalize the last stream */
    if (line_info_p == NULL)
    {
      dst_p = line_column_buffer;
      total_length += stream_size + 1;
    }
    else
    {
      *block_size_p = 0;
      /* Small encoded value of has_line bit. */
      *dst_p++ = (uint8_t) value;
    }

    if (value)
    {
      value = parser_line_info_difference_get (stream_current_line, stream_prev_line);
      dst_p += parser_line_info_encode_small (dst_p, value);
      stream_prev_column = ECMA_LINE_INFO_COLUMN_DEFAULT;
    }

    value = parser_line_info_difference_get (stream_current_column, stream_prev_column);
    dst_p += parser_line_info_encode_small (dst_p, value);

    if (line_info_p == NULL)
    {
      total_length += (uint32_t) (dst_p - line_column_buffer);
    }

    if (line_info_p != NULL)
    {
      break;
    }

    total_length_size = parser_line_info_encode (block_buffer, total_length);

    /* TODO: Support allocation fail. */
    line_info_p = (uint8_t *) jmem_heap_alloc_block (total_length + total_length_size);
    dst_p = line_info_p + parser_line_info_encode (line_info_p, total_length);
  }

  JERRY_ASSERT (line_info_p + total_length_size + total_length == dst_p);

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
    ecma_line_info_dump (line_info_p);
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  return line_info_p;
} /* parser_line_info_generate */

#endif /* JERRY_LINE_INFO */

#endif /* JERRY_PARSER */

/**
 * @}
 * @}
 * @}
 */

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

#ifndef ECMA_LINE_INFO_H
#define ECMA_LINE_INFO_H

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmalineinfo Line info
 * @{
 */

#if JERRY_LINE_INFO

#include "ecma-globals.h"

/**
 * Increase the current value of line or column.
 */
#define ECMA_LINE_INFO_INCREASE 0x0

/**
 * Decrease the current value of line or column.
 */
#define ECMA_LINE_INFO_DECREASE 0x1

/**
 * Line update is present.
 */
#define ECMA_LINE_INFO_HAS_LINE 0x1

/**
 * A default value for columns after a line update.
 */
#define ECMA_LINE_INFO_COLUMN_DEFAULT 126

/**
 * Default encoding: flag which is set for the last byte of an encoded number.
 */
#define ECMA_LINE_INFO_NUMBER_TERMINATOR 0x1

/**
 * Default encoding: size in bits of the number part stored in a byte.
 */
#define ECMA_LINE_INFO_NUMBER_SHIFT 7

/**
 * Small encoding: a value which represents a two byte long number.
 */
#define ECMA_LINE_INFO_ENCODE_TWO_BYTE (UINT8_MAX - 2)

/**
 * Small encoding: minimum value of an encoded two byte long number.
 */
#define ECMA_LINE_INFO_ENCODE_TWO_BYTE_MIN (UINT8_MAX - 2)

/**
 * Small encoding: a value which represents a three byte long number.
 */
#define ECMA_LINE_INFO_ENCODE_THREE_BYTE (UINT8_MAX - 1)

/**
 * Small encoding: minimum value of an encoded three byte long number.
 */
#define ECMA_LINE_INFO_ENCODE_THREE_BYTE_MIN (ECMA_LINE_INFO_ENCODE_TWO_BYTE_MIN + UINT8_MAX + 1)

/**
 * Small encoding: a value which represents a five byte long number.
 */
#define ECMA_LINE_INFO_ENCODE_FIVE_BYTE UINT8_MAX

/**
 * Maximum number of line/column entries stored in a stream.
 */
#define ECMA_LINE_INFO_STREAM_VALUE_COUNT_MAX 48

/**
 * Minimum maximum size of a stream (except the last one).
 */
#define ECMA_LINE_INFO_STREAM_SIZE_MIN \
  ((2 * ECMA_LINE_INFO_STREAM_VALUE_COUNT_MAX) - 1)

/* Helper functions for parser/js/js-parser-line-info-create.c. */
uint32_t ecma_line_info_decode (uint8_t **buffer_p);
uint32_t ecma_line_info_difference_update (uint32_t current_value, uint32_t difference_value);

/* General functions. */
void ecma_line_info_free (uint8_t *line_info_p);
void ecma_line_info_get (uint8_t *line_info_p, uint32_t offset,
                         jerry_backtrace_location_t *location_p);

#if JERRY_PARSER_DUMP_BYTE_CODE
void ecma_line_info_dump (uint8_t *line_info_p);
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

#endif /* ECMA_LINE_INFO_H */

/**
 * @}
 * @}
 */

#endif /* !ECMA_LINE_INFO_H */

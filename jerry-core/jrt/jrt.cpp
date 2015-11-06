/* Copyright 2014-2015 Samsung Electronics Co., Ltd.
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

#include <string.h>

#include "jrt.h"

/**
 * Read data of specified type from specified buffer
 *
 * Note:
 *      Offset is in-out and is incremented if the read operation completes successfully.
 *
 * @return true, if read was successful, i.e. offset + sizeof (data) doesn't exceed buffer size,
 *         false - otherwise.
 */

bool __attr_always_inline___
jrt_read_from_buffer_by_offset (const uint8_t *buffer_p, /**< buffer */
                                size_t buffer_size, /**< size of buffer */
                                size_t *in_out_buffer_offset_p, /**< in: offset to read from,
                                                                 *   out: offset, incremented on sizeof (T) */
                                void *out_data_p, /**< out: data */
                                size_t out_data_size) /**< size of the out_data */
{
  if (*in_out_buffer_offset_p + out_data_size > buffer_size)
  {
    return false;
  }

  memcpy (out_data_p, buffer_p + *in_out_buffer_offset_p, out_data_size);
  *in_out_buffer_offset_p += out_data_size;

  return true;
} /* jrt_read_from_buffer_by_offset */

/**
 * Write data of specified type to specified buffer
 *
 * Note:
 *      Offset is in-out and is incremented if the write operation completes successfully.
 *
 * @return true, if write was successful, i.e. offset + sizeof (data) doesn't exceed buffer size,
 *         false - otherwise.
 */
bool __attr_always_inline___
jrt_write_to_buffer_by_offset (uint8_t *buffer_p, /**< buffer */
                               size_t buffer_size, /**< size of buffer */
                               size_t *in_out_buffer_offset_p, /**< in: offset to read from,
                                                                *   out: offset, incremented on sizeof (T) */
                               void *data_p, /**< data */
                               size_t data_size) /**< size of data */
{
  if (*in_out_buffer_offset_p + data_size > buffer_size)
  {
    return false;
  }

  memcpy (buffer_p + *in_out_buffer_offset_p, data_p, data_size);
  *in_out_buffer_offset_p += data_size;

  return true;
} /* jrt_write_to_buffer_by_offset */

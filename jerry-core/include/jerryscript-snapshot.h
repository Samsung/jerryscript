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

#ifndef JERRYSCRIPT_SNAPSHOT_H
#define JERRYSCRIPT_SNAPSHOT_H

#include "jerryscript-types.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/** \addtogroup jerry-snapshot Jerry engine interface - Snapshot feature
 * @{
 */

/**
 * Jerry snapshot format version.
 */
#define JERRY_SNAPSHOT_VERSION (66u)

/**
 * Flags for jerry_generate_snapshot and jerry_generate_function_snapshot.
 */
typedef enum
{
  JERRY_SNAPSHOT_SAVE_STATIC = (1u << 0), /**< static snapshot */
} jerry_generate_snapshot_opts_t;

/**
 * Flags for jerry_exec_snapshot_at and jerry_load_function_snapshot_at.
 */
typedef enum
{
  JERRY_SNAPSHOT_EXEC_COPY_DATA = (1u << 0), /**< copy snashot data */
  JERRY_SNAPSHOT_EXEC_ALLOW_STATIC = (1u << 1), /**< static snapshots allowed */
} jerry_exec_snapshot_opts_t;

/**
 * Snapshot functions.
 */
jerry_value_t jerry_generate_snapshot (const jerry_char_t *source_p, size_t source_size,
                                       const jerry_parse_options_t *options_p, uint32_t generate_snapshot_opts,
                                       uint32_t *buffer_p, size_t buffer_size);
jerry_value_t jerry_generate_function_snapshot (const jerry_char_t *source_p, size_t source_size,
                                                const jerry_char_t *args_p, size_t args_size,
                                                const jerry_parse_options_t *options_p,
                                                uint32_t generate_snapshot_opts,
                                                uint32_t *buffer_p, size_t buffer_size);

jerry_value_t jerry_exec_snapshot (const uint32_t *snapshot_p, size_t snapshot_size,
                                   size_t func_index, uint32_t exec_snapshot_opts);
jerry_value_t jerry_load_function_snapshot (const uint32_t *function_snapshot_p,
                                            const size_t function_snapshot_size,
                                            size_t func_index, uint32_t exec_snapshot_opts);

size_t jerry_merge_snapshots (const uint32_t **inp_buffers_p, size_t *inp_buffer_sizes_p, size_t number_of_snapshots,
                              uint32_t *out_buffer_p, size_t out_buffer_size, const char **error_p);
size_t jerry_get_literals_from_snapshot (const uint32_t *snapshot_p, size_t snapshot_size,
                                         jerry_char_t *lit_buf_p, size_t lit_buf_size, bool is_c_format);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYSCRIPT_SNAPSHOT_H */

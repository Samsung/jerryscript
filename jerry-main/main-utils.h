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

#ifndef MAIN_UTILS_H
#define MAIN_UTILS_H

#include "main-options.h"

/**
 * Standalone Jerry exit codes
 */
#define JERRY_STANDALONE_EXIT_CODE_OK   (0)
#define JERRY_STANDALONE_EXIT_CODE_FAIL (1)

void main_init_engine (main_args_t *arguments_p);
void main_print_unhandled_exception (jerry_value_t error_value);

jerry_value_t main_wait_for_source_callback (const jerry_char_t *resource_name_p,
                                             size_t resource_name_size,
                                             const jerry_char_t *source_p,
                                             size_t source_size,
                                             void *user_p);

bool main_is_value_reset (jerry_value_t value);

#endif /* !MAIN_UTILS_H */

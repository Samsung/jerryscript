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

#ifndef JERRYX_HANDLE_SCOPE_INTERNAL_H
#define JERRYX_HANDLE_SCOPE_INTERNAL_H

#include "jerryscript-ext/handle-scope.h"
#include "jerryscript-port.h"
#include "jerryscript.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** MARK: - handle-scope-allocator.c */
typedef struct jerryx_handle_scope_pool_s jerryx_handle_scope_pool_t;
/**
 * A linear allocating memory pool for type jerryx_handle_scope_t,
 * in which allocated item shall be released in reversed order of allocation
 */
struct jerryx_handle_scope_pool_s
{
  jerryx_handle_scope_t prelist[JERRYX_SCOPE_PRELIST_SIZE]; /**< inlined handle scopes in the pool */
  size_t count; /**< number of handle scopes in the pool */
  jerryx_handle_scope_dynamic_t *start; /**< start address of dynamically allocated handle scope list */
};

jerryx_handle_scope_t *jerryx_handle_scope_get_parent (jerryx_handle_scope_t *scope);

jerryx_handle_scope_t *jerryx_handle_scope_get_child (jerryx_handle_scope_t *scope);

jerryx_handle_scope_t *jerryx_handle_scope_alloc (void);

void jerryx_handle_scope_free (jerryx_handle_scope_t *scope);
/** MARK: - END handle-scope-allocator.c */

/** MARK: - handle-scope.c */
void jerryx_handle_scope_release_handles (jerryx_handle_scope scope);

jerry_value_t jerryx_hand_scope_escape_handle_from_prelist (jerryx_handle_scope scope, size_t idx);

jerry_value_t jerryx_handle_scope_add_handle_to (jerryx_handle_t *handle, jerryx_handle_scope scope);

jerryx_handle_scope_status jerryx_escape_handle_internal (jerryx_escapable_handle_scope scope,
                                                          jerry_value_t escapee,
                                                          jerry_value_t *result,
                                                          bool should_promote);
/** MARK: - END handle-scope.c */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYX_HANDLE_SCOPE_INTERNAL_H */

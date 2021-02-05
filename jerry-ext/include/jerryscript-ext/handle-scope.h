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

#ifndef JERRYX_HANDLE_SCOPE_H
#define JERRYX_HANDLE_SCOPE_H

#include "jerryscript.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef JERRYX_HANDLE_PRELIST_SIZE
#define JERRYX_HANDLE_PRELIST_SIZE 20
#endif

#ifndef JERRYX_SCOPE_PRELIST_SIZE
#define JERRYX_SCOPE_PRELIST_SIZE 20
#endif

typedef struct jerryx_handle_t jerryx_handle_t;
/**
 * Dynamically allocated handle in the scopes.
 * Scopes has it's own size-limited linear storage of handles. Still there
 * might be not enough space left for new handles, dynamically allocated
 * `jerryx_handle_t` could ease the pre-allocated linear memory burden.
 */
struct jerryx_handle_t
{
  jerry_value_t jval; /**< jerry value of the handle bound to */
  jerryx_handle_t *sibling; /**< next sibling the the handle */
};

#define JERRYX_HANDLE_SCOPE_FIELDS                          \
  jerry_value_t handle_prelist[JERRYX_HANDLE_PRELIST_SIZE]; \
  uint8_t prelist_handle_count;                             \
  bool escaped;                                             \
  jerryx_handle_t *handle_ptr

typedef struct jerryx_handle_scope_s jerryx_handle_scope_t;
typedef jerryx_handle_scope_t *jerryx_handle_scope;
typedef jerryx_handle_scope_t *jerryx_escapable_handle_scope;
/**
 * Inlined simple handle scope type.
 */
struct jerryx_handle_scope_s
{
  JERRYX_HANDLE_SCOPE_FIELDS; /**< common handle scope fields */
};

typedef struct jerryx_handle_scope_dynamic_s jerryx_handle_scope_dynamic_t;
/**
 * Dynamically allocated handle scope type.
 */
struct jerryx_handle_scope_dynamic_s
{
  JERRYX_HANDLE_SCOPE_FIELDS; /**< common handle scope fields */
  jerryx_handle_scope_dynamic_t *child; /**< child dynamically allocated handle scope */
  jerryx_handle_scope_dynamic_t *parent; /**< parent dynamically allocated handle scope */
};

#undef JERRYX_HANDLE_SCOPE_FIELDS

typedef enum
{
  jerryx_handle_scope_ok = 0,

  jerryx_escape_called_twice,
  jerryx_handle_scope_mismatch,
} jerryx_handle_scope_status;

jerryx_handle_scope_status jerryx_open_handle_scope (jerryx_handle_scope *result);

jerryx_handle_scope_status jerryx_close_handle_scope (jerryx_handle_scope scope);

jerryx_handle_scope_status jerryx_open_escapable_handle_scope (jerryx_handle_scope *result);

jerryx_handle_scope_status jerryx_close_escapable_handle_scope (jerryx_handle_scope scope);

jerryx_handle_scope_status
jerryx_escape_handle (jerryx_escapable_handle_scope scope, jerry_value_t escapee, jerry_value_t *result);

/**
 * Completely escape a handle from handle scope,
 * leave life time management totally up to user.
 */
jerryx_handle_scope_status
jerryx_remove_handle (jerryx_escapable_handle_scope scope, jerry_value_t escapee, jerry_value_t *result);

jerry_value_t jerryx_create_handle (jerry_value_t jval);

jerry_value_t jerryx_create_handle_in_scope (jerry_value_t jval, jerryx_handle_scope scope);

/** MARK: - handle-scope-allocator.c */
jerryx_handle_scope_t *jerryx_handle_scope_get_current (void);

jerryx_handle_scope_t *jerryx_handle_scope_get_root (void);
/** MARK: - END handle-scope-allocator.c */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYX_HANDLE_SCOPE_H */

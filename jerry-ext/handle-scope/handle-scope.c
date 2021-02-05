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

#include "handle-scope-internal.h"
#include "jext-common.h"
#include <stdlib.h>

JERRYX_STATIC_ASSERT (JERRYX_SCOPE_PRELIST_SIZE < 32, JERRYX_SCOPE_PRELIST_SIZE_MUST_BE_LESS_THAN_SIZE_OF_UINT8_T);

/**
 * Opens a new handle scope and attach it to current global scope as a child scope.
 *
 * @param result - [out value] opened scope.
 * @return status code, jerryx_handle_scope_ok if success.
 */
jerryx_handle_scope_status
jerryx_open_handle_scope (jerryx_handle_scope *result)
{
  *result = jerryx_handle_scope_alloc ();
  return jerryx_handle_scope_ok;
} /** jerryx_open_handle_scope */

/**
 * Release all jerry values attached to given scope
 *
 * @param scope - the scope of handles to be released.
 */
void
jerryx_handle_scope_release_handles (jerryx_handle_scope scope)
{
  size_t prelist_handle_count = scope->prelist_handle_count;
  if (prelist_handle_count == JERRYX_HANDLE_PRELIST_SIZE && scope->handle_ptr != NULL)
  {
    jerryx_handle_t *a_handle = scope->handle_ptr;
    while (a_handle != NULL)
    {
      jerry_release_value (a_handle->jval);
      jerryx_handle_t *sibling = a_handle->sibling;
      jerry_heap_free (a_handle, sizeof (jerryx_handle_t));
      a_handle = sibling;
    }
    scope->handle_ptr = NULL;
    prelist_handle_count = JERRYX_HANDLE_PRELIST_SIZE;
  }

  for (size_t idx = 0; idx < prelist_handle_count; idx++)
  {
    jerry_release_value (scope->handle_prelist[idx]);
  }
  scope->prelist_handle_count = 0;
} /** jerryx_handle_scope_release_handles */

/**
 * Close the scope and its child scopes and release all jerry values that
 * resides in the scopes.
 * Scopes must be closed in the reverse order from which they were created.
 *
 * @param scope - the scope closed.
 * @return status code, jerryx_handle_scope_ok if success.
 */
jerryx_handle_scope_status
jerryx_close_handle_scope (jerryx_handle_scope scope)
{
  /**
   * Release all handles related to given scope and its child scopes
   */
  jerryx_handle_scope a_scope = scope;
  do
  {
    jerryx_handle_scope_release_handles (a_scope);
    jerryx_handle_scope child = jerryx_handle_scope_get_child (a_scope);
    jerryx_handle_scope_free (a_scope);
    a_scope = child;
  } while (a_scope != NULL);

  return jerryx_handle_scope_ok;
} /** jerryx_close_handle_scope */

/**
 * Opens a new handle scope from which one object can be promoted to the outer scope
 * and attach it to current global scope as a child scope.
 *
 * @param result - [out value] opened escapable handle scope.
 * @return status code, jerryx_handle_scope_ok if success.
 */
jerryx_handle_scope_status
jerryx_open_escapable_handle_scope (jerryx_handle_scope *result)
{
  return jerryx_open_handle_scope (result);
} /** jerryx_open_escapable_handle_scope */

/**
 * Close the scope and its child scopes and release all jerry values that
 * resides in the scopes.
 * Scopes must be closed in the reverse order from which they were created.
 *
 * @param scope - the one to be closed.
 * @return status code, jerryx_handle_scope_ok if success.
 */
jerryx_handle_scope_status
jerryx_close_escapable_handle_scope (jerryx_handle_scope scope)
{
  return jerryx_close_handle_scope (scope);
} /** jerryx_close_escapable_handle_scope */

/**
 * Internal helper.
 * Escape a jerry value from the scope, yet did not promote it to outer scope.
 * An assertion of if parent exists shall be made before invoking this function.
 *
 * @param scope - scope of the handle added to.
 * @param idx - expected index of the handle in the scope's prelist.
 * @returns escaped jerry value id
 */
jerry_value_t
jerryx_hand_scope_escape_handle_from_prelist (jerryx_handle_scope scope, size_t idx)
{
  jerry_value_t jval = scope->handle_prelist[idx];
  if (scope->prelist_handle_count == JERRYX_HANDLE_PRELIST_SIZE && scope->handle_ptr != NULL)
  {
    jerryx_handle_t *handle = scope->handle_ptr;
    scope->handle_ptr = handle->sibling;
    scope->handle_prelist[idx] = handle->jval;
    jerry_heap_free (handle, sizeof (jerryx_handle_t));
    return jval;
  }

  if (idx < JERRYX_HANDLE_PRELIST_SIZE - 1)
  {
    scope->handle_prelist[idx] = scope->handle_prelist[scope->prelist_handle_count - 1];
  }
  return jval;
} /** jerryx_hand_scope_escape_handle_from_prelist */

/**
 * Internal helper.
 * Escape a jerry value from the given escapable handle scope.
 *
 * @param scope - the expected scope to be escaped from.
 * @param escapee - the jerry value to be escaped.
 * @param result - [out value] escaped jerry value result.
 * @param should_promote - true if the escaped value should be added to parent
 *                         scope after escaped from the given handle scope.
 * @return status code, jerryx_handle_scope_ok if success.
 */
jerryx_handle_scope_status
jerryx_escape_handle_internal (jerryx_escapable_handle_scope scope,
                               jerry_value_t escapee,
                               jerry_value_t *result,
                               bool should_promote)
{
  if (scope->escaped)
  {
    return jerryx_escape_called_twice;
  }

  jerryx_handle_scope parent = jerryx_handle_scope_get_parent (scope);
  if (parent == NULL)
  {
    return jerryx_handle_scope_mismatch;
  }

  bool found = false;
  {
    size_t found_idx = 0;
    size_t prelist_count = scope->prelist_handle_count;
    /**
     * Search prelist in a reversed order since last added handle
     * is possible the one to be escaped
     */
    for (size_t idx_plus_1 = prelist_count; idx_plus_1 > 0; --idx_plus_1)
    {
      if (escapee == scope->handle_prelist[idx_plus_1 - 1])
      {
        found = true;
        found_idx = idx_plus_1 - 1;
        break;
      }
    }

    if (found)
    {
      *result = jerryx_hand_scope_escape_handle_from_prelist (scope, found_idx);

      --scope->prelist_handle_count;
      if (should_promote)
      {
        scope->escaped = true;
        /**
         * Escape handle to parent scope
         */
        jerryx_create_handle_in_scope (*result, jerryx_handle_scope_get_parent (scope));
      }
      return jerryx_handle_scope_ok;
    }
  };

  if (scope->prelist_handle_count <= JERRYX_HANDLE_PRELIST_SIZE && scope->handle_ptr == NULL)
  {
    return jerryx_handle_scope_mismatch;
  }

  /**
   * Handle chain list is already in an reversed order,
   * search through it as it is
   */
  jerryx_handle_t *handle = scope->handle_ptr;
  jerryx_handle_t *memo_handle = NULL;
  jerryx_handle_t *found_handle = NULL;
  while (!found)
  {
    if (handle == NULL)
    {
      return jerryx_handle_scope_mismatch;
    }
    if (handle->jval != escapee)
    {
      memo_handle = handle;
      handle = handle->sibling;
      continue;
    }
    /**
     * Remove found handle from current scope's handle chain
     */
    found = true;
    found_handle = handle;
    if (memo_handle == NULL)
    {
      // found handle is the first handle in heap
      scope->handle_ptr = found_handle->sibling;
    }
    else
    {
      memo_handle->sibling = found_handle->sibling;
    }
  }

  if (should_promote)
  {
    /**
     * Escape handle to parent scope
     */
    *result = jerryx_handle_scope_add_handle_to (found_handle, parent);
  }

  if (should_promote)
  {
    scope->escaped = true;
  }
  return jerryx_handle_scope_ok;
} /** jerryx_escape_handle_internal */

/**
 * Promotes the handle to the JavaScript object so that it is valid for the lifetime of
 * the outer scope. It can only be called once per scope. If it is called more than
 * once an error will be returned.
 *
 * @param scope - the expected scope to be escaped from.
 * @param escapee - the jerry value to be escaped.
 * @param result - [out value] escaped jerry value result.
 * @return status code, jerryx_handle_scope_ok if success.
 */
jerryx_handle_scope_status
jerryx_escape_handle (jerryx_escapable_handle_scope scope, jerry_value_t escapee, jerry_value_t *result)
{
  return jerryx_escape_handle_internal (scope, escapee, result, true);
} /** jerryx_escape_handle */

/**
 * Escape a handle from scope yet do not promote it to the outer scope.
 * Leave the handle's life time management up to user.
 *
 * @param scope - the expected scope to be removed from.
 * @param escapee - the jerry value to be removed.
 * @param result - [out value] removed jerry value result.
 * @return status code, jerryx_handle_scope_ok if success.
 */
jerryx_handle_scope_status
jerryx_remove_handle (jerryx_escapable_handle_scope scope, jerry_value_t escapee, jerry_value_t *result)
{
  return jerryx_escape_handle_internal (scope, escapee, result, false);
} /** jerryx_remove_handle */

/**
 * Try to reuse given handle if possible while adding to the scope.
 *
 * @param handle - the one to be added to the scope.
 * @param scope - the scope of handle to be added to.
 * @returns the jerry value id wrapped by given handle.
 */
jerry_value_t
jerryx_handle_scope_add_handle_to (jerryx_handle_t *handle, jerryx_handle_scope scope)
{
  size_t prelist_handle_count = scope->prelist_handle_count;
  if (prelist_handle_count < JERRYX_HANDLE_PRELIST_SIZE)
  {
    ++scope->prelist_handle_count;
    jerry_value_t jval = handle->jval;
    jerry_heap_free (handle, sizeof (jerryx_handle_t));
    scope->handle_prelist[prelist_handle_count] = jval;
    return jval;
  }

  handle->sibling = scope->handle_ptr;
  scope->handle_ptr = handle;
  return handle->jval;
} /** jerryx_handle_scope_add_handle_to */

/**
 * Add given jerry value to the scope.
 *
 * @param jval - jerry value to be added to scope.
 * @param scope - the scope of the jerry value been expected to be added to.
 * @return jerry value that added to scope.
 */
jerry_value_t
jerryx_create_handle_in_scope (jerry_value_t jval, jerryx_handle_scope scope)
{
  size_t prelist_handle_count = scope->prelist_handle_count;
  if (prelist_handle_count < JERRYX_HANDLE_PRELIST_SIZE)
  {
    scope->handle_prelist[prelist_handle_count] = jval;

    ++scope->prelist_handle_count;
    return jval;
  }
  jerryx_handle_t *handle = (jerryx_handle_t *) jerry_heap_alloc (sizeof (jerryx_handle_t));
  JERRYX_ASSERT (handle != NULL);
  handle->jval = jval;

  handle->sibling = scope->handle_ptr;
  scope->handle_ptr = handle;

  return jval;
} /** jerryx_create_handle_in_scope */

/**
 * Add given jerry value to current top scope.
 *
 * @param jval - jerry value to be added to scope.
 * @return jerry value that added to scope.
 */
jerry_value_t
jerryx_create_handle (jerry_value_t jval)
{
  return jerryx_create_handle_in_scope (jval, jerryx_handle_scope_get_current ());
} /** jerryx_create_handle */

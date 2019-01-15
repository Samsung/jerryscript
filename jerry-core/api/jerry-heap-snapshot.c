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

#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "jerryscript-heap-snapshot.h"
#include "jmem.h"

#ifdef JERRY_ENABLE_HEAP_SNAPSHOT

static void ecma_object_get_metadata (ecma_object_t *object_p,
                                      jerry_heap_snapshot_node_type_t *node_type,
                                      ecma_string_t **repr
                                      )
{
  if (ecma_is_lexical_environment (object_p))
  {
    *node_type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_HIDDEN;
    return;
  }

  ecma_object_type_t type = ecma_get_object_type (object_p);

  if (ecma_get_object_is_builtin (object_p))
  {
    *node_type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_OBJECT;
    if (type == ECMA_OBJECT_TYPE_CLASS)
    {
      lit_magic_string_id_t id = ecma_object_get_class_name (object_p);
      *repr = ecma_get_magic_string (id);
      return;
    }
    // No direct way to get the names of the rest of the builtins.
    return;
  }

  switch (type)
  {
    case ECMA_OBJECT_TYPE_CLASS:
    {
      *node_type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_OBJECT;
      lit_magic_string_id_t id = ecma_object_get_class_name (object_p);
      *repr = ecma_get_magic_string (id);
      return;
    }
    case ECMA_OBJECT_TYPE_FUNCTION:
    {
      *node_type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_CLOSURE;
      return;
    }
    case ECMA_OBJECT_TYPE_EXTERNAL_FUNCTION:
    {
      *node_type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_NATIVE;
      return;
    }
    case ECMA_OBJECT_TYPE_ARRAY:
    {
      *node_type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_ARRAY;
      return;
    }
    case ECMA_OBJECT_TYPE_GENERAL:
    {
      *node_type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_HIDDEN;
      return;
    }
#ifndef CONFIG_DISABLE_ES2015_ARROW_FUNCTION
    case ECMA_OBJECT_TYPE_ARROW_FUNCTION:
#endif /* !CONFIG_DISABLE_ES2015_ARROW_FUNCTION */
    case ECMA_OBJECT_TYPE_BOUND_FUNCTION:
    {
      *node_type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_CLOSURE;
      return;
    }
    case ECMA_OBJECT_TYPE_PSEUDO_ARRAY:
    {
      *node_type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_ARRAY;
      return;
    }
    default:
    {
      JERRY_UNREACHABLE ();
    }
  }
} /* ecma_object_get_metadata */

typedef struct
{
  jerry_heap_snapshot_node_callback_t node_callback;
  jerry_heap_snapshot_edge_callback_t edge_callback;
  void *user_data_p;
} ecma_heap_snapshot_walk_context_t;

static void
ecma_heap_snapshot_walk_callback (void *parent_p, /**< parent object */
                                  void *object_p, /**< object */
                                  ecma_heap_gc_allocation_type_t alloc_type, /**< allocation type */
                                  jerry_heap_snapshot_edge_type_t edge_type, /**< connecting edge */
                                  ecma_string_t *name_p, /**< connecting edge's name */
                                  void *user_data_p)
{
  ecma_heap_snapshot_walk_context_t *ctx = (ecma_heap_snapshot_walk_context_t *) user_data_p;

  jerry_heap_snapshot_node_type_t type;
  ecma_string_t *repr_p = NULL;
  jerry_value_t repr_value = jerry_create_undefined ();
#ifdef JMEM_TRACK_ALLOCATION_SIZES
  const size_t size = jmem_heap_allocation_size (object_p);
#else
  const size_t size = 0;
#endif

  switch (alloc_type)
  {
    case ECMA_GC_ALLOCATION_TYPE_OBJECT:
    {
      ecma_object_get_metadata (object_p, &type, &repr_p);
      if (repr_p)
      {
        repr_value = ecma_make_string_value (repr_p);
      }
      break;
    }
    case ECMA_GC_ALLOCATION_TYPE_STRING:
    {
      type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_STRING;
      repr_value = ecma_make_string_value ((ecma_string_t *) object_p);
      repr_p = (ecma_string_t *) (uintptr_t) repr_value;
      break;
    }
    case ECMA_GC_ALLOCATION_TYPE_PROPERTY_PAIR:
    case ECMA_GC_ALLOCATION_TYPE_LIT_STORAGE:
    {
      type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_HIDDEN;
      break;
    }
    case ECMA_GC_ALLOCATION_TYPE_BYTECODE:
    {
      type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_CODE;
      break;
    }
    case ECMA_GC_ALLOCATION_TYPE_NATIVE:
    {
      type = JERRY_HEAP_SNAPSHOT_NODE_TYPE_NATIVE;
      break;
    }
    default:
    {
      JERRY_UNREACHABLE ();
    }
  }

  jerry_heap_snapshot_node_id_t object_node;
  if (alloc_type == ECMA_GC_ALLOCATION_TYPE_STRING)
  {
    // Strings need special attention due to direct/indirect packing.
    object_node = (uintptr_t) ecma_make_string_value ((ecma_string_t *) object_p);
  }
  else
  {
    // Everything else is simply a pointer (albeit maybe not to an ecma_object_t).
    object_node = (uintptr_t) ecma_make_object_value (object_p);
  }

  jerry_heap_snapshot_node_id_t parent_node;
  // Strings are never parents of other nodes, so the potential lack of symmetry is OK.
  if (parent_p)
  {
    parent_node = (uintptr_t) ecma_make_object_value (parent_p);
  }
  else
  {
    parent_node = (uintptr_t) jerry_create_undefined ();
  }

  ctx->node_callback (object_node,
                      type, size,
                      repr_value, (uintptr_t) repr_p,
                      ctx->user_data_p);

  // Only output edges with parents - ones without are uninteresting.
  if (parent_p)
  {
    jerry_value_t name_value = name_p ? ecma_make_string_value (name_p) : jerry_create_undefined ();
    ctx->edge_callback (parent_node,
                        object_node,
                        edge_type,
                        name_value, name_p ? (uintptr_t) name_value : 0,
                        ctx->user_data_p);
  }
} /* ecma_heap_snapshot_walk_callback */

/**
 * Enumerate all heap allocations, plus referenced off-heap allocations.
 */
void
jerry_heap_snapshot_capture (jerry_heap_snapshot_node_callback_t node_cb,
                             jerry_heap_snapshot_edge_callback_t edge_cb,
                             void *user_data_p)
{
  ecma_heap_snapshot_walk_context_t ctx =
  {
    .node_callback = node_cb,
    .edge_callback = edge_cb,
    .user_data_p = user_data_p
  };
  ecma_gc_walk_heap (ecma_heap_snapshot_walk_callback, &ctx);
} /* jerry_heap_snapshot_capture */

#endif /* JERRY_ENABLE_HEAP_SNAPSHOT */

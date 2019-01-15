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

/**
 * Garbage collector implementation
 */

#include "ecma-alloc.h"
#include "ecma-globals.h"
#include "ecma-gc.h"
#include "ecma-function-object.h"
#include "ecma-helpers.h"
#include "ecma-property-hashmap.h"
#include "jcontext.h"
#include "jrt.h"
#include "jrt-libc-includes.h"
#include "jrt-bit-fields.h"
#include "re-compiler.h"
#include "vm-defines.h"
#include "vm-stack.h"

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
#include "ecma-typedarray-object.h"
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
#include "ecma-promise-object.h"
#endif /* !CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
#ifndef CONFIG_DISABLE_ES2015_MAP_BUILTIN
#include "ecma-map-object.h"
#endif /* !CONFIG_DISABLE_ES2015_MAP_BUILTIN */

/* TODO: Extract GC to a separate component */

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmagc Garbage collector
 * @{
 */

/*
 * The garbage collector uses the reference counter
 * of object: it increases the counter by one when
 * the object is marked at the first time.
 */

/**
 * Get next object in list of objects with same generation.
 *
 * @return pointer to the next ecma-object
 *         NULL - if there is no next ecma-object
 */
static inline ecma_object_t *
ecma_gc_get_object_next (ecma_object_t *object_p) /**< object */
{
  JERRY_ASSERT (object_p != NULL);

  return ECMA_GET_POINTER (ecma_object_t, object_p->gc_next_cp);
} /* ecma_gc_get_object_next */

/**
 * Set next object in list of objects with same generation.
 */
static inline void
ecma_gc_set_object_next (ecma_object_t *object_p, /**< object */
                         ecma_object_t *next_object_p) /**< next object */
{
  JERRY_ASSERT (object_p != NULL);

  ECMA_SET_POINTER (object_p->gc_next_cp, next_object_p);
} /* ecma_gc_set_object_next */

/**
 * Get visited flag of the object.
 *
 * @return true  - if visited
 *         false - otherwise
 */
static inline bool
ecma_gc_is_object_visited (ecma_object_t *object_p) /**< object */
{
  JERRY_ASSERT (object_p != NULL);

  return (object_p->type_flags_refs >= ECMA_OBJECT_REF_ONE);
} /* ecma_gc_is_object_visited */

/**
 * Set visited flag of the object.
 */
static void
ecma_gc_set_object_visited (void *parent_p, /**< parent object */
                            void *alloc_p, /**< object */
                            ecma_heap_gc_allocation_type_t allocation_type, /**< connecting edge */
                            jerry_heap_snapshot_edge_type_t edge_type, /**< connecting edge */
                            ecma_string_t *edge_name_p, /**< connecting edge's name */
                            void *user_data_p) /**< user context */
{
  JERRY_UNUSED (parent_p);
  JERRY_UNUSED (allocation_type);
  JERRY_UNUSED (edge_type);
  JERRY_UNUSED (edge_name_p);
  JERRY_UNUSED (user_data_p);
  ecma_object_t *object_p = (ecma_object_t *) alloc_p;
  /* Set reference counter to one if it is zero. */
  if (object_p->type_flags_refs < ECMA_OBJECT_REF_ONE)
  {
    object_p->type_flags_refs |= ECMA_OBJECT_REF_ONE;
  }
} /* ecma_gc_set_object_visited */

/**
 * Initialize GC information for the object
 */
inline void
ecma_init_gc_info (ecma_object_t *object_p) /**< object */
{
  JERRY_CONTEXT (ecma_gc_objects_number)++;
  JERRY_CONTEXT (ecma_gc_new_objects)++;

  JERRY_ASSERT (JERRY_CONTEXT (ecma_gc_new_objects) <= JERRY_CONTEXT (ecma_gc_objects_number));

  JERRY_ASSERT (object_p->type_flags_refs < ECMA_OBJECT_REF_ONE);
  object_p->type_flags_refs = (uint16_t) (object_p->type_flags_refs | ECMA_OBJECT_REF_ONE);

  ecma_gc_set_object_next (object_p, JERRY_CONTEXT (ecma_gc_objects_p));
  JERRY_CONTEXT (ecma_gc_objects_p) = object_p;
} /* ecma_init_gc_info */

/**
 * Increase reference counter of an object
 */
void
ecma_ref_object (ecma_object_t *object_p) /**< object */
{
  if (JERRY_LIKELY (object_p->type_flags_refs < ECMA_OBJECT_MAX_REF))
  {
    object_p->type_flags_refs = (uint16_t) (object_p->type_flags_refs + ECMA_OBJECT_REF_ONE);
  }
  else
  {
    jerry_fatal (ERR_REF_COUNT_LIMIT);
  }
} /* ecma_ref_object */

/**
 * Decrease reference counter of an object
 */
inline void JERRY_ATTR_ALWAYS_INLINE
ecma_deref_object (ecma_object_t *object_p) /**< object */
{
  JERRY_ASSERT (object_p->type_flags_refs >= ECMA_OBJECT_REF_ONE);
  object_p->type_flags_refs = (uint16_t) (object_p->type_flags_refs - ECMA_OBJECT_REF_ONE);
} /* ecma_deref_object */

/**
 * Traverse referenced objects of property
 */
inline static void JERRY_ATTR_ALWAYS_INLINE
ecma_gc_traverse_property_inner (ecma_property_pair_t *property_pair_p, /**< property pair */
                                 uint32_t index, /**< property index */
                                 ecma_gc_traverse_callback_t traverse_cb, /**< traverse callback */
                                 bool full_traverse, /**< traverse to non-GC-visible allocations */
                                 void *user_data_p) /**< context to pass to traverse_cb */
{
  uint8_t property = property_pair_p->header.types[index];

  ecma_string_t *prop_name_p = NULL;

  if (full_traverse &&
      (ECMA_PROPERTY_GET_NAME_TYPE (property) != ECMA_DIRECT_STRING_MAGIC
       || property_pair_p->names_cp[index] < LIT_GC_MARK_REQUIRED_MAGIC_STRING__COUNT))
  {
    // This does not allocate any memory, but does increment the refcount on the string.
    prop_name_p = ecma_string_from_property_name (property, property_pair_p->names_cp[index]);
    ecma_deref_ecma_string (prop_name_p);
    traverse_cb (property_pair_p,
                 (ecma_object_t *) prop_name_p,
                 ECMA_GC_ALLOCATION_TYPE_STRING,
                 JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROPERTY_NAME,
                 NULL,
                 user_data_p);
  }

  switch (ECMA_PROPERTY_GET_TYPE (property))
  {
    case ECMA_PROPERTY_TYPE_NAMEDDATA:
    {
      ecma_value_t value = property_pair_p->values[index].value;

      if (ecma_is_value_object (value))
      {
        ecma_object_t *value_obj_p = ecma_get_object_from_value (value);

        traverse_cb ((ecma_object_t *) property_pair_p,
                     value_obj_p,
                     ECMA_GC_ALLOCATION_TYPE_OBJECT,
                     JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROPERTY,
                     prop_name_p,
                     user_data_p);
      }
      else if (full_traverse && ecma_is_value_string (value))
      {
        traverse_cb ((ecma_object_t *) property_pair_p,
                     ecma_get_string_from_value (value),
                     ECMA_GC_ALLOCATION_TYPE_STRING,
                     JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROPERTY,
                     prop_name_p,
                     user_data_p);
      }
      break;
    }
    case ECMA_PROPERTY_TYPE_NAMEDACCESSOR:
    {
      ecma_property_value_t *accessor_objs_p = property_pair_p->values + index;
      ecma_object_t *getter_obj_p = ecma_get_named_accessor_property_getter (accessor_objs_p);
      ecma_object_t *setter_obj_p = ecma_get_named_accessor_property_setter (accessor_objs_p);

      if (getter_obj_p != NULL)
      {
        traverse_cb ((ecma_object_t *) property_pair_p,
                     getter_obj_p,
                     ECMA_GC_ALLOCATION_TYPE_OBJECT,
                     JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROPERTY_GET,
                     prop_name_p,
                     user_data_p);
      }

      if (setter_obj_p != NULL)
      {
        traverse_cb ((ecma_object_t *) property_pair_p,
                     setter_obj_p,
                     ECMA_GC_ALLOCATION_TYPE_OBJECT,
                     JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROPERTY_SET,
                     prop_name_p,
                     user_data_p);
      }
      break;
    }
    case ECMA_PROPERTY_TYPE_INTERNAL:
    {
      JERRY_ASSERT (ECMA_PROPERTY_GET_NAME_TYPE (property) == ECMA_DIRECT_STRING_MAGIC
                    && property_pair_p->names_cp[index] >= LIT_FIRST_INTERNAL_MAGIC_STRING);
      break;
    }
    default:
    {
      JERRY_ASSERT (ECMA_PROPERTY_GET_TYPE (property) == ECMA_PROPERTY_TYPE_SPECIAL);

      JERRY_ASSERT (property == ECMA_PROPERTY_TYPE_HASHMAP
                    || property == ECMA_PROPERTY_TYPE_DELETED);
      break;
    }
  }
} /* ecma_gc_traverse_property_inner */

/**
 * Mark referenced objects of a property - wrapper around ecma_gc_traverse_property_inner
 */
static void
ecma_gc_mark_property (ecma_property_pair_t *property_pair_p, /**< property pair */
                       uint32_t index) /**< property index */
{
  ecma_gc_traverse_property_inner (property_pair_p, index, ecma_gc_set_object_visited, false, NULL);
} /* ecma_gc_mark_property */

/**
 * Traverse referenced objects of a property - wrapper around ecma_gc_traverse_property_inner
 */
static void
ecma_gc_traverse_property (ecma_property_pair_t *property_pair_p, /**< property pair */
                           uint32_t index, /**< property index */
                           ecma_gc_traverse_callback_t traverse_cb, /**< traverse callback */
                           void *user_data_p) /**< context to pass to traverse_cb */
{
  ecma_gc_traverse_property_inner (property_pair_p, index, traverse_cb, true, user_data_p);
} /* ecma_gc_traverse_property */

#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN

/**
 * Traverse objects referenced by Promise built-in.
 */
inline static void JERRY_ATTR_ALWAYS_INLINE
ecma_gc_traverse_promise_object (ecma_extended_object_t *ext_object_p, /**< extended object */
                                 ecma_gc_traverse_callback_t traverse_cb, /**< traverse callback */
                                 void *user_data_p) /**< context to pass to traverse_cb */
{
  /* Traverse promise result. */
  ecma_value_t result = ext_object_p->u.class_prop.u.value;

  if (ecma_is_value_object (result))
  {
    traverse_cb (ext_object_p,
                 ecma_get_object_from_value (result),
                 ECMA_GC_ALLOCATION_TYPE_OBJECT,
                 JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROMISE_RESULT,
                 NULL,
                 user_data_p);
  }

  /* Traverse all reactions. */
  ecma_value_t *ecma_value_p;
  ecma_value_p = ecma_collection_iterator_init (((ecma_promise_object_t *) ext_object_p)->fulfill_reactions);

  while (ecma_value_p != NULL)
  {
    traverse_cb (ext_object_p,
                 ecma_get_object_from_value (*ecma_value_p),
                 ECMA_GC_ALLOCATION_TYPE_OBJECT,
                 JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROMISE_FULFILL,
                 NULL,
                 user_data_p);
    ecma_value_p = ecma_collection_iterator_next (ecma_value_p);
  }

  ecma_value_p = ecma_collection_iterator_init (((ecma_promise_object_t *) ext_object_p)->reject_reactions);

  while (ecma_value_p != NULL)
  {
    traverse_cb (ext_object_p,
                 ecma_get_object_from_value (*ecma_value_p),
                 ECMA_GC_ALLOCATION_TYPE_OBJECT,
                 JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROMISE_REJECT,
                 NULL,
                 user_data_p);
    ecma_value_p = ecma_collection_iterator_next (ecma_value_p);
  }
} /* ecma_gc_traverse_promise_object */

#endif /* !CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */

#ifndef CONFIG_DISABLE_ES2015_MAP_BUILTIN

/**
 * Traverse objects referenced by Map built-in.
 */
inline static void JERRY_ATTR_ALWAYS_INLINE
ecma_gc_traverse_map_object (ecma_extended_object_t *ext_object_p, /**< extended object */
                             ecma_gc_traverse_callback_t traverse_cb, /**< traverse callback */
                             void *user_data_p) /**< context to pass to traverse_cb */
{
  ecma_map_object_t *map_object_p = (ecma_map_object_t *) ext_object_p;

  jmem_cpointer_t first_chunk_cp = map_object_p->first_chunk_cp;

  if (JERRY_UNLIKELY (first_chunk_cp == ECMA_NULL_POINTER))
  {
    return;
  }

  ecma_value_t *item_p = ECMA_GET_NON_NULL_POINTER (ecma_map_object_chunk_t, first_chunk_cp)->items;

  while (true)
  {
    ecma_value_t item = *item_p++;

    if (!ecma_is_value_pointer (item))
    {
      if (ecma_is_value_object (item))
      {
        traverse_cb (ext_object_p,
                     ecma_get_object_from_value (item),
                     ECMA_GC_ALLOCATION_TYPE_OBJECT,
                     JERRY_HEAP_SNAPSHOT_EDGE_TYPE_MAP_ELEMENT,
                     NULL,
                     user_data_p);
      }
    }
    else
    {
      item_p = (ecma_value_t *) ecma_get_pointer_from_value (item);

      if (item_p == NULL)
      {
        return;
      }
    }
  }
} /* ecma_gc_traverse_map_object */

#endif /* !CONFIG_DISABLE_ES2015_MAP_BUILTIN */

/**
 * Traverse objects starting from specified object as root
 *
 * This is force-inlined so wrappers (e.g. ecma_gc_mark)
 * always benefit from the compiler eliding full_traverse conditionals,
 * unused parameters to traverse_cb, etc.
 */
inline static void JERRY_ATTR_ALWAYS_INLINE
ecma_gc_traverse_inner (ecma_object_t *object_p, /**< object to traverse from */
                        ecma_gc_traverse_callback_t traverse_cb, /**< traverse callback */
                        bool full_traverse, /**< traverse to non-GC-visible allocations */
                        void *user_data_p) /**< context to pass to traverse_cb */
{
  JERRY_ASSERT (object_p != NULL);
  if (!full_traverse)
  {
    JERRY_ASSERT (ecma_gc_is_object_visited (object_p));
  }

  bool traverse_properties = true;

  if (ecma_is_lexical_environment (object_p))
  {
    ecma_object_t *lex_env_p = ecma_get_lex_env_outer_reference (object_p);
    if (lex_env_p != NULL)
    {
      traverse_cb (object_p,
                   lex_env_p,
                   ECMA_GC_ALLOCATION_TYPE_OBJECT,
                   JERRY_HEAP_SNAPSHOT_EDGE_TYPE_LEXENV,
                   NULL,
                   user_data_p);
    }

    if (ecma_get_lex_env_type (object_p) != ECMA_LEXICAL_ENVIRONMENT_DECLARATIVE)
    {
      ecma_object_t *binding_object_p = ecma_get_lex_env_binding_object (object_p);
      traverse_cb (object_p,
                   binding_object_p,
                   ECMA_GC_ALLOCATION_TYPE_OBJECT,
                   JERRY_HEAP_SNAPSHOT_EDGE_TYPE_BIND,
                   NULL,
                   user_data_p);

      traverse_properties = false;
    }
  }
  else
  {
    ecma_object_t *proto_p = ecma_get_object_prototype (object_p);
    if (proto_p != NULL)
    {
      traverse_cb (object_p,
                   proto_p,
                   ECMA_GC_ALLOCATION_TYPE_OBJECT,
                   JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROTOTYPE,
                   NULL,
                   user_data_p);
    }

    switch (ecma_get_object_type (object_p))
    {
      case ECMA_OBJECT_TYPE_CLASS:
      {
        ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

        switch (ext_object_p->u.class_prop.class_id)
        {
#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
          case LIT_MAGIC_STRING_PROMISE_UL:
          {
            ecma_gc_traverse_promise_object (ext_object_p, traverse_cb, user_data_p);
            break;
          }
#endif /* !CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
#ifndef CONFIG_DISABLE_ES2015_MAP_BUILTIN
          case LIT_MAGIC_STRING_MAP_UL:
          {
            ecma_gc_traverse_map_object (ext_object_p, traverse_cb, user_data_p);
            break;
          }
#endif /* !CONFIG_DISABLE_ES2015_MAP_BUILTIN */
          default:
          {
            break;
          }
        }

        break;
      }
      case ECMA_OBJECT_TYPE_PSEUDO_ARRAY:
      {
        ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

        switch (ext_object_p->u.pseudo_array.type)
        {
#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
          case ECMA_PSEUDO_ARRAY_TYPEDARRAY:
          case ECMA_PSEUDO_ARRAY_TYPEDARRAY_WITH_INFO:
          {
            traverse_cb (object_p,
                         ecma_typedarray_get_arraybuffer (object_p),
                         ECMA_GC_ALLOCATION_TYPE_OBJECT,
                         JERRY_HEAP_SNAPSHOT_EDGE_TYPE_ELEMENTS,
                         NULL,
                         user_data_p);
            break;
          }
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
          default:
          {
            JERRY_ASSERT (ext_object_p->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS);

            ecma_object_t *lex_env_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_object_t,
                                                                        ext_object_p->u.pseudo_array.u2.lex_env_cp);

            traverse_cb (object_p,
                         lex_env_p,
                         ECMA_GC_ALLOCATION_TYPE_OBJECT,
                         JERRY_HEAP_SNAPSHOT_EDGE_TYPE_LEXENV,
                         NULL,
                         user_data_p);
            break;
          }
        }

        break;
      }
      case ECMA_OBJECT_TYPE_BOUND_FUNCTION:
      {
        ecma_extended_object_t *ext_function_p = (ecma_extended_object_t *) object_p;

        ecma_object_t *target_func_obj_p;
        target_func_obj_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_object_t,
                                                             ext_function_p->u.bound_function.target_function);

        traverse_cb (object_p,
                     target_func_obj_p,
                     ECMA_GC_ALLOCATION_TYPE_OBJECT,
                     JERRY_HEAP_SNAPSHOT_EDGE_TYPE_BIND,
                     NULL,
                     user_data_p);

        ecma_value_t args_len_or_this = ext_function_p->u.bound_function.args_len_or_this;

        if (!ecma_is_value_integer_number (args_len_or_this))
        {
          if (ecma_is_value_object (args_len_or_this))
          {
            traverse_cb (object_p,
                         ecma_get_object_from_value (args_len_or_this),
                         ECMA_GC_ALLOCATION_TYPE_OBJECT,
                         JERRY_HEAP_SNAPSHOT_EDGE_TYPE_THIS,
                         NULL,
                         user_data_p);
          }
          break;
        }

        ecma_integer_value_t args_length = ecma_get_integer_from_value (args_len_or_this);
        ecma_value_t *args_p = (ecma_value_t *) (ext_function_p + 1);

        JERRY_ASSERT (args_length > 0);

        for (ecma_integer_value_t i = 0; i < args_length; i++)
        {
          if (ecma_is_value_object (args_p[i]))
          {
            traverse_cb (object_p,
                         ecma_get_object_from_value (args_p[i]),
                         ECMA_GC_ALLOCATION_TYPE_OBJECT,
                         JERRY_HEAP_SNAPSHOT_EDGE_TYPE_BIND_ARGS,
                         NULL,
                         user_data_p);
          }
        }
        break;
      }
      case ECMA_OBJECT_TYPE_FUNCTION:
      {
        if (!ecma_get_object_is_builtin (object_p))
        {
          ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) object_p;

          traverse_cb (object_p,
                       ECMA_GET_INTERNAL_VALUE_POINTER (ecma_object_t,
                                                        ext_func_p->u.function.scope_cp),
                       ECMA_GC_ALLOCATION_TYPE_OBJECT,
                       JERRY_HEAP_SNAPSHOT_EDGE_TYPE_SCOPE,
                       NULL, user_data_p);
        }
        break;
      }
#ifndef CONFIG_DISABLE_ES2015_ARROW_FUNCTION
      case ECMA_OBJECT_TYPE_ARROW_FUNCTION:
      {
        ecma_arrow_function_t *arrow_func_p = (ecma_arrow_function_t *) object_p;

        traverse_cb (object_p,
                 ECMA_GET_NON_NULL_POINTER (ecma_object_t,
                                            arrow_func_p->scope_cp),
                 ECMA_GC_ALLOCATION_TYPE_OBJECT,
                 JERRY_HEAP_SNAPSHOT_EDGE_TYPE_SCOPE,
                 NULL, user_data_p);

        if (ecma_is_value_object (arrow_func_p->this_binding))
        {
          traverse_cb (object_p,
                   ecma_get_object_from_value (arrow_func_p->this_binding),
                   ECMA_GC_ALLOCATION_TYPE_OBJECT,
                   JERRY_HEAP_SNAPSHOT_EDGE_TYPE_THIS,
                   NULL, user_data_p);
        }
        break;
      }
#endif /* !CONFIG_DISABLE_ES2015_ARROW_FUNCTION */
      default:
      {
        break;
      }
    }
  }

  if (traverse_properties)
  {
    ecma_property_header_t *prop_iter_p = ecma_get_property_list (object_p);

    if (prop_iter_p != NULL && prop_iter_p->types[0] == ECMA_PROPERTY_TYPE_HASHMAP)
    {
      prop_iter_p = ECMA_GET_POINTER (ecma_property_header_t,
                                      prop_iter_p->next_property_cp);
    }

    while (prop_iter_p != NULL)
    {
      JERRY_ASSERT (ECMA_PROPERTY_IS_PROPERTY_PAIR (prop_iter_p));

      // This branch is resolved at compile time to more efficiently execute GC runs.
      if (full_traverse)
      {
        traverse_cb (object_p,
                     (ecma_object_t *) prop_iter_p,
                     ECMA_GC_ALLOCATION_TYPE_PROPERTY_PAIR,
                     JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
                     NULL,
                     user_data_p);
        ecma_gc_traverse_property ((ecma_property_pair_t *) prop_iter_p, 0, traverse_cb, user_data_p);
        ecma_gc_traverse_property ((ecma_property_pair_t *) prop_iter_p, 1, traverse_cb, user_data_p);
      }
      else
      {
        ecma_gc_mark_property ((ecma_property_pair_t *) prop_iter_p, 0);
        ecma_gc_mark_property ((ecma_property_pair_t *) prop_iter_p, 1);
      }

      prop_iter_p = ECMA_GET_POINTER (ecma_property_header_t,
                                      prop_iter_p->next_property_cp);
    }
  }
} /* ecma_gc_traverse_inner */


/**
 * A fixed instantiation of ecma_gc_traverse_inner intended for regular GC runs.
 */
static void
ecma_gc_mark (ecma_object_t *object_p) /**< object whose descendents to mark as visited */
{
  ecma_gc_traverse_inner (object_p, ecma_gc_set_object_visited, false, NULL);
} /* ecma_gc_mark */

/**
 * A generic instantiation of ecma_gc_traverse_inner for full-heap traversal.
 */
static void
ecma_gc_traverse (ecma_object_t *object_p, /**< object whose descendents to traverse */
                  ecma_gc_traverse_callback_t traverse_cb, /**< callback to call for each descendent */
                  void *user_data_p) /**< context to pass to traverse_cb */
{
  ecma_gc_traverse_inner (object_p, traverse_cb, true, user_data_p);
} /* ecma_gc_traverse */

/**
 * Free the native handle/pointer by calling its free callback.
 */
static void
ecma_gc_free_native_pointer (ecma_property_t *property_p) /**< property */
{
  JERRY_ASSERT (property_p != NULL);

  ecma_property_value_t *value_p = ECMA_PROPERTY_VALUE_PTR (property_p);
  ecma_native_pointer_t *native_pointer_p;

  native_pointer_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_native_pointer_t,
                                                      value_p->value);

  if (native_pointer_p->info_p != NULL)
  {
    ecma_object_native_free_callback_t free_cb = native_pointer_p->info_p->free_cb;

    if (free_cb != NULL)
    {
      free_cb (native_pointer_p->data_p);
    }
  }

  jmem_heap_free_block (native_pointer_p, sizeof (ecma_native_pointer_t));
} /* ecma_gc_free_native_pointer */

/**
 * Free specified object.
 */
static void
ecma_gc_free_object (ecma_object_t *object_p) /**< object to free */
{
  JERRY_ASSERT (object_p != NULL
                && !ecma_gc_is_object_visited (object_p)
                && object_p->type_flags_refs < ECMA_OBJECT_REF_ONE);

  bool obj_is_not_lex_env = !ecma_is_lexical_environment (object_p);

  if (obj_is_not_lex_env
      || ecma_get_lex_env_type (object_p) == ECMA_LEXICAL_ENVIRONMENT_DECLARATIVE)
  {
    ecma_property_header_t *prop_iter_p = ecma_get_property_list (object_p);

    if (prop_iter_p != NULL && prop_iter_p->types[0] == ECMA_PROPERTY_TYPE_HASHMAP)
    {
      ecma_property_hashmap_free (object_p);
      prop_iter_p = ecma_get_property_list (object_p);
    }

    while (prop_iter_p != NULL)
    {
      JERRY_ASSERT (ECMA_PROPERTY_IS_PROPERTY_PAIR (prop_iter_p));

      /* Both cannot be deleted. */
      JERRY_ASSERT (prop_iter_p->types[0] != ECMA_PROPERTY_TYPE_DELETED
                    || prop_iter_p->types[1] != ECMA_PROPERTY_TYPE_DELETED);

      ecma_property_pair_t *prop_pair_p = (ecma_property_pair_t *) prop_iter_p;

      for (int i = 0; i < ECMA_PROPERTY_PAIR_ITEM_COUNT; i++)
      {
        ecma_property_t *property_p = (ecma_property_t *) (prop_iter_p->types + i);
        jmem_cpointer_t name_cp = prop_pair_p->names_cp[i];

        /* Call the native's free callback. */
        if (ECMA_PROPERTY_GET_NAME_TYPE (*property_p) == ECMA_DIRECT_STRING_MAGIC
            && (name_cp == LIT_INTERNAL_MAGIC_STRING_NATIVE_POINTER))
        {
          ecma_gc_free_native_pointer (property_p);
        }

        if (prop_iter_p->types[i] != ECMA_PROPERTY_TYPE_DELETED)
        {
          ecma_free_property (object_p, name_cp, property_p);
        }
      }

      prop_iter_p = ECMA_GET_POINTER (ecma_property_header_t,
                                      prop_iter_p->next_property_cp);

      ecma_dealloc_property_pair (prop_pair_p);
    }
  }

  JERRY_ASSERT (JERRY_CONTEXT (ecma_gc_objects_number) > 0);
  JERRY_CONTEXT (ecma_gc_objects_number)--;

  if (obj_is_not_lex_env)
  {
    ecma_object_type_t object_type = ecma_get_object_type (object_p);

    size_t ext_object_size = sizeof (ecma_extended_object_t);

    if (ecma_get_object_is_builtin (object_p))
    {
      uint8_t length_and_bitset_size;

      if (object_type == ECMA_OBJECT_TYPE_CLASS
          || object_type == ECMA_OBJECT_TYPE_ARRAY)
      {
        ext_object_size = sizeof (ecma_extended_built_in_object_t);
        length_and_bitset_size = ((ecma_extended_built_in_object_t *) object_p)->built_in.length_and_bitset_size;
      }
      else
      {
        length_and_bitset_size = ((ecma_extended_object_t *) object_p)->u.built_in.length_and_bitset_size;
      }

      ext_object_size += (2 * sizeof (uint32_t)) * (length_and_bitset_size >> ECMA_BUILT_IN_BITSET_SHIFT);
    }

    if (object_type == ECMA_OBJECT_TYPE_CLASS)
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      switch (ext_object_p->u.class_prop.class_id)
      {
        case LIT_MAGIC_STRING_STRING_UL:
        case LIT_MAGIC_STRING_NUMBER_UL:
        {
          ecma_free_value (ext_object_p->u.class_prop.u.value);
          break;
        }

        case LIT_MAGIC_STRING_DATE_UL:
        {
          ecma_number_t *num_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_number_t,
                                                                  ext_object_p->u.class_prop.u.value);
          ecma_dealloc_number (num_p);
          break;
        }

        case LIT_MAGIC_STRING_REGEXP_UL:
        {
          ecma_compiled_code_t *bytecode_p;
          bytecode_p = ECMA_GET_INTERNAL_VALUE_ANY_POINTER (ecma_compiled_code_t,
                                                            ext_object_p->u.class_prop.u.value);

          if (bytecode_p != NULL)
          {
            ecma_bytecode_deref (bytecode_p);
          }
          break;
        }
#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
        case LIT_MAGIC_STRING_ARRAY_BUFFER_UL:
        {
          ecma_length_t arraybuffer_length = ext_object_p->u.class_prop.u.length;
          size_t size;

          if (ECMA_ARRAYBUFFER_HAS_EXTERNAL_MEMORY (ext_object_p))
          {
            size = sizeof (ecma_arraybuffer_external_info);

            /* Call external free callback if any. */
            ecma_arraybuffer_external_info *array_p = (ecma_arraybuffer_external_info *) ext_object_p;
            JERRY_ASSERT (array_p != NULL);

            if (array_p->free_cb != NULL)
            {
              (array_p->free_cb) (array_p->buffer_p);
            }
          }
          else
          {
            size = sizeof (ecma_extended_object_t) + arraybuffer_length;
          }

          ecma_dealloc_extended_object (object_p, size);
          return;
        }
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
        case LIT_MAGIC_STRING_PROMISE_UL:
        {
          ecma_free_value_if_not_object (ext_object_p->u.class_prop.u.value);
          ecma_free_values_collection (((ecma_promise_object_t *) object_p)->fulfill_reactions,
                                       ECMA_COLLECTION_NO_REF_OBJECTS);
          ecma_free_values_collection (((ecma_promise_object_t *) object_p)->reject_reactions,
                                       ECMA_COLLECTION_NO_REF_OBJECTS);
          ecma_dealloc_extended_object (object_p, sizeof (ecma_promise_object_t));
          return;
        }
#endif /* !CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */
#ifndef CONFIG_DISABLE_ES2015_MAP_BUILTIN
        case LIT_MAGIC_STRING_MAP_UL:
        {
          ecma_op_map_clear_map ((ecma_map_object_t *) object_p);
          ecma_dealloc_extended_object (object_p, sizeof (ecma_map_object_t));
          return;
        }
#endif /* !CONFIG_DISABLE_ES2015_MAP_BUILTIN */
        default:
        {
          /* The undefined id represents an uninitialized class. */
          JERRY_ASSERT (ext_object_p->u.class_prop.class_id == LIT_MAGIC_STRING_UNDEFINED
                        || ext_object_p->u.class_prop.class_id == LIT_MAGIC_STRING_ARGUMENTS_UL
                        || ext_object_p->u.class_prop.class_id == LIT_MAGIC_STRING_BOOLEAN_UL
                        || ext_object_p->u.class_prop.class_id == LIT_MAGIC_STRING_ERROR_UL);
          break;
        }
      }

      ecma_dealloc_extended_object (object_p, ext_object_size);
      return;
    }

    if (ecma_get_object_is_builtin (object_p)
        || object_type == ECMA_OBJECT_TYPE_ARRAY
        || object_type == ECMA_OBJECT_TYPE_EXTERNAL_FUNCTION)
    {
      ecma_dealloc_extended_object (object_p, ext_object_size);
      return;
    }

    if (object_type == ECMA_OBJECT_TYPE_FUNCTION)
    {
      /* Function with byte-code (not a built-in function). */
      ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) object_p;

#ifdef JERRY_ENABLE_SNAPSHOT_EXEC
      if (ext_func_p->u.function.bytecode_cp != ECMA_NULL_POINTER)
      {
        ecma_bytecode_deref (ECMA_GET_INTERNAL_VALUE_POINTER (ecma_compiled_code_t,
                                                              ext_func_p->u.function.bytecode_cp));
        ecma_dealloc_extended_object (object_p, sizeof (ecma_extended_object_t));
      }
      else
      {
        ecma_dealloc_extended_object (object_p, sizeof (ecma_static_function_t));
      }
#else /* !JERRY_ENABLE_SNAPSHOT_EXEC */
      ecma_bytecode_deref (ECMA_GET_INTERNAL_VALUE_POINTER (ecma_compiled_code_t,
                                                            ext_func_p->u.function.bytecode_cp));
      ecma_dealloc_extended_object (object_p, sizeof (ecma_extended_object_t));
#endif /* JERRY_ENABLE_SNAPSHOT_EXEC */
      return;
    }

#ifndef CONFIG_DISABLE_ES2015_ARROW_FUNCTION
    if (object_type == ECMA_OBJECT_TYPE_ARROW_FUNCTION)
    {
      ecma_arrow_function_t *arrow_func_p = (ecma_arrow_function_t *) object_p;

      ecma_free_value_if_not_object (arrow_func_p->this_binding);

#ifdef JERRY_ENABLE_SNAPSHOT_EXEC
      if (arrow_func_p->bytecode_cp != ECMA_NULL_POINTER)
      {
        ecma_bytecode_deref (ECMA_GET_NON_NULL_POINTER (ecma_compiled_code_t,
                                                        arrow_func_p->bytecode_cp));
        ecma_dealloc_extended_object (object_p, sizeof (ecma_arrow_function_t));
      }
      else
      {
        ecma_dealloc_extended_object (object_p, sizeof (ecma_static_arrow_function_t));
      }
#else /* !JERRY_ENABLE_SNAPSHOT_EXEC */
      ecma_bytecode_deref (ECMA_GET_NON_NULL_POINTER (ecma_compiled_code_t,
                                                      arrow_func_p->bytecode_cp));
      ecma_dealloc_extended_object (object_p, sizeof (ecma_arrow_function_t));
#endif /* JERRY_ENABLE_SNAPSHOT_EXEC */
      return;
    }
#endif /* !CONFIG_DISABLE_ES2015_ARROW_FUNCTION */

    if (object_type == ECMA_OBJECT_TYPE_PSEUDO_ARRAY)
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      switch (ext_object_p->u.pseudo_array.type)
      {
#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
        case ECMA_PSEUDO_ARRAY_TYPEDARRAY:
        {
          ecma_dealloc_extended_object (object_p, sizeof (ecma_extended_object_t));
          return;
        }
        case ECMA_PSEUDO_ARRAY_TYPEDARRAY_WITH_INFO:
        {
          ecma_dealloc_extended_object (object_p, sizeof (ecma_extended_typedarray_object_t));
          return;
        }
#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
        default:
        {
          JERRY_ASSERT (ext_object_p->u.pseudo_array.type == ECMA_PSEUDO_ARRAY_ARGUMENTS);

          ecma_length_t formal_params_number = ext_object_p->u.pseudo_array.u1.length;
          ecma_value_t *arg_Literal_p = (ecma_value_t *) (ext_object_p + 1);

          for (ecma_length_t i = 0; i < formal_params_number; i++)
          {
            if (arg_Literal_p[i] != ECMA_VALUE_EMPTY)
            {
              ecma_string_t *name_p = ecma_get_string_from_value (arg_Literal_p[i]);
              ecma_deref_ecma_string (name_p);
            }
          }

          size_t formal_params_size = formal_params_number * sizeof (ecma_value_t);
          ecma_dealloc_extended_object (object_p, sizeof (ecma_extended_object_t) + formal_params_size);
          return;
        }
      }
    }

    if (object_type == ECMA_OBJECT_TYPE_BOUND_FUNCTION)
    {
      ecma_extended_object_t *ext_function_p = (ecma_extended_object_t *) object_p;

      ecma_value_t args_len_or_this = ext_function_p->u.bound_function.args_len_or_this;

      if (!ecma_is_value_integer_number (args_len_or_this))
      {
        ecma_free_value_if_not_object (args_len_or_this);
        ecma_dealloc_extended_object (object_p, sizeof (ecma_extended_object_t));
        return;
      }

      ecma_integer_value_t args_length = ecma_get_integer_from_value (args_len_or_this);
      ecma_value_t *args_p = (ecma_value_t *) (ext_function_p + 1);

      for (ecma_integer_value_t i = 0; i < args_length; i++)
      {
        ecma_free_value_if_not_object (args_p[i]);
      }

      size_t args_size = ((size_t) args_length) * sizeof (ecma_value_t);
      ecma_dealloc_extended_object (object_p, sizeof (ecma_extended_object_t) + args_size);
      return;
    }
  }

  ecma_dealloc_object (object_p);
} /* ecma_gc_free_object */

/**
 * Run garbage collection
 */
void
ecma_gc_run (jmem_free_unused_memory_severity_t severity) /**< gc severity */
{
  JERRY_CONTEXT (ecma_gc_new_objects) = 0;

  ecma_object_t *white_gray_objects_p = JERRY_CONTEXT (ecma_gc_objects_p);
  ecma_object_t *black_objects_p = NULL;

  ecma_object_t *obj_iter_p = white_gray_objects_p;
  ecma_object_t *obj_prev_p = NULL;

  /* Move root objects (i.e. they have global or stack references) to the black list. */
  while (obj_iter_p != NULL)
  {
    ecma_object_t *obj_next_p = ecma_gc_get_object_next (obj_iter_p);

    JERRY_ASSERT (obj_prev_p == NULL
                  || ecma_gc_get_object_next (obj_prev_p) == obj_iter_p);

    if (ecma_gc_is_object_visited (obj_iter_p))
    {
      /* Moving the object to list of marked objects. */
      if (JERRY_LIKELY (obj_prev_p != NULL))
      {
        obj_prev_p->gc_next_cp = obj_iter_p->gc_next_cp;
      }
      else
      {
        white_gray_objects_p = obj_next_p;
      }

      ecma_gc_set_object_next (obj_iter_p, black_objects_p);
      black_objects_p = obj_iter_p;
    }
    else
    {
      obj_prev_p = obj_iter_p;
    }

    obj_iter_p = obj_next_p;
  }

  /* Mark root objects. */
  obj_iter_p = black_objects_p;
  while (obj_iter_p != NULL)
  {
    ecma_gc_mark (obj_iter_p);
    obj_iter_p = ecma_gc_get_object_next (obj_iter_p);
  }

  ecma_object_t *first_root_object_p = black_objects_p;

  /* Mark non-root objects. */
  bool marked_anything_during_current_iteration;

  do
  {
    marked_anything_during_current_iteration = false;

    obj_prev_p = NULL;
    obj_iter_p = white_gray_objects_p;

    while (obj_iter_p != NULL)
    {
      ecma_object_t *obj_next_p = ecma_gc_get_object_next (obj_iter_p);

      JERRY_ASSERT (obj_prev_p == NULL
                    || ecma_gc_get_object_next (obj_prev_p) == obj_iter_p);

      if (ecma_gc_is_object_visited (obj_iter_p))
      {
        /* Moving the object to list of marked objects */
        if (JERRY_LIKELY (obj_prev_p != NULL))
        {
          obj_prev_p->gc_next_cp = obj_iter_p->gc_next_cp;
        }
        else
        {
          white_gray_objects_p = obj_next_p;
        }

        ecma_gc_set_object_next (obj_iter_p, black_objects_p);
        black_objects_p = obj_iter_p;

        ecma_gc_mark (obj_iter_p);
        marked_anything_during_current_iteration = true;
      }
      else
      {
        obj_prev_p = obj_iter_p;
      }

      obj_iter_p = obj_next_p;
    }
  }
  while (marked_anything_during_current_iteration);

  /* Sweep objects that are currently unmarked. */
  obj_iter_p = white_gray_objects_p;

  while (obj_iter_p != NULL)
  {
    ecma_object_t *obj_next_p = ecma_gc_get_object_next (obj_iter_p);

    JERRY_ASSERT (!ecma_gc_is_object_visited (obj_iter_p));

    ecma_gc_free_object (obj_iter_p);
    obj_iter_p = obj_next_p;
  }

  /* Reset the reference counter of non-root black objects. */
  obj_iter_p = black_objects_p;

  while (obj_iter_p != first_root_object_p)
  {
    /* The reference counter must be 1. */
    ecma_deref_object (obj_iter_p);
    JERRY_ASSERT (obj_iter_p->type_flags_refs < ECMA_OBJECT_REF_ONE);

    obj_iter_p = ecma_gc_get_object_next (obj_iter_p);
  }

  if (severity == JMEM_FREE_UNUSED_MEMORY_SEVERITY_HIGH)
  {
    obj_iter_p = black_objects_p;

    while (obj_iter_p != NULL)
    {
      if (!ecma_is_lexical_environment (obj_iter_p)
          || ecma_get_lex_env_type (obj_iter_p) == ECMA_LEXICAL_ENVIRONMENT_DECLARATIVE)
      {
        ecma_property_header_t *prop_iter_p = ecma_get_property_list (obj_iter_p);

        if (prop_iter_p != NULL && prop_iter_p->types[0] == ECMA_PROPERTY_TYPE_HASHMAP)
        {
          ecma_property_hashmap_free (obj_iter_p);
        }
      }

      obj_iter_p = ecma_gc_get_object_next (obj_iter_p);
    }
  }

  JERRY_CONTEXT (ecma_gc_objects_p) = black_objects_p;

#ifndef CONFIG_DISABLE_REGEXP_BUILTIN
  /* Free RegExp bytecodes stored in cache */
  re_cache_gc_run ();
#endif /* !CONFIG_DISABLE_REGEXP_BUILTIN */
} /* ecma_gc_run */

/**
 * Struct used to store user context when performing a full heap traversal
 */
typedef struct
{
  ecma_gc_traverse_callback_t traverse_cb; /**< User's traverse callback */
  void *user_data_p; /**< Context to pass to traverse_cb */
} ecma_gc_walk_heap_full_traverse_context_t;

/**
 * Callback wrapper to unwrap or enumerate certain heap structures before passing them to the user's heap-traversal
 * callback.
 */
static void
ecma_gc_walk_heap_full_traverse_cb (void *parent_p, /**< Parent of allocation */
                                    void *alloc_p, /**< Allocation */
                                    ecma_heap_gc_allocation_type_t allocation_type, /**< Type of allocation */
                                    jerry_heap_snapshot_edge_type_t edge_type, /**< Relation of parent to allocation */
                                    ecma_string_t *edge_name_p, /**< Name of edge */
                                    void *user_data_p) /**< Context to pass to traverse_cb */
{
  ecma_gc_walk_heap_full_traverse_context_t *ctx = (ecma_gc_walk_heap_full_traverse_context_t *) user_data_p;
  ctx->traverse_cb (parent_p, alloc_p, allocation_type, edge_type, edge_name_p, ctx->user_data_p);

  ecma_object_t *object_p = alloc_p;
  if (allocation_type == ECMA_GC_ALLOCATION_TYPE_OBJECT &&
      !ecma_is_lexical_environment (object_p) &&
      ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_FUNCTION &&
      !ecma_get_object_is_builtin (object_p))
  {
    const ecma_compiled_code_t *bytecode_p = ecma_op_function_get_compiled_code ((ecma_extended_object_t *) object_p);
    ctx->traverse_cb (object_p,
                      (void *) bytecode_p,
                      ECMA_GC_ALLOCATION_TYPE_BYTECODE,
                      JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
                      NULL,
                      ctx->user_data_p);
    // Set up for sourcemap handler.
    allocation_type = ECMA_GC_ALLOCATION_TYPE_BYTECODE;
    alloc_p = (void *) bytecode_p;
  }

  if (allocation_type == ECMA_GC_ALLOCATION_TYPE_BYTECODE)
  {
    ecma_compiled_code_t *bytecode_p = (ecma_compiled_code_t *) alloc_p;
    // Recurse into referenced bytecode literals.
    size_t header_size;
    uint32_t const_literal_end;
    uint32_t literal_end;
    if (bytecode_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
    {
      uint8_t *byte_p = (uint8_t *) bytecode_p;
      cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) byte_p;
      const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
      literal_end = (uint32_t) (args_p->literal_end - args_p->register_end);
      header_size = sizeof (cbc_uint16_arguments_t);
    }
    else
    {
      uint8_t *byte_p = (uint8_t *) bytecode_p;
      cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) byte_p;
      const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
      literal_end = (uint32_t) (args_p->literal_end - args_p->register_end);
      header_size = sizeof (cbc_uint8_arguments_t);
    }
    ecma_value_t *literal_start_p = (ecma_value_t *) (((uint8_t *) bytecode_p) + header_size);
    for (uint32_t i = const_literal_end; i < literal_end; i++)
    {
      size_t literal_offset = (size_t) literal_start_p[i];
      if (literal_offset != 0)
      {
        ecma_compiled_code_t *nested_bytecode_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_compiled_code_t,
                                                                                   literal_start_p[i]);
        ecma_gc_walk_heap_full_traverse_cb (bytecode_p,
                                            nested_bytecode_p,
                                            ECMA_GC_ALLOCATION_TYPE_BYTECODE,
                                            JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
                                            NULL,
                                            ctx);
      }
    }
  }
} /* ecma_gc_walk_heap_full_traverse_cb */

/**
 * Enumerate all heap allocations and their referenced off-heap allocations.
 */
void
ecma_gc_walk_heap (ecma_gc_traverse_callback_t traverse_cb, /**< callback to call for each allocation */
                   void *user_data_p) /**< context to pass to traverse_cb */
{
  /* Allocations known to the garbage collector
     We must wrap these to perform some additional traversal on certain objects */
  ecma_gc_walk_heap_full_traverse_context_t traverse_ctx =
  {
    .traverse_cb = traverse_cb,
    .user_data_p = user_data_p
  };

  ecma_object_t *obj_iter_p = JERRY_CONTEXT (ecma_gc_objects_p);
  while (obj_iter_p != NULL)
  {
    ecma_gc_walk_heap_full_traverse_cb (NULL,
                                        obj_iter_p,
                                        ECMA_GC_ALLOCATION_TYPE_OBJECT,
                                        JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
                                        NULL,
                                        &traverse_ctx);
    ecma_gc_traverse (obj_iter_p, ecma_gc_walk_heap_full_traverse_cb, &traverse_ctx);
    obj_iter_p = ecma_gc_get_object_next (obj_iter_p);
  }

  /* String literals allocated at runtime */
  ecma_lit_storage_item_t *string_list_p = JERRY_CONTEXT (string_list_first_p);
  while (string_list_p != NULL)
  {
    traverse_cb (NULL,
                 string_list_p,
                 ECMA_GC_ALLOCATION_TYPE_LIT_STORAGE,
                 JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
                 NULL,
                 user_data_p);
    for (int i = 0; i < ECMA_LIT_STORAGE_VALUE_COUNT; i++)
    {
      if (string_list_p->values[i] != JMEM_CP_NULL)
      {
        ecma_string_t *string_p;
        string_p = JMEM_CP_GET_NON_NULL_POINTER (ecma_string_t,
                                                 string_list_p->values[i]);
        traverse_cb (NULL,
                     string_p,
                     ECMA_GC_ALLOCATION_TYPE_STRING,
                     JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
                     NULL,
                     user_data_p);
      }
    }
    string_list_p = JMEM_CP_GET_POINTER (ecma_lit_storage_item_t,
                                         string_list_p->next_cp);
  }

  /* Number literals allocated at runtime */
  ecma_lit_storage_item_t *number_list_p = JERRY_CONTEXT (number_list_first_p);
  while (number_list_p != NULL)
  {
    traverse_cb (NULL,
                 number_list_p,
                 ECMA_GC_ALLOCATION_TYPE_LIT_STORAGE,
                 JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
                 NULL,
                 user_data_p);
    for (int i = 0; i < ECMA_LIT_STORAGE_VALUE_COUNT; i++)
    {
      if (number_list_p->values[i] != JMEM_CP_NULL)
      {
        /* Not a mistake to use ecma_string_t - see ecma_find_or_create_literal_number */
        ecma_string_t *string_p;
        string_p = JMEM_CP_GET_NON_NULL_POINTER (ecma_string_t,
                                                 number_list_p->values[i]);
        traverse_cb (NULL,
                     string_p,
                     ECMA_GC_ALLOCATION_TYPE_STRING,
                     JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
                     NULL,
                     user_data_p);
      }
    }

    number_list_p = JMEM_CP_GET_POINTER (ecma_lit_storage_item_t,
                                         number_list_p->next_cp);
  }

  /* String literals allocated at compile time */
  for (lit_magic_string_id_t id = 0;
       id < LIT_NON_INTERNAL_MAGIC_STRING__COUNT;
       id++)
  {
    ecma_string_t *string_p = ecma_get_magic_string (id);
    traverse_cb (NULL,
                 string_p,
                 ECMA_GC_ALLOCATION_TYPE_STRING,
                 JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
                 NULL,
                 user_data_p);
  }

  /* External string literals allocated at compile time */
  for (lit_magic_string_ex_id_t id = 0;
      id < JERRY_CONTEXT (lit_magic_string_ex_count);
      id++)
  {
    ecma_string_t *string_p = (ecma_string_t *) ECMA_CREATE_DIRECT_STRING (ECMA_DIRECT_STRING_MAGIC_EX, (uintptr_t) id);
    traverse_cb (NULL,
                 string_p,
                 ECMA_GC_ALLOCATION_TYPE_STRING,
                 JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
                 NULL,
                 user_data_p);
  }

  /* Root bytecode object (will recurse to children) */
  vm_frame_ctx_t *vm_ctx_p = JERRY_CONTEXT (vm_top_context_p);
  while (vm_ctx_p && vm_ctx_p->prev_context_p)
  {
    vm_ctx_p = vm_ctx_p->prev_context_p;
  }
  if (vm_ctx_p)
  {
    ecma_gc_walk_heap_full_traverse_cb (NULL,
                                        (void *) vm_ctx_p->bytecode_header_p,
                                        ECMA_GC_ALLOCATION_TYPE_BYTECODE,
                                        JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
                                        NULL,
                                        &traverse_ctx);
  }
} /* ecma_gc_walk_heap */

/**
 * Try to free some memory (depending on severity).
 */
void
ecma_free_unused_memory (jmem_free_unused_memory_severity_t severity) /**< severity of the request */
{
#ifdef JERRY_DEBUGGER
  while ((JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
         && JERRY_CONTEXT (debugger_byte_code_free_tail) != ECMA_NULL_POINTER)
  {
    /* Wait until all byte code is freed or the connection is aborted. */
    jerry_debugger_receive (NULL);
  }
#endif /* JERRY_DEBUGGER */

  if (severity == JMEM_FREE_UNUSED_MEMORY_SEVERITY_LOW)
  {
#ifndef CONFIG_ECMA_PROPERTY_HASHMAP_DISABLE
    if (JERRY_CONTEXT (ecma_prop_hashmap_alloc_state) > ECMA_PROP_HASHMAP_ALLOC_ON)
    {
      --JERRY_CONTEXT (ecma_prop_hashmap_alloc_state);
    }
    JERRY_CONTEXT (status_flags) &= (uint32_t) ~ECMA_STATUS_HIGH_SEV_GC;
#endif /* !CONFIG_ECMA_PROPERTY_HASHMAP_DISABLE */

    /*
     * If there is enough newly allocated objects since last GC, probably it is worthwhile to start GC now.
     * Otherwise, probability to free sufficient space is considered to be low.
     */
    size_t new_objects_share = CONFIG_ECMA_GC_NEW_OBJECTS_SHARE_TO_START_GC;

    if (JERRY_CONTEXT (ecma_gc_new_objects) * new_objects_share > JERRY_CONTEXT (ecma_gc_objects_number))
    {
      ecma_gc_run (severity);
    }
  }
  else
  {
    JERRY_ASSERT (severity == JMEM_FREE_UNUSED_MEMORY_SEVERITY_HIGH);

#ifndef CONFIG_ECMA_PROPERTY_HASHMAP_DISABLE
    if (JERRY_CONTEXT (status_flags) & ECMA_STATUS_HIGH_SEV_GC)
    {
      JERRY_CONTEXT (ecma_prop_hashmap_alloc_state) = ECMA_PROP_HASHMAP_ALLOC_MAX;
    }
    else if (JERRY_CONTEXT (ecma_prop_hashmap_alloc_state) < ECMA_PROP_HASHMAP_ALLOC_MAX)
    {
      ++JERRY_CONTEXT (ecma_prop_hashmap_alloc_state);
      JERRY_CONTEXT (status_flags) |= ECMA_STATUS_HIGH_SEV_GC;
    }
#endif /* !CONFIG_ECMA_PROPERTY_HASHMAP_DISABLE */

    /* Freeing as much memory as we currently can */
    ecma_gc_run (severity);
  }
} /* ecma_free_unused_memory */

/**
 * @}
 * @}
 */

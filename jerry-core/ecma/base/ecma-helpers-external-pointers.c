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

#include "ecma-alloc.h"
#include "ecma-array-object.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects-general.h"
#include "ecma-objects.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmahelpers Helpers for operations with ECMA data types
 * @{
 */

/**
 * Create a native pointer property to store the native pointer and its type info.
 *
 * @return true - if property was just created with specified value,
 *         false - otherwise, if property existed before the call, it's value was updated
 */
bool
ecma_create_native_pointer_property (ecma_object_t *obj_p, /**< object to create property in */
                                     void *native_p, /**< native pointer */
                                     void *info_p) /**< native pointer's type info */
{
  ecma_string_t *name_p = ecma_get_internal_string (LIT_INTERNAL_MAGIC_STRING_NATIVE_POINTER);

  if (ecma_op_object_is_fast_array (obj_p))
  {
    ecma_fast_array_convert_to_normal (obj_p);
  }

  ecma_property_t *property_p = ecma_find_named_property (obj_p, name_p);

  bool is_new = (property_p == NULL);

  ecma_native_pointer_t *native_pointer_p;

  if (property_p == NULL)
  {
    ecma_property_value_t *value_p;
    ECMA_CREATE_INTERNAL_PROPERTY (obj_p, name_p, property_p, value_p);

    native_pointer_p = jmem_heap_alloc_block (sizeof (ecma_native_pointer_t));
    ECMA_SET_INTERNAL_VALUE_POINTER (value_p->value, native_pointer_p);
  }
  else
  {
    ecma_property_value_t *value_p = ECMA_PROPERTY_VALUE_PTR (property_p);

    ecma_native_pointer_t *iter_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_native_pointer_t, value_p->value);

    /* There should be at least 1 native pointer in the chain */
    JERRY_ASSERT (iter_p != NULL);

    while (true)
    {
      if (iter_p->info_p == info_p)
      {
        /* The native info already exists -> update the corresponding data */
        iter_p->data_p = native_p;
        return false;
      }

      if (iter_p->next_p == NULL)
      {
        /* The native info does not exist -> append a new element to the chain */
        break;
      }

      iter_p = iter_p->next_p;
    }

    native_pointer_p = jmem_heap_alloc_block (sizeof (ecma_native_pointer_t));

    iter_p->next_p = native_pointer_p;
  }

  native_pointer_p->data_p = native_p;
  native_pointer_p->info_p = info_p;
  native_pointer_p->next_p = NULL;

  return is_new;
} /* ecma_create_native_pointer_property */

/**
 * Get value of native package stored in the object's property with specified identifier
 *
 * Note:
 *      property identifier should be one of the following:
 *        - LIT_INTERNAL_MAGIC_STRING_NATIVE_POINTER
 *
 * @return native pointer data if property exists
 *         NULL otherwise
 */
ecma_native_pointer_t *
ecma_get_native_pointer_value (ecma_object_t *obj_p, /**< object to get property value from */
                               void *info_p) /**< native pointer's type info */
{
  if (ecma_op_object_is_fast_array (obj_p))
  {
    /* Fast access mode array can not have native pointer properties */
    return NULL;
  }

  ecma_string_t *name_p = ecma_get_internal_string (LIT_INTERNAL_MAGIC_STRING_NATIVE_POINTER);
  ecma_property_t *property_p = ecma_find_named_property (obj_p, name_p);

  if (property_p == NULL)
  {
    return NULL;
  }

  ecma_property_value_t *value_p = ECMA_PROPERTY_VALUE_PTR (property_p);

  ecma_native_pointer_t *native_pointer_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_native_pointer_t, value_p->value);

  JERRY_ASSERT (native_pointer_p != NULL);

  while (native_pointer_p != NULL)
  {
    if (native_pointer_p->info_p == info_p)
    {
      return native_pointer_p;
    }

    native_pointer_p = native_pointer_p->next_p;
  }

  return NULL;
} /* ecma_get_native_pointer_value */

/**
 * Delete the previously set native pointer by the native type info from the specified object.
 *
 * Note:
 *      If the specified object has no matching native pointer for the given native type info
 *      the function has no effect.
 *
 * @return true - if the native pointer has been deleted succesfully
 *         false - otherwise
 */
bool
ecma_delete_native_pointer_property (ecma_object_t *obj_p, /**< object to delete property from */
                                     void *info_p) /**< native pointer's type info */
{
  if (ecma_op_object_is_fast_array (obj_p))
  {
    /* Fast access mode array can not have native pointer properties */
    return false;
  }

  ecma_string_t *name_p = ecma_get_internal_string (LIT_INTERNAL_MAGIC_STRING_NATIVE_POINTER);
  ecma_property_t *property_p = ecma_find_named_property (obj_p, name_p);

  if (property_p == NULL)
  {
    return false;
  }

  ecma_property_value_t *value_p = ECMA_PROPERTY_VALUE_PTR (property_p);

  ecma_native_pointer_t *native_pointer_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_native_pointer_t, value_p->value);
  ecma_native_pointer_t *prev_p = NULL;

  JERRY_ASSERT (native_pointer_p != NULL);

  while (native_pointer_p != NULL)
  {
    if (native_pointer_p->info_p == info_p)
    {
      if (prev_p == NULL)
      {
        if (native_pointer_p->next_p == NULL)
        {
          /* Only one native pointer property exists, so the property can be deleted as well. */
          ecma_op_general_object_delete (obj_p, name_p, false);

          jmem_heap_free_block (native_pointer_p, sizeof (ecma_native_pointer_t));
          return true;
        }
        else
        {
          /* There are at least two native pointers and the first one should be deleted.
             In this case the second element's data is copied to the head of the chain, and freed as well. */
          ecma_native_pointer_t *next_p = native_pointer_p->next_p;
          memcpy (native_pointer_p, next_p, sizeof (ecma_native_pointer_t));
          jmem_heap_free_block (next_p, sizeof (ecma_native_pointer_t));
          return true;
        }
      }
      else
      {
        /* There are at least two native pointers and not the first element should be deleted.
           In this case the current element's next element reference is copied to the previous element. */
        prev_p->next_p = native_pointer_p->next_p;
        jmem_heap_free_block (native_pointer_p, sizeof (ecma_native_pointer_t));
        return true;
      }
    }

    prev_p = native_pointer_p;
    native_pointer_p = native_pointer_p->next_p;
  }

  return false;
} /* ecma_delete_native_pointer_property */

/**
 * @}
 * @}
 */

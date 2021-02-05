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

#include "ecma-arraybuffer-object.h"
#include "ecma-builtins.h"
#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "ecma-typedarray-object.h"
#include "jcontext.h"
#include "jmem.h"

#if JERRY_BUILTIN_TYPEDARRAY

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmaarraybufferobject ECMA ArrayBuffer object related routines
 * @{
 */

/**
 * Helper function: create arraybuffer object based on the array length
 *
 * The struct of arraybuffer object:
 *   ecma_object_t
 *   extend_part
 *   data buffer
 *
 * @return ecma_object_t *
 */
ecma_object_t *
ecma_arraybuffer_new_object (uint32_t length) /**< length of the arraybuffer */
{
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_ARRAYBUFFER_PROTOTYPE);
  ecma_object_t *object_p =
    ecma_create_object (prototype_obj_p, sizeof (ecma_extended_object_t) + length, ECMA_OBJECT_TYPE_CLASS);

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;
  ext_object_p->u.class_prop.extra_info = ECMA_ARRAYBUFFER_INTERNAL_MEMORY;
  ext_object_p->u.class_prop.class_id = LIT_MAGIC_STRING_ARRAY_BUFFER_UL;
  ext_object_p->u.class_prop.u.length = length;

  lit_utf8_byte_t *buf = (lit_utf8_byte_t *) (ext_object_p + 1);
  memset (buf, 0, length);

  return object_p;
} /* ecma_arraybuffer_new_object */

/**
 * Helper function: create arraybuffer object with external buffer backing.
 *
 * The struct of external arraybuffer object:
 *   ecma_object_t
 *   extend_part
 *   arraybuffer external info part
 *
 * @return ecma_object_t *, pointer to the created ArrayBuffer object
 */
ecma_object_t *
ecma_arraybuffer_new_object_external (uint32_t length, /**< length of the buffer_p to use */
                                      void *buffer_p, /**< pointer for ArrayBuffer's buffer backing */
                                      ecma_object_native_free_callback_t free_cb) /**< buffer free callback */
{
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_ARRAYBUFFER_PROTOTYPE);
  ecma_object_t *object_p =
    ecma_create_object (prototype_obj_p, sizeof (ecma_arraybuffer_external_info), ECMA_OBJECT_TYPE_CLASS);

  ecma_arraybuffer_external_info *array_object_p = (ecma_arraybuffer_external_info *) object_p;
  array_object_p->extended_object.u.class_prop.extra_info = ECMA_ARRAYBUFFER_EXTERNAL_MEMORY;
  array_object_p->extended_object.u.class_prop.class_id = LIT_MAGIC_STRING_ARRAY_BUFFER_UL;
  array_object_p->extended_object.u.class_prop.u.length = length;

  array_object_p->buffer_p = buffer_p;
  array_object_p->free_cb = free_cb;

  return object_p;
} /* ecma_arraybuffer_new_object_external */

/**
 * ArrayBuffer object creation operation.
 *
 * See also: ES2015 24.1.1.1
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_create_arraybuffer_object (const ecma_value_t *arguments_list_p, /**< list of arguments that
                                                                          *   are passed to String constructor */
                                   uint32_t arguments_list_len) /**< length of the arguments' list */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  ecma_object_t *proto_p = ecma_op_get_prototype_from_constructor (JERRY_CONTEXT (current_new_target_p),
                                                                   ECMA_BUILTIN_ID_ARRAYBUFFER_PROTOTYPE);

  if (proto_p == NULL)
  {
    return ECMA_VALUE_ERROR;
  }

  ecma_number_t length_num = 0;

  if (arguments_list_len > 0)
  {
    if (ecma_is_value_number (arguments_list_p[0]))
    {
      length_num = ecma_get_number_from_value (arguments_list_p[0]);
    }
    else
    {
      ecma_value_t to_number_value = ecma_op_to_number (arguments_list_p[0], &length_num);

      if (ECMA_IS_VALUE_ERROR (to_number_value))
      {
        ecma_deref_object (proto_p);
        return to_number_value;
      }
    }

    if (ecma_number_is_nan (length_num))
    {
      length_num = 0;
    }

    const uint32_t maximum_size_in_byte = UINT32_MAX - sizeof (ecma_extended_object_t) - JMEM_ALIGNMENT + 1;

    if (length_num <= -1.0 || length_num > (ecma_number_t) maximum_size_in_byte + 0.5)
    {
      ecma_deref_object (proto_p);
      return ecma_raise_range_error (ECMA_ERR_MSG ("Invalid ArrayBuffer length"));
    }
  }

  uint32_t length_uint32 = ecma_number_to_uint32 (length_num);

  ecma_object_t *array_buffer = ecma_arraybuffer_new_object (length_uint32);
  ECMA_SET_NON_NULL_POINTER (array_buffer->u2.prototype_cp, proto_p);
  ecma_deref_object (proto_p);

  return ecma_make_object_value (array_buffer);
} /* ecma_op_create_arraybuffer_object */

/**
 * Helper function: check if the target is ArrayBuffer
 *
 *
 * See also: ES2015 24.1.1.4
 *
 * @return true - if value is an ArrayBuffer object
 *         false - otherwise
 */
bool
ecma_is_arraybuffer (ecma_value_t target) /**< the target value */
{
  return (ecma_is_value_object (target)
          && ecma_object_class_is (ecma_get_object_from_value (target), LIT_MAGIC_STRING_ARRAY_BUFFER_UL));
} /* ecma_is_arraybuffer */

/**
 * Helper function: return the length of the buffer inside the arraybuffer object
 *
 * @return uint32_t, the length of the arraybuffer
 */
uint32_t JERRY_ATTR_PURE
ecma_arraybuffer_get_length (ecma_object_t *object_p) /**< pointer to the ArrayBuffer object */
{
  JERRY_ASSERT (ecma_object_class_is (object_p, LIT_MAGIC_STRING_ARRAY_BUFFER_UL));

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;
  return ecma_arraybuffer_is_detached (object_p) ? 0 : ext_object_p->u.class_prop.u.length;
} /* ecma_arraybuffer_get_length */

/**
 * Helper function: return the pointer to the data buffer inside the arraybuffer object
 *
 * @return pointer to the data buffer
 */
extern inline lit_utf8_byte_t *JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_arraybuffer_get_buffer (ecma_object_t *object_p) /**< pointer to the ArrayBuffer object */
{
  JERRY_ASSERT (ecma_object_class_is (object_p, LIT_MAGIC_STRING_ARRAY_BUFFER_UL));

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

  if (ECMA_ARRAYBUFFER_HAS_EXTERNAL_MEMORY (ext_object_p))
  {
    ecma_arraybuffer_external_info *array_p = (ecma_arraybuffer_external_info *) ext_object_p;
    JERRY_ASSERT (!ecma_arraybuffer_is_detached (object_p) || array_p->buffer_p == NULL);
    return (lit_utf8_byte_t *) array_p->buffer_p;
  }
  else if (ext_object_p->u.class_prop.extra_info & ECMA_ARRAYBUFFER_DETACHED)
  {
    return NULL;
  }

  return (lit_utf8_byte_t *) (ext_object_p + 1);
} /* ecma_arraybuffer_get_buffer */

/**
 * Helper function: check if the target ArrayBuffer is detached
 *
 * @return true - if value is an detached ArrayBuffer object
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_arraybuffer_is_detached (ecma_object_t *object_p) /**< pointer to the ArrayBuffer object */
{
  JERRY_ASSERT (ecma_object_class_is (object_p, LIT_MAGIC_STRING_ARRAY_BUFFER_UL));

  return (((ecma_extended_object_t *) object_p)->u.class_prop.extra_info & ECMA_ARRAYBUFFER_DETACHED) != 0;
} /* ecma_arraybuffer_is_detached */

/**
 * ArrayBuffer object detaching operation
 *
 * See also: ES2015 24.1.1.3
 *
 * @return true - if detach op succeeded
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_arraybuffer_detach (ecma_object_t *object_p) /**< pointer to the ArrayBuffer object */
{
  JERRY_ASSERT (ecma_object_class_is (object_p, LIT_MAGIC_STRING_ARRAY_BUFFER_UL));

  if (ecma_arraybuffer_is_detached (object_p))
  {
    return false;
  }

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;
  ext_object_p->u.class_prop.extra_info |= ECMA_ARRAYBUFFER_DETACHED;

  if (ECMA_ARRAYBUFFER_HAS_EXTERNAL_MEMORY (ext_object_p))
  {
    ecma_arraybuffer_external_info *array_p = (ecma_arraybuffer_external_info *) ext_object_p;

    if (array_p->free_cb != NULL)
    {
      array_p->free_cb (array_p->buffer_p);
      array_p->free_cb = NULL;
    }

    ext_object_p->u.class_prop.u.length = 0;
    array_p->buffer_p = NULL;
  }

  return true;
} /* ecma_arraybuffer_detach */

/**
 * @}
 * @}
 */
#endif /* JERRY_BUILTIN_TYPEDARRAY */

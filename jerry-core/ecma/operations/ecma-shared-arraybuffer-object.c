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

#include "ecma-shared-arraybuffer-object.h"
#include "ecma-typedarray-object.h"
#include "ecma-objects.h"
#include "ecma-builtins.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "jmem.h"
#include "jcontext.h"
#include "ecma-function-object.h"

#if JERRY_BUILTIN_TYPEDARRAY

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmasharedarraybufferobject ECMA SharedArrayBuffer object related routines
 * @{
 */

/**
 * Helper function: create SharedArrayBuffer object based on the array length
 *
 * The struct of arraybuffer object:
 *   ecma_object_t
 *   extend_part
 *   data buffer
 *
 * @return ecma_object_t *
 */
ecma_object_t *
ecma_shared_arraybuffer_new_object (uint32_t length) /**< length of the SharedArrayBuffer */
{
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_SHARED_ARRAYBUFFER_PROTOTYPE);
  ecma_object_t *object_p = ecma_create_object (prototype_obj_p,
                                                sizeof (ecma_extended_object_t) + length,
                                                ECMA_OBJECT_TYPE_CLASS);

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;
  ext_object_p->u.cls.type = ECMA_OBJECT_CLASS_SHARED_ARRAY_BUFFER;
  ext_object_p->u.cls.u1.array_buffer_flags = ECMA_ARRAYBUFFER_INTERNAL_MEMORY;
  ext_object_p->u.cls.u3.length = length;

  lit_utf8_byte_t *buf = (lit_utf8_byte_t *) (ext_object_p + 1);
  memset (buf, 0, length);

  return object_p;
} /* ecma_shared_arraybuffer_new_object */

/**
 * Helper function: create SharedArrayBuffer object with external buffer backing.
 *
 * The struct of external arraybuffer object:
 *   ecma_object_t
 *   extend_part
 *   SharedArrayBuffer external info part
 *
 * @return ecma_object_t *, pointer to the created SharedArrayBuffer object
 */
ecma_object_t *
ecma_shared_arraybuffer_new_object_external (uint32_t length, /**< length of the buffer_p to use */
                                             void *buffer_p, /**< pointer for SharedArrayBuffer's buffer backing */
                                             jerry_value_free_callback_t free_cb) /**< buffer free callback */
{
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_SHARED_ARRAYBUFFER_PROTOTYPE);
  ecma_object_t *object_p = ecma_create_object (prototype_obj_p,
                                                sizeof (ecma_arraybuffer_external_info),
                                                ECMA_OBJECT_TYPE_CLASS);

  ecma_arraybuffer_external_info *array_object_p = (ecma_arraybuffer_external_info *) object_p;
  array_object_p->extended_object.u.cls.type = ECMA_OBJECT_CLASS_SHARED_ARRAY_BUFFER;
  array_object_p->extended_object.u.cls.u1.array_buffer_flags = ECMA_ARRAYBUFFER_EXTERNAL_MEMORY;
  array_object_p->extended_object.u.cls.u3.length = length;

  array_object_p->buffer_p = buffer_p;
  array_object_p->free_cb = free_cb;

  return object_p;
} /* ecma_shared_arraybuffer_new_object_external */

/**
 * SharedArrayBuffer object creation operation.
 *
 * See also: ES11 24.1.1.1
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_create_shared_arraybuffer_object (const ecma_value_t *arguments_list_p, /**< list of arguments that
                                                                          *   are passed to String constructor */
                                          uint32_t arguments_list_len) /**< length of the arguments' list */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  ecma_object_t *proto_p = ecma_op_get_prototype_from_constructor (JERRY_CONTEXT (current_new_target_p),
                                                                   ECMA_BUILTIN_ID_SHARED_ARRAYBUFFER_PROTOTYPE);

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
      return ecma_raise_range_error (ECMA_ERR_MSG ("Invalid Shared ArrayBuffer length"));
    }
  }

  uint32_t length_uint32 = ecma_number_to_uint32 (length_num);
  ecma_object_t *shared_array_buffer = ecma_shared_arraybuffer_new_object (length_uint32);
  ECMA_SET_NON_NULL_POINTER (shared_array_buffer->u2.prototype_cp, proto_p);
  ecma_deref_object (proto_p);

  return ecma_make_object_value (shared_array_buffer);
} /* ecma_op_create_shared_arraybuffer_object */

/**
 * Helper function: check if the target is SharedArrayBuffer
 *
 * See also: ES11 24.1.1.4
 *
 * @return true - if value is a SharedArrayBuffer object
 *         false - otherwise
 */
bool
ecma_is_shared_arraybuffer (ecma_value_t target) /**< the target value */
{
  return (ecma_is_value_object (target)
          && ecma_object_class_is (ecma_get_object_from_value (target), ECMA_OBJECT_CLASS_SHARED_ARRAY_BUFFER));
} /* ecma_is_shared_arraybuffer */

/**
 * @}
 * @}
 */
#endif /* JERRY_BUILTIN_TYPEDARRAY */

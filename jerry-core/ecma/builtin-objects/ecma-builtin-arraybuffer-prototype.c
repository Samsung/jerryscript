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
#include "ecma-builtin-helpers.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "jrt-libc-includes.h"
#include "jrt.h"

#if JERRY_BUILTIN_TYPEDARRAY

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-arraybuffer-prototype.inc.h"
#define BUILTIN_UNDERSCORED_ID  arraybuffer_prototype
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup arraybufferprototype ECMA ArrayBuffer.prototype object built-in
 * @{
 */

/**
 * The ArrayBuffer.prototype.bytelength accessor
 *
 * See also:
 *          ES2015, 24.1.4.1
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_arraybuffer_prototype_bytelength_getter (ecma_value_t this_arg) /**< this argument */
{
  if (ecma_is_value_object (this_arg))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (this_arg);

    if (ecma_object_class_is (object_p, LIT_MAGIC_STRING_ARRAY_BUFFER_UL))
    {
      if (ecma_arraybuffer_is_detached (object_p))
      {
        return ecma_raise_type_error (ECMA_ERR_MSG ("ArrayBuffer has been detached"));
      }
      uint32_t len = ecma_arraybuffer_get_length (object_p);

      return ecma_make_uint32_value (len);
    }
  }

  return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a ArrayBuffer object"));
} /* ecma_builtin_arraybuffer_prototype_bytelength_getter */

/**
 * The ArrayBuffer.prototype object's 'slice' routine
 *
 * See also:
 *          ECMA-262 v11, 24.1.4.3
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_arraybuffer_prototype_object_slice (ecma_value_t this_arg, /**< this argument */
                                                 const ecma_value_t *argument_list_p, /**< arguments list */
                                                 uint32_t arguments_number) /**< number of arguments */
{
  if (!ecma_is_value_object (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not an object"));
  }

  ecma_object_t *object_p = ecma_get_object_from_value (this_arg);

  /* 2. */
  if (!ecma_object_class_is (object_p, LIT_MAGIC_STRING_ARRAY_BUFFER_UL))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not an ArrayBuffer object"));
  }

  /* TODO: step 3. if SharedArrayBuffer will be implemented */

  /* 4. */
  if (ecma_arraybuffer_is_detached (object_p))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("ArrayBuffer has been detached"));
  }

  /* 5. */
  uint32_t len = ecma_arraybuffer_get_length (object_p);

  uint32_t start = 0;
  uint32_t end = len;

  if (arguments_number > 0)
  {
    /* 6-7. */
    if (ECMA_IS_VALUE_ERROR (ecma_builtin_helper_uint32_index_normalize (argument_list_p[0], len, &start)))
    {
      return ECMA_VALUE_ERROR;
    }

    if (arguments_number > 1 && !ecma_is_value_undefined (argument_list_p[1]))
    {
      /* 8-9 .*/
      if (ECMA_IS_VALUE_ERROR (ecma_builtin_helper_uint32_index_normalize (argument_list_p[1], len, &end)))
      {
        return ECMA_VALUE_ERROR;
      }
    }
  }

  /* 10. */
  uint32_t new_len = (end >= start) ? (end - start) : 0;

  /* 11. */
  ecma_value_t ctor = ecma_op_species_constructor (object_p, ECMA_BUILTIN_ID_ARRAYBUFFER);

  if (ECMA_IS_VALUE_ERROR (ctor))
  {
    return ctor;
  }

  /* 12. */
  ecma_object_t *ctor_obj_p = ecma_get_object_from_value (ctor);
  ecma_value_t new_len_value = ecma_make_uint32_value (new_len);

  ecma_value_t new_arraybuffer = ecma_op_function_construct (ctor_obj_p, ctor_obj_p, &new_len_value, 1);

  ecma_deref_object (ctor_obj_p);
  ecma_free_value (new_len_value);

  if (ECMA_IS_VALUE_ERROR (new_arraybuffer))
  {
    return new_arraybuffer;
  }

  ecma_object_t *new_arraybuffer_p = ecma_get_object_from_value (new_arraybuffer);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 13. */
  if (!ecma_object_class_is (new_arraybuffer_p, LIT_MAGIC_STRING_ARRAY_BUFFER_UL))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Return value is not an ArrayBuffer object"));
    goto free_new_arraybuffer;
  }

  /* TODO: step 14. if SharedArrayBuffer will be implemented */

  /* 15. */
  if (ecma_arraybuffer_is_detached (new_arraybuffer_p))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Returned ArrayBuffer has been detached"));
    goto free_new_arraybuffer;
  }

  /* 16. */
  if (ecma_op_same_value (new_arraybuffer, this_arg))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("ArrayBuffer subclass returned this from species constructor"));
    goto free_new_arraybuffer;
  }

  /* 17. */
  if (ecma_arraybuffer_get_length (new_arraybuffer_p) < new_len)
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Derived ArrayBuffer constructor created a too small buffer"));
    goto free_new_arraybuffer;
  }

  /* 19. */
  if (ecma_arraybuffer_is_detached (object_p))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Original ArrayBuffer has been detached"));
    goto free_new_arraybuffer;
  }

  /* 20. */
  lit_utf8_byte_t *old_buf = ecma_arraybuffer_get_buffer (object_p);

  /* 21. */
  lit_utf8_byte_t *new_buf = ecma_arraybuffer_get_buffer (new_arraybuffer_p);

  /* 22. */
  memcpy (new_buf, old_buf + start, new_len);

free_new_arraybuffer:
  if (ret_value != ECMA_VALUE_EMPTY)
  {
    ecma_deref_object (new_arraybuffer_p);
  }
  else
  {
    /* 23. */
    ret_value = ecma_make_object_value (new_arraybuffer_p);
  }

  return ret_value;
} /* ecma_builtin_arraybuffer_prototype_object_slice */

/**
 * @}
 * @}
 * @}
 */

#endif /* JERRY_BUILTIN_TYPEDARRAY */

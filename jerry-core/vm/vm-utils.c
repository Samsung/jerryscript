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

#include "ecma-array-object.h"
#include "ecma-helpers.h"
#include "ecma-line-info.h"
#include "jcontext.h"
#include "lit-char-helpers.h"
#include "vm.h"

/**
 * Check whether currently executed code is strict mode code
 *
 * @return true - current code is executed in strict mode,
 *         false - otherwise
 */
bool
vm_is_strict_mode (void)
{
  JERRY_ASSERT (JERRY_CONTEXT (vm_top_context_p) != NULL);

  return JERRY_CONTEXT (vm_top_context_p)->status_flags & VM_FRAME_CTX_IS_STRICT;
} /* vm_is_strict_mode */

/**
 * Check whether currently performed call (on top of call-stack) is performed in form,
 * meeting conditions of 'Direct Call to Eval' (see also: ECMA-262 v5, 15.1.2.1.1)
 *
 * Warning:
 *         the function should only be called from implementation
 *         of built-in 'eval' routine of Global object
 *
 * @return true - currently performed call is performed through 'eval' identifier,
 *                without 'this' argument,
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_ALWAYS_INLINE
vm_is_direct_eval_form_call (void)
{
  return (JERRY_CONTEXT (status_flags) & ECMA_STATUS_DIRECT_EVAL) != 0;
} /* vm_is_direct_eval_form_call */

/**
 * Get backtrace. The backtrace is an array of strings where
 * each string contains the position of the corresponding frame.
 * The array length is zero if the backtrace is not available.
 *
 * @return array ecma value
 */
ecma_value_t
vm_get_backtrace (uint32_t max_depth) /**< maximum backtrace depth, 0 = unlimited */
{
#if JERRY_LINE_INFO
  vm_frame_ctx_t *context_p = JERRY_CONTEXT (vm_top_context_p);

  if (max_depth == 0)
  {
    max_depth = UINT32_MAX;
  }

  ecma_object_t *array_p = ecma_op_new_array_object (0);
  JERRY_ASSERT (ecma_op_object_is_fast_array (array_p));
  uint32_t index = 0;

  while (context_p != NULL)
  {
    const ecma_compiled_code_t *bytecode_header_p = context_p->shared_p->bytecode_header_p;
    ecma_value_t resource_name = ecma_get_resource_name (bytecode_header_p);
    ecma_string_t *str_p = ecma_get_string_from_value (resource_name);
    ecma_stringbuilder_t builder = ecma_stringbuilder_create ();

    if (ecma_string_is_empty (str_p))
    {
      ecma_stringbuilder_append_raw (&builder, (const lit_utf8_byte_t *)"<unknown>:", 10);
    }
    else
    {
      ecma_stringbuilder_append (&builder, str_p);
      ecma_stringbuilder_append_byte (&builder, LIT_CHAR_COLON);
    }

    if (bytecode_header_p->status_flags & CBC_CODE_FLAGS_HAS_LINE_INFO)
    {
      jerry_backtrace_location_t location;
      ecma_line_info_get (ecma_compiled_code_get_line_info (bytecode_header_p),
                          (uint32_t) (context_p->byte_code_p - context_p->byte_code_start_p),
                          &location);

      ecma_string_t *line_str_p = ecma_new_ecma_string_from_uint32 (location.line);
      ecma_stringbuilder_append (&builder, line_str_p);
      ecma_deref_ecma_string (line_str_p);

      ecma_stringbuilder_append_byte (&builder, LIT_CHAR_COLON);

      line_str_p = ecma_new_ecma_string_from_uint32 (location.column);
      ecma_stringbuilder_append (&builder, line_str_p);
      ecma_deref_ecma_string (line_str_p);
    }
    else
    {
      ecma_stringbuilder_append_raw (&builder, (const lit_utf8_byte_t *)"1:1", 3);
    }

    ecma_string_t *builder_str_p = ecma_stringbuilder_finalize (&builder);
    ecma_fast_array_set_property (array_p, index, ecma_make_string_value (builder_str_p));
    ecma_deref_ecma_string (builder_str_p);

    context_p = context_p->prev_context_p;
    index++;

    if (index >= max_depth)
    {
      break;
    }
  }

  return ecma_make_object_value (array_p);
#else /* !JERRY_LINE_INFO */
  JERRY_UNUSED (max_depth);

  return ecma_make_object_value (ecma_op_new_array_object (0));
#endif /* JERRY_LINE_INFO */
} /* vm_get_backtrace */

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

#include "debugger.h"
#include "byte-code.h"
#include "ecma-array-object.h"
#include "ecma-builtin-helpers.h"
#include "ecma-conversion.h"
#include "ecma-eval.h"
#include "ecma-function-object.h"
#include "ecma-objects.h"
#include "jcontext.h"
#include "jerryscript-port.h"
#include "lit-char-helpers.h"

#if JERRY_DEBUGGER

/**
 * Incoming message: next message of string data.
 */
typedef struct
{
  uint8_t type; /**< type of the message */
} jerry_debugger_receive_uint8_data_part_t;

/**
 * The number of message types in the debugger should reflect the
 * debugger versioning.
 */
JERRY_STATIC_ASSERT (JERRY_DEBUGGER_MESSAGES_OUT_MAX_COUNT == 33 && JERRY_DEBUGGER_MESSAGES_IN_MAX_COUNT == 21
                       && JERRY_DEBUGGER_VERSION == 9,
                     debugger_version_correlates_to_message_type_count);

/**
 * Waiting for data from the client.
 */
#define JERRY_DEBUGGER_RECEIVE_DATA_MODE (JERRY_DEBUGGER_BREAKPOINT_MODE | JERRY_DEBUGGER_CLIENT_SOURCE_MODE)

/**
 * Type cast the debugger send buffer into a specific type.
 */
#define JERRY_DEBUGGER_SEND_BUFFER_AS(type, name_p) \
  type *name_p = (type *) (JERRY_CONTEXT (debugger_send_buffer_payload_p))

/**
 * Type cast the debugger receive buffer into a specific type.
 */
#define JERRY_DEBUGGER_RECEIVE_BUFFER_AS(type, name_p) type *name_p = ((type *) recv_buffer_p)

/**
 * Free all unreferenced byte code structures which
 * were not acknowledged by the debugger client.
 */
void
jerry_debugger_free_unreferenced_byte_code (void)
{
  jerry_debugger_byte_code_free_t *byte_code_free_p =
    JMEM_CP_GET_POINTER (jerry_debugger_byte_code_free_t, JERRY_CONTEXT (debugger_byte_code_free_tail));

  while (byte_code_free_p != NULL)
  {
    jerry_debugger_byte_code_free_t *prev_byte_code_free_p =
      JMEM_CP_GET_POINTER (jerry_debugger_byte_code_free_t, byte_code_free_p->prev_cp);

    jmem_heap_free_block (byte_code_free_p, ((size_t) byte_code_free_p->size) << JMEM_ALIGNMENT_LOG);

    byte_code_free_p = prev_byte_code_free_p;
  }
} /* jerry_debugger_free_unreferenced_byte_code */

/**
 * Send data over an active connection.
 *
 * @return true - if the data was sent successfully
 *         false - otherwise
 */
static bool
jerry_debugger_send (size_t message_length) /**< message length in bytes */
{
  JERRY_ASSERT (message_length <= JERRY_CONTEXT (debugger_max_send_size));

  jerry_debugger_transport_header_t *header_p = JERRY_CONTEXT (debugger_transport_header_p);
  uint8_t *payload_p = JERRY_CONTEXT (debugger_send_buffer_payload_p);

  return header_p->send (header_p, payload_p, message_length);
} /* jerry_debugger_send */

/**
 * Send backtrace.
 */
static void
jerry_debugger_send_backtrace (const uint8_t *recv_buffer_p) /**< pointer to the received data */
{
  JERRY_DEBUGGER_RECEIVE_BUFFER_AS (jerry_debugger_receive_get_backtrace_t, get_backtrace_p);

  uint32_t min_depth;
  memcpy (&min_depth, get_backtrace_p->min_depth, sizeof (uint32_t));
  uint32_t max_depth;
  memcpy (&max_depth, get_backtrace_p->max_depth, sizeof (uint32_t));

  if (max_depth == 0)
  {
    max_depth = UINT32_MAX;
  }

  if (get_backtrace_p->get_total_frame_count != 0)
  {
    JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_backtrace_total_t, backtrace_total_p);
    backtrace_total_p->type = JERRY_DEBUGGER_BACKTRACE_TOTAL;

    vm_frame_ctx_t *iter_frame_ctx_p = JERRY_CONTEXT (vm_top_context_p);
    uint32_t frame_count = 0;
    while (iter_frame_ctx_p != NULL)
    {
      if (!(iter_frame_ctx_p->shared_p->bytecode_header_p->status_flags & (CBC_CODE_FLAGS_STATIC_FUNCTION)))
      {
        frame_count++;
      }
      iter_frame_ctx_p = iter_frame_ctx_p->prev_context_p;
    }
    memcpy (backtrace_total_p->frame_count, &frame_count, sizeof (frame_count));

    jerry_debugger_send (sizeof (jerry_debugger_send_type_t) + sizeof (frame_count));
  }

  JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_backtrace_t, backtrace_p);

  backtrace_p->type = JERRY_DEBUGGER_BACKTRACE;

  vm_frame_ctx_t *frame_ctx_p = JERRY_CONTEXT (vm_top_context_p);

  size_t current_frame = 0;
  const size_t max_frame_count = JERRY_DEBUGGER_SEND_MAX (jerry_debugger_frame_t);
  const size_t max_message_size = JERRY_DEBUGGER_SEND_SIZE (max_frame_count, jerry_debugger_frame_t);

  if (min_depth <= max_depth)
  {
    uint32_t min_depth_offset = 0;

    while (frame_ctx_p != NULL && min_depth_offset < min_depth)
    {
      frame_ctx_p = frame_ctx_p->prev_context_p;
      min_depth_offset++;
    }

    while (frame_ctx_p != NULL && min_depth_offset++ < max_depth)
    {
      if (frame_ctx_p->shared_p->bytecode_header_p->status_flags
          & (CBC_CODE_FLAGS_DEBUGGER_IGNORE | CBC_CODE_FLAGS_STATIC_FUNCTION))
      {
        frame_ctx_p = frame_ctx_p->prev_context_p;
        continue;
      }

      if (current_frame >= max_frame_count)
      {
        if (!jerry_debugger_send (max_message_size))
        {
          return;
        }
        current_frame = 0;
      }

      jerry_debugger_frame_t *frame_p = backtrace_p->frames + current_frame;

      jmem_cpointer_t byte_code_cp;
      JMEM_CP_SET_NON_NULL_POINTER (byte_code_cp, frame_ctx_p->shared_p->bytecode_header_p);
      memcpy (frame_p->byte_code_cp, &byte_code_cp, sizeof (jmem_cpointer_t));

      uint32_t offset = (uint32_t) (frame_ctx_p->byte_code_p - (uint8_t *) frame_ctx_p->shared_p->bytecode_header_p);
      memcpy (frame_p->offset, &offset, sizeof (uint32_t));

      frame_ctx_p = frame_ctx_p->prev_context_p;
      current_frame++;
    }
  }

  size_t message_size = current_frame * sizeof (jerry_debugger_frame_t);

  backtrace_p->type = JERRY_DEBUGGER_BACKTRACE_END;

  jerry_debugger_send (sizeof (jerry_debugger_send_type_t) + message_size);
} /* jerry_debugger_send_backtrace */

/**
 * Send the scope chain types.
 */
static void
jerry_debugger_send_scope_chain (void)
{
  vm_frame_ctx_t *iter_frame_ctx_p = JERRY_CONTEXT (vm_top_context_p);

  const size_t max_byte_count = JERRY_DEBUGGER_SEND_MAX (uint8_t);
  const size_t max_message_size = JERRY_DEBUGGER_SEND_SIZE (max_byte_count, uint8_t);

  JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_string_t, message_type_p);
  message_type_p->type = JERRY_DEBUGGER_SCOPE_CHAIN;

  size_t buffer_pos = 0;
  bool next_func_is_local = true;
  ecma_object_t *lex_env_p = iter_frame_ctx_p->lex_env_p;

  while (true)
  {
    JERRY_ASSERT (ecma_is_lexical_environment (lex_env_p));

    if (buffer_pos == max_byte_count)
    {
      if (!jerry_debugger_send (max_message_size))
      {
        return;
      }

      buffer_pos = 0;
    }

    if (ecma_get_lex_env_type (lex_env_p) == ECMA_LEXICAL_ENVIRONMENT_DECLARATIVE)
    {
      if ((lex_env_p->type_flags_refs & ECMA_OBJECT_FLAG_BLOCK) != 0)
      {
        message_type_p->string[buffer_pos++] = JERRY_DEBUGGER_SCOPE_NON_CLOSURE;
      }
      else if (next_func_is_local)
      {
        message_type_p->string[buffer_pos++] = JERRY_DEBUGGER_SCOPE_LOCAL;
        next_func_is_local = false;
      }
      else
      {
        message_type_p->string[buffer_pos++] = JERRY_DEBUGGER_SCOPE_CLOSURE;
      }
    }
    else if (ecma_get_lex_env_type (lex_env_p) == ECMA_LEXICAL_ENVIRONMENT_THIS_OBJECT_BOUND)
    {
      if (lex_env_p->u2.outer_reference_cp == JMEM_CP_NULL)
      {
        message_type_p->string[buffer_pos++] = JERRY_DEBUGGER_SCOPE_GLOBAL;
        break;
      }
      else
      {
        message_type_p->string[buffer_pos++] = JERRY_DEBUGGER_SCOPE_WITH;
      }
    }

    JERRY_ASSERT (lex_env_p->u2.outer_reference_cp != JMEM_CP_NULL);
    lex_env_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, lex_env_p->u2.outer_reference_cp);
  }

  message_type_p->type = JERRY_DEBUGGER_SCOPE_CHAIN_END;

  jerry_debugger_send (sizeof (jerry_debugger_send_type_t) + buffer_pos);
} /* jerry_debugger_send_scope_chain */

/**
 * Get type of the scope variable property.
 * @return (jerry_debugger_scope_variable_type_t)
 */
static uint8_t
jerry_debugger_get_variable_type (ecma_value_t value) /**< input ecma value */
{
  uint8_t ret_value = JERRY_DEBUGGER_VALUE_NONE;

  if (ecma_is_value_undefined (value))
  {
    ret_value = JERRY_DEBUGGER_VALUE_UNDEFINED;
  }
  else if (ecma_is_value_null (value))
  {
    ret_value = JERRY_DEBUGGER_VALUE_NULL;
  }
  else if (ecma_is_value_boolean (value))
  {
    ret_value = JERRY_DEBUGGER_VALUE_BOOLEAN;
  }
  else if (ecma_is_value_number (value))
  {
    ret_value = JERRY_DEBUGGER_VALUE_NUMBER;
  }
  else if (ecma_is_value_string (value))
  {
    ret_value = JERRY_DEBUGGER_VALUE_STRING;
  }
  else
  {
    JERRY_ASSERT (ecma_is_value_object (value));

    if (ecma_get_object_type (ecma_get_object_from_value (value)) == ECMA_OBJECT_TYPE_ARRAY)
    {
      ret_value = JERRY_DEBUGGER_VALUE_ARRAY;
    }
    else
    {
      ret_value = ecma_op_is_callable (value) ? JERRY_DEBUGGER_VALUE_FUNCTION : JERRY_DEBUGGER_VALUE_OBJECT;
    }
  }

  JERRY_ASSERT (ret_value != JERRY_DEBUGGER_VALUE_NONE);

  return ret_value;
} /* jerry_debugger_get_variable_type */

/**
 * Helper function for jerry_debugger_send_scope_variables.
 *
 * It will copies the given scope values type, length and value into the outgoing message string.
 *
 * @param variable_type type (jerry_debugger_scope_variable_type_t)
 * @return true - if the copy was successfully
 *         false - otherwise
 */
static bool
jerry_debugger_copy_variables_to_string_message (uint8_t variable_type, /**< type */
                                                 ecma_string_t *value_str, /**< property name or value string */
                                                 jerry_debugger_send_string_t *message_string_p, /**< msg pointer */
                                                 size_t *buffer_pos) /**< string data position of the message */
{
  const size_t max_byte_count = JERRY_DEBUGGER_SEND_MAX (uint8_t);
  const size_t max_message_size = JERRY_DEBUGGER_SEND_SIZE (max_byte_count, uint8_t);

  ECMA_STRING_TO_UTF8_STRING (value_str, str_buff, str_buff_size);

  size_t str_size = 0;
  size_t str_limit = 255;
  bool result = true;

  bool type_processed = false;

  while (true)
  {
    if (*buffer_pos == max_byte_count)
    {
      if (!jerry_debugger_send (max_message_size))
      {
        result = false;
        break;
      }

      *buffer_pos = 0;
    }

    if (!type_processed)
    {
      if (variable_type != JERRY_DEBUGGER_VALUE_NONE)
      {
        message_string_p->string[*buffer_pos] = variable_type;
        *buffer_pos += 1;
      }
      type_processed = true;
      continue;
    }

    if (variable_type == JERRY_DEBUGGER_VALUE_FUNCTION)
    {
      str_size = 0; // do not copy function values
    }
    else
    {
      str_size = (str_buff_size > str_limit) ? str_limit : str_buff_size;
    }

    message_string_p->string[*buffer_pos] = (uint8_t) str_size;
    *buffer_pos += 1;
    break;
  }

  if (result)
  {
    size_t free_bytes = max_byte_count - *buffer_pos;
    const uint8_t *string_p = str_buff;

    while (str_size > free_bytes)
    {
      memcpy (message_string_p->string + *buffer_pos, string_p, free_bytes);

      if (!jerry_debugger_send (max_message_size))
      {
        result = false;
        break;
      }

      string_p += free_bytes;
      str_size -= free_bytes;
      free_bytes = max_byte_count;
      *buffer_pos = 0;
    }

    if (result)
    {
      memcpy (message_string_p->string + *buffer_pos, string_p, str_size);
      *buffer_pos += str_size;
    }
  }

  ECMA_FINALIZE_UTF8_STRING (str_buff, str_buff_size);

  return result;
} /* jerry_debugger_copy_variables_to_string_message */

/**
 * Send variables of the given scope chain level.
 */
static void
jerry_debugger_send_scope_variables (const uint8_t *recv_buffer_p) /**< pointer to the received data */
{
  JERRY_DEBUGGER_RECEIVE_BUFFER_AS (jerry_debugger_receive_get_scope_variables_t, get_scope_variables_p);

  uint32_t chain_index;
  memcpy (&chain_index, get_scope_variables_p->chain_index, sizeof (uint32_t));

  vm_frame_ctx_t *iter_frame_ctx_p = JERRY_CONTEXT (vm_top_context_p);
  ecma_object_t *lex_env_p = iter_frame_ctx_p->lex_env_p;

  while (chain_index != 0)
  {
    if (JERRY_UNLIKELY (lex_env_p->u2.outer_reference_cp == JMEM_CP_NULL))
    {
      jerry_debugger_send_type (JERRY_DEBUGGER_SCOPE_VARIABLES_END);
      return;
    }

    lex_env_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, lex_env_p->u2.outer_reference_cp);

    if ((ecma_get_lex_env_type (lex_env_p) == ECMA_LEXICAL_ENVIRONMENT_THIS_OBJECT_BOUND)
        || (ecma_get_lex_env_type (lex_env_p) == ECMA_LEXICAL_ENVIRONMENT_DECLARATIVE))
    {
      chain_index--;
    }
  }

  jmem_cpointer_t prop_iter_cp;

  if (ecma_get_lex_env_type (lex_env_p) == ECMA_LEXICAL_ENVIRONMENT_DECLARATIVE)
  {
    prop_iter_cp = lex_env_p->u1.property_list_cp;
  }
  else
  {
    JERRY_ASSERT (ecma_get_lex_env_type (lex_env_p) == ECMA_LEXICAL_ENVIRONMENT_THIS_OBJECT_BOUND);
    ecma_object_t *binding_obj_p = ecma_get_lex_env_binding_object (lex_env_p);

    if (JERRY_UNLIKELY (ecma_op_object_is_fast_array (binding_obj_p)))
    {
      ecma_fast_array_convert_to_normal (binding_obj_p);
    }

    prop_iter_cp = binding_obj_p->u1.property_list_cp;
  }

  JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_string_t, message_string_p);
  message_string_p->type = JERRY_DEBUGGER_SCOPE_VARIABLES;

  size_t buffer_pos = 0;

  while (prop_iter_cp != JMEM_CP_NULL)
  {
    ecma_property_header_t *prop_iter_p = ECMA_GET_NON_NULL_POINTER (ecma_property_header_t, prop_iter_cp);
    JERRY_ASSERT (ECMA_PROPERTY_IS_PROPERTY_PAIR (prop_iter_p));

    ecma_property_pair_t *prop_pair_p = (ecma_property_pair_t *) prop_iter_p;

    for (int i = 0; i < ECMA_PROPERTY_PAIR_ITEM_COUNT; i++)
    {
      if (ECMA_PROPERTY_IS_NAMED_PROPERTY (prop_iter_p->types[i]))
      {
        if (ECMA_PROPERTY_GET_NAME_TYPE (prop_iter_p->types[i]) == ECMA_DIRECT_STRING_MAGIC
            && prop_pair_p->names_cp[i] >= LIT_NON_INTERNAL_MAGIC_STRING__COUNT)
        {
          continue;
        }

        ecma_string_t *prop_name = ecma_string_from_property_name (prop_iter_p->types[i], prop_pair_p->names_cp[i]);

        if (!jerry_debugger_copy_variables_to_string_message (JERRY_DEBUGGER_VALUE_NONE,
                                                              prop_name,
                                                              message_string_p,
                                                              &buffer_pos))
        {
          ecma_deref_ecma_string (prop_name);
          return;
        }

        ecma_deref_ecma_string (prop_name);

        ecma_property_value_t prop_value_p = prop_pair_p->values[i];

        uint8_t variable_type = jerry_debugger_get_variable_type (prop_value_p.value);

        ecma_string_t *str_p = ecma_op_to_string (prop_value_p.value);
        JERRY_ASSERT (str_p != NULL);

        if (!jerry_debugger_copy_variables_to_string_message (variable_type, str_p, message_string_p, &buffer_pos))
        {
          ecma_deref_ecma_string (str_p);
          return;
        }

        ecma_deref_ecma_string (str_p);
      }
    }

    prop_iter_cp = prop_iter_p->next_property_cp;
  }

  message_string_p->type = JERRY_DEBUGGER_SCOPE_VARIABLES_END;
  jerry_debugger_send (sizeof (jerry_debugger_send_type_t) + buffer_pos);
} /* jerry_debugger_send_scope_variables */

/**
 * Send result of evaluated expression or throw an error.
 *
 * @return true - if execution should be resumed
 *         false - otherwise
 */
static bool
jerry_debugger_send_eval (const lit_utf8_byte_t *eval_string_p, /**< evaluated string */
                          size_t eval_string_size) /**< evaluated string size */
{
  JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED);
  JERRY_ASSERT (!(JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_VM_IGNORE));

  JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_VM_IGNORE);

  uint32_t chain_index;
  memcpy (&chain_index, eval_string_p, sizeof (uint32_t));
  uint32_t parse_opts = ECMA_PARSE_DIRECT_EVAL | (chain_index << ECMA_PARSE_CHAIN_INDEX_SHIFT);

  ecma_value_t result = ecma_op_eval_chars_buffer (eval_string_p + 5, eval_string_size - 5, parse_opts);
  JERRY_DEBUGGER_CLEAR_FLAGS (JERRY_DEBUGGER_VM_IGNORE);

  if (!ECMA_IS_VALUE_ERROR (result))
  {
    if (eval_string_p[4] != JERRY_DEBUGGER_EVAL_EVAL)
    {
      JERRY_ASSERT (eval_string_p[4] == JERRY_DEBUGGER_EVAL_THROW || eval_string_p[4] == JERRY_DEBUGGER_EVAL_ABORT);
      JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_VM_EXCEPTION_THROWN);

      /* Stop where the error is caught. */
      JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_VM_STOP);
      JERRY_CONTEXT (debugger_stop_context) = NULL;

      jcontext_raise_exception (result);
      jcontext_set_abort_flag (eval_string_p[4] == JERRY_DEBUGGER_EVAL_ABORT);

      return true;
    }

    if (!ecma_is_value_string (result))
    {
      ecma_string_t *str_p = ecma_op_to_string (result);
      ecma_value_t to_string_value = ecma_make_string_value (str_p);
      ecma_free_value (result);
      result = to_string_value;
    }
  }

  ecma_value_t message = result;
  uint8_t type = JERRY_DEBUGGER_EVAL_OK;

  if (ECMA_IS_VALUE_ERROR (result))
  {
    type = JERRY_DEBUGGER_EVAL_ERROR;
    result = JERRY_CONTEXT (error_value);

    if (ecma_is_value_object (result))
    {
      ecma_object_t *obj_p = ecma_get_object_from_value (result);
      message = ecma_op_object_find (obj_p, ecma_get_magic_string (LIT_MAGIC_STRING_MESSAGE));

      if (!ecma_is_value_string (message) || ecma_string_is_empty (ecma_get_string_from_value (message)))
      {
        ecma_free_value (message);
        lit_magic_string_id_t id = ecma_object_get_class_name (obj_p);
        ecma_free_value (result);

        const lit_utf8_byte_t *string_p = lit_get_magic_string_utf8 (id);
        jerry_debugger_send_string (JERRY_DEBUGGER_EVAL_RESULT, type, string_p, strlen ((const char *) string_p));
        return false;
      }
    }
    else
    {
      /* Primitive type. */
      ecma_string_t *str_p = ecma_op_to_string (result);
      JERRY_ASSERT (str_p != NULL);

      message = ecma_make_string_value (str_p);
    }

    ecma_free_value (result);
  }

  ecma_string_t *string_p = ecma_get_string_from_value (message);

  ECMA_STRING_TO_UTF8_STRING (string_p, buffer_p, buffer_size);
  jerry_debugger_send_string (JERRY_DEBUGGER_EVAL_RESULT, type, buffer_p, buffer_size);
  ECMA_FINALIZE_UTF8_STRING (buffer_p, buffer_size);

  ecma_free_value (message);

  return false;
} /* jerry_debugger_send_eval */

/**
 * Check received packet size.
 */
#define JERRY_DEBUGGER_CHECK_PACKET_SIZE(type)  \
  if (message_size != sizeof (type))            \
  {                                             \
    JERRY_ERROR_MSG ("Invalid message size\n"); \
    jerry_debugger_transport_close ();          \
    return false;                               \
  }

/**
 * Receive message from the client.
 *
 * @return true - if message is processed successfully
 *         false - otherwise
 */
static inline bool JERRY_ATTR_ALWAYS_INLINE
jerry_debugger_process_message (const uint8_t *recv_buffer_p, /**< pointer to the received data */
                                uint32_t message_size, /**< message size */
                                bool *resume_exec_p, /**< pointer to the resume exec flag */
                                uint8_t *expected_message_type_p, /**< message type */
                                jerry_debugger_uint8_data_t **message_data_p) /**< custom message data */
{
  /* Process the received message. */

  if (recv_buffer_p[0] >= JERRY_DEBUGGER_CONTINUE && !(JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_BREAKPOINT_MODE))
  {
    JERRY_ERROR_MSG ("Message requires breakpoint mode\n");
    jerry_debugger_transport_close ();
    return false;
  }

  if (*expected_message_type_p != 0)
  {
    JERRY_ASSERT (*expected_message_type_p == JERRY_DEBUGGER_EVAL_PART
                  || *expected_message_type_p == JERRY_DEBUGGER_CLIENT_SOURCE_PART);

    jerry_debugger_uint8_data_t *uint8_data_p = (jerry_debugger_uint8_data_t *) *message_data_p;

    if (recv_buffer_p[0] != *expected_message_type_p)
    {
      jmem_heap_free_block (uint8_data_p, uint8_data_p->uint8_size + sizeof (jerry_debugger_uint8_data_t));
      JERRY_ERROR_MSG ("Unexpected message\n");
      jerry_debugger_transport_close ();
      return false;
    }

    JERRY_DEBUGGER_RECEIVE_BUFFER_AS (jerry_debugger_receive_uint8_data_part_t, uint8_data_part_p);

    if (message_size < sizeof (jerry_debugger_receive_uint8_data_part_t) + 1)
    {
      jmem_heap_free_block (uint8_data_p, uint8_data_p->uint8_size + sizeof (jerry_debugger_uint8_data_t));
      JERRY_ERROR_MSG ("Invalid message size\n");
      jerry_debugger_transport_close ();
      return false;
    }

    uint32_t expected_data = uint8_data_p->uint8_size - uint8_data_p->uint8_offset;

    message_size -= (uint32_t) sizeof (jerry_debugger_receive_uint8_data_part_t);

    if (message_size > expected_data)
    {
      jmem_heap_free_block (uint8_data_p, uint8_data_p->uint8_size + sizeof (jerry_debugger_uint8_data_t));
      JERRY_ERROR_MSG ("Invalid message size\n");
      jerry_debugger_transport_close ();
      return false;
    }

    lit_utf8_byte_t *string_p = (lit_utf8_byte_t *) (uint8_data_p + 1);
    memcpy (string_p + uint8_data_p->uint8_offset, (lit_utf8_byte_t *) (uint8_data_part_p + 1), message_size);

    if (message_size < expected_data)
    {
      uint8_data_p->uint8_offset += message_size;
      return true;
    }

    bool result;

    if (*expected_message_type_p == JERRY_DEBUGGER_EVAL_PART)
    {
      if (jerry_debugger_send_eval (string_p, uint8_data_p->uint8_size))
      {
        *resume_exec_p = true;
      }
      result = (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED) != 0;
    }
    else
    {
      result = true;
      JERRY_DEBUGGER_CLEAR_FLAGS (JERRY_DEBUGGER_CLIENT_SOURCE_MODE);
      *resume_exec_p = true;
    }

    *expected_message_type_p = 0;
    return result;
  }

  switch (recv_buffer_p[0])
  {
    case JERRY_DEBUGGER_FREE_BYTE_CODE_CP:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_byte_code_cp_t);

      JERRY_DEBUGGER_RECEIVE_BUFFER_AS (jerry_debugger_receive_byte_code_cp_t, byte_code_p);

      jmem_cpointer_t byte_code_free_cp;
      memcpy (&byte_code_free_cp, byte_code_p->byte_code_cp, sizeof (jmem_cpointer_t));

      if (byte_code_free_cp != JERRY_CONTEXT (debugger_byte_code_free_tail))
      {
        JERRY_ERROR_MSG ("Invalid byte code free order\n");
        jerry_debugger_transport_close ();
        return false;
      }

      jerry_debugger_byte_code_free_t *byte_code_free_p =
        JMEM_CP_GET_NON_NULL_POINTER (jerry_debugger_byte_code_free_t, byte_code_free_cp);

      if (byte_code_free_p->prev_cp != ECMA_NULL_POINTER)
      {
        JERRY_CONTEXT (debugger_byte_code_free_tail) = byte_code_free_p->prev_cp;
      }
      else
      {
        JERRY_CONTEXT (debugger_byte_code_free_head) = ECMA_NULL_POINTER;
        JERRY_CONTEXT (debugger_byte_code_free_tail) = ECMA_NULL_POINTER;
      }

#if JERRY_MEM_STATS
      jmem_stats_free_byte_code_bytes (((size_t) byte_code_free_p->size) << JMEM_ALIGNMENT_LOG);
#endif /* JERRY_MEM_STATS */

      jmem_heap_free_block (byte_code_free_p, ((size_t) byte_code_free_p->size) << JMEM_ALIGNMENT_LOG);
      return true;
    }

    case JERRY_DEBUGGER_UPDATE_BREAKPOINT:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_update_breakpoint_t);

      JERRY_DEBUGGER_RECEIVE_BUFFER_AS (jerry_debugger_receive_update_breakpoint_t, update_breakpoint_p);

      jmem_cpointer_t byte_code_cp;
      memcpy (&byte_code_cp, update_breakpoint_p->byte_code_cp, sizeof (jmem_cpointer_t));
      uint8_t *byte_code_p = JMEM_CP_GET_NON_NULL_POINTER (uint8_t, byte_code_cp);

      uint32_t offset;
      memcpy (&offset, update_breakpoint_p->offset, sizeof (uint32_t));
      byte_code_p += offset;

      JERRY_ASSERT (*byte_code_p == CBC_BREAKPOINT_ENABLED || *byte_code_p == CBC_BREAKPOINT_DISABLED);

      *byte_code_p = update_breakpoint_p->is_set_breakpoint ? CBC_BREAKPOINT_ENABLED : CBC_BREAKPOINT_DISABLED;
      return true;
    }

    case JERRY_DEBUGGER_MEMSTATS:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_type_t);

      jerry_debugger_send_memstats ();
      return true;
    }

    case JERRY_DEBUGGER_STOP:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_type_t);

      JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_VM_STOP);
      JERRY_CONTEXT (debugger_stop_context) = NULL;
      *resume_exec_p = false;
      return true;
    }

    case JERRY_DEBUGGER_CONTINUE:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_type_t);

      JERRY_DEBUGGER_CLEAR_FLAGS (JERRY_DEBUGGER_VM_STOP);
      JERRY_CONTEXT (debugger_stop_context) = NULL;
      *resume_exec_p = true;
      return true;
    }

    case JERRY_DEBUGGER_STEP:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_type_t);

      JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_VM_STOP);
      JERRY_CONTEXT (debugger_stop_context) = NULL;
      *resume_exec_p = true;
      return true;
    }

    case JERRY_DEBUGGER_NEXT:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_type_t);

      JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_VM_STOP);
      JERRY_CONTEXT (debugger_stop_context) = JERRY_CONTEXT (vm_top_context_p);
      *resume_exec_p = true;
      return true;
    }

    case JERRY_DEBUGGER_FINISH:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_type_t);

      JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_VM_STOP);

      /* This will point to the current context's parent (where the function was called)
       * and in case of NULL the result will the same as in case of STEP. */
      JERRY_CONTEXT (debugger_stop_context) = JERRY_CONTEXT (vm_top_context_p->prev_context_p);
      *resume_exec_p = true;
      return true;
    }

    case JERRY_DEBUGGER_GET_BACKTRACE:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_get_backtrace_t);

      jerry_debugger_send_backtrace (recv_buffer_p);
      return true;
    }

    case JERRY_DEBUGGER_GET_SCOPE_CHAIN:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_type_t);

      jerry_debugger_send_scope_chain ();

      return true;
    }

    case JERRY_DEBUGGER_GET_SCOPE_VARIABLES:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_get_scope_variables_t);

      jerry_debugger_send_scope_variables (recv_buffer_p);

      return true;
    }

    case JERRY_DEBUGGER_EXCEPTION_CONFIG:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_exception_config_t);
      JERRY_DEBUGGER_RECEIVE_BUFFER_AS (jerry_debugger_receive_exception_config_t, exception_config_p);

      if (exception_config_p->enable == 0)
      {
        JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_VM_IGNORE_EXCEPTION);
        JERRY_DEBUG_MSG ("Stop at exception disabled\n");
      }
      else
      {
        JERRY_DEBUGGER_CLEAR_FLAGS (JERRY_DEBUGGER_VM_IGNORE_EXCEPTION);
        JERRY_DEBUG_MSG ("Stop at exception enabled\n");
      }

      return true;
    }

    case JERRY_DEBUGGER_PARSER_CONFIG:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_parser_config_t);
      JERRY_DEBUGGER_RECEIVE_BUFFER_AS (jerry_debugger_receive_parser_config_t, parser_config_p);

      if (parser_config_p->enable_wait != 0)
      {
        JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_PARSER_WAIT);
        JERRY_DEBUG_MSG ("Waiting after parsing enabled\n");
      }
      else
      {
        JERRY_DEBUGGER_CLEAR_FLAGS (JERRY_DEBUGGER_PARSER_WAIT);
        JERRY_DEBUG_MSG ("Waiting after parsing disabled\n");
      }

      return true;
    }

    case JERRY_DEBUGGER_PARSER_RESUME:
    {
      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_type_t);

      if (!(JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_PARSER_WAIT_MODE))
      {
        JERRY_ERROR_MSG ("Not in parser wait mode\n");
        jerry_debugger_transport_close ();
        return false;
      }

      JERRY_DEBUGGER_CLEAR_FLAGS (JERRY_DEBUGGER_PARSER_WAIT_MODE);
      return true;
    }

    case JERRY_DEBUGGER_EVAL:
    {
      if (message_size < sizeof (jerry_debugger_receive_eval_first_t) + 5)
      {
        JERRY_ERROR_MSG ("Invalid message size\n");
        jerry_debugger_transport_close ();
        return false;
      }

      JERRY_DEBUGGER_RECEIVE_BUFFER_AS (jerry_debugger_receive_eval_first_t, eval_first_p);

      uint32_t eval_size;
      memcpy (&eval_size, eval_first_p->eval_size, sizeof (uint32_t));

      if (eval_size <= JERRY_CONTEXT (debugger_max_receive_size) - sizeof (jerry_debugger_receive_eval_first_t))
      {
        if (eval_size != message_size - sizeof (jerry_debugger_receive_eval_first_t))
        {
          JERRY_ERROR_MSG ("Invalid message size\n");
          jerry_debugger_transport_close ();
          return false;
        }

        if (jerry_debugger_send_eval ((lit_utf8_byte_t *) (eval_first_p + 1), eval_size))
        {
          *resume_exec_p = true;
        }

        return (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED) != 0;
      }

      jerry_debugger_uint8_data_t *eval_uint8_data_p;
      size_t eval_data_size = sizeof (jerry_debugger_uint8_data_t) + eval_size;

      eval_uint8_data_p = (jerry_debugger_uint8_data_t *) jmem_heap_alloc_block (eval_data_size);

      eval_uint8_data_p->uint8_size = eval_size;
      eval_uint8_data_p->uint8_offset = (uint32_t) (message_size - sizeof (jerry_debugger_receive_eval_first_t));

      lit_utf8_byte_t *eval_string_p = (lit_utf8_byte_t *) (eval_uint8_data_p + 1);
      memcpy (eval_string_p,
              (lit_utf8_byte_t *) (eval_first_p + 1),
              message_size - sizeof (jerry_debugger_receive_eval_first_t));

      *message_data_p = eval_uint8_data_p;
      *expected_message_type_p = JERRY_DEBUGGER_EVAL_PART;

      return true;
    }

    case JERRY_DEBUGGER_CLIENT_SOURCE:
    {
      if (message_size <= sizeof (jerry_debugger_receive_client_source_first_t))
      {
        JERRY_ERROR_MSG ("Invalid message size\n");
        jerry_debugger_transport_close ();
        return false;
      }

      if (!(JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CLIENT_SOURCE_MODE))
      {
        JERRY_ERROR_MSG ("Not in client source mode\n");
        jerry_debugger_transport_close ();
        return false;
      }

      JERRY_DEBUGGER_RECEIVE_BUFFER_AS (jerry_debugger_receive_client_source_first_t, client_source_first_p);

      uint32_t client_source_size;
      memcpy (&client_source_size, client_source_first_p->code_size, sizeof (uint32_t));

      uint32_t header_size = sizeof (jerry_debugger_receive_client_source_first_t);

      if (client_source_size <= JERRY_CONTEXT (debugger_max_receive_size) - header_size
          && client_source_size != message_size - header_size)
      {
        JERRY_ERROR_MSG ("Invalid message size\n");
        jerry_debugger_transport_close ();
        return false;
      }

      jerry_debugger_uint8_data_t *client_source_data_p;
      size_t client_source_data_size = sizeof (jerry_debugger_uint8_data_t) + client_source_size;

      client_source_data_p = (jerry_debugger_uint8_data_t *) jmem_heap_alloc_block (client_source_data_size);

      client_source_data_p->uint8_size = client_source_size;
      client_source_data_p->uint8_offset =
        (uint32_t) (message_size - sizeof (jerry_debugger_receive_client_source_first_t));

      lit_utf8_byte_t *client_source_string_p = (lit_utf8_byte_t *) (client_source_data_p + 1);
      memcpy (client_source_string_p,
              (lit_utf8_byte_t *) (client_source_first_p + 1),
              message_size - sizeof (jerry_debugger_receive_client_source_first_t));

      *message_data_p = client_source_data_p;

      if (client_source_data_p->uint8_size != client_source_data_p->uint8_offset)
      {
        *expected_message_type_p = JERRY_DEBUGGER_CLIENT_SOURCE_PART;
      }
      else
      {
        JERRY_DEBUGGER_CLEAR_FLAGS (JERRY_DEBUGGER_CLIENT_SOURCE_MODE);
        *resume_exec_p = true;
      }
      return true;
    }

    case JERRY_DEBUGGER_NO_MORE_SOURCES:
    {
      if (!(JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CLIENT_SOURCE_MODE))
      {
        JERRY_ERROR_MSG ("Not in client source mode\n");
        jerry_debugger_transport_close ();
        return false;
      }

      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_type_t);

      JERRY_DEBUGGER_UPDATE_FLAGS (JERRY_DEBUGGER_CLIENT_NO_SOURCE, JERRY_DEBUGGER_CLIENT_SOURCE_MODE);

      *resume_exec_p = true;

      return true;
    }

    case JERRY_DEBUGGER_CONTEXT_RESET:
    {
      if (!(JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CLIENT_SOURCE_MODE))
      {
        JERRY_ERROR_MSG ("Not in client source mode\n");
        jerry_debugger_transport_close ();
        return false;
      }

      JERRY_DEBUGGER_CHECK_PACKET_SIZE (jerry_debugger_receive_type_t);

      JERRY_DEBUGGER_UPDATE_FLAGS (JERRY_DEBUGGER_CONTEXT_RESET_MODE, JERRY_DEBUGGER_CLIENT_SOURCE_MODE);

      *resume_exec_p = true;

      return true;
    }

    default:
    {
      JERRY_ERROR_MSG ("Unexpected message.");
      jerry_debugger_transport_close ();
      return false;
    }
  }
} /* jerry_debugger_process_message */

/**
 * Receive message from the client.
 *
 * Note:
 *   If the function returns with true, the value of
 *   JERRY_DEBUGGER_VM_STOP flag should be ignored.
 *
 * @return true - if execution should be resumed,
 *         false - otherwise
 */
bool
jerry_debugger_receive (jerry_debugger_uint8_data_t **message_data_p) /**< [out] data received from client */
{
  JERRY_ASSERT (jerry_debugger_transport_is_connected ());

  JERRY_ASSERT (message_data_p != NULL ? !!(JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_RECEIVE_DATA_MODE)
                                       : !(JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_RECEIVE_DATA_MODE));

  JERRY_CONTEXT (debugger_message_delay) = JERRY_DEBUGGER_MESSAGE_FREQUENCY;

  bool resume_exec = false;
  uint8_t expected_message_type = 0;

  while (true)
  {
    jerry_debugger_transport_receive_context_t context;
    if (!jerry_debugger_transport_receive (&context))
    {
      JERRY_ASSERT (!(JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED));
      return true;
    }

    if (context.message_p == NULL)
    {
      JERRY_CONTEXT (debugger_received_length) = (uint16_t) context.received_length;

      if (expected_message_type != 0)
      {
        jerry_debugger_transport_sleep ();
        continue;
      }

      return resume_exec;
    }

    /* Only datagram packets are supported. */
    JERRY_ASSERT (context.message_total_length > 0);

    /* The jerry_debugger_process_message function is inlined
     * so passing these arguments is essentially free. */
    if (!jerry_debugger_process_message (context.message_p,
                                         (uint32_t) context.message_length,
                                         &resume_exec,
                                         &expected_message_type,
                                         message_data_p))
    {
      JERRY_ASSERT (!(JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED));
      return true;
    }

    jerry_debugger_transport_receive_completed (&context);
  }
} /* jerry_debugger_receive */

#undef JERRY_DEBUGGER_CHECK_PACKET_SIZE

/**
 * Tell the client that a breakpoint has been hit and wait for further debugger commands.
 */
void
jerry_debugger_breakpoint_hit (uint8_t message_type) /**< message type */
{
  JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED);

  JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_breakpoint_hit_t, breakpoint_hit_p);

  breakpoint_hit_p->type = message_type;

  vm_frame_ctx_t *frame_ctx_p = JERRY_CONTEXT (vm_top_context_p);

  jmem_cpointer_t byte_code_header_cp;
  JMEM_CP_SET_NON_NULL_POINTER (byte_code_header_cp, frame_ctx_p->shared_p->bytecode_header_p);
  memcpy (breakpoint_hit_p->byte_code_cp, &byte_code_header_cp, sizeof (jmem_cpointer_t));

  uint32_t offset = (uint32_t) (frame_ctx_p->byte_code_p - (uint8_t *) frame_ctx_p->shared_p->bytecode_header_p);
  memcpy (breakpoint_hit_p->offset, &offset, sizeof (uint32_t));

  if (!jerry_debugger_send (sizeof (jerry_debugger_send_breakpoint_hit_t)))
  {
    return;
  }

  JERRY_DEBUGGER_UPDATE_FLAGS (JERRY_DEBUGGER_BREAKPOINT_MODE, JERRY_DEBUGGER_VM_EXCEPTION_THROWN);

  jerry_debugger_uint8_data_t *uint8_data = NULL;

  while (!jerry_debugger_receive (&uint8_data))
  {
    jerry_debugger_transport_sleep ();
  }

  if (uint8_data != NULL)
  {
    jmem_heap_free_block (uint8_data, uint8_data->uint8_size + sizeof (jerry_debugger_uint8_data_t));
  }

  JERRY_DEBUGGER_CLEAR_FLAGS (JERRY_DEBUGGER_BREAKPOINT_MODE);

  JERRY_CONTEXT (debugger_message_delay) = JERRY_DEBUGGER_MESSAGE_FREQUENCY;
} /* jerry_debugger_breakpoint_hit */

/**
 * Send the type signal to the client.
 */
void
jerry_debugger_send_type (jerry_debugger_header_type_t type) /**< message type */
{
  JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED);

  JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_type_t, message_type_p);

  message_type_p->type = (uint8_t) type;

  jerry_debugger_send (sizeof (jerry_debugger_send_type_t));
} /* jerry_debugger_send_type */

/**
 * Send the type signal to the client.
 *
 * @return true - if the data sent successfully to the debugger client,
 *         false - otherwise
 */
bool
jerry_debugger_send_configuration (uint8_t max_message_size) /**< maximum message size */
{
  JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_configuration_t, configuration_p);

  /* Helper structure for endianness check. */
  union
  {
    uint16_t uint16_value; /**< a 16-bit value */
    uint8_t uint8_value[2]; /**< lower and upper byte of a 16-bit value */
  } endian_data;

  endian_data.uint16_value = 1;

  configuration_p->type = JERRY_DEBUGGER_CONFIGURATION;
  configuration_p->configuration = 0;

  if (endian_data.uint8_value[0] == 1)
  {
    configuration_p->configuration |= (uint8_t) JERRY_DEBUGGER_LITTLE_ENDIAN;
  }

  uint32_t version = JERRY_DEBUGGER_VERSION;
  memcpy (configuration_p->version, &version, sizeof (uint32_t));

  configuration_p->max_message_size = max_message_size;
  configuration_p->cpointer_size = sizeof (jmem_cpointer_t);

  return jerry_debugger_send (sizeof (jerry_debugger_send_configuration_t));
} /* jerry_debugger_send_configuration */

/**
 * Send raw data to the debugger client.
 */
void
jerry_debugger_send_data (jerry_debugger_header_type_t type, /**< message type */
                          const void *data, /**< raw data */
                          size_t size) /**< size of data */
{
  JERRY_ASSERT (size <= JERRY_DEBUGGER_SEND_MAX (uint8_t));

  JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_type_t, message_type_p);

  message_type_p->type = (uint8_t) type;
  memcpy (message_type_p + 1, data, size);

  jerry_debugger_send (sizeof (jerry_debugger_send_type_t) + size);
} /* jerry_debugger_send_data */

/**
 * Send string to the debugger client.
 *
 * @return true - if the data sent successfully to the debugger client,
 *         false - otherwise
 */
bool
jerry_debugger_send_string (uint8_t message_type, /**< message type */
                            uint8_t sub_type, /**< subtype of the string */
                            const uint8_t *string_p, /**< string data */
                            size_t string_length) /**< length of string */
{
  JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED);

  const size_t max_byte_count = JERRY_DEBUGGER_SEND_MAX (uint8_t);
  const size_t max_message_size = JERRY_DEBUGGER_SEND_SIZE (max_byte_count, uint8_t);

  JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_string_t, message_string_p);

  message_string_p->type = message_type;

  if (sub_type != JERRY_DEBUGGER_NO_SUBTYPE)
  {
    string_length += 1;
  }

  while (string_length > max_byte_count)
  {
    memcpy (message_string_p->string, string_p, max_byte_count);

    if (!jerry_debugger_send (max_message_size))
    {
      return false;
    }

    string_length -= max_byte_count;
    string_p += max_byte_count;
  }

  message_string_p->type = (uint8_t) (message_type + 1);

  if (sub_type != JERRY_DEBUGGER_NO_SUBTYPE)
  {
    memcpy (message_string_p->string, string_p, string_length - 1);
    message_string_p->string[string_length - 1] = sub_type;
  }
  else
  {
    memcpy (message_string_p->string, string_p, string_length);
  }

  return jerry_debugger_send (sizeof (jerry_debugger_send_type_t) + string_length);
} /* jerry_debugger_send_string */

/**
 * Send the function compressed pointer to the debugger client.
 *
 * @return true - if the data was sent successfully to the debugger client,
 *         false - otherwise
 */
bool
jerry_debugger_send_function_cp (jerry_debugger_header_type_t type, /**< message type */
                                 ecma_compiled_code_t *compiled_code_p) /**< byte code pointer */
{
  JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED);

  JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_byte_code_cp_t, byte_code_cp_p);

  byte_code_cp_p->type = (uint8_t) type;

  jmem_cpointer_t compiled_code_cp;
  JMEM_CP_SET_NON_NULL_POINTER (compiled_code_cp, compiled_code_p);
  memcpy (byte_code_cp_p->byte_code_cp, &compiled_code_cp, sizeof (jmem_cpointer_t));

  return jerry_debugger_send (sizeof (jerry_debugger_send_byte_code_cp_t));
} /* jerry_debugger_send_function_cp */

/**
 * Send function data to the debugger client.
 *
 * @return true - if the data sent successfully to the debugger client,
 *         false - otherwise
 */
bool
jerry_debugger_send_parse_function (uint32_t line, /**< line */
                                    uint32_t column) /**< column */
{
  JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED);

  JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_parse_function_t, message_parse_function_p);

  message_parse_function_p->type = JERRY_DEBUGGER_PARSE_FUNCTION;
  memcpy (message_parse_function_p->line, &line, sizeof (uint32_t));
  memcpy (message_parse_function_p->column, &column, sizeof (uint32_t));

  return jerry_debugger_send (sizeof (jerry_debugger_send_parse_function_t));
} /* jerry_debugger_send_parse_function */

/**
 * Send memory statistics to the debugger client.
 */
void
jerry_debugger_send_memstats (void)
{
  JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED);

  JERRY_DEBUGGER_SEND_BUFFER_AS (jerry_debugger_send_memstats_t, memstats_p);

  memstats_p->type = JERRY_DEBUGGER_MEMSTATS_RECEIVE;

#if JERRY_MEM_STATS /* if memory statistics feature is enabled */
  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  uint32_t allocated_bytes = (uint32_t) heap_stats->allocated_bytes;
  memcpy (memstats_p->allocated_bytes, &allocated_bytes, sizeof (uint32_t));
  uint32_t byte_code_bytes = (uint32_t) heap_stats->byte_code_bytes;
  memcpy (memstats_p->byte_code_bytes, &byte_code_bytes, sizeof (uint32_t));
  uint32_t string_bytes = (uint32_t) heap_stats->string_bytes;
  memcpy (memstats_p->string_bytes, &string_bytes, sizeof (uint32_t));
  uint32_t object_bytes = (uint32_t) heap_stats->object_bytes;
  memcpy (memstats_p->object_bytes, &object_bytes, sizeof (uint32_t));
  uint32_t property_bytes = (uint32_t) heap_stats->property_bytes;
  memcpy (memstats_p->property_bytes, &property_bytes, sizeof (uint32_t));
#else /* !JERRY_MEM_STATS if not, just put zeros */
  memset (memstats_p->allocated_bytes, 0, sizeof (uint32_t));
  memset (memstats_p->byte_code_bytes, 0, sizeof (uint32_t));
  memset (memstats_p->string_bytes, 0, sizeof (uint32_t));
  memset (memstats_p->object_bytes, 0, sizeof (uint32_t));
  memset (memstats_p->property_bytes, 0, sizeof (uint32_t));
#endif /* JERRY_MEM_STATS */

  jerry_debugger_send (sizeof (jerry_debugger_send_memstats_t));
} /* jerry_debugger_send_memstats */

/*
 * Converts an standard error into a string.
 *
 * @return standard error string
 */
static ecma_string_t *
jerry_debugger_exception_object_to_string (ecma_value_t exception_obj_value) /**< exception object */
{
  ecma_object_t *object_p = ecma_get_object_from_value (exception_obj_value);

  jmem_cpointer_t prototype_cp = object_p->u2.prototype_cp;

  if (prototype_cp == JMEM_CP_NULL)
  {
    return NULL;
  }

  ecma_object_t *prototype_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, prototype_cp);

  if (ecma_get_object_type (prototype_p) != ECMA_OBJECT_TYPE_GENERAL || !ecma_get_object_is_builtin (prototype_p))
  {
    return NULL;
  }

  lit_magic_string_id_t string_id;

  switch (((ecma_extended_object_t *) prototype_p)->u.built_in.id)
  {
#if JERRY_BUILTIN_ERRORS
    case ECMA_BUILTIN_ID_EVAL_ERROR_PROTOTYPE:
    {
      string_id = LIT_MAGIC_STRING_EVAL_ERROR_UL;
      break;
    }
    case ECMA_BUILTIN_ID_RANGE_ERROR_PROTOTYPE:
    {
      string_id = LIT_MAGIC_STRING_RANGE_ERROR_UL;
      break;
    }
    case ECMA_BUILTIN_ID_REFERENCE_ERROR_PROTOTYPE:
    {
      string_id = LIT_MAGIC_STRING_REFERENCE_ERROR_UL;
      break;
    }
    case ECMA_BUILTIN_ID_SYNTAX_ERROR_PROTOTYPE:
    {
      string_id = LIT_MAGIC_STRING_SYNTAX_ERROR_UL;
      break;
    }
    case ECMA_BUILTIN_ID_TYPE_ERROR_PROTOTYPE:
    {
      string_id = LIT_MAGIC_STRING_TYPE_ERROR_UL;
      break;
    }
    case ECMA_BUILTIN_ID_URI_ERROR_PROTOTYPE:
    {
      string_id = LIT_MAGIC_STRING_URI_ERROR_UL;
      break;
    }
#endif /* JERRY_BUILTIN_ERRORS */
    case ECMA_BUILTIN_ID_ERROR_PROTOTYPE:
    {
      string_id = LIT_MAGIC_STRING_ERROR_UL;
      break;
    }
    default:
    {
      return NULL;
    }
  }

  ecma_stringbuilder_t builder = ecma_stringbuilder_create ();

  ecma_stringbuilder_append_magic (&builder, string_id);

  ecma_property_t *property_p = ecma_find_named_property (ecma_get_object_from_value (exception_obj_value),
                                                          ecma_get_magic_string (LIT_MAGIC_STRING_MESSAGE));

  if (property_p == NULL || !(*property_p & ECMA_PROPERTY_FLAG_DATA))
  {
    return ecma_stringbuilder_finalize (&builder);
  }

  ecma_property_value_t *prop_value_p = ECMA_PROPERTY_VALUE_PTR (property_p);

  if (!ecma_is_value_string (prop_value_p->value))
  {
    return ecma_stringbuilder_finalize (&builder);
  }

  ecma_stringbuilder_append_byte (&builder, LIT_CHAR_COLON);
  ecma_stringbuilder_append_byte (&builder, LIT_CHAR_SP);
  ecma_stringbuilder_append (&builder, ecma_get_string_from_value (prop_value_p->value));

  return ecma_stringbuilder_finalize (&builder);
} /* jerry_debugger_exception_object_to_string */

/**
 * Send string representation of exception to the client.
 *
 * @return true - if the data sent successfully to the debugger client,
 *         false - otherwise
 */
bool
jerry_debugger_send_exception_string (ecma_value_t exception_value)
{
  JERRY_ASSERT (jcontext_has_pending_exception ());
  ecma_string_t *string_p = NULL;

  if (ecma_is_value_object (exception_value))
  {
    string_p = jerry_debugger_exception_object_to_string (exception_value);

    if (string_p == NULL)
    {
      string_p = ecma_get_string_from_value (ecma_builtin_helper_object_to_string (exception_value));
    }
  }
  else if (ecma_is_value_string (exception_value))
  {
    string_p = ecma_get_string_from_value (exception_value);
    ecma_ref_ecma_string (string_p);
  }
  else
  {
    string_p = ecma_op_to_string (exception_value);
  }

  ECMA_STRING_TO_UTF8_STRING (string_p, string_data_p, string_size);

  bool result =
    jerry_debugger_send_string (JERRY_DEBUGGER_EXCEPTION_STR, JERRY_DEBUGGER_NO_SUBTYPE, string_data_p, string_size);

  ECMA_FINALIZE_UTF8_STRING (string_data_p, string_size);

  ecma_deref_ecma_string (string_p);
  return result;
} /* jerry_debugger_send_exception_string */

#endif /* JERRY_DEBUGGER */

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

#include "js-parser-internal.h"
#include "ecma-helpers.h"

#if JERRY_PARSER

#if JERRY_LINE_INFO
#include "jcontext.h"
#endif /* JERRY_LINE_INFO */

/** \addtogroup parser Parser
 * @{
 *
 * \addtogroup jsparser JavaScript
 * @{
 *
 * \addtogroup jsparser_utils Utility
 * @{
 */

/**********************************************************************/
/* Emitting byte codes                                                */
/**********************************************************************/

/**
 * Append two bytes to the cbc stream.
 */
static void
parser_emit_two_bytes (parser_context_t *context_p, /**< context */
                       uint8_t first_byte, /**< first byte */
                       uint8_t second_byte) /**< second byte */
{
  uint32_t last_position = context_p->byte_code.last_position;

  if (last_position + 2 <= PARSER_CBC_STREAM_PAGE_SIZE)
  {
    parser_mem_page_t *page_p = context_p->byte_code.last_p;

    page_p->bytes[last_position] = first_byte;
    page_p->bytes[last_position + 1] = second_byte;
    context_p->byte_code.last_position = last_position + 2;
  }
  else if (last_position >= PARSER_CBC_STREAM_PAGE_SIZE)
  {
    parser_mem_page_t *page_p;

    parser_cbc_stream_alloc_page (context_p, &context_p->byte_code);
    page_p = context_p->byte_code.last_p;
    page_p->bytes[0] = first_byte;
    page_p->bytes[1] = second_byte;
    context_p->byte_code.last_position = 2;
  }
  else
  {
    context_p->byte_code.last_p->bytes[PARSER_CBC_STREAM_PAGE_SIZE - 1] = first_byte;
    parser_cbc_stream_alloc_page (context_p, &context_p->byte_code);
    context_p->byte_code.last_p->bytes[0] = second_byte;
    context_p->byte_code.last_position = 1;
  }
} /* parser_emit_two_bytes */

/**
 * Append byte to the end of the current byte code stream.
 *
 * @param context_p parser context
 * @param byte byte
 */
#define PARSER_APPEND_TO_BYTE_CODE(context_p, byte) \
  if ((context_p)->byte_code.last_position >= PARSER_CBC_STREAM_PAGE_SIZE) \
  { \
    parser_cbc_stream_alloc_page ((context_p), &(context_p)->byte_code); \
  } \
  (context_p)->byte_code.last_p->bytes[(context_p)->byte_code.last_position++] = (uint8_t) (byte)

#if JERRY_PARSER_DUMP_BYTE_CODE

/**
 * Print literal corresponding to the current index
 */
static void
parser_print_literal (parser_context_t *context_p, /**< context */
                      uint16_t literal_index) /**< index of literal */
{
  parser_scope_stack_t *scope_stack_p = context_p->scope_stack_p;
  parser_scope_stack_t *scope_stack_end_p = scope_stack_p + context_p->scope_stack_top;
  bool in_scope_literal = false;

  while (scope_stack_p < scope_stack_end_p)
  {
    scope_stack_end_p--;

    if (scope_stack_end_p->map_from == PARSER_SCOPE_STACK_FUNC)
    {
      if (literal_index == scope_stack_end_p->map_to)
      {
        in_scope_literal = true;
        break;
      }
    }
    else if (literal_index == scanner_decode_map_to (scope_stack_end_p))
    {
      in_scope_literal = true;
      break;
    }
  }

  if (literal_index < PARSER_REGISTER_START)
  {
    JERRY_DEBUG_MSG (in_scope_literal ? " IDX:%d->" : " idx:%d->", literal_index);
    lexer_literal_t *literal_p = PARSER_GET_LITERAL (literal_index);
    util_print_literal (literal_p);
    return;
  }

  if (!in_scope_literal)
  {
    JERRY_DEBUG_MSG (" reg:%d", (int) (literal_index - PARSER_REGISTER_START));
    return;
  }

  JERRY_DEBUG_MSG (" REG:%d->", (int) (literal_index - PARSER_REGISTER_START));

  lexer_literal_t *literal_p = PARSER_GET_LITERAL (scope_stack_end_p->map_from);
  util_print_literal (literal_p);
} /* parser_print_literal */

#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

/**
 * Append the current byte code to the stream
 */
void
parser_flush_cbc (parser_context_t *context_p) /**< context */
{
  uint8_t flags;
  uint16_t last_opcode = context_p->last_cbc_opcode;

  if (last_opcode == PARSER_CBC_UNAVAILABLE)
  {
    return;
  }

  context_p->status_flags |= PARSER_NO_END_LABEL;

  if (PARSER_IS_BASIC_OPCODE (last_opcode))
  {
    cbc_opcode_t opcode = (cbc_opcode_t) last_opcode;

    JERRY_ASSERT (opcode < CBC_END);
    flags = cbc_flags[opcode];

    PARSER_APPEND_TO_BYTE_CODE (context_p, opcode);
    context_p->byte_code_size++;
  }
  else
  {
    cbc_ext_opcode_t opcode = (cbc_ext_opcode_t) PARSER_GET_EXT_OPCODE (last_opcode);

    JERRY_ASSERT (opcode < CBC_EXT_END);
    flags = cbc_ext_flags[opcode];
    parser_emit_two_bytes (context_p, CBC_EXT_OPCODE, (uint8_t) opcode);
    context_p->byte_code_size += 2;
  }

  JERRY_ASSERT ((flags >> CBC_STACK_ADJUST_SHIFT) >= CBC_STACK_ADJUST_BASE
                 || (CBC_STACK_ADJUST_BASE - (flags >> CBC_STACK_ADJUST_SHIFT)) <= context_p->stack_depth);
  PARSER_PLUS_EQUAL_U16 (context_p->stack_depth, CBC_STACK_ADJUST_VALUE (flags));

  if (flags & (CBC_HAS_LITERAL_ARG | CBC_HAS_LITERAL_ARG2))
  {
    uint16_t literal_index = context_p->last_cbc.literal_index;

    parser_emit_two_bytes (context_p,
                           (uint8_t) (literal_index & 0xff),
                           (uint8_t) (literal_index >> 8));
    context_p->byte_code_size += 2;
  }

  if (flags & CBC_HAS_LITERAL_ARG2)
  {
    uint16_t literal_index = context_p->last_cbc.value;

    parser_emit_two_bytes (context_p,
                           (uint8_t) (literal_index & 0xff),
                           (uint8_t) (literal_index >> 8));
    context_p->byte_code_size += 2;

    if (!(flags & CBC_HAS_LITERAL_ARG))
    {
      literal_index = context_p->last_cbc.third_literal_index;

      parser_emit_two_bytes (context_p,
                             (uint8_t) (literal_index & 0xff),
                             (uint8_t) (literal_index >> 8));
      context_p->byte_code_size += 2;
    }
  }

  if (flags & CBC_HAS_BYTE_ARG)
  {
    uint8_t byte_argument = (uint8_t) context_p->last_cbc.value;

    JERRY_ASSERT (context_p->last_cbc.value <= CBC_MAXIMUM_BYTE_VALUE);

    if (flags & CBC_POP_STACK_BYTE_ARG)
    {
      JERRY_ASSERT (context_p->stack_depth >= byte_argument);
      PARSER_MINUS_EQUAL_U16 (context_p->stack_depth, byte_argument);
    }

    PARSER_APPEND_TO_BYTE_CODE (context_p, byte_argument);
    context_p->byte_code_size++;
  }

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
    JERRY_DEBUG_MSG ("  [%3d] %s",
                     (int) context_p->stack_depth,
                     PARSER_IS_BASIC_OPCODE (last_opcode) ? cbc_names[last_opcode]
                                                          : cbc_ext_names[PARSER_GET_EXT_OPCODE (last_opcode)]);

    if (flags & (CBC_HAS_LITERAL_ARG | CBC_HAS_LITERAL_ARG2))
    {
      parser_print_literal (context_p, context_p->last_cbc.literal_index);
    }

    if (flags & CBC_HAS_LITERAL_ARG2)
    {
      parser_print_literal (context_p, context_p->last_cbc.value);

      if (!(flags & CBC_HAS_LITERAL_ARG))
      {
        parser_print_literal (context_p, context_p->last_cbc.third_literal_index);
      }
    }

    if (flags & CBC_HAS_BYTE_ARG)
    {
      if (last_opcode == CBC_PUSH_NUMBER_POS_BYTE
          || last_opcode == CBC_PUSH_LITERAL_PUSH_NUMBER_POS_BYTE)
      {
        JERRY_DEBUG_MSG (" number:%d", (int) context_p->last_cbc.value + 1);
      }
      else if (last_opcode == CBC_PUSH_NUMBER_NEG_BYTE
               || last_opcode == CBC_PUSH_LITERAL_PUSH_NUMBER_NEG_BYTE)
      {
        JERRY_DEBUG_MSG (" number:%d", -((int) context_p->last_cbc.value + 1));
      }
      else
      {
        JERRY_DEBUG_MSG (" byte_arg:%d", (int) context_p->last_cbc.value);
      }
    }

    JERRY_DEBUG_MSG ("\n");
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  if (context_p->stack_depth > context_p->stack_limit)
  {
    context_p->stack_limit = context_p->stack_depth;
    if (context_p->stack_limit > PARSER_MAXIMUM_STACK_LIMIT)
    {
      parser_raise_error (context_p, PARSER_ERR_STACK_LIMIT_REACHED);
    }
  }

  context_p->last_cbc_opcode = PARSER_CBC_UNAVAILABLE;
} /* parser_flush_cbc */

/**
 * Append a byte code
 */
void
parser_emit_cbc (parser_context_t *context_p, /**< context */
                 uint16_t opcode) /**< opcode */
{
  JERRY_ASSERT (PARSER_ARGS_EQ (opcode, 0));

  if (context_p->last_cbc_opcode != PARSER_CBC_UNAVAILABLE)
  {
    parser_flush_cbc (context_p);
  }

  context_p->last_cbc_opcode = opcode;
} /* parser_emit_cbc */

/**
 * Append a byte code with a literal argument
 */
void
parser_emit_cbc_literal (parser_context_t *context_p, /**< context */
                         uint16_t opcode, /**< opcode */
                         uint16_t literal_index) /**< literal index */
{
  JERRY_ASSERT (PARSER_ARGS_EQ (opcode, CBC_HAS_LITERAL_ARG));

  if (context_p->last_cbc_opcode != PARSER_CBC_UNAVAILABLE)
  {
    parser_flush_cbc (context_p);
  }

  context_p->last_cbc_opcode = opcode;
  context_p->last_cbc.literal_index = literal_index;
  context_p->last_cbc.literal_type = LEXER_UNUSED_LITERAL;
  context_p->last_cbc.literal_keyword_type = LEXER_EOS;
} /* parser_emit_cbc_literal */

/**
 * Append a byte code with a literal and value argument
 */
void
parser_emit_cbc_literal_value (parser_context_t *context_p, /**< context */
                               uint16_t opcode, /**< opcode */
                               uint16_t literal_index, /**< literal index */
                               uint16_t value) /**< value */
{
  JERRY_ASSERT (PARSER_ARGS_EQ (opcode, CBC_HAS_LITERAL_ARG | CBC_HAS_LITERAL_ARG2));

  if (context_p->last_cbc_opcode != PARSER_CBC_UNAVAILABLE)
  {
    parser_flush_cbc (context_p);
  }

  context_p->last_cbc_opcode = opcode;
  context_p->last_cbc.literal_index = literal_index;
  context_p->last_cbc.literal_type = LEXER_UNUSED_LITERAL;
  context_p->last_cbc.literal_keyword_type = LEXER_EOS;
  context_p->last_cbc.value = value;
} /* parser_emit_cbc_literal_value */

/**
 * Append a byte code with the current literal argument
 */
void
parser_emit_cbc_literal_from_token (parser_context_t *context_p, /**< context */
                                    uint16_t opcode) /**< opcode */
{
  JERRY_ASSERT (PARSER_ARGS_EQ (opcode, CBC_HAS_LITERAL_ARG));

  if (context_p->last_cbc_opcode != PARSER_CBC_UNAVAILABLE)
  {
    parser_flush_cbc (context_p);
  }

  context_p->last_cbc_opcode = opcode;
  context_p->last_cbc.literal_index = context_p->lit_object.index;
  context_p->last_cbc.literal_type = context_p->token.lit_location.type;
  context_p->last_cbc.literal_keyword_type = context_p->token.keyword_type;
} /* parser_emit_cbc_literal_from_token */

/**
 * Append a byte code with a call argument
 */
void
parser_emit_cbc_call (parser_context_t *context_p, /**< context */
                      uint16_t opcode, /**< opcode */
                      size_t call_arguments) /**< number of arguments */
{
  JERRY_ASSERT (PARSER_ARGS_EQ (opcode, CBC_HAS_BYTE_ARG));
  JERRY_ASSERT (call_arguments <= CBC_MAXIMUM_BYTE_VALUE);

  if (context_p->last_cbc_opcode != PARSER_CBC_UNAVAILABLE)
  {
    parser_flush_cbc (context_p);
  }

  context_p->last_cbc_opcode = opcode;
  context_p->last_cbc.value = (uint16_t) call_arguments;
} /* parser_emit_cbc_call */

/**
 * Append a push number 1/2 byte code
 */
void
parser_emit_cbc_push_number (parser_context_t *context_p, /**< context */
                             bool is_negative_number) /**< sign is negative */
{
  uint16_t value = context_p->lit_object.index;
  uint16_t lit_value = PARSER_INVALID_LITERAL_INDEX;

  if (context_p->last_cbc_opcode != PARSER_CBC_UNAVAILABLE)
  {
    if (context_p->last_cbc_opcode == CBC_PUSH_LITERAL)
    {
      lit_value = context_p->last_cbc.literal_index;
    }
    else
    {
      if (context_p->last_cbc_opcode == CBC_PUSH_TWO_LITERALS)
      {
        context_p->last_cbc_opcode = CBC_PUSH_LITERAL;
        lit_value = context_p->last_cbc.value;
      }
      else if (context_p->last_cbc_opcode == CBC_PUSH_THREE_LITERALS)
      {
        context_p->last_cbc_opcode = CBC_PUSH_TWO_LITERALS;
        lit_value = context_p->last_cbc.third_literal_index;
      }

      parser_flush_cbc (context_p);
    }
  }

  if (value == 0)
  {
    if (lit_value == PARSER_INVALID_LITERAL_INDEX)
    {
      context_p->last_cbc_opcode = CBC_PUSH_NUMBER_0;
      return;
    }

    context_p->last_cbc_opcode = CBC_PUSH_LITERAL_PUSH_NUMBER_0;
    context_p->last_cbc.literal_index = lit_value;
    return;
  }

  uint16_t opcode;

  if (lit_value == PARSER_INVALID_LITERAL_INDEX)
  {
    opcode = (is_negative_number ? CBC_PUSH_NUMBER_NEG_BYTE
                                 : CBC_PUSH_NUMBER_POS_BYTE);

    JERRY_ASSERT (CBC_STACK_ADJUST_VALUE (PARSER_GET_FLAGS (opcode)) == 1);
  }
  else
  {
    opcode = (is_negative_number ? CBC_PUSH_LITERAL_PUSH_NUMBER_NEG_BYTE
                                 : CBC_PUSH_LITERAL_PUSH_NUMBER_POS_BYTE);
    JERRY_ASSERT (CBC_STACK_ADJUST_VALUE (PARSER_GET_FLAGS (opcode)) == 2);

    context_p->last_cbc.literal_index = lit_value;
  }

  JERRY_ASSERT (value > 0 && value <= CBC_PUSH_NUMBER_BYTE_RANGE_END);

  context_p->last_cbc_opcode = opcode;
  context_p->last_cbc.value = (uint16_t) (value - 1);
} /* parser_emit_cbc_push_number */

#if JERRY_LINE_INFO

/**
 * Append a line info data
 */
void
parser_emit_line_info (parser_context_t *context_p, /**< context */
                       uint32_t line, /**< current line */
                       bool flush_cbc) /**< flush last byte code */
{
  if (flush_cbc && context_p->last_cbc_opcode != PARSER_CBC_UNAVAILABLE)
  {
    parser_flush_cbc (context_p);
  }

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
    JERRY_DEBUG_MSG ("  [%3d] CBC_EXT_LINE %d\n", (int) context_p->stack_depth, line);
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  parser_emit_two_bytes (context_p, CBC_EXT_OPCODE, CBC_EXT_LINE);
  context_p->byte_code_size += 2;

  context_p->last_line_info_line = line;

  const uint32_t max_shift_plus_7 = 7 * 5;
  uint32_t shift = 7;

  while (shift < max_shift_plus_7 && (line >> shift) > 0)
  {
    shift += 7;
  }

  do
  {
    shift -= 7;

    uint8_t byte = (uint8_t) ((line >> shift) & CBC_LOWER_SEVEN_BIT_MASK);

    if (shift > 0)
    {
      byte = (uint8_t) (byte | CBC_HIGHEST_BIT_MASK);
    }

    PARSER_APPEND_TO_BYTE_CODE (context_p, byte);
    context_p->byte_code_size++;
  }
  while (shift > 0);
} /* parser_emit_line_info */

#endif /* JERRY_LINE_INFO */

/**
 * Append a byte code with a branch argument
 */
void
parser_emit_cbc_forward_branch (parser_context_t *context_p, /**< context */
                                uint16_t opcode, /**< opcode */
                                parser_branch_t *branch_p) /**< branch result */
{
  uint8_t flags;
  uint32_t extra_byte_code_increase;

  if (context_p->last_cbc_opcode != PARSER_CBC_UNAVAILABLE)
  {
    parser_flush_cbc (context_p);
  }

  context_p->status_flags |= PARSER_NO_END_LABEL;

  if (PARSER_IS_BASIC_OPCODE (opcode))
  {
    JERRY_ASSERT (opcode < CBC_END);
    flags = cbc_flags[opcode];
    extra_byte_code_increase = 0;
  }
  else
  {
    PARSER_APPEND_TO_BYTE_CODE (context_p, CBC_EXT_OPCODE);
    opcode = (uint16_t) PARSER_GET_EXT_OPCODE (opcode);

    JERRY_ASSERT (opcode < CBC_EXT_END);
    flags = cbc_ext_flags[opcode];
    extra_byte_code_increase = 1;
  }

  JERRY_ASSERT (flags & CBC_HAS_BRANCH_ARG);
  JERRY_ASSERT (CBC_BRANCH_IS_FORWARD (flags));
  JERRY_ASSERT (CBC_BRANCH_OFFSET_LENGTH (opcode) == 1);

  /* Branch opcodes never push anything onto the stack. */
  JERRY_ASSERT ((flags >> CBC_STACK_ADJUST_SHIFT) >= CBC_STACK_ADJUST_BASE
                 || (CBC_STACK_ADJUST_BASE - (flags >> CBC_STACK_ADJUST_SHIFT)) <= context_p->stack_depth);
  PARSER_PLUS_EQUAL_U16 (context_p->stack_depth, CBC_STACK_ADJUST_VALUE (flags));

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
    JERRY_DEBUG_MSG ("  [%3d] %s\n",
                     (int) context_p->stack_depth,
                     extra_byte_code_increase == 0 ? cbc_names[opcode] : cbc_ext_names[opcode]);
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  PARSER_PLUS_EQUAL_U16 (opcode, PARSER_MAX_BRANCH_LENGTH - 1);

  parser_emit_two_bytes (context_p, (uint8_t) opcode, 0);
  branch_p->page_p = context_p->byte_code.last_p;
  branch_p->offset = (context_p->byte_code.last_position - 1) | (context_p->byte_code_size << 8);

  context_p->byte_code_size += extra_byte_code_increase;

#if PARSER_MAXIMUM_CODE_SIZE <= UINT16_MAX
  PARSER_APPEND_TO_BYTE_CODE (context_p, 0);
#else /* PARSER_MAXIMUM_CODE_SIZE > UINT16_MAX */
  parser_emit_two_bytes (context_p, 0, 0);
#endif /* PARSER_MAXIMUM_CODE_SIZE <= UINT16_MAX */

  context_p->byte_code_size += PARSER_MAX_BRANCH_LENGTH + 1;

  if (context_p->stack_depth > context_p->stack_limit)
  {
    context_p->stack_limit = context_p->stack_depth;
    if (context_p->stack_limit > PARSER_MAXIMUM_STACK_LIMIT)
    {
      parser_raise_error (context_p, PARSER_ERR_STACK_LIMIT_REACHED);
    }
  }
} /* parser_emit_cbc_forward_branch */

/**
 * Append a branch byte code and create an item.
 *
 * @return newly created parser branch node
 */
parser_branch_node_t *
parser_emit_cbc_forward_branch_item (parser_context_t *context_p, /**< context */
                                     uint16_t opcode, /**< opcode */
                                     parser_branch_node_t *next_p) /**< next branch */
{
  parser_branch_t branch;
  parser_branch_node_t *new_item;

  /* Since byte code insertion may throw an out-of-memory error,
   * the branch is constructed locally, and copied later. */
  parser_emit_cbc_forward_branch (context_p, opcode, &branch);

  new_item = (parser_branch_node_t *) parser_malloc (context_p, sizeof (parser_branch_node_t));
  new_item->branch = branch;
  new_item->next_p = next_p;
  return new_item;
} /* parser_emit_cbc_forward_branch_item */

/**
 * Append a byte code with a branch argument
 */
void
parser_emit_cbc_backward_branch (parser_context_t *context_p, /**< context */
                                 uint16_t opcode, /**< opcode */
                                 uint32_t offset) /**< destination offset */
{
  uint8_t flags;
#if JERRY_PARSER_DUMP_BYTE_CODE
  const char *name;
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  if (context_p->last_cbc_opcode != PARSER_CBC_UNAVAILABLE)
  {
    parser_flush_cbc (context_p);
  }

  context_p->status_flags |= PARSER_NO_END_LABEL;
  offset = context_p->byte_code_size - offset;

  if (PARSER_IS_BASIC_OPCODE (opcode))
  {
    JERRY_ASSERT (opcode < CBC_END);
    flags = cbc_flags[opcode];

#if JERRY_PARSER_DUMP_BYTE_CODE
    name = cbc_names[opcode];
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */
  }
  else
  {
    PARSER_APPEND_TO_BYTE_CODE (context_p, CBC_EXT_OPCODE);
    opcode = (uint16_t) PARSER_GET_EXT_OPCODE (opcode);

    JERRY_ASSERT (opcode < CBC_EXT_END);
    flags = cbc_ext_flags[opcode];
    context_p->byte_code_size++;

#if JERRY_PARSER_DUMP_BYTE_CODE
    name = cbc_ext_names[opcode];
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */
  }

  JERRY_ASSERT (flags & CBC_HAS_BRANCH_ARG);
  JERRY_ASSERT (CBC_BRANCH_IS_BACKWARD (flags));
  JERRY_ASSERT (CBC_BRANCH_OFFSET_LENGTH (opcode) == 1);
  JERRY_ASSERT (offset <= context_p->byte_code_size);

  /* Branch opcodes never push anything onto the stack. */
  JERRY_ASSERT ((flags >> CBC_STACK_ADJUST_SHIFT) >= CBC_STACK_ADJUST_BASE
                 || (CBC_STACK_ADJUST_BASE - (flags >> CBC_STACK_ADJUST_SHIFT)) <= context_p->stack_depth);
  PARSER_PLUS_EQUAL_U16 (context_p->stack_depth, CBC_STACK_ADJUST_VALUE (flags));

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
    JERRY_DEBUG_MSG ("  [%3d] %s\n", (int) context_p->stack_depth, name);
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  context_p->byte_code_size += 2;
#if PARSER_MAXIMUM_CODE_SIZE > UINT16_MAX
  if (offset > UINT16_MAX)
  {
    opcode++;
    context_p->byte_code_size++;
  }
#endif /* PARSER_MAXIMUM_CODE_SIZE > UINT16_MAX */

  if (offset > UINT8_MAX)
  {
    opcode++;
    context_p->byte_code_size++;
  }

  PARSER_APPEND_TO_BYTE_CODE (context_p, (uint8_t) opcode);

#if PARSER_MAXIMUM_CODE_SIZE > UINT16_MAX
  if (offset > UINT16_MAX)
  {
    PARSER_APPEND_TO_BYTE_CODE (context_p, offset >> 16);
  }
#endif /* PARSER_MAXIMUM_CODE_SIZE > UINT16_MAX */

  if (offset > UINT8_MAX)
  {
    PARSER_APPEND_TO_BYTE_CODE (context_p, (offset >> 8) & 0xff);
  }

  PARSER_APPEND_TO_BYTE_CODE (context_p, offset & 0xff);
} /* parser_emit_cbc_backward_branch */

#undef PARSER_CHECK_LAST_POSITION
#undef PARSER_APPEND_TO_BYTE_CODE

/**
 * Helper function for parser.
 *
 * @return a new string based on encode.
 */
ecma_string_t *
parser_new_ecma_string_from_literal (lexer_literal_t *literal_p) /**< literal */
{
  JERRY_ASSERT (literal_p != NULL);

  ecma_string_t *new_string = NULL;

  if (literal_p->status_flags & LEXER_FLAG_ASCII)
  {
    new_string = ecma_new_ecma_string_from_ascii (literal_p->u.char_p, literal_p->prop.length);
  }
  else
  {
    new_string = ecma_new_ecma_string_from_utf8 (literal_p->u.char_p, literal_p->prop.length);
  }

  return new_string;
} /* parser_new_ecma_string_from_literal  */

/**
 * Set a branch to the current byte code position
 */
void
parser_set_branch_to_current_position (parser_context_t *context_p, /**< context */
                                       parser_branch_t *branch_p) /**< branch result */
{
  uint32_t delta;
  size_t offset;
  parser_mem_page_t *page_p = branch_p->page_p;

  if (context_p->last_cbc_opcode != PARSER_CBC_UNAVAILABLE)
  {
    parser_flush_cbc (context_p);
  }

  context_p->status_flags &= (uint32_t) ~PARSER_NO_END_LABEL;

  JERRY_ASSERT (context_p->byte_code_size > (branch_p->offset >> 8));

  delta = context_p->byte_code_size - (branch_p->offset >> 8);
  offset = (branch_p->offset & CBC_LOWER_SEVEN_BIT_MASK);

  JERRY_ASSERT (delta <= PARSER_MAXIMUM_CODE_SIZE);

#if PARSER_MAXIMUM_CODE_SIZE <= UINT16_MAX
  page_p->bytes[offset++] = (uint8_t) (delta >> 8);
  if (offset >= PARSER_CBC_STREAM_PAGE_SIZE)
  {
    page_p = page_p->next_p;
    offset = 0;
  }
#else /* PARSER_MAXIMUM_CODE_SIZE > UINT16_MAX */
  page_p->bytes[offset++] = (uint8_t) (delta >> 16);
  if (offset >= PARSER_CBC_STREAM_PAGE_SIZE)
  {
    page_p = page_p->next_p;
    offset = 0;
  }
  page_p->bytes[offset++] = (uint8_t) ((delta >> 8) & 0xff);
  if (offset >= PARSER_CBC_STREAM_PAGE_SIZE)
  {
    page_p = page_p->next_p;
    offset = 0;
  }
#endif /* PARSER_MAXIMUM_CODE_SIZE <= UINT16_MAX */
  page_p->bytes[offset] = delta & 0xff;
} /* parser_set_branch_to_current_position */

/**
 * Set breaks to the current byte code position
 */
void
parser_set_breaks_to_current_position (parser_context_t *context_p, /**< context */
                                       parser_branch_node_t *current_p) /**< branch list */
{
  while (current_p != NULL)
  {
    parser_branch_node_t *next_p = current_p->next_p;

    if (!(current_p->branch.offset & CBC_HIGHEST_BIT_MASK))
    {
      parser_set_branch_to_current_position (context_p, &current_p->branch);
    }
    parser_free (current_p, sizeof (parser_branch_node_t));
    current_p = next_p;
  }
} /* parser_set_breaks_to_current_position */

/**
 * Set continues to the current byte code position
 */
void
parser_set_continues_to_current_position (parser_context_t *context_p, /**< context */
                                          parser_branch_node_t *current_p) /**< branch list */
{
  while (current_p != NULL)
  {
    if (current_p->branch.offset & CBC_HIGHEST_BIT_MASK)
    {
      parser_set_branch_to_current_position (context_p, &current_p->branch);
    }
    current_p = current_p->next_p;
  }
} /* parser_set_continues_to_current_position */

#if JERRY_ESNEXT

/**
 * Return the size of internal record corresponding to a class field
 *
 * @return internal record size
 */
static size_t
parser_get_class_field_info_size (uint8_t class_field_type) /**< class field type */
{
  if (class_field_type & PARSER_CLASS_FIELD_INITIALIZED)
  {
    return sizeof (scanner_range_t) + 1;
  }

  if (class_field_type & PARSER_CLASS_FIELD_NORMAL)
  {
    return sizeof (scanner_location_t) + 1;
  }

  return 1;
} /* parser_get_class_field_info_size */

/**
 * Reverse the field list of a class
 */
void
parser_reverse_class_fields (parser_context_t *context_p, /**< context */
                             size_t fields_size) /**< size of consumed memory */
{
  uint8_t *data_p = (uint8_t *) parser_malloc (context_p, fields_size);
  uint8_t *data_end_p = data_p + fields_size;
  uint8_t *current_p = data_p;
  bool has_fields = false;
  parser_stack_iterator_t iterator;

  JERRY_ASSERT (!(context_p->stack_top_uint8 & PARSER_CLASS_FIELD_END));

  parser_stack_iterator_init (context_p, &iterator);

  do
  {
    uint8_t class_field_type = parser_stack_iterator_read_uint8 (&iterator);
    size_t info_size = parser_get_class_field_info_size (class_field_type);

    parser_stack_iterator_read (&iterator, current_p, info_size);
    parser_stack_iterator_skip (&iterator, info_size);
    current_p += info_size;

    if (!(class_field_type & PARSER_CLASS_FIELD_STATIC))
    {
      has_fields = true;
      context_p->stack_top_uint8 = class_field_type;
    }
  }
  while (current_p < data_end_p);

  parser_stack_iterator_init (context_p, &iterator);
  current_p = data_end_p;

  bool has_static_fields = false;

  if (has_fields)
  {
    do
    {
      uint8_t class_field_type = current_p[-1];

      size_t info_size = parser_get_class_field_info_size (class_field_type);

      if (!(class_field_type & PARSER_CLASS_FIELD_STATIC))
      {
        current_p -= info_size;
        parser_stack_iterator_write (&iterator, current_p, info_size);
        parser_stack_iterator_skip (&iterator, info_size);
        continue;
      }

      if (!has_static_fields)
      {
        has_static_fields = true;
        current_p[-1] |= PARSER_CLASS_FIELD_END;
      }
      current_p -= info_size;
    }
    while (current_p > data_p);
  }
  else
  {
    /* All class fields are static. */
    has_static_fields = true;
    JERRY_ASSERT (data_end_p[-1] & PARSER_CLASS_FIELD_STATIC);
    context_p->stack_top_uint8 = data_end_p[-1];
  }

  if (has_static_fields)
  {
    current_p = data_end_p;

    do
    {
      uint8_t class_field_type = current_p[-1];

      size_t info_size = parser_get_class_field_info_size (class_field_type);
      current_p -= info_size;

      if (class_field_type & PARSER_CLASS_FIELD_STATIC)
      {
        parser_stack_iterator_write (&iterator, current_p, info_size);
        parser_stack_iterator_skip (&iterator, info_size);
      }
    }
    while (current_p > data_p);
  }

  parser_free (data_p, fields_size);
} /* parser_reverse_class_fields */

#endif /* JERRY_ESNEXT */

#if JERRY_ERROR_MESSAGES
/**
 * Returns with the string representation of the error
 */
const char *
parser_error_to_string (parser_error_t error) /**< error code */
{
  switch (error)
  {
    case PARSER_ERR_OUT_OF_MEMORY:
    {
      return "Out of memory";
    }
    case PARSER_ERR_LITERAL_LIMIT_REACHED:
    {
      return "Maximum number of literals reached";
    }
    case PARSER_ERR_SCOPE_STACK_LIMIT_REACHED:
    {
      return "Maximum depth of scope stack reached";
    }
    case PARSER_ERR_ARGUMENT_LIMIT_REACHED:
    {
      return "Maximum number of function arguments reached";
    }
    case PARSER_ERR_STACK_LIMIT_REACHED:
    {
      return "Maximum function stack size reached";
    }
    case PARSER_ERR_INVALID_CHARACTER:
    {
      return "Invalid (unexpected) character";
    }
    case PARSER_ERR_INVALID_OCTAL_DIGIT:
    {
      return "Invalid octal digit";
    }
    case PARSER_ERR_INVALID_HEX_DIGIT:
    {
      return "Invalid hexadecimal digit";
    }
#if JERRY_ESNEXT
    case PARSER_ERR_INVALID_BIN_DIGIT:
    {
      return "Invalid binary digit";
    }
#endif /* JERRY_ESNEXT */
    case PARSER_ERR_INVALID_ESCAPE_SEQUENCE:
    {
      return "Invalid escape sequence";
    }
    case PARSER_ERR_INVALID_UNICODE_ESCAPE_SEQUENCE:
    {
      return "Invalid unicode escape sequence";
    }
    case PARSER_ERR_INVALID_IDENTIFIER_START:
    {
      return "Character cannot be start of an identifier";
    }
    case PARSER_ERR_INVALID_IDENTIFIER_PART:
    {
      return "Character cannot be part of an identifier";
    }
    case PARSER_ERR_INVALID_KEYWORD:
    {
      return "Escape sequences are not allowed in keywords";
    }
    case PARSER_ERR_INVALID_NUMBER:
    {
      return "Invalid number";
    }
    case PARSER_ERR_MISSING_EXPONENT:
    {
      return "Missing exponent part";
    }
    case PARSER_ERR_INVALID_UNDERSCORE_IN_NUMBER:
    {
      return "Invalid use of underscore character in number literals";
    }
    case PARSER_ERR_IDENTIFIER_AFTER_NUMBER:
    {
      return "Identifier cannot start after a number";
    }
#if JERRY_BUILTIN_BIGINT
    case PARSER_ERR_INVALID_BIGINT:
    {
      return "Number is not a valid BigInt";
    }
#endif /* JERRY_BUILTIN_BIGINT */
    case PARSER_ERR_INVALID_REGEXP:
    {
      return "Invalid regular expression";
    }
    case PARSER_ERR_UNKNOWN_REGEXP_FLAG:
    {
      return "Unknown regexp flag";
    }
    case PARSER_ERR_DUPLICATED_REGEXP_FLAG:
    {
      return "Duplicated regexp flag";
    }
    case PARSER_ERR_UNSUPPORTED_REGEXP:
    {
      return "Regexp is not supported in the selected profile";
    }
    case PARSER_ERR_IDENTIFIER_TOO_LONG:
    {
      return "Identifier is too long";
    }
    case PARSER_ERR_STRING_TOO_LONG:
    {
      return "String is too long";
    }
    case PARSER_ERR_NUMBER_TOO_LONG:
    {
      return "Number is too long";
    }
    case PARSER_ERR_REGEXP_TOO_LONG:
    {
      return "Regexp is too long";
    }
    case PARSER_ERR_UNTERMINATED_MULTILINE_COMMENT:
    {
      return "Unterminated multiline comment";
    }
    case PARSER_ERR_UNTERMINATED_STRING:
    {
      return "Unterminated string literal";
    }
    case PARSER_ERR_UNTERMINATED_REGEXP:
    {
      return "Unterminated regexp literal";
    }
    case PARSER_ERR_NEWLINE_NOT_ALLOWED:
    {
      return "Newline is not allowed in strings or regexps";
    }
    case PARSER_ERR_OCTAL_NUMBER_NOT_ALLOWED:
    {
      return "Octal numbers are not allowed in strict mode";
    }
    case PARSER_ERR_OCTAL_ESCAPE_NOT_ALLOWED:
    {
      return "Octal escape sequences are not allowed in strict mode";
    }
#if JERRY_ESNEXT
    case PARSER_ERR_TEMPLATE_STR_OCTAL_ESCAPE:
    {
      return "Octal escape sequences are not allowed in template strings";
    }
#endif /* JERRY_ESNEXT */
    case PARSER_ERR_STRICT_IDENT_NOT_ALLOWED:
    {
      return "Identifier name is reserved in strict mode";
    }
    case PARSER_ERR_EVAL_NOT_ALLOWED:
    {
      return "Eval is not allowed to be used here in strict mode";
    }
    case PARSER_ERR_ARGUMENTS_NOT_ALLOWED:
    {
      return "Arguments is not allowed to be used here in strict mode";
    }
#if JERRY_ESNEXT
    case PARSER_ERR_USE_STRICT_NOT_ALLOWED:
    {
      return "The 'use strict' directive is not allowed for functions with non-simple arguments";
    }
    case PARSER_ERR_YIELD_NOT_ALLOWED:
    {
      return "Yield expression is not allowed here";
    }
    case PARSER_ERR_AWAIT_NOT_ALLOWED:
    {
      return "Await expression is not allowed here";
    }
    case PARSER_ERR_FOR_IN_OF_DECLARATION:
    {
      return "for in-of loop variable declaration may not have an initializer";
    }
    case PARSER_ERR_FOR_AWAIT_NO_ASYNC:
    {
      return "for-await-of is only allowed inside async functions and generators";
    }
    case PARSER_ERR_FOR_AWAIT_NO_OF:
    {
      return "only 'of' form is allowed for for-await loops";
    }
    case PARSER_ERR_DUPLICATED_PROTO:
    {
      return "Duplicate __proto__ fields are not allowed in object literals";
    }
    case PARSER_ERR_INVALID_LHS_ASSIGNMENT:
    {
      return "Invalid left-hand side in assignment";
    }
    case PARSER_ERR_INVALID_LHS_POSTFIX_OP:
    {
      return "Invalid left-hand side expression in postfix operation";
    }
    case PARSER_ERR_INVALID_LHS_PREFIX_OP:
    {
      return "Invalid left-hand side expression in prefix operation";
    }
    case PARSER_ERR_INVALID_LHS_FOR_LOOP:
    {
      return "Invalid left-hand-side in for-loop";
    }
#endif /* JERRY_ESNEXT */
    case PARSER_ERR_DELETE_IDENT_NOT_ALLOWED:
    {
      return "Deleting identifier is not allowed in strict mode";
    }
    case PARSER_ERR_EVAL_CANNOT_ASSIGNED:
    {
      return "Eval cannot be assigned to in strict mode";
    }
    case PARSER_ERR_ARGUMENTS_CANNOT_ASSIGNED:
    {
      return "Arguments cannot be assigned to in strict mode";
    }
    case PARSER_ERR_WITH_NOT_ALLOWED:
    {
      return "With statement not allowed in strict mode";
    }
    case PARSER_ERR_MULTIPLE_DEFAULTS_NOT_ALLOWED:
    {
      return "Multiple default cases are not allowed";
    }
    case PARSER_ERR_DEFAULT_NOT_IN_SWITCH:
    {
      return "Default statement must be in a switch block";
    }
    case PARSER_ERR_CASE_NOT_IN_SWITCH:
    {
      return "Case statement must be in a switch block";
    }
    case PARSER_ERR_LEFT_PAREN_EXPECTED:
    {
      return "Expected '(' token";
    }
    case PARSER_ERR_LEFT_BRACE_EXPECTED:
    {
      return "Expected '{' token";
    }
    case PARSER_ERR_RIGHT_PAREN_EXPECTED:
    {
      return "Expected ')' token";
    }
    case PARSER_ERR_RIGHT_SQUARE_EXPECTED:
    {
      return "Expected ']' token";
    }
    case PARSER_ERR_COLON_EXPECTED:
    {
      return "Expected ':' token";
    }
    case PARSER_ERR_COLON_FOR_CONDITIONAL_EXPECTED:
    {
      return "Expected ':' token for ?: conditional expression";
    }
    case PARSER_ERR_SEMICOLON_EXPECTED:
    {
      return "Expected ';' token";
    }
    case PARSER_ERR_IN_EXPECTED:
    {
      return "Expected 'in' token";
    }
    case PARSER_ERR_WHILE_EXPECTED:
    {
      return "While expected for do-while loop";
    }
    case PARSER_ERR_CATCH_FINALLY_EXPECTED:
    {
      return "Catch or finally block expected";
    }
    case PARSER_ERR_ARRAY_ITEM_SEPARATOR_EXPECTED:
    {
      return "Expected ',' or ']' after an array item";
    }
    case PARSER_ERR_OBJECT_ITEM_SEPARATOR_EXPECTED:
    {
      return "Expected ',' or '}' after a property definition";
    }
    case PARSER_ERR_IDENTIFIER_EXPECTED:
    {
      return "Identifier expected";
    }
    case PARSER_ERR_EXPRESSION_EXPECTED:
    {
      return "Expression expected";
    }
    case PARSER_ERR_PRIMARY_EXP_EXPECTED:
    {
      return "Primary expression expected";
    }
    case PARSER_ERR_LEFT_HAND_SIDE_EXP_EXPECTED:
    {
      return "Left-hand-side expression expected";
    }
    case PARSER_ERR_STATEMENT_EXPECTED:
    {
      return "Statement expected";
    }
    case PARSER_ERR_PROPERTY_IDENTIFIER_EXPECTED:
    {
      return "Property identifier expected";
    }
    case PARSER_ERR_ARGUMENT_LIST_EXPECTED:
    {
      return "Expected argument list";
    }
    case PARSER_ERR_NO_ARGUMENTS_EXPECTED:
    {
      return "Property getters must have no arguments";
    }
    case PARSER_ERR_ONE_ARGUMENT_EXPECTED:
    {
      return "Property setters must have one argument";
    }
    case PARSER_ERR_INVALID_EXPRESSION:
    {
      return "Invalid expression";
    }
    case PARSER_ERR_INVALID_SWITCH:
    {
      return "Invalid switch body";
    }
    case PARSER_ERR_INVALID_BREAK:
    {
      return "Break statement must be inside a loop or switch";
    }
    case PARSER_ERR_INVALID_BREAK_LABEL:
    {
      return "Labeled statement targeted by a break not found";
    }
    case PARSER_ERR_INVALID_CONTINUE:
    {
      return "Continue statement must be inside a loop";
    }
    case PARSER_ERR_INVALID_CONTINUE_LABEL:
    {
      return "Labeled statement targeted by a continue not found";
    }
    case PARSER_ERR_INVALID_RETURN:
    {
      return "Return statement must be inside a function body";
    }
    case PARSER_ERR_INVALID_RIGHT_SQUARE:
    {
      return "Unexpected '}' token";
    }
    case PARSER_ERR_DUPLICATED_LABEL:
    {
      return "Duplicated label";
    }
    case PARSER_ERR_OBJECT_PROPERTY_REDEFINED:
    {
      return "Property of object literal redefined";
    }
    case PARSER_ERR_NON_STRICT_ARG_DEFINITION:
    {
      return "Non-strict argument definition";
    }
#if JERRY_ESNEXT
    case PARSER_ERR_VARIABLE_REDECLARED:
    {
      return ecma_error_local_variable_is_redeclared;
    }
    case PARSER_ERR_LEXICAL_SINGLE_STATEMENT:
    {
      return "Lexical declaration cannot appear in a single-statement context";
    }
    case PARSER_ERR_LABELLED_FUNC_NOT_IN_BLOCK:
    {
      return "Labelled functions are only allowed inside blocks";
    }
    case PARSER_ERR_LEXICAL_LET_BINDING:
    {
      return "Let binding cannot appear in let/const declarations";
    }
    case PARSER_ERR_MISSING_ASSIGN_AFTER_CONST:
    {
      return "Value assignment is expected after a const declaration";
    }
    case PARSER_ERR_MULTIPLE_CLASS_CONSTRUCTORS:
    {
      return "Multiple constructors are not allowed";
    }
    case PARSER_ERR_CLASS_CONSTRUCTOR_AS_ACCESSOR:
    {
      return "Class constructor may not be an accessor";
    }
    case PARSER_ERR_INVALID_CLASS_CONSTRUCTOR:
    {
      return "Class constructor may not be a generator or async function";
    }
    case PARSER_ERR_CLASS_STATIC_PROTOTYPE:
    {
      return "Classes may not have a static property called 'prototype'";
    }
    case PARSER_ERR_UNEXPECTED_SUPER_KEYWORD:
    {
      return "Super is not allowed to be used here";
    }
    case PARSER_ERR_TOO_MANY_CLASS_FIELDS:
    {
      return "Too many computed class fields are declared";
    }
    case PARSER_ERR_ARGUMENTS_IN_CLASS_FIELD:
    {
      return "In class field declarations 'arguments' is not allowed";
    }
    case PARSER_ERR_RIGHT_BRACE_EXPECTED:
    {
      return "Expected '}' token";
    }
    case PARSER_ERR_OF_EXPECTED:
    {
      return "Expected 'of' token";
    }
    case PARSER_ERR_ASSIGNMENT_EXPECTED:
    {
      return "Unexpected arrow function or yield expression (parentheses around the expression may help)";
    }
    case PARSER_ERR_DUPLICATED_ARGUMENT_NAMES:
    {
      return "Duplicated function argument names are not allowed here";
    }
    case PARSER_ERR_INVALID_DESTRUCTURING_PATTERN:
    {
      return "Invalid destructuring assignment target";
    }
    case PARSER_ERR_ILLEGAL_PROPERTY_IN_DECLARATION:
    {
      return "Illegal property in declaration context";
    }
    case PARSER_ERR_INVALID_EXPONENTIATION:
    {
      return "Left operand of ** operator cannot be unary expression";
    }
    case PARSER_ERR_INVALID_NULLISH_COALESCING:
    {
      return "Cannot chain nullish with logical AND or OR";
    }
    case PARSER_ERR_FORMAL_PARAM_AFTER_REST_PARAMETER:
    {
      return "Rest parameter must be the last formal parameter";
    }
    case PARSER_ERR_SETTER_REST_PARAMETER:
    {
      return "Setter function argument must not be a rest parameter";
    }
    case PARSER_ERR_REST_PARAMETER_DEFAULT_INITIALIZER:
    {
      return "Rest parameter may not have a default initializer";
    }
    case PARSER_ERR_NEW_TARGET_EXPECTED:
    {
      return "Expected new.target expression";
    }
    case PARSER_ERR_NEW_TARGET_NOT_ALLOWED:
    {
      return "new.target expression is not allowed here";
    }
#endif /* JERRY_ESNEXT */
#if JERRY_MODULE_SYSTEM
    case PARSER_ERR_FILE_NOT_FOUND:
    {
      return "Requested module not found";
    }
    case PARSER_ERR_FROM_EXPECTED:
    {
      return "Expected 'from' token";
    }
    case PARSER_ERR_FROM_COMMA_EXPECTED:
    {
      return "Expected 'from' or ',' token";
    }
    case PARSER_ERR_AS_EXPECTED:
    {
      return "Expected 'as' token";
    }
    case PARSER_ERR_STRING_EXPECTED:
    {
      return "Expected a string literal";
    }
    case PARSER_ERR_MODULE_UNEXPECTED:
    {
      return "Unexpected import or export statement";
    }
    case PARSER_ERR_LEFT_BRACE_MULTIPLY_EXPECTED:
    {
      return "Expected '{' or '*' token";
    }
    case PARSER_ERR_LEFT_BRACE_MULTIPLY_LITERAL_EXPECTED:
    {
      return "Expected '{' or '*' or literal token";
    }
    case PARSER_ERR_RIGHT_BRACE_COMMA_EXPECTED:
    {
      return "Expected '}' or ',' token";
    }
    case PARSER_ERR_DUPLICATED_EXPORT_IDENTIFIER:
    {
      return "Duplicate exported identifier";
    }
    case PARSER_ERR_DUPLICATED_IMPORT_BINDING:
    {
      return "Duplicated imported binding name";
    }
    case PARSER_ERR_EXPORT_NOT_DEFINED:
    {
      return "Export not defined in module";
    }
#endif /* JERRY_MODULE_SYSTEM */

    default:
    {
      JERRY_ASSERT (error == PARSER_ERR_NO_ERROR);
      return "No error";
    }
  }
} /* parser_error_to_string */
#endif /* JERRY_ERROR_MESSAGES */

/**
 * @}
 * @}
 * @}
 */

#endif /* JERRY_PARSER */

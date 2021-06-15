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
#include "ecma-exceptions.h"
#include "ecma-helpers.h"
#include "ecma-literal-storage.h"
#include "ecma-module.h"
#include "jcontext.h"
#include "js-parser-internal.h"

#if JERRY_PARSER

JERRY_STATIC_ASSERT ((int) ECMA_PARSE_STRICT_MODE == (int) PARSER_IS_STRICT,
                     ecma_parse_strict_mode_must_be_equal_to_parser_is_strict);

#if JERRY_ESNEXT
JERRY_STATIC_ASSERT (PARSER_SAVE_STATUS_FLAGS (PARSER_ALLOW_SUPER) == 0x1,
                     incorrect_saving_of_ecma_parse_allow_super);
JERRY_STATIC_ASSERT (PARSER_RESTORE_STATUS_FLAGS (ECMA_PARSE_ALLOW_SUPER) == PARSER_ALLOW_SUPER,
                     incorrect_restoring_of_ecma_parse_allow_super);

JERRY_STATIC_ASSERT (PARSER_RESTORE_STATUS_FLAGS (ECMA_PARSE_FUNCTION_CONTEXT) == 0,
                     ecma_parse_function_context_must_not_be_transformed);
#endif /* JERRY_ESNEXT */

/** \addtogroup parser Parser
 * @{
 *
 * \addtogroup jsparser JavaScript
 * @{
 *
 * \addtogroup jsparser_parser Parser
 * @{
 */

/**
 * Compute real literal indicies.
 *
 * @return length of the prefix opcodes
 */
static void
parser_compute_indicies (parser_context_t *context_p, /**< context */
                         uint16_t *ident_end, /**< end of the identifier group */
                         uint16_t *const_literal_end) /**< end of the const literal group */
{
  parser_list_iterator_t literal_iterator;
  lexer_literal_t *literal_p;

  uint16_t ident_count = 0;
  uint16_t const_literal_count = 0;

#if JERRY_RESOURCE_NAME
  /* Resource name will be stored as the last const literal. */
  if (JERRY_UNLIKELY (context_p->literal_count >= PARSER_MAXIMUM_NUMBER_OF_LITERALS))
  {
    parser_raise_error (context_p, PARSER_ERR_LITERAL_LIMIT_REACHED);
  }

  const_literal_count++;
  context_p->literal_count++;
#endif /* JERRY_RESOURCE_NAME */

  uint16_t ident_index;
  uint16_t const_literal_index;
  uint16_t literal_index;

  /* First phase: count the number of items in each group. */
  parser_list_iterator_init (&context_p->literal_pool, &literal_iterator);
  while ((literal_p = (lexer_literal_t *) parser_list_iterator_next (&literal_iterator)))
  {
    switch (literal_p->type)
    {
      case LEXER_IDENT_LITERAL:
      {
        if (literal_p->status_flags & LEXER_FLAG_USED)
        {
          ident_count++;
          break;
        }
        else if (!(literal_p->status_flags & LEXER_FLAG_SOURCE_PTR))
        {
          jmem_heap_free_block ((void *) literal_p->u.char_p, literal_p->prop.length);
          /* This literal should not be freed even if an error is encountered later. */
          literal_p->status_flags |= LEXER_FLAG_SOURCE_PTR;
        }
        continue;
      }
      case LEXER_STRING_LITERAL:
      {
        const_literal_count++;
        break;
      }
      case LEXER_NUMBER_LITERAL:
      {
        const_literal_count++;
        continue;
      }
      case LEXER_FUNCTION_LITERAL:
      case LEXER_REGEXP_LITERAL:
      {
        continue;
      }
      default:
      {
        JERRY_ASSERT (literal_p->type == LEXER_UNUSED_LITERAL);
        continue;
      }
    }

    const uint8_t *char_p = literal_p->u.char_p;
    uint32_t status_flags = context_p->status_flags;

    if ((literal_p->status_flags & LEXER_FLAG_SOURCE_PTR)
        && literal_p->prop.length < 0xfff)
    {
      size_t bytes_to_end = (size_t) (context_p->source_end_p - char_p);

      if (bytes_to_end < 0xfffff)
      {
        literal_p->u.source_data = ((uint32_t) bytes_to_end) | (((uint32_t) literal_p->prop.length) << 20);
        literal_p->status_flags |= LEXER_FLAG_LATE_INIT;
        status_flags |= PARSER_HAS_LATE_LIT_INIT;
        context_p->status_flags = status_flags;
        char_p = NULL;
      }
    }

    if (char_p != NULL)
    {
      literal_p->u.value = ecma_find_or_create_literal_string (char_p,
                                                               literal_p->prop.length,
                                                               (literal_p->status_flags & LEXER_FLAG_ASCII));

      if (!(literal_p->status_flags & LEXER_FLAG_SOURCE_PTR))
      {
        jmem_heap_free_block ((void *) char_p, literal_p->prop.length);
        /* This literal should not be freed even if an error is encountered later. */
        literal_p->status_flags |= LEXER_FLAG_SOURCE_PTR;
      }
    }
  }

  ident_index = context_p->register_count;
  const_literal_index = (uint16_t) (ident_index + ident_count);
  literal_index = (uint16_t) (const_literal_index + const_literal_count);

  /* Second phase: Assign an index to each literal. */
  parser_list_iterator_init (&context_p->literal_pool, &literal_iterator);

  while ((literal_p = (lexer_literal_t *) parser_list_iterator_next (&literal_iterator)))
  {
    switch (literal_p->type)
    {
      case LEXER_IDENT_LITERAL:
      {
        if (literal_p->status_flags & LEXER_FLAG_USED)
        {
          literal_p->prop.index = ident_index;
          ident_index++;
        }
        break;
      }
      case LEXER_STRING_LITERAL:
      case LEXER_NUMBER_LITERAL:
      {
        JERRY_ASSERT ((literal_p->status_flags & ~(LEXER_FLAG_SOURCE_PTR | LEXER_FLAG_LATE_INIT)) == 0);
        literal_p->prop.index = const_literal_index;
        const_literal_index++;
        break;
      }
      case LEXER_FUNCTION_LITERAL:
      case LEXER_REGEXP_LITERAL:
      {
        JERRY_ASSERT (literal_p->status_flags == 0);

        literal_p->prop.index = literal_index;
        literal_index++;
        break;
      }
      default:
      {
        JERRY_ASSERT (literal_p->type == LEXER_UNUSED_LITERAL
                      && literal_p->status_flags == LEXER_FLAG_FUNCTION_ARGUMENT);
        break;
      }
    }
  }

#if JERRY_RESOURCE_NAME
  /* Resource name will be stored as the last const literal. */
  const_literal_index++;
#endif /* JERRY_RESOURCE_NAME */

  JERRY_ASSERT (ident_index == context_p->register_count + ident_count);
  JERRY_ASSERT (const_literal_index == ident_index + const_literal_count);
  JERRY_ASSERT (literal_index <= context_p->register_count + context_p->literal_count);

  context_p->literal_count = literal_index;

  *ident_end = ident_index;
  *const_literal_end = const_literal_index;
} /* parser_compute_indicies */

/**
 * Initialize literal pool.
 */
static void
parser_init_literal_pool (parser_context_t *context_p, /**< context */
                          ecma_value_t *literal_pool_p) /**< start of literal pool */
{
  parser_list_iterator_t literal_iterator;
  lexer_literal_t *literal_p;

  parser_list_iterator_init (&context_p->literal_pool, &literal_iterator);

  while ((literal_p = (lexer_literal_t *) parser_list_iterator_next (&literal_iterator)))
  {
    switch (literal_p->type)
    {
      case LEXER_IDENT_LITERAL:
      {
        if (!(literal_p->status_flags & LEXER_FLAG_USED))
        {
          break;
        }
        /* FALLTHRU */
      }
      case LEXER_STRING_LITERAL:
      {
        ecma_value_t lit_value = literal_p->u.value;

        JERRY_ASSERT (literal_p->prop.index >= context_p->register_count);
        literal_pool_p[literal_p->prop.index] = lit_value;
        break;
      }
      case LEXER_NUMBER_LITERAL:
      {
        JERRY_ASSERT (literal_p->prop.index >= context_p->register_count);

        literal_pool_p[literal_p->prop.index] = literal_p->u.value;
        break;
      }
      case LEXER_FUNCTION_LITERAL:
      case LEXER_REGEXP_LITERAL:
      {
        JERRY_ASSERT (literal_p->prop.index >= context_p->register_count);

        ECMA_SET_INTERNAL_VALUE_POINTER (literal_pool_p[literal_p->prop.index],
                                         literal_p->u.bytecode_p);
        break;
      }
      default:
      {
        JERRY_ASSERT (literal_p->type == LEXER_UNUSED_LITERAL);
        break;
      }
    }
  }
} /* parser_init_literal_pool */

/*
 * During byte code post processing certain bytes are not
 * copied into the final byte code buffer. For example, if
 * one byte is enough for encoding a literal index, the
 * second byte is not copied. However, when a byte is skipped,
 * the offsets of those branches which crosses (jumps over)
 * that byte code should also be decreased by one. Instead
 * of finding these jumps every time when a byte is skipped,
 * all branch offset updates are computed in one step.
 *
 * Branch offset mapping example:
 *
 * Let's assume that each parser_mem_page of the byte_code
 * buffer is 8 bytes long and only 4 bytes are kept for a
 * given page:
 *
 * +---+---+---+---+---+---+---+---+
 * | X | 1 | 2 | 3 | X | 4 | X | X |
 * +---+---+---+---+---+---+---+---+
 *
 * X marks those bytes which are removed. The resulting
 * offset mapping is the following:
 *
 * +---+---+---+---+---+---+---+---+
 * | 0 | 1 | 2 | 3 | 3 | 4 | 4 | 4 |
 * +---+---+---+---+---+---+---+---+
 *
 * Each X is simply replaced by the index of the previous
 * index starting from zero. This shows the number of
 * copied bytes before a given byte including the byte
 * itself. The last byte always shows the number of bytes
 * copied from this page.
 *
 * This mapping allows recomputing all branch targets,
 * since mapping[to] - mapping[from] is the new argument
 * for forward branches. As for backward branches, the
 * equation is reversed to mapping[from] - mapping[to].
 *
 * The mapping is relative to one page, so distance
 * computation affecting multiple pages requires a loop.
 * We should also note that only argument bytes can
 * be skipped, so removed bytes cannot be targeted by
 * branches. Valid branches always target instruction
 * starts only.
 */

/**
 * Recompute the argument of a forward branch.
 *
 * @return the new distance
 */
static size_t
parser_update_forward_branch (parser_mem_page_t *page_p, /**< current page */
                              size_t full_distance, /**< full distance */
                              uint8_t bytes_copied_before_jump) /**< bytes copied before jump */
{
  size_t new_distance = 0;

  while (full_distance > PARSER_CBC_STREAM_PAGE_SIZE)
  {
    new_distance += page_p->bytes[PARSER_CBC_STREAM_PAGE_SIZE - 1] & CBC_LOWER_SEVEN_BIT_MASK;
    full_distance -= PARSER_CBC_STREAM_PAGE_SIZE;
    page_p = page_p->next_p;
  }

  new_distance += page_p->bytes[full_distance - 1] & CBC_LOWER_SEVEN_BIT_MASK;
  return new_distance - bytes_copied_before_jump;
} /* parser_update_forward_branch */

/**
 * Recompute the argument of a backward branch.
 *
 * @return the new distance
 */
static size_t
parser_update_backward_branch (parser_mem_page_t *page_p, /**< current page */
                               size_t full_distance, /**< full distance */
                               uint8_t bytes_copied_before_jump) /**< bytes copied before jump */
{
  size_t new_distance = bytes_copied_before_jump;

  while (full_distance >= PARSER_CBC_STREAM_PAGE_SIZE)
  {
    JERRY_ASSERT (page_p != NULL);
    new_distance += page_p->bytes[PARSER_CBC_STREAM_PAGE_SIZE - 1] & CBC_LOWER_SEVEN_BIT_MASK;
    full_distance -= PARSER_CBC_STREAM_PAGE_SIZE;
    page_p = page_p->next_p;
  }

  if (full_distance > 0)
  {
    size_t offset = PARSER_CBC_STREAM_PAGE_SIZE - full_distance;

    JERRY_ASSERT (page_p != NULL);

    new_distance += page_p->bytes[PARSER_CBC_STREAM_PAGE_SIZE - 1] & CBC_LOWER_SEVEN_BIT_MASK;
    new_distance -= page_p->bytes[offset - 1] & CBC_LOWER_SEVEN_BIT_MASK;
  }

  return new_distance;
} /* parser_update_backward_branch */

/**
 * Update targets of all branches in one step.
 */
static void
parse_update_branches (parser_context_t *context_p, /**< context */
                       uint8_t *byte_code_p) /**< byte code */
{
  parser_mem_page_t *page_p = context_p->byte_code.first_p;
  parser_mem_page_t *prev_page_p = NULL;
  parser_mem_page_t *last_page_p = context_p->byte_code.last_p;
  size_t last_position = context_p->byte_code.last_position;
  size_t offset = 0;
  size_t bytes_copied = 0;

  if (last_position >= PARSER_CBC_STREAM_PAGE_SIZE)
  {
    last_page_p = NULL;
    last_position = 0;
  }

  while (page_p != last_page_p || offset < last_position)
  {
    /* Branch instructions are marked to improve search speed. */
    if (page_p->bytes[offset] & CBC_HIGHEST_BIT_MASK)
    {
      uint8_t *bytes_p = byte_code_p + bytes_copied;
      uint8_t flags;
      uint8_t bytes_copied_before_jump = 0;
      size_t branch_argument_length;
      size_t target_distance;
      size_t length;

      if (offset > 0)
      {
        bytes_copied_before_jump = page_p->bytes[offset - 1] & CBC_LOWER_SEVEN_BIT_MASK;
      }
      bytes_p += bytes_copied_before_jump;

      if (*bytes_p == CBC_EXT_OPCODE)
      {
        bytes_p++;
        flags = cbc_ext_flags[*bytes_p];
      }
      else
      {
        flags = cbc_flags[*bytes_p];
      }

      JERRY_ASSERT (flags & CBC_HAS_BRANCH_ARG);
      branch_argument_length = CBC_BRANCH_OFFSET_LENGTH (*bytes_p);
      bytes_p++;

      /* Decoding target. */
      length = branch_argument_length;
      target_distance = 0;
      do
      {
        target_distance = (target_distance << 8) | *bytes_p;
        bytes_p++;
      }
      while (--length > 0);

      if (CBC_BRANCH_IS_FORWARD (flags))
      {
        /* Branch target was not set. */
        JERRY_ASSERT (target_distance > 0);

        target_distance = parser_update_forward_branch (page_p,
                                                        offset + target_distance,
                                                        bytes_copied_before_jump);
      }
      else
      {
        if (target_distance < offset)
        {
          uint8_t bytes_copied_before_target = page_p->bytes[offset - target_distance - 1];
          bytes_copied_before_target = bytes_copied_before_target & CBC_LOWER_SEVEN_BIT_MASK;

          target_distance = (size_t) (bytes_copied_before_jump - bytes_copied_before_target);
        }
        else if (target_distance == offset)
        {
          target_distance = bytes_copied_before_jump;
        }
        else
        {
          target_distance = parser_update_backward_branch (prev_page_p,
                                                           target_distance - offset,
                                                           bytes_copied_before_jump);
        }
      }

      /* Encoding target again. */
      do
      {
        bytes_p--;
        *bytes_p = (uint8_t) (target_distance & 0xff);
        target_distance >>= 8;
      }
      while (--branch_argument_length > 0);
    }

    offset++;
    if (offset >= PARSER_CBC_STREAM_PAGE_SIZE)
    {
      parser_mem_page_t *next_p = page_p->next_p;

      /* We reverse the pages before the current page. */
      page_p->next_p = prev_page_p;
      prev_page_p = page_p;

      bytes_copied += page_p->bytes[PARSER_CBC_STREAM_PAGE_SIZE - 1] & CBC_LOWER_SEVEN_BIT_MASK;
      page_p = next_p;
      offset = 0;
    }
  }

  /* After this point the pages of the byte code stream are
   * not used anymore. However, they needs to be freed during
   * cleanup, so the first and last pointers of the stream
   * descriptor are reversed as well. */
  if (last_page_p != NULL)
  {
    JERRY_ASSERT (last_page_p == context_p->byte_code.last_p);
    last_page_p->next_p = prev_page_p;
  }
  else
  {
    last_page_p = context_p->byte_code.last_p;
  }

  context_p->byte_code.last_p = context_p->byte_code.first_p;
  context_p->byte_code.first_p = last_page_p;
} /* parse_update_branches */

#if JERRY_DEBUGGER

/**
 * Send current breakpoint list.
 */
static void
parser_send_breakpoints (parser_context_t *context_p, /**< context */
                         jerry_debugger_header_type_t type) /**< message type */
{
  JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED);
  JERRY_ASSERT (context_p->breakpoint_info_count > 0);

  jerry_debugger_send_data (type,
                            context_p->breakpoint_info,
                            context_p->breakpoint_info_count * sizeof (parser_breakpoint_info_t));

  context_p->breakpoint_info_count = 0;
} /* parser_send_breakpoints */

/**
 * Append a breakpoint info.
 */
void
parser_append_breakpoint_info (parser_context_t *context_p, /**< context */
                               jerry_debugger_header_type_t type, /**< message type */
                               uint32_t value) /**< line or offset of the breakpoint */
{
  JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED);

  context_p->status_flags |= PARSER_DEBUGGER_BREAKPOINT_APPENDED;

  if (context_p->breakpoint_info_count >= JERRY_DEBUGGER_SEND_MAX (parser_breakpoint_info_t))
  {
    parser_send_breakpoints (context_p, type);
  }

  context_p->breakpoint_info[context_p->breakpoint_info_count].value = value;
  context_p->breakpoint_info_count = (uint16_t) (context_p->breakpoint_info_count + 1);
} /* parser_append_breakpoint_info */

#endif /* JERRY_DEBUGGER */

/**
 * Forward iterator: move to the next byte code
 *
 * @param page_p page
 * @param offset offset
 */
#define PARSER_NEXT_BYTE(page_p, offset) \
  do { \
    if (++(offset) >= PARSER_CBC_STREAM_PAGE_SIZE) \
    { \
      offset = 0; \
      page_p = page_p->next_p; \
    } \
  } while (0)

/**
 * Forward iterator: move to the next byte code. Also updates the offset of the previous byte code.
 *
 * @param page_p page
 * @param offset offset
 * @param real_offset real offset
 */
#define PARSER_NEXT_BYTE_UPDATE(page_p, offset, real_offset) \
  do { \
    page_p->bytes[offset] = real_offset; \
    if (++(offset) >= PARSER_CBC_STREAM_PAGE_SIZE) \
    { \
      offset = 0; \
      real_offset = 0; \
      page_p = page_p->next_p; \
    } \
  } while (0)

/**
 * Post processing main function.
 *
 * @return compiled code
 */
static ecma_compiled_code_t *
parser_post_processing (parser_context_t *context_p) /**< context */
{
  uint16_t literal_one_byte_limit;
  uint16_t ident_end;
  uint16_t const_literal_end;
  parser_mem_page_t *page_p;
  parser_mem_page_t *last_page_p;
  size_t last_position;
  size_t offset;
  size_t length;
  size_t literal_length;
  size_t total_size;
  uint8_t real_offset;
  uint8_t *byte_code_p;
  bool needs_uint16_arguments;
  cbc_opcode_t last_opcode = CBC_EXT_OPCODE;
  ecma_compiled_code_t *compiled_code_p;
  ecma_value_t *literal_pool_p;
  uint8_t *dst_p;

#if JERRY_ESNEXT
  if ((context_p->status_flags & (PARSER_IS_FUNCTION | PARSER_LEXICAL_BLOCK_NEEDED))
      == (PARSER_IS_FUNCTION | PARSER_LEXICAL_BLOCK_NEEDED))
  {
    PARSER_MINUS_EQUAL_U16 (context_p->stack_depth, PARSER_BLOCK_CONTEXT_STACK_ALLOCATION);
#ifndef JERRY_NDEBUG
    PARSER_MINUS_EQUAL_U16 (context_p->context_stack_depth, PARSER_BLOCK_CONTEXT_STACK_ALLOCATION);
#endif /* !JERRY_NDEBUG */

    context_p->status_flags &= (uint32_t) ~PARSER_LEXICAL_BLOCK_NEEDED;

    parser_emit_cbc (context_p, CBC_CONTEXT_END);

    parser_branch_t branch;
    parser_stack_pop (context_p, &branch, sizeof (parser_branch_t));
    parser_set_branch_to_current_position (context_p, &branch);

    JERRY_ASSERT (!(context_p->status_flags & PARSER_NO_END_LABEL));
  }

  if (PARSER_IS_NORMAL_ASYNC_FUNCTION (context_p->status_flags))
  {
    PARSER_MINUS_EQUAL_U16 (context_p->stack_depth, PARSER_TRY_CONTEXT_STACK_ALLOCATION);
#ifndef JERRY_NDEBUG
    PARSER_MINUS_EQUAL_U16 (context_p->context_stack_depth, PARSER_TRY_CONTEXT_STACK_ALLOCATION);
#endif /* !JERRY_NDEBUG */

    if (context_p->stack_limit < PARSER_FINALLY_CONTEXT_STACK_ALLOCATION)
    {
      context_p->stack_limit = PARSER_FINALLY_CONTEXT_STACK_ALLOCATION;
    }

    parser_branch_t branch;

    parser_stack_pop (context_p, &branch, sizeof (parser_branch_t));
    parser_set_branch_to_current_position (context_p, &branch);

    JERRY_ASSERT (!(context_p->status_flags & PARSER_NO_END_LABEL));
  }
#endif /* JERRY_ESNEXT */

  JERRY_ASSERT (context_p->stack_depth == 0);
#ifndef JERRY_NDEBUG
  JERRY_ASSERT (context_p->context_stack_depth == 0);
#endif /* !JERRY_NDEBUG */

  if ((size_t) context_p->stack_limit + (size_t) context_p->register_count > PARSER_MAXIMUM_STACK_LIMIT)
  {
    parser_raise_error (context_p, PARSER_ERR_STACK_LIMIT_REACHED);
  }

  JERRY_ASSERT (context_p->literal_count <= PARSER_MAXIMUM_NUMBER_OF_LITERALS);

#if JERRY_DEBUGGER
  if ((JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
      && !(context_p->status_flags & PARSER_DEBUGGER_BREAKPOINT_APPENDED))
  {
    /* Always provide at least one breakpoint. */
    parser_emit_cbc (context_p, CBC_BREAKPOINT_DISABLED);
    parser_flush_cbc (context_p);

    parser_append_breakpoint_info (context_p, JERRY_DEBUGGER_BREAKPOINT_LIST, context_p->token.line);

    context_p->last_breakpoint_line = context_p->token.line;
  }

  if ((JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
      && context_p->breakpoint_info_count > 0)
  {
    parser_send_breakpoints (context_p, JERRY_DEBUGGER_BREAKPOINT_LIST);
    JERRY_ASSERT (context_p->breakpoint_info_count == 0);
  }
#endif /* JERRY_DEBUGGER */

  parser_compute_indicies (context_p, &ident_end, &const_literal_end);

  if (context_p->literal_count <= CBC_MAXIMUM_SMALL_VALUE)
  {
    literal_one_byte_limit = CBC_MAXIMUM_BYTE_VALUE - 1;
  }
  else
  {
    literal_one_byte_limit = CBC_LOWER_SEVEN_BIT_MASK;
  }

  last_page_p = context_p->byte_code.last_p;
  last_position = context_p->byte_code.last_position;

  if (last_position >= PARSER_CBC_STREAM_PAGE_SIZE)
  {
    last_page_p = NULL;
    last_position = 0;
  }

  page_p = context_p->byte_code.first_p;
  offset = 0;
  length = 0;

  while (page_p != last_page_p || offset < last_position)
  {
    uint8_t *opcode_p;
    uint8_t flags;
    size_t branch_offset_length;

    opcode_p = page_p->bytes + offset;
    last_opcode = (cbc_opcode_t) (*opcode_p);
    PARSER_NEXT_BYTE (page_p, offset);
    branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (last_opcode);
    flags = cbc_flags[last_opcode];
    length++;

    switch (last_opcode)
    {
      case CBC_EXT_OPCODE:
      {
        cbc_ext_opcode_t ext_opcode;

        ext_opcode = (cbc_ext_opcode_t) page_p->bytes[offset];
        branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (ext_opcode);
        flags = cbc_ext_flags[ext_opcode];
        PARSER_NEXT_BYTE (page_p, offset);
        length++;

#if JERRY_LINE_INFO
        if (ext_opcode == CBC_EXT_LINE)
        {
          uint8_t last_byte = 0;

          do
          {
            last_byte = page_p->bytes[offset];
            PARSER_NEXT_BYTE (page_p, offset);
            length++;
          }
          while (last_byte & CBC_HIGHEST_BIT_MASK);

          continue;
        }
#endif /* JERRY_LINE_INFO */
        break;
      }
      case CBC_POST_DECR:
      {
        *opcode_p = CBC_PRE_DECR;
        break;
      }
      case CBC_POST_INCR:
      {
        *opcode_p = CBC_PRE_INCR;
        break;
      }
      case CBC_POST_DECR_IDENT:
      {
        *opcode_p = CBC_PRE_DECR_IDENT;
        break;
      }
      case CBC_POST_INCR_IDENT:
      {
        *opcode_p = CBC_PRE_INCR_IDENT;
        break;
      }
      default:
      {
        break;
      }
    }

    while (flags & (CBC_HAS_LITERAL_ARG | CBC_HAS_LITERAL_ARG2))
    {
      uint8_t *first_byte = page_p->bytes + offset;
      uint32_t literal_index = *first_byte;

      PARSER_NEXT_BYTE (page_p, offset);
      length++;

      literal_index |= ((uint32_t) page_p->bytes[offset]) << 8;

      if (literal_index >= PARSER_REGISTER_START)
      {
        literal_index -= PARSER_REGISTER_START;
      }
      else
      {
        literal_index = (PARSER_GET_LITERAL (literal_index))->prop.index;
      }

      if (literal_index <= literal_one_byte_limit)
      {
        *first_byte = (uint8_t) literal_index;
      }
      else
      {
        if (context_p->literal_count <= CBC_MAXIMUM_SMALL_VALUE)
        {
          JERRY_ASSERT (literal_index <= CBC_MAXIMUM_SMALL_VALUE);
          *first_byte = CBC_MAXIMUM_BYTE_VALUE;
          page_p->bytes[offset] = (uint8_t) (literal_index - CBC_MAXIMUM_BYTE_VALUE);
          length++;
        }
        else
        {
          JERRY_ASSERT (literal_index <= CBC_MAXIMUM_FULL_VALUE);
          *first_byte = (uint8_t) ((literal_index >> 8) | CBC_HIGHEST_BIT_MASK);
          page_p->bytes[offset] = (uint8_t) (literal_index & 0xff);
          length++;
        }
      }
      PARSER_NEXT_BYTE (page_p, offset);

      if (flags & CBC_HAS_LITERAL_ARG2)
      {
        if (flags & CBC_HAS_LITERAL_ARG)
        {
          flags = CBC_HAS_LITERAL_ARG;
        }
        else
        {
          flags = CBC_HAS_LITERAL_ARG | CBC_HAS_LITERAL_ARG2;
        }
      }
      else
      {
        break;
      }
    }

    if (flags & CBC_HAS_BYTE_ARG)
    {
      /* This argument will be copied without modification. */
      PARSER_NEXT_BYTE (page_p, offset);
      length++;
    }

    if (flags & CBC_HAS_BRANCH_ARG)
    {
      bool prefix_zero = true;

      /* The leading zeroes are dropped from the stream.
       * Although dropping these zeroes for backward
       * branches are unnecessary, we use the same
       * code path for simplicity. */
      JERRY_ASSERT (branch_offset_length > 0 && branch_offset_length <= 3);

      while (--branch_offset_length > 0)
      {
        uint8_t byte = page_p->bytes[offset];
        if (byte > 0 || !prefix_zero)
        {
          prefix_zero = false;
          length++;
        }
        else
        {
          JERRY_ASSERT (CBC_BRANCH_IS_FORWARD (flags));
        }
        PARSER_NEXT_BYTE (page_p, offset);
      }

      if (last_opcode == (cbc_opcode_t) (CBC_JUMP_FORWARD + PARSER_MAX_BRANCH_LENGTH - 1)
          && prefix_zero
          && page_p->bytes[offset] == PARSER_MAX_BRANCH_LENGTH + 1)
      {
        /* Uncoditional jumps which jump right after the instruction
         * are effectively NOPs. These jumps are removed from the
         * stream. The 1 byte long CBC_JUMP_FORWARD form marks these
         * instructions, since this form is constructed during post
         * processing and cannot be emitted directly. */
        *opcode_p = CBC_JUMP_FORWARD;
        length--;
      }
      else
      {
        /* Other last bytes are always copied. */
        length++;
      }

      PARSER_NEXT_BYTE (page_p, offset);
    }
  }

  if (!(context_p->status_flags & PARSER_NO_END_LABEL)
      || !(PARSER_OPCODE_IS_RETURN (last_opcode)))
  {
    context_p->status_flags &= (uint32_t) ~PARSER_NO_END_LABEL;

#if JERRY_ESNEXT
    if (PARSER_IS_NORMAL_ASYNC_FUNCTION (context_p->status_flags))
    {
      length++;
    }
#endif /* JERRY_ESNEXT */

    length++;
  }

  needs_uint16_arguments = false;
  total_size = sizeof (cbc_uint8_arguments_t);

  if (context_p->stack_limit > CBC_MAXIMUM_BYTE_VALUE
      || context_p->register_count > CBC_MAXIMUM_BYTE_VALUE
      || context_p->literal_count > CBC_MAXIMUM_BYTE_VALUE)
  {
    needs_uint16_arguments = true;
    total_size = sizeof (cbc_uint16_arguments_t);
  }

  literal_length = (size_t) (context_p->literal_count - context_p->register_count) * sizeof (ecma_value_t);

  total_size += literal_length + length;

  if (PARSER_NEEDS_MAPPED_ARGUMENTS (context_p->status_flags))
  {
    total_size += context_p->argument_count * sizeof (ecma_value_t);
  }

#if JERRY_ESNEXT
  /* function.name */
  if (!(context_p->status_flags & PARSER_CLASS_CONSTRUCTOR))
  {
    total_size += sizeof (ecma_value_t);
  }

  if (context_p->argument_length != UINT16_MAX)
  {
    total_size += sizeof (ecma_value_t);
  }

  if (context_p->tagged_template_literal_cp != JMEM_CP_NULL)
  {
    total_size += sizeof (ecma_value_t);
  }
#endif /* JERRY_ESNEXT */

  total_size = JERRY_ALIGNUP (total_size, JMEM_ALIGNMENT);

  compiled_code_p = (ecma_compiled_code_t *) parser_malloc (context_p, total_size);

#if JERRY_SNAPSHOT_SAVE || JERRY_PARSER_DUMP_BYTE_CODE
  // Avoid getting junk bytes
  memset (compiled_code_p, 0, total_size);
#endif /* JERRY_SNAPSHOT_SAVE || JERRY_PARSER_DUMP_BYTE_CODE */

#if JERRY_MEM_STATS
  jmem_stats_allocate_byte_code_bytes (total_size);
#endif /* JERRY_MEM_STATS */

  byte_code_p = (uint8_t *) compiled_code_p;
  compiled_code_p->size = (uint16_t) (total_size >> JMEM_ALIGNMENT_LOG);
  compiled_code_p->refs = 1;
  compiled_code_p->status_flags = 0;

#if JERRY_ESNEXT
  if (context_p->status_flags & PARSER_FUNCTION_HAS_REST_PARAM)
  {
    JERRY_ASSERT (context_p->argument_count > 0);
    context_p->argument_count--;
  }
#endif /* JERRY_ESNEXT */

  if (needs_uint16_arguments)
  {
    cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) compiled_code_p;

    args_p->stack_limit = context_p->stack_limit;
    args_p->argument_end = context_p->argument_count;
    args_p->register_end = context_p->register_count;
    args_p->ident_end = ident_end;
    args_p->const_literal_end = const_literal_end;
    args_p->literal_end = context_p->literal_count;
#if JERRY_BUILTIN_REALMS
    ECMA_SET_INTERNAL_VALUE_POINTER (args_p->realm_value, JERRY_CONTEXT (global_object_p));
#endif /* JERRY_BUILTIN_REALMS */

    compiled_code_p->status_flags |= CBC_CODE_FLAGS_UINT16_ARGUMENTS;
    byte_code_p += sizeof (cbc_uint16_arguments_t);
  }
  else
  {
    cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) compiled_code_p;

    args_p->stack_limit = (uint8_t) context_p->stack_limit;
    args_p->argument_end = (uint8_t) context_p->argument_count;
    args_p->register_end = (uint8_t) context_p->register_count;
    args_p->ident_end = (uint8_t) ident_end;
    args_p->const_literal_end = (uint8_t) const_literal_end;
    args_p->literal_end = (uint8_t) context_p->literal_count;
#if JERRY_BUILTIN_REALMS
    ECMA_SET_INTERNAL_VALUE_POINTER (args_p->realm_value, JERRY_CONTEXT (global_object_p));
#endif /* JERRY_BUILTIN_REALMS */

    byte_code_p += sizeof (cbc_uint8_arguments_t);
  }

  uint16_t encoding_limit;
  uint16_t encoding_delta;

  if (context_p->literal_count > CBC_MAXIMUM_SMALL_VALUE)
  {
    compiled_code_p->status_flags |= CBC_CODE_FLAGS_FULL_LITERAL_ENCODING;
    encoding_limit = CBC_FULL_LITERAL_ENCODING_LIMIT;
    encoding_delta = CBC_FULL_LITERAL_ENCODING_DELTA;
  }
  else
  {
    encoding_limit = CBC_SMALL_LITERAL_ENCODING_LIMIT;
    encoding_delta = CBC_SMALL_LITERAL_ENCODING_DELTA;
  }

  if (context_p->status_flags & PARSER_IS_STRICT)
  {
    compiled_code_p->status_flags |= CBC_CODE_FLAGS_STRICT_MODE;
  }

  if ((context_p->status_flags & PARSER_ARGUMENTS_NEEDED)
      && PARSER_NEEDS_MAPPED_ARGUMENTS (context_p->status_flags))
  {
    compiled_code_p->status_flags |= CBC_CODE_FLAGS_MAPPED_ARGUMENTS_NEEDED;
  }

  if (!(context_p->status_flags & PARSER_LEXICAL_ENV_NEEDED))
  {
    compiled_code_p->status_flags |= CBC_CODE_FLAGS_LEXICAL_ENV_NOT_NEEDED;
  }

  uint16_t function_type = CBC_FUNCTION_TO_TYPE_BITS (CBC_FUNCTION_NORMAL);

  if (context_p->status_flags & (PARSER_IS_PROPERTY_GETTER | PARSER_IS_PROPERTY_SETTER))
  {
    function_type = CBC_FUNCTION_TO_TYPE_BITS (CBC_FUNCTION_ACCESSOR);
  }
  else if (!(context_p->status_flags & PARSER_IS_FUNCTION))
  {
    function_type = CBC_FUNCTION_TO_TYPE_BITS (CBC_FUNCTION_SCRIPT);
  }
#if JERRY_ESNEXT
  else if (context_p->status_flags & PARSER_IS_ARROW_FUNCTION)
  {
    if (context_p->status_flags & PARSER_IS_ASYNC_FUNCTION)
    {
      function_type = CBC_FUNCTION_TO_TYPE_BITS (CBC_FUNCTION_ASYNC_ARROW);
    }
    else
    {
      function_type = CBC_FUNCTION_TO_TYPE_BITS (CBC_FUNCTION_ARROW);
    }
  }
  else if (context_p->status_flags & PARSER_IS_GENERATOR_FUNCTION)
  {
    if (context_p->status_flags & PARSER_IS_ASYNC_FUNCTION)
    {
      function_type = CBC_FUNCTION_TO_TYPE_BITS (CBC_FUNCTION_ASYNC_GENERATOR);
    }
    else
    {
      function_type = CBC_FUNCTION_TO_TYPE_BITS (CBC_FUNCTION_GENERATOR);
    }
  }
  else if (context_p->status_flags & PARSER_IS_ASYNC_FUNCTION)
  {
    function_type = CBC_FUNCTION_TO_TYPE_BITS (CBC_FUNCTION_ASYNC);
  }
  else if (context_p->status_flags & PARSER_CLASS_CONSTRUCTOR)
  {
    function_type = CBC_FUNCTION_TO_TYPE_BITS (CBC_FUNCTION_CONSTRUCTOR);
  }
  else if (context_p->status_flags & PARSER_IS_METHOD)
  {
    function_type = CBC_FUNCTION_TO_TYPE_BITS (CBC_FUNCTION_METHOD);
  }

  if (context_p->status_flags & PARSER_LEXICAL_BLOCK_NEEDED)
  {
    JERRY_ASSERT (!(context_p->status_flags & PARSER_IS_FUNCTION));
    compiled_code_p->status_flags |= CBC_CODE_FLAGS_LEXICAL_BLOCK_NEEDED;
  }
#endif /* JERRY_ESNEXT */

  compiled_code_p->status_flags |= function_type;

  literal_pool_p = ((ecma_value_t *) byte_code_p) - context_p->register_count;
  byte_code_p += literal_length;
  dst_p = byte_code_p;

  parser_init_literal_pool (context_p, literal_pool_p);

#if JERRY_RESOURCE_NAME
  literal_pool_p[const_literal_end - 1] = context_p->resource_name;
#endif /* JERRY_RESOURCE_NAME */

  page_p = context_p->byte_code.first_p;
  offset = 0;
  real_offset = 0;
  uint8_t last_register_index = (uint8_t) JERRY_MIN (context_p->register_count,
                                                     (PARSER_MAXIMUM_NUMBER_OF_REGISTERS - 1));

  while (page_p != last_page_p || offset < last_position)
  {
    uint8_t flags;
    uint8_t *opcode_p;
    uint8_t *branch_mark_p;
    cbc_opcode_t opcode;
    size_t branch_offset_length;

    opcode_p = dst_p;
    branch_mark_p = page_p->bytes + offset;
    opcode = (cbc_opcode_t) (*branch_mark_p);
    branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (opcode);

    if (opcode == CBC_JUMP_FORWARD)
    {
      /* These opcodes are deleted from the stream. */
      size_t counter = PARSER_MAX_BRANCH_LENGTH + 1;

      do
      {
        PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
      }
      while (--counter > 0);

      continue;
    }

    /* Storing the opcode */
    *dst_p++ = (uint8_t) opcode;
    real_offset++;
    PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
    flags = cbc_flags[opcode];

#if JERRY_DEBUGGER
    if (opcode == CBC_BREAKPOINT_DISABLED)
    {
      uint32_t bp_offset = (uint32_t) (((uint8_t *) dst_p) - ((uint8_t *) compiled_code_p) - 1);
      parser_append_breakpoint_info (context_p, JERRY_DEBUGGER_BREAKPOINT_OFFSET_LIST, bp_offset);
    }
#endif /* JERRY_DEBUGGER */

    if (opcode == CBC_EXT_OPCODE)
    {
      cbc_ext_opcode_t ext_opcode;

      ext_opcode = (cbc_ext_opcode_t) page_p->bytes[offset];
      flags = cbc_ext_flags[ext_opcode];
      branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (ext_opcode);

      /* Storing the extended opcode */
      *dst_p++ = (uint8_t) ext_opcode;
      opcode_p++;
      real_offset++;
      PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);

#if JERRY_LINE_INFO
      if (ext_opcode == CBC_EXT_LINE)
      {
        uint8_t last_byte = 0;

        do
        {
          last_byte = page_p->bytes[offset];
          *dst_p++ = last_byte;

          real_offset++;
          PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
        }
        while (last_byte & CBC_HIGHEST_BIT_MASK);

        continue;
      }
#endif /* JERRY_LINE_INFO */
    }

    /* Only literal and call arguments can be combined. */
    JERRY_ASSERT (!(flags & CBC_HAS_BRANCH_ARG)
                   || !(flags & (CBC_HAS_BYTE_ARG | CBC_HAS_LITERAL_ARG)));

    while (flags & (CBC_HAS_LITERAL_ARG | CBC_HAS_LITERAL_ARG2))
    {
      uint16_t first_byte = page_p->bytes[offset];

      uint8_t *opcode_pos_p = dst_p - 1;
      *dst_p++ = (uint8_t) first_byte;
      real_offset++;
      PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);

      if (first_byte > literal_one_byte_limit)
      {
        *dst_p++ = page_p->bytes[offset];

        if (first_byte >= encoding_limit)
        {
          first_byte = (uint16_t) (((first_byte << 8) | dst_p[-1]) - encoding_delta);
        }
        real_offset++;
      }
      PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);

      if (flags & CBC_HAS_LITERAL_ARG2)
      {
        if (flags & CBC_HAS_LITERAL_ARG)
        {
          flags = CBC_HAS_LITERAL_ARG;
        }
        else
        {
          flags = CBC_HAS_LITERAL_ARG | CBC_HAS_LITERAL_ARG2;
        }
      }
      else
      {
        if (opcode == CBC_ASSIGN_SET_IDENT && JERRY_LIKELY (first_byte < last_register_index))
        {
          *opcode_pos_p = CBC_MOV_IDENT;
        }

        break;
      }
    }

    if (flags & CBC_HAS_BYTE_ARG)
    {
      /* This argument will be copied without modification. */
      *dst_p++ = page_p->bytes[offset];
      real_offset++;
      PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
      continue;
    }

    if (flags & CBC_HAS_BRANCH_ARG)
    {
      *branch_mark_p |= CBC_HIGHEST_BIT_MASK;
      bool prefix_zero = true;

      /* The leading zeroes are dropped from the stream. */
      JERRY_ASSERT (branch_offset_length > 0 && branch_offset_length <= 3);

      while (--branch_offset_length > 0)
      {
        uint8_t byte = page_p->bytes[offset];
        if (byte > 0 || !prefix_zero)
        {
          prefix_zero = false;
          *dst_p++ = page_p->bytes[offset];
          real_offset++;
        }
        else
        {
          /* When a leading zero is dropped, the branch
           * offset length must be decreased as well. */
          (*opcode_p)--;
        }
        PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
      }

      *dst_p++ = page_p->bytes[offset];
      real_offset++;
      PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
      continue;
    }
  }

#if JERRY_DEBUGGER
  if ((JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
      && context_p->breakpoint_info_count > 0)
  {
    parser_send_breakpoints (context_p, JERRY_DEBUGGER_BREAKPOINT_OFFSET_LIST);
    JERRY_ASSERT (context_p->breakpoint_info_count == 0);
  }
#endif /* JERRY_DEBUGGER */

  if (!(context_p->status_flags & PARSER_NO_END_LABEL))
  {
    *dst_p++ = CBC_RETURN_WITH_BLOCK;

#if JERRY_ESNEXT
    if (PARSER_IS_NORMAL_ASYNC_FUNCTION (context_p->status_flags))
    {
      dst_p[-1] = CBC_EXT_OPCODE;
      dst_p[0] = CBC_EXT_ASYNC_EXIT;
      dst_p++;
    }
#endif /* JERRY_ESNEXT */
  }
  JERRY_ASSERT (dst_p == byte_code_p + length);

  parse_update_branches (context_p, byte_code_p);

  parser_cbc_stream_free (&context_p->byte_code);

  if (context_p->status_flags & PARSER_HAS_LATE_LIT_INIT)
  {
    parser_list_iterator_t literal_iterator;
    lexer_literal_t *literal_p;
    uint16_t register_count = context_p->register_count;

    parser_list_iterator_init (&context_p->literal_pool, &literal_iterator);
    while ((literal_p = (lexer_literal_t *) parser_list_iterator_next (&literal_iterator)))
    {
      if ((literal_p->status_flags & LEXER_FLAG_LATE_INIT)
          && literal_p->prop.index >= register_count)
      {
        uint32_t source_data = literal_p->u.source_data;
        const uint8_t *char_p = context_p->source_end_p - (source_data & 0xfffff);
        ecma_value_t lit_value = ecma_find_or_create_literal_string (char_p,
                                                                     source_data >> 20,
                                                                     literal_p->status_flags & LEXER_FLAG_ASCII);
        literal_pool_p[literal_p->prop.index] = lit_value;
      }
    }
  }

  ecma_value_t *base_p = (ecma_value_t *) (((uint8_t *) compiled_code_p) + total_size);

  if (PARSER_NEEDS_MAPPED_ARGUMENTS (context_p->status_flags))
  {
    parser_list_iterator_t literal_iterator;
    uint16_t argument_count = 0;
    uint16_t register_count = context_p->register_count;
    base_p -= context_p->argument_count;

    parser_list_iterator_init (&context_p->literal_pool, &literal_iterator);
    while (argument_count < context_p->argument_count)
    {
      lexer_literal_t *literal_p;
      literal_p = (lexer_literal_t *) parser_list_iterator_next (&literal_iterator);

      JERRY_ASSERT (literal_p != NULL);

      if (!(literal_p->status_flags & LEXER_FLAG_FUNCTION_ARGUMENT))
      {
        continue;
      }

      /* All arguments must be moved to initialized registers. */
      if (literal_p->type == LEXER_UNUSED_LITERAL)
      {
        base_p[argument_count] = ECMA_VALUE_EMPTY;
        argument_count++;
        continue;
      }

      JERRY_ASSERT (literal_p->type == LEXER_IDENT_LITERAL);

      JERRY_ASSERT (literal_p->prop.index >= register_count);

      base_p[argument_count] = literal_pool_p[literal_p->prop.index];
      argument_count++;
    }
  }

#if JERRY_ESNEXT
  if (!(context_p->status_flags & PARSER_CLASS_CONSTRUCTOR))
  {
    *(--base_p) = ecma_make_magic_string_value (LIT_MAGIC_STRING__EMPTY);
  }

  if (context_p->argument_length != UINT16_MAX)
  {
    compiled_code_p->status_flags |= CBC_CODE_FLAGS_HAS_EXTENDED_INFO;
    *(--base_p) = context_p->argument_length;
  }

  if (context_p->tagged_template_literal_cp != JMEM_CP_NULL)
  {
    compiled_code_p->status_flags |= CBC_CODE_FLAGS_HAS_TAGGED_LITERALS;
    base_p[-1] = (ecma_value_t) context_p->tagged_template_literal_cp;
  }
#endif /* JERRY_ESNEXT */

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
    util_print_cbc (compiled_code_p);
    JERRY_DEBUG_MSG ("\nByte code size: %d bytes\n", (int) length);
    context_p->total_byte_code_size += (uint32_t) length;
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

#if JERRY_DEBUGGER
  if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
  {
    jerry_debugger_send_function_cp (JERRY_DEBUGGER_BYTE_CODE_CP, compiled_code_p);
  }
#endif /* JERRY_DEBUGGER */

  return compiled_code_p;
} /* parser_post_processing */

#undef PARSER_NEXT_BYTE
#undef PARSER_NEXT_BYTE_UPDATE

/**
 * Free identifiers and literals.
 */
static void
parser_free_literals (parser_list_t *literal_pool_p) /**< literals */
{
  parser_list_iterator_t literal_iterator;
  lexer_literal_t *literal_p;

  parser_list_iterator_init (literal_pool_p, &literal_iterator);
  while ((literal_p = (lexer_literal_t *) parser_list_iterator_next (&literal_iterator)) != NULL)
  {
    util_free_literal (literal_p);
  }

  parser_list_free (literal_pool_p);
} /* parser_free_literals */

/**
 * Parse function arguments
 */
static void
parser_parse_function_arguments (parser_context_t *context_p, /**< context */
                                 lexer_token_type_t end_type) /**< expected end type */
{
  JERRY_ASSERT (context_p->next_scanner_info_p->type == SCANNER_TYPE_FUNCTION);

#if JERRY_ESNEXT
  JERRY_ASSERT (context_p->status_flags & PARSER_IS_FUNCTION);
  JERRY_ASSERT (!(context_p->status_flags & PARSER_LEXICAL_BLOCK_NEEDED));

  bool has_duplicated_arg_names = false;

  if (PARSER_IS_NORMAL_ASYNC_FUNCTION (context_p->status_flags))
  {
    parser_branch_t branch;
    parser_emit_cbc_ext_forward_branch (context_p, CBC_EXT_TRY_CREATE_CONTEXT, &branch);
    parser_stack_push (context_p, &branch, sizeof (parser_branch_t));

#ifndef JERRY_NDEBUG
    context_p->context_stack_depth = PARSER_TRY_CONTEXT_STACK_ALLOCATION;
#endif /* !JERRY_NDEBUG */
  }
#endif /* JERRY_ESNEXT */

  if (context_p->token.type == end_type)
  {
#if JERRY_ESNEXT
    context_p->status_flags &= (uint32_t) ~PARSER_DISALLOW_AWAIT_YIELD;

    if (context_p->status_flags & PARSER_IS_GENERATOR_FUNCTION)
    {
      scanner_create_variables (context_p, SCANNER_CREATE_VARS_IS_FUNCTION_ARGS);
      parser_emit_cbc_ext (context_p, CBC_EXT_CREATE_GENERATOR);
      parser_emit_cbc (context_p, CBC_POP);
      scanner_create_variables (context_p, SCANNER_CREATE_VARS_IS_FUNCTION_BODY);
      return;
    }
#endif /* JERRY_ESNEXT */
    scanner_create_variables (context_p, SCANNER_CREATE_VARS_NO_OPTS);
    return;
  }

#if JERRY_ESNEXT
  bool has_complex_argument = (context_p->next_scanner_info_p->u8_arg & SCANNER_FUNCTION_HAS_COMPLEX_ARGUMENT) != 0;
#endif /* JERRY_ESNEXT */
  bool is_strict = (context_p->next_scanner_info_p->u8_arg & SCANNER_FUNCTION_IS_STRICT) != 0;

  scanner_create_variables (context_p, SCANNER_CREATE_VARS_IS_FUNCTION_ARGS);
  scanner_set_active (context_p);

#if JERRY_ESNEXT
  context_p->status_flags |= PARSER_FUNCTION_IS_PARSING_ARGS;
#endif /* JERRY_ESNEXT */

  while (true)
  {
#if JERRY_ESNEXT
    if (context_p->token.type == LEXER_THREE_DOTS)
    {
      if (context_p->status_flags & PARSER_IS_PROPERTY_SETTER)
      {
        parser_raise_error (context_p, PARSER_ERR_SETTER_REST_PARAMETER);
      }
      lexer_next_token (context_p);

      if (has_duplicated_arg_names)
      {
        parser_raise_error (context_p, PARSER_ERR_DUPLICATED_ARGUMENT_NAMES);
      }

      context_p->status_flags |= PARSER_FUNCTION_HAS_REST_PARAM | PARSER_FUNCTION_HAS_COMPLEX_ARGUMENT;
    }

    if (context_p->token.type == LEXER_LEFT_SQUARE || context_p->token.type == LEXER_LEFT_BRACE)
    {
      if (has_duplicated_arg_names)
      {
        parser_raise_error (context_p, PARSER_ERR_DUPLICATED_ARGUMENT_NAMES);
      }

      context_p->status_flags |= PARSER_FUNCTION_HAS_COMPLEX_ARGUMENT;

      if (!(context_p->status_flags & PARSER_FUNCTION_HAS_REST_PARAM))
      {
        parser_emit_cbc_literal (context_p,
                                 CBC_PUSH_LITERAL,
                                 (uint16_t) (PARSER_REGISTER_START + context_p->argument_count));
      }
      else
      {
        parser_emit_cbc_ext (context_p, CBC_EXT_PUSH_REST_OBJECT);
      }

      uint32_t flags = (PARSER_PATTERN_BINDING
                        | PARSER_PATTERN_TARGET_ON_STACK
                        | PARSER_PATTERN_LOCAL
                        | PARSER_PATTERN_ARGUMENTS);

      if (context_p->next_scanner_info_p->source_p == context_p->source_p)
      {
        if (context_p->next_scanner_info_p->type == SCANNER_TYPE_INITIALIZER)
        {
          if (context_p->next_scanner_info_p->u8_arg & SCANNER_LITERAL_OBJECT_HAS_REST)
          {
            flags |= PARSER_PATTERN_HAS_REST_ELEMENT;
          }

          if (context_p->status_flags & PARSER_FUNCTION_HAS_REST_PARAM)
          {
            parser_raise_error (context_p, PARSER_ERR_REST_PARAMETER_DEFAULT_INITIALIZER);
          }

          if (context_p->argument_length == UINT16_MAX)
          {
            context_p->argument_length = context_p->argument_count;
          }

          flags |= PARSER_PATTERN_TARGET_DEFAULT;
        }
        else if (context_p->next_scanner_info_p->type == SCANNER_TYPE_LITERAL_FLAGS)
        {
          if (context_p->next_scanner_info_p->u8_arg & SCANNER_LITERAL_OBJECT_HAS_REST)
          {
            flags |= PARSER_PATTERN_HAS_REST_ELEMENT;
          }
          scanner_release_next (context_p, sizeof (scanner_info_t));
        }
        else
        {
          parser_raise_error (context_p, PARSER_ERR_INVALID_DESTRUCTURING_PATTERN);
        }
      }

      parser_parse_initializer (context_p, flags);

      context_p->argument_count++;
      if (context_p->argument_count >= PARSER_MAXIMUM_NUMBER_OF_REGISTERS)
      {
        parser_raise_error (context_p, PARSER_ERR_ARGUMENT_LIMIT_REACHED);
      }

      if (context_p->token.type != LEXER_COMMA)
      {
        if (context_p->token.type != end_type)
        {
          parser_error_t error = ((end_type == LEXER_RIGHT_PAREN) ? PARSER_ERR_RIGHT_PAREN_EXPECTED
                                                                  : PARSER_ERR_IDENTIFIER_EXPECTED);

          parser_raise_error (context_p, error);
        }
        break;
      }

      lexer_next_token (context_p);

      if (context_p->token.type == end_type)
      {
        break;
      }
      continue;
    }
#endif /* JERRY_ESNEXT */

    if (context_p->token.type != LEXER_LITERAL
        || context_p->token.lit_location.type != LEXER_IDENT_LITERAL)
    {
      parser_raise_error (context_p, PARSER_ERR_IDENTIFIER_EXPECTED);
    }

    lexer_construct_literal_object (context_p,
                                    &context_p->token.lit_location,
                                    LEXER_IDENT_LITERAL);

    if (context_p->token.keyword_type >= LEXER_FIRST_NON_STRICT_ARGUMENTS)
    {
      context_p->status_flags |= PARSER_HAS_NON_STRICT_ARG;
    }

    if (JERRY_UNLIKELY (context_p->lit_object.literal_p->status_flags & LEXER_FLAG_FUNCTION_ARGUMENT))
    {
#if JERRY_ESNEXT
      if ((context_p->status_flags & PARSER_FUNCTION_HAS_COMPLEX_ARGUMENT)
          || (context_p->status_flags & PARSER_IS_ARROW_FUNCTION))
      {
        parser_raise_error (context_p, PARSER_ERR_DUPLICATED_ARGUMENT_NAMES);
      }
      has_duplicated_arg_names = true;
#endif /* JERRY_ESNEXT */

      context_p->status_flags |= PARSER_HAS_NON_STRICT_ARG;
    }
    else
    {
      context_p->lit_object.literal_p->status_flags |= LEXER_FLAG_FUNCTION_ARGUMENT;
    }

    lexer_next_token (context_p);

#if JERRY_ESNEXT
    uint16_t literal_index = context_p->lit_object.index;

    if (context_p->token.type == LEXER_ASSIGN)
    {
      JERRY_ASSERT (has_complex_argument);

      if (context_p->status_flags & PARSER_FUNCTION_HAS_REST_PARAM)
      {
        parser_raise_error (context_p, PARSER_ERR_REST_PARAMETER_DEFAULT_INITIALIZER);
      }

      if (context_p->argument_length == UINT16_MAX)
      {
        context_p->argument_length = context_p->argument_count;
      }

      parser_branch_t skip_init;

      if (has_duplicated_arg_names)
      {
        parser_raise_error (context_p, PARSER_ERR_DUPLICATED_ARGUMENT_NAMES);
      }

      context_p->status_flags |= PARSER_FUNCTION_HAS_COMPLEX_ARGUMENT;

      /* LEXER_ASSIGN does not overwrite lit_object. */
      parser_emit_cbc_literal (context_p,
                               CBC_PUSH_LITERAL,
                               (uint16_t) (PARSER_REGISTER_START + context_p->argument_count));
      parser_emit_cbc_ext_forward_branch (context_p, CBC_EXT_DEFAULT_INITIALIZER, &skip_init);

      lexer_next_token (context_p);
      parser_parse_expression (context_p, PARSE_EXPR_NO_COMMA);

      parser_set_branch_to_current_position (context_p, &skip_init);

      uint16_t opcode = CBC_ASSIGN_LET_CONST;

      if (literal_index >= PARSER_REGISTER_START)
      {
        opcode = CBC_MOV_IDENT;
      }
      else if (!scanner_literal_is_created (context_p, literal_index))
      {
        opcode = CBC_INIT_ARG_OR_CATCH;
      }

      parser_emit_cbc_literal (context_p, opcode, literal_index);
    }
    else if (context_p->status_flags & PARSER_FUNCTION_HAS_REST_PARAM)
    {
      parser_emit_cbc_ext (context_p, CBC_EXT_PUSH_REST_OBJECT);

      uint16_t opcode = CBC_MOV_IDENT;

      if (literal_index < PARSER_REGISTER_START)
      {
        opcode = CBC_INIT_ARG_OR_CATCH;

        if (scanner_literal_is_created (context_p, literal_index))
        {
          opcode = CBC_ASSIGN_LET_CONST;
        }
      }

      parser_emit_cbc_literal (context_p, opcode, literal_index);
    }
    else if (has_complex_argument && literal_index < PARSER_REGISTER_START)
    {
      uint16_t opcode = CBC_INIT_ARG_OR_FUNC;

      if (scanner_literal_is_created (context_p, literal_index))
      {
        opcode = CBC_ASSIGN_LET_CONST_LITERAL;
      }

      parser_emit_cbc_literal_value (context_p,
                                     opcode,
                                     (uint16_t) (PARSER_REGISTER_START + context_p->argument_count),
                                     literal_index);
    }
#endif /* JERRY_ESNEXT */

    context_p->argument_count++;
    if (context_p->argument_count >= PARSER_MAXIMUM_NUMBER_OF_REGISTERS)
    {
      parser_raise_error (context_p, PARSER_ERR_ARGUMENT_LIMIT_REACHED);
    }

    if (context_p->token.type != LEXER_COMMA)
    {
      if (context_p->token.type != end_type)
      {
        parser_error_t error = ((end_type == LEXER_RIGHT_PAREN) ? PARSER_ERR_RIGHT_PAREN_EXPECTED
                                                                : PARSER_ERR_IDENTIFIER_EXPECTED);

        parser_raise_error (context_p, error);
      }
      break;
    }

#if JERRY_ESNEXT
    if (context_p->status_flags & PARSER_FUNCTION_HAS_REST_PARAM)
    {
      parser_raise_error (context_p, PARSER_ERR_FORMAL_PARAM_AFTER_REST_PARAMETER);
    }
#endif /* JERRY_ESNEXT */

    lexer_next_token (context_p);

#if JERRY_ESNEXT
    if (context_p->token.type == end_type)
    {
      break;
    }
#endif /* JERRY_ESNEXT */
  }

  scanner_revert_active (context_p);

#if JERRY_ESNEXT
  JERRY_ASSERT (has_complex_argument || !(context_p->status_flags & PARSER_FUNCTION_HAS_COMPLEX_ARGUMENT));

  if (context_p->status_flags & PARSER_IS_GENERATOR_FUNCTION)
  {
    parser_emit_cbc_ext (context_p, CBC_EXT_CREATE_GENERATOR);
    parser_emit_cbc (context_p, CBC_POP);
  }

  if (context_p->status_flags & PARSER_LEXICAL_BLOCK_NEEDED)
  {
    if ((context_p->next_scanner_info_p->u8_arg & SCANNER_FUNCTION_LEXICAL_ENV_NEEDED)
        || scanner_is_context_needed (context_p, PARSER_CHECK_FUNCTION_CONTEXT))
    {
      context_p->status_flags |= PARSER_LEXICAL_ENV_NEEDED;

      parser_branch_t branch;
      parser_emit_cbc_forward_branch (context_p, CBC_BLOCK_CREATE_CONTEXT, &branch);
      parser_stack_push (context_p, &branch, sizeof (parser_branch_t));

#ifndef JERRY_NDEBUG
      PARSER_PLUS_EQUAL_U16 (context_p->context_stack_depth, PARSER_BLOCK_CONTEXT_STACK_ALLOCATION);
#endif /* !JERRY_NDEBUG */
    }
    else
    {
      context_p->status_flags &= (uint32_t) ~PARSER_LEXICAL_BLOCK_NEEDED;
    }
  }

  context_p->status_flags &= (uint32_t) ~(PARSER_DISALLOW_AWAIT_YIELD | PARSER_FUNCTION_IS_PARSING_ARGS);
#endif /* JERRY_ESNEXT */

  scanner_create_variables (context_p, SCANNER_CREATE_VARS_IS_FUNCTION_BODY);

  if (is_strict)
  {
    context_p->status_flags |= PARSER_IS_STRICT;
  }
} /* parser_parse_function_arguments */

#ifndef JERRY_NDEBUG
JERRY_STATIC_ASSERT (PARSER_SCANNING_SUCCESSFUL == PARSER_HAS_LATE_LIT_INIT,
                     parser_scanning_successful_should_share_the_bit_position_with_parser_has_late_lit_init);
#endif /* !JERRY_NDEBUG */

/**
 * Parse and compile EcmaScript source code
 *
 * Note: source must be a valid UTF-8 string
 *
 * @return compiled code
 */
static ecma_compiled_code_t *
parser_parse_source (const uint8_t *arg_list_p, /**< function argument list */
                     size_t arg_list_size, /**< size of function argument list */
                     const uint8_t *source_p, /**< valid UTF-8 source code */
                     size_t source_size, /**< size of the source code */
                     uint32_t parse_opts, /**< ecma_parse_opts_t option bits */
                     const ecma_parse_options_t *options_p) /**< additional configuration options */
{
  parser_context_t context;
  ecma_compiled_code_t *compiled_code_p;

  context.error = PARSER_ERR_NO_ERROR;
  context.status_flags = parse_opts & PARSER_STRICT_MODE_MASK;
  context.global_status_flags = parse_opts;

#if JERRY_MODULE_SYSTEM
  if (context.global_status_flags & ECMA_PARSE_MODULE)
  {
    context.status_flags |= PARSER_IS_STRICT;
  }

  context.module_names_p = NULL;
#endif /* JERRY_MODULE_SYSTEM */

  if (arg_list_p != NULL)
  {
    context.status_flags |= PARSER_IS_FUNCTION;
#if JERRY_ESNEXT
    if (parse_opts & ECMA_PARSE_GENERATOR_FUNCTION)
    {
      context.status_flags |= PARSER_IS_GENERATOR_FUNCTION;
    }
    if (parse_opts & ECMA_PARSE_ASYNC_FUNCTION)
    {
      context.status_flags |= PARSER_IS_ASYNC_FUNCTION;
    }
#endif /* JERRY_ESNEXT */
  }

#if JERRY_ESNEXT
  context.status_flags |= PARSER_RESTORE_STATUS_FLAGS (parse_opts);
  context.tagged_template_literal_cp = JMEM_CP_NULL;
#endif /* JERRY_ESNEXT */

  context.stack_depth = 0;
  context.stack_limit = 0;
  context.options_p = options_p;
  context.last_context_p = NULL;
  context.last_statement.current_p = NULL;
  context.token.flags = 0;
  lexer_init_line_info (&context);

#if JERRY_RESOURCE_NAME
  ecma_value_t resource_name = ecma_make_magic_string_value (LIT_MAGIC_STRING_RESOURCE_ANON);

  if (context.global_status_flags & ECMA_PARSE_EVAL)
  {
    resource_name = ecma_make_magic_string_value (LIT_MAGIC_STRING_RESOURCE_EVAL);
  }

  if (context.options_p != NULL
      && (context.options_p->options & JERRY_PARSE_HAS_RESOURCE)
      && context.options_p->resource_name_length > 0)
  {
    resource_name = ecma_find_or_create_literal_string (context.options_p->resource_name_p,
                                                        (lit_utf8_size_t) context.options_p->resource_name_length,
                                                        context.status_flags & LEXER_FLAG_ASCII);
  }

  context.resource_name = resource_name;
#endif /* !JERRY_RESOURCE_NAME */

  scanner_info_t scanner_info_end;
  scanner_info_end.next_p = NULL;
  scanner_info_end.source_p = NULL;
  scanner_info_end.type = SCANNER_TYPE_END;
  context.next_scanner_info_p = &scanner_info_end;
  context.active_scanner_info_p = NULL;
  context.skipped_scanner_info_p = NULL;
  context.skipped_scanner_info_end_p = NULL;

  context.last_cbc_opcode = PARSER_CBC_UNAVAILABLE;

  context.argument_count = 0;
#if JERRY_ESNEXT
  context.argument_length = UINT16_MAX;
#endif /* JERRY_ESNEXT */
  context.register_count = 0;
  context.literal_count = 0;

  parser_cbc_stream_init (&context.byte_code);
  context.byte_code_size = 0;
  parser_list_init (&context.literal_pool,
                    sizeof (lexer_literal_t),
                    (uint32_t) ((128 - sizeof (void *)) / sizeof (lexer_literal_t)));
  context.scope_stack_p = NULL;
  context.scope_stack_size = 0;
  context.scope_stack_top = 0;
  context.scope_stack_reg_top = 0;
#if JERRY_ESNEXT
  context.scope_stack_global_end = 0;
  context.tagged_template_literal_cp = JMEM_CP_NULL;
#endif /* JERRY_ESNEXT */

#ifndef JERRY_NDEBUG
  context.context_stack_depth = 0;
#endif /* !JERRY_NDEBUG */

#if JERRY_PARSER_DUMP_BYTE_CODE
  context.is_show_opcodes = (JERRY_CONTEXT (jerry_init_flags) & JERRY_INIT_SHOW_OPCODES);
  context.total_byte_code_size = 0;

  if (context.is_show_opcodes)
  {
    JERRY_DEBUG_MSG ("\n--- %s parsing start ---\n\n",
                     (arg_list_p == NULL) ? "Script"
                                          : "Function");
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  scanner_scan_all (&context,
                    arg_list_p,
                    arg_list_p + arg_list_size,
                    source_p,
                    source_p + source_size);

  if (JERRY_UNLIKELY (context.error != PARSER_ERR_NO_ERROR))
  {
    JERRY_ASSERT (context.error == PARSER_ERR_OUT_OF_MEMORY);

    /* It is unlikely that memory can be allocated in an out-of-memory
     * situation. However, a simple value can still be thrown. */
    jcontext_raise_exception (ECMA_VALUE_NULL);
    return NULL;
  }

  if (arg_list_p == NULL)
  {
    context.source_p = source_p;
    context.source_end_p = source_p + source_size;
  }
  else
  {
    context.source_p = arg_list_p;
    context.source_end_p = arg_list_p + arg_list_size;
  }

  context.u.allocated_buffer_p = NULL;
  context.token.flags = 0;
  lexer_init_line_info (&context);

  parser_stack_init (&context);

#if JERRY_DEBUGGER
  context.breakpoint_info_count = 0;
#endif /* JERRY_DEBUGGER */

  JERRY_ASSERT (context.next_scanner_info_p->source_p == context.source_p);
  JERRY_ASSERT (context.next_scanner_info_p->type == SCANNER_TYPE_FUNCTION);

  if (context.next_scanner_info_p->u8_arg & SCANNER_FUNCTION_IS_STRICT)
  {
    context.status_flags |= PARSER_IS_STRICT;
  }

  PARSER_TRY (context.try_buffer)
  {
    /* Pushing a dummy value ensures the stack is never empty.
     * This simplifies the stack management routines. */
    parser_stack_push_uint8 (&context, CBC_MAXIMUM_BYTE_VALUE);
    /* The next token must always be present to make decisions
     * in the parser. Therefore when a token is consumed, the
     * lexer_next_token() must be immediately called. */
    lexer_next_token (&context);

    if (arg_list_p != NULL)
    {
      parser_parse_function_arguments (&context, LEXER_EOS);

      JERRY_ASSERT (context.next_scanner_info_p->type == SCANNER_TYPE_END_ARGUMENTS);
      scanner_release_next (&context, sizeof (scanner_info_t));

      context.source_p = source_p;
      context.source_end_p = source_p + source_size;
      lexer_init_line_info (&context);

      lexer_next_token (&context);
    }
#if JERRY_MODULE_SYSTEM
    else if (parse_opts & ECMA_PARSE_MODULE)
    {
      parser_branch_t branch;
      parser_emit_cbc_forward_branch (&context, CBC_JUMP_FORWARD, &branch);

      scanner_create_variables (&context, SCANNER_CREATE_VARS_NO_OPTS);
      parser_emit_cbc (&context, CBC_RETURN_WITH_BLOCK);

      parser_set_branch_to_current_position (&context, &branch);
    }
#endif /* JERRY_MODULE_SYSTEM */
    else
    {
      JERRY_ASSERT (context.next_scanner_info_p->source_p == source_p
                    && context.next_scanner_info_p->type == SCANNER_TYPE_FUNCTION);

#if JERRY_ESNEXT
      if (scanner_is_context_needed (&context, PARSER_CHECK_GLOBAL_CONTEXT))
      {
        context.status_flags |= PARSER_LEXICAL_BLOCK_NEEDED;
      }

      if (!(parse_opts & ECMA_PARSE_EVAL))
      {
        scanner_check_variables (&context);
      }
#endif /* JERRY_ESNEXT */

      scanner_create_variables (&context, SCANNER_CREATE_VARS_IS_SCRIPT);
    }

    parser_parse_statements (&context);

    JERRY_ASSERT (context.last_statement.current_p == NULL);

    JERRY_ASSERT (context.last_cbc_opcode == PARSER_CBC_UNAVAILABLE);
    JERRY_ASSERT (context.u.allocated_buffer_p == NULL);

#ifndef JERRY_NDEBUG
    JERRY_ASSERT (context.status_flags & PARSER_SCANNING_SUCCESSFUL);
    JERRY_ASSERT (!(context.global_status_flags & ECMA_PARSE_INTERNAL_FOR_IN_OFF_CONTEXT_ERROR));
    context.status_flags &= (uint32_t) ~PARSER_SCANNING_SUCCESSFUL;
#endif /* !JERRY_NDEBUG */

    JERRY_ASSERT (!(context.status_flags & PARSER_HAS_LATE_LIT_INIT));

    compiled_code_p = parser_post_processing (&context);
    parser_list_free (&context.literal_pool);

    /* When parsing is successful, only the dummy value can be remained on the stack. */
    JERRY_ASSERT (context.stack_top_uint8 == CBC_MAXIMUM_BYTE_VALUE
                  && context.stack.last_position == 1
                  && context.stack.first_p != NULL
                  && context.stack.first_p->next_p == NULL
                  && context.stack.last_p == NULL);

    JERRY_ASSERT (arg_list_p != NULL || !(context.status_flags & PARSER_ARGUMENTS_NEEDED));

#if JERRY_PARSER_DUMP_BYTE_CODE
    if (context.is_show_opcodes)
    {
      JERRY_DEBUG_MSG ("\n%s parsing successfully completed. Total byte code size: %d bytes\n",
                       (arg_list_p == NULL) ? "Script"
                                            : "Function",
                       (int) context.total_byte_code_size);
    }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */
  }
  PARSER_CATCH
  {
    if (context.last_statement.current_p != NULL)
    {
      parser_free_jumps (context.last_statement);
    }

    parser_free_allocated_buffer (&context);

    scanner_cleanup (&context);

#if JERRY_MODULE_SYSTEM
    if (context.module_names_p != NULL)
    {
      ecma_module_release_module_names (context.module_names_p);
    }
#endif

    compiled_code_p = NULL;
    parser_free_literals (&context.literal_pool);
    parser_cbc_stream_free (&context.byte_code);
  }
  PARSER_TRY_END

  if (context.scope_stack_p != NULL)
  {
    parser_free (context.scope_stack_p, context.scope_stack_size * sizeof (parser_scope_stack_t));
  }

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context.is_show_opcodes)
  {
    JERRY_DEBUG_MSG ("\n--- %s parsing end ---\n\n",
                     (arg_list_p == NULL) ? "Script"
                                          : "Function");
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  parser_stack_free (&context);

  if (compiled_code_p != NULL)
  {
    return compiled_code_p;
  }

#if JERRY_DEBUGGER
  if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
  {
    jerry_debugger_send_type (JERRY_DEBUGGER_PARSE_ERROR);
  }
#endif /* JERRY_DEBUGGER */

  if (context.error == PARSER_ERR_OUT_OF_MEMORY)
  {
    /* It is unlikely that memory can be allocated in an out-of-memory
     * situation. However, a simple value can still be thrown. */
    jcontext_raise_exception (ECMA_VALUE_NULL);
    return NULL;
  }

#if JERRY_ERROR_MESSAGES
  ecma_string_t *err_str_p;

  if (context.error == PARSER_ERR_INVALID_REGEXP)
  {
    ecma_value_t error = jcontext_take_exception ();
    ecma_property_t *prop_p = ecma_find_named_property (ecma_get_object_from_value (error),
                                                        ecma_get_magic_string (LIT_MAGIC_STRING_MESSAGE));
    ecma_free_value (error);
    JERRY_ASSERT (prop_p);
    err_str_p = ecma_get_string_from_value (ECMA_PROPERTY_VALUE_PTR (prop_p)->value);
    ecma_ref_ecma_string (err_str_p);
  }
  else
  {
    const lit_utf8_byte_t *err_bytes_p = (const lit_utf8_byte_t *) parser_error_to_string (context.error);
    lit_utf8_size_t err_bytes_size = lit_zt_utf8_string_size (err_bytes_p);
    err_str_p = ecma_new_ecma_string_from_utf8 (err_bytes_p, err_bytes_size);
  }
  ecma_value_t err_str_val = ecma_make_string_value (err_str_p);
  ecma_value_t line_str_val = ecma_make_uint32_value (context.token.line);
  ecma_value_t col_str_val = ecma_make_uint32_value (context.token.column);

  ecma_raise_standard_error_with_format (JERRY_ERROR_SYNTAX,
                                         "% [%:%:%]",
                                         err_str_val,
                                         context.resource_name,
                                         line_str_val,
                                         col_str_val);

  ecma_free_value (col_str_val);
  ecma_free_value (line_str_val);
  ecma_deref_ecma_string (err_str_p);
#else /* !JERRY_ERROR_MESSAGES */
  if (context.error == PARSER_ERR_INVALID_REGEXP)
  {
    jcontext_release_exception ();
  }

  ecma_raise_syntax_error ("");
#endif /* JERRY_ERROR_MESSAGES */

  return NULL;
} /* parser_parse_source */

/**
 * Save parser context before function parsing.
 */
static void
parser_save_context (parser_context_t *context_p, /**< context */
                     parser_saved_context_t *saved_context_p) /**< target for saving the context */
{
  JERRY_ASSERT (context_p->last_cbc_opcode == PARSER_CBC_UNAVAILABLE);

#if JERRY_DEBUGGER
  if ((JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
      && context_p->breakpoint_info_count > 0)
  {
    parser_send_breakpoints (context_p, JERRY_DEBUGGER_BREAKPOINT_LIST);
    context_p->breakpoint_info_count = 0;
  }
#endif /* JERRY_DEBUGGER */

#if JERRY_ESNEXT
  if (context_p->status_flags & PARSER_FUNCTION_IS_PARSING_ARGS)
  {
    context_p->status_flags |= PARSER_LEXICAL_BLOCK_NEEDED;
  }
#endif /* JERRY_ESNEXT */

  /* Save private part of the context. */

  saved_context_p->status_flags = context_p->status_flags;
  saved_context_p->stack_depth = context_p->stack_depth;
  saved_context_p->stack_limit = context_p->stack_limit;
  saved_context_p->prev_context_p = context_p->last_context_p;
  saved_context_p->last_statement = context_p->last_statement;

  saved_context_p->argument_count = context_p->argument_count;
#if JERRY_ESNEXT
  saved_context_p->argument_length = context_p->argument_length;
#endif /* JERRY_ESNEXT */
  saved_context_p->register_count = context_p->register_count;
  saved_context_p->literal_count = context_p->literal_count;

  saved_context_p->byte_code = context_p->byte_code;
  saved_context_p->byte_code_size = context_p->byte_code_size;
  saved_context_p->literal_pool_data = context_p->literal_pool.data;
  saved_context_p->scope_stack_p = context_p->scope_stack_p;
  saved_context_p->scope_stack_size = context_p->scope_stack_size;
  saved_context_p->scope_stack_top = context_p->scope_stack_top;
  saved_context_p->scope_stack_reg_top = context_p->scope_stack_reg_top;
#if JERRY_ESNEXT
  saved_context_p->scope_stack_global_end = context_p->scope_stack_global_end;
  saved_context_p->tagged_template_literal_cp = context_p->tagged_template_literal_cp;
#endif /* JERRY_ESNEXT */

#ifndef JERRY_NDEBUG
  saved_context_p->context_stack_depth = context_p->context_stack_depth;
#endif /* !JERRY_NDEBUG */

  /* Reset private part of the context. */

  context_p->status_flags &= PARSER_IS_STRICT;
  context_p->stack_depth = 0;
  context_p->stack_limit = 0;
  context_p->last_context_p = saved_context_p;
  context_p->last_statement.current_p = NULL;

  context_p->argument_count = 0;
#if JERRY_ESNEXT
  context_p->argument_length = UINT16_MAX;
#endif /* JERRY_ESNEXT */
  context_p->register_count = 0;
  context_p->literal_count = 0;

  parser_cbc_stream_init (&context_p->byte_code);
  context_p->byte_code_size = 0;
  parser_list_reset (&context_p->literal_pool);
  context_p->scope_stack_p = NULL;
  context_p->scope_stack_size = 0;
  context_p->scope_stack_top = 0;
  context_p->scope_stack_reg_top = 0;
#if JERRY_ESNEXT
  context_p->scope_stack_global_end = 0;
  context_p->tagged_template_literal_cp = JMEM_CP_NULL;
#endif /* JERRY_ESNEXT */

#ifndef JERRY_NDEBUG
  context_p->context_stack_depth = 0;
#endif /* !JERRY_NDEBUG */
} /* parser_save_context */

/**
 * Restore parser context after function parsing.
 */
static void
parser_restore_context (parser_context_t *context_p, /**< context */
                        parser_saved_context_t *saved_context_p) /**< target for saving the context */
{
  parser_list_free (&context_p->literal_pool);

  if (context_p->scope_stack_p != NULL)
  {
    parser_free (context_p->scope_stack_p, context_p->scope_stack_size * sizeof (parser_scope_stack_t));
  }

  /* Restore private part of the context. */

  JERRY_ASSERT (context_p->last_cbc_opcode == PARSER_CBC_UNAVAILABLE);

  context_p->status_flags = saved_context_p->status_flags;
  context_p->stack_depth = saved_context_p->stack_depth;
  context_p->stack_limit = saved_context_p->stack_limit;
  context_p->last_context_p = saved_context_p->prev_context_p;
  context_p->last_statement = saved_context_p->last_statement;

  context_p->argument_count = saved_context_p->argument_count;
#if JERRY_ESNEXT
  context_p->argument_length = saved_context_p->argument_length;
#endif /* JERRY_ESNEXT */
  context_p->register_count = saved_context_p->register_count;
  context_p->literal_count = saved_context_p->literal_count;

  context_p->byte_code = saved_context_p->byte_code;
  context_p->byte_code_size = saved_context_p->byte_code_size;
  context_p->literal_pool.data = saved_context_p->literal_pool_data;
  context_p->scope_stack_p = saved_context_p->scope_stack_p;
  context_p->scope_stack_size = saved_context_p->scope_stack_size;
  context_p->scope_stack_top = saved_context_p->scope_stack_top;
  context_p->scope_stack_reg_top = saved_context_p->scope_stack_reg_top;
#if JERRY_ESNEXT
  context_p->scope_stack_global_end = saved_context_p->scope_stack_global_end;
  context_p->tagged_template_literal_cp = saved_context_p->tagged_template_literal_cp;
#endif /* JERRY_ESNEXT */

#ifndef JERRY_NDEBUG
  context_p->context_stack_depth = saved_context_p->context_stack_depth;
#endif /* !JERRY_NDEBUG */
} /* parser_restore_context */

/**
 * Parse function code
 *
 * @return compiled code
 */
ecma_compiled_code_t *
parser_parse_function (parser_context_t *context_p, /**< context */
                       uint32_t status_flags) /**< extra status flags */
{
  parser_saved_context_t saved_context;
  ecma_compiled_code_t *compiled_code_p;

  JERRY_ASSERT (status_flags & PARSER_IS_FUNCTION);
  parser_save_context (context_p, &saved_context);
  context_p->status_flags |= status_flags;
#if JERRY_ESNEXT
  context_p->status_flags |= PARSER_ALLOW_NEW_TARGET;
#endif /* JERRY_ESNEXT */

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
#if JERRY_ESNEXT
    JERRY_DEBUG_MSG ("\n--- %s parsing start ---\n\n",
                     (context_p->status_flags & PARSER_CLASS_CONSTRUCTOR) ? "Class constructor"
                                                                          : "Function");
#else /* !JERRY_ESNEXT */
    JERRY_DEBUG_MSG ("\n--- Function parsing start ---\n\n");
#endif /* JERRY_ESNEXT */
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

#if JERRY_DEBUGGER
  if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
  {
    jerry_debugger_send_parse_function (context_p->token.line, context_p->token.column);
  }
#endif /* JERRY_DEBUGGER */

  lexer_next_token (context_p);

  if (context_p->token.type != LEXER_LEFT_PAREN)
  {
    parser_raise_error (context_p, PARSER_ERR_ARGUMENT_LIST_EXPECTED);
  }

  lexer_next_token (context_p);

  parser_parse_function_arguments (context_p, LEXER_RIGHT_PAREN);
  lexer_next_token (context_p);

  if ((context_p->status_flags & PARSER_IS_PROPERTY_GETTER)
      && context_p->argument_count != 0)
  {
    parser_raise_error (context_p, PARSER_ERR_NO_ARGUMENTS_EXPECTED);
  }

  if ((context_p->status_flags & PARSER_IS_PROPERTY_SETTER)
      && context_p->argument_count != 1)
  {
    parser_raise_error (context_p, PARSER_ERR_ONE_ARGUMENT_EXPECTED);
  }

#if JERRY_ESNEXT
  if ((context_p->status_flags & (PARSER_CLASS_CONSTRUCTOR | PARSER_ALLOW_SUPER_CALL)) == PARSER_CLASS_CONSTRUCTOR)
  {
    parser_emit_cbc_ext (context_p, CBC_EXT_RUN_FIELD_INIT);
    parser_flush_cbc (context_p);
  }
#endif /* JERRY_ESNEXT */

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes
      && (context_p->status_flags & PARSER_HAS_NON_STRICT_ARG))
  {
    JERRY_DEBUG_MSG ("  Note: legacy (non-strict) argument definition\n\n");
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  if (context_p->token.type != LEXER_LEFT_BRACE)
  {
    parser_raise_error (context_p, PARSER_ERR_LEFT_BRACE_EXPECTED);
  }

  lexer_next_token (context_p);
  parser_parse_statements (context_p);
  compiled_code_p = parser_post_processing (context_p);

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
#if JERRY_ESNEXT
    JERRY_DEBUG_MSG ("\n--- %s parsing end ---\n\n",
                     (context_p->status_flags & PARSER_CLASS_CONSTRUCTOR) ? "Class constructor"
                                                                          : "Function");
#else /* !JERRY_ESNEXT */
    JERRY_DEBUG_MSG ("\n--- Function parsing end ---\n\n");
#endif /* JERRY_ESNEXT */
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  parser_restore_context (context_p, &saved_context);

  return compiled_code_p;
} /* parser_parse_function */

#if JERRY_ESNEXT

/**
 * Parse arrow function code
 *
 * @return compiled code
 */
ecma_compiled_code_t *
parser_parse_arrow_function (parser_context_t *context_p, /**< context */
                             uint32_t status_flags) /**< extra status flags */
{
  parser_saved_context_t saved_context;
  ecma_compiled_code_t *compiled_code_p;

  JERRY_ASSERT (status_flags & PARSER_IS_FUNCTION);
  JERRY_ASSERT (status_flags & PARSER_IS_ARROW_FUNCTION);
  parser_save_context (context_p, &saved_context);
  context_p->status_flags |= status_flags;
  context_p->status_flags |= saved_context.status_flags & (PARSER_ALLOW_NEW_TARGET
                                                           | PARSER_ALLOW_SUPER
                                                           | PARSER_ALLOW_SUPER_CALL);

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
    JERRY_DEBUG_MSG ("\n--- Arrow function parsing start ---\n\n");
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

#if JERRY_DEBUGGER
  if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
  {
    jerry_debugger_send_parse_function (context_p->token.line, context_p->token.column);
  }
#endif /* JERRY_DEBUGGER */

  if (context_p->token.type == LEXER_LEFT_PAREN)
  {
    lexer_next_token (context_p);
    parser_parse_function_arguments (context_p, LEXER_RIGHT_PAREN);
    lexer_next_token (context_p);
  }
  else
  {
    parser_parse_function_arguments (context_p, LEXER_ARROW);
  }

  JERRY_ASSERT (context_p->token.type == LEXER_ARROW);

  lexer_next_token (context_p);

  if (context_p->token.type == LEXER_LEFT_BRACE)
  {
    lexer_next_token (context_p);

    context_p->status_flags |= PARSER_IS_CLOSURE;
    parser_parse_statements (context_p);

    /* Unlike normal function, arrow functions consume their close brace. */
    JERRY_ASSERT (context_p->token.type == LEXER_RIGHT_BRACE);
    lexer_next_token (context_p);
  }
  else
  {
    if (context_p->status_flags & PARSER_IS_STRICT
        && context_p->status_flags & PARSER_HAS_NON_STRICT_ARG)
    {
      parser_raise_error (context_p, PARSER_ERR_NON_STRICT_ARG_DEFINITION);
    }

    parser_parse_expression (context_p, PARSE_EXPR_NO_COMMA);

    if (context_p->last_cbc_opcode == CBC_PUSH_LITERAL)
    {
      context_p->last_cbc_opcode = CBC_RETURN_WITH_LITERAL;
    }
    else
    {
      parser_emit_cbc (context_p, CBC_RETURN);
    }
    parser_flush_cbc (context_p);

    lexer_update_await_yield (context_p, saved_context.status_flags);
  }

  compiled_code_p = parser_post_processing (context_p);

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
    JERRY_DEBUG_MSG ("\n--- Arrow function parsing end ---\n\n");
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  parser_restore_context (context_p, &saved_context);

  return compiled_code_p;
} /* parser_parse_arrow_function */

/**
 * Parse class fields
 *
 * @return compiled code
 */
ecma_compiled_code_t *
parser_parse_class_fields (parser_context_t *context_p) /**< context */
{
  parser_saved_context_t saved_context;
  ecma_compiled_code_t *compiled_code_p;

  uint32_t extra_status_flags = context_p->status_flags & PARSER_INSIDE_WITH;

  parser_save_context (context_p, &saved_context);
  context_p->status_flags |= (PARSER_IS_FUNCTION
                              | PARSER_ALLOW_SUPER
                              | PARSER_INSIDE_CLASS_FIELD
                              | PARSER_ALLOW_NEW_TARGET
                              | extra_status_flags);

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
    JERRY_DEBUG_MSG ("\n--- Class fields parsing start ---\n\n");
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

#if JERRY_DEBUGGER
  if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
  {
    jerry_debugger_send_parse_function (context_p->token.line, context_p->token.column);
  }
#endif /* JERRY_DEBUGGER */

  const uint8_t *source_end_p = context_p->source_end_p;
  bool first_computed_class_field = true;
  scanner_location_t end_location;
  scanner_get_location (&end_location, context_p);

  do
  {
    uint8_t class_field_type = context_p->stack_top_uint8;
    parser_stack_pop_uint8 (context_p);

    scanner_range_t range = {0};

    if (class_field_type & PARSER_CLASS_FIELD_INITIALIZED)
    {
      parser_stack_pop (context_p, &range, sizeof (scanner_range_t));
    }
    else if (class_field_type & PARSER_CLASS_FIELD_NORMAL)
    {
      parser_stack_pop (context_p, &range.start_location, sizeof (scanner_location_t));
    }

    uint16_t literal_index = 0;

    if (class_field_type & PARSER_CLASS_FIELD_NORMAL)
    {
      scanner_set_location (context_p, &range.start_location);
      context_p->source_end_p = source_end_p;
      scanner_seek (context_p);

      lexer_expect_object_literal_id (context_p, LEXER_OBJ_IDENT_ONLY_IDENTIFIERS);

      literal_index = context_p->lit_object.index;

      if (class_field_type & PARSER_CLASS_FIELD_INITIALIZED)
      {
        lexer_next_token (context_p);
        JERRY_ASSERT (context_p->token.type == LEXER_ASSIGN);
      }
    }
    else if (first_computed_class_field)
    {
      parser_emit_cbc (context_p, CBC_PUSH_NUMBER_0);
      first_computed_class_field = false;
    }

    if (class_field_type & PARSER_CLASS_FIELD_INITIALIZED)
    {
      if (!(class_field_type & PARSER_CLASS_FIELD_NORMAL))
      {
        scanner_set_location (context_p, &range.start_location);
        scanner_seek (context_p);
      }

#if JERRY_LINE_INFO
      if (context_p->token.line != context_p->last_line_info_line)
      {
        parser_emit_line_info (context_p, context_p->token.line, true);
      }
#endif /* JERRY_LINE_INFO */

      context_p->source_end_p = range.source_end_p;
      lexer_next_token (context_p);
      parser_parse_expression (context_p, PARSE_EXPR_NO_COMMA);

      if (context_p->token.type != LEXER_EOS)
      {
        parser_raise_error (context_p, PARSER_ERR_SEMICOLON_EXPECTED);
      }
    }
    else
    {
      parser_emit_cbc (context_p, CBC_PUSH_UNDEFINED);
    }

    if (class_field_type & PARSER_CLASS_FIELD_NORMAL)
    {
      parser_emit_cbc_literal (context_p, CBC_ASSIGN_PROP_THIS_LITERAL, literal_index);

      /* Prepare stack slot for assignment property reference base. Needed by vm.c */
      if (context_p->stack_limit == context_p->stack_depth)
      {
        context_p->stack_limit++;
        JERRY_ASSERT (context_p->stack_limit <= PARSER_MAXIMUM_STACK_LIMIT);
      }
    }
    else
    {
      parser_flush_cbc (context_p);

      /* The next opcode pushes two more temporary values onto the stack */
      if (context_p->stack_depth + 1 > context_p->stack_limit)
      {
        context_p->stack_limit = (uint16_t) (context_p->stack_depth + 1);
        if (context_p->stack_limit > PARSER_MAXIMUM_STACK_LIMIT)
        {
          parser_raise_error (context_p, PARSER_ERR_STACK_LIMIT_REACHED);
        }
      }

      parser_emit_cbc_ext (context_p, CBC_EXT_SET_NEXT_COMPUTED_FIELD);
    }
  }
  while (!(context_p->stack_top_uint8 & PARSER_CLASS_FIELD_END));

  if (!first_computed_class_field)
  {
    parser_emit_cbc (context_p, CBC_POP);
  }

  parser_flush_cbc (context_p);
  context_p->source_end_p = source_end_p;
  scanner_set_location (context_p, &end_location);

  compiled_code_p = parser_post_processing (context_p);

#if JERRY_PARSER_DUMP_BYTE_CODE
  if (context_p->is_show_opcodes)
  {
    JERRY_DEBUG_MSG ("\n--- Class fields parsing end ---\n\n");
  }
#endif /* JERRY_PARSER_DUMP_BYTE_CODE */

  parser_restore_context (context_p, &saved_context);

  return compiled_code_p;
} /* parser_parse_class_fields */

/**
 * Check whether the last emitted cbc opcode was an anonymous function declaration
 *
 * @return PARSER_NOT_FUNCTION_LITERAL - if the last opcode is not a function literal
 *         PARSER_NAMED_FUNCTION - if the last opcode is not a named function declataion
 *         PARSER_ANONYMOUS_CLASS - if the last opcode is an anonymous class declaration
 *         literal index of the anonymous function literal - otherwise
 */
uint16_t
parser_check_anonymous_function_declaration (parser_context_t *context_p) /**< context */
{
  if (context_p->last_cbc_opcode == PARSER_TO_EXT_OPCODE (CBC_EXT_FINALIZE_ANONYMOUS_CLASS))
  {
    return PARSER_ANONYMOUS_CLASS;
  }

  if (context_p->last_cbc.literal_type != LEXER_FUNCTION_LITERAL)
  {
    return PARSER_NOT_FUNCTION_LITERAL;
  }

  uint16_t literal_index = PARSER_NOT_FUNCTION_LITERAL;

  if (context_p->last_cbc_opcode == CBC_PUSH_LITERAL)
  {
    literal_index = context_p->last_cbc.literal_index;
  }
  else if (context_p->last_cbc_opcode == CBC_PUSH_TWO_LITERALS)
  {
    literal_index = context_p->last_cbc.value;
  }
  else if (context_p->last_cbc_opcode == CBC_PUSH_THREE_LITERALS)
  {
    literal_index = context_p->last_cbc.third_literal_index;
  }
  else
  {
    return PARSER_NOT_FUNCTION_LITERAL;
  }

  const ecma_compiled_code_t *bytecode_p;
  bytecode_p = (const ecma_compiled_code_t *) (PARSER_GET_LITERAL (literal_index)->u.bytecode_p);
  bool is_anon = ecma_is_value_magic_string (*ecma_compiled_code_resolve_function_name (bytecode_p),
                                             LIT_MAGIC_STRING__EMPTY);

  return (is_anon ? literal_index : PARSER_NAMED_FUNCTION);
} /* parser_check_anonymous_function_declaration */

/**
 * Set the function name of the function literal corresponds to the given function literal index
 * to the given character buffer of literal corresponds to the given name index.
 */
void
parser_set_function_name (parser_context_t *context_p, /**< context */
                          uint16_t function_literal_index, /**< function literal index */
                          uint16_t name_index, /**< function name literal index */
                          uint32_t status_flags) /**< status flags */
{
  ecma_compiled_code_t *bytecode_p;
  bytecode_p = (ecma_compiled_code_t *) (PARSER_GET_LITERAL (function_literal_index)->u.bytecode_p);

  parser_compiled_code_set_function_name (context_p, bytecode_p, name_index, status_flags);
} /* parser_set_function_name */

/**
 * Set the function name of the given compiled code
 * to the given character buffer of literal corresponds to the given name index.
 */
void
parser_compiled_code_set_function_name (parser_context_t *context_p, /**< context */
                                        ecma_compiled_code_t *bytecode_p, /**< function literal index */
                                        uint16_t name_index, /**< function name literal index */
                                        uint32_t status_flags) /**< status flags */
{
  ecma_value_t *func_name_start_p;
  func_name_start_p = ecma_compiled_code_resolve_function_name ((const ecma_compiled_code_t *) bytecode_p);

  if (JERRY_UNLIKELY (!ecma_is_value_magic_string (*func_name_start_p, LIT_MAGIC_STRING__EMPTY)))
  {
    return;
  }

  parser_scope_stack_t *scope_stack_start_p = context_p->scope_stack_p;
  parser_scope_stack_t *scope_stack_p = scope_stack_start_p + context_p->scope_stack_top;

  while (scope_stack_p > scope_stack_start_p)
  {
    scope_stack_p--;

    if (scope_stack_p->map_from != PARSER_SCOPE_STACK_FUNC
        && scanner_decode_map_to (scope_stack_p) == name_index)
    {
      name_index = scope_stack_p->map_from;
      break;
    }
  }

  lexer_literal_t *name_lit_p = (lexer_literal_t *) PARSER_GET_LITERAL (name_index);

  if (name_lit_p->type != LEXER_IDENT_LITERAL && name_lit_p->type != LEXER_STRING_LITERAL)
  {
    return;
  }

  uint8_t *name_buffer_p = (uint8_t *) name_lit_p->u.char_p;
  uint32_t name_length = name_lit_p->prop.length;

  if (status_flags & (PARSER_IS_PROPERTY_GETTER | PARSER_IS_PROPERTY_SETTER))
  {
    name_length += 4;
    name_buffer_p = (uint8_t *) parser_malloc (context_p, name_length * sizeof (uint8_t));
    char *prefix_p = (status_flags & PARSER_IS_PROPERTY_GETTER) ? "get " : "set ";
    memcpy (name_buffer_p, prefix_p, 4);
    memcpy (name_buffer_p + 4, name_lit_p->u.char_p, name_lit_p->prop.length);
  }

  *func_name_start_p = ecma_find_or_create_literal_string (name_buffer_p, name_length, status_flags & LEXER_FLAG_ASCII);

  if (name_buffer_p != name_lit_p->u.char_p)
  {
    parser_free (name_buffer_p, name_length);
  }
} /* parser_compiled_code_set_function_name */

#endif /* JERRY_ESNEXT */

/**
 * Raise a parse error.
 */
void
parser_raise_error (parser_context_t *context_p, /**< context */
                    parser_error_t error) /**< error code */
{
  /* Must be compatible with the scanner because
   * the lexer might throws errors during prescanning. */
  parser_saved_context_t *saved_context_p = context_p->last_context_p;

  while (saved_context_p != NULL)
  {
    parser_cbc_stream_free (&saved_context_p->byte_code);

    /* First the current literal pool is freed, and then it is replaced
     * by the literal pool coming from the saved context. Since literals
     * are not used anymore, this is a valid replacement. The last pool
     * is freed by parser_parse_source. */

    parser_free_literals (&context_p->literal_pool);
    context_p->literal_pool.data = saved_context_p->literal_pool_data;

    if (context_p->scope_stack_p != NULL)
    {
      parser_free (context_p->scope_stack_p, context_p->scope_stack_size * sizeof (parser_scope_stack_t));
    }
    context_p->scope_stack_p = saved_context_p->scope_stack_p;
    context_p->scope_stack_size = saved_context_p->scope_stack_size;

    if (saved_context_p->last_statement.current_p != NULL)
    {
      parser_free_jumps (saved_context_p->last_statement);
    }

#if JERRY_ESNEXT
    if (saved_context_p->tagged_template_literal_cp != JMEM_CP_NULL)
    {
      ecma_collection_t *collection = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_collection_t,
                                                                       saved_context_p->tagged_template_literal_cp);
      ecma_collection_free_template_literal (collection);
    }
#endif /* JERRY_ESNEXT  */

    saved_context_p = saved_context_p->prev_context_p;
  }

#if JERRY_ESNEXT
  if (context_p->tagged_template_literal_cp != JMEM_CP_NULL)
  {
    ecma_collection_t *collection = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_collection_t,
                                                                     context_p->tagged_template_literal_cp);
    ecma_collection_free_template_literal (collection);
  }
#endif /* JERRY_ESNEXT  */

  context_p->error = error;
  PARSER_THROW (context_p->try_buffer);
  /* Should never been reached. */
  JERRY_ASSERT (0);
} /* parser_raise_error */

#endif /* JERRY_PARSER */

/**
 * Parse EcmaScript source code
 *
 * Note:
 *      if arg_list_p is not NULL, a function body is parsed
 *      returned value must be freed with ecma_free_value
 *
 * @return pointer to compiled byte code - if success
 *         NULL - otherwise
 */
ecma_compiled_code_t *
parser_parse_script (const uint8_t *arg_list_p, /**< function argument list */
                     size_t arg_list_size, /**< size of function argument list */
                     const uint8_t *source_p, /**< source code */
                     size_t source_size, /**< size of the source code */
                     uint32_t parse_opts, /**< ecma_parse_opts_t option bits */
                     const ecma_parse_options_t *options_p) /**< additional configuration options */
{
#if JERRY_PARSER

#if JERRY_DEBUGGER
  if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
  {
    jerry_debugger_send_string (JERRY_DEBUGGER_SOURCE_CODE,
                                JERRY_DEBUGGER_NO_SUBTYPE,
                                source_p,
                                source_size);
  }
#endif /* JERRY_DEBUGGER */

  ecma_compiled_code_t *bytecode_p = parser_parse_source (arg_list_p,
                                                          arg_list_size,
                                                          source_p,
                                                          source_size,
                                                          parse_opts,
                                                          options_p);

  if (JERRY_UNLIKELY (bytecode_p == NULL))
  {
    /* Exception has already thrown. */
    return NULL;
  }

#if JERRY_DEBUGGER
  if ((JERRY_CONTEXT (debugger_flags) & (JERRY_DEBUGGER_CONNECTED | JERRY_DEBUGGER_PARSER_WAIT))
      == (JERRY_DEBUGGER_CONNECTED | JERRY_DEBUGGER_PARSER_WAIT))
  {
    JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_PARSER_WAIT_MODE);
    jerry_debugger_send_type (JERRY_DEBUGGER_WAITING_AFTER_PARSE);

    while (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_PARSER_WAIT_MODE)
    {
      jerry_debugger_receive (NULL);

      if (!(JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED))
      {
        break;
      }

      jerry_debugger_transport_sleep ();
    }
  }
#endif /* JERRY_DEBUGGER */

  return bytecode_p;
#else /* !JERRY_PARSER */
  JERRY_UNUSED (arg_list_p);
  JERRY_UNUSED (arg_list_size);
  JERRY_UNUSED (source_p);
  JERRY_UNUSED (source_size);
  JERRY_UNUSED (parse_opts);
  JERRY_UNUSED (resource_name);

  ecma_raise_syntax_error (ECMA_ERR_MSG ("Source code parsing is disabled"));
  return NULL;
#endif /* JERRY_PARSER */
} /* parser_parse_script */

/**
 * @}
 * @}
 * @}
 */

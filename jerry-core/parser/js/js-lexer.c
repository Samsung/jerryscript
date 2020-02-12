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
#include "ecma-helpers.h"
#include "ecma-function-object.h"
#include "ecma-literal-storage.h"
#include "js-parser-internal.h"
#include "lit-char-helpers.h"
#include "jcontext.h"

#if ENABLED (JERRY_PARSER)

/** \addtogroup parser Parser
 * @{
 *
 * \addtogroup jsparser JavaScript
 * @{
 *
 * \addtogroup jsparser_lexer Lexer
 * @{
 */

JERRY_STATIC_ASSERT (LEXER_NUMBER_BINARY > LEXER_NUMBER_OCTAL,
                     lexer_number_binary_must_be_greater_than_lexer_number_octal);

/**
 * Check whether the UTF-8 intermediate is an octet or not
 */
#define IS_UTF8_INTERMEDIATE_OCTET(byte) (((byte) & LIT_UTF8_EXTRA_BYTE_MASK) == LIT_UTF8_2_BYTE_CODE_POINT_MIN)

/**
 * Align column to the next tab position.
 *
 * @return aligned position
 */
static parser_line_counter_t
align_column_to_tab (parser_line_counter_t column) /**< current column */
{
  /* Tab aligns to zero column start position. */
  return (parser_line_counter_t) (((column + (8u - 1u)) & ~ECMA_STRING_CONTAINER_MASK) + 1u);
} /* align_column_to_tab */

/**
 * Parse hexadecimal character sequence
 *
 * @return character value or UINT32_MAX on error
 */
static lit_code_point_t
lexer_hex_to_code_point (const uint8_t *source_p, /**< current source position */
                         parser_line_counter_t length) /**< source length */
{
  lit_code_point_t result = 0;

  do
  {
    uint32_t byte = *source_p++;

    result <<= 4;

    if (byte >= LIT_CHAR_0 && byte <= LIT_CHAR_9)
    {
      result += byte - LIT_CHAR_0;
    }
    else
    {
      byte = LEXER_TO_ASCII_LOWERCASE (byte);
      if (byte >= LIT_CHAR_LOWERCASE_A && byte <= LIT_CHAR_LOWERCASE_F)
      {
        result += byte - (LIT_CHAR_LOWERCASE_A - 10);
      }
      else
      {
        return UINT32_MAX;
      }
    }
  }
  while (--length > 0);

  return result;
} /* lexer_hex_to_code_point */

#if ENABLED (JERRY_ES2015)

/**
 * Parse hexadecimal character sequence enclosed in braces
 *
 * @return character value or UINT32_MAX on error
 */
static lit_code_point_t
lexer_hex_in_braces_to_code_point (const uint8_t *source_p, /**< current source position */
                                   const uint8_t *source_end_p, /**< source end */
                                   uint32_t *length_p) /**< [out] length of the sequence */
{
  lit_code_point_t result = 0;
  /* Four is the size of \u{} sequence. */
  uint32_t length = 4;

  JERRY_ASSERT (source_p[-1] == LIT_CHAR_LEFT_BRACE);
  JERRY_ASSERT (source_p < source_end_p);

  do
  {
    uint32_t byte = *source_p++;

    result <<= 4;

    if (byte >= LIT_CHAR_0 && byte <= LIT_CHAR_9)
    {
      result += byte - LIT_CHAR_0;
    }
    else
    {
      byte = LEXER_TO_ASCII_LOWERCASE (byte);
      if (byte >= LIT_CHAR_LOWERCASE_A && byte <= LIT_CHAR_LOWERCASE_F)
      {
        result += byte - (LIT_CHAR_LOWERCASE_A - 10);
      }
      else
      {
        return UINT32_MAX;
      }
    }

    if (result >= (LIT_UNICODE_CODE_POINT_MAX + 1) || source_p >= source_end_p)
    {
      return UINT32_MAX;
    }
    length++;
  }
  while (*source_p != LIT_CHAR_RIGHT_BRACE);

  *length_p = length;
  return result;
} /* lexer_hex_in_braces_to_code_point */

#endif /* ENABLED (JERRY_ES2015) */

/**
 * Parse hexadecimal character sequence
 *
 * @return character value
 */
static lit_code_point_t
lexer_unchecked_hex_to_character (const uint8_t **source_p) /**< [in, out] current source position */
{
  lit_code_point_t result = 0;
  const uint8_t *char_p = *source_p;
  uint32_t length = (char_p[-1] == LIT_CHAR_LOWERCASE_U) ? 4 : 2;

#if ENABLED (JERRY_ES2015)
  if (char_p[0] == LIT_CHAR_LEFT_BRACE)
  {
    length = 0;
    char_p++;
  }
#endif /* ENABLED (JERRY_ES2015) */

  while (true)
  {
    uint32_t byte = *char_p++;

    result <<= 4;

    if (byte >= LIT_CHAR_0 && byte <= LIT_CHAR_9)
    {
      result += byte - LIT_CHAR_0;
    }
    else
    {
      JERRY_ASSERT ((byte >= LIT_CHAR_LOWERCASE_A && byte <= LIT_CHAR_LOWERCASE_F)
                    || (byte >= LIT_CHAR_UPPERCASE_A && byte <= LIT_CHAR_UPPERCASE_F));

      result += LEXER_TO_ASCII_LOWERCASE (byte) - (LIT_CHAR_LOWERCASE_A - 10);
    }

    JERRY_ASSERT (result <= LIT_UNICODE_CODE_POINT_MAX);

#if ENABLED (JERRY_ES2015)
    if (length == 0)
    {
      if (*char_p != LIT_CHAR_RIGHT_BRACE)
      {
        continue;
      }
      *source_p = char_p + 1;
      return result;
    }
#endif /* ENABLED (JERRY_ES2015) */

    if (--length == 0)
    {
      *source_p = char_p;
      return result;
    }
  }
} /* lexer_unchecked_hex_to_character */

/**
 * Skip space mode
 */
typedef enum
{
  LEXER_SKIP_SPACES,                 /**< skip spaces mode */
  LEXER_SKIP_SINGLE_LINE_COMMENT,    /**< parse single line comment */
  LEXER_SKIP_MULTI_LINE_COMMENT,     /**< parse multi line comment */
} skip_mode_t;

/**
 * Skip spaces.
 */
static void
lexer_skip_spaces (parser_context_t *context_p) /**< context */
{
  skip_mode_t mode = LEXER_SKIP_SPACES;
  const uint8_t *source_end_p = context_p->source_end_p;

  if (context_p->token.flags & LEXER_NO_SKIP_SPACES)
  {
    context_p->token.flags &= (uint8_t) ~LEXER_NO_SKIP_SPACES;
    return;
  }

  context_p->token.flags = 0;

  while (true)
  {
    if (context_p->source_p >= source_end_p)
    {
      if (mode == LEXER_SKIP_MULTI_LINE_COMMENT)
      {
        parser_raise_error (context_p, PARSER_ERR_UNTERMINATED_MULTILINE_COMMENT);
      }
      return;
    }

    switch (context_p->source_p[0])
    {
      case LIT_CHAR_CR:
      {
        if (context_p->source_p + 1 < source_end_p
            && context_p->source_p[1] == LIT_CHAR_LF)
        {
          context_p->source_p++;
        }
        /* FALLTHRU */
      }

      case LIT_CHAR_LF:
      {
        context_p->line++;
        context_p->column = 0;
        context_p->token.flags = LEXER_WAS_NEWLINE;

        if (mode == LEXER_SKIP_SINGLE_LINE_COMMENT)
        {
          mode = LEXER_SKIP_SPACES;
        }
        /* FALLTHRU */
      }

      case LIT_CHAR_VTAB:
      case LIT_CHAR_FF:
      case LIT_CHAR_SP:
      {
        context_p->source_p++;
        context_p->column++;
        continue;
      }

      case LIT_CHAR_TAB:
      {
        context_p->column = align_column_to_tab (context_p->column);
        context_p->source_p++;
        continue;
      }

      case LIT_CHAR_SLASH:
      {
        if (mode == LEXER_SKIP_SPACES
            && context_p->source_p + 1 < source_end_p)
        {
          if (context_p->source_p[1] == LIT_CHAR_SLASH)
          {
            mode = LEXER_SKIP_SINGLE_LINE_COMMENT;
          }
          else if (context_p->source_p[1] == LIT_CHAR_ASTERISK)
          {
            mode = LEXER_SKIP_MULTI_LINE_COMMENT;
            context_p->token.line = context_p->line;
            context_p->token.column = context_p->column;
          }

          if (mode != LEXER_SKIP_SPACES)
          {
            context_p->source_p += 2;
            PARSER_PLUS_EQUAL_LC (context_p->column, 2);
            continue;
          }
        }
        break;
      }

      case LIT_CHAR_ASTERISK:
      {
        if (mode == LEXER_SKIP_MULTI_LINE_COMMENT
            && context_p->source_p + 1 < source_end_p
            && context_p->source_p[1] == LIT_CHAR_SLASH)
        {
          mode = LEXER_SKIP_SPACES;
          context_p->source_p += 2;
          PARSER_PLUS_EQUAL_LC (context_p->column, 2);
          continue;
        }
        break;
      }

      case 0xc2:
      {
        if (context_p->source_p + 1 < source_end_p
            && context_p->source_p[1] == 0xa0)
        {
          /* Codepoint \u00A0 */
          context_p->source_p += 2;
          context_p->column++;
          continue;
        }
        break;
      }

      case LEXER_NEWLINE_LS_PS_BYTE_1:
      {
        JERRY_ASSERT (context_p->source_p + 2 < source_end_p);
        if (LEXER_NEWLINE_LS_PS_BYTE_23 (context_p->source_p))
        {
          /* Codepoint \u2028 and \u2029 */
          context_p->source_p += 3;
          context_p->line++;
          context_p->column = 1;
          context_p->token.flags = LEXER_WAS_NEWLINE;

          if (mode == LEXER_SKIP_SINGLE_LINE_COMMENT)
          {
            mode = LEXER_SKIP_SPACES;
          }
          continue;
        }
        break;
      }

      case 0xef:
      {
        if (context_p->source_p + 2 < source_end_p
            && context_p->source_p[1] == 0xbb
            && context_p->source_p[2] == 0xbf)
        {
          /* Codepoint \uFEFF */
          context_p->source_p += 3;
          context_p->column++;
          continue;
        }
        break;
      }

      default:
      {
        break;
      }
    }

    if (mode == LEXER_SKIP_SPACES)
    {
      return;
    }

    context_p->source_p++;

    if (context_p->source_p < source_end_p
        && IS_UTF8_INTERMEDIATE_OCTET (context_p->source_p[0]))
    {
      context_p->column++;
    }
  }
} /* lexer_skip_spaces */

#if ENABLED (JERRY_ES2015)
/**
 * Skip all the continuous empty statements.
 */
void
lexer_skip_empty_statements (parser_context_t *context_p) /**< context */
{
  lexer_skip_spaces (context_p);

  while (context_p->source_p < context_p->source_end_p
         && *context_p->source_p == LIT_CHAR_SEMICOLON)
  {
    lexer_consume_next_character (context_p);
    lexer_skip_spaces (context_p);
  }

  context_p->token.flags = (uint8_t) (context_p->token.flags | LEXER_NO_SKIP_SPACES);
} /* lexer_skip_empty_statements */
#endif /* ENABLED (JERRY_ES2015) */

/**
 * Keyword data.
 */
typedef struct
{
  const uint8_t *keyword_p; /**< keyword string */
  lexer_token_type_t type;  /**< keyword token type */
} keyword_string_t;

/**
 * @{
 * Keyword defines
 */
#define LEXER_KEYWORD(name, type) { (const uint8_t *) (name), (type) }
#define LEXER_KEYWORD_LIST_LENGTH(name) (const uint8_t) (sizeof ((name)) / sizeof ((name)[0]))
/** @} */

/**
 * Length of the shortest keyword.
 */
#define LEXER_KEYWORD_MIN_LENGTH 2

/**
 * Length of the longest keyword.
 */
#define LEXER_KEYWORD_MAX_LENGTH 10

/**
 * Keywords with 2 characters.
 */
static const keyword_string_t keywords_with_length_2[] =
{
  LEXER_KEYWORD ("do", LEXER_KEYW_DO),
  LEXER_KEYWORD ("if", LEXER_KEYW_IF),
  LEXER_KEYWORD ("in", LEXER_KEYW_IN),
};

/**
 * Keywords with 3 characters.
 */
static const keyword_string_t keywords_with_length_3[] =
{
  LEXER_KEYWORD ("for", LEXER_KEYW_FOR),
  LEXER_KEYWORD ("let", LEXER_KEYW_LET),
  LEXER_KEYWORD ("new", LEXER_KEYW_NEW),
  LEXER_KEYWORD ("try", LEXER_KEYW_TRY),
  LEXER_KEYWORD ("var", LEXER_KEYW_VAR),
};

/**
 * Keywords with 4 characters.
 */
static const keyword_string_t keywords_with_length_4[] =
{
  LEXER_KEYWORD ("case", LEXER_KEYW_CASE),
  LEXER_KEYWORD ("else", LEXER_KEYW_ELSE),
  LEXER_KEYWORD ("enum", LEXER_KEYW_ENUM),
  LEXER_KEYWORD ("eval", LEXER_KEYW_EVAL),
  LEXER_KEYWORD ("null", LEXER_LIT_NULL),
  LEXER_KEYWORD ("this", LEXER_KEYW_THIS),
  LEXER_KEYWORD ("true", LEXER_LIT_TRUE),
  LEXER_KEYWORD ("void", LEXER_KEYW_VOID),
  LEXER_KEYWORD ("with", LEXER_KEYW_WITH),
};

/**
 * Keywords with 5 characters.
 */
static const keyword_string_t keywords_with_length_5[] =
{
#if ENABLED (JERRY_ES2015)
  LEXER_KEYWORD ("async", LEXER_KEYW_ASYNC),
  LEXER_KEYWORD ("await", LEXER_KEYW_AWAIT),
#endif /* ENABLED (JERRY_ES2015) */
  LEXER_KEYWORD ("break", LEXER_KEYW_BREAK),
  LEXER_KEYWORD ("catch", LEXER_KEYW_CATCH),
  LEXER_KEYWORD ("class", LEXER_KEYW_CLASS),
  LEXER_KEYWORD ("const", LEXER_KEYW_CONST),
  LEXER_KEYWORD ("false", LEXER_LIT_FALSE),
  LEXER_KEYWORD ("super", LEXER_KEYW_SUPER),
  LEXER_KEYWORD ("throw", LEXER_KEYW_THROW),
  LEXER_KEYWORD ("while", LEXER_KEYW_WHILE),
  LEXER_KEYWORD ("yield", LEXER_KEYW_YIELD),
};

/**
 * Keywords with 6 characters.
 */
static const keyword_string_t keywords_with_length_6[] =
{
  LEXER_KEYWORD ("delete", LEXER_KEYW_DELETE),
  LEXER_KEYWORD ("export", LEXER_KEYW_EXPORT),
  LEXER_KEYWORD ("import", LEXER_KEYW_IMPORT),
  LEXER_KEYWORD ("public", LEXER_KEYW_PUBLIC),
  LEXER_KEYWORD ("return", LEXER_KEYW_RETURN),
  LEXER_KEYWORD ("static", LEXER_KEYW_STATIC),
  LEXER_KEYWORD ("switch", LEXER_KEYW_SWITCH),
  LEXER_KEYWORD ("typeof", LEXER_KEYW_TYPEOF),
};

/**
 * Keywords with 7 characters.
 */
static const keyword_string_t keywords_with_length_7[] =
{
  LEXER_KEYWORD ("default", LEXER_KEYW_DEFAULT),
  LEXER_KEYWORD ("extends", LEXER_KEYW_EXTENDS),
  LEXER_KEYWORD ("finally", LEXER_KEYW_FINALLY),
  LEXER_KEYWORD ("package", LEXER_KEYW_PACKAGE),
  LEXER_KEYWORD ("private", LEXER_KEYW_PRIVATE),
};

/**
 * Keywords with 8 characters.
 */
static const keyword_string_t keywords_with_length_8[] =
{
  LEXER_KEYWORD ("continue", LEXER_KEYW_CONTINUE),
  LEXER_KEYWORD ("debugger", LEXER_KEYW_DEBUGGER),
  LEXER_KEYWORD ("function", LEXER_KEYW_FUNCTION),
};

/**
 * Keywords with 9 characters.
 */
static const keyword_string_t keywords_with_length_9[] =
{
  LEXER_KEYWORD ("arguments", LEXER_KEYW_ARGUMENTS),
  LEXER_KEYWORD ("interface", LEXER_KEYW_INTERFACE),
  LEXER_KEYWORD ("protected", LEXER_KEYW_PROTECTED),
};

/**
 * Keywords with 10 characters.
 */
static const keyword_string_t keywords_with_length_10[] =
{
  LEXER_KEYWORD ("implements", LEXER_KEYW_IMPLEMENTS),
  LEXER_KEYWORD ("instanceof", LEXER_KEYW_INSTANCEOF),
};

/**
 * List of the keyword groups.
 */
static const keyword_string_t * const keyword_strings_list[] =
{
  keywords_with_length_2,
  keywords_with_length_3,
  keywords_with_length_4,
  keywords_with_length_5,
  keywords_with_length_6,
  keywords_with_length_7,
  keywords_with_length_8,
  keywords_with_length_9,
  keywords_with_length_10
};

JERRY_STATIC_ASSERT (sizeof (keyword_strings_list) / sizeof (const keyword_string_t *)
                     == (LEXER_KEYWORD_MAX_LENGTH - LEXER_KEYWORD_MIN_LENGTH) + 1,
                     keyword_strings_list_size_must_equal_to_keyword_max_length_difference);

/**
 * List of the keyword groups length.
 */
static const uint8_t keyword_lengths_list[] =
{
  LEXER_KEYWORD_LIST_LENGTH (keywords_with_length_2),
  LEXER_KEYWORD_LIST_LENGTH (keywords_with_length_3),
  LEXER_KEYWORD_LIST_LENGTH (keywords_with_length_4),
  LEXER_KEYWORD_LIST_LENGTH (keywords_with_length_5),
  LEXER_KEYWORD_LIST_LENGTH (keywords_with_length_6),
  LEXER_KEYWORD_LIST_LENGTH (keywords_with_length_7),
  LEXER_KEYWORD_LIST_LENGTH (keywords_with_length_8),
  LEXER_KEYWORD_LIST_LENGTH (keywords_with_length_9),
  LEXER_KEYWORD_LIST_LENGTH (keywords_with_length_10)
};

#undef LEXER_KEYWORD
#undef LEXER_KEYWORD_LIST_LENGTH

/**
 * Flags for lexer_parse_identifier.
 */
typedef enum
{
  LEXER_PARSE_NO_OPTS = 0, /**< no options */
  LEXER_PARSE_CHECK_KEYWORDS = (1 << 0), /**< check keywords */
  LEXER_PARSE_CHECK_START_AND_RETURN = (1 << 1), /**< check identifier start and return */
  LEXER_PARSE_CHECK_PART_AND_RETURN = (1 << 2), /**< check identifier part and return */
} lexer_parse_options_t;

JERRY_STATIC_ASSERT (LEXER_FIRST_NON_RESERVED_KEYWORD < LEXER_FIRST_FUTURE_STRICT_RESERVED_WORD,
                     lexer_first_non_reserved_keyword_must_be_before_lexer_first_future_strict_reserved_word);

/**
 * Parse identifier.
 *
 * @return true, if an identifier is parsed, false otherwise
 */
static bool
lexer_parse_identifier (parser_context_t *context_p, /**< context */
                        lexer_parse_options_t options) /**< check keywords */
{
  /* Only very few identifiers contains \u escape sequences. */
  const uint8_t *source_p = context_p->source_p;
  /* Note: newline or tab cannot be part of an identifier. */
  parser_line_counter_t column = context_p->column;
  const uint8_t *source_end_p = context_p->source_end_p;
  size_t length = 0;
  uint8_t has_escape = false;

  do
  {
    if (*source_p == LIT_CHAR_BACKSLASH)
    {
      /* After a backslash an identifier must start. */
      lit_code_point_t code_point = UINT32_MAX;
      uint32_t escape_length = 6;

      if (options & (LEXER_PARSE_CHECK_START_AND_RETURN | LEXER_PARSE_CHECK_PART_AND_RETURN))
      {
        return true;
      }

      has_escape = true;

#if ENABLED (JERRY_ES2015)
      if (source_p + 5 <= source_end_p && source_p[1] == LIT_CHAR_LOWERCASE_U)
      {
        if (source_p[2] == LIT_CHAR_LEFT_BRACE)
        {
          code_point = lexer_hex_in_braces_to_code_point (source_p + 3, source_end_p, &escape_length);
        }
        else if (source_p + 6 <= source_end_p)
        {
          code_point = lexer_hex_to_code_point (source_p + 2, 4);
        }
      }
#else /* !ENABLED (JERRY_ES2015) */
      if (source_p + 6 <= source_end_p && source_p[1] == LIT_CHAR_LOWERCASE_U)
      {
        code_point = lexer_hex_to_code_point (source_p + 2, 4);
      }
#endif /* ENABLED (JERRY_ES2015) */

      if (code_point == UINT32_MAX)
      {
        context_p->source_p = source_p;
        context_p->token.column = column;
        parser_raise_error (context_p, PARSER_ERR_INVALID_UNICODE_ESCAPE_SEQUENCE);
      }

      if (length == 0)
      {
        if (!lit_code_point_is_identifier_start (code_point))
        {
          parser_raise_error (context_p, PARSER_ERR_INVALID_IDENTIFIER_START);
        }
      }
      else
      {
        if (!lit_code_point_is_identifier_part (code_point))
        {
          parser_raise_error (context_p, PARSER_ERR_INVALID_IDENTIFIER_PART);
        }
      }

      length += lit_code_point_get_cesu8_length (code_point);
      source_p += escape_length;
      PARSER_PLUS_EQUAL_LC (column, escape_length);
      continue;
    }

    lit_code_point_t code_point = *source_p;
    lit_utf8_size_t utf8_length = 1, decoded_length = 1, char_count = 1;

    if (JERRY_UNLIKELY (code_point >= LIT_UTF8_2_BYTE_MARKER))
    {
#if ENABLED (JERRY_ES2015)
      utf8_length = lit_read_code_point_from_utf8 (source_p,
                                                   (lit_utf8_size_t) (source_end_p - source_p),
                                                   &code_point);
      decoded_length = utf8_length;

      /* Only ES2015 supports code points outside of the basic plane which can be part of an identifier. */
      if ((code_point >= LIT_UTF16_HIGH_SURROGATE_MIN && code_point <= LIT_UTF16_HIGH_SURROGATE_MAX)
          && source_p + 3 < source_end_p)
      {
        lit_code_point_t low_surrogate;
        lit_read_code_point_from_utf8 (source_p + 3,
                                       (lit_utf8_size_t) (source_end_p - (source_p + 3)),
                                       &low_surrogate);

        if (low_surrogate >= LIT_UTF16_LOW_SURROGATE_MIN && low_surrogate <= LIT_UTF16_LOW_SURROGATE_MAX)
        {
          code_point = lit_convert_surrogate_pair_to_code_point ((ecma_char_t) code_point,
                                                                 (ecma_char_t) low_surrogate);
          utf8_length = 2 * 3;
          decoded_length = 2 * 3;
          char_count = 2;
        }
      }
      else if (source_p[0] >= LIT_UTF8_4_BYTE_MARKER)
      {
        decoded_length = 2 * 3;
        has_escape = true;
      }
#else /* !ENABLED (JERRY_ES2015) */
      if (code_point < LIT_UTF8_4_BYTE_MARKER)
      {
        utf8_length = lit_read_code_point_from_utf8 (source_p,
                                                     (lit_utf8_size_t) (source_end_p - source_p),
                                                     &code_point);
        decoded_length = utf8_length;
      }
      else
      {
        code_point = 0;
      }
#endif /* ENABLED (JERRY_ES2015) */
    }

    if (length == 0)
    {
      if (JERRY_UNLIKELY (options & (LEXER_PARSE_CHECK_START_AND_RETURN | LEXER_PARSE_CHECK_PART_AND_RETURN)))
      {
        if (options & LEXER_PARSE_CHECK_START_AND_RETURN)
        {
          return lit_code_point_is_identifier_start (code_point);
        }
        else
        {
          return lit_code_point_is_identifier_part (code_point);
        }
      }

      if (!lit_code_point_is_identifier_start (code_point))
      {
        return false;
      }
    }
    else if (!lit_code_point_is_identifier_part (code_point))
    {
      break;
    }

    source_p += utf8_length;
    length += decoded_length;
    PARSER_PLUS_EQUAL_LC (column, char_count);
  }
  while (source_p < source_end_p);

  JERRY_ASSERT (length > 0);

  context_p->token.type = LEXER_LITERAL;
  context_p->token.keyword_type = LEXER_EOS;
  context_p->token.lit_location.type = LEXER_IDENT_LITERAL;
  context_p->token.lit_location.has_escape = has_escape;

  context_p->token.column = context_p->column;
  context_p->token.lit_location.char_p = context_p->source_p;
  context_p->token.lit_location.length = (prop_length_t) length;

  if (JERRY_UNLIKELY (length > PARSER_MAXIMUM_IDENT_LENGTH))
  {
    parser_raise_error (context_p, PARSER_ERR_IDENTIFIER_TOO_LONG);
  }

  /* Check keywords. */
  if ((options & LEXER_PARSE_CHECK_KEYWORDS)
      && (length >= LEXER_KEYWORD_MIN_LENGTH && length <= LEXER_KEYWORD_MAX_LENGTH))
  {
    const uint8_t *ident_start_p = context_p->source_p;
    uint8_t buffer_p[LEXER_KEYWORD_MAX_LENGTH];

    if (JERRY_UNLIKELY (context_p->token.lit_location.has_escape))
    {
      lexer_convert_ident_to_cesu8 (buffer_p, ident_start_p, (prop_length_t) length);
      ident_start_p = buffer_p;
    }

    const keyword_string_t *keyword_list_p = keyword_strings_list[length - LEXER_KEYWORD_MIN_LENGTH];

    int start = 0;
    int end = keyword_lengths_list[length - LEXER_KEYWORD_MIN_LENGTH];
    int middle = end / 2;

    do
    {
      const keyword_string_t *keyword_p = keyword_list_p + middle;
      int compare_result = ident_start_p[0] - keyword_p->keyword_p[0];

      if (compare_result == 0)
      {
        compare_result = memcmp (ident_start_p, keyword_p->keyword_p, length);

        if (compare_result == 0)
        {
          context_p->token.keyword_type = (uint8_t) keyword_p->type;

          if (JERRY_LIKELY (keyword_p->type < LEXER_FIRST_NON_RESERVED_KEYWORD))
          {
            if (ident_start_p == buffer_p)
            {
              /* Escape sequences are not allowed in a keyword. */
              parser_raise_error (context_p, PARSER_ERR_INVALID_KEYWORD);
            }

            context_p->token.type = (uint8_t) keyword_p->type;
            break;
          }

#if ENABLED (JERRY_ES2015)
          if (keyword_p->type == LEXER_KEYW_YIELD && (context_p->status_flags & PARSER_IS_GENERATOR_FUNCTION))
          {
            if (ident_start_p == buffer_p)
            {
              parser_raise_error (context_p, PARSER_ERR_INVALID_KEYWORD);
            }

            if (context_p->status_flags & PARSER_DISALLOW_YIELD)
            {
              parser_raise_error (context_p, PARSER_ERR_YIELD_NOT_ALLOWED);
            }

            context_p->token.type = (uint8_t) LEXER_KEYW_YIELD;
            break;
          }

          if (keyword_p->type == LEXER_KEYW_LET && ident_start_p != buffer_p)
          {
            if (context_p->status_flags & PARSER_IS_STRICT)
            {
              context_p->token.type = (uint8_t) LEXER_KEYW_LET;
            }
            break;
          }
#endif /* ENABLED (JERRY_ES2015) */

          if (keyword_p->type >= LEXER_FIRST_FUTURE_STRICT_RESERVED_WORD
              && (context_p->status_flags & PARSER_IS_STRICT))
          {
            parser_raise_error (context_p, PARSER_ERR_STRICT_IDENT_NOT_ALLOWED);
          }
          break;
        }
      }

      if (compare_result > 0)
      {
        start = middle + 1;
      }
      else
      {
        JERRY_ASSERT (compare_result < 0);
        end = middle;
      }

      middle = (start + end) / 2;
    }
    while (start < end);
  }

  context_p->source_p = source_p;
  context_p->column = column;
  return true;
} /* lexer_parse_identifier */

/**
 * Parse string.
 */
void
lexer_parse_string (parser_context_t *context_p, /**< context */
                    lexer_string_options_t opts) /**< options */
{
#if ENABLED (JERRY_ES2015)
  const size_t raw_length_inc = (opts & LEXER_STRING_RAW) ? 1 : 0;
#else /* ENABLED (JERRY_ES2015) */
  JERRY_UNUSED (opts);
#endif /* ENABLED (JERRY_ES2015) */

  uint8_t str_end_character = context_p->source_p[0];
  const uint8_t *source_p = context_p->source_p + 1;
  const uint8_t *string_start_p = source_p;
  const uint8_t *source_end_p = context_p->source_end_p;
  parser_line_counter_t line = context_p->line;
  parser_line_counter_t column = (parser_line_counter_t) (context_p->column + 1);
  parser_line_counter_t original_line = line;
  parser_line_counter_t original_column = column;
  size_t length = 0;
  uint8_t has_escape = false;

#if ENABLED (JERRY_ES2015)
  if (str_end_character == LIT_CHAR_RIGHT_BRACE)
  {
    str_end_character = LIT_CHAR_GRAVE_ACCENT;
  }
#endif /* ENABLED (JERRY_ES2015) */

  while (true)
  {
    if (source_p >= source_end_p)
    {
      context_p->token.line = original_line;
      context_p->token.column = (parser_line_counter_t) (original_column - 1);
      parser_raise_error (context_p, PARSER_ERR_UNTERMINATED_STRING);
    }

    if (*source_p == str_end_character)
    {
      break;
    }

    if (*source_p == LIT_CHAR_BACKSLASH)
    {
      source_p++;
      column++;
      if (source_p >= source_end_p)
      {
        /* Will throw an unterminated string error. */
        continue;
      }

#if ENABLED (JERRY_ES2015)
      length += raw_length_inc;
#endif /* ENABLED (JERRY_ES2015) */

      has_escape = true;

      /* Newline is ignored. */
      if (*source_p == LIT_CHAR_CR)
      {
        source_p++;
        if (source_p < source_end_p
            && *source_p == LIT_CHAR_LF)
        {
          source_p++;
        }

        line++;
#if ENABLED (JERRY_ES2015)
        length += raw_length_inc;
#endif /* ENABLED (JERRY_ES2015) */
        column = 1;
        continue;
      }
      else if (*source_p == LIT_CHAR_LF)
      {
        source_p++;
        line++;
#if ENABLED (JERRY_ES2015)
        length += raw_length_inc;
#endif /* ENABLED (JERRY_ES2015) */
        column = 1;
        continue;
      }
      else if (*source_p == LEXER_NEWLINE_LS_PS_BYTE_1 && LEXER_NEWLINE_LS_PS_BYTE_23 (source_p))
      {
        source_p += 3;
        line++;
        column = 1;
        continue;
      }

#if ENABLED (JERRY_ES2015)
      if (opts & LEXER_STRING_RAW)
      {
        continue;
      }
#endif /* ENABLED (JERRY_ES2015) */

      if (*source_p == LIT_CHAR_0
          && source_p + 1 < source_end_p
          && (*(source_p + 1) < LIT_CHAR_0 || *(source_p + 1) > LIT_CHAR_9))
      {
        source_p++;
        column++;
        length++;
        continue;
      }

      /* Except \x, \u, and octal numbers, everything is
       * converted to a character which has the same byte length. */
      if (*source_p >= LIT_CHAR_0 && *source_p <= LIT_CHAR_3)
      {
        if (context_p->status_flags & PARSER_IS_STRICT)
        {
          parser_raise_error (context_p, PARSER_ERR_OCTAL_ESCAPE_NOT_ALLOWED);
        }

        source_p++;
        column++;

        if (source_p < source_end_p && *source_p >= LIT_CHAR_0 && *source_p <= LIT_CHAR_7)
        {
          source_p++;
          column++;

          if (source_p < source_end_p && *source_p >= LIT_CHAR_0 && *source_p <= LIT_CHAR_7)
          {
            /* Numbers >= 0x200 (0x80) requires
             * two bytes for encoding in UTF-8. */
            if (source_p[-2] >= LIT_CHAR_2)
            {
              length++;
            }

            source_p++;
            column++;
          }
        }

        length++;
        continue;
      }

      if (*source_p >= LIT_CHAR_4 && *source_p <= LIT_CHAR_7)
      {
        if (context_p->status_flags & PARSER_IS_STRICT)
        {
          parser_raise_error (context_p, PARSER_ERR_OCTAL_ESCAPE_NOT_ALLOWED);
        }

        source_p++;
        column++;

        if (source_p < source_end_p && *source_p >= LIT_CHAR_0 && *source_p <= LIT_CHAR_7)
        {
          source_p++;
          column++;
        }

        /* The maximum number is 0x4d so the UTF-8
         * representation is always one byte. */
        length++;
        continue;
      }

      if (*source_p == LIT_CHAR_LOWERCASE_X || *source_p == LIT_CHAR_LOWERCASE_U)
      {
        uint32_t escape_length = (*source_p == LIT_CHAR_LOWERCASE_X) ? 3 : 5;
        lit_code_point_t code_point = UINT32_MAX;

        context_p->token.line = line;
        context_p->token.column = (parser_line_counter_t) (column - 1);

#if ENABLED (JERRY_ES2015)
        if (source_p + 4 <= source_end_p
            && source_p[0] == LIT_CHAR_LOWERCASE_U
            && source_p[1] == LIT_CHAR_LEFT_BRACE)
        {
          code_point = lexer_hex_in_braces_to_code_point (source_p + 2, source_end_p, &escape_length);
          escape_length--;
        }
        else
        {
#endif /* ENABLED (JERRY_ES2015) */
          if (source_p + escape_length <= source_end_p)
          {
            code_point = lexer_hex_to_code_point (source_p + 1, escape_length - 1);
          }
#if ENABLED (JERRY_ES2015)
        }
#endif /* ENABLED (JERRY_ES2015) */

        if (code_point == UINT32_MAX)
        {
          parser_raise_error (context_p, PARSER_ERR_INVALID_UNICODE_ESCAPE_SEQUENCE);
        }

        length += lit_code_point_get_cesu8_length (code_point);

        source_p += escape_length;
        PARSER_PLUS_EQUAL_LC (column, escape_length);
        continue;
      }
    }
#if ENABLED (JERRY_ES2015)
    else if (str_end_character == LIT_CHAR_GRAVE_ACCENT &&
             source_p[0] == LIT_CHAR_DOLLAR_SIGN &&
             source_p + 1 < source_end_p &&
             source_p[1] == LIT_CHAR_LEFT_BRACE)
    {
      source_p++;
      break;
    }
#endif /* ENABLED (JERRY_ES2015) */

    if (*source_p >= LIT_UTF8_4_BYTE_MARKER)
    {
      /* Processing 4 byte unicode sequence (even if it is
       * after a backslash). Always converted to two 3 byte
       * long sequence. */
      length += 2 * 3;
      has_escape = true;
      source_p += 4;
      column++;
      continue;
    }
    else if (*source_p == LIT_CHAR_TAB)
    {
      column = align_column_to_tab (column);
      /* Subtract -1 because column is increased below. */
      column--;
    }
#if ENABLED (JERRY_ES2015)
    else if (str_end_character == LIT_CHAR_GRAVE_ACCENT)
    {
      /* Newline (without backslash) is part of the string.
         Note: ECMAScript v6, 11.8.6.1 <CR> or <CR><LF> are both normalized to <LF> */
      if (*source_p == LIT_CHAR_CR)
      {
        has_escape = true;
        source_p++;
        length++;
        if (source_p < source_end_p
            && *source_p == LIT_CHAR_LF)
        {
          source_p++;
        }
        line++;
        column = 1;
        continue;
      }
      else if (*source_p == LIT_CHAR_LF)
      {
        source_p++;
        length++;
        line++;
        column = 1;
        continue;
      }
      else if (*source_p == LEXER_NEWLINE_LS_PS_BYTE_1 && LEXER_NEWLINE_LS_PS_BYTE_23 (source_p))
      {
        source_p += 3;
        length += 3;
        line++;
        column = 1;
        continue;
      }
    }
#endif /* ENABLED (JERRY_ES2015) */
    else if (*source_p == LIT_CHAR_CR
             || *source_p == LIT_CHAR_LF
             || (*source_p == LEXER_NEWLINE_LS_PS_BYTE_1 && LEXER_NEWLINE_LS_PS_BYTE_23 (source_p)))
    {
      context_p->token.line = line;
      context_p->token.column = column;
      parser_raise_error (context_p, PARSER_ERR_NEWLINE_NOT_ALLOWED);
    }

    source_p++;
    column++;
    length++;

    while (source_p < source_end_p
           && IS_UTF8_INTERMEDIATE_OCTET (*source_p))
    {
      source_p++;
      length++;
    }
  }

  if (length > PARSER_MAXIMUM_STRING_LENGTH)
  {
    parser_raise_error (context_p, PARSER_ERR_STRING_TOO_LONG);
  }

#if ENABLED (JERRY_ES2015)
  context_p->token.type = ((str_end_character != LIT_CHAR_GRAVE_ACCENT) ? LEXER_LITERAL
                                                                        : LEXER_TEMPLATE_LITERAL);
#else /* !ENABLED (JERRY_ES2015) */
  context_p->token.type = LEXER_LITERAL;
#endif /* ENABLED (JERRY_ES2015) */

  /* Fill literal data. */
  context_p->token.lit_location.char_p = string_start_p;
  context_p->token.lit_location.length = (prop_length_t) length;
  context_p->token.lit_location.type = LEXER_STRING_LITERAL;
  context_p->token.lit_location.has_escape = has_escape;

  context_p->source_p = source_p + 1;
  context_p->line = line;
  context_p->column = (parser_line_counter_t) (column + 1);
} /* lexer_parse_string */

/**
 * Parse octal number.
 */
static inline void
lexer_parse_octal_number (parser_context_t *context_p, /** context */
                          const uint8_t **source_p) /**< current source position */
{
  do
  {
    (*source_p)++;
  }
  while (*source_p < context_p->source_end_p
         && *source_p[0] >= LIT_CHAR_0
         && *source_p[0] <= LIT_CHAR_7);

  if (*source_p < context_p->source_end_p
      && (*source_p[0] == LIT_CHAR_8 || *source_p[0] == LIT_CHAR_9))
  {
    parser_raise_error (context_p, PARSER_ERR_INVALID_OCTAL_DIGIT);
  }
} /* lexer_parse_octal_number */

/**
 * Parse number.
 */
static void
lexer_parse_number (parser_context_t *context_p) /**< context */
{
  const uint8_t *source_p = context_p->source_p;
  const uint8_t *source_end_p = context_p->source_end_p;
  bool can_be_float = false;
  size_t length;

  context_p->token.type = LEXER_LITERAL;
  context_p->token.keyword_type = LEXER_EOS;
  context_p->token.extra_value = LEXER_NUMBER_DECIMAL;
  context_p->token.lit_location.char_p = source_p;
  context_p->token.lit_location.type = LEXER_NUMBER_LITERAL;
  context_p->token.lit_location.has_escape = false;

  if (source_p[0] == LIT_CHAR_0
      && source_p + 1 < source_end_p)
  {
    if (LEXER_TO_ASCII_LOWERCASE (source_p[1]) == LIT_CHAR_LOWERCASE_X)
    {
      context_p->token.extra_value = LEXER_NUMBER_HEXADECIMAL;
      source_p += 2;

      if (source_p >= source_end_p
          || !lit_char_is_hex_digit (source_p[0]))
      {
        parser_raise_error (context_p, PARSER_ERR_INVALID_HEX_DIGIT);
      }

      do
      {
        source_p++;
      }
      while (source_p < source_end_p
             && lit_char_is_hex_digit (source_p[0]));
    }
#if ENABLED (JERRY_ES2015)
    else if (LEXER_TO_ASCII_LOWERCASE (source_p[1]) == LIT_CHAR_LOWERCASE_O)
    {
      context_p->token.extra_value = LEXER_NUMBER_OCTAL;
      context_p->token.lit_location.char_p++;
      context_p->source_p++;
      source_p += 2;

      if (source_p >= source_end_p
          || !lit_char_is_octal_digit (source_p[0]))
      {
        parser_raise_error (context_p, PARSER_ERR_INVALID_OCTAL_DIGIT);
      }

      lexer_parse_octal_number (context_p, &source_p);
    }
#endif /* ENABLED (JERRY_ES2015) */
    else if (source_p[1] >= LIT_CHAR_0
             && source_p[1] <= LIT_CHAR_7)
    {
      context_p->token.extra_value = LEXER_NUMBER_OCTAL;

      if (context_p->status_flags & PARSER_IS_STRICT)
      {
        parser_raise_error (context_p, PARSER_ERR_OCTAL_NUMBER_NOT_ALLOWED);
      }

      lexer_parse_octal_number (context_p, &source_p);
    }
    else if (source_p[1] >= LIT_CHAR_8
             && source_p[1] <= LIT_CHAR_9)
    {
      parser_raise_error (context_p, PARSER_ERR_INVALID_NUMBER);
    }
#if ENABLED (JERRY_ES2015)
    else if (LEXER_TO_ASCII_LOWERCASE (source_p[1]) == LIT_CHAR_LOWERCASE_B)
    {
      context_p->token.extra_value = LEXER_NUMBER_BINARY;
      context_p->token.lit_location.char_p++;
      context_p->source_p++;
      source_p += 2;

      if (source_p >= source_end_p
          || !lit_char_is_binary_digit (source_p[0]))
      {
        parser_raise_error (context_p, PARSER_ERR_INVALID_BIN_DIGIT);
      }

      do
      {
        source_p++;
      }
      while (source_p < source_end_p
               && lit_char_is_binary_digit (source_p[0]));
    }
#endif /* ENABLED (JERRY_ES2015) */
    else
    {
      can_be_float = true;
      source_p++;
    }
  }
  else
  {
    while (source_p < source_end_p
           && source_p[0] >= LIT_CHAR_0
           && source_p[0] <= LIT_CHAR_9)
    {
      source_p++;
    }

    can_be_float = true;
  }

  if (can_be_float)
  {
    if (source_p < source_end_p
        && source_p[0] == LIT_CHAR_DOT)
    {
      source_p++;
      while (source_p < source_end_p
             && source_p[0] >= LIT_CHAR_0
             && source_p[0] <= LIT_CHAR_9)
      {
        source_p++;
      }
    }

    if (source_p < source_end_p
        && LEXER_TO_ASCII_LOWERCASE (source_p[0]) == LIT_CHAR_LOWERCASE_E)
    {
      source_p++;

      if (source_p < source_end_p
          && (source_p[0] == LIT_CHAR_PLUS || source_p[0] == LIT_CHAR_MINUS))
      {
        source_p++;
      }

      if (source_p >= source_end_p
          || source_p[0] < LIT_CHAR_0
          || source_p[0] > LIT_CHAR_9)
      {
        parser_raise_error (context_p, PARSER_ERR_MISSING_EXPONENT);
      }

      do
      {
        source_p++;
      }
      while (source_p < source_end_p
             && source_p[0] >= LIT_CHAR_0
             && source_p[0] <= LIT_CHAR_9);
    }
  }

  length = (size_t) (source_p - context_p->source_p);
  if (length > PARSER_MAXIMUM_IDENT_LENGTH)
  {
    parser_raise_error (context_p, PARSER_ERR_NUMBER_TOO_LONG);
  }

  context_p->token.lit_location.length = (prop_length_t) length;
  PARSER_PLUS_EQUAL_LC (context_p->column, length);
  context_p->source_p = source_p;

  if (source_p < source_end_p && lexer_parse_identifier (context_p, LEXER_PARSE_CHECK_START_AND_RETURN))
  {
    parser_raise_error (context_p, PARSER_ERR_IDENTIFIER_AFTER_NUMBER);
  }
} /* lexer_parse_number */

/**
 * One character long token (e.g. comma).
 *
 * @param char1 character
 * @param type1 type
 */
#define LEXER_TYPE_A_TOKEN(char1, type1) \
  case (uint8_t) (char1) : \
  { \
    context_p->token.type = (type1); \
    length = 1; \
    break; \
  }

/**
 * Token pair, where the first token is prefix of the second (e.g. % and %=).
 *
 * @param char1 first character
 * @param type1 type of the first character
 * @param char2 second character
 * @param type2 type of the second character
 */
#define LEXER_TYPE_B_TOKEN(char1, type1, char2, type2) \
  case (uint8_t) (char1) : \
  { \
    if (length >= 2 && context_p->source_p[1] == (uint8_t) (char2)) \
    { \
      context_p->token.type = (type2); \
      length = 2; \
      break; \
    } \
    \
    context_p->token.type = (type1); \
    length = 1; \
    break; \
  }

/**
 * Three tokens, where the first is the prefix of the other two (e.g. &, &&, &=).
 *
 * @param char1 first character
 * @param type1 type of the first character
 * @param char2 second character
 * @param type2 type of the second character
 * @param char3 third character
 * @param type3 type of the third character
 */
#define LEXER_TYPE_C_TOKEN(char1, type1, char2, type2, char3, type3) \
  case (uint8_t) (char1) : \
  { \
    if (length >= 2) \
    { \
      if (context_p->source_p[1] == (uint8_t) (char2)) \
      { \
        context_p->token.type = (type2); \
        length = 2; \
        break; \
      } \
      \
      if (context_p->source_p[1] == (uint8_t) (char3)) \
      { \
        context_p->token.type = (type3); \
        length = 2; \
        break; \
      } \
    } \
    \
    context_p->token.type = (type1); \
    length = 1; \
    break; \
  }

/**
 * Get next token.
 */
void
lexer_next_token (parser_context_t *context_p) /**< context */
{
  size_t length;

  lexer_skip_spaces (context_p);

  context_p->token.line = context_p->line;
  context_p->token.column = context_p->column;

  length = (size_t) (context_p->source_end_p - context_p->source_p);
  if (length == 0)
  {
    context_p->token.type = LEXER_EOS;
    return;
  }

  if (lexer_parse_identifier (context_p, LEXER_PARSE_CHECK_KEYWORDS))
  {
    return;
  }

  if (context_p->source_p[0] >= LIT_CHAR_0 && context_p->source_p[0] <= LIT_CHAR_9)
  {
    lexer_parse_number (context_p);
    return;
  }

  switch (context_p->source_p[0])
  {
    LEXER_TYPE_A_TOKEN (LIT_CHAR_LEFT_BRACE, LEXER_LEFT_BRACE);
    LEXER_TYPE_A_TOKEN (LIT_CHAR_LEFT_PAREN, LEXER_LEFT_PAREN);
    LEXER_TYPE_A_TOKEN (LIT_CHAR_LEFT_SQUARE, LEXER_LEFT_SQUARE);
    LEXER_TYPE_A_TOKEN (LIT_CHAR_RIGHT_BRACE, LEXER_RIGHT_BRACE);
    LEXER_TYPE_A_TOKEN (LIT_CHAR_RIGHT_PAREN, LEXER_RIGHT_PAREN);
    LEXER_TYPE_A_TOKEN (LIT_CHAR_RIGHT_SQUARE, LEXER_RIGHT_SQUARE);
    LEXER_TYPE_A_TOKEN (LIT_CHAR_SEMICOLON, LEXER_SEMICOLON);
    LEXER_TYPE_A_TOKEN (LIT_CHAR_COMMA, LEXER_COMMA);

    case (uint8_t) LIT_CHAR_DOT:
    {
      if (length >= 2
          && (context_p->source_p[1] >= LIT_CHAR_0 && context_p->source_p[1] <= LIT_CHAR_9))
      {
        lexer_parse_number (context_p);
        return;
      }

#if ENABLED (JERRY_ES2015)
      if (length >= 3
          && context_p->source_p[1] == LIT_CHAR_DOT
          && context_p->source_p[2] == LIT_CHAR_DOT)
      {
        context_p->token.type = LEXER_THREE_DOTS;
        length = 3;
        break;
      }
#endif /* ENABLED (JERRY_ES2015) */

      context_p->token.type = LEXER_DOT;
      length = 1;
      break;
    }

    case (uint8_t) LIT_CHAR_LESS_THAN:
    {
      if (length >= 2)
      {
        if (context_p->source_p[1] == (uint8_t) LIT_CHAR_EQUALS)
        {
          context_p->token.type = LEXER_LESS_EQUAL;
          length = 2;
          break;
        }

        if (context_p->source_p[1] == (uint8_t) LIT_CHAR_LESS_THAN)
        {
          if (length >= 3 && context_p->source_p[2] == (uint8_t) LIT_CHAR_EQUALS)
          {
            context_p->token.type = LEXER_ASSIGN_LEFT_SHIFT;
            length = 3;
            break;
          }

          context_p->token.type = LEXER_LEFT_SHIFT;
          length = 2;
          break;
        }
      }

      context_p->token.type = LEXER_LESS;
      length = 1;
      break;
    }

    case (uint8_t) LIT_CHAR_GREATER_THAN:
    {
      if (length >= 2)
      {
        if (context_p->source_p[1] == (uint8_t) LIT_CHAR_EQUALS)
        {
          context_p->token.type = LEXER_GREATER_EQUAL;
          length = 2;
          break;
        }

        if (context_p->source_p[1] == (uint8_t) LIT_CHAR_GREATER_THAN)
        {
          if (length >= 3)
          {
            if (context_p->source_p[2] == (uint8_t) LIT_CHAR_EQUALS)
            {
              context_p->token.type = LEXER_ASSIGN_RIGHT_SHIFT;
              length = 3;
              break;
            }

            if (context_p->source_p[2] == (uint8_t) LIT_CHAR_GREATER_THAN)
            {
              if (length >= 4 && context_p->source_p[3] == (uint8_t) LIT_CHAR_EQUALS)
              {
                context_p->token.type = LEXER_ASSIGN_UNS_RIGHT_SHIFT;
                length = 4;
                break;
              }

              context_p->token.type = LEXER_UNS_RIGHT_SHIFT;
              length = 3;
              break;
            }
          }

          context_p->token.type = LEXER_RIGHT_SHIFT;
          length = 2;
          break;
        }
      }

      context_p->token.type = LEXER_GREATER;
      length = 1;
      break;
    }

    case (uint8_t) LIT_CHAR_EQUALS:
    {
      if (length >= 2)
      {
        if (context_p->source_p[1] == (uint8_t) LIT_CHAR_EQUALS)
        {
          if (length >= 3 && context_p->source_p[2] == (uint8_t) LIT_CHAR_EQUALS)
          {
            context_p->token.type = LEXER_STRICT_EQUAL;
            length = 3;
            break;
          }

          context_p->token.type = LEXER_EQUAL;
          length = 2;
          break;
        }

#if ENABLED (JERRY_ES2015)
        if (context_p->source_p[1] == (uint8_t) LIT_CHAR_GREATER_THAN)
        {
          context_p->token.type = LEXER_ARROW;
          length = 2;
          break;
        }
#endif /* ENABLED (JERRY_ES2015) */
      }

      context_p->token.type = LEXER_ASSIGN;
      length = 1;
      break;
    }

    case (uint8_t) LIT_CHAR_EXCLAMATION:
    {
      if (length >= 2 && context_p->source_p[1] == (uint8_t) LIT_CHAR_EQUALS)
      {
        if (length >= 3 && context_p->source_p[2] == (uint8_t) LIT_CHAR_EQUALS)
        {
          context_p->token.type = LEXER_STRICT_NOT_EQUAL;
          length = 3;
          break;
        }

        context_p->token.type = LEXER_NOT_EQUAL;
        length = 2;
        break;
      }

      context_p->token.type = LEXER_LOGICAL_NOT;
      length = 1;
      break;
    }

    LEXER_TYPE_C_TOKEN (LIT_CHAR_PLUS, LEXER_ADD, LIT_CHAR_EQUALS,
                        LEXER_ASSIGN_ADD, LIT_CHAR_PLUS, LEXER_INCREASE)
    LEXER_TYPE_C_TOKEN (LIT_CHAR_MINUS, LEXER_SUBTRACT, LIT_CHAR_EQUALS,
                        LEXER_ASSIGN_SUBTRACT, LIT_CHAR_MINUS, LEXER_DECREASE)

    LEXER_TYPE_B_TOKEN (LIT_CHAR_ASTERISK, LEXER_MULTIPLY, LIT_CHAR_EQUALS,
                        LEXER_ASSIGN_MULTIPLY)
    LEXER_TYPE_B_TOKEN (LIT_CHAR_SLASH, LEXER_DIVIDE, LIT_CHAR_EQUALS,
                        LEXER_ASSIGN_DIVIDE)
    LEXER_TYPE_B_TOKEN (LIT_CHAR_PERCENT, LEXER_MODULO, LIT_CHAR_EQUALS,
                        LEXER_ASSIGN_MODULO)

    LEXER_TYPE_C_TOKEN (LIT_CHAR_AMPERSAND, LEXER_BIT_AND, LIT_CHAR_EQUALS,
                        LEXER_ASSIGN_BIT_AND, LIT_CHAR_AMPERSAND, LEXER_LOGICAL_AND)
    LEXER_TYPE_C_TOKEN (LIT_CHAR_VLINE, LEXER_BIT_OR, LIT_CHAR_EQUALS,
                        LEXER_ASSIGN_BIT_OR, LIT_CHAR_VLINE, LEXER_LOGICAL_OR)

    LEXER_TYPE_B_TOKEN (LIT_CHAR_CIRCUMFLEX, LEXER_BIT_XOR, LIT_CHAR_EQUALS,
                        LEXER_ASSIGN_BIT_XOR)

    LEXER_TYPE_A_TOKEN (LIT_CHAR_TILDE, LEXER_BIT_NOT);
    LEXER_TYPE_A_TOKEN (LIT_CHAR_QUESTION, LEXER_QUESTION_MARK);
    LEXER_TYPE_A_TOKEN (LIT_CHAR_COLON, LEXER_COLON);

    case LIT_CHAR_SINGLE_QUOTE:
    case LIT_CHAR_DOUBLE_QUOTE:
#if ENABLED (JERRY_ES2015)
    case LIT_CHAR_GRAVE_ACCENT:
#endif /* ENABLED (JERRY_ES2015) */
    {
      lexer_parse_string (context_p, LEXER_STRING_NO_OPTS);
      return;
    }

    default:
    {
      parser_raise_error (context_p, PARSER_ERR_INVALID_CHARACTER);
    }
  }

  context_p->source_p += length;
  PARSER_PLUS_EQUAL_LC (context_p->column, length);
} /* lexer_next_token */

#undef LEXER_TYPE_A_TOKEN
#undef LEXER_TYPE_B_TOKEN
#undef LEXER_TYPE_C_TOKEN
#undef LEXER_TYPE_D_TOKEN

/**
 * Checks whether the next token starts with the specified character.
 *
 * @return true - if the next is the specified character
 *         false - otherwise
 */
bool
lexer_check_next_character (parser_context_t *context_p, /**< context */
                            lit_utf8_byte_t character) /**< specified character */
{
  if (!(context_p->token.flags & LEXER_NO_SKIP_SPACES))
  {
    lexer_skip_spaces (context_p);
    context_p->token.flags = (uint8_t) (context_p->token.flags | LEXER_NO_SKIP_SPACES);
  }

  return (context_p->source_p < context_p->source_end_p
          && context_p->source_p[0] == (uint8_t) character);
} /* lexer_check_next_character */

/**
 * Checks whether the next token starts with either specified characters.
 *
 * @return true - if the next is the specified character
 *         false - otherwise
 */
bool
lexer_check_next_characters (parser_context_t *context_p, /**< context */
                             lit_utf8_byte_t character1, /**< first alternative character */
                             lit_utf8_byte_t character2) /**< second alternative character */
{
  if (!(context_p->token.flags & LEXER_NO_SKIP_SPACES))
  {
    lexer_skip_spaces (context_p);
    context_p->token.flags = (uint8_t) (context_p->token.flags | LEXER_NO_SKIP_SPACES);
  }

  return (context_p->source_p < context_p->source_end_p
          && (context_p->source_p[0] == (uint8_t) character1
              || context_p->source_p[0] == (uint8_t) character2));
} /* lexer_check_next_characters */

/**
 * Consumes the next character. The character cannot be a white space.
 *
 * @return consumed character
 */
inline uint8_t JERRY_ATTR_ALWAYS_INLINE
lexer_consume_next_character (parser_context_t *context_p)
{
  JERRY_ASSERT (context_p->source_p < context_p->source_end_p);

  context_p->token.flags &= (uint8_t) ~LEXER_NO_SKIP_SPACES;

  PARSER_PLUS_EQUAL_LC (context_p->column, 1);
  return *context_p->source_p++;
} /* lexer_consume_next_character */

#if ENABLED (JERRY_ES2015)

/**
 * Checks whether the next token is a type used for detecting arrow functions.
 *
 * @return true if the next token is an arrow token
 */
bool
lexer_check_arrow (parser_context_t *context_p) /**< context */
{
  if (!(context_p->token.flags & LEXER_NO_SKIP_SPACES))
  {
    lexer_skip_spaces (context_p);
    context_p->token.flags = (uint8_t) (context_p->token.flags | LEXER_NO_SKIP_SPACES);
  }

  return (!(context_p->token.flags & LEXER_WAS_NEWLINE)
          && context_p->source_p + 2 <= context_p->source_end_p
          && context_p->source_p[0] == (uint8_t) LIT_CHAR_EQUALS
          && context_p->source_p[1] == (uint8_t) LIT_CHAR_GREATER_THAN);
} /* lexer_check_arrow */

/**
 * Checks whether the next token is a comma or equal sign.
 *
 * @return true if the next token is a comma or equal sign
 */
bool
lexer_check_arrow_param (parser_context_t *context_p) /**< context */
{
  JERRY_ASSERT (context_p->token.flags & LEXER_NO_SKIP_SPACES);

  if (context_p->source_p >= context_p->source_end_p)
  {
    return false;
  }

  if (context_p->source_p[0] == LIT_CHAR_COMMA)
  {
    return true;
  }

  if (context_p->source_p[0] != LIT_CHAR_EQUALS)
  {
    return false;
  }

  return (context_p->source_p + 1 >= context_p->source_end_p
          || context_p->source_p[1] != LIT_CHAR_EQUALS);
} /* lexer_check_arrow_param */

/**
 * Checks whether the yield expression has no argument.
 *
 * @return true if it has no argument
 */
bool
lexer_check_yield_no_arg (parser_context_t *context_p) /**< context */
{
  if (context_p->token.flags & LEXER_WAS_NEWLINE)
  {
    return true;
  }

  switch (context_p->token.type)
  {
    case LEXER_RIGHT_BRACE:
    case LEXER_RIGHT_PAREN:
    case LEXER_RIGHT_SQUARE:
    case LEXER_COMMA:
    case LEXER_COLON:
    case LEXER_SEMICOLON:
    case LEXER_EOS:
    {
      return true;
    }
    default:
    {
      return false;
    }
  }
} /* lexer_check_yield_no_arg */

/**
 * Checks whether the next token is a multiply and consumes it.
 *
 * @return true if the next token is a multiply
 */
bool
lexer_consume_generator (parser_context_t *context_p) /**< context */
{
  JERRY_ASSERT (context_p->token.flags & LEXER_NO_SKIP_SPACES);
  JERRY_ASSERT (context_p->token.type == LEXER_KEYW_ASYNC);

  if (context_p->source_p >= context_p->source_end_p
      || context_p->source_p[0] != LIT_CHAR_ASTERISK
      || (context_p->source_p + 1 < context_p->source_end_p
          && (context_p->source_p[1] == LIT_CHAR_EQUALS || context_p->source_p[1] == LIT_CHAR_ASTERISK)))
  {
    return false;
  }

  lexer_consume_next_character (context_p);
  context_p->token.type = LEXER_MULTIPLY;
  return true;
} /* lexer_consume_generator */

#endif /* ENABLED (JERRY_ES2015) */

/**
 * Convert an ident with escapes to a utf8 string.
 */
void
lexer_convert_ident_to_cesu8 (uint8_t *destination_p, /**< destination string */
                              const uint8_t *source_p, /**< source string */
                              prop_length_t length) /**< length of destination string */
{
  const uint8_t *destination_end_p = destination_p + length;

  JERRY_ASSERT (length <= PARSER_MAXIMUM_IDENT_LENGTH);

  do
  {
    if (*source_p == LIT_CHAR_BACKSLASH)
    {
      source_p += 2;
      destination_p += lit_code_point_to_cesu8_bytes (destination_p,
                                                      lexer_unchecked_hex_to_character (&source_p));
      continue;
    }

#if ENABLED (JERRY_ES2015)
    if (*source_p >= LIT_UTF8_4_BYTE_MARKER)
    {
      lit_four_byte_utf8_char_to_cesu8 (destination_p, source_p);

      destination_p += 6;
      source_p += 4;
      continue;
    }
#endif /* ENABLED (JERRY_ES2015) */

    *destination_p++ = *source_p++;
  }
  while (destination_p < destination_end_p);
} /* lexer_convert_ident_to_cesu8 */

/**
 * Convert literal to character sequence
 */
const uint8_t *
lexer_convert_literal_to_chars (parser_context_t *context_p, /**< context */
                                const lexer_lit_location_t *literal_p, /**< literal location */
                                uint8_t *local_byte_array_p, /**< local byte array to store chars */
                                lexer_string_options_t opts) /**< options */
{
  JERRY_ASSERT (context_p->u.allocated_buffer_p == NULL);

  if (!literal_p->has_escape)
  {
    return literal_p->char_p;
  }

  uint8_t *destination_start_p;
  if (literal_p->length > LEXER_MAX_LITERAL_LOCAL_BUFFER_SIZE)
  {
    context_p->u.allocated_buffer_p = (uint8_t *) parser_malloc_local (context_p, literal_p->length);
    context_p->allocated_buffer_size = literal_p->length;
    destination_start_p = context_p->u.allocated_buffer_p;
  }
  else
  {
    destination_start_p = local_byte_array_p;
  }

  if (literal_p->type == LEXER_IDENT_LITERAL)
  {
    lexer_convert_ident_to_cesu8 (destination_start_p, literal_p->char_p, literal_p->length);
    return destination_start_p;
  }

  const uint8_t *source_p = literal_p->char_p;
  uint8_t *destination_p = destination_start_p;

  uint8_t str_end_character = source_p[-1];

#if ENABLED (JERRY_ES2015)
  if (str_end_character == LIT_CHAR_RIGHT_BRACE)
  {
    str_end_character = LIT_CHAR_GRAVE_ACCENT;
  }

  bool is_raw = (opts & LEXER_STRING_RAW) != 0;
#else /* !ENABLED (JERRY_ES2015) */
  JERRY_UNUSED (opts);
  bool is_raw = false;
#endif /* ENABLED (JERRY_ES2015) */

  while (true)
  {
    if (*source_p == str_end_character)
    {
      break;
    }

    if (*source_p == LIT_CHAR_BACKSLASH && !is_raw)
    {
      uint8_t conv_character;

      source_p++;
      JERRY_ASSERT (source_p < context_p->source_end_p);

      /* Newline is ignored. */
      if (*source_p == LIT_CHAR_CR)
      {
        source_p++;
        JERRY_ASSERT (source_p < context_p->source_end_p);

        if (*source_p == LIT_CHAR_LF)
        {
          source_p++;
        }
        continue;
      }
      else if (*source_p == LIT_CHAR_LF)
      {
        source_p++;
        continue;
      }
      else if (*source_p == LEXER_NEWLINE_LS_PS_BYTE_1 && LEXER_NEWLINE_LS_PS_BYTE_23 (source_p))
      {
        source_p += 3;
        continue;
      }

      if (*source_p >= LIT_CHAR_0 && *source_p <= LIT_CHAR_3)
      {
        lit_code_point_t octal_number = (uint32_t) (*source_p - LIT_CHAR_0);

        source_p++;
        JERRY_ASSERT (source_p < context_p->source_end_p);

        if (*source_p >= LIT_CHAR_0 && *source_p <= LIT_CHAR_7)
        {
          octal_number = octal_number * 8 + (uint32_t) (*source_p - LIT_CHAR_0);
          source_p++;
          JERRY_ASSERT (source_p < context_p->source_end_p);

          if (*source_p >= LIT_CHAR_0 && *source_p <= LIT_CHAR_7)
          {
            octal_number = octal_number * 8 + (uint32_t) (*source_p - LIT_CHAR_0);
            source_p++;
            JERRY_ASSERT (source_p < context_p->source_end_p);
          }
        }

        destination_p += lit_code_point_to_cesu8_bytes (destination_p, octal_number);
        continue;
      }

      if (*source_p >= LIT_CHAR_4 && *source_p <= LIT_CHAR_7)
      {
        uint32_t octal_number = (uint32_t) (*source_p - LIT_CHAR_0);

        source_p++;
        JERRY_ASSERT (source_p < context_p->source_end_p);

        if (*source_p >= LIT_CHAR_0 && *source_p <= LIT_CHAR_7)
        {
          octal_number = octal_number * 8 + (uint32_t) (*source_p - LIT_CHAR_0);
          source_p++;
          JERRY_ASSERT (source_p < context_p->source_end_p);
        }

        *destination_p++ = (uint8_t) octal_number;
        continue;
      }

      if (*source_p == LIT_CHAR_LOWERCASE_X || *source_p == LIT_CHAR_LOWERCASE_U)
      {
        source_p++;
        destination_p += lit_code_point_to_cesu8_bytes (destination_p,
                                                        lexer_unchecked_hex_to_character (&source_p));
        continue;
      }

      conv_character = *source_p;
      switch (*source_p)
      {
        case LIT_CHAR_LOWERCASE_B:
        {
          conv_character = 0x08;
          break;
        }
        case LIT_CHAR_LOWERCASE_T:
        {
          conv_character = 0x09;
          break;
        }
        case LIT_CHAR_LOWERCASE_N:
        {
          conv_character = 0x0a;
          break;
        }
        case LIT_CHAR_LOWERCASE_V:
        {
          conv_character = 0x0b;
          break;
        }
        case LIT_CHAR_LOWERCASE_F:
        {
          conv_character = 0x0c;
          break;
        }
        case LIT_CHAR_LOWERCASE_R:
        {
          conv_character = 0x0d;
          break;
        }
      }

      if (conv_character != *source_p)
      {
        *destination_p++ = conv_character;
        source_p++;
        continue;
      }
    }
#if ENABLED (JERRY_ES2015)
    else if (str_end_character == LIT_CHAR_GRAVE_ACCENT)
    {
      if (source_p[0] == LIT_CHAR_DOLLAR_SIGN
          && source_p[1] == LIT_CHAR_LEFT_BRACE)
      {
        source_p++;
        JERRY_ASSERT (source_p < context_p->source_end_p);
        break;
      }
      if (*source_p == LIT_CHAR_CR)
      {
        *destination_p++ = LIT_CHAR_LF;
        source_p++;
        if (*source_p != str_end_character
            && *source_p == LIT_CHAR_LF)
        {
          source_p++;
        }
        continue;
      }
    }
#endif /* ENABLED (JERRY_ES2015) */

    if (*source_p >= LIT_UTF8_4_BYTE_MARKER)
    {
      /* Processing 4 byte unicode sequence (even if it is
        * after a backslash). Always converted to two 3 byte
        * long sequence. */
      lit_four_byte_utf8_char_to_cesu8 (destination_p, source_p);

      destination_p += 6;
      source_p += 4;
      continue;
    }

    *destination_p++ = *source_p++;

    /* There is no need to check the source_end_p
      * since the string is terminated by a quotation mark. */
    while (IS_UTF8_INTERMEDIATE_OCTET (*source_p))
    {
      *destination_p++ = *source_p++;
    }
  }

  JERRY_ASSERT (destination_p == destination_start_p + literal_p->length);

  return destination_start_p;
} /* lexer_convert_literal_to_chars */

/**
 * Construct a literal object from an identifier.
 */
void
lexer_construct_literal_object (parser_context_t *context_p, /**< context */
                                const lexer_lit_location_t *lit_location_p, /**< literal location */
                                uint8_t literal_type) /**< final literal type */
{
  uint8_t local_byte_array[LEXER_MAX_LITERAL_LOCAL_BUFFER_SIZE];

  const uint8_t *char_p = lexer_convert_literal_to_chars (context_p,
                                                          lit_location_p,
                                                          local_byte_array,
                                                          LEXER_STRING_NO_OPTS);

  size_t length = lit_location_p->length;
  parser_list_iterator_t literal_iterator;
  lexer_literal_t *literal_p;
  uint32_t literal_index = 0;
  bool search_scope_stack = (literal_type == LEXER_IDENT_LITERAL);

  if (JERRY_UNLIKELY (literal_type == LEXER_NEW_IDENT_LITERAL))
  {
    literal_type = LEXER_IDENT_LITERAL;
  }

  JERRY_ASSERT (literal_type == LEXER_IDENT_LITERAL
                || literal_type == LEXER_STRING_LITERAL);

  JERRY_ASSERT (literal_type != LEXER_IDENT_LITERAL || length <= PARSER_MAXIMUM_IDENT_LENGTH);
  JERRY_ASSERT (literal_type != LEXER_STRING_LITERAL || length <= PARSER_MAXIMUM_STRING_LENGTH);

  parser_list_iterator_init (&context_p->literal_pool, &literal_iterator);

  while ((literal_p = (lexer_literal_t *) parser_list_iterator_next (&literal_iterator)) != NULL)
  {
    if (literal_p->type == literal_type
        && literal_p->prop.length == length
        && memcmp (literal_p->u.char_p, char_p, length) == 0)
    {
      context_p->lit_object.literal_p = literal_p;
      context_p->lit_object.index = (uint16_t) literal_index;

      parser_free_allocated_buffer (context_p);

      if (search_scope_stack)
      {
        parser_scope_stack_t *scope_stack_start_p = context_p->scope_stack_p;
        parser_scope_stack_t *scope_stack_p = scope_stack_start_p + context_p->scope_stack_top;

        while (scope_stack_p > scope_stack_start_p)
        {
          scope_stack_p--;

          if (scope_stack_p->map_from == literal_index)
          {
            JERRY_ASSERT (scanner_decode_map_to (scope_stack_p) >= PARSER_REGISTER_START
                          || (literal_p->status_flags & LEXER_FLAG_USED));
            context_p->lit_object.index = scanner_decode_map_to (scope_stack_p);
            return;
          }
        }

        literal_p->status_flags |= LEXER_FLAG_USED;
      }
      return;
    }

    literal_index++;
  }

  JERRY_ASSERT (literal_index == context_p->literal_count);

  if (literal_index >= PARSER_MAXIMUM_NUMBER_OF_LITERALS)
  {
    parser_raise_error (context_p, PARSER_ERR_LITERAL_LIMIT_REACHED);
  }

  literal_p = (lexer_literal_t *) parser_list_append (context_p, &context_p->literal_pool);
  literal_p->prop.length = (prop_length_t) length;
  literal_p->type = literal_type;

  uint8_t status_flags = LEXER_FLAG_SOURCE_PTR;

  if (length > 0 && char_p == local_byte_array)
  {
    literal_p->u.char_p = (uint8_t *) jmem_heap_alloc_block (length);
    memcpy ((uint8_t *) literal_p->u.char_p, char_p, length);
    status_flags = 0;
  }
  else
  {
    literal_p->u.char_p = char_p;

    /* Buffer is taken over when a new literal is constructed. */
    if (context_p->u.allocated_buffer_p != NULL)
    {
      JERRY_ASSERT (char_p == context_p->u.allocated_buffer_p);

      context_p->u.allocated_buffer_p = NULL;
      status_flags = 0;
    }
  }

  if (search_scope_stack)
  {
    status_flags |= LEXER_FLAG_USED;
  }

  literal_p->status_flags = status_flags;

  context_p->lit_object.literal_p = literal_p;
  context_p->lit_object.index = (uint16_t) literal_index;
  context_p->literal_count++;

  JERRY_ASSERT (context_p->u.allocated_buffer_p == NULL);
} /* lexer_construct_literal_object */

/**
 * Construct a number object.
 *
 * @return true if number is small number
 */
bool
lexer_construct_number_object (parser_context_t *context_p, /**< context */
                               bool is_expr, /**< expression is parsed */
                               bool is_negative_number) /**< sign is negative */
{
  parser_list_iterator_t literal_iterator;
  lexer_literal_t *literal_p;
  ecma_number_t num;
  uint32_t literal_index = 0;
  prop_length_t length = context_p->token.lit_location.length;

  if (context_p->token.extra_value < LEXER_NUMBER_OCTAL)
  {
    num = ecma_utf8_string_to_number (context_p->token.lit_location.char_p,
                                      length);
  }
  else
  {
    const uint8_t *src_p = context_p->token.lit_location.char_p;
    const uint8_t *src_end_p = src_p + length - 1;
    ecma_number_t multiplier = 8.0;

#if ENABLED (JERRY_ES2015)
    if (context_p->token.extra_value == LEXER_NUMBER_BINARY)
    {
      multiplier = 2.0;
    }
#endif /* ENABLED (JERRY_ES2015) */

    num = 0;
    do
    {
      src_p++;
      num = num * multiplier + (ecma_number_t) (*src_p - LIT_CHAR_0);
    }
    while (src_p < src_end_p);
  }

  if (is_expr)
  {
    int32_t int_num = (int32_t) num;

    if (int_num == num
        && int_num <= CBC_PUSH_NUMBER_BYTE_RANGE_END
        && (int_num != 0 || !is_negative_number))
    {
      context_p->lit_object.index = (uint16_t) int_num;
      return true;
    }
  }

  if (is_negative_number)
  {
    num = -num;
  }

  ecma_value_t lit_value = ecma_find_or_create_literal_number (num);
  parser_list_iterator_init (&context_p->literal_pool, &literal_iterator);

  while ((literal_p = (lexer_literal_t *) parser_list_iterator_next (&literal_iterator)) != NULL)
  {
    if (literal_p->type == LEXER_NUMBER_LITERAL
        && literal_p->u.value == lit_value)
    {
      context_p->lit_object.literal_p = literal_p;
      context_p->lit_object.index = (uint16_t) literal_index;
      return false;
    }

    literal_index++;
  }

  JERRY_ASSERT (literal_index == context_p->literal_count);

  if (literal_index >= PARSER_MAXIMUM_NUMBER_OF_LITERALS)
  {
    parser_raise_error (context_p, PARSER_ERR_LITERAL_LIMIT_REACHED);
  }

  literal_p = (lexer_literal_t *) parser_list_append (context_p, &context_p->literal_pool);
  literal_p->u.value = lit_value;
  literal_p->prop.length = 0; /* Unused. */
  literal_p->type = LEXER_NUMBER_LITERAL;
  literal_p->status_flags = 0;

  context_p->lit_object.literal_p = literal_p;
  context_p->lit_object.index = (uint16_t) literal_index;

  context_p->literal_count++;
  return false;
} /* lexer_construct_number_object */

/**
 * Convert a push number opcode to push literal opcode
 */
void
lexer_convert_push_number_to_push_literal (parser_context_t *context_p) /**< context */
{
  ecma_integer_value_t value;
  bool two_literals = !PARSER_IS_BASIC_OPCODE (context_p->last_cbc_opcode);

  if (context_p->last_cbc_opcode == CBC_PUSH_NUMBER_0
      || context_p->last_cbc_opcode == PARSER_TO_EXT_OPCODE (CBC_EXT_PUSH_LITERAL_PUSH_NUMBER_0))
  {
    value = 0;
  }
  else if (context_p->last_cbc_opcode == CBC_PUSH_NUMBER_POS_BYTE
           || context_p->last_cbc_opcode == PARSER_TO_EXT_OPCODE (CBC_EXT_PUSH_LITERAL_PUSH_NUMBER_POS_BYTE))
  {
    value = ((ecma_integer_value_t) context_p->last_cbc.value) + 1;
  }
  else
  {
    JERRY_ASSERT (context_p->last_cbc_opcode == CBC_PUSH_NUMBER_NEG_BYTE
                  || context_p->last_cbc_opcode == PARSER_TO_EXT_OPCODE (CBC_EXT_PUSH_LITERAL_PUSH_NUMBER_NEG_BYTE));
    value = -((ecma_integer_value_t) context_p->last_cbc.value) - 1;
  }

  ecma_value_t lit_value = ecma_make_integer_value (value);

  parser_list_iterator_t literal_iterator;
  parser_list_iterator_init (&context_p->literal_pool, &literal_iterator);

  context_p->last_cbc_opcode = two_literals ? CBC_PUSH_TWO_LITERALS : CBC_PUSH_LITERAL;

  uint32_t literal_index = 0;
  lexer_literal_t *literal_p;

  while ((literal_p = (lexer_literal_t *) parser_list_iterator_next (&literal_iterator)) != NULL)
  {
    if (literal_p->type == LEXER_NUMBER_LITERAL
        && literal_p->u.value == lit_value)
    {
      if (two_literals)
      {
        context_p->last_cbc.value = (uint16_t) literal_index;
      }
      else
      {
        context_p->last_cbc.literal_index = (uint16_t) literal_index;
      }
      return;
    }

    literal_index++;
  }

  JERRY_ASSERT (literal_index == context_p->literal_count);

  if (literal_index >= PARSER_MAXIMUM_NUMBER_OF_LITERALS)
  {
    parser_raise_error (context_p, PARSER_ERR_LITERAL_LIMIT_REACHED);
  }

  literal_p = (lexer_literal_t *) parser_list_append (context_p, &context_p->literal_pool);
  literal_p->u.value = lit_value;
  literal_p->prop.length = 0; /* Unused. */
  literal_p->type = LEXER_NUMBER_LITERAL;
  literal_p->status_flags = 0;

  context_p->literal_count++;

  if (two_literals)
  {
    context_p->last_cbc.value = (uint16_t) literal_index;
  }
  else
  {
    context_p->last_cbc.literal_index = (uint16_t) literal_index;
  }
} /* lexer_convert_push_number_to_push_literal */

/**
 * Construct a function literal object.
 *
 * @return function object literal index
 */
uint16_t
lexer_construct_function_object (parser_context_t *context_p, /**< context */
                                 uint32_t extra_status_flags) /**< extra status flags */
{
  ecma_compiled_code_t *compiled_code_p;
  lexer_literal_t *literal_p;
  uint16_t result_index;

  if (context_p->literal_count >= PARSER_MAXIMUM_NUMBER_OF_LITERALS)
  {
    parser_raise_error (context_p, PARSER_ERR_LITERAL_LIMIT_REACHED);
  }

  parser_flush_cbc (context_p);

  if (context_p->status_flags & PARSER_INSIDE_WITH)
  {
    extra_status_flags |= PARSER_INSIDE_WITH;
  }

  literal_p = (lexer_literal_t *) parser_list_append (context_p, &context_p->literal_pool);
  literal_p->type = LEXER_UNUSED_LITERAL;
  literal_p->status_flags = 0;

  result_index = context_p->literal_count;
  context_p->literal_count++;

#if ENABLED (JERRY_ES2015)
  if (!(extra_status_flags & PARSER_IS_ARROW_FUNCTION))
  {
    compiled_code_p = parser_parse_function (context_p, extra_status_flags);
  }
  else
  {
    compiled_code_p = parser_parse_arrow_function (context_p, extra_status_flags);
  }
#else /* !ENABLED (JERRY_ES2015) */
  compiled_code_p = parser_parse_function (context_p, extra_status_flags);
#endif /* ENABLED (JERRY_ES2015) */

  literal_p->u.bytecode_p = compiled_code_p;
  literal_p->type = LEXER_FUNCTION_LITERAL;

  return result_index;
} /* lexer_construct_function_object */

/**
 * Construct a regular expression object.
 */
void
lexer_construct_regexp_object (parser_context_t *context_p, /**< context */
                               bool parse_only) /**< parse only */
{
#if ENABLED (JERRY_BUILTIN_REGEXP)
  const uint8_t *source_p = context_p->source_p;
  const uint8_t *regex_start_p = context_p->source_p;
  const uint8_t *regex_end_p = regex_start_p;
  const uint8_t *source_end_p = context_p->source_end_p;
  parser_line_counter_t column = context_p->column;
  lexer_literal_t *literal_p;
  bool in_class = false;
  uint16_t current_flags;
  lit_utf8_size_t length;

  JERRY_ASSERT (context_p->token.type == LEXER_DIVIDE
                || context_p->token.type == LEXER_ASSIGN_DIVIDE);

  if (context_p->token.type == LEXER_ASSIGN_DIVIDE)
  {
    regex_start_p--;
  }

  while (true)
  {
    if (source_p >= source_end_p)
    {
      parser_raise_error (context_p, PARSER_ERR_UNTERMINATED_REGEXP);
    }

    if (!in_class && source_p[0] == LIT_CHAR_SLASH)
    {
      regex_end_p = source_p;
      source_p++;
      column++;
      break;
    }

    switch (source_p[0])
    {
      case LIT_CHAR_CR:
      case LIT_CHAR_LF:
      case LEXER_NEWLINE_LS_PS_BYTE_1:
      {
        if (source_p[0] != LEXER_NEWLINE_LS_PS_BYTE_1
            || LEXER_NEWLINE_LS_PS_BYTE_23 (source_p))
        {
          parser_raise_error (context_p, PARSER_ERR_NEWLINE_NOT_ALLOWED);
        }
        break;
      }
      case LIT_CHAR_TAB:
      {
        column = align_column_to_tab (column);
         /* Subtract -1 because column is increased below. */
        column--;
        break;
      }
      case LIT_CHAR_LEFT_SQUARE:
      {
        in_class = true;
        break;
      }
      case LIT_CHAR_RIGHT_SQUARE:
      {
        in_class = false;
        break;
      }
      case LIT_CHAR_BACKSLASH:
      {
        if (source_p + 1 >= source_end_p)
        {
          parser_raise_error (context_p, PARSER_ERR_UNTERMINATED_REGEXP);
        }

        if (source_p[1] >= 0x20 && source_p[1] <= LIT_UTF8_1_BYTE_CODE_POINT_MAX)
        {
          source_p++;
          column++;
        }
      }
    }

    source_p++;
    column++;

    while (source_p < source_end_p
           && IS_UTF8_INTERMEDIATE_OCTET (source_p[0]))
    {
      source_p++;
    }
  }

  current_flags = 0;
  while (source_p < source_end_p)
  {
    uint32_t flag = 0;

    if (source_p[0] == LIT_CHAR_LOWERCASE_G)
    {
      flag = RE_FLAG_GLOBAL;
    }
    else if (source_p[0] == LIT_CHAR_LOWERCASE_I)
    {
      flag = RE_FLAG_IGNORE_CASE;
    }
    else if (source_p[0] == LIT_CHAR_LOWERCASE_M)
    {
      flag = RE_FLAG_MULTILINE;
    }
    else if (source_p[0] == LIT_CHAR_LOWERCASE_U)
    {
      flag = RE_FLAG_UNICODE;
    }
    else if (source_p[0] == LIT_CHAR_LOWERCASE_Y)
    {
      flag = RE_FLAG_STICKY;
    }

    if (flag == 0)
    {
      break;
    }

    if (current_flags & flag)
    {
      parser_raise_error (context_p, PARSER_ERR_DUPLICATED_REGEXP_FLAG);
    }

    current_flags = (uint16_t) (current_flags | flag);
    source_p++;
    column++;
  }

  context_p->source_p = source_p;
  context_p->column = column;

  if (source_p < source_end_p && lexer_parse_identifier (context_p, LEXER_PARSE_CHECK_PART_AND_RETURN))
  {
    parser_raise_error (context_p, PARSER_ERR_UNKNOWN_REGEXP_FLAG);
  }

  length = (lit_utf8_size_t) (regex_end_p - regex_start_p);
  if (length > PARSER_MAXIMUM_STRING_LENGTH)
  {
    parser_raise_error (context_p, PARSER_ERR_REGEXP_TOO_LONG);
  }

  context_p->column = column;
  context_p->source_p = source_p;

  if (parse_only)
  {
    return;
  }

  if (context_p->literal_count >= PARSER_MAXIMUM_NUMBER_OF_LITERALS)
  {
    parser_raise_error (context_p, PARSER_ERR_LITERAL_LIMIT_REACHED);
  }

  literal_p = (lexer_literal_t *) parser_list_append (context_p, &context_p->literal_pool);
  literal_p->prop.length = (prop_length_t) length;
  literal_p->type = LEXER_UNUSED_LITERAL;
  literal_p->status_flags = 0;

  context_p->literal_count++;

  /* Compile the RegExp literal and store the RegExp bytecode pointer */
  const re_compiled_code_t *re_bytecode_p = NULL;
  ecma_value_t completion_value;

  ecma_string_t *pattern_str_p = NULL;

  if (lit_is_valid_cesu8_string (regex_start_p, length))
  {
    pattern_str_p = ecma_new_ecma_string_from_utf8 (regex_start_p, length);
  }
  else
  {
    JERRY_ASSERT (lit_is_valid_utf8_string (regex_start_p, length));
    pattern_str_p = ecma_new_ecma_string_from_utf8_converted_to_cesu8 (regex_start_p, length);
  }

  completion_value = re_compile_bytecode (&re_bytecode_p,
                                          pattern_str_p,
                                          current_flags);
  ecma_deref_ecma_string (pattern_str_p);

  if (ECMA_IS_VALUE_ERROR (completion_value))
  {
    jcontext_release_exception ();
    parser_raise_error (context_p, PARSER_ERR_INVALID_REGEXP);
  }

  ecma_free_value (completion_value);

  literal_p->type = LEXER_REGEXP_LITERAL;
  literal_p->u.bytecode_p = (ecma_compiled_code_t *) re_bytecode_p;

  context_p->token.type = LEXER_LITERAL;
  context_p->token.keyword_type = LEXER_EOS;
  context_p->token.lit_location.type = LEXER_REGEXP_LITERAL;

  context_p->lit_object.literal_p = literal_p;
  context_p->lit_object.index = (uint16_t) (context_p->literal_count - 1);
#else /* !ENABLED (JERRY_BUILTIN_REGEXP) */
  JERRY_UNUSED (parse_only);
  parser_raise_error (context_p, PARSER_ERR_UNSUPPORTED_REGEXP);
#endif /* ENABLED (JERRY_BUILTIN_REGEXP) */
} /* lexer_construct_regexp_object */

/**
 * Next token must be an identifier.
 */
void
lexer_expect_identifier (parser_context_t *context_p, /**< context */
                         uint8_t literal_type) /**< literal type */
{
  JERRY_ASSERT (literal_type == LEXER_STRING_LITERAL
                || literal_type == LEXER_IDENT_LITERAL
                || literal_type == LEXER_NEW_IDENT_LITERAL);

  lexer_skip_spaces (context_p);
  context_p->token.line = context_p->line;
  context_p->token.column = context_p->column;

  if (context_p->source_p < context_p->source_end_p
      && lexer_parse_identifier (context_p, (literal_type != LEXER_STRING_LITERAL ? LEXER_PARSE_CHECK_KEYWORDS
                                                                                  : LEXER_PARSE_NO_OPTS)))
  {
    if (context_p->token.type == LEXER_LITERAL)
    {
      JERRY_ASSERT (context_p->token.lit_location.type == LEXER_IDENT_LITERAL);

      lexer_construct_literal_object (context_p,
                                      &context_p->token.lit_location,
                                      literal_type);

      if (literal_type != LEXER_STRING_LITERAL
          && (context_p->status_flags & PARSER_IS_STRICT))
      {
        if (context_p->token.keyword_type == LEXER_KEYW_EVAL)
        {
          parser_raise_error (context_p, PARSER_ERR_EVAL_NOT_ALLOWED);
        }
        else if (context_p->token.keyword_type == LEXER_KEYW_ARGUMENTS)
        {
          parser_raise_error (context_p, PARSER_ERR_ARGUMENTS_NOT_ALLOWED);
        }
      }
      return;
    }
  }
#if ENABLED (JERRY_ES2015_MODULE_SYSTEM)
  else if (context_p->status_flags & PARSER_MODULE_DEFAULT_CLASS_OR_FUNC)
  {
    /* When parsing default exports for modules, it is not required by functions or classes to have identifiers.
     * In this case we use a synthetic name for them. */
    context_p->token.type = LEXER_LITERAL;
    context_p->token.keyword_type = LEXER_EOS;
    context_p->token.lit_location.type = LEXER_IDENT_LITERAL;
    context_p->token.lit_location.has_escape = false;
    lexer_construct_literal_object (context_p, &lexer_default_literal, literal_type);
    context_p->status_flags &= (uint32_t) ~(PARSER_MODULE_DEFAULT_CLASS_OR_FUNC);
    return;
  }
#endif /* ENABLED (JERRY_ES2015_MODULE_SYSTEM) */

  parser_raise_error (context_p, PARSER_ERR_IDENTIFIER_EXPECTED);
} /* lexer_expect_identifier */

/**
 * Next token must be an identifier.
 */
void
lexer_expect_object_literal_id (parser_context_t *context_p, /**< context */
                                uint32_t ident_opts) /**< lexer_obj_ident_opts_t option bits */
{
  lexer_skip_spaces (context_p);

  if (context_p->source_p >= context_p->source_end_p)
  {
    parser_raise_error (context_p, PARSER_ERR_PROPERTY_IDENTIFIER_EXPECTED);
  }

#if ENABLED (JERRY_ES2015)
  int is_class_method = ((ident_opts & LEXER_OBJ_IDENT_CLASS_METHOD)
                         && !(ident_opts & LEXER_OBJ_IDENT_ONLY_IDENTIFIERS)
                         && (context_p->token.type != LEXER_KEYW_STATIC));
#endif /* ENABLED (JERRY_ES2015) */

  context_p->token.line = context_p->line;
  context_p->token.column = context_p->column;
  bool create_literal_object = false;

  if (lexer_parse_identifier (context_p, LEXER_PARSE_NO_OPTS))
  {
    if (!(ident_opts & (LEXER_OBJ_IDENT_ONLY_IDENTIFIERS | LEXER_OBJ_IDENT_OBJECT_PATTERN)))
    {
      lexer_skip_spaces (context_p);
      context_p->token.flags = (uint8_t) (context_p->token.flags | LEXER_NO_SKIP_SPACES);

      if (context_p->source_p < context_p->source_end_p
#if ENABLED (JERRY_ES2015)
          && context_p->source_p[0] != LIT_CHAR_COMMA
          && context_p->source_p[0] != LIT_CHAR_RIGHT_BRACE
          && context_p->source_p[0] != LIT_CHAR_LEFT_PAREN
#endif /* ENABLED (JERRY_ES2015) */
          && context_p->source_p[0] != LIT_CHAR_COLON)
      {
        if (lexer_compare_literal_to_string (context_p, "get", 3))
        {
          context_p->token.type = LEXER_PROPERTY_GETTER;
          return;
        }

        if (lexer_compare_literal_to_string (context_p, "set", 3))
        {
          context_p->token.type = LEXER_PROPERTY_SETTER;
          return;
        }

#if ENABLED (JERRY_ES2015)
        if (lexer_compare_literal_to_string (context_p, "async", 5))
        {
          context_p->token.type = LEXER_KEYW_ASYNC;
          return;
        }
#endif /* ENABLED (JERRY_ES2015) */
      }
    }

#if ENABLED (JERRY_ES2015)
    if (is_class_method && lexer_compare_literal_to_string (context_p, "static", 6))
    {
      context_p->token.type = LEXER_KEYW_STATIC;
      return;
    }
#endif /* ENABLED (JERRY_ES2015) */

    create_literal_object = true;
  }
  else
  {
    switch (context_p->source_p[0])
    {
      case LIT_CHAR_DOUBLE_QUOTE:
      case LIT_CHAR_SINGLE_QUOTE:
      {
        lexer_parse_string (context_p, LEXER_STRING_NO_OPTS);
        create_literal_object = true;
        break;
      }
#if ENABLED (JERRY_ES2015)
      case LIT_CHAR_LEFT_SQUARE:
      {
        lexer_consume_next_character (context_p);

        lexer_next_token (context_p);
        parser_parse_expression (context_p, PARSE_EXPR_NO_COMMA);

        if (context_p->token.type != LEXER_RIGHT_SQUARE)
        {
          parser_raise_error (context_p, PARSER_ERR_RIGHT_SQUARE_EXPECTED);
        }
        return;
      }
      case LIT_CHAR_ASTERISK:
#endif /* ENABLED (JERRY_ES2015) */
      case LIT_CHAR_RIGHT_BRACE:
      {
        if (ident_opts & LEXER_OBJ_IDENT_ONLY_IDENTIFIERS)
        {
          break;
        }

        context_p->token.type = LEXER_RIGHT_BRACE;
#if ENABLED (JERRY_ES2015)
        if (context_p->source_p[0] == LIT_CHAR_ASTERISK)
        {
          context_p->token.type = LEXER_MULTIPLY;
        }
#endif /* ENABLED (JERRY_ES2015) */

        lexer_consume_next_character (context_p);
        return;
      }
      default:
      {
        const uint8_t *char_p = context_p->source_p;

        if (char_p[0] == LIT_CHAR_DOT)
        {
          char_p++;
        }

        if (char_p < context_p->source_end_p
            && char_p[0] >= LIT_CHAR_0
            && char_p[0] <= LIT_CHAR_9)
        {
          lexer_parse_number (context_p);
          lexer_construct_number_object (context_p, false, false);
          return;
        }
        break;
      }
    }
  }

  if (create_literal_object)
  {
#if ENABLED (JERRY_ES2015)
    if (is_class_method && lexer_compare_literal_to_string (context_p, "constructor", 11))
    {
      context_p->token.type = LEXER_CLASS_CONSTRUCTOR;
      return;
    }
#endif /* ENABLED (JERRY_ES2015) */

    lexer_construct_literal_object (context_p,
                                    &context_p->token.lit_location,
                                    LEXER_STRING_LITERAL);
    return;
  }

  parser_raise_error (context_p, PARSER_ERR_PROPERTY_IDENTIFIER_EXPECTED);
} /* lexer_expect_object_literal_id */

/**
 * Read next token without checking keywords
 *
 * @return true if the next literal is identifier, false otherwise
 */
bool
lexer_scan_identifier (parser_context_t *context_p) /**< context */
{
  lexer_skip_spaces (context_p);
  context_p->token.line = context_p->line;
  context_p->token.column = context_p->column;

  if (context_p->source_p < context_p->source_end_p
      && lexer_parse_identifier (context_p, LEXER_PARSE_NO_OPTS))
  {
    return true;
  }

  lexer_next_token (context_p);
  return false;
} /* lexer_scan_identifier */

/**
 * Check whether the identifier is a modifier in a property definition.
 */
void
lexer_check_property_modifier (parser_context_t *context_p) /**< context */
{
  JERRY_ASSERT (!(context_p->token.flags & LEXER_NO_SKIP_SPACES));
  JERRY_ASSERT (context_p->token.type = LEXER_LITERAL
                && context_p->token.lit_location.type == LEXER_IDENT_LITERAL);

  lexer_skip_spaces (context_p);
  context_p->token.flags = (uint8_t) (context_p->token.flags | LEXER_NO_SKIP_SPACES);

  if (context_p->source_p >= context_p->source_end_p
#if ENABLED (JERRY_ES2015)
      || context_p->source_p[0] == LIT_CHAR_COMMA
      || context_p->source_p[0] == LIT_CHAR_RIGHT_BRACE
      || context_p->source_p[0] == LIT_CHAR_LEFT_PAREN
      || context_p->source_p[0] == LIT_CHAR_EQUALS
#endif /* ENABLED (JERRY_ES2015) */
      || context_p->source_p[0] == LIT_CHAR_COLON)
  {
    return;
  }

  if (lexer_compare_literal_to_string (context_p, "get", 3))
  {
    context_p->token.type = LEXER_PROPERTY_GETTER;
    return;
  }

  if (lexer_compare_literal_to_string (context_p, "set", 3))
  {
    context_p->token.type = LEXER_PROPERTY_SETTER;
    return;
  }

#if ENABLED (JERRY_ES2015)
  if (lexer_compare_literal_to_string (context_p, "async", 5))
  {
    context_p->token.type = LEXER_KEYW_ASYNC;
    return;
  }
#endif /* ENABLED (JERRY_ES2015) */
} /* lexer_check_property_modifier */

/**
 * Compares two identifiers.
 *
 * Note:
 *   Escape sequences are allowed in the left identifier, but not in the right
 *
 * @return true if the two identifiers are the same
 */
static bool
lexer_compare_identifier_to_chars (const uint8_t *left_p, /**< left identifier */
                                   const uint8_t *right_p, /**< right identifier string */
                                   size_t size) /**< byte size of the two identifiers */
{
  uint8_t utf8_buf[6];

  do
  {
    if (*left_p == *right_p)
    {
      left_p++;
      right_p++;
      size--;
      continue;
    }

    size_t escape_size;

    if (*left_p == LIT_CHAR_BACKSLASH)
    {
      left_p += 2;
      lit_code_point_t code_point = lexer_unchecked_hex_to_character (&left_p);

      escape_size = lit_code_point_to_cesu8_bytes (utf8_buf, code_point);
    }
    else if (*left_p >= LIT_UTF8_4_BYTE_MARKER)
    {
      lit_four_byte_utf8_char_to_cesu8 (utf8_buf, left_p);
      escape_size = 3 * 2;
      left_p += 4;
    }
    else
    {
      return false;
    }

    size -= escape_size;

    uint8_t *utf8_p = utf8_buf;
    do
    {
      if (*right_p++ != *utf8_p++)
      {
        return false;
      }
    }
    while (--escape_size > 0);
  }
  while (size > 0);

  return true;
} /* lexer_compare_identifier_to_chars */

/**
 * Compares an identifier to a string.
 *
 * Note:
 *   Escape sequences are allowed in the left identifier, but not in the right
 *
 * @return true if the identifier equals to string
 */
bool
lexer_compare_identifier_to_string (const lexer_lit_location_t *left_p, /**< left literal */
                                    const uint8_t *right_p, /**< right identifier string */
                                    size_t size) /**< byte size of the right identifier */
{
  if (left_p->length != size)
  {
    return false;
  }

  if (!left_p->has_escape)
  {
    return memcmp (left_p->char_p, right_p, size) == 0;
  }

  return lexer_compare_identifier_to_chars (left_p->char_p, right_p, size);
} /* lexer_compare_identifier_to_string */

/**
 * Compares two identifiers.
 *
 * Note:
 *   Escape sequences are allowed in both identifiers
 *
 * @return true if the two identifiers are the same
 */
bool
lexer_compare_identifiers (parser_context_t *context_p, /**< context */
                           const lexer_lit_location_t *left_p, /**< left literal */
                           const lexer_lit_location_t *right_p) /**< right literal */
{
  prop_length_t length = left_p->length;

  if (length != right_p->length)
  {
    return false;
  }

  if (!left_p->has_escape)
  {
    return lexer_compare_identifier_to_chars (right_p->char_p, left_p->char_p, length);
  }

  if (!right_p->has_escape)
  {
    return lexer_compare_identifier_to_chars (left_p->char_p, right_p->char_p, length);
  }

  uint8_t buf_p[64];

  if (length <= 64)
  {
    lexer_convert_ident_to_cesu8 (buf_p, left_p->char_p, length);
    return lexer_compare_identifier_to_chars (right_p->char_p, buf_p, length);
  }

  uint8_t *dynamic_buf_p = parser_malloc (context_p, length);

  lexer_convert_ident_to_cesu8 (dynamic_buf_p, left_p->char_p, length);
  bool result = lexer_compare_identifier_to_chars (right_p->char_p, dynamic_buf_p, length);
  parser_free (dynamic_buf_p, length);

  return result;
} /* lexer_compare_identifiers */

/**
 * Compares the current identifier in the context to the parameter identifier
 *
 * Note:
 *   Escape sequences are allowed.
 *
 * @return true if the input identifiers are the same
 */
bool
lexer_current_is_literal (parser_context_t *context_p, /**< context */
                          const lexer_lit_location_t *right_ident_p) /**< identifier */
{
  JERRY_ASSERT (context_p->token.type == LEXER_LITERAL
                && context_p->token.lit_location.type == LEXER_IDENT_LITERAL);

  lexer_lit_location_t *left_ident_p = &context_p->token.lit_location;

  JERRY_ASSERT (left_ident_p->length > 0 && right_ident_p->length > 0);

  if (left_ident_p->length != right_ident_p->length)
  {
    return false;
  }

  if (!left_ident_p->has_escape && !right_ident_p->has_escape)
  {
    return memcmp (left_ident_p->char_p, right_ident_p->char_p, left_ident_p->length) == 0;
  }

  return lexer_compare_identifiers (context_p, left_ident_p, right_ident_p);
} /* lexer_current_is_literal */

/**
 * Compares the current string token to "use strict".
 *
 * Note:
 *   Escape sequences are not allowed.
 *
 * @return true if "use strict" is found, false otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
lexer_string_is_use_strict (parser_context_t *context_p) /**< context */
{
  JERRY_ASSERT (context_p->token.type == LEXER_LITERAL
                && context_p->token.lit_location.type == LEXER_STRING_LITERAL);

  return (context_p->token.lit_location.length == 10
          && !context_p->token.lit_location.has_escape
          && memcmp (context_p->token.lit_location.char_p, "use strict", 10) == 0);
} /* lexer_string_is_use_strict */

/**
 * Checks whether the string before the current token is a directive or a string literal.
 *
 * @return true if the string is a directive, false otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
lexer_string_is_directive (parser_context_t *context_p) /**< context */
{
  return (context_p->token.type == LEXER_SEMICOLON
          || context_p->token.type == LEXER_RIGHT_BRACE
          || ((context_p->token.flags & LEXER_WAS_NEWLINE)
              && !LEXER_IS_BINARY_OP_TOKEN (context_p->token.type)
              && context_p->token.type != LEXER_LEFT_PAREN
              && context_p->token.type != LEXER_LEFT_SQUARE
              && context_p->token.type != LEXER_DOT));
} /* lexer_string_is_directive */

#if ENABLED (JERRY_ES2015)

/**
 * Compares the current token to an expected identifier.
 *
 * Note:
 *   Escape sequences are not allowed.
 *
 * @return true if they are the same, false otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
lexer_token_is_identifier (parser_context_t *context_p, /**< context */
                           const char *identifier_p, /**< identifier */
                           size_t identifier_length) /**< identifier length */
{
  /* Checking has_escape is unnecessary because memcmp will fail if escape sequences are present. */
  return (context_p->token.type == LEXER_LITERAL
          && context_p->token.lit_location.type == LEXER_IDENT_LITERAL
          && context_p->token.lit_location.length == identifier_length
          && memcmp (context_p->token.lit_location.char_p, identifier_p, identifier_length) == 0);
} /* lexer_token_is_identifier */

/**
 * Compares the current identifier token to "let".
 *
 * Note:
 *   Escape sequences are not allowed.
 *
 * @return true if "let" is found, false otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
lexer_token_is_let (parser_context_t *context_p) /**< context */
{
  JERRY_ASSERT (context_p->token.type == LEXER_LITERAL);

  return (context_p->token.keyword_type == LEXER_KEYW_LET
          && !context_p->token.lit_location.has_escape);
} /* lexer_token_is_let */

/**
 * Compares the current identifier token to "async".
 *
 * Note:
 *   Escape sequences are not allowed.
 *
 * @return true if "async" is found, false otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
lexer_token_is_async (parser_context_t *context_p) /**< context */
{
  JERRY_ASSERT (context_p->token.type == LEXER_LITERAL
                || context_p->token.type == LEXER_TEMPLATE_LITERAL);

  return (context_p->token.keyword_type == LEXER_KEYW_ASYNC
          && !context_p->token.lit_location.has_escape);
} /* lexer_token_is_async */

#endif /* ENABLED (JERRY_ES2015) */

/**
 * Compares the current identifier or string to an expected string.
 *
 * Note:
 *   Escape sequences are not allowed.
 *
 * @return true if they are the same, false otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
lexer_compare_literal_to_string (parser_context_t *context_p, /**< context */
                                 const char *string_p, /**< string */
                                 size_t string_length) /**< string length */
{
  JERRY_ASSERT (context_p->token.type == LEXER_LITERAL
                && (context_p->token.lit_location.type == LEXER_IDENT_LITERAL
                    || context_p->token.lit_location.type == LEXER_STRING_LITERAL));

  /* Checking has_escape is unnecessary because memcmp will fail if escape sequences are present. */
  return (context_p->token.lit_location.length == string_length
          && memcmp (context_p->token.lit_location.char_p, string_p, string_length) == 0);
} /* lexer_compare_literal_to_string */

/**
 * Convert binary lvalue token to binary token
 * e.g. += -> +
 *      ^= -> ^
 *
 * @return binary token
 */
uint8_t
lexer_convert_binary_lvalue_token_to_binary (uint8_t token) /**< binary lvalue token */
{
  JERRY_ASSERT (LEXER_IS_BINARY_LVALUE_TOKEN (token));
  JERRY_ASSERT (token != LEXER_ASSIGN);

  if (token <= LEXER_ASSIGN_MODULO)
  {
    return (uint8_t) (LEXER_ADD + (token - LEXER_ASSIGN_ADD));
  }

  if (token <= LEXER_ASSIGN_UNS_RIGHT_SHIFT)
  {
    return (uint8_t) (LEXER_LEFT_SHIFT + (token - LEXER_ASSIGN_LEFT_SHIFT));
  }

  switch (token)
  {
    case LEXER_ASSIGN_BIT_AND:
    {
      return LEXER_BIT_AND;
    }
    case LEXER_ASSIGN_BIT_OR:
    {
      return LEXER_BIT_OR;
    }
    default:
    {
      JERRY_ASSERT (token == LEXER_ASSIGN_BIT_XOR);
      return LEXER_BIT_XOR;
    }
  }
} /* lexer_convert_binary_lvalue_token_to_binary */

/**
 * @}
 * @}
 * @}
 */

#endif /* ENABLED (JERRY_PARSER) */

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
#include "ecma-builtin-helpers.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects-general.h"
#include "ecma-objects.h"
#include "jrt-libc-includes.h"
#include "jrt.h"
#include "lit-char-helpers.h"
#include "lit-globals.h"

#if JERRY_BUILTIN_JSON

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-json.inc.h"
#define BUILTIN_UNDERSCORED_ID  json
#include "ecma-builtin-internal-routines-template.inc.h"

/**
 * The number of expected hexidecimal characters in a hex escape sequence
 */
#define ECMA_JSON_HEX_ESCAPE_SEQUENCE_LENGTH (4)

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup json ECMA JSON object built-in
 * @{
 */

/**
 * JSON token type
 */
typedef enum
{
  TOKEN_INVALID, /**< error token */
  TOKEN_END, /**< end of stream reached */
  TOKEN_NUMBER, /**< JSON number */
  TOKEN_STRING, /**< JSON string */
  TOKEN_NULL, /**< JSON null primitive value */
  TOKEN_TRUE, /**< JSON true primitive value */
  TOKEN_FALSE, /**< JSON false primitive value */
  TOKEN_LEFT_BRACE, /**< JSON left brace */
  TOKEN_RIGHT_BRACE, /**< JSON right brace */
  TOKEN_LEFT_SQUARE, /**< JSON left square bracket */
  TOKEN_RIGHT_SQUARE, /**< JSON right square bracket */
  TOKEN_COMMA, /**< JSON comma */
  TOKEN_COLON /**< JSON colon */
} ecma_json_token_type_t;

/**
 * JSON token
 */
typedef struct
{
  ecma_json_token_type_t type; /**< type of the current token */
  const lit_utf8_byte_t *current_p; /**< current position of the string processed by the parser */
  const lit_utf8_byte_t *end_p; /**< end of the string processed by the parser */

  /**
   * Fields depending on type.
   */
  union
  {
    ecma_string_t *string_p; /**< when type is string_token it contains the string */
    ecma_number_t number; /**< when type is number_token, it contains the value of the number */
  } u;
} ecma_json_token_t;

/**
 * Parse and extract string token.
 */
static void
ecma_builtin_json_parse_string (ecma_json_token_t *token_p) /**< token argument */
{
  const lit_utf8_byte_t *current_p = token_p->current_p;
  const lit_utf8_byte_t *end_p = token_p->end_p;

  ecma_stringbuilder_t result_builder = ecma_stringbuilder_create ();
  const lit_utf8_byte_t *unappended_p = current_p;

  while (true)
  {
    if (current_p >= end_p || *current_p <= 0x1f)
    {
      goto invalid_string;
    }

    if (*current_p == LIT_CHAR_DOUBLE_QUOTE)
    {
      break;
    }

    if (*current_p == LIT_CHAR_BACKSLASH)
    {
      ecma_stringbuilder_append_raw (&result_builder, unappended_p, (lit_utf8_size_t) (current_p - unappended_p));

      current_p++;

      /* If there is an escape sequence but there's no escapable character just return */
      if (current_p >= end_p)
      {
        goto invalid_string;
      }

      const lit_utf8_byte_t c = *current_p;
      switch (c)
      {
        case LIT_CHAR_DOUBLE_QUOTE:
        case LIT_CHAR_SLASH:
        case LIT_CHAR_BACKSLASH:
        {
          ecma_stringbuilder_append_byte (&result_builder, c);
          current_p++;
          break;
        }
        case LIT_CHAR_LOWERCASE_B:
        {
          ecma_stringbuilder_append_byte (&result_builder, LIT_CHAR_BS);
          current_p++;
          break;
        }
        case LIT_CHAR_LOWERCASE_F:
        {
          ecma_stringbuilder_append_byte (&result_builder, LIT_CHAR_FF);
          current_p++;
          break;
        }
        case LIT_CHAR_LOWERCASE_N:
        {
          ecma_stringbuilder_append_byte (&result_builder, LIT_CHAR_LF);
          current_p++;
          break;
        }
        case LIT_CHAR_LOWERCASE_R:
        {
          ecma_stringbuilder_append_byte (&result_builder, LIT_CHAR_CR);
          current_p++;
          break;
        }
        case LIT_CHAR_LOWERCASE_T:
        {
          ecma_stringbuilder_append_byte (&result_builder, LIT_CHAR_TAB);
          current_p++;
          break;
        }
        case LIT_CHAR_LOWERCASE_U:
        {
          uint32_t hex_value = lit_char_hex_lookup (current_p + 1, end_p, ECMA_JSON_HEX_ESCAPE_SEQUENCE_LENGTH);
          if (hex_value == UINT32_MAX)
          {
            goto invalid_string;
          }

          ecma_stringbuilder_append_char (&result_builder, (ecma_char_t) hex_value);
          current_p += ECMA_JSON_HEX_ESCAPE_SEQUENCE_LENGTH + 1;
          break;
        }
        default:
        {
          goto invalid_string;
        }
      }

      unappended_p = current_p;
      continue;
    }

    current_p++;
  }

  ecma_stringbuilder_append_raw (&result_builder, unappended_p, (lit_utf8_size_t) (current_p - unappended_p));
  token_p->u.string_p = ecma_stringbuilder_finalize (&result_builder);
  token_p->current_p = current_p + 1;
  token_p->type = TOKEN_STRING;
  return;

invalid_string:
  ecma_stringbuilder_destroy (&result_builder);
} /* ecma_builtin_json_parse_string */

/**
 * Parse and extract string token.
 */
static void
ecma_builtin_json_parse_number (ecma_json_token_t *token_p) /**< token argument */
{
  const lit_utf8_byte_t *current_p = token_p->current_p;
  const lit_utf8_byte_t *end_p = token_p->end_p;
  const lit_utf8_byte_t *start_p = current_p;

  JERRY_ASSERT (current_p < end_p);

  if (*current_p == LIT_CHAR_MINUS)
  {
    current_p++;
  }

  if (current_p >= end_p)
  {
    return;
  }

  if (*current_p == LIT_CHAR_0)
  {
    current_p++;

    if (current_p < end_p && lit_char_is_decimal_digit (*current_p))
    {
      return;
    }
  }
  else if (lit_char_is_decimal_digit (*current_p))
  {
    do
    {
      current_p++;
    } while (current_p < end_p && lit_char_is_decimal_digit (*current_p));
  }

  if (current_p < end_p && *current_p == LIT_CHAR_DOT)
  {
    current_p++;

    if (current_p >= end_p || !lit_char_is_decimal_digit (*current_p))
    {
      return;
    }

    do
    {
      current_p++;
    } while (current_p < end_p && lit_char_is_decimal_digit (*current_p));
  }

  if (current_p < end_p && (*current_p == LIT_CHAR_LOWERCASE_E || *current_p == LIT_CHAR_UPPERCASE_E))
  {
    current_p++;

    if (current_p < end_p && (*current_p == LIT_CHAR_PLUS || *current_p == LIT_CHAR_MINUS))
    {
      current_p++;
    }

    if (current_p >= end_p || !lit_char_is_decimal_digit (*current_p))
    {
      return;
    }

    do
    {
      current_p++;
    } while (current_p < end_p && lit_char_is_decimal_digit (*current_p));
  }

  token_p->type = TOKEN_NUMBER;
  token_p->u.number = ecma_utf8_string_to_number (start_p, (lit_utf8_size_t) (current_p - start_p), 0);

  token_p->current_p = current_p;
} /* ecma_builtin_json_parse_number */

/**
 * Parse next token.
 *
 * The function fills the fields of the ecma_json_token_t
 * argument and advances the string pointer.
 */
static void
ecma_builtin_json_parse_next_token (ecma_json_token_t *token_p, /**< token argument */
                                    bool parse_string) /**< strings are allowed to parse */
{
  const lit_utf8_byte_t *current_p = token_p->current_p;
  const lit_utf8_byte_t *end_p = token_p->end_p;
  token_p->type = TOKEN_INVALID;

  while (current_p < end_p
         && (*current_p == LIT_CHAR_SP || *current_p == LIT_CHAR_CR || *current_p == LIT_CHAR_LF
             || *current_p == LIT_CHAR_TAB))
  {
    current_p++;
  }

  if (current_p == end_p)
  {
    token_p->type = TOKEN_END;
    return;
  }

  switch (*current_p)
  {
    case LIT_CHAR_LEFT_BRACE:
    {
      token_p->type = TOKEN_LEFT_BRACE;
      token_p->current_p = current_p + 1;
      return;
    }
    case LIT_CHAR_RIGHT_BRACE:
    {
      token_p->type = TOKEN_RIGHT_BRACE;
      token_p->current_p = current_p + 1;
      return;
    }
    case LIT_CHAR_LEFT_SQUARE:
    {
      token_p->type = TOKEN_LEFT_SQUARE;
      token_p->current_p = current_p + 1;
      return;
    }
    case LIT_CHAR_RIGHT_SQUARE:
    {
      token_p->type = TOKEN_RIGHT_SQUARE;
      token_p->current_p = current_p + 1;
      return;
    }
    case LIT_CHAR_COMMA:
    {
      token_p->type = TOKEN_COMMA;
      token_p->current_p = current_p + 1;
      return;
    }
    case LIT_CHAR_COLON:
    {
      token_p->type = TOKEN_COLON;
      token_p->current_p = current_p + 1;
      return;
    }
    case LIT_CHAR_DOUBLE_QUOTE:
    {
      if (parse_string)
      {
        token_p->current_p = current_p + 1;
        ecma_builtin_json_parse_string (token_p);
      }
      return;
    }
    case LIT_CHAR_LOWERCASE_N:
    {
      lit_utf8_size_t size = lit_get_magic_string_size (LIT_MAGIC_STRING_NULL);
      if (current_p + size <= end_p)
      {
        if (!memcmp (lit_get_magic_string_utf8 (LIT_MAGIC_STRING_NULL), current_p, size))
        {
          token_p->type = TOKEN_NULL;
          token_p->current_p = current_p + size;
          return;
        }
      }
      break;
    }
    case LIT_CHAR_LOWERCASE_T:
    {
      lit_utf8_size_t size = lit_get_magic_string_size (LIT_MAGIC_STRING_TRUE);
      if (current_p + size <= end_p)
      {
        if (!memcmp (lit_get_magic_string_utf8 (LIT_MAGIC_STRING_TRUE), current_p, size))
        {
          token_p->type = TOKEN_TRUE;
          token_p->current_p = current_p + size;
          return;
        }
      }
      break;
    }
    case LIT_CHAR_LOWERCASE_F:
    {
      lit_utf8_size_t size = lit_get_magic_string_size (LIT_MAGIC_STRING_FALSE);
      if (current_p + size <= end_p)
      {
        if (!memcmp (lit_get_magic_string_utf8 (LIT_MAGIC_STRING_FALSE), current_p, size))
        {
          token_p->type = TOKEN_FALSE;
          token_p->current_p = current_p + size;
          return;
        }
      }
      break;
    }
    default:
    {
      if (*current_p == LIT_CHAR_MINUS || lit_char_is_decimal_digit (*current_p))
      {
        token_p->current_p = current_p;
        ecma_builtin_json_parse_number (token_p);
        return;
      }
      break;
    }
  }
} /* ecma_builtin_json_parse_next_token */

/**
 * Utility for defining properties.
 *
 * It silently ignores all errors.
 */
static void
ecma_builtin_json_define_value_property (ecma_object_t *obj_p, /**< this object */
                                         ecma_string_t *property_name_p, /**< property name */
                                         ecma_value_t value) /**< value */
{
  ecma_value_t completion_value =
    ecma_builtin_helper_def_prop (obj_p, property_name_p, value, ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE);

  JERRY_ASSERT (ecma_is_value_boolean (completion_value));
} /* ecma_builtin_json_define_value_property */

/**
 * Parse next value.
 *
 * The function fills the fields of the ecma_json_token_t
 * argument and advances the string pointer.
 *
 * @return ecma_value with the property value
 */
static ecma_value_t
ecma_builtin_json_parse_value (ecma_json_token_t *token_p) /**< token argument */
{
  switch (token_p->type)
  {
    case TOKEN_NUMBER:
    {
      return ecma_make_number_value (token_p->u.number);
    }
    case TOKEN_STRING:
    {
      return ecma_make_string_value (token_p->u.string_p);
    }
    case TOKEN_NULL:
    {
      return ECMA_VALUE_NULL;
    }
    case TOKEN_TRUE:
    {
      return ECMA_VALUE_TRUE;
    }
    case TOKEN_FALSE:
    {
      return ECMA_VALUE_FALSE;
    }
    case TOKEN_LEFT_BRACE:
    {
      ecma_object_t *object_p = ecma_op_create_object_object_noarg ();

      ecma_builtin_json_parse_next_token (token_p, true);

      if (token_p->type == TOKEN_RIGHT_BRACE)
      {
        return ecma_make_object_value (object_p);
      }

      while (true)
      {
        if (token_p->type != TOKEN_STRING)
        {
          break;
        }

        ecma_string_t *name_p = token_p->u.string_p;

        ecma_builtin_json_parse_next_token (token_p, false);
        if (token_p->type != TOKEN_COLON)
        {
          ecma_deref_ecma_string (name_p);
          break;
        }

        ecma_builtin_json_parse_next_token (token_p, true);
        ecma_value_t value = ecma_builtin_json_parse_value (token_p);

        if (ecma_is_value_empty (value))
        {
          ecma_deref_ecma_string (name_p);
          break;
        }

        ecma_builtin_json_define_value_property (object_p, name_p, value);
        ecma_deref_ecma_string (name_p);
        ecma_free_value (value);

        ecma_builtin_json_parse_next_token (token_p, false);
        if (token_p->type == TOKEN_RIGHT_BRACE)
        {
          return ecma_make_object_value (object_p);
        }

        if (token_p->type != TOKEN_COMMA)
        {
          break;
        }

        ecma_builtin_json_parse_next_token (token_p, true);
      }

      /*
       * Parse error occured.
       */
      ecma_deref_object (object_p);
      return ECMA_VALUE_EMPTY;
    }
    case TOKEN_LEFT_SQUARE:
    {
      uint32_t length = 0;
      ecma_object_t *array_p = ecma_op_new_array_object (0);

      ecma_builtin_json_parse_next_token (token_p, true);

      if (token_p->type == TOKEN_RIGHT_SQUARE)
      {
        return ecma_make_object_value (array_p);
      }

      while (true)
      {
        ecma_value_t value = ecma_builtin_json_parse_value (token_p);

        if (ecma_is_value_empty (value))
        {
          JERRY_ASSERT (token_p->type != TOKEN_STRING);
          break;
        }

        ecma_value_t completion;
        completion = ecma_builtin_helper_def_prop_by_index (array_p,
                                                            length,
                                                            value,
                                                            ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE);
        JERRY_ASSERT (ecma_is_value_true (completion));
        ecma_free_value (value);

        ecma_builtin_json_parse_next_token (token_p, false);

        if (token_p->type == TOKEN_RIGHT_SQUARE)
        {
          return ecma_make_object_value (array_p);
        }

        if (token_p->type != TOKEN_COMMA)
        {
          JERRY_ASSERT (token_p->type != TOKEN_STRING);
          break;
        }

        ecma_builtin_json_parse_next_token (token_p, true);
        length++;
      }

      ecma_deref_object (array_p);
      return ECMA_VALUE_EMPTY;
    }
    default:
    {
      return ECMA_VALUE_EMPTY;
    }
  }
} /* ecma_builtin_json_parse_value */

static ecma_value_t ecma_builtin_json_internalize_process_property (ecma_object_t *reviver_p,
                                                                    ecma_object_t *object_p,
                                                                    ecma_string_t *prop_name);

/**
 * Abstract operation InternalizeJSONProperty
 *
 * See also:
 *         ECMA-262 v5, 24.3.1.1
 *         ECMA-262 v11, 24.5.1.1
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_json_internalize_property (ecma_object_t *reviver_p, /**< reviver function */
                                        ecma_object_t *holder_p, /**< holder object */
                                        ecma_string_t *name_p) /**< property name */
{
  JERRY_ASSERT (reviver_p);
  JERRY_ASSERT (holder_p);
  JERRY_ASSERT (name_p);

  /* 1. */
  ecma_value_t value = ecma_op_object_get (holder_p, name_p);

  /* 2. */
  if (ECMA_IS_VALUE_ERROR (value))
  {
    return value;
  }

  /* 3. */
  if (ecma_is_value_object (value))
  {
    /* 3.a */
    ecma_value_t is_array = ecma_is_value_array (value);

#if JERRY_ESNEXT
    if (ECMA_IS_VALUE_ERROR (is_array))
    {
      ecma_free_value (value);
      return is_array;
    }
#endif /* JERRY_ESNEXT */

    ecma_object_t *object_p = ecma_get_object_from_value (value);

    /* 3.c */
    if (ecma_is_value_true (is_array))
    {
      /* 3.c.ii */
      ecma_length_t length;
      ecma_value_t to_len = ecma_op_object_get_length (object_p, &length);

      /* 3.c.iii */
#if JERRY_BUILTIN_PROXY
      if (ECMA_IS_VALUE_ERROR (to_len))
      {
        ecma_deref_object (object_p);
        return to_len;
      }
#endif /* JERRY_BUILTIN_PROXY */
      JERRY_ASSERT (ecma_is_value_empty (to_len));

      /* 3.c.iv */
      for (ecma_length_t i = 0; i < length; i++)
      {
        ecma_string_t *prop_index = ecma_new_ecma_string_from_length (i);
        ecma_value_t result = ecma_builtin_json_internalize_process_property (reviver_p, object_p, prop_index);

        ecma_deref_ecma_string (prop_index);

        if (ECMA_IS_VALUE_ERROR (result))
        {
          ecma_deref_object (object_p);
          return result;
        }

        JERRY_ASSERT (result == ECMA_VALUE_TRUE);
      }
    }
    /* 3.d */
    else
    {
      ecma_collection_t *props_p =
        ecma_op_object_get_enumerable_property_names (object_p, ECMA_ENUMERABLE_PROPERTY_KEYS);
#if JERRY_ESNEXT
      if (JERRY_UNLIKELY (props_p == NULL))
      {
        ecma_deref_object (object_p);
        return ECMA_VALUE_ERROR;
      }
#else /* !JERRY_ESNEXT */
      JERRY_ASSERT (props_p != NULL);
#endif /* JERRY_ESNEXT */

      ecma_value_t *buffer_p = props_p->buffer_p;

      /* 3.d.iii */
      for (uint32_t i = 0; i < props_p->item_count; i++)
      {
        ecma_string_t *property_name_p = ecma_get_string_from_value (buffer_p[i]);
        ecma_value_t result = ecma_builtin_json_internalize_process_property (reviver_p, object_p, property_name_p);

        if (ECMA_IS_VALUE_ERROR (result))
        {
          ecma_collection_free (props_p);
          ecma_deref_object (object_p);
          return result;
        }

        JERRY_ASSERT (result == ECMA_VALUE_TRUE);
      }

      ecma_collection_free (props_p);
    }
  }

  ecma_value_t arguments_list[2];
  arguments_list[0] = ecma_make_string_value (name_p);
  arguments_list[1] = value;

  /* 4. */
  ecma_value_t ret_value = ecma_op_function_call (reviver_p, ecma_make_object_value (holder_p), arguments_list, 2);
  ecma_free_value (value);
  return ret_value;
} /* ecma_builtin_json_internalize_property */

/**
 * Part of the InternalizeJSONProperty abstract method.
 *
 * See also:
 *         ECMA-262 v5, 15.12.2
 *         ECMA-262 v11, 24.5.1.1 in step 2
 *
 * @return ECMA_VALUE_TRUE - if no error occured.
 *         error if one of the operation failed.
 */
static ecma_value_t
ecma_builtin_json_internalize_process_property (ecma_object_t *reviver_p, /**< reviver function */
                                                ecma_object_t *object_p, /**< holder object */
                                                ecma_string_t *prop_name) /**< property name */
{
  /* ES11: 2.b.iii.1 / 2.c.ii.1 */
  ecma_value_t new_element = ecma_builtin_json_internalize_property (reviver_p, object_p, prop_name);

  if (ECMA_IS_VALUE_ERROR (new_element))
  {
    return new_element;
  }

  /* ES11: 2.b.iii.2 / 2.c.ii.2 */
  if (ecma_is_value_undefined (new_element))
  {
    /* ES11: 2.b.iii.2.a / 2.c.ii.2.a */
    ecma_value_t delete_val = ecma_op_object_delete (object_p, prop_name, false);

#if JERRY_ESNEXT
    if (ECMA_IS_VALUE_ERROR (delete_val))
    {
      return delete_val;
    }
#endif /* JERRY_ESNEXT */

    JERRY_ASSERT (ecma_is_value_boolean (delete_val));
  }
  else
  {
    /* ES11: 2.b.iii.3.a / 2.c.ii.3.a */
    ecma_value_t def_value =
      ecma_builtin_helper_def_prop (object_p, prop_name, new_element, ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE);
    ecma_free_value (new_element);

#if JERRY_ESNEXT
    if (ECMA_IS_VALUE_ERROR (def_value))
    {
      return def_value;
    }
#endif /* JERRY_ESNEXT */

    JERRY_ASSERT (ecma_is_value_boolean (def_value));
  }

  return ECMA_VALUE_TRUE;
} /* ecma_builtin_json_internalize_process_property */

/**
 * Function to set a string token from the given arguments, fills its fields and advances the string pointer.
 *
 * @return ecma_value_t containing an object or an error massage
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_builtin_json_parse_buffer (const lit_utf8_byte_t *str_start_p, /**< String to parse */
                                lit_utf8_size_t string_size) /**< size of the string */
{
  ecma_json_token_t token;
  token.current_p = str_start_p;
  token.end_p = str_start_p + string_size;

  ecma_builtin_json_parse_next_token (&token, true);
  ecma_value_t result = ecma_builtin_json_parse_value (&token);

  if (!ecma_is_value_empty (result))
  {
    ecma_builtin_json_parse_next_token (&token, false);
    if (token.type == TOKEN_END)
    {
      return result;
    }

    ecma_free_value (result);
  }

  return ecma_raise_syntax_error (ECMA_ERR_MSG ("Invalid JSON format"));
} /*ecma_builtin_json_parse_buffer*/

/**
 * The JSON object's 'parse' routine
 *
 * See also:
 *          ECMA-262 v5, 15.12.2
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_json_parse (ecma_value_t this_arg, /**< 'this' argument */
                         ecma_value_t arg1, /**< string argument */
                         ecma_value_t arg2) /**< reviver argument */
{
  JERRY_UNUSED (this_arg);

  ecma_string_t *text_string_p = ecma_op_to_string (arg1);

  if (JERRY_UNLIKELY (text_string_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }

  ECMA_STRING_TO_UTF8_STRING (text_string_p, str_start_p, string_size);
  ecma_value_t result = ecma_builtin_json_parse_buffer (str_start_p, string_size);
  ECMA_FINALIZE_UTF8_STRING (str_start_p, string_size);
  ecma_deref_ecma_string (text_string_p);

  if (!ECMA_IS_VALUE_ERROR (result) && ecma_op_is_callable (arg2))
  {
    ecma_object_t *object_p = ecma_op_create_object_object_noarg ();

    ecma_property_value_t *prop_value_p =
      ecma_create_named_data_property (object_p,
                                       ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY),
                                       ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE,
                                       NULL);

    ecma_named_data_property_assign_value (object_p, prop_value_p, result);

    ecma_free_value (result);
    result = ecma_builtin_json_internalize_property (ecma_get_object_from_value (arg2),
                                                     object_p,
                                                     ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY));
    ecma_deref_object (object_p);
  }

  return result;
} /* ecma_builtin_json_parse */

/**
 * Abstract operation 'QuoteJSONString' defined in 24.3.2.2
 */
static void
ecma_builtin_json_quote (ecma_stringbuilder_t *builder_p, /**< builder for the result */
                         ecma_string_t *string_p) /**< string that should be quoted */
{
  ECMA_STRING_TO_UTF8_STRING (string_p, string_buff, string_buff_size);
  const lit_utf8_byte_t *str_p = string_buff;
  const lit_utf8_byte_t *regular_str_start_p = string_buff;
  const lit_utf8_byte_t *str_end_p = str_p + string_buff_size;

  ecma_stringbuilder_append_byte (builder_p, LIT_CHAR_DOUBLE_QUOTE);

  while (str_p < str_end_p)
  {
    ecma_char_t c = lit_cesu8_read_next (&str_p);

    bool should_escape = false;

#if JERRY_ESNEXT
    if (lit_is_code_point_utf16_high_surrogate (c))
    {
      if (str_p < str_end_p)
      {
        const ecma_char_t next_ch = lit_cesu8_peek_next (str_p);
        if (lit_is_code_point_utf16_low_surrogate (next_ch))
        {
          str_p += LIT_UTF8_MAX_BYTES_IN_CODE_UNIT;
          continue;
        }
        should_escape = true;
      }
      else
      {
        should_escape = true;
      }
    }
    else if (lit_is_code_point_utf16_low_surrogate (c))
    {
      should_escape = true;
    }
#endif /* JERRY_ESNEXT */

    if (c == LIT_CHAR_BACKSLASH || c == LIT_CHAR_DOUBLE_QUOTE)
    {
      ecma_stringbuilder_append_raw (builder_p,
                                     regular_str_start_p,
                                     (lit_utf8_size_t) (str_p - regular_str_start_p - 1));
      regular_str_start_p = str_p;
      ecma_stringbuilder_append_byte (builder_p, LIT_CHAR_BACKSLASH);
      ecma_stringbuilder_append_byte (builder_p, (lit_utf8_byte_t) c);
    }
    else if (c < LIT_CHAR_SP || should_escape)
    {
      /**
       * In ES10 we should escape high or low surrogate characters,
       * so we shouldn't append the unescaped character to the stringbuilder
       */
      uint8_t offset = should_escape ? LIT_UTF8_MAX_BYTES_IN_CODE_UNIT : 1;

      ecma_stringbuilder_append_raw (builder_p,
                                     regular_str_start_p,
                                     (lit_utf8_size_t) (str_p - regular_str_start_p - offset));

      regular_str_start_p = str_p;

      switch (c)
      {
        case LIT_CHAR_BS:
        {
          ecma_stringbuilder_append_raw (builder_p, (lit_utf8_byte_t *) "\\b", 2);
          break;
        }
        case LIT_CHAR_FF:
        {
          ecma_stringbuilder_append_raw (builder_p, (lit_utf8_byte_t *) "\\f", 2);
          break;
        }
        case LIT_CHAR_LF:
        {
          ecma_stringbuilder_append_raw (builder_p, (lit_utf8_byte_t *) "\\n", 2);
          break;
        }
        case LIT_CHAR_CR:
        {
          ecma_stringbuilder_append_raw (builder_p, (lit_utf8_byte_t *) "\\r", 2);
          break;
        }
        case LIT_CHAR_TAB:
        {
          ecma_stringbuilder_append_raw (builder_p, (lit_utf8_byte_t *) "\\t", 2);
          break;
        }
        default: /* Hexadecimal. */
        {
          lit_char_unicode_escape (builder_p, c);
          break;
        }
      }
    }
  }

  ecma_stringbuilder_append_raw (builder_p, regular_str_start_p, (lit_utf8_size_t) (str_end_p - regular_str_start_p));
  ecma_stringbuilder_append_byte (builder_p, LIT_CHAR_DOUBLE_QUOTE);

  ECMA_FINALIZE_UTF8_STRING (string_buff, string_buff_size);
} /* ecma_builtin_json_quote */

static ecma_value_t ecma_builtin_json_serialize_property (ecma_json_stringify_context_t *context_p,
                                                          ecma_object_t *holder_p,
                                                          ecma_string_t *key_p);

/**
 * Abstract operation 'SerializeJSONObject' defined in 24.3.2.3
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_json_serialize_object (ecma_json_stringify_context_t *context_p, /**< context*/
                                    ecma_object_t *obj_p) /**< the object*/
{
  /* 1. */
  if (ecma_json_has_object_in_stack (context_p->occurence_stack_last_p, obj_p))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("The structure is cyclical"));
  }

  /* 2. */
  ecma_json_occurence_stack_item_t stack_item;
  stack_item.next_p = context_p->occurence_stack_last_p;
  stack_item.object_p = obj_p;
  context_p->occurence_stack_last_p = &stack_item;

  /* 3. - 4.*/
  const lit_utf8_size_t stepback_size = ecma_stringbuilder_get_size (&context_p->indent_builder);
  ecma_stringbuilder_append (&context_p->indent_builder, context_p->gap_str_p);

  const bool has_gap = !ecma_compare_ecma_string_to_magic_id (context_p->gap_str_p, LIT_MAGIC_STRING__EMPTY);
  const lit_utf8_size_t separator_size = ecma_stringbuilder_get_size (&context_p->indent_builder);

  ecma_collection_t *property_keys_p;
  /* 5. */
  if (context_p->property_list_p != NULL)
  {
    property_keys_p = context_p->property_list_p;
  }
  /* 6. */
  else
  {
    property_keys_p = ecma_op_object_get_enumerable_property_names (obj_p, ECMA_ENUMERABLE_PROPERTY_KEYS);

#if JERRY_BUILTIN_PROXY
    if (property_keys_p == NULL)
    {
      return ECMA_VALUE_ERROR;
    }
#endif /* JERRY_BUILTIN_PROXY */
  }

  /* 8. */
  ecma_value_t *buffer_p = property_keys_p->buffer_p;

  ecma_stringbuilder_append_byte (&context_p->result_builder, LIT_CHAR_LEFT_BRACE);
  const lit_utf8_size_t left_brace = ecma_stringbuilder_get_size (&context_p->result_builder);
  lit_utf8_size_t last_prop = left_brace;
  ecma_value_t result = ECMA_VALUE_EMPTY;

  for (uint32_t i = 0; i < property_keys_p->item_count; i++)
  {
    if (has_gap)
    {
      ecma_stringbuilder_append_raw (&context_p->result_builder,
                                     ecma_stringbuilder_get_data (&context_p->indent_builder),
                                     separator_size);
    }

    ecma_string_t *key_p = ecma_get_string_from_value (buffer_p[i]);
    ecma_builtin_json_quote (&context_p->result_builder, key_p);
    ecma_stringbuilder_append_byte (&context_p->result_builder, LIT_CHAR_COLON);

    /* 8.c.iii */
    if (has_gap)
    {
      ecma_stringbuilder_append_byte (&context_p->result_builder, LIT_CHAR_SP);
    }

    result = ecma_builtin_json_serialize_property (context_p, obj_p, key_p);

    if (ECMA_IS_VALUE_ERROR (result))
    {
      goto cleanup;
    }

    /* 8.b */
    if (!ecma_is_value_undefined (result))
    {
      /* ecma_builtin_json_serialize_property already appended the result. */
      JERRY_ASSERT (ecma_is_value_empty (result));

      ecma_stringbuilder_append_byte (&context_p->result_builder, LIT_CHAR_COMMA);
      last_prop = ecma_stringbuilder_get_size (&context_p->result_builder);
    }
    else
    {
      /* The property should not be appended, we must backtrack. */
      ecma_stringbuilder_revert (&context_p->result_builder, last_prop);
    }
  }

  if (last_prop != left_brace)
  {
    /* Remove the last comma. */
    ecma_stringbuilder_revert (&context_p->result_builder, last_prop - 1);

    if (has_gap)
    {
      /* We appended at least one element, and have a separator, so must append the stepback. */
      ecma_stringbuilder_append_raw (&context_p->result_builder,
                                     ecma_stringbuilder_get_data (&context_p->indent_builder),
                                     stepback_size);
    }
  }

  ecma_stringbuilder_append_byte (&context_p->result_builder, LIT_CHAR_RIGHT_BRACE);
  result = ECMA_VALUE_EMPTY;

  /* 11. */
  context_p->occurence_stack_last_p = stack_item.next_p;

  /* 12. */
  ecma_stringbuilder_revert (&context_p->indent_builder, stepback_size);

cleanup:
  if (context_p->property_list_p == NULL)
  {
    ecma_collection_free (property_keys_p);
  }

  return result;
} /* ecma_builtin_json_serialize_object */

/**
 * Abstract operation 'SerializeJSONArray' defined in 24.3.2.4
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_json_serialize_array (ecma_json_stringify_context_t *context_p, /**< context*/
                                   ecma_object_t *obj_p) /**< the array object*/
{
#ifndef JERRY_NDEBUG
  ecma_value_t obj_value = ecma_make_object_value (obj_p);
  ecma_value_t is_array = ecma_is_value_array (obj_value);

  JERRY_ASSERT (ecma_is_value_true (is_array));
#endif /* !JERRY_NDEBUG */

  /* 1. */
  if (ecma_json_has_object_in_stack (context_p->occurence_stack_last_p, obj_p))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("The structure is cyclical"));
  }

  /* 2. */
  ecma_json_occurence_stack_item_t stack_item;
  stack_item.next_p = context_p->occurence_stack_last_p;
  stack_item.object_p = obj_p;
  context_p->occurence_stack_last_p = &stack_item;

  /* 3. - 4.*/
  const lit_utf8_size_t stepback_size = ecma_stringbuilder_get_size (&context_p->indent_builder);
  ecma_stringbuilder_append (&context_p->indent_builder, context_p->gap_str_p);
  const lit_utf8_size_t separator_size = ecma_stringbuilder_get_size (&context_p->indent_builder);

  const bool has_gap = !ecma_compare_ecma_string_to_magic_id (context_p->gap_str_p, LIT_MAGIC_STRING__EMPTY);

  /* 6. */
  ecma_length_t array_length;
  ecma_value_t length_value = ecma_op_object_get_length (obj_p, &array_length);

#if JERRY_BUILTIN_PROXY
  if (ECMA_IS_VALUE_ERROR (length_value))
  {
    return length_value;
  }
#endif /* JERRY_BUILTIN_PROXY */
  JERRY_ASSERT (ecma_is_value_empty (length_value));

  ecma_stringbuilder_append_byte (&context_p->result_builder, LIT_CHAR_LEFT_SQUARE);

  const lit_utf8_size_t left_square = ecma_stringbuilder_get_size (&context_p->result_builder);
  lit_utf8_size_t last_prop = left_square;

  /* 8. - 9. */
  for (ecma_length_t index = 0; index < array_length; index++)
  {
    /* 9.a */
    ecma_string_t *index_str_p = ecma_new_ecma_string_from_length (index);

    if (has_gap)
    {
      ecma_stringbuilder_append_raw (&context_p->result_builder,
                                     ecma_stringbuilder_get_data (&context_p->indent_builder),
                                     separator_size);
    }

    ecma_value_t result = ecma_builtin_json_serialize_property (context_p, obj_p, index_str_p);
    ecma_deref_ecma_string (index_str_p);

    if (ECMA_IS_VALUE_ERROR (result))
    {
      return result;
    }

    if (ecma_is_value_undefined (result))
    {
      /* 9.c */
      ecma_stringbuilder_append_magic (&context_p->result_builder, LIT_MAGIC_STRING_NULL);
    }
    else
    {
      JERRY_ASSERT (ecma_is_value_empty (result));
    }

    last_prop = ecma_stringbuilder_get_size (&context_p->result_builder);
    ecma_stringbuilder_append_byte (&context_p->result_builder, LIT_CHAR_COMMA);
  }

  /* Remove the last comma. */
  ecma_stringbuilder_revert (&context_p->result_builder, last_prop);

  /* 11.b.iii */
  if (last_prop != left_square && has_gap)
  {
    /* We appended at least one element, and have a separator, so must append the stepback. */
    ecma_stringbuilder_append_raw (&context_p->result_builder,
                                   ecma_stringbuilder_get_data (&context_p->indent_builder),
                                   stepback_size);
  }

  ecma_stringbuilder_append_byte (&context_p->result_builder, LIT_CHAR_RIGHT_SQUARE);

  /* 12. */
  context_p->occurence_stack_last_p = stack_item.next_p;

  /* 13. */
  ecma_stringbuilder_revert (&context_p->indent_builder, stepback_size);

  return ECMA_VALUE_EMPTY;
} /* ecma_builtin_json_serialize_array */

/**
 * Abstract operation 'SerializeJSONProperty' defined in 24.3.2.1
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_json_serialize_property (ecma_json_stringify_context_t *context_p, /**< context*/
                                      ecma_object_t *holder_p, /**< the object*/
                                      ecma_string_t *key_p) /**< property key*/
{
  /* 1. */
  ecma_value_t value = ecma_op_object_get (holder_p, key_p);

  /* 2. */
  if (ECMA_IS_VALUE_ERROR (value))
  {
    return value;
  }

  /* 3. */
  if (ecma_is_value_object (value) || ecma_is_value_bigint (value))
  {
    ecma_value_t to_object_value = ecma_op_to_object (value);

    if (ECMA_IS_VALUE_ERROR (to_object_value))
    {
      ecma_free_value (value);
      return to_object_value;
    }

    ecma_object_t *value_obj_p = ecma_get_object_from_value (to_object_value);
    ecma_value_t to_json =
      ecma_op_object_get_with_receiver (value_obj_p, ecma_get_magic_string (LIT_MAGIC_STRING_TO_JSON_UL), value);

    ecma_deref_object (value_obj_p);

    if (ECMA_IS_VALUE_ERROR (to_json))
    {
      ecma_free_value (value);
      return to_json;
    }

    /* 3.c */
    if (ecma_op_is_callable (to_json))
    {
      ecma_value_t key_value = ecma_make_string_value (key_p);
      ecma_value_t call_args[] = { key_value };
      ecma_object_t *to_json_obj_p = ecma_get_object_from_value (to_json);

      ecma_value_t result = ecma_op_function_call (to_json_obj_p, value, call_args, 1);
      ecma_free_value (value);

      if (ECMA_IS_VALUE_ERROR (result))
      {
        ecma_deref_object (to_json_obj_p);
        return result;
      }
      value = result;
    }
    ecma_free_value (to_json);
  }

  /* 4. */
  if (context_p->replacer_function_p)
  {
    ecma_value_t holder_value = ecma_make_object_value (holder_p);
    ecma_value_t key_value = ecma_make_string_value (key_p);
    ecma_value_t call_args[] = { key_value, value };

    ecma_value_t result = ecma_op_function_call (context_p->replacer_function_p, holder_value, call_args, 2);
    ecma_free_value (value);

    if (ECMA_IS_VALUE_ERROR (result))
    {
      return result;
    }

    value = result;
  }

  /* 5. */
  if (ecma_is_value_object (value))
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (value);
    lit_magic_string_id_t class_name = ecma_object_get_class_name (obj_p);

    /* 5.a */
    if (class_name == LIT_MAGIC_STRING_NUMBER_UL)
    {
      ecma_number_t num;
      value = ecma_op_to_number (value, &num);
      ecma_deref_object (obj_p);

      if (ECMA_IS_VALUE_ERROR (value))
      {
        return value;
      }

      value = ecma_make_number_value (num);
    }
    /* 5.b */
    else if (class_name == LIT_MAGIC_STRING_STRING_UL)
    {
      ecma_string_t *str_p = ecma_op_to_string (value);
      ecma_deref_object (obj_p);

      if (JERRY_UNLIKELY (str_p == NULL))
      {
        return ECMA_VALUE_ERROR;
      }

      value = ecma_make_string_value (str_p);
    }
    /* 5.c */
    else if (class_name == LIT_MAGIC_STRING_BOOLEAN_UL)
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) obj_p;
      value = ext_object_p->u.class_prop.u.value;
      ecma_deref_object (obj_p);
    }
#if JERRY_BUILTIN_BIGINT
    /* 5.d */
    else if (class_name == LIT_MAGIC_STRING_BIGINT_UL)
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) obj_p;
      value = ecma_copy_value (ext_object_p->u.class_prop.u.value);
      ecma_deref_object (obj_p);
    }
#endif /* JERRY_BUILTIN_BIGINT */
  }

  /* 6. - 8. */
  if (ecma_is_value_null (value))
  {
    ecma_stringbuilder_append_magic (&context_p->result_builder, LIT_MAGIC_STRING_NULL);
    return ECMA_VALUE_EMPTY;
  }

  if (ecma_is_value_true (value))
  {
    ecma_stringbuilder_append_magic (&context_p->result_builder, LIT_MAGIC_STRING_TRUE);
    return ECMA_VALUE_EMPTY;
  }

  if (ecma_is_value_false (value))
  {
    ecma_stringbuilder_append_magic (&context_p->result_builder, LIT_MAGIC_STRING_FALSE);
    return ECMA_VALUE_EMPTY;
  }

  /* 9. */
  if (ecma_is_value_string (value))
  {
    ecma_string_t *value_str_p = ecma_get_string_from_value (value);
    /* Quote will append the result. */
    ecma_builtin_json_quote (&context_p->result_builder, value_str_p);
    ecma_deref_ecma_string (value_str_p);

    return ECMA_VALUE_EMPTY;
  }

  /* 10. */
  if (ecma_is_value_number (value))
  {
    ecma_number_t num_value = ecma_get_number_from_value (value);

    /* 10.a */
    if (!ecma_number_is_nan (num_value) && !ecma_number_is_infinity (num_value))
    {
      ecma_string_t *result_string_p = ecma_op_to_string (value);
      JERRY_ASSERT (result_string_p != NULL);

      ecma_stringbuilder_append (&context_p->result_builder, result_string_p);
      ecma_deref_ecma_string (result_string_p);
    }
    else
    {
      /* 10.b */
      ecma_stringbuilder_append_magic (&context_p->result_builder, LIT_MAGIC_STRING_NULL);
    }

    ecma_free_value (value);
    return ECMA_VALUE_EMPTY;
  }

#if JERRY_BUILTIN_BIGINT
  if (ecma_is_value_bigint (value))
  {
    ecma_free_value (value);
    return ecma_raise_type_error (ECMA_ERR_MSG ("BigInt cannot be serialized"));
  }
#endif /* JERRY_BUILTIN_BIGINT */

  /* 11. */
  if (ecma_is_value_object (value) && !ecma_op_is_callable (value))
  {
    ecma_value_t is_array = ecma_is_value_array (value);

#if JERRY_BUILTIN_BIGINT
    if (ECMA_IS_VALUE_ERROR (is_array))
    {
      ecma_free_value (value);
      return is_array;
    }
#endif /* JERRY_BUILTIN_BIGINT */

    ecma_object_t *obj_p = ecma_get_object_from_value (value);

    ecma_value_t ret_value;
    /* 10.a */
    if (ecma_is_value_true (is_array))
    {
      ret_value = ecma_builtin_json_serialize_array (context_p, obj_p);
    }
    /* 10.b */
    else
    {
      ret_value = ecma_builtin_json_serialize_object (context_p, obj_p);
    }

    ecma_deref_object (obj_p);
    return ret_value;
  }

  /* 12. */
  ecma_free_value (value);
  return ECMA_VALUE_UNDEFINED;
} /* ecma_builtin_json_serialize_property */

/**
 * Helper function to stringify an object in JSON format representing an ecma_value.
 *
 *  @return ecma_value_t string created from an abject formating by a given context
 *          Returned value must be freed with ecma_free_value.
 *
 */
static ecma_value_t
ecma_builtin_json_str_helper (ecma_json_stringify_context_t *context_p, /**< context argument */
                              const ecma_value_t arg1) /**< object argument */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;
  ecma_object_t *obj_wrapper_p = ecma_op_create_object_object_noarg ();
  ecma_string_t *empty_str_p = ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY);
  ecma_value_t put_comp_val =
    ecma_builtin_helper_def_prop (obj_wrapper_p, empty_str_p, arg1, ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE);

  JERRY_ASSERT (ecma_is_value_true (put_comp_val));

  context_p->result_builder = ecma_stringbuilder_create ();

  if (!ecma_compare_ecma_string_to_magic_id (context_p->gap_str_p, LIT_MAGIC_STRING__EMPTY))
  {
    ecma_stringbuilder_append_byte (&context_p->indent_builder, LIT_CHAR_LF);
  }

  ret_value = ecma_builtin_json_serialize_property (context_p, obj_wrapper_p, empty_str_p);
  ecma_deref_object (obj_wrapper_p);

  if (ECMA_IS_VALUE_ERROR (ret_value) || ecma_is_value_undefined (ret_value))
  {
    ecma_stringbuilder_destroy (&context_p->result_builder);
    return ret_value;
  }

  return ecma_make_string_value (ecma_stringbuilder_finalize (&context_p->result_builder));
} /* ecma_builtin_json_str_helper */

/**
 * Function to create a JSON string from a JS value.
 *
 * Note:
 *      The returned value must be freed with ecma_free_value.
 *
 * @return - ecma_value_t containing a json string.
 *         - Error value in case of any errors.
 */
ecma_value_t
ecma_builtin_json_stringify_no_opts (const ecma_value_t value) /**< value to stringify */
{
  ecma_json_stringify_context_t context;
  context.occurence_stack_last_p = NULL;
  context.indent_builder = ecma_stringbuilder_create ();
  context.property_list_p = NULL;
  context.replacer_function_p = NULL;
  context.gap_str_p = ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY);

  ecma_value_t ret_value = ecma_builtin_json_str_helper (&context, value);

  ecma_deref_ecma_string (context.gap_str_p);
  ecma_stringbuilder_destroy (&context.indent_builder);
  return ret_value;
} /* ecma_builtin_json_stringify_no_opts */

/**
 * The JSON object's 'stringify' routine
 *
 * See also:
 *          ECMA-262 v5, 15.12.3
 *          ECMA-262 v11, 24.5.2
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_json_stringify (ecma_value_t this_arg, /**< 'this' argument */
                             ecma_value_t arg1, /**< value */
                             ecma_value_t arg2, /**< replacer */
                             ecma_value_t arg3) /**< space */
{
  JERRY_UNUSED (this_arg);

  ecma_json_stringify_context_t context;
  context.replacer_function_p = NULL;
  context.property_list_p = NULL;

  /* 4. */
  if (ecma_is_value_object (arg2))
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (arg2);

    /* 4.a */
    if (ecma_op_is_callable (arg2))
    {
      context.replacer_function_p = obj_p;
    }
    /* 4.b */
    else
    {
      ecma_value_t is_array = ecma_is_value_array (arg2);

#if JERRY_ESNEXT
      if (ECMA_IS_VALUE_ERROR (is_array))
      {
        return is_array;
      }
#endif /* JERRY_ESNEXT */

      if (ecma_is_value_true (is_array))
      {
        ecma_length_t array_length;
        ecma_value_t to_len = ecma_op_object_get_length (obj_p, &array_length);

#if JERRY_BUILTIN_PROXY
        if (ECMA_IS_VALUE_ERROR (to_len))
        {
          return to_len;
        }
#endif /* JERRY_BUILTIN_PROXY */
        JERRY_ASSERT (ecma_is_value_empty (to_len));

        context.property_list_p = ecma_new_collection ();

        uint32_t index = 0;

        /* 4.b.iii.5 */
        while (index < array_length)
        {
          ecma_value_t value = ecma_op_object_get_by_index (obj_p, index);

          if (ECMA_IS_VALUE_ERROR (value))
          {
            ecma_collection_free (context.property_list_p);
            return value;
          }

          /* 4.b.iii.5.c */
          ecma_value_t item = ECMA_VALUE_UNDEFINED;

          /* 4.b.iii.5.d */
          if (ecma_is_value_string (value))
          {
            ecma_ref_ecma_string (ecma_get_string_from_value (value));
            item = value;
          }
          /* 4.b.iii.5.e */
          else if (ecma_is_value_number (value))
          {
            ecma_string_t *number_str_p = ecma_op_to_string (value);
            JERRY_ASSERT (number_str_p != NULL);
            item = ecma_make_string_value (number_str_p);
          }
          /* 4.b.iii.5.f */
          else if (ecma_is_value_object (value))
          {
            ecma_object_t *value_obj_p = ecma_get_object_from_value (value);
            lit_magic_string_id_t class_id = ecma_object_get_class_name (value_obj_p);

            if (class_id == LIT_MAGIC_STRING_NUMBER_UL || class_id == LIT_MAGIC_STRING_STRING_UL)
            {
              ecma_string_t *str_p = ecma_op_to_string (value);

              if (JERRY_UNLIKELY (str_p == NULL))
              {
                ecma_collection_free (context.property_list_p);
                ecma_free_value (value);
                return ECMA_VALUE_ERROR;
              }

              item = ecma_make_string_value (str_p);
            }
          }

          ecma_free_value (value);

          /* 4.b.iii.5.g */
          if (!ecma_is_value_undefined (item))
          {
            JERRY_ASSERT (ecma_is_value_string (item));
            ecma_string_t *string_p = ecma_get_string_from_value (item);

            if (!ecma_collection_has_string_value (context.property_list_p, string_p))
            {
              ecma_collection_push_back (context.property_list_p, item);
            }
            else
            {
              ecma_deref_ecma_string (string_p);
            }
          }

          index++;
        }
      }
    }
  }

  ecma_value_t space;

  /* 5. */
  if (ecma_is_value_object (arg3))
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (arg3);
    lit_magic_string_id_t class_name = ecma_object_get_class_name (obj_p);

    /* 5.a */
    if (class_name == LIT_MAGIC_STRING_NUMBER_UL)
    {
      ecma_number_t num;
      ecma_value_t value = ecma_op_to_number (arg3, &num);

      if (ECMA_IS_VALUE_ERROR (value))
      {
        if (context.property_list_p != NULL)
        {
          ecma_collection_free (context.property_list_p);
        }
        return value;
      }
      space = ecma_make_number_value (num);
    }
    /* 5.b */
    else if (class_name == LIT_MAGIC_STRING_STRING_UL)
    {
      ecma_string_t *value_str_p = ecma_op_to_string (arg3);

      if (JERRY_UNLIKELY (value_str_p == NULL))
      {
        if (context.property_list_p != NULL)
        {
          ecma_collection_free (context.property_list_p);
        }
        return ECMA_VALUE_ERROR;
      }

      space = ecma_make_string_value (value_str_p);
    }
    else
    {
      space = ecma_copy_value (arg3);
    }
  }
  else
  {
    space = ecma_copy_value (arg3);
  }

  /* 6. */
  if (ecma_is_value_number (space))
  {
    /* 6.a */
    ecma_number_t num_of_spaces;
    ecma_op_to_integer (space, &num_of_spaces);

    num_of_spaces = JERRY_MIN (10, num_of_spaces);

    /* 6.b */
    if (num_of_spaces < 1)
    {
      context.gap_str_p = ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY);
    }
    else
    {
      JMEM_DEFINE_LOCAL_ARRAY (space_buff, num_of_spaces, char);

      memset (space_buff, LIT_CHAR_SP, (size_t) num_of_spaces);
      context.gap_str_p =
        ecma_new_ecma_string_from_utf8 ((lit_utf8_byte_t *) space_buff, (lit_utf8_size_t) num_of_spaces);

      JMEM_FINALIZE_LOCAL_ARRAY (space_buff);
    }
  }
  /* 7. */
  else if (ecma_is_value_string (space))
  {
    ecma_string_t *space_str_p = ecma_get_string_from_value (space);
    lit_utf8_size_t num_of_chars = ecma_string_get_length (space_str_p);

    if (num_of_chars < 10)
    {
      ecma_ref_ecma_string (space_str_p);
      context.gap_str_p = space_str_p;
    }
    else
    {
      context.gap_str_p = ecma_string_substr (space_str_p, 0, 10);
    }
  }
  /* 8. */
  else
  {
    context.gap_str_p = ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY);
  }

  ecma_free_value (space);

  /* 1., 2., 3. */
  context.occurence_stack_last_p = NULL;
  context.indent_builder = ecma_stringbuilder_create ();

  /* 9. */
  ecma_value_t ret_value = ecma_builtin_json_str_helper (&context, arg1);

  ecma_deref_ecma_string (context.gap_str_p);
  ecma_stringbuilder_destroy (&context.indent_builder);

  if (context.property_list_p != NULL)
  {
    ecma_collection_free (context.property_list_p);
  }

  return ret_value;
} /* ecma_builtin_json_stringify */

/**
 * @}
 * @}
 * @}
 */

#endif /* JERRY_BUILTIN_JSON */

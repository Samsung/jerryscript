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

#ifndef JS_PARSER_H
#define JS_PARSER_H

#include "ecma-globals.h"

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
 * Error codes.
 */
typedef enum
{
  PARSER_ERR_NO_ERROR,                                /**< no error */

  PARSER_ERR_OUT_OF_MEMORY,                           /**< out of memory */
  PARSER_ERR_LITERAL_LIMIT_REACHED,                   /**< maximum number of literals reached */
  PARSER_ERR_SCOPE_STACK_LIMIT_REACHED,               /**< maximum depth of scope stack reached */
  PARSER_ERR_ARGUMENT_LIMIT_REACHED,                  /**< maximum number of function arguments reached */
  PARSER_ERR_STACK_LIMIT_REACHED,                     /**< maximum function stack size reached */

  PARSER_ERR_INVALID_CHARACTER,                       /**< unexpected character */
  PARSER_ERR_INVALID_OCTAL_DIGIT,                     /**< invalid octal digit */
  PARSER_ERR_INVALID_HEX_DIGIT,                       /**< invalid hexadecimal digit */
#if JERRY_ESNEXT
  PARSER_ERR_INVALID_BIN_DIGIT,                       /**< invalid binary digit */
#endif /* JERRY_ESNEXT */
  PARSER_ERR_INVALID_ESCAPE_SEQUENCE,                 /**< invalid escape sequence */
  PARSER_ERR_INVALID_UNICODE_ESCAPE_SEQUENCE,         /**< invalid unicode escape sequence */
  PARSER_ERR_INVALID_IDENTIFIER_START,                /**< character cannot be start of an identifier */
  PARSER_ERR_INVALID_IDENTIFIER_PART,                 /**< character cannot be part of an identifier */
  PARSER_ERR_INVALID_KEYWORD,                         /**< escape sequences are not allowed in keywords */

  PARSER_ERR_INVALID_NUMBER,                          /**< invalid number literal */
  PARSER_ERR_MISSING_EXPONENT,                        /**< missing exponent */
  PARSER_ERR_IDENTIFIER_AFTER_NUMBER,                 /**< identifier start after number */
  PARSER_ERR_INVALID_UNDERSCORE_IN_NUMBER,        /**< invalid use of underscore in number */
#if JERRY_BUILTIN_BIGINT
  PARSER_ERR_INVALID_BIGINT,                          /**< number is not a valid BigInt */
#endif /* JERRY_BUILTIN_BIGINT */

  PARSER_ERR_INVALID_REGEXP,                          /**< invalid regular expression */
  PARSER_ERR_UNKNOWN_REGEXP_FLAG,                     /**< unknown regexp flag */
  PARSER_ERR_DUPLICATED_REGEXP_FLAG,                  /**< duplicated regexp flag */
  PARSER_ERR_UNSUPPORTED_REGEXP,                      /**< regular expression is not supported */

  PARSER_ERR_IDENTIFIER_TOO_LONG,                     /**< too long identifier */
  PARSER_ERR_STRING_TOO_LONG,                         /**< too long string literal */
  PARSER_ERR_NUMBER_TOO_LONG,                         /**< too long number literal */
  PARSER_ERR_REGEXP_TOO_LONG,                         /**< too long regexp literal */

  PARSER_ERR_UNTERMINATED_MULTILINE_COMMENT,          /**< unterminated multiline comment */
  PARSER_ERR_UNTERMINATED_STRING,                     /**< unterminated string literal */
  PARSER_ERR_UNTERMINATED_REGEXP,                     /**< unterminated regexp literal */

  PARSER_ERR_NEWLINE_NOT_ALLOWED,                     /**< newline is not allowed */
  PARSER_ERR_OCTAL_NUMBER_NOT_ALLOWED,                /**< octal numbers are not allowed in strict mode */
  PARSER_ERR_OCTAL_ESCAPE_NOT_ALLOWED,                /**< octal escape sequences are not allowed in strict mode */
#if JERRY_ESNEXT
  PARSER_ERR_TEMPLATE_STR_OCTAL_ESCAPE,               /**< octal escape sequences are not allowed in template strings */
#endif /* JERRY_ESNEXT */
  PARSER_ERR_STRICT_IDENT_NOT_ALLOWED,                /**< identifier name is reserved in strict mode */
  PARSER_ERR_EVAL_NOT_ALLOWED,                        /**< eval is not allowed here in strict mode */
  PARSER_ERR_ARGUMENTS_NOT_ALLOWED,                   /**< arguments is not allowed here in strict mode */
#if JERRY_ESNEXT
  PARSER_ERR_USE_STRICT_NOT_ALLOWED,                  /**< use strict directive is not allowed */
  PARSER_ERR_YIELD_NOT_ALLOWED,                       /**< yield expression is not allowed */
  PARSER_ERR_AWAIT_NOT_ALLOWED,                       /**< await expression is not allowed */
  PARSER_ERR_FOR_IN_OF_DECLARATION,                   /**< variable declaration in for-in or for-of loop */
  PARSER_ERR_FOR_AWAIT_NO_ASYNC,                      /**< for-await-of is only allowed inside async functions */
  PARSER_ERR_FOR_AWAIT_NO_OF,                         /**< only 'of' form is allowed for for-await loops */
  PARSER_ERR_DUPLICATED_PROTO,                        /**< duplicated __proto__ fields are not allowed */
  PARSER_ERR_INVALID_LHS_ASSIGNMENT,                  /**< invalid LeftHandSide in assignment */
  PARSER_ERR_INVALID_LHS_POSTFIX_OP,                  /**< invalid LeftHandSide expression in postfix operation */
  PARSER_ERR_INVALID_LHS_PREFIX_OP,                   /**< invalid LeftHandSide expression in prefix operation */
  PARSER_ERR_INVALID_LHS_FOR_LOOP,                    /**< invalid LeftHandSide in for-loop */
#endif /* JERRY_ESNEXT */
  PARSER_ERR_DELETE_IDENT_NOT_ALLOWED,                /**< identifier delete is not allowed in strict mode */
  PARSER_ERR_EVAL_CANNOT_ASSIGNED,                    /**< eval cannot be assigned in strict mode */
  PARSER_ERR_ARGUMENTS_CANNOT_ASSIGNED,               /**< arguments cannot be assigned in strict mode */
  PARSER_ERR_WITH_NOT_ALLOWED,                        /**< with statement is not allowed in strict mode */
  PARSER_ERR_MULTIPLE_DEFAULTS_NOT_ALLOWED,           /**< multiple default cases are not allowed */
  PARSER_ERR_DEFAULT_NOT_IN_SWITCH,                   /**< default statement is not in switch block */
  PARSER_ERR_CASE_NOT_IN_SWITCH,                      /**< case statement is not in switch block */

  PARSER_ERR_LEFT_PAREN_EXPECTED,                     /**< left paren expected */
  PARSER_ERR_LEFT_BRACE_EXPECTED,                     /**< left brace expected */
  PARSER_ERR_RIGHT_PAREN_EXPECTED,                    /**< right paren expected */
  PARSER_ERR_RIGHT_SQUARE_EXPECTED,                   /**< right square expected */

  PARSER_ERR_COLON_EXPECTED,                          /**< colon expected */
  PARSER_ERR_COLON_FOR_CONDITIONAL_EXPECTED,          /**< colon expected for conditional expression */
  PARSER_ERR_SEMICOLON_EXPECTED,                      /**< semicolon expected */
  PARSER_ERR_IN_EXPECTED,                             /**< in keyword expected */
  PARSER_ERR_WHILE_EXPECTED,                          /**< while expected for do-while loop */
  PARSER_ERR_CATCH_FINALLY_EXPECTED,                  /**< catch or finally expected */
  PARSER_ERR_ARRAY_ITEM_SEPARATOR_EXPECTED,           /**< array item separator expected */
  PARSER_ERR_OBJECT_ITEM_SEPARATOR_EXPECTED,          /**< object item separator expected */
  PARSER_ERR_IDENTIFIER_EXPECTED,                     /**< identifier expected */
  PARSER_ERR_EXPRESSION_EXPECTED,                     /**< expression expected */
  PARSER_ERR_PRIMARY_EXP_EXPECTED,                    /**< primary expression expected */
  PARSER_ERR_LEFT_HAND_SIDE_EXP_EXPECTED,             /**< left-hand-side expression expected */
  PARSER_ERR_STATEMENT_EXPECTED,                      /**< statement expected */
  PARSER_ERR_PROPERTY_IDENTIFIER_EXPECTED,            /**< property identifier expected */
  PARSER_ERR_ARGUMENT_LIST_EXPECTED,                  /**< argument list expected */
  PARSER_ERR_NO_ARGUMENTS_EXPECTED,                   /**< property getters must have no arguments */
  PARSER_ERR_ONE_ARGUMENT_EXPECTED,                   /**< property setters must have one argument */

  PARSER_ERR_INVALID_EXPRESSION,                      /**< invalid expression */
  PARSER_ERR_INVALID_SWITCH,                          /**< invalid switch body */
  PARSER_ERR_INVALID_BREAK,                           /**< break must be inside a loop or switch */
  PARSER_ERR_INVALID_BREAK_LABEL,                     /**< break target not found */
  PARSER_ERR_INVALID_CONTINUE,                        /**< continue must be inside a loop */
  PARSER_ERR_INVALID_CONTINUE_LABEL,                  /**< continue target not found */
  PARSER_ERR_INVALID_RETURN,                          /**< return must be inside a function */
  PARSER_ERR_INVALID_RIGHT_SQUARE,                    /**< right square must terminate a block */
  PARSER_ERR_DUPLICATED_LABEL,                        /**< duplicated label */
  PARSER_ERR_OBJECT_PROPERTY_REDEFINED,               /**< property of object literal redefined */
#if JERRY_ESNEXT
  PARSER_ERR_VARIABLE_REDECLARED,                     /**< a variable redeclared */
  PARSER_ERR_LEXICAL_SINGLE_STATEMENT,                /**< lexical declaration in single statement context */
  PARSER_ERR_LABELLED_FUNC_NOT_IN_BLOCK,              /**< labelled functions are only allowed inside blocks */
  PARSER_ERR_LEXICAL_LET_BINDING,                     /**< let binding cannot be declared in let/const */
  PARSER_ERR_MISSING_ASSIGN_AFTER_CONST,              /**< an assignment is required after a const declaration */

  PARSER_ERR_MULTIPLE_CLASS_CONSTRUCTORS,             /**< multiple class constructor */
  PARSER_ERR_CLASS_CONSTRUCTOR_AS_ACCESSOR,           /**< class constructor cannot be an accessor */
  PARSER_ERR_INVALID_CLASS_CONSTRUCTOR,               /**< class constructor cannot be a generator or async function */
  PARSER_ERR_CLASS_STATIC_PROTOTYPE,                  /**< static method name 'prototype' is not allowed */
  PARSER_ERR_UNEXPECTED_SUPER_KEYWORD,                /**< unexpected super keyword */
  PARSER_ERR_TOO_MANY_CLASS_FIELDS,                   /**< too many computed class fields */
  PARSER_ERR_ARGUMENTS_IN_CLASS_FIELD,                /**< arguments is not allowed in class fields */

  PARSER_ERR_RIGHT_BRACE_EXPECTED,                    /**< right brace expected */
  PARSER_ERR_OF_EXPECTED,                             /**< of keyword expected */

  PARSER_ERR_ASSIGNMENT_EXPECTED,                     /**< assignment expression expected */
  PARSER_ERR_FORMAL_PARAM_AFTER_REST_PARAMETER,       /**< formal parameter after rest parameter */
  PARSER_ERR_SETTER_REST_PARAMETER,                   /**< setter rest parameter */
  PARSER_ERR_REST_PARAMETER_DEFAULT_INITIALIZER,      /**< rest parameter default initializer */
  PARSER_ERR_DUPLICATED_ARGUMENT_NAMES,               /**< duplicated argument names */
  PARSER_ERR_INVALID_DESTRUCTURING_PATTERN,           /**< invalid destructuring pattern */
  PARSER_ERR_ILLEGAL_PROPERTY_IN_DECLARATION,         /**< illegal property in declaration context */
  PARSER_ERR_INVALID_EXPONENTIATION,                  /**< left operand of ** operator cannot be unary expression */
  PARSER_ERR_INVALID_NULLISH_COALESCING,              /**< Cannot chain nullish with logical AND or OR. */
  PARSER_ERR_NEW_TARGET_EXPECTED,                     /**< expected new.target expression */
  PARSER_ERR_NEW_TARGET_NOT_ALLOWED,                  /**< new.target is not allowed in the given context */
#endif /* JERRY_ESNEXT */
#if JERRY_MODULE_SYSTEM
  PARSER_ERR_FILE_NOT_FOUND,                          /**< file not found*/
  PARSER_ERR_FROM_EXPECTED,                           /**< from expected */
  PARSER_ERR_FROM_COMMA_EXPECTED,                     /**< from or comma expected */
  PARSER_ERR_AS_EXPECTED,                             /**< as expected */
  PARSER_ERR_STRING_EXPECTED,                         /**< string literal expected */
  PARSER_ERR_MODULE_UNEXPECTED,                       /**< unexpected import or export statement */
  PARSER_ERR_LEFT_BRACE_MULTIPLY_LITERAL_EXPECTED,    /**< left brace or multiply or literal expected */
  PARSER_ERR_LEFT_BRACE_MULTIPLY_EXPECTED,            /**< left brace or multiply expected */
  PARSER_ERR_RIGHT_BRACE_COMMA_EXPECTED,              /**< right brace or comma expected */
  PARSER_ERR_DUPLICATED_EXPORT_IDENTIFIER,            /**< duplicated export identifier name */
  PARSER_ERR_DUPLICATED_IMPORT_BINDING,               /**< duplicated import binding name */
  PARSER_ERR_EXPORT_NOT_DEFINED,                      /**< export is not defined in module */
  PARSER_ERR_IMPORT_AFTER_NEW,                        /**< module import call is not allowed after new */
#endif /* JERRY_MODULE_SYSTEM */

  PARSER_ERR_NON_STRICT_ARG_DEFINITION                /**< non-strict argument definition */
} parser_error_t;

/**
 * Source code line counter type.
 */
typedef uint32_t parser_line_counter_t;

/* Note: source must be a valid UTF-8 string */
ecma_compiled_code_t *
parser_parse_script (const uint8_t *arg_list_p, size_t arg_list_size,
                     const uint8_t *source_p, size_t source_size,
                     uint32_t parse_opts, const ecma_parse_options_t *options_p);

#if JERRY_ERROR_MESSAGES
const char *parser_error_to_string (parser_error_t);
#endif /* JERRY_ERROR_MESSAGES */

/**
 * @}
 * @}
 * @}
 */

#endif /* !JS_PARSER_H */

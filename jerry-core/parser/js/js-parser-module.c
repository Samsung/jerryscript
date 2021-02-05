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

#if JERRY_MODULE_SYSTEM
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-lex-env.h"
#include "ecma-module.h"
#include "jcontext.h"
#include "jerryscript-port.h"

/**
 * Description of "*default*" literal string.
 */
const lexer_lit_location_t lexer_default_literal = { (const uint8_t *) "*default*", 9, LEXER_IDENT_LITERAL, false };

/**
 * Check for duplicated imported binding names.
 *
 * @return true - if the given name is a duplicate
 *         false - otherwise
 */
bool
parser_module_check_duplicate_import (parser_context_t *context_p, /**< parser context */
                                      ecma_string_t *local_name_p) /**< newly imported name */
{
  ecma_module_names_t *module_names_p = context_p->module_current_node_p->module_names_p;
  while (module_names_p != NULL)
  {
    if (ecma_compare_ecma_strings (module_names_p->local_name_p, local_name_p))
    {
      return true;
    }

    module_names_p = module_names_p->next_p;
  }

  ecma_module_node_t *module_node_p = JERRY_CONTEXT (module_current_p)->imports_p;
  while (module_node_p != NULL)
  {
    module_names_p = module_node_p->module_names_p;

    while (module_names_p != NULL)
    {
      if (ecma_compare_ecma_strings (module_names_p->local_name_p, local_name_p))
      {
        return true;
      }

      module_names_p = module_names_p->next_p;
    }

    module_node_p = module_node_p->next_p;
  }

  return false;
} /* parser_module_check_duplicate_import */

/**
 * Append an identifier to the exported bindings.
 */
void
parser_module_append_export_name (parser_context_t *context_p) /**< parser context */
{
  if (!(context_p->status_flags & PARSER_MODULE_STORE_IDENT))
  {
    return;
  }

  context_p->module_identifier_lit_p = context_p->lit_object.literal_p;

  ecma_string_t *name_p = ecma_new_ecma_string_from_utf8 (context_p->lit_object.literal_p->u.char_p,
                                                          context_p->lit_object.literal_p->prop.length);

  if (parser_module_check_duplicate_export (context_p, name_p))
  {
    ecma_deref_ecma_string (name_p);
    parser_raise_error (context_p, PARSER_ERR_DUPLICATED_EXPORT_IDENTIFIER);
  }

  parser_module_add_names_to_node (context_p, name_p, name_p);
  ecma_deref_ecma_string (name_p);
} /* parser_module_append_export_name */

/**
 * Check for duplicated exported bindings.
 * @return - true - if the exported name is a duplicate
 *           false - otherwise
 */
bool
parser_module_check_duplicate_export (parser_context_t *context_p, /**< parser context */
                                      ecma_string_t *export_name_p) /**< exported identifier */
{
  /* We have to check in the currently constructed node, as well as all of the already added nodes. */
  ecma_module_names_t *current_names_p = context_p->module_current_node_p->module_names_p;
  while (current_names_p != NULL)
  {
    if (ecma_compare_ecma_strings (current_names_p->imex_name_p, export_name_p))
    {
      return true;
    }
    current_names_p = current_names_p->next_p;
  }

  ecma_module_node_t *export_node_p = JERRY_CONTEXT (module_current_p)->local_exports_p;
  if (export_node_p != NULL)
  {
    JERRY_ASSERT (export_node_p->next_p == NULL);
    ecma_module_names_t *name_p = export_node_p->module_names_p;

    while (name_p != NULL)
    {
      if (ecma_compare_ecma_strings (name_p->imex_name_p, export_name_p))
      {
        return true;
      }

      name_p = name_p->next_p;
    }
  }

  export_node_p = JERRY_CONTEXT (module_current_p)->indirect_exports_p;
  while (export_node_p != NULL)
  {
    ecma_module_names_t *name_p = export_node_p->module_names_p;

    while (name_p != NULL)
    {
      if (ecma_compare_ecma_strings (name_p->imex_name_p, export_name_p))
      {
        return true;
      }

      name_p = name_p->next_p;
    }

    export_node_p = export_node_p->next_p;
  }

  /* Star exports don't have any names associated with them, so no need to check those. */
  return false;
} /* parser_module_check_duplicate_export */

/**
 * Add export node to parser context.
 */
void
parser_module_finalize_export_node (parser_context_t *context_p) /**< parser context */
{
  ecma_module_node_t *module_node_p = context_p->module_current_node_p;
  context_p->module_current_node_p = NULL;
  ecma_module_node_t **export_list_p;

  /* Check which list we should add it to. */
  if (module_node_p->module_request_p)
  {
    /* If the export node has a module request, that means it's either an indirect export, or a star export. */
    if (!module_node_p->module_names_p)
    {
      /* If there are no names in the node, then it's a star export. */
      export_list_p = &(JERRY_CONTEXT (module_current_p)->star_exports_p);
    }
    else
    {
      export_list_p = &(JERRY_CONTEXT (module_current_p)->indirect_exports_p);
    }
  }
  else
  {
    /* If there is no module request, then it's a local export. */
    export_list_p = &(JERRY_CONTEXT (module_current_p)->local_exports_p);
  }

  /* Check if we have a node with the same module request, append to it if we do. */
  ecma_module_node_t *stored_exports_p = *export_list_p;
  while (stored_exports_p != NULL)
  {
    if (stored_exports_p->module_request_p == module_node_p->module_request_p)
    {
      ecma_module_names_t *module_names_p = module_node_p->module_names_p;

      if (module_names_p != NULL)
      {
        while (module_names_p->next_p != NULL)
        {
          module_names_p = module_names_p->next_p;
        }

        module_names_p->next_p = stored_exports_p->module_names_p;
        stored_exports_p->module_names_p = module_node_p->module_names_p;
        module_node_p->module_names_p = NULL;
      }

      jmem_heap_free_block (module_node_p, sizeof (ecma_module_node_t));
      return;
    }

    stored_exports_p = stored_exports_p->next_p;
  }

  module_node_p->next_p = *export_list_p;
  *export_list_p = module_node_p;
} /* parser_module_finalize_export_node */

/**
 * Add import node to parser context.
 */
void
parser_module_finalize_import_node (parser_context_t *context_p) /**< parser context */
{
  ecma_module_node_t *module_node_p = context_p->module_current_node_p;
  context_p->module_current_node_p = NULL;
  ecma_module_node_t *stored_imports_p = JERRY_CONTEXT (module_current_p)->imports_p;

  /* Check if we have a node with the same module request, append to it if we do. */
  while (stored_imports_p != NULL)
  {
    if (stored_imports_p->module_request_p == module_node_p->module_request_p)
    {
      ecma_module_names_t *module_names_p = module_node_p->module_names_p;

      if (module_names_p != NULL)
      {
        while (module_names_p->next_p != NULL)
        {
          module_names_p = module_names_p->next_p;
        }

        module_names_p->next_p = stored_imports_p->module_names_p;
        stored_imports_p->module_names_p = module_node_p->module_names_p;
        module_node_p->module_names_p = NULL;
      }

      jmem_heap_free_block (module_node_p, sizeof (ecma_module_node_t));
      return;
    }

    stored_imports_p = stored_imports_p->next_p;
  }

  module_node_p->next_p = JERRY_CONTEXT (module_current_p)->imports_p;
  JERRY_CONTEXT (module_current_p)->imports_p = module_node_p;
} /* parser_module_finalize_import_node */

/**
 * Add module names to current module node.
 */
void
parser_module_add_names_to_node (parser_context_t *context_p, /**< parser context */
                                 ecma_string_t *imex_name_p, /**< import/export name */
                                 ecma_string_t *local_name_p) /**< local name */
{
  ecma_module_names_t *new_names_p = (ecma_module_names_t *) parser_malloc (context_p, sizeof (ecma_module_names_t));
  memset (new_names_p, 0, sizeof (ecma_module_names_t));

  ecma_module_node_t *module_node_p = context_p->module_current_node_p;
  new_names_p->next_p = module_node_p->module_names_p;
  module_node_p->module_names_p = new_names_p;

  JERRY_ASSERT (imex_name_p != NULL);
  ecma_ref_ecma_string (imex_name_p);
  new_names_p->imex_name_p = imex_name_p;

  JERRY_ASSERT (local_name_p != NULL);
  ecma_ref_ecma_string (local_name_p);
  new_names_p->local_name_p = local_name_p;
} /* parser_module_add_names_to_node */

/**
 * Create a permanent import/export node from a template node.
 * @return - the copy of the template if the second parameter is not NULL.
 *         - otherwise: an empty node.
 */
ecma_module_node_t *
parser_module_create_module_node (parser_context_t *context_p) /**< parser context */
{
  ecma_module_node_t *node_p = (ecma_module_node_t *) parser_malloc (context_p, sizeof (ecma_module_node_t));
  memset (node_p, 0, sizeof (ecma_module_node_t));

  return node_p;
} /* parser_module_create_module_node */

/**
 * Parse an ExportClause.
 */
void
parser_module_parse_export_clause (parser_context_t *context_p) /**< parser context */
{
  bool has_module_specifier = false;

  if (context_p->source_p == context_p->next_scanner_info_p->source_p)
  {
    has_module_specifier = true;
    JERRY_ASSERT (context_p->next_scanner_info_p->type == SCANNER_TYPE_EXPORT_MODULE_SPECIFIER);
    scanner_release_next (context_p, sizeof (scanner_info_t));
  }

  JERRY_ASSERT (context_p->token.type == LEXER_LEFT_BRACE);
  lexer_next_token (context_p);

  while (true)
  {
    if (context_p->token.type == LEXER_RIGHT_BRACE)
    {
      lexer_next_token (context_p);
      break;
    }

    /* 15.2.3.1 The referenced binding cannot be a reserved word. */
    if (context_p->token.type != LEXER_LITERAL || context_p->token.lit_location.type != LEXER_IDENT_LITERAL
        || context_p->token.keyword_type >= LEXER_FIRST_FUTURE_STRICT_RESERVED_WORD)
    {
      parser_raise_error (context_p, PARSER_ERR_IDENTIFIER_EXPECTED);
    }

    ecma_string_t *export_name_p = NULL;
    ecma_string_t *local_name_p = NULL;

    lexer_construct_literal_object (context_p, &context_p->token.lit_location, LEXER_NEW_IDENT_LITERAL);

    if (!has_module_specifier && !scanner_literal_exists (context_p, context_p->lit_object.index))
    {
      parser_raise_error (context_p, PARSER_ERR_EXPORT_NOT_DEFINED);
    }

    uint16_t local_name_index = context_p->lit_object.index;
    uint16_t export_name_index = PARSER_MAXIMUM_NUMBER_OF_LITERALS;

    lexer_next_token (context_p);
    if (lexer_token_is_identifier (context_p, "as", 2))
    {
      lexer_next_token (context_p);

      if (context_p->token.type != LEXER_LITERAL || context_p->token.lit_location.type != LEXER_IDENT_LITERAL)
      {
        parser_raise_error (context_p, PARSER_ERR_IDENTIFIER_EXPECTED);
      }

      lexer_construct_literal_object (context_p, &context_p->token.lit_location, LEXER_NEW_IDENT_LITERAL);

      export_name_index = context_p->lit_object.index;

      lexer_next_token (context_p);
    }

    lexer_literal_t *literal_p = PARSER_GET_LITERAL (local_name_index);
    local_name_p = ecma_new_ecma_string_from_utf8 (literal_p->u.char_p, literal_p->prop.length);

    if (export_name_index != PARSER_MAXIMUM_NUMBER_OF_LITERALS)
    {
      lexer_literal_t *as_literal_p = PARSER_GET_LITERAL (export_name_index);
      export_name_p = ecma_new_ecma_string_from_utf8 (as_literal_p->u.char_p, as_literal_p->prop.length);
    }
    else
    {
      export_name_p = local_name_p;
      ecma_ref_ecma_string (local_name_p);
    }

    if (parser_module_check_duplicate_export (context_p, export_name_p))
    {
      ecma_deref_ecma_string (local_name_p);
      ecma_deref_ecma_string (export_name_p);
      parser_raise_error (context_p, PARSER_ERR_DUPLICATED_EXPORT_IDENTIFIER);
    }

    parser_module_add_names_to_node (context_p, export_name_p, local_name_p);
    ecma_deref_ecma_string (local_name_p);
    ecma_deref_ecma_string (export_name_p);

    if (context_p->token.type != LEXER_COMMA && context_p->token.type != LEXER_RIGHT_BRACE)
    {
      parser_raise_error (context_p, PARSER_ERR_RIGHT_BRACE_COMMA_EXPECTED);
    }
    else if (context_p->token.type == LEXER_COMMA)
    {
      lexer_next_token (context_p);
    }

    if (lexer_token_is_identifier (context_p, "from", 4))
    {
      parser_raise_error (context_p, PARSER_ERR_RIGHT_BRACE_EXPECTED);
    }
  }
} /* parser_module_parse_export_clause */

/**
 * Parse an ImportClause
 */
void
parser_module_parse_import_clause (parser_context_t *context_p) /**< parser context */
{
  JERRY_ASSERT (context_p->token.type == LEXER_LEFT_BRACE);
  lexer_next_token (context_p);

  while (true)
  {
    if (context_p->token.type == LEXER_RIGHT_BRACE)
    {
      lexer_next_token (context_p);
      break;
    }

    if (context_p->token.type != LEXER_LITERAL || context_p->token.lit_location.type != LEXER_IDENT_LITERAL)
    {
      parser_raise_error (context_p, PARSER_ERR_IDENTIFIER_EXPECTED);
    }

    if (context_p->next_scanner_info_p->source_p == context_p->source_p)
    {
      JERRY_ASSERT (context_p->next_scanner_info_p->type == SCANNER_TYPE_ERR_REDECLARED);
      parser_raise_error (context_p, PARSER_ERR_VARIABLE_REDECLARED);
    }

    ecma_string_t *import_name_p = NULL;
    ecma_string_t *local_name_p = NULL;

    lexer_construct_literal_object (context_p, &context_p->token.lit_location, LEXER_NEW_IDENT_LITERAL);

    uint16_t import_name_index = context_p->lit_object.index;
    uint16_t local_name_index = PARSER_MAXIMUM_NUMBER_OF_LITERALS;

    lexer_next_token (context_p);
    if (lexer_token_is_identifier (context_p, "as", 2))
    {
      lexer_next_token (context_p);

      if (context_p->token.type != LEXER_LITERAL || context_p->token.lit_location.type != LEXER_IDENT_LITERAL)
      {
        parser_raise_error (context_p, PARSER_ERR_IDENTIFIER_EXPECTED);
      }

      if (context_p->next_scanner_info_p->source_p == context_p->source_p)
      {
        JERRY_ASSERT (context_p->next_scanner_info_p->type == SCANNER_TYPE_ERR_REDECLARED);
        parser_raise_error (context_p, PARSER_ERR_VARIABLE_REDECLARED);
      }

      lexer_construct_literal_object (context_p, &context_p->token.lit_location, LEXER_NEW_IDENT_LITERAL);

      local_name_index = context_p->lit_object.index;

      lexer_next_token (context_p);
    }

    lexer_literal_t *literal_p = PARSER_GET_LITERAL (import_name_index);
    import_name_p = ecma_new_ecma_string_from_utf8 (literal_p->u.char_p, literal_p->prop.length);

    if (local_name_index != PARSER_MAXIMUM_NUMBER_OF_LITERALS)
    {
      lexer_literal_t *as_literal_p = PARSER_GET_LITERAL (local_name_index);
      local_name_p = ecma_new_ecma_string_from_utf8 (as_literal_p->u.char_p, as_literal_p->prop.length);
    }
    else
    {
      local_name_p = import_name_p;
      ecma_ref_ecma_string (local_name_p);
    }

    if (parser_module_check_duplicate_import (context_p, local_name_p))
    {
      ecma_deref_ecma_string (local_name_p);
      ecma_deref_ecma_string (import_name_p);
      parser_raise_error (context_p, PARSER_ERR_DUPLICATED_IMPORT_BINDING);
    }

    parser_module_add_names_to_node (context_p, import_name_p, local_name_p);
    ecma_deref_ecma_string (local_name_p);
    ecma_deref_ecma_string (import_name_p);

    if (context_p->token.type != LEXER_COMMA && (context_p->token.type != LEXER_RIGHT_BRACE))
    {
      parser_raise_error (context_p, PARSER_ERR_RIGHT_BRACE_COMMA_EXPECTED);
    }
    else if (context_p->token.type == LEXER_COMMA)
    {
      lexer_next_token (context_p);
    }

    if (lexer_token_is_identifier (context_p, "from", 4))
    {
      parser_raise_error (context_p, PARSER_ERR_RIGHT_BRACE_EXPECTED);
    }
  }
} /* parser_module_parse_import_clause */

/**
 * Raises parser error if the import or export statement is not in the global scope.
 */
void
parser_module_check_request_place (parser_context_t *context_p) /**< parser context */
{
  if (context_p->last_context_p != NULL || context_p->stack_top_uint8 != 0
      || (context_p->status_flags & PARSER_IS_FUNCTION) || (context_p->global_status_flags & ECMA_PARSE_EVAL)
      || (context_p->global_status_flags & ECMA_PARSE_MODULE) == 0)
  {
    parser_raise_error (context_p, PARSER_ERR_MODULE_UNEXPECTED);
  }
} /* parser_module_check_request_place */

/**
 * Handle module specifier at the end of the import / export statement.
 */
void
parser_module_handle_module_specifier (parser_context_t *context_p) /**< parser context */
{
  ecma_module_node_t *module_node_p = context_p->module_current_node_p;
  if (context_p->token.type != LEXER_LITERAL || context_p->token.lit_location.type != LEXER_STRING_LITERAL
      || context_p->token.lit_location.length == 0)
  {
    parser_raise_error (context_p, PARSER_ERR_STRING_EXPECTED);
  }

  lexer_construct_literal_object (context_p, &context_p->token.lit_location, LEXER_STRING_LITERAL);

  ecma_string_t *name_p = ecma_new_ecma_string_from_utf8 (context_p->lit_object.literal_p->u.char_p,
                                                          context_p->lit_object.literal_p->prop.length);

  ecma_module_t *module_p = ecma_module_find_native_module (name_p);

  if (module_p)
  {
    ecma_deref_ecma_string (name_p);
    goto module_found;
  }

  ecma_deref_ecma_string (name_p);
  ecma_string_t *path_p = ecma_module_create_normalized_path (context_p->lit_object.literal_p->u.char_p,
                                                              context_p->lit_object.literal_p->prop.length,
                                                              JERRY_CONTEXT (module_current_p)->path_p);

  if (path_p == NULL)
  {
    parser_raise_error (context_p, PARSER_ERR_FILE_NOT_FOUND);
  }

  module_p = ecma_module_find_module (path_p);

module_found:
  module_node_p->module_request_p = module_p;
  lexer_next_token (context_p);
} /* parser_module_handle_module_specifier */

#endif /* JERRY_MODULE_SYSTEM */

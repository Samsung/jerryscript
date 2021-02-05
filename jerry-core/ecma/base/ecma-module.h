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

#ifndef ECMA_MODULE_H
#define ECMA_MODULE_H

#include "common.h"
#include "ecma-globals.h"

#if JERRY_MODULE_SYSTEM

#define ECMA_MODULE_MAX_PATH 255u

/**
 * Imported or exported names, such as "a as b"
 * Note: See https://www.ecma-international.org/ecma-262/6.0/#table-39
 *       and https://www.ecma-international.org/ecma-262/6.0/#table-41
 */
typedef struct ecma_module_names
{
  struct ecma_module_names *next_p; /**< next linked list node */
  ecma_string_t *imex_name_p; /**< Import/export name of the item */
  ecma_string_t *local_name_p; /**< Local name of the item */
} ecma_module_names_t;

/**
 * An enum identifing the current state of the module
 */
typedef enum
{
  ECMA_MODULE_STATE_INIT = 0, /**< module is initialized */
  ECMA_MODULE_STATE_PARSING = 1, /**< module is currently being parsed */
  ECMA_MODULE_STATE_PARSED = 2, /**< module has been parsed */
  ECMA_MODULE_STATE_EVALUATING = 3, /**< module is currently being evaluated */
  ECMA_MODULE_STATE_EVALUATED = 4, /**< module has been evaluated */
  ECMA_MODULE_STATE_NATIVE = 5, /**< module is native */
  ECMA_MODULE_STATE_ROOT = 6, /**< module is a root module */
} ecma_module_state_t;

/**
 * Module structure storing an instance of a module
 */
typedef struct ecma_module
{
  /* TODO(dbatyai): These could be compressed pointers */
  struct ecma_module *next_p; /**< next module in the list */
  struct ecma_module_node *imports_p; /**< import requests of the module */
  struct ecma_module_node *local_exports_p; /**< local exports of the module */
  struct ecma_module_node *indirect_exports_p; /**< indirect exports of the module */
  struct ecma_module_node *star_exports_p; /**< star exports of the module*/
  ecma_string_t *path_p; /**< path of the module */
  ecma_compiled_code_t *compiled_code_p; /**< compiled code for the module */
  ecma_object_t *scope_p; /**< lexical lenvironment of the module */
  ecma_object_t *namespace_object_p; /**< namespace object of the module */
  ecma_module_state_t state; /**< evaluation state of the module */
} ecma_module_t;

/**
 * Module node to store imports / exports.
 */
typedef struct ecma_module_node
{
  struct ecma_module_node *next_p; /**< next linked list node */
  ecma_module_names_t *module_names_p; /**< names of the requested import/export node */
  ecma_module_t *module_request_p; /**< module structure of the requested module */
} ecma_module_node_t;

/**
 *  A record that can be used to store {module, identifier} pairs
 */
typedef struct ecma_module_record
{
  ecma_module_t *module_p; /**< module */
  ecma_string_t *name_p; /**< identifier name */
} ecma_module_record_t;

/**
 *  A list of module records that can be used to identify circular imports during resolution
 */
typedef struct ecma_module_resolve_set
{
  struct ecma_module_resolve_set *next_p; /**< next in linked list */
  ecma_module_record_t record; /**< module record */
} ecma_module_resolve_set_t;

/**
 * A list that is used like a stack to drive the resolution process, instead of recursion.
 */
typedef struct ecma_module_resolve_stack
{
  struct ecma_module_resolve_stack *next_p; /**< next in linked list */
  ecma_module_t *module_p; /**< module request */
  ecma_string_t *export_name_p; /**< export identifier name */
  bool resolving; /**< flag storing wether the current frame started resolving */
} ecma_module_resolve_stack_t;

bool ecma_module_resolve_set_insert (ecma_module_resolve_set_t **set_p,
                                     ecma_module_t *const module_p,
                                     ecma_string_t *const export_name_p);
void ecma_module_resolve_set_cleanup (ecma_module_resolve_set_t *set_p);

void ecma_module_resolve_stack_push (ecma_module_resolve_stack_t **stack_p,
                                     ecma_module_t *const module_p,
                                     ecma_string_t *const export_name_p);
void ecma_module_resolve_stack_pop (ecma_module_resolve_stack_t **stack_p);

ecma_string_t *ecma_module_create_normalized_path (const lit_utf8_byte_t *char_p,
                                                   lit_utf8_size_t size,
                                                   ecma_string_t *const base_path_p);

ecma_module_t *ecma_module_find_module (ecma_string_t *const path_p);
ecma_module_t *ecma_module_find_native_module (ecma_string_t *const path_p);

ecma_value_t ecma_module_parse_referenced_modules (void);
ecma_value_t ecma_module_initialize (ecma_module_t *module_p);

void ecma_module_initialize_context (ecma_string_t *root_path_p);
void ecma_module_cleanup_context (void);

void ecma_module_release_module_nodes (ecma_module_node_t *module_node_p);
void ecma_module_cleanup (ecma_module_t *head_p);
#endif /* JERRY_MODULE_SYSTEM */

#endif /* !ECMA_MODULE_H */

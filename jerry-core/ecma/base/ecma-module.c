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

#include "jcontext.h"

#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-lex-env.h"
#include "ecma-module.h"
#include "ecma-objects.h"
#include "lit-char-helpers.h"
#include "vm.h"

#if JERRY_MODULE_SYSTEM

/**
 * Initialize context variables for the root module.
 *
 * @return new module
 */
ecma_module_t *
ecma_module_create (void)
{
  JERRY_ASSERT (JERRY_CONTEXT (module_current_p) == NULL);

  ecma_object_t *obj_p = ecma_create_object (NULL, sizeof (ecma_module_t), ECMA_OBJECT_TYPE_CLASS);

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) obj_p;
  ext_object_p->u.cls.type = ECMA_OBJECT_CLASS_MODULE;
  ext_object_p->u.cls.u1.module_state = JERRY_MODULE_STATE_UNLINKED;
  ext_object_p->u.cls.u2.module_flags = 0;

  ecma_module_t *module_p = (ecma_module_t *) obj_p;

  module_p->scope_p = NULL;
  module_p->namespace_object_p = NULL;
  module_p->imports_p = NULL;
  module_p->local_exports_p = NULL;
  module_p->indirect_exports_p = NULL;
  module_p->star_exports_p = NULL;
  module_p->u.compiled_code_p = NULL;

  return module_p;
} /* ecma_module_create */

/**
 * Cleanup context variables for the root module.
 */
void
ecma_module_cleanup_context (void)
{
  ecma_deref_object ((ecma_object_t *) JERRY_CONTEXT (module_current_p));
#ifndef JERRY_NDEBUG
  JERRY_CONTEXT (module_current_p) = NULL;
#endif /* JERRY_NDEBUG */
} /* ecma_module_cleanup_context */

/**
 * Sets module state to error.
 */
static void
ecma_module_set_error_state (ecma_module_t *module_p) /**< module */
{
  module_p->header.u.cls.u1.module_state = JERRY_MODULE_STATE_ERROR;

  if (JERRY_CONTEXT (module_state_changed_callback_p) != NULL
      && !jcontext_has_pending_abort ())
  {
    jerry_value_t exception = jcontext_take_exception ();

    JERRY_CONTEXT (module_state_changed_callback_p) (JERRY_MODULE_STATE_ERROR,
                                                     ecma_make_object_value (&module_p->header.object),
                                                     exception,
                                                     JERRY_CONTEXT (module_state_changed_callback_user_p));
    jcontext_raise_exception (exception);
  }
} /* ecma_module_set_error_state */

/**
 * Gets the internal module pointer of a module
 *
 * @return module pointer
 */
static inline ecma_module_t * JERRY_ATTR_ALWAYS_INLINE
ecma_module_get_from_object (ecma_value_t module_val) /**< module */
{
  JERRY_ASSERT (ecma_is_value_object (module_val));

  ecma_object_t *object_p = ecma_get_object_from_value (module_val);

  JERRY_ASSERT (ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_CLASS);
  JERRY_ASSERT (((ecma_extended_object_t *) object_p)->u.cls.type == ECMA_OBJECT_CLASS_MODULE);

  return (ecma_module_t *) object_p;
} /* ecma_module_get_from_object */

/**
 *  Inserts a {module, export_name} record into a resolve set.
 *  Note: See 15.2.1.16.3 - resolveSet and exportStarSet
 *
 *  @return true - if the set already contains the record
 *          false - otherwise
 */
static bool
ecma_module_resolve_set_insert (ecma_module_resolve_set_t **set_p, /**< [in, out] resolve set */
                                ecma_module_t *const module_p, /**< module */
                                ecma_string_t *const export_name_p) /**< export name */
{
  JERRY_ASSERT (set_p != NULL);
  ecma_module_resolve_set_t *current_p = *set_p;

  while (current_p != NULL)
  {
    if (current_p->record.module_p == module_p
        && ecma_compare_ecma_strings (current_p->record.name_p, export_name_p))
    {
      return false;
    }

    current_p = current_p->next_p;
  }

  ecma_module_resolve_set_t *new_p;
  new_p = (ecma_module_resolve_set_t *) jmem_heap_alloc_block (sizeof (ecma_module_resolve_set_t));

  new_p->next_p = *set_p;
  new_p->record.module_p = module_p;
  ecma_ref_ecma_string (export_name_p);
  new_p->record.name_p = export_name_p;

  *set_p = new_p;
  return true;
} /* ecma_module_resolve_set_insert */

/**
 * Cleans up contents of a resolve set.
 */
static void
ecma_module_resolve_set_cleanup (ecma_module_resolve_set_t *set_p) /**< resolve set */
{
  while (set_p != NULL)
  {
    ecma_module_resolve_set_t *next_p = set_p->next_p;
    ecma_deref_ecma_string (set_p->record.name_p);
    jmem_heap_free_block (set_p, sizeof (ecma_module_resolve_set_t));
    set_p = next_p;
  }
} /* ecma_module_resolve_set_cleanup */

/**
 * Pushes a new resolve frame on top of a resolve stack and initializes it
 * to begin resolving the specified exported name in the base module.
 */
static void
ecma_module_resolve_stack_push (ecma_module_resolve_stack_t **stack_p, /**< [in, out] resolve stack */
                                ecma_module_t * const module_p, /**< base module */
                                ecma_string_t * const export_name_p) /**< exported name */
{
  JERRY_ASSERT (stack_p != NULL);
  ecma_module_resolve_stack_t *new_frame_p;
  new_frame_p = (ecma_module_resolve_stack_t *) jmem_heap_alloc_block (sizeof (ecma_module_resolve_stack_t));

  ecma_ref_ecma_string (export_name_p);
  new_frame_p->export_name_p = export_name_p;
  new_frame_p->module_p = module_p;
  new_frame_p->resolving = false;

  new_frame_p->next_p = *stack_p;
  *stack_p = new_frame_p;
} /* ecma_module_resolve_stack_push */

/**
 * Pops the topmost frame from a resolve stack.
 */
static void
ecma_module_resolve_stack_pop (ecma_module_resolve_stack_t **stack_p) /**< [in, out] resolve stack */
{
  JERRY_ASSERT (stack_p != NULL);
  ecma_module_resolve_stack_t *current_p = *stack_p;

  if (current_p != NULL)
  {
    *stack_p = current_p->next_p;
    ecma_deref_ecma_string (current_p->export_name_p);
    jmem_heap_free_block (current_p, sizeof (ecma_module_resolve_stack_t));
  }
} /* ecma_module_resolve_stack_pop */

/**
 * Resolves which module satisfies an export based from a specific module in the import tree.
 * If no error occurs, out_record_p will contain a {module, local_name} record, which satisfies
 * the export, or {NULL, NULL} if the export is ambiguous.
 * Note: See 15.2.1.16.3
 *
 * @return ECMA_VALUE_ERROR - if an error occured
 *         ECMA_VALUE_EMPTY - otherwise
 */
static ecma_value_t
ecma_module_resolve_export (ecma_module_t *const module_p, /**< base module */
                            ecma_string_t *const export_name_p, /**< export name */
                            ecma_module_record_t *out_record_p) /**< [out] found module record */
{
  ecma_module_resolve_set_t *resolve_set_p = NULL;
  ecma_module_resolve_stack_t *stack_p = NULL;

  bool found = false;
  ecma_module_record_t found_record = { NULL, NULL };
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  ecma_module_resolve_stack_push (&stack_p, module_p, export_name_p);

  while (stack_p != NULL)
  {
    ecma_module_resolve_stack_t *current_frame_p = stack_p;

    ecma_module_t *current_module_p = current_frame_p->module_p;
    ecma_string_t *current_export_name_p = current_frame_p->export_name_p;

    if (!current_frame_p->resolving)
    {
      current_frame_p->resolving = true;

      /* 15.2.1.16.3 / 2-3 */
      if (!ecma_module_resolve_set_insert (&resolve_set_p, current_module_p, current_export_name_p))
      {
        /* This is a circular import request. */
        ecma_module_resolve_stack_pop (&stack_p);
        continue;
      }

      if (current_module_p->local_exports_p != NULL)
      {
        /* 15.2.1.16.3 / 4 */
        ecma_module_names_t *export_names_p = current_module_p->local_exports_p;
        while (export_names_p != NULL)
        {
          if (ecma_compare_ecma_strings (current_export_name_p, export_names_p->imex_name_p))
          {
            if (found)
            {
              /* This is an ambigous export. */
              found_record.module_p = NULL;
              found_record.name_p = NULL;
              break;
            }

            /* The current module provides a direct binding for this export. */
            found = true;
            found_record.module_p = current_module_p;
            found_record.name_p = export_names_p->local_name_p;
            break;
          }

          export_names_p = export_names_p->next_p;
        }
      }

      if (found)
      {
        /* We found a resolution for the current frame, return to the previous. */
        ecma_module_resolve_stack_pop (&stack_p);
        continue;
      }

      /* 15.2.1.16.3 / 5 */
      ecma_module_node_t *indirect_export_p = current_module_p->indirect_exports_p;
      while (indirect_export_p != NULL)
      {
        ecma_module_names_t *export_names_p = indirect_export_p->module_names_p;
        while (export_names_p != NULL)
        {
          if (ecma_compare_ecma_strings (current_export_name_p, export_names_p->imex_name_p))
          {
            /* 5.2.1.16.3 / 5.a.iv */
            ecma_module_resolve_stack_push (&stack_p,
                                            ecma_module_get_from_object (*indirect_export_p->u.module_object_p),
                                            export_names_p->local_name_p);
            break;
          }

          export_names_p = export_names_p->next_p;
        }

        indirect_export_p = indirect_export_p->next_p;
      }

      /* We need to check whether the newly pushed indirect exports resolve to anything.
       * Keep current frame in the stack, and continue from the topmost frame. */
      continue;
    } /* if (!current_frame_p->resolving) */

    /* By the time we return to the current frame, the indirect exports will have finished resolving. */
    if (found)
    {
      /* We found at least one export that satisfies the current request.
       * Pop current frame, and return to the previous. */
      ecma_module_resolve_stack_pop (&stack_p);
      continue;
    }

    /* 15.2.1.16.3 / 6 */
    if (ecma_compare_ecma_string_to_magic_id (current_export_name_p, LIT_MAGIC_STRING_DEFAULT))
    {
      ret_value = ecma_raise_syntax_error (ECMA_ERR_MSG ("No explicitly defined default export in module"));
      break;
    }

    /* 15.2.1.16.3 / 7-8 */
    if (!ecma_module_resolve_set_insert (&resolve_set_p,
                                         current_module_p,
                                         ecma_get_magic_string (LIT_MAGIC_STRING_ASTERIX_CHAR)))
    {
      /* This is a circular import request. */
      ecma_module_resolve_stack_pop (&stack_p);
      continue;
    }

    /* Pop the current frame, we have nothing else to do here after the star export resolutions are queued. */
    ecma_module_resolve_stack_pop (&stack_p);

    /* 15.2.1.16.3 / 10 */
    ecma_module_node_t *star_export_p = current_module_p->star_exports_p;
    while (star_export_p != NULL)
    {
      JERRY_ASSERT (star_export_p->module_names_p == NULL);

      /* 15.2.1.16.3 / 10.c */
      ecma_module_resolve_stack_push (&stack_p,
                                      ecma_module_get_from_object (*star_export_p->u.module_object_p),
                                      export_name_p);

      star_export_p = star_export_p->next_p;
    }
  }

  /* Clean up. */
  ecma_module_resolve_set_cleanup (resolve_set_p);
  while (stack_p)
  {
    ecma_module_resolve_stack_pop (&stack_p);
  }

  if (ECMA_IS_VALUE_ERROR (ret_value))
  {
    /* No default export was found */
    return ret_value;
  }

  if (found)
  {
    *out_record_p = found_record;
  }
  else
  {
    ret_value = ecma_raise_syntax_error (ECMA_ERR_MSG ("Unexported or circular import request"));
  }

  return ret_value;
} /* ecma_module_resolve_export */

/**
 * Evaluates an EcmaScript module.
 *
 * @return ECMA_VALUE_ERROR - if an error occured
 *         module script result - otherwise
 */
ecma_value_t
ecma_module_evaluate (ecma_module_t *module_p) /**< module */
{
  if (module_p->header.u.cls.u1.module_state == JERRY_MODULE_STATE_ERROR)
  {
    return ecma_raise_range_error (ECMA_ERR_MSG ("Module is in error state"));
  }

  if (module_p->header.u.cls.u1.module_state >= JERRY_MODULE_STATE_EVALUATING)
  {
    return ECMA_VALUE_EMPTY;
  }

  JERRY_ASSERT (module_p->header.u.cls.u1.module_state == JERRY_MODULE_STATE_LINKED);
  JERRY_ASSERT (module_p->scope_p != NULL);

  module_p->header.u.cls.u1.module_state = JERRY_MODULE_STATE_EVALUATING;

  ecma_value_t ret_value;

  if (module_p->header.u.cls.u2.module_flags & ECMA_MODULE_IS_NATIVE)
  {
    ret_value = ECMA_VALUE_UNDEFINED;

    if (module_p->u.callback)
    {
      ret_value = module_p->u.callback (ecma_make_object_value (&module_p->header.object));

      if (JERRY_UNLIKELY (ecma_is_value_error_reference (ret_value)))
      {
        ecma_raise_error_from_error_reference (ret_value);
        ret_value = ECMA_VALUE_ERROR;
      }
    }
  }
  else
  {
    ret_value = vm_run_module (module_p);
  }

  if (JERRY_LIKELY (!ECMA_IS_VALUE_ERROR (ret_value)))
  {
    module_p->header.u.cls.u1.module_state = JERRY_MODULE_STATE_EVALUATED;

    if (JERRY_CONTEXT (module_state_changed_callback_p) != NULL)
    {
      JERRY_CONTEXT (module_state_changed_callback_p) (JERRY_MODULE_STATE_EVALUATED,
                                                       ecma_make_object_value (&module_p->header.object),
                                                       ret_value,
                                                       JERRY_CONTEXT (module_state_changed_callback_user_p));
    }
  }
  else
  {
    ecma_module_set_error_state (module_p);
  }

  if (!(module_p->header.u.cls.u2.module_flags & ECMA_MODULE_IS_NATIVE))
  {
    ecma_bytecode_deref (module_p->u.compiled_code_p);
  }

  module_p->u.compiled_code_p = NULL;
  return ret_value;
} /* ecma_module_evaluate */

/**
 * Resolves an export and adds it to the modules namespace object, if the export name is not yet handled.
 * Note: See 15.2.1.16.2 and 15.2.1.18
 *
 * @return ECMA_VALUE_ERROR - if an error occured
 *         ECMA_VALUE_EMPTY - otherwise
 */
static ecma_value_t
ecma_module_namespace_object_add_export_if_needed (ecma_module_t *module_p, /**< module */
                                                   ecma_string_t *export_name_p) /**< export name */
{
  JERRY_ASSERT (module_p->namespace_object_p != NULL);
  ecma_value_t result = ECMA_VALUE_EMPTY;

  /* Default exports should not be added to the namespace object. */
  if (ecma_compare_ecma_string_to_magic_id (export_name_p, LIT_MAGIC_STRING_DEFAULT)
      || ecma_find_named_property (module_p->namespace_object_p, export_name_p) != NULL)
  {
    /* This export name has already been handled. */
    return result;
  }

  ecma_module_record_t record;
  result = ecma_module_resolve_export (module_p, export_name_p, &record);

  if (ECMA_IS_VALUE_ERROR (result))
  {
    return result;
  }

  if (record.module_p == NULL)
  {
    /* 15.2.1.18 / 3.d.iv Skip ambiguous names. */
    return result;
  }

  ecma_property_t *property_p = ecma_find_named_property (record.module_p->scope_p, record.name_p);

  ecma_create_named_reference_property (module_p->namespace_object_p,
                                        export_name_p,
                                        property_p);
  return result;
} /* ecma_module_namespace_object_add_export_if_needed */

/**
 * Creates a namespace object for a module.
 * Note: See 15.2.1.18
 *
 * @return ECMA_VALUE_ERROR - if an error occured
 *         ECMA_VALUE_EMPTY - otherwise
 */
ecma_value_t
ecma_module_create_namespace_object (ecma_module_t *module_p) /**< module */
{
  ecma_value_t result = ECMA_VALUE_EMPTY;
  if (module_p->namespace_object_p != NULL)
  {
    return result;
  }

  JERRY_ASSERT (module_p->header.u.cls.u1.module_state >= JERRY_MODULE_STATE_LINKED
                && module_p->header.u.cls.u1.module_state <= JERRY_MODULE_STATE_EVALUATED);
  ecma_module_resolve_set_t *resolve_set_p = NULL;
  ecma_module_resolve_stack_t *stack_p = NULL;

  ecma_object_t *namespace_object_p = ecma_create_object (NULL,
                                                          sizeof (ecma_extended_object_t),
                                                          ECMA_OBJECT_TYPE_CLASS);

  namespace_object_p->type_flags_refs &= (ecma_object_descriptor_t) ~ECMA_OBJECT_FLAG_EXTENSIBLE;

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) namespace_object_p;
  ext_object_p->u.cls.type = ECMA_OBJECT_CLASS_MODULE_NAMESPACE;
  ECMA_SET_INTERNAL_VALUE_POINTER (ext_object_p->u.cls.u3.value, module_p);

  module_p->namespace_object_p = namespace_object_p;
  ecma_deref_object (namespace_object_p);

  ecma_module_resolve_stack_push (&stack_p, module_p, ecma_get_magic_string (LIT_MAGIC_STRING_ASTERIX_CHAR));
  while (stack_p != NULL)
  {
    ecma_module_resolve_stack_t *current_frame_p = stack_p;
    ecma_module_t *current_module_p = current_frame_p->module_p;

    ecma_module_resolve_stack_pop (&stack_p);

    /* 15.2.1.16.2 / 2-3 */
    if (!ecma_module_resolve_set_insert (&resolve_set_p,
                                         current_module_p,
                                         ecma_get_magic_string (LIT_MAGIC_STRING_ASTERIX_CHAR)))
    {
      /* Circular import. */
      continue;
    }

    ecma_module_names_t *export_names_p = current_module_p->local_exports_p;

    if (export_names_p != NULL)
    {
      /* 15.2.1.16.2 / 5 */
      do
      {
        result = ecma_module_namespace_object_add_export_if_needed (module_p,
                                                                    export_names_p->imex_name_p);
        export_names_p = export_names_p->next_p;
      }
      while (export_names_p != NULL && ecma_is_value_empty (result));
    }

    /* 15.2.1.16.2 / 6 */
    ecma_module_node_t *indirect_export_p = current_module_p->indirect_exports_p;
    while (indirect_export_p != NULL && ecma_is_value_empty (result))
    {
      export_names_p = indirect_export_p->module_names_p;

      JERRY_ASSERT (export_names_p != NULL);

      do
      {
        result = ecma_module_namespace_object_add_export_if_needed (module_p,
                                                                    export_names_p->imex_name_p);
        export_names_p = export_names_p->next_p;
      }
      while (export_names_p != NULL && ecma_is_value_empty (result));

      indirect_export_p = indirect_export_p->next_p;
    }

    /* 15.2.1.16.2 / 7 */
    ecma_module_node_t *star_export_p = current_module_p->star_exports_p;
    while (star_export_p != NULL && ecma_is_value_empty (result))
    {
      JERRY_ASSERT (star_export_p->module_names_p == NULL);

      /* 15.2.1.16.3/10.c */
      ecma_module_resolve_stack_push (&stack_p,
                                      ecma_module_get_from_object (*star_export_p->u.module_object_p),
                                      ecma_get_magic_string (LIT_MAGIC_STRING_ASTERIX_CHAR));

      star_export_p = star_export_p->next_p;
    }

    if (ECMA_IS_VALUE_ERROR (result))
    {
      break;
    }
  }

  /* Clean up. */
  ecma_module_resolve_set_cleanup (resolve_set_p);
  while (stack_p)
  {
    ecma_module_resolve_stack_pop (&stack_p);
  }

  return result;
} /* ecma_module_create_namespace_object */

/**
 * Connects imported values to the current module scope.
 *
 * @return ECMA_VALUE_ERROR - if an error occured
 *         ECMA_VALUE_EMPTY - otherwise
 */
static ecma_value_t
ecma_module_connect_imports (ecma_module_t *module_p)
{
  ecma_object_t *local_env_p = module_p->scope_p;
  JERRY_ASSERT (ecma_is_lexical_environment (local_env_p));

  ecma_module_node_t *import_node_p = module_p->imports_p;

  /* Check that the imported bindings don't exist yet. */
  while (import_node_p != NULL)
  {
    ecma_module_names_t *import_names_p = import_node_p->module_names_p;

    while (import_names_p != NULL)
    {
      ecma_object_t *lex_env_p = local_env_p;
      ecma_property_t *binding_p = NULL;

      if (lex_env_p->type_flags_refs & ECMA_OBJECT_FLAG_BLOCK)
      {
        binding_p = ecma_find_named_property (lex_env_p, import_names_p->local_name_p);

        JERRY_ASSERT (lex_env_p->u2.outer_reference_cp != JMEM_CP_NULL);
        lex_env_p = ECMA_GET_NON_NULL_POINTER (ecma_object_t, lex_env_p->u2.outer_reference_cp);
      }

      if (binding_p != NULL)
      {
        return ecma_raise_syntax_error (ECMA_ERR_MSG ("Imported binding shadows local variable"));
      }

      ecma_value_t status = ecma_op_has_binding (lex_env_p, import_names_p->local_name_p);

#if JERRY_BUILTIN_PROXY
      if (ECMA_IS_VALUE_ERROR (status))
      {
        return status;
      }
#endif /* JERRY_BUILTIN_PROXY */

      if (ecma_is_value_true (status))
      {
        return ecma_raise_syntax_error (ECMA_ERR_MSG ("Imported binding shadows local variable"));
      }

      import_names_p = import_names_p->next_p;
    }

    import_node_p = import_node_p->next_p;
  }

  import_node_p = module_p->imports_p;

  /* Resolve imports and create local bindings. */
  while (import_node_p != NULL)
  {
    ecma_module_names_t *import_names_p = import_node_p->module_names_p;
    ecma_module_t *imported_module_p = ecma_module_get_from_object (import_node_p->u.path_or_module);

    /* Module is evaluated even if it is used only in export-from statements. */
    ecma_value_t result = ecma_module_evaluate (imported_module_p);

    if (ECMA_IS_VALUE_ERROR (result))
    {
      return result;
    }

    ecma_free_value (result);

    while (import_names_p != NULL)
    {
      const bool is_namespace_import = ecma_compare_ecma_string_to_magic_id (import_names_p->imex_name_p,
                                                                             LIT_MAGIC_STRING_ASTERIX_CHAR);

      if (is_namespace_import)
      {
        result = ecma_module_create_namespace_object (imported_module_p);

        if (ECMA_IS_VALUE_ERROR (result))
        {
          return result;
        }

        ecma_property_value_t *value_p;
        value_p = ecma_create_named_data_property (module_p->scope_p,
                                                   import_names_p->local_name_p,
                                                   ECMA_PROPERTY_FLAG_WRITABLE,
                                                   NULL);
        value_p->value = ecma_make_object_value (imported_module_p->namespace_object_p);
      }
      else /* !is_namespace_import */
      {
        ecma_module_record_t record;
        result = ecma_module_resolve_export (imported_module_p, import_names_p->imex_name_p, &record);

        if (ECMA_IS_VALUE_ERROR (result))
        {
          return result;
        }

        if (record.module_p == NULL)
        {
          return ecma_raise_syntax_error (ECMA_ERR_MSG ("Ambiguous import request"));
        }

        ecma_property_t *property_p = ecma_find_named_property (record.module_p->scope_p, record.name_p);

        ecma_create_named_reference_property (module_p->scope_p,
                                              import_names_p->local_name_p,
                                              property_p);
      }

      import_names_p = import_names_p->next_p;
    }

    import_node_p = import_node_p->next_p;
  }

  return ECMA_VALUE_EMPTY;
} /* ecma_module_connect_imports */

/**
 * Checks if indirect exports in the current context are resolvable.
 * Note: See 15.2.1.16.4 / 9.
 *
 * @return ECMA_VALUE_ERROR - if an error occured
 *         ECMA_VALUE_EMPTY - otherwise
 */
static ecma_value_t
ecma_module_check_indirect_exports (ecma_module_t *module_p)
{
  ecma_module_node_t *indirect_export_p = module_p->indirect_exports_p;
  while (indirect_export_p != NULL)
  {
    ecma_module_names_t *name_p = indirect_export_p->module_names_p;
    while (name_p != NULL)
    {
      ecma_module_record_t record;
      ecma_value_t result;

      result = ecma_module_resolve_export (ecma_module_get_from_object (*indirect_export_p->u.module_object_p),
                                           name_p->local_name_p,
                                           &record);

      if (ECMA_IS_VALUE_ERROR (result))
      {
        return result;
      }

      JERRY_ASSERT (ecma_is_value_empty (result));

      if (record.module_p == NULL)
      {
        return ecma_raise_syntax_error (ECMA_ERR_MSG ("Ambiguous indirect export request"));
      }

      name_p = name_p->next_p;
    }

    indirect_export_p = indirect_export_p->next_p;
  }

  return ECMA_VALUE_EMPTY;
} /* ecma_module_check_indirect_exports */

/**
 * Initialize the current module by creating the local binding for the imported variables
 * and verifying indirect exports.
 *
 * @return ECMA_VALUE_ERROR - if an error occured
 *         ECMA_VALUE_EMPTY - otherwise
 */
ecma_value_t
ecma_module_initialize (ecma_module_t *module_p) /**< module */
{
  ecma_value_t ret_value = ecma_module_connect_imports (module_p);

  if (ecma_is_value_empty (ret_value))
  {
    ret_value = ecma_module_check_indirect_exports (module_p);
  }

  return ret_value;
} /* ecma_module_initialize */

/**
 * Gets the internal module pointer of a module
 *
 * @return module pointer - if module_val is a valid module,
 *         NULL - otherwise
 */
ecma_module_t *
ecma_module_get_resolved_module (ecma_value_t module_val) /**< module */
{
  if (!ecma_is_value_object (module_val))
  {
    return NULL;
  }

  ecma_object_t *object_p = ecma_get_object_from_value (module_val);

  if (ecma_get_object_type (object_p) != ECMA_OBJECT_TYPE_CLASS)
  {
    return NULL;
  }

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

  if (ext_object_p->u.cls.type != ECMA_OBJECT_CLASS_MODULE)
  {
    return NULL;
  }

  return (ecma_module_t *) object_p;
} /* ecma_module_get_resolved_module */

/**
 * A module stack for depth-first search
 */
typedef struct ecma_module_stack_item_t
{
  struct ecma_module_stack_item_t *prev_p; /**< prev in the stack */
  struct ecma_module_stack_item_t *parent_p; /**< parent item in the stack */
  ecma_module_t *module_p; /**< currently processed module */
  ecma_module_node_t *node_p; /**< currently processed node */
  uint32_t dfs_index; /**< dfs index (ES2020 15.2.1.16) */
} ecma_module_stack_item_t;

/**
 * Link module dependencies
 *
 * @return ECMA_VALUE_ERROR - if an error occured
 *         ECMA_VALUE_TRUE - otherwise
 */
ecma_value_t
ecma_module_link (ecma_module_t *module_p, /**< root module */
                  jerry_module_resolve_callback_t callback, /**< resolve module callback */
                  void *user_p) /**< pointer passed to the resolve callback */
{
  if (module_p->header.u.cls.u1.module_state != JERRY_MODULE_STATE_UNLINKED)
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Module must be in unlinked state"));
  }

  module_p->header.u.cls.u1.module_state = JERRY_MODULE_STATE_LINKING;

  uint32_t dfs_index = 0;
  ecma_module_stack_item_t *last_p;
  ecma_module_node_t *node_p;

  last_p = (ecma_module_stack_item_t *) jmem_heap_alloc_block (sizeof (ecma_module_stack_item_t));
  last_p->prev_p = NULL;
  last_p->parent_p = NULL;
  last_p->module_p = module_p;
  last_p->node_p = module_p->imports_p;
  last_p->dfs_index = dfs_index;

  module_p->header.u.cls.u3.dfs_ancestor_index = dfs_index;

  ecma_value_t module_val = ecma_make_object_value (&module_p->header.object);
  ecma_module_stack_item_t *current_p = last_p;

restart:
  /* Entering into processing new node phase. Resolve dependencies first. */
  node_p = current_p->node_p;

  JERRY_ASSERT (ecma_module_get_from_object (module_val)->imports_p == node_p);

  while (node_p != NULL)
  {
    if (ecma_is_value_object (node_p->u.path_or_module))
    {
      /* Already linked. */
      node_p = node_p->next_p;
      continue;
    }

    JERRY_ASSERT (ecma_is_value_string (node_p->u.path_or_module));

    ecma_value_t resolve_result = callback (node_p->u.path_or_module, module_val, user_p);

    if (JERRY_UNLIKELY (ecma_is_value_error_reference (resolve_result)))
    {
      ecma_raise_error_from_error_reference (resolve_result);
      goto error;
    }

    ecma_module_t *resolved_module_p = ecma_module_get_resolved_module (resolve_result);

    if (resolved_module_p == NULL)
    {
      ecma_free_value (resolve_result);
      ecma_raise_type_error (ECMA_ERR_MSG ("Callback result must be a module"));
      goto error;
    }

    ecma_deref_ecma_string (ecma_get_string_from_value (node_p->u.path_or_module));
    node_p->u.path_or_module = resolve_result;
    ecma_deref_object (ecma_get_object_from_value (resolve_result));

    if (resolved_module_p->header.u.cls.u1.module_state == JERRY_MODULE_STATE_ERROR)
    {
      ecma_raise_type_error (ECMA_ERR_MSG ("Cannot link to a module which is in error state"));
      goto error;
    }

    node_p = node_p->next_p;
  }

  /* Find next unlinked node, or return to parent */
  while (true)
  {
    ecma_module_t *current_module_p = current_p->module_p;
    node_p = current_p->node_p;

    while (node_p != NULL)
    {
      module_p = ecma_module_get_from_object (node_p->u.path_or_module);

      if (module_p->header.u.cls.u1.module_state == JERRY_MODULE_STATE_UNLINKED)
      {
        current_p->node_p = node_p->next_p;
        module_p->header.u.cls.u1.module_state = JERRY_MODULE_STATE_LINKING;

        ecma_module_stack_item_t *item_p;
        item_p = (ecma_module_stack_item_t *) jmem_heap_alloc_block (sizeof (ecma_module_stack_item_t));

        dfs_index++;

        item_p->prev_p = last_p;
        item_p->parent_p = current_p;
        item_p->module_p = module_p;
        item_p->node_p = module_p->imports_p;
        item_p->dfs_index = dfs_index;

        module_p->header.u.cls.u3.dfs_ancestor_index = dfs_index;

        last_p = item_p;
        current_p = item_p;
        module_val = node_p->u.path_or_module;
        goto restart;
      }

      if (module_p->header.u.cls.u1.module_state == JERRY_MODULE_STATE_LINKING)
      {
        uint32_t dfs_ancestor_index = module_p->header.u.cls.u3.dfs_ancestor_index;

        if (dfs_ancestor_index < current_module_p->header.u.cls.u3.dfs_ancestor_index)
        {
          current_module_p->header.u.cls.u3.dfs_ancestor_index = dfs_ancestor_index;
        }
      }

      node_p = node_p->next_p;
    }

    if (current_module_p->scope_p == NULL)
    {
      JERRY_ASSERT (!(current_module_p->header.u.cls.u2.module_flags & ECMA_MODULE_IS_NATIVE));

      /* Initialize scope for handling circular references. */
      ecma_value_t result = vm_init_module_scope (current_module_p);

      if (ECMA_IS_VALUE_ERROR (result))
      {
        ecma_module_set_error_state (current_module_p);
        goto error;
      }

      JERRY_ASSERT (result == ECMA_VALUE_EMPTY);
    }

    if (current_module_p->header.u.cls.u3.dfs_ancestor_index != current_p->dfs_index)
    {
      current_p = current_p->parent_p;
      JERRY_ASSERT (current_p != NULL);

      uint32_t dfs_ancestor_index = current_module_p->header.u.cls.u3.dfs_ancestor_index;

      if (dfs_ancestor_index < current_p->module_p->header.u.cls.u3.dfs_ancestor_index)
      {
        current_p->module_p->header.u.cls.u3.dfs_ancestor_index = dfs_ancestor_index;
      }
      continue;
    }

    ecma_module_stack_item_t *end_p = current_p->prev_p;
    current_p = current_p->parent_p;

    do
    {
      ecma_module_stack_item_t *prev_p = last_p->prev_p;

      JERRY_ASSERT (last_p->module_p->header.u.cls.u1.module_state == JERRY_MODULE_STATE_LINKING);
      last_p->module_p->header.u.cls.u1.module_state = JERRY_MODULE_STATE_LINKED;

      if (JERRY_CONTEXT (module_state_changed_callback_p) != NULL)
      {
        JERRY_CONTEXT (module_state_changed_callback_p) (JERRY_MODULE_STATE_LINKED,
                                                         ecma_make_object_value (&last_p->module_p->header.object),
                                                         ECMA_VALUE_UNDEFINED,
                                                         JERRY_CONTEXT (module_state_changed_callback_user_p));
      }

      jmem_heap_free_block (last_p, sizeof (ecma_module_stack_item_t));
      last_p = prev_p;
    }
    while (last_p != end_p);

    if (current_p == NULL)
    {
      return ECMA_VALUE_TRUE;
    }
  }

error:
  JERRY_ASSERT (last_p != NULL);

  do
  {
    ecma_module_stack_item_t *prev_p = last_p->prev_p;

    JERRY_ASSERT (last_p->module_p->header.u.cls.u1.module_state == JERRY_MODULE_STATE_LINKING);
    last_p->module_p->header.u.cls.u1.module_state = JERRY_MODULE_STATE_UNLINKED;

    jmem_heap_free_block (last_p, sizeof (ecma_module_stack_item_t));
    last_p = prev_p;
  }
  while (last_p != NULL);

  return ECMA_VALUE_ERROR;
} /* ecma_module_link */

/**
 * Compute the result of 'import()' calls
 *
 * @return promise object representing the result of the operation
 */
ecma_value_t
ecma_module_import (ecma_value_t specifier, /**< module specifier */
                    ecma_value_t referrer) /**< referrer (script, module or undefined) */
{
  ecma_string_t *specifier_p = ecma_op_to_string (specifier);

  if (JERRY_UNLIKELY (specifier_p == NULL))
  {
    goto error;
  }

  jerry_value_t result;
  ecma_module_t *module_p;

  if (JERRY_CONTEXT (module_import_callback_p) != NULL)
  {
    result = JERRY_CONTEXT (module_import_callback_p) (ecma_make_string_value (specifier_p),
                                                       referrer,
                                                       JERRY_CONTEXT (module_import_callback_user_p));
    ecma_deref_ecma_string (specifier_p);

    if (JERRY_UNLIKELY (ecma_is_value_error_reference (result)))
    {
      ecma_raise_error_from_error_reference (result);
      goto error;
    }

    if (ecma_is_value_object (result)
        && ecma_is_promise (ecma_get_object_from_value (result)))
    {
      return result;
    }

    module_p = ecma_module_get_resolved_module (result);

    if (module_p == NULL)
    {
      ecma_free_value (result);
      goto error_module_instantiate;
    }
  }
  else
  {
    result = jerry_port_module_resolve (ecma_make_string_value (specifier_p), referrer, NULL);
    ecma_deref_ecma_string (specifier_p);

    if (JERRY_UNLIKELY (ecma_is_value_error_reference (result)))
    {
      ecma_raise_error_from_error_reference (result);
      goto error;
    }

    module_p = ecma_module_get_resolved_module (result);

    if (module_p == NULL)
    {
      ecma_free_value (result);
      goto error_module_instantiate;
    }

    if (module_p->header.u.cls.u1.module_state == JERRY_MODULE_STATE_UNLINKED)
    {
      result = ecma_module_link (module_p, jerry_port_module_resolve, NULL);

      if (ECMA_IS_VALUE_ERROR (result))
      {
        ecma_deref_object (&module_p->header.object);
        goto error;
      }

      JERRY_ASSERT (result == ECMA_VALUE_TRUE);
    }

    if (module_p->header.u.cls.u1.module_state == JERRY_MODULE_STATE_LINKED)
    {
      result = ecma_module_evaluate (module_p);

      if (ECMA_IS_VALUE_ERROR (result))
      {
        ecma_deref_object (&module_p->header.object);
        goto error;
      }

      ecma_free_value (result);
    }
  }

  if (module_p->header.u.cls.u1.module_state != JERRY_MODULE_STATE_EVALUATED)
  {
    ecma_deref_object (&module_p->header.object);
    goto error_module_instantiate;
  }

  result = ecma_module_create_namespace_object (module_p);

  if (ECMA_IS_VALUE_ERROR (result))
  {
    ecma_deref_object (&module_p->header.object);
    goto error;
  }

  JERRY_ASSERT (result == ECMA_VALUE_EMPTY);

  result = ecma_op_create_promise_object (ECMA_VALUE_EMPTY, ECMA_VALUE_UNDEFINED, NULL);
  ecma_fulfill_promise (result, ecma_make_object_value (module_p->namespace_object_p));
  ecma_deref_object (&module_p->header.object);
  return result;

error_module_instantiate:
  ecma_raise_range_error (ECMA_ERR_MSG ("Module cannot be instantiated"));

error:
  if (jcontext_has_pending_abort ())
  {
    return ECMA_VALUE_ERROR;
  }

  ecma_value_t exception = jcontext_take_exception ();

  ecma_value_t promise = ecma_op_create_promise_object (ECMA_VALUE_EMPTY, ECMA_VALUE_UNDEFINED, NULL);
  ecma_reject_promise (promise, exception);
  ecma_free_value (exception);
  return promise;
} /* ecma_module_import */

/**
 * Cleans up a list of module names.
 */
void
ecma_module_release_module_names (ecma_module_names_t *module_name_p) /**< first module name */
{
  while (module_name_p != NULL)
  {
    ecma_module_names_t *next_p = module_name_p->next_p;

    ecma_deref_ecma_string (module_name_p->imex_name_p);
    ecma_deref_ecma_string (module_name_p->local_name_p);
    jmem_heap_free_block (module_name_p, sizeof (ecma_module_names_t));

    module_name_p = next_p;
  }
} /* ecma_module_release_module_names */

/**
 * Cleans up a list of module nodes.
 */
static void
ecma_module_release_module_nodes (ecma_module_node_t *module_node_p, /**< first module node */
                                  bool is_import) /**< free path variable */
{
  while (module_node_p != NULL)
  {
    ecma_module_node_t *next_p = module_node_p->next_p;

    ecma_module_release_module_names (module_node_p->module_names_p);

    if (is_import && ecma_is_value_string (module_node_p->u.path_or_module))
    {
      ecma_deref_ecma_string (ecma_get_string_from_value (module_node_p->u.path_or_module));
    }

    jmem_heap_free_block (module_node_p, sizeof (ecma_module_node_t));
    module_node_p = next_p;
  }
} /* ecma_module_release_module_nodes */

/**
 * Cleans up and releases a module structure including all referenced modules.
 */
void
ecma_module_release_module (ecma_module_t *module_p) /**< module */
{
  jerry_module_state_t state = (jerry_module_state_t) module_p->header.u.cls.u1.module_state;

  JERRY_ASSERT (state != JERRY_MODULE_STATE_INVALID);

#ifndef JERRY_NDEBUG
  module_p->scope_p = NULL;
  module_p->namespace_object_p = NULL;
#endif /* JERRY_NDEBUG */

  ecma_module_release_module_names (module_p->local_exports_p);

  if (module_p->header.u.cls.u2.module_flags & ECMA_MODULE_IS_NATIVE)
  {
    return;
  }

  ecma_module_release_module_nodes (module_p->imports_p, true);
  ecma_module_release_module_nodes (module_p->indirect_exports_p, false);
  ecma_module_release_module_nodes (module_p->star_exports_p, false);

  if (module_p->u.compiled_code_p != NULL)
  {
    ecma_bytecode_deref (module_p->u.compiled_code_p);
#ifndef JERRY_NDEBUG
    module_p->u.compiled_code_p = NULL;
#endif /* JERRY_NDEBUG */
  }
} /* ecma_module_release_module */

#endif /* JERRY_MODULE_SYSTEM */

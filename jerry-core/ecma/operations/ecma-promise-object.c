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

#include "ecma-promise-object.h"
#include "ecma-alloc.h"
#include "ecma-array-object.h"
#include "ecma-boolean-object.h"
#include "ecma-builtin-handlers.h"
#include "ecma-builtins.h"
#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-jobqueue.h"
#include "ecma-objects-general.h"
#include "ecma-objects.h"
#include "jcontext.h"

#if JERRY_BUILTIN_PROMISE

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmapromiseobject ECMA Promise object related routines
 * @{
 */

/**
 * Check if an object is promise.
 *
 * @return true - if the object is a promise.
 *         false - otherwise.
 */
extern inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_is_promise (ecma_object_t *obj_p) /**< points to object */
{
  return ecma_object_class_is (obj_p, LIT_MAGIC_STRING_PROMISE_UL);
} /* ecma_is_promise */

/**
 * Get the result of the promise.
 *
 * @return ecma value of the promise result.
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_promise_get_result (ecma_object_t *obj_p) /**< points to promise object */
{
  JERRY_ASSERT (ecma_is_promise (obj_p));

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) obj_p;

  return ecma_copy_value (ext_object_p->u.class_prop.u.value);
} /* ecma_promise_get_result */

/**
 * Set the PromiseResult of promise.
 */
static inline void JERRY_ATTR_ALWAYS_INLINE
ecma_promise_set_result (ecma_object_t *obj_p, /**< points to promise object */
                         ecma_value_t result) /**< the result value */
{
  JERRY_ASSERT (ecma_is_promise (obj_p));

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) obj_p;

  JERRY_ASSERT (ext_object_p->u.class_prop.u.value == ECMA_VALUE_UNDEFINED);

  ext_object_p->u.class_prop.u.value = result;
} /* ecma_promise_set_result */

/**
 * Get the PromiseState of promise.
 *
 * @return the state's enum value
 */
uint16_t
ecma_promise_get_flags (ecma_object_t *obj_p) /**< points to promise object */
{
  JERRY_ASSERT (ecma_is_promise (obj_p));

  return ((ecma_extended_object_t *) obj_p)->u.class_prop.extra_info;
} /* ecma_promise_get_flags */

/**
 * Set the PromiseState of promise.
 */
static inline void JERRY_ATTR_ALWAYS_INLINE
ecma_promise_set_state (ecma_object_t *obj_p, /**< points to promise object */
                        bool is_fulfilled) /**< new flags */
{
  JERRY_ASSERT (ecma_is_promise (obj_p));
  JERRY_ASSERT (ecma_promise_get_flags (obj_p) & ECMA_PROMISE_IS_PENDING);

  uint16_t flags_to_invert =
    (is_fulfilled ? (ECMA_PROMISE_IS_PENDING | ECMA_PROMISE_IS_FULFILLED) : ECMA_PROMISE_IS_PENDING);

  ((ecma_extended_object_t *) obj_p)->u.class_prop.extra_info ^= flags_to_invert;
} /* ecma_promise_set_state */

/**
 * Take a collection of Reactions and enqueue a new PromiseReactionJob for each Reaction.
 *
 * See also: ES2015 25.4.1.8
 */
static void
ecma_promise_trigger_reactions (ecma_collection_t *reactions, /**< lists of reactions */
                                ecma_value_t value, /**< value for resolve or reject */
                                bool is_reject) /**< true if promise is rejected, false otherwise */
{
  ecma_value_t *buffer_p = reactions->buffer_p;
  ecma_value_t *buffer_end_p = buffer_p + reactions->item_count;

  while (buffer_p < buffer_end_p)
  {
    ecma_value_t object_with_tag = *buffer_p++;
    ecma_object_t *object_p = ECMA_GET_NON_NULL_POINTER_FROM_POINTER_TAG (ecma_object_t, object_with_tag);
    ecma_value_t object = ecma_make_object_value (object_p);

    if (JMEM_CP_GET_THIRD_BIT_FROM_POINTER_TAG (object_with_tag))
    {
      ecma_enqueue_promise_async_reaction_job (object, value, is_reject);
      continue;
    }

    if (!is_reject)
    {
      ecma_value_t handler = ECMA_VALUE_TRUE;

      if (JMEM_CP_GET_FIRST_BIT_FROM_POINTER_TAG (object_with_tag))
      {
        handler = *buffer_p++;
      }

      ecma_enqueue_promise_reaction_job (object, handler, value);
    }
    else if (JMEM_CP_GET_FIRST_BIT_FROM_POINTER_TAG (object_with_tag))
    {
      buffer_p++;
    }

    if (is_reject)
    {
      ecma_value_t handler = ECMA_VALUE_FALSE;

      if (JMEM_CP_GET_SECOND_BIT_FROM_POINTER_TAG (object_with_tag))
      {
        handler = *buffer_p++;
      }

      ecma_enqueue_promise_reaction_job (object, handler, value);
    }
    else if (JMEM_CP_GET_SECOND_BIT_FROM_POINTER_TAG (object_with_tag))
    {
      buffer_p++;
    }
  }
} /* ecma_promise_trigger_reactions */

/**
 * Checks whether a resolver is called before.
 *
 * @return true if it was called before, false otherwise
 */
static inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_is_resolver_already_called (ecma_object_t *promise_obj_p) /**< promise */
{
  return (ecma_promise_get_flags (promise_obj_p) & ECMA_PROMISE_ALREADY_RESOLVED) != 0;
} /* ecma_is_resolver_already_called */

/**
 * HostPromiseRejectionTracker
 *
 * See also: ES11 25.6.1.9
 */
static void
ecma_track_promise_rejection (ecma_object_t *obj_p, /**< rejected promise */
                              jerry_promise_rejection_operation_t operation) /**< operation */
{
  JERRY_ASSERT (ecma_is_promise (obj_p));

  if (!(ecma_promise_get_flags (obj_p) & ECMA_PROMISE_HANDLED))
  {
    jerry_port_track_promise_rejection (ecma_make_object_value (obj_p), operation);
  }
} /* ecma_track_promise_rejection */

/**
 * Reject a Promise with a reason.
 *
 * See also: ES2015 25.4.1.7
 */
void
ecma_reject_promise (ecma_value_t promise, /**< promise */
                     ecma_value_t reason) /**< reason for reject */
{
  ecma_object_t *obj_p = ecma_get_object_from_value (promise);

  JERRY_ASSERT (ecma_promise_get_flags (obj_p) & ECMA_PROMISE_IS_PENDING);

  ecma_promise_set_state (obj_p, false);
  ecma_promise_set_result (obj_p, ecma_copy_value_if_not_object (reason));
  ecma_promise_object_t *promise_p = (ecma_promise_object_t *) obj_p;

  /* GC can be triggered by ecma_new_collection so freeing the collection
     first and creating a new one might cause a heap after use event. */
  ecma_collection_t *reactions = promise_p->reactions;

  /* Fulfill reactions will never be triggered. */
  ecma_promise_trigger_reactions (reactions, reason, true);

  promise_p->reactions = ecma_new_collection ();

  ecma_collection_destroy (reactions);
  ecma_track_promise_rejection (obj_p, JERRY_PROMISE_REJECTION_OPERATION_REJECT);
} /* ecma_reject_promise */

/**
 * Fulfill a Promise with a value.
 *
 * See also: ES2015 25.4.1.4
 */
void
ecma_fulfill_promise (ecma_value_t promise, /**< promise */
                      ecma_value_t value) /**< fulfilled value */
{
  ecma_object_t *obj_p = ecma_get_object_from_value (promise);

  JERRY_ASSERT (ecma_promise_get_flags (obj_p) & ECMA_PROMISE_IS_PENDING);

  if (promise == value)
  {
    ecma_raise_type_error (ECMA_ERR_MSG ("A promise cannot be resolved with itself"));
    ecma_value_t exception = jcontext_take_exception ();
    ecma_reject_promise (promise, exception);
    ecma_free_value (exception);
    return;
  }

  if (ecma_is_value_object (value))
  {
    ecma_value_t then = ecma_op_object_get_by_magic_id (ecma_get_object_from_value (value), LIT_MAGIC_STRING_THEN);

    if (ECMA_IS_VALUE_ERROR (then))
    {
      then = jcontext_take_exception ();
      ecma_reject_promise (promise, then);
      ecma_free_value (then);
      return;
    }

    if (ecma_op_is_callable (then))
    {
      ecma_enqueue_promise_resolve_thenable_job (promise, value, then);
      ecma_free_value (then);
      return;
    }

    ecma_free_value (then);
  }

  ecma_promise_set_state (obj_p, true);
  ecma_promise_set_result (obj_p, ecma_copy_value_if_not_object (value));
  ecma_promise_object_t *promise_p = (ecma_promise_object_t *) obj_p;

  /* GC can be triggered by ecma_new_collection so freeing the collection
     first and creating a new one might cause a heap after use event. */
  ecma_collection_t *reactions = promise_p->reactions;

  /* Reject reactions will never be triggered. */
  ecma_promise_trigger_reactions (reactions, value, false);

  promise_p->reactions = ecma_new_collection ();

  ecma_collection_destroy (reactions);
} /* ecma_fulfill_promise */

/**
 * Native handler for Promise Reject Function.
 *
 * See also: ES2015 25.4.1.3.1
 *
 * @return ecma value of undefined.
 */
ecma_value_t
ecma_promise_reject_handler (const ecma_value_t function, /**< the function itself */
                             const ecma_value_t this_arg, /**< this_arg of the function */
                             const ecma_value_t argv[], /**< argument list */
                             const uint32_t argc) /**< argument number */
{
  JERRY_UNUSED (this_arg);
  ecma_promise_resolver_t *function_p = (ecma_promise_resolver_t *) ecma_get_object_from_value (function);

  /* 1. */
  ecma_object_t *promise_obj_p = ecma_get_object_from_value (function_p->promise);
  JERRY_ASSERT (ecma_is_promise (promise_obj_p));

  /* 3., 4. */
  if (!ecma_is_resolver_already_called (promise_obj_p))
  {
    /* 5. */
    ((ecma_extended_object_t *) promise_obj_p)->u.class_prop.extra_info |= ECMA_PROMISE_ALREADY_RESOLVED;

    /* 6. */
    ecma_value_t reject_value = (argc == 0) ? ECMA_VALUE_UNDEFINED : argv[0];
    ecma_reject_promise (function_p->promise, reject_value);
  }

  return ECMA_VALUE_UNDEFINED;
} /* ecma_promise_reject_handler */

/**
 * Native handler for Promise Resolve Function.
 *
 * See also: ES2015 25.4.1.3.2
 *
 * @return ecma value of undefined.
 */
ecma_value_t
ecma_promise_resolve_handler (const ecma_value_t function, /**< the function itself */
                              const ecma_value_t this_arg, /**< this_arg of the function */
                              const ecma_value_t argv[], /**< argument list */
                              const uint32_t argc) /**< argument number */
{
  JERRY_UNUSED (this_arg);
  ecma_promise_resolver_t *function_p = (ecma_promise_resolver_t *) ecma_get_object_from_value (function);

  /* 1. */
  ecma_object_t *promise_obj_p = ecma_get_object_from_value (function_p->promise);
  JERRY_ASSERT (ecma_is_promise (promise_obj_p));

  /* 3., 4. */
  if (!ecma_is_resolver_already_called (promise_obj_p))
  {
    /* 5. */
    ((ecma_extended_object_t *) promise_obj_p)->u.class_prop.extra_info |= ECMA_PROMISE_ALREADY_RESOLVED;

    ecma_fulfill_promise (function_p->promise, (argc == 0) ? ECMA_VALUE_UNDEFINED : argv[0]);
  }

  return ECMA_VALUE_UNDEFINED;
} /* ecma_promise_resolve_handler */

/**
 * Helper function for PromiseCreateResovingFucntions.
 *
 * See also: ES2015 25.4.1.3 2. - 7.
 *
 * @return pointer to the resolving function
 */
static ecma_object_t *
ecma_promise_create_resolving_functions_helper (ecma_object_t *promise_p, /**< Promise Object */
                                                ecma_native_handler_id_t id) /**< Callback handler */
{
  ecma_object_t *func_obj_p = ecma_op_create_native_handler (id, sizeof (ecma_promise_resolver_t));

  ecma_promise_resolver_t *resolver_p = (ecma_promise_resolver_t *) func_obj_p;
  resolver_p->promise = ecma_make_object_value (promise_p);

  return func_obj_p;
} /* ecma_promise_create_resolving_functions_helper */

/**
 * Create a PromiseCreateResovingFucntions.
 *
 * See also: ES2015 25.4.1.3
 *
 * @return pointer to the resolving functions
 */
void
ecma_promise_create_resolving_functions (ecma_promise_object_t *promise_p) /**< the promise object */
{
  /* 2. - 7. */
  ecma_object_t *resolve_func_p =
    ecma_promise_create_resolving_functions_helper ((ecma_object_t *) promise_p, ECMA_NATIVE_HANDLER_PROMISE_RESOLVE);
  ecma_object_t *reject_func_p =
    ecma_promise_create_resolving_functions_helper ((ecma_object_t *) promise_p, ECMA_NATIVE_HANDLER_PROMISE_REJECT);

  promise_p->resolve = ecma_make_object_value (resolve_func_p);
  promise_p->reject = ecma_make_object_value (reject_func_p);

  ecma_deref_object (resolve_func_p);
  ecma_deref_object (reject_func_p);
} /* ecma_promise_create_resolving_functions */

/**
 * Create a promise object.
 *
 * See also: ES2015 25.4.3.1
 *
 * @return ecma value of the new promise object
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_create_promise_object (ecma_value_t executor, /**< the executor function or object */
                               ecma_promise_executor_type_t type) /**< indicates the type of executor */
{
  JERRY_ASSERT (JERRY_CONTEXT (current_new_target_p) != NULL);
  /* 3. */
  ecma_object_t *proto_p =
    ecma_op_get_prototype_from_constructor (JERRY_CONTEXT (current_new_target_p), ECMA_BUILTIN_ID_PROMISE_PROTOTYPE);

  if (JERRY_UNLIKELY (proto_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }

  /* Calling ecma_new_collection might trigger a GC call, so this
   * allocation is performed before the object is constructed. */
  ecma_collection_t *reactions = ecma_new_collection ();

  ecma_object_t *object_p = ecma_create_object (proto_p, sizeof (ecma_promise_object_t), ECMA_OBJECT_TYPE_CLASS);
  ecma_deref_object (proto_p);
  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;
  ext_object_p->u.class_prop.class_id = LIT_MAGIC_STRING_PROMISE_UL;
  /* 5 */
  ext_object_p->u.class_prop.extra_info = ECMA_PROMISE_IS_PENDING;
  ext_object_p->u.class_prop.u.value = ECMA_VALUE_UNDEFINED;

  /* 6-8. */
  ecma_promise_object_t *promise_object_p = (ecma_promise_object_t *) object_p;
  promise_object_p->reactions = reactions;
  /* Creating the resolving function may trigger a GC, so these need to be initialized. */
  promise_object_p->resolve = ECMA_VALUE_EMPTY;
  promise_object_p->reject = ECMA_VALUE_EMPTY;

  ecma_promise_create_resolving_functions (promise_object_p);

  /* 9. */
  ecma_value_t completion = ECMA_VALUE_UNDEFINED;

  if (type == ECMA_PROMISE_EXECUTOR_FUNCTION)
  {
    JERRY_ASSERT (ecma_op_is_callable (executor));

    ecma_value_t argv[] = { promise_object_p->resolve, promise_object_p->reject };
    completion = ecma_op_function_call (ecma_get_object_from_value (executor), ECMA_VALUE_UNDEFINED, argv, 2);
  }
  else
  {
    JERRY_ASSERT (type == ECMA_PROMISE_EXECUTOR_EMPTY);
    JERRY_UNUSED (executor);
  }

  ecma_value_t status = ECMA_VALUE_EMPTY;

  if (ECMA_IS_VALUE_ERROR (completion))
  {
    /* 10.a. */
    completion = jcontext_take_exception ();
    status = ecma_op_function_call (ecma_get_object_from_value (promise_object_p->reject),
                                    ECMA_VALUE_UNDEFINED,
                                    &completion,
                                    1);
  }

  ecma_free_value (completion);

  /* 10.b. */
  if (ECMA_IS_VALUE_ERROR (status))
  {
    ecma_deref_object (object_p);
    return status;
  }

  /* 11. */
  ecma_free_value (status);

  return ecma_make_object_value (object_p);
} /* ecma_op_create_promise_object */

/**
 * Helper function for increase or decrease the remaining count.
 *
 * @return the current remaining count after increase or decrease.
 */
uint32_t
ecma_promise_remaining_inc_or_dec (ecma_value_t remaining, /**< the remaining count */
                                   bool is_inc) /**< whether to increase the count */
{
  JERRY_ASSERT (ecma_is_value_object (remaining));

  ecma_object_t *remaining_p = ecma_get_object_from_value (remaining);
  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) remaining_p;

  JERRY_ASSERT (ext_object_p->u.class_prop.class_id == LIT_MAGIC_STRING_NUMBER_UL);

  JERRY_ASSERT (ecma_is_value_integer_number (ext_object_p->u.class_prop.u.value));

  uint32_t current = (uint32_t) ecma_get_integer_from_value (ext_object_p->u.class_prop.u.value);

  if (is_inc)
  {
    current++;
  }
  else
  {
    current--;
  }
  ext_object_p->u.class_prop.u.value = ecma_make_uint32_value (current);

  return current;
} /* ecma_promise_remaining_inc_or_dec */

/**
 * Native handler for Promise.all Resolve Element Function.
 *
 * See also:
 *         ES2015 25.4.4.1.2
 *
 * @return ecma value of undefined.
 */
ecma_value_t
ecma_promise_all_handler_cb (const ecma_value_t function_obj, /**< the function itself */
                             const ecma_value_t this_val, /**< this_arg of the function */
                             const ecma_value_t args_p[], /**< argument list */
                             const uint32_t args_count) /**< argument number */
{
  JERRY_UNUSED (this_val);
  JERRY_UNUSED (args_count);
  ecma_promise_all_executor_t *executor_p = (ecma_promise_all_executor_t *) ecma_get_object_from_value (function_obj);

  /* 1 - 2. */
  if (executor_p->index == 0)
  {
    return ECMA_VALUE_UNDEFINED;
  }

  /* 8. */
  ecma_op_object_put_by_index (ecma_get_object_from_value (executor_p->values),
                               (uint32_t) (executor_p->index - 1),
                               args_p[0],
                               false);
  /* 3. */
  executor_p->index = 0;

  /* 9-10. */
  ecma_value_t ret = ECMA_VALUE_UNDEFINED;
  if (ecma_promise_remaining_inc_or_dec (executor_p->remaining_elements, false) == 0)
  {
    ecma_value_t capability = executor_p->capability;
    ecma_promise_capabality_t *capability_p = (ecma_promise_capabality_t *) ecma_get_object_from_value (capability);
    ret = ecma_op_function_call (ecma_get_object_from_value (capability_p->resolve),
                                 ECMA_VALUE_UNDEFINED,
                                 &executor_p->values,
                                 1);
  }

  return ret;
} /* ecma_promise_all_handler_cb */

/**
 * GetCapabilitiesExecutor Functions
 *
 * Checks and sets a promiseCapability's resolve and reject properties.
 *
 * See also: ES11 25.6.1.5.1
 *
 * @return ECMA_VALUE_UNDEFINED or TypeError
 *         returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_get_capabilities_executor_cb (const ecma_value_t function_obj, /**< the function itself */
                                      const ecma_value_t this_val, /**< this_arg of the function */
                                      const ecma_value_t args_p[], /**< argument list */
                                      const uint32_t args_count) /**< argument number */
{
  JERRY_UNUSED (this_val);

  /* 1. */
  ecma_promise_capability_executor_t *executor_p =
    (ecma_promise_capability_executor_t *) ecma_get_object_from_value (function_obj);

  /* 2-3. */
  ecma_object_t *capability_obj_p = ecma_get_object_from_value (executor_p->capability);
  JERRY_ASSERT (ecma_object_class_is (capability_obj_p, LIT_INTERNAL_MAGIC_PROMISE_CAPABILITY));
  ecma_promise_capabality_t *capability_p = (ecma_promise_capabality_t *) capability_obj_p;

  /* 4. */
  if (!ecma_is_value_undefined (capability_p->resolve))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Resolve must be undefined"));
  }

  /* 5. */
  if (!ecma_is_value_undefined (capability_p->reject))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Reject must be undefined"));
  }

  /* 6. */
  capability_p->resolve = args_count > 0 ? args_p[0] : ECMA_VALUE_UNDEFINED;
  /* 7. */
  capability_p->reject = args_count > 1 ? args_p[1] : ECMA_VALUE_UNDEFINED;

  /* 8. */
  return ECMA_VALUE_UNDEFINED;
} /* ecma_op_get_capabilities_executor_cb */

/**
 * Create a new PromiseCapability.
 *
 * See also: ES11 25.6.1.5
 *
 * @return NULL - if the operation raises error
 *         new PromiseCapability object - otherwise
 */
ecma_object_t *
ecma_promise_new_capability (ecma_value_t constructor)
{
  /* 1. */
  if (!ecma_is_constructor (constructor))
  {
    ecma_raise_type_error (ECMA_ERR_MSG ("Invalid capability"));
    return NULL;
  }

  ecma_object_t *constructor_obj_p = ecma_get_object_from_value (constructor);

  /* 3. */
  ecma_object_t *capability_obj_p = ecma_create_object (ecma_builtin_get (ECMA_BUILTIN_ID_OBJECT_PROTOTYPE),
                                                        sizeof (ecma_promise_capabality_t),
                                                        ECMA_OBJECT_TYPE_CLASS);

  ecma_promise_capabality_t *capability_p = (ecma_promise_capabality_t *) capability_obj_p;
  capability_p->header.u.class_prop.class_id = LIT_INTERNAL_MAGIC_PROMISE_CAPABILITY;
  capability_p->header.u.class_prop.u.promise = ECMA_VALUE_UNDEFINED;
  capability_p->resolve = ECMA_VALUE_UNDEFINED;
  capability_p->reject = ECMA_VALUE_UNDEFINED;

  /* 4-5. */
  ecma_object_t *executor_p = ecma_op_create_native_handler (ECMA_NATIVE_HANDLER_PROMISE_CAPABILITY_EXECUTOR,
                                                             sizeof (ecma_promise_capability_executor_t));

  /* 6. */
  ecma_promise_capability_executor_t *executor_func_p = (ecma_promise_capability_executor_t *) executor_p;
  executor_func_p->capability = ecma_make_object_value (capability_obj_p);

  /* 7. */
  ecma_value_t executor = ecma_make_object_value (executor_p);
  ecma_value_t promise = ecma_op_function_construct (constructor_obj_p, constructor_obj_p, &executor, 1);
  ecma_deref_object (executor_p);

  if (ECMA_IS_VALUE_ERROR (promise))
  {
    ecma_deref_object (capability_obj_p);
    return NULL;
  }

  /* 8. */
  if (!ecma_op_is_callable (capability_p->resolve))
  {
    ecma_free_value (promise);
    ecma_deref_object (capability_obj_p);
    ecma_raise_type_error (ECMA_ERR_MSG ("'resolve' parameter must be callable"));
    return NULL;
  }

  /* 9. */
  if (!ecma_op_is_callable (capability_p->reject))
  {
    ecma_free_value (promise);
    ecma_deref_object (capability_obj_p);
    ecma_raise_type_error (ECMA_ERR_MSG ("'reject' parameter must be callable"));
    return NULL;
  }

  /* 10. */
  capability_p->header.u.class_prop.u.promise = promise;

  ecma_free_value (promise);

  /* 11. */
  return capability_obj_p;
} /* ecma_promise_new_capability */

/**
 * The common function for 'reject' and 'resolve'.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_promise_reject_or_resolve (ecma_value_t this_arg, /**< "this" argument */
                                ecma_value_t value, /**< rejected or resolved value */
                                bool is_resolve) /**< the operation is resolve */
{
  if (!ecma_is_value_object (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not an object"));
  }

  if (is_resolve && ecma_is_value_object (value) && ecma_is_promise (ecma_get_object_from_value (value)))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (value);
    ecma_value_t constructor = ecma_op_object_get_by_magic_id (object_p, LIT_MAGIC_STRING_CONSTRUCTOR);

    if (ECMA_IS_VALUE_ERROR (constructor))
    {
      return constructor;
    }

    /* The this_arg must be an object. */
    bool is_same_value = (constructor == this_arg);
    ecma_free_value (constructor);

    if (is_same_value)
    {
      return ecma_copy_value (value);
    }
  }

  ecma_object_t *capability_obj_p = ecma_promise_new_capability (this_arg);

  if (JERRY_UNLIKELY (capability_obj_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }

  ecma_promise_capabality_t *capability_p = (ecma_promise_capabality_t *) capability_obj_p;

  ecma_value_t func = is_resolve ? capability_p->resolve : capability_p->reject;

  ecma_value_t call_ret = ecma_op_function_call (ecma_get_object_from_value (func), ECMA_VALUE_UNDEFINED, &value, 1);

  if (ECMA_IS_VALUE_ERROR (call_ret))
  {
    ecma_deref_object (capability_obj_p);
    return call_ret;
  }

  ecma_free_value (call_ret);

  ecma_value_t promise = ecma_copy_value (capability_p->header.u.class_prop.u.promise);
  ecma_deref_object (capability_obj_p);

  return promise;
} /* ecma_promise_reject_or_resolve */

/**
 * It performs the "then" operation on promiFulfilled
 * and onRejected as its settlement actions.
 *
 * See also: 25.4.5.3.1
 *
 * @return ecma value of the new promise object
 *         Returned value must be freed with ecma_free_value
 */
static ecma_value_t
ecma_promise_do_then (ecma_value_t promise, /**< the promise which call 'then' */
                      ecma_value_t on_fulfilled, /**< on_fulfilled function */
                      ecma_value_t on_rejected, /**< on_rejected function */
                      ecma_object_t *result_capability_obj_p) /**< promise capability */
{
  JERRY_ASSERT (ecma_object_class_is (result_capability_obj_p, LIT_INTERNAL_MAGIC_PROMISE_CAPABILITY));

  ecma_promise_capabality_t *capability_p = (ecma_promise_capabality_t *) result_capability_obj_p;

  /* 3. boolean true indicates "indentity" */
  if (!ecma_op_is_callable (on_fulfilled))
  {
    on_fulfilled = ECMA_VALUE_TRUE;
  }

  /* 4. boolean false indicates "thrower" */
  if (!ecma_op_is_callable (on_rejected))
  {
    on_rejected = ECMA_VALUE_FALSE;
  }

  ecma_object_t *promise_obj_p = ecma_get_object_from_value (promise);
  ecma_promise_object_t *promise_p = (ecma_promise_object_t *) promise_obj_p;

  uint16_t flags = ecma_promise_get_flags (promise_obj_p);

  if (flags & ECMA_PROMISE_IS_PENDING)
  {
    /* 7. */
    /* [ capability, (on_fulfilled), (on_rejected) ] */
    ecma_value_t reaction_values[3];
    ecma_value_t *reactions_p = reaction_values + 1;

    uint8_t tag = 0;

    if (on_fulfilled != ECMA_VALUE_TRUE)
    {
      tag |= JMEM_FIRST_TAG_BIT_MASK;
      *reactions_p++ = on_fulfilled;
    }

    if (on_rejected != ECMA_VALUE_FALSE)
    {
      tag |= JMEM_SECOND_TAG_BIT_MASK;
      *reactions_p++ = on_rejected;
    }

    ECMA_SET_NON_NULL_POINTER_TAG (reaction_values[0], result_capability_obj_p, tag);

    uint32_t value_count = (uint32_t) (reactions_p - reaction_values);
    ecma_collection_append (promise_p->reactions, reaction_values, value_count);
  }
  else if (flags & ECMA_PROMISE_IS_FULFILLED)
  {
    /* 8. */
    ecma_value_t value = ecma_promise_get_result (promise_obj_p);
    ecma_enqueue_promise_reaction_job (ecma_make_object_value (result_capability_obj_p), on_fulfilled, value);
    ecma_free_value (value);
  }
  else
  {
    /* 9. */
    ecma_value_t reason = ecma_promise_get_result (promise_obj_p);
    ecma_track_promise_rejection (promise_obj_p, JERRY_PROMISE_REJECTION_OPERATION_HANDLE);
    ecma_enqueue_promise_reaction_job (ecma_make_object_value (result_capability_obj_p), on_rejected, reason);
    ecma_free_value (reason);
  }

  /* ES11: 11. */
  promise_p->header.u.class_prop.extra_info |= ECMA_PROMISE_HANDLED;

  /* 10. */
  return ecma_copy_value (capability_p->header.u.class_prop.u.promise);
} /* ecma_promise_do_then */

/**
 * The common function for ecma_builtin_promise_prototype_then
 * and ecma_builtin_promise_prototype_catch.
 *
 * @return ecma value of a new promise object.
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_promise_then (ecma_value_t promise, /**< the promise which call 'then' */
                   ecma_value_t on_fulfilled, /**< on_fulfilled function */
                   ecma_value_t on_rejected) /**< on_rejected function */
{
  if (!ecma_is_value_object (promise))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not an object"));
  }

  ecma_object_t *obj = ecma_get_object_from_value (promise);

  if (!ecma_is_promise (obj))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a Promise"));
  }

  ecma_value_t species = ecma_op_species_constructor (obj, ECMA_BUILTIN_ID_PROMISE);
  if (ECMA_IS_VALUE_ERROR (species))
  {
    return species;
  }

  ecma_object_t *result_capability_obj_p = ecma_promise_new_capability (species);
  ecma_free_value (species);

  if (JERRY_UNLIKELY (result_capability_obj_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }

  ecma_value_t ret = ecma_promise_do_then (promise, on_fulfilled, on_rejected, result_capability_obj_p);
  ecma_deref_object (result_capability_obj_p);

  return ret;
} /* ecma_promise_then */

/**
 * Definition of valueThunk function
 *
 * See also:
 *         ES2020 25.6.5.3.1 step 8.
 *
 * @return ecma value
 */
ecma_value_t
ecma_value_thunk_helper_cb (const ecma_value_t function_obj, /**< the function itself */
                            const ecma_value_t this_val, /**< this_arg of the function */
                            const ecma_value_t args_p[], /**< argument list */
                            const uint32_t args_count) /**< argument number */
{
  JERRY_UNUSED_3 (this_val, args_p, args_count);

  ecma_object_t *func_obj_p = ecma_get_object_from_value (function_obj);
  ecma_promise_value_thunk_t *value_thunk_obj_p = (ecma_promise_value_thunk_t *) func_obj_p;

  return ecma_copy_value (value_thunk_obj_p->value);
} /* ecma_value_thunk_helper_cb */

/**
 * Definition of thrower function
 *
 * See also:
 *         ES2020 25.6.5.3.2 step 8.
 *
 * @return ecma value
 */
ecma_value_t
ecma_value_thunk_thrower_cb (const ecma_value_t function_obj, /**< the function itself */
                             const ecma_value_t this_val, /**< this_arg of the function */
                             const ecma_value_t args_p[], /**< argument list */
                             const uint32_t args_count) /**< argument number */
{
  JERRY_UNUSED_3 (this_val, args_p, args_count);

  ecma_object_t *func_obj_p = ecma_get_object_from_value (function_obj);
  ecma_promise_value_thunk_t *value_thunk_obj_p = (ecma_promise_value_thunk_t *) func_obj_p;

  jcontext_raise_exception (ecma_copy_value (value_thunk_obj_p->value));

  return ECMA_VALUE_ERROR;
} /* ecma_value_thunk_thrower_cb */

/**
 * Helper function for Then Finally and Catch Finally common parts
 *
 * See also:
 *         ES2020 25.6.5.3.1
 *         ES2020 25.6.5.3.2
 *
 * @return ecma value
 */
static ecma_value_t
ecma_promise_then_catch_finally_helper (ecma_value_t function_obj, /**< the function itself */
                                        ecma_native_handler_id_t id, /**< handler id */
                                        ecma_value_t arg) /**< callback function argument */
{
  /* 2. */
  ecma_object_t *func_obj_p = ecma_get_object_from_value (function_obj);
  ecma_promise_finally_function_t *finally_func_obj = (ecma_promise_finally_function_t *) func_obj_p;

  /* 3. */
  JERRY_ASSERT (ecma_op_is_callable (finally_func_obj->on_finally));

  /* 4. */
  ecma_value_t result =
    ecma_op_function_call (ecma_get_object_from_value (finally_func_obj->on_finally), ECMA_VALUE_UNDEFINED, NULL, 0);

  if (ECMA_IS_VALUE_ERROR (result))
  {
    return result;
  }

  /* 6. */
  JERRY_ASSERT (ecma_is_constructor (finally_func_obj->constructor));

  /* 7. */
  ecma_value_t promise = ecma_promise_reject_or_resolve (finally_func_obj->constructor, result, true);

  ecma_free_value (result);

  if (ECMA_IS_VALUE_ERROR (promise))
  {
    return promise;
  }

  /* 8. */
  ecma_object_t *value_thunk_func_p = ecma_op_create_native_handler (id, sizeof (ecma_promise_value_thunk_t));

  ecma_promise_value_thunk_t *value_thunk_func_obj = (ecma_promise_value_thunk_t *) value_thunk_func_p;
  value_thunk_func_obj->value = ecma_copy_value_if_not_object (arg);

  /* 9. */
  ecma_value_t value_thunk = ecma_make_object_value (value_thunk_func_p);
  ecma_value_t ret_value = ecma_op_invoke_by_magic_id (promise, LIT_MAGIC_STRING_THEN, &value_thunk, 1);

  ecma_free_value (promise);
  ecma_deref_object (value_thunk_func_p);

  return ret_value;
} /* ecma_promise_then_catch_finally_helper */

/**
 * Definition of Then Finally Function
 *
 * See also:
 *         ES2020 25.6.5.3.1
 *
 * @return ecma value
 */
ecma_value_t
ecma_promise_then_finally_cb (const ecma_value_t function_obj, /**< the function itself */
                              const ecma_value_t this_val, /**< this_arg of the function */
                              const ecma_value_t args_p[], /**< argument list */
                              const uint32_t args_count) /**< argument number */
{
  JERRY_UNUSED_2 (this_val, args_count);
  JERRY_ASSERT (args_count > 0);

  return ecma_promise_then_catch_finally_helper (function_obj, ECMA_NATIVE_HANDLER_VALUE_THUNK, args_p[0]);
} /* ecma_promise_then_finally_cb */

/**
 * Definition of Catch Finally Function
 *
 * See also:
 *         ES2020 25.6.5.3.2
 *
 * @return ecma value
 */
ecma_value_t
ecma_promise_catch_finally_cb (const ecma_value_t function_obj, /**< the function itself */
                               const ecma_value_t this_val, /**< this_arg of the function */
                               const ecma_value_t args_p[], /**< argument list */
                               const uint32_t args_count) /**< argument number */
{
  JERRY_UNUSED_2 (this_val, args_count);
  JERRY_ASSERT (args_count > 0);

  return ecma_promise_then_catch_finally_helper (function_obj, ECMA_NATIVE_HANDLER_VALUE_THROWER, args_p[0]);
} /* ecma_promise_catch_finally_cb */

/**
 * The common function for ecma_builtin_promise_prototype_finally
 *
 * @return ecma value of a new promise object.
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_promise_finally (ecma_value_t promise, /**< the promise which call 'finally' */
                      ecma_value_t on_finally) /**< on_finally function */
{
  /* 2. */
  if (!ecma_is_value_object (promise))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not an object"));
  }

  ecma_object_t *obj = ecma_get_object_from_value (promise);

  /* 3. */
  ecma_value_t species = ecma_op_species_constructor (obj, ECMA_BUILTIN_ID_PROMISE);

  if (ECMA_IS_VALUE_ERROR (species))
  {
    return species;
  }

  /* 4. */
  JERRY_ASSERT (ecma_is_constructor (species));

  /* 5. */
  if (!ecma_op_is_callable (on_finally))
  {
    ecma_free_value (species);
    ecma_value_t invoke_args[2] = { on_finally, on_finally };
    return ecma_op_invoke_by_magic_id (promise, LIT_MAGIC_STRING_THEN, invoke_args, 2);
  }

  /* 6.a-b */
  ecma_object_t *then_finally_obj_p =
    ecma_op_create_native_handler (ECMA_NATIVE_HANDLER_PROMISE_THEN_FINALLY, sizeof (ecma_promise_finally_function_t));

  /* 6.c-d */
  ecma_promise_finally_function_t *then_finally_func_obj_p = (ecma_promise_finally_function_t *) then_finally_obj_p;
  then_finally_func_obj_p->constructor = species;
  then_finally_func_obj_p->on_finally = on_finally;

  /* 6.e-f */
  ecma_object_t *catch_finally_obj_p =
    ecma_op_create_native_handler (ECMA_NATIVE_HANDLER_PROMISE_CATCH_FINALLY, sizeof (ecma_promise_finally_function_t));

  /* 6.g-h */
  ecma_promise_finally_function_t *catch_finally_func_obj = (ecma_promise_finally_function_t *) catch_finally_obj_p;
  catch_finally_func_obj->constructor = species;
  catch_finally_func_obj->on_finally = on_finally;

  ecma_deref_object (ecma_get_object_from_value (species));

  /* 7. */
  ecma_value_t invoke_args[2] = { ecma_make_object_value (then_finally_obj_p),
                                  ecma_make_object_value (catch_finally_obj_p) };

  ecma_value_t ret_value = ecma_op_invoke_by_magic_id (promise, LIT_MAGIC_STRING_THEN, invoke_args, 2);

  ecma_deref_object (then_finally_obj_p);
  ecma_deref_object (catch_finally_obj_p);

  return ret_value;
} /* ecma_promise_finally */

/**
 * Resume the execution of an async function after the promise is resolved
 */
void
ecma_promise_async_then (ecma_value_t promise, /**< promise object */
                         ecma_value_t executable_object) /**< executable object of the async function */
{
  ecma_object_t *promise_obj_p = ecma_get_object_from_value (promise);
  uint16_t flags = ecma_promise_get_flags (promise_obj_p);

  if (flags & ECMA_PROMISE_IS_PENDING)
  {
    ecma_value_t executable_object_with_tag;
    ECMA_SET_NON_NULL_POINTER_TAG (executable_object_with_tag, ecma_get_object_from_value (executable_object), 0);
    ECMA_SET_THIRD_BIT_TO_POINTER_TAG (executable_object_with_tag);

    ecma_collection_push_back (((ecma_promise_object_t *) promise_obj_p)->reactions, executable_object_with_tag);
    return;
  }

  ecma_value_t value = ecma_promise_get_result (promise_obj_p);
  ecma_enqueue_promise_async_reaction_job (executable_object, value, !(flags & ECMA_PROMISE_IS_FULFILLED));
  ecma_free_value (value);
} /* ecma_promise_async_then */

/**
 * Resolves the value and resume the execution of an async function after the resolve is completed
 *
 * @return ECMA_VALUE_UNDEFINED if not error is occured, an error otherwise
 */
ecma_value_t
ecma_promise_async_await (ecma_extended_object_t *async_generator_object_p, /**< async generator function */
                          ecma_value_t value) /**< value to be resolved (takes the reference) */
{
  ecma_value_t promise = ecma_make_object_value (ecma_builtin_get (ECMA_BUILTIN_ID_PROMISE));
  ecma_value_t result = ecma_promise_reject_or_resolve (promise, value, true);

  ecma_free_value (value);

  if (ECMA_IS_VALUE_ERROR (result))
  {
    return result;
  }

  ecma_promise_async_then (result, ecma_make_object_value ((ecma_object_t *) async_generator_object_p));
  ecma_free_value (result);
  return ECMA_VALUE_UNDEFINED;
} /* ecma_promise_async_await */

/**
 * @}
 * @}
 */
#endif /* JERRY_BUILTIN_PROMISE */

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

#ifndef JERRYX_ARG_H
#define JERRYX_ARG_H

#include "jerryscript.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * The forward declaration of jerryx_arg_t.
 */
typedef struct jerryx_arg_t jerryx_arg_t;

/**
 * The forward declaration of jerryx_arg_js_iterator_t
 */
typedef struct jerryx_arg_js_iterator_t jerryx_arg_js_iterator_t;

/**
 * Signature of the transform function.
 * @param js_arg_iter_p - available JS args
 * @param c_arg_p - native arg
 */
typedef jerry_value_t (*jerryx_arg_transform_func_t) (jerryx_arg_js_iterator_t *js_arg_iter_p,
                                                      const jerryx_arg_t *c_arg_p);

/**
 * The structure used in jerryx_arg_object_properties
 */
typedef struct
{
  const jerry_char_t **name_p; /**< property name list of the JS object */
  jerry_length_t name_cnt; /**< count of the name list */
  const jerryx_arg_t *c_arg_p; /**< points to the array of transformation steps */
  jerry_length_t c_arg_cnt; /**< the count of the `c_arg_p` array */
} jerryx_arg_object_props_t;

/**
 * The structure used in jerryx_arg_array
 */
typedef struct
{
  const jerryx_arg_t *c_arg_p; /**< points to the array of transformation steps */
  jerry_length_t c_arg_cnt; /**< the count of the `c_arg_p` array */
} jerryx_arg_array_items_t;

/**
 * The structure defining a single validation & transformation step.
 */
struct jerryx_arg_t
{
  jerryx_arg_transform_func_t func; /**< the transform function */
  void *dest; /**< pointer to destination where func should store the result */
  uintptr_t extra_info; /**< extra information, specific to func */
};

jerry_value_t jerryx_arg_transform_this_and_args (const jerry_value_t this_val,
                                                  const jerry_value_t *js_arg_p,
                                                  const jerry_length_t js_arg_cnt,
                                                  const jerryx_arg_t *c_arg_p,
                                                  jerry_length_t c_arg_cnt);

jerry_value_t jerryx_arg_transform_args (const jerry_value_t *js_arg_p,
                                         const jerry_length_t js_arg_cnt,
                                         const jerryx_arg_t *c_arg_p,
                                         jerry_length_t c_arg_cnt);

jerry_value_t jerryx_arg_transform_object_properties (const jerry_value_t obj_val,
                                                      const jerry_char_t **name_p,
                                                      const jerry_length_t name_cnt,
                                                      const jerryx_arg_t *c_arg_p,
                                                      jerry_length_t c_arg_cnt);
jerry_value_t
jerryx_arg_transform_array (const jerry_value_t array_val, const jerryx_arg_t *c_arg_p, jerry_length_t c_arg_cnt);

/**
 * Indicates whether an argument is allowed to be coerced into the expected JS type.
 */
typedef enum
{
  JERRYX_ARG_COERCE, /**< the transform inside will invoke toNumber, toBoolean or toString */
  JERRYX_ARG_NO_COERCE /**< the type coercion is not allowed. */
} jerryx_arg_coerce_t;

/**
 * Indicates whether an argument is optional or required.
 */
typedef enum
{
  /**
   * The argument is optional. If the argument is `undefined` the transform is
   * successful and `c_arg_p->dest` remains untouched.
   */
  JERRYX_ARG_OPTIONAL,
  /**
   * The argument is required. If the argument is `undefined` the transform
   * will fail and `c_arg_p->dest` remains untouched.
   */
  JERRYX_ARG_REQUIRED
} jerryx_arg_optional_t;

/**
 * Indicates the rounding policy which will be chosen to transform an integer.
 */
typedef enum
{
  JERRYX_ARG_ROUND, /**< round */
  JERRYX_ARG_FLOOR, /**< floor */
  JERRYX_ARG_CEIL /**< ceil */
} jerryx_arg_round_t;

/**
 * Indicates the clamping policy which will be chosen to transform an integer.
 * If the policy is NO_CLAMP, and the number is out of range,
 * then the transformer will throw a range error.
 */
typedef enum
{
  JERRYX_ARG_CLAMP, /**< clamp the number when it is out of range */
  JERRYX_ARG_NO_CLAMP /**< throw a range error */
} jerryx_arg_clamp_t;

/* Inline functions for initializing jerryx_arg_t */

#define JERRYX_ARG_INTEGER(type)                                                 \
  static inline jerryx_arg_t jerryx_arg_##type (type##_t *dest,                  \
                                                jerryx_arg_round_t round_flag,   \
                                                jerryx_arg_clamp_t clamp_flag,   \
                                                jerryx_arg_coerce_t coerce_flag, \
                                                jerryx_arg_optional_t opt_flag);

JERRYX_ARG_INTEGER (uint8)
JERRYX_ARG_INTEGER (int8)
JERRYX_ARG_INTEGER (uint16)
JERRYX_ARG_INTEGER (int16)
JERRYX_ARG_INTEGER (uint32)
JERRYX_ARG_INTEGER (int32)

#undef JERRYX_ARG_INTEGER

static inline jerryx_arg_t
jerryx_arg_number (double *dest, jerryx_arg_coerce_t coerce_flag, jerryx_arg_optional_t opt_flag);
static inline jerryx_arg_t
jerryx_arg_boolean (bool *dest, jerryx_arg_coerce_t coerce_flag, jerryx_arg_optional_t opt_flag);
static inline jerryx_arg_t
jerryx_arg_string (char *dest, uint32_t size, jerryx_arg_coerce_t coerce_flag, jerryx_arg_optional_t opt_flag);
static inline jerryx_arg_t
jerryx_arg_utf8_string (char *dest, uint32_t size, jerryx_arg_coerce_t coerce_flag, jerryx_arg_optional_t opt_flag);
static inline jerryx_arg_t jerryx_arg_function (jerry_value_t *dest, jerryx_arg_optional_t opt_flag);
static inline jerryx_arg_t
jerryx_arg_native_pointer (void **dest, const jerry_object_native_info_t *info_p, jerryx_arg_optional_t opt_flag);
static inline jerryx_arg_t jerryx_arg_ignore (void);
static inline jerryx_arg_t jerryx_arg_custom (void *dest, uintptr_t extra_info, jerryx_arg_transform_func_t func);
static inline jerryx_arg_t jerryx_arg_object_properties (const jerryx_arg_object_props_t *object_props_p,
                                                         jerryx_arg_optional_t opt_flag);
static inline jerryx_arg_t jerryx_arg_array (const jerryx_arg_array_items_t *array_items_p,
                                             jerryx_arg_optional_t opt_flag);

jerry_value_t jerryx_arg_transform_optional (jerryx_arg_js_iterator_t *js_arg_iter_p,
                                             const jerryx_arg_t *c_arg_p,
                                             jerryx_arg_transform_func_t func);

/* Helper functions for transform functions. */
jerry_value_t jerryx_arg_js_iterator_pop (jerryx_arg_js_iterator_t *js_arg_iter_p);
jerry_value_t jerryx_arg_js_iterator_restore (jerryx_arg_js_iterator_t *js_arg_iter_p);
jerry_value_t jerryx_arg_js_iterator_peek (jerryx_arg_js_iterator_t *js_arg_iter_p);
jerry_length_t jerryx_arg_js_iterator_index (jerryx_arg_js_iterator_t *js_arg_iter_p);

#include "arg.impl.h"

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYX_ARG_H */

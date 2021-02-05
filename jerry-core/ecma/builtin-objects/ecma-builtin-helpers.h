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

#ifndef ECMA_BUILTIN_HELPERS_H
#define ECMA_BUILTIN_HELPERS_H

#include "ecma-exceptions.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-regexp-object.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltinhelpers ECMA builtin helper operations
 * @{
 */

/**
 * Mode of string index routine.
 */
typedef enum
{
  /** These routines must be in this order */
  ECMA_STRING_LAST_INDEX_OF, /**< String.lastIndexOf: ECMA-262 v5, 15.5.4.8 */
  ECMA_STRING_INDEX_OF, /**< String.indexOf: ECMA-262 v5, 15.5.4.7 */
  ECMA_STRING_STARTS_WITH, /**< String.startsWith: ECMA-262 v6, 21.1.3.18 */
  ECMA_STRING_INCLUDES, /**< String.includes: ECMA-262 v6, 21.1.3.7 */
  ECMA_STRING_ENDS_WITH /**< String.includes: ECMA-262 v6, 21.1.3.6 */
} ecma_string_index_of_mode_t;

ecma_value_t ecma_builtin_helper_object_to_string (const ecma_value_t this_arg);
ecma_string_t *ecma_builtin_helper_get_to_locale_string_at_index (ecma_object_t *obj_p, ecma_length_t index);
ecma_value_t ecma_builtin_helper_array_concat_value (ecma_object_t *obj_p, ecma_length_t *length_p, ecma_value_t value);
ecma_value_t ecma_builtin_helper_uint32_index_normalize (ecma_value_t arg, uint32_t length, uint32_t *number_p);
ecma_value_t
ecma_builtin_helper_array_index_normalize (ecma_value_t arg, ecma_length_t length, ecma_length_t *number_p);
ecma_value_t ecma_builtin_helper_string_index_normalize (ecma_number_t index, uint32_t length, bool nan_to_zero);
ecma_value_t ecma_builtin_helper_string_prototype_object_index_of (ecma_string_t *original_str_p,
                                                                   ecma_value_t arg1,
                                                                   ecma_value_t arg2,
                                                                   ecma_string_index_of_mode_t mode);
uint32_t
ecma_builtin_helper_string_find_index (ecma_string_t *original_str_p, ecma_string_t *search_str_p, uint32_t start_pos);
ecma_value_t
ecma_builtin_helper_def_prop (ecma_object_t *obj_p, ecma_string_t *name_p, ecma_value_t value, uint32_t opts);

ecma_value_t
ecma_builtin_helper_def_prop_by_index (ecma_object_t *obj_p, ecma_length_t index, ecma_value_t value, uint32_t opts);
/**
 * Context for replace substitutions
 */
typedef struct
{
  ecma_stringbuilder_t builder; /**< result string builder */
  const lit_utf8_byte_t *string_p; /**< source string */
  lit_utf8_size_t string_size; /**< source string size */
  const lit_utf8_byte_t *matched_p; /**< matched string */
  lit_utf8_size_t matched_size; /**< matcehd string size */
  lit_utf8_size_t match_byte_pos; /**< byte position of the match in the source string */
  uint16_t flags; /**< replace flags */

  /**
   * Capture results
   */
  union
  {
#if JERRY_BUILTIN_REGEXP
    const ecma_regexp_capture_t *captures_p; /**< array of regexp capturing groups */
#endif /* JERRY_BUILTIN_REGEXP */
    const ecma_collection_t *collection_p; /**< collection of captured substrings */
  } u;

  uint32_t capture_count; /**< number of captures in the capturing group array */
  ecma_string_t *replace_str_p; /**< replacement string */
} ecma_replace_context_t;

void ecma_builtin_replace_substitute (ecma_replace_context_t *ctx_p);

#if JERRY_ESNEXT
bool ecma_builtin_is_regexp_exec (ecma_extended_object_t *obj_p);
#endif /* JERRY_ESNEXT */

#if JERRY_BUILTIN_DATE

/**
 * Time range defines for helper functions.
 *
 * See also:
 *          ECMA-262 v5, 15.9.1.1, 15.9.1.10
 */

/** Hours in a day. */
#define ECMA_DATE_HOURS_PER_DAY ((ecma_number_t) 24)

/** Minutes in an hour. */
#define ECMA_DATE_MINUTES_PER_HOUR ((ecma_number_t) 60)

/** Seconds in a minute. */
#define ECMA_DATE_SECONDS_PER_MINUTE ((ecma_number_t) 60)

/** Milliseconds in a second. */
#define ECMA_DATE_MS_PER_SECOND ((ecma_number_t) 1000)

/** ECMA_DATE_MS_PER_MINUTE == 60000 */
#define ECMA_DATE_MS_PER_MINUTE (ECMA_DATE_MS_PER_SECOND * ECMA_DATE_SECONDS_PER_MINUTE)

/** ECMA_DATE_MS_PER_HOUR == 3600000 */
#define ECMA_DATE_MS_PER_HOUR (ECMA_DATE_MS_PER_MINUTE * ECMA_DATE_MINUTES_PER_HOUR)

/** ECMA_DATE_MS_PER_DAY == 86400000 */
#define ECMA_DATE_MS_PER_DAY (ECMA_DATE_MS_PER_HOUR * ECMA_DATE_HOURS_PER_DAY)

/**
 * This gives a range of 8,640,000,000,000,000 milliseconds
 * to either side of 01 January, 1970 UTC.
 */
#define ECMA_DATE_MAX_VALUE 8.64e15

/**
 * Timezone type.
 */
typedef enum
{
  ECMA_DATE_UTC, /**< date vaule is in UTC */
  ECMA_DATE_LOCAL /**< date vaule is in local time */
} ecma_date_timezone_t;

/* ecma-builtin-helpers-date.c */
extern const char day_names_p[7][3];
extern const char month_names_p[12][3];

ecma_number_t ecma_date_day (ecma_number_t time);
ecma_number_t ecma_date_time_within_day (ecma_number_t time);
ecma_number_t ecma_date_year_from_time (ecma_number_t time);
ecma_number_t ecma_date_month_from_time (ecma_number_t time);
ecma_number_t ecma_date_date_from_time (ecma_number_t time);
ecma_number_t ecma_date_week_day (ecma_number_t time);
ecma_number_t ecma_date_local_time_zone_adjustment (ecma_number_t time);
ecma_number_t ecma_date_utc (ecma_number_t time);
ecma_number_t ecma_date_hour_from_time (ecma_number_t time);
ecma_number_t ecma_date_min_from_time (ecma_number_t time);
ecma_number_t ecma_date_sec_from_time (ecma_number_t time);
ecma_number_t ecma_date_ms_from_time (ecma_number_t time);
ecma_number_t ecma_date_make_time (ecma_number_t hour, ecma_number_t min, ecma_number_t sec, ecma_number_t ms);
ecma_number_t ecma_date_make_day (ecma_number_t year, ecma_number_t month, ecma_number_t date);
ecma_number_t ecma_date_make_date (ecma_number_t day, ecma_number_t time);
ecma_number_t ecma_date_time_clip (ecma_number_t time);
ecma_number_t ecma_date_timezone_offset (ecma_number_t time);

ecma_value_t ecma_date_value_to_string (ecma_number_t datetime_number);
ecma_value_t ecma_date_value_to_utc_string (ecma_number_t datetime_number);
ecma_value_t ecma_date_value_to_iso_string (ecma_number_t datetime_number);
ecma_value_t ecma_date_value_to_date_string (ecma_number_t datetime_number);
ecma_value_t ecma_date_value_to_time_string (ecma_number_t datetime_number);

#endif /* JERRY_BUILTIN_DATE */

#if JERRY_BUILTIN_JSON
/* ecma-builtin-helper-json.c */

/**
 * Occurence stack item of JSON.stringify()
 */
typedef struct struct_ecma_json_occurence_stack_item_t
{
  struct struct_ecma_json_occurence_stack_item_t *next_p; /**< next stack item */
  ecma_object_t *object_p; /**< current object */
} ecma_json_occurence_stack_item_t;

/**
 * Context for JSON.stringify()
 */
typedef struct
{
  /** Collection for property keys. */
  ecma_collection_t *property_list_p;

  /** Collection for traversing objects. */
  ecma_json_occurence_stack_item_t *occurence_stack_last_p;

  /** The actual indentation text. */
  ecma_stringbuilder_t indent_builder;

  /** The indentation text. */
  ecma_string_t *gap_str_p;

  /** The replacer function. */
  ecma_object_t *replacer_function_p;

  /** Result string builder. */
  ecma_stringbuilder_t result_builder;
} ecma_json_stringify_context_t;

ecma_value_t ecma_builtin_json_parse_buffer (const lit_utf8_byte_t *str_start_p, lit_utf8_size_t string_size);
ecma_value_t ecma_builtin_json_stringify_no_opts (const ecma_value_t value);
bool ecma_json_has_object_in_stack (ecma_json_occurence_stack_item_t *stack_p, ecma_object_t *object_p);

ecma_value_t ecma_builtin_helper_json_create_non_formatted_json (lit_utf8_byte_t left_bracket,
                                                                 lit_utf8_byte_t right_bracket,
                                                                 ecma_collection_t *partial_p);
#endif /* JERRY_BUILTIN_JSON */

/* ecma-builtin-helper-error.c */

ecma_value_t ecma_builtin_helper_error_dispatch_call (ecma_standard_error_t error_type,
                                                      const ecma_value_t *arguments_list_p,
                                                      uint32_t arguments_list_len);

/* ecma-builtin-helpers-sort.c */

/**
 * Comparison callback function header for sorting helper routines.
 */
typedef ecma_value_t (*ecma_builtin_helper_sort_compare_fn_t) (ecma_value_t lhs, /**< left value */
                                                               ecma_value_t rhs, /**< right value */
                                                               ecma_value_t compare_func); /**< compare function */

ecma_value_t ecma_builtin_helper_array_merge_sort_helper (ecma_value_t *array_p,
                                                          uint32_t length,
                                                          ecma_value_t compare_func,
                                                          const ecma_builtin_helper_sort_compare_fn_t sort_cb);

/**
 * @}
 * @}
 */

#endif /* !ECMA_BUILTIN_HELPERS_H */

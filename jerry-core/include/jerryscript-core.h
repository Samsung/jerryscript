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

#ifndef JERRYSCRIPT_CORE_H
#define JERRYSCRIPT_CORE_H

#include "jerryscript-types.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/** \addtogroup jerry Jerry engine interface
 * @{
 */

/**
 * General engine functions.
 */
void jerry_init (jerry_init_flag_t flags);
void jerry_cleanup (void);
void jerry_register_magic_strings (const jerry_char_t * const *ex_str_items_p,
                                   uint32_t count,
                                   const jerry_length_t *str_lengths_p);
void jerry_gc (jerry_gc_mode_t mode);
void *jerry_get_context_data (const jerry_context_data_manager_t *manager_p);

bool jerry_get_memory_stats (jerry_heap_stats_t *out_stats_p);

/**
 * Parser and executor functions.
 */
bool jerry_run_simple (const jerry_char_t *script_source_p, size_t script_source_size, jerry_init_flag_t flags);
jerry_value_t jerry_parse (const jerry_char_t *source_p, size_t source_size,
                           const jerry_parse_options_t *options_p);
jerry_value_t jerry_parse_function (const jerry_char_t *arg_list_p, size_t arg_list_size,
                                    const jerry_char_t *source_p, size_t source_size,
                                    const jerry_parse_options_t *options_p);
jerry_value_t jerry_run (const jerry_value_t func_val);
jerry_value_t jerry_eval (const jerry_char_t *source_p, size_t source_size, uint32_t parse_opts);

jerry_value_t jerry_run_all_enqueued_jobs (void);

/**
 * Get the global context.
 */
jerry_value_t jerry_get_global_object (void);

/**
 * Checker functions of 'jerry_value_t'.
 */
bool jerry_value_is_abort (const jerry_value_t value);
bool jerry_value_is_array (const jerry_value_t value);
bool jerry_value_is_boolean (const jerry_value_t value);
bool jerry_value_is_constructor (const jerry_value_t value);
bool jerry_value_is_error (const jerry_value_t value);
bool jerry_value_is_function (const jerry_value_t value);
bool jerry_value_is_async_function (const jerry_value_t value);
bool jerry_value_is_number (const jerry_value_t value);
bool jerry_value_is_null (const jerry_value_t value);
bool jerry_value_is_object (const jerry_value_t value);
bool jerry_value_is_promise (const jerry_value_t value);
bool jerry_value_is_proxy (const jerry_value_t value);
bool jerry_value_is_string (const jerry_value_t value);
bool jerry_value_is_symbol (const jerry_value_t value);
bool jerry_value_is_bigint (const jerry_value_t value);
bool jerry_value_is_undefined (const jerry_value_t value);
bool jerry_value_is_true (const jerry_value_t value);
bool jerry_value_is_false (const jerry_value_t value);

jerry_type_t jerry_value_get_type (const jerry_value_t value);
jerry_object_type_t jerry_object_get_type (const jerry_value_t value);
jerry_function_type_t jerry_function_get_type (const jerry_value_t value);
jerry_iterator_type_t jerry_iterator_get_type (const jerry_value_t value);

/**
 * Checker function of whether the specified compile feature is enabled.
 */
bool jerry_is_feature_enabled (const jerry_feature_t feature);

/**
 * Binary operations
 */
jerry_value_t jerry_binary_operation (jerry_binary_operation_t op,
                                      const jerry_value_t lhs,
                                      const jerry_value_t rhs);

/**
 * Error manipulation functions.
 */
jerry_value_t jerry_create_abort_from_value (jerry_value_t value, bool release);
jerry_value_t jerry_create_error_from_value (jerry_value_t value, bool release);
jerry_value_t jerry_get_value_from_error (jerry_value_t value, bool release);
void jerry_set_error_object_created_callback (jerry_error_object_created_callback_t callback, void *user_p);

/**
 * Error object function(s).
 */
jerry_error_t jerry_get_error_type (jerry_value_t value);

/**
 * Getter functions of 'jerry_value_t'.
 */
double jerry_get_number_value (const jerry_value_t value);

/**
 * Functions for string values.
 */
jerry_size_t jerry_get_string_size (const jerry_value_t value);
jerry_size_t jerry_get_utf8_string_size (const jerry_value_t value);
jerry_length_t jerry_get_string_length (const jerry_value_t value);
jerry_length_t jerry_get_utf8_string_length (const jerry_value_t value);
jerry_size_t jerry_string_to_char_buffer (const jerry_value_t value, jerry_char_t *buffer_p, jerry_size_t buffer_size);
jerry_size_t jerry_string_to_utf8_char_buffer (const jerry_value_t value,
                                               jerry_char_t *buffer_p,
                                               jerry_size_t buffer_size);
jerry_size_t jerry_substring_to_char_buffer (const jerry_value_t value,
                                             jerry_length_t start_pos,
                                             jerry_length_t end_pos,
                                             jerry_char_t *buffer_p,
                                             jerry_size_t buffer_size);
jerry_size_t jerry_substring_to_utf8_char_buffer (const jerry_value_t value,
                                                  jerry_length_t start_pos,
                                                  jerry_length_t end_pos,
                                                  jerry_char_t *buffer_p,
                                                  jerry_size_t buffer_size);

/**
 * Functions for array object values.
 */
uint32_t jerry_get_array_length (const jerry_value_t value);

/**
 * Converters of 'jerry_value_t'.
 */
bool jerry_value_to_boolean (const jerry_value_t value);
jerry_value_t jerry_value_to_number (const jerry_value_t value);
jerry_value_t jerry_value_to_object (const jerry_value_t value);
jerry_value_t jerry_value_to_primitive (const jerry_value_t value);
jerry_value_t jerry_value_to_string (const jerry_value_t value);
jerry_value_t jerry_value_to_bigint (const jerry_value_t value);
double jerry_value_as_integer (const jerry_value_t value);
int32_t jerry_value_as_int32 (const jerry_value_t value);
uint32_t jerry_value_as_uint32 (const jerry_value_t value);

/**
 * Acquire types with reference counter (increase the references).
 */
jerry_value_t jerry_acquire_value (jerry_value_t value);

/**
 * Release the referenced values.
 */
void jerry_release_value (jerry_value_t value);

/**
 * Create functions of API values.
 */
jerry_value_t jerry_create_array (uint32_t size);
jerry_value_t jerry_create_boolean (bool value);
jerry_value_t jerry_create_error (jerry_error_t error_type, const jerry_char_t *message_p);
jerry_value_t jerry_create_error_sz (jerry_error_t error_type, const jerry_char_t *message_p,
                                     jerry_size_t message_size);
jerry_value_t jerry_create_external_function (jerry_external_handler_t handler_p);
jerry_value_t jerry_create_number (double value);
jerry_value_t jerry_create_number_infinity (bool sign);
jerry_value_t jerry_create_number_nan (void);
jerry_value_t jerry_create_null (void);
jerry_value_t jerry_create_object (void);
jerry_value_t jerry_create_promise (void);
jerry_value_t jerry_create_proxy (const jerry_value_t target, const jerry_value_t handler);
jerry_value_t jerry_create_special_proxy (const jerry_value_t target, const jerry_value_t handler,
                                          uint32_t options);
jerry_value_t jerry_create_regexp (const jerry_char_t *pattern, uint16_t flags);
jerry_value_t jerry_create_regexp_sz (const jerry_char_t *pattern, jerry_size_t pattern_size, uint16_t flags);
jerry_value_t jerry_create_string_from_utf8 (const jerry_char_t *str_p);
jerry_value_t jerry_create_string_sz_from_utf8 (const jerry_char_t *str_p, jerry_size_t str_size);
jerry_value_t jerry_create_string (const jerry_char_t *str_p);
jerry_value_t jerry_create_string_sz (const jerry_char_t *str_p, jerry_size_t str_size);
jerry_value_t jerry_create_external_string (const jerry_char_t *str_p,
                                            jerry_value_free_callback_t free_cb);
jerry_value_t jerry_create_external_string_sz (const jerry_char_t *str_p, jerry_size_t str_size,
                                               jerry_value_free_callback_t free_cb);
jerry_value_t jerry_create_symbol (const jerry_value_t value);
jerry_value_t jerry_create_bigint (const uint64_t *digits_p, uint32_t size, bool sign);
jerry_value_t jerry_create_undefined (void);
jerry_value_t jerry_create_realm (void);

/**
 * General API functions of JS objects.
 */
jerry_value_t jerry_has_property (const jerry_value_t obj_val, const jerry_value_t prop_name_val);
jerry_value_t jerry_has_own_property (const jerry_value_t obj_val, const jerry_value_t prop_name_val);
bool jerry_has_internal_property (const jerry_value_t obj_val, const jerry_value_t prop_name_val);
bool jerry_delete_property (const jerry_value_t obj_val, const jerry_value_t prop_name_val);
bool jerry_delete_property_by_index (const jerry_value_t obj_val, uint32_t index);
bool jerry_delete_internal_property (const jerry_value_t obj_val, const jerry_value_t prop_name_val);

jerry_value_t jerry_get_property (const jerry_value_t obj_val, const jerry_value_t prop_name_val);
jerry_value_t jerry_get_property_by_index (const jerry_value_t obj_val, uint32_t index);
jerry_value_t jerry_get_own_property (const jerry_value_t obj_val, const jerry_value_t prop_name_val,
                                      const jerry_value_t receiver_val, bool *found_p);
jerry_value_t jerry_get_internal_property (const jerry_value_t obj_val, const jerry_value_t prop_name_val);
jerry_value_t jerry_set_property (const jerry_value_t obj_val, const jerry_value_t prop_name_val,
                                  const jerry_value_t value_to_set);
jerry_value_t jerry_set_property_by_index (const jerry_value_t obj_val, uint32_t index,
                                           const jerry_value_t value_to_set);
bool jerry_set_internal_property (const jerry_value_t obj_val, const jerry_value_t prop_name_val,
                                  const jerry_value_t value_to_set);

jerry_property_descriptor_t jerry_property_descriptor_create (void);
jerry_value_t jerry_define_own_property (const jerry_value_t obj_val,
                                         const jerry_value_t prop_name_val,
                                         const jerry_property_descriptor_t *prop_desc_p);

jerry_value_t jerry_get_own_property_descriptor (const jerry_value_t obj_val,
                                                 const jerry_value_t prop_name_val,
                                                 jerry_property_descriptor_t *prop_desc_p);
void jerry_property_descriptor_free (const jerry_property_descriptor_t *prop_desc_p);

jerry_value_t jerry_call_function (const jerry_value_t func_obj_val, const jerry_value_t this_val,
                                   const jerry_value_t args_p[], jerry_size_t args_count);
jerry_value_t jerry_construct_object (const jerry_value_t func_obj_val, const jerry_value_t args_p[],
                                      jerry_size_t args_count);

jerry_value_t jerry_get_object_keys (const jerry_value_t obj_val);
jerry_value_t jerry_get_prototype (const jerry_value_t obj_val);
jerry_value_t jerry_set_prototype (const jerry_value_t obj_val, const jerry_value_t proto_obj_val);

bool jerry_get_object_native_pointer (const jerry_value_t obj_val,
                                      void **out_native_pointer_p,
                                      const jerry_object_native_info_t *native_pointer_info_p);
void jerry_set_object_native_pointer (const jerry_value_t obj_val,
                                      void *native_pointer_p,
                                      const jerry_object_native_info_t *native_info_p);
bool jerry_delete_object_native_pointer (const jerry_value_t obj_val,
                                         const jerry_object_native_info_t *native_info_p);
void jerry_native_pointer_init_references (void *native_pointer_p,
                                           const jerry_object_native_info_t *native_info_p);
void jerry_native_pointer_release_references (void *native_pointer_p,
                                              const jerry_object_native_info_t *native_info_p);
void jerry_native_pointer_set_reference (jerry_value_t *reference_p, jerry_value_t value);

bool jerry_objects_foreach (jerry_objects_foreach_t foreach_p,
                            void *user_data);
bool jerry_objects_foreach_by_native_info (const jerry_object_native_info_t *native_info_p,
                                           jerry_objects_foreach_by_native_info_t foreach_p,
                                           void *user_data_p);

bool jerry_foreach_object_property (const jerry_value_t obj_val, jerry_object_property_foreach_t foreach_p,
                                    void *user_data_p);

jerry_value_t jerry_object_get_property_names (const jerry_value_t obj_val, jerry_property_filter_t filter);
jerry_value_t jerry_from_property_descriptor (const jerry_property_descriptor_t *src_prop_desc_p);
jerry_value_t jerry_to_property_descriptor (jerry_value_t obj_value, jerry_property_descriptor_t *out_prop_desc_p);

/**
 * Module functions.
 */

jerry_value_t jerry_module_link (const jerry_value_t module_val,
                                 jerry_module_resolve_callback_t callback_p, void *user_p);
jerry_value_t jerry_module_evaluate (const jerry_value_t module_val);
jerry_module_state_t jerry_module_get_state (const jerry_value_t module_val);
void jerry_module_set_state_changed_callback (jerry_module_state_changed_callback_t callback, void *user_p);
size_t jerry_module_get_number_of_requests (const jerry_value_t module_val);
jerry_value_t jerry_module_get_request (const jerry_value_t module_val, size_t request_index);
jerry_value_t jerry_module_get_namespace (const jerry_value_t module_val);

jerry_value_t jerry_native_module_create (jerry_native_module_evaluate_callback_t callback,
                                          const jerry_value_t * const exports_p, size_t number_of_exports);
jerry_value_t jerry_native_module_get_export (const jerry_value_t native_mmodule_val,
                                              const jerry_value_t export_name_val);
jerry_value_t jerry_native_module_set_export (const jerry_value_t native_mmodule_val,
                                              const jerry_value_t export_name_val,
                                              const jerry_value_t value_to_set);

/**
 * Promise functions.
 */

jerry_value_t jerry_resolve_or_reject_promise (jerry_value_t promise, jerry_value_t argument, bool is_resolve);

jerry_value_t jerry_get_promise_result (const jerry_value_t promise);
jerry_promise_state_t jerry_get_promise_state (const jerry_value_t promise);

void jerry_promise_set_callback (jerry_promise_event_filter_t filters, jerry_promise_callback_t callback,
                                 void *user_p);

/**
 * Symbol functions.
 */

jerry_value_t jerry_get_well_known_symbol (jerry_well_known_symbol_t symbol);
jerry_value_t jerry_get_symbol_description (const jerry_value_t symbol);
jerry_value_t jerry_get_symbol_descriptive_string (const jerry_value_t symbol);

/**
 * Realm functions.
 */
jerry_value_t jerry_set_realm (jerry_value_t realm_value);
jerry_value_t jerry_realm_get_this (jerry_value_t realm_value);
jerry_value_t jerry_realm_set_this (jerry_value_t realm_value, jerry_value_t this_value);

/**
 * BigInt functions.
 */
uint32_t jerry_get_bigint_size_in_digits (jerry_value_t value);
void jerry_get_bigint_digits (jerry_value_t value, uint64_t *digits_p, uint32_t size, bool *sign_p);

/**
 * Proxy functions.
 */
jerry_value_t jerry_get_proxy_target (jerry_value_t proxy_value);
jerry_value_t jerry_get_proxy_handler (jerry_value_t proxy_value);

/**
 * Input validator functions.
 */
bool jerry_is_valid_utf8_string (const jerry_char_t *utf8_buf_p, jerry_size_t buf_size);
bool jerry_is_valid_cesu8_string (const jerry_char_t *cesu8_buf_p, jerry_size_t buf_size);

/*
 * Dynamic memory management functions.
 */
void *jerry_heap_alloc (size_t size);
void jerry_heap_free (void *mem_p, size_t size);

/*
 * External context functions.
 */
jerry_context_t *jerry_create_context (uint32_t heap_size, jerry_context_alloc_t alloc, void *cb_data_p);

/**
 * Backtrace functions.
 */
jerry_value_t jerry_get_backtrace (uint32_t max_depth);
void jerry_backtrace_capture (jerry_backtrace_callback_t callback, void *user_p);
jerry_backtrace_frame_types_t jerry_backtrace_get_frame_type (jerry_backtrace_frame_t *frame_p);
const jerry_backtrace_location_t *jerry_backtrace_get_location (jerry_backtrace_frame_t *frame_p);
const jerry_value_t *jerry_backtrace_get_function (jerry_backtrace_frame_t *frame_p);
const jerry_value_t *jerry_backtrace_get_this (jerry_backtrace_frame_t *frame_p);
bool jerry_backtrace_is_strict (jerry_backtrace_frame_t *frame_p);

/**
 * Miscellaneous functions.
 */
void jerry_set_vm_exec_stop_callback (jerry_vm_exec_stop_callback_t stop_cb, void *user_p, uint32_t frequency);
jerry_value_t jerry_get_resource_name (const jerry_value_t value);

/**
 * Array buffer components.
 */
bool jerry_value_is_arraybuffer (const jerry_value_t value);
jerry_value_t jerry_create_arraybuffer (const jerry_length_t size);
jerry_value_t jerry_create_arraybuffer_external (const jerry_length_t size,
                                                 uint8_t *buffer_p,
                                                 jerry_value_free_callback_t free_cb);
jerry_length_t jerry_arraybuffer_write (const jerry_value_t value,
                                        jerry_length_t offset,
                                        const uint8_t *buf_p,
                                        jerry_length_t buf_size);
jerry_length_t jerry_arraybuffer_read (const jerry_value_t value,
                                       jerry_length_t offset,
                                       uint8_t *buf_p,
                                       jerry_length_t buf_size);
jerry_length_t jerry_get_arraybuffer_byte_length (const jerry_value_t value);
uint8_t *jerry_get_arraybuffer_pointer (const jerry_value_t value);
jerry_value_t jerry_is_arraybuffer_detachable (const jerry_value_t value);
jerry_value_t jerry_detach_arraybuffer (const jerry_value_t value);

/**
 * SharedArrayBuffer components.
 */

bool jerry_value_is_shared_arraybuffer (const jerry_value_t value);
jerry_value_t jerry_create_shared_arraybuffer (const jerry_length_t size);
jerry_value_t jerry_create_shared_arraybuffer_external (const jerry_length_t size,
                                                        uint8_t *buffer_p,
                                                        jerry_value_free_callback_t free_cb);

/**
 * DataView functions.
 */
jerry_value_t
jerry_create_dataview (const jerry_value_t value,
                       const jerry_length_t byte_offset,
                       const jerry_length_t byte_length);

bool
jerry_value_is_dataview (const jerry_value_t value);

jerry_value_t
jerry_get_dataview_buffer (const jerry_value_t dataview,
                           jerry_length_t *byte_offset,
                           jerry_length_t *byte_length);

/**
 * TypedArray functions.
 */

bool jerry_value_is_typedarray (jerry_value_t value);
jerry_value_t jerry_create_typedarray (jerry_typedarray_type_t type_name, jerry_length_t length);
jerry_value_t jerry_create_typedarray_for_arraybuffer_sz (jerry_typedarray_type_t type_name,
                                                          const jerry_value_t arraybuffer,
                                                          jerry_length_t byte_offset,
                                                          jerry_length_t length);
jerry_value_t jerry_create_typedarray_for_arraybuffer (jerry_typedarray_type_t type_name,
                                                       const jerry_value_t arraybuffer);
jerry_typedarray_type_t jerry_get_typedarray_type (jerry_value_t value);
jerry_length_t jerry_get_typedarray_length (jerry_value_t value);
jerry_value_t jerry_get_typedarray_buffer (jerry_value_t value,
                                           jerry_length_t *byte_offset,
                                           jerry_length_t *byte_length);
jerry_value_t jerry_json_parse (const jerry_char_t *string_p, jerry_size_t string_size);
jerry_value_t jerry_json_stringify (const jerry_value_t object_to_stringify);
jerry_value_t jerry_create_container (jerry_container_type_t container_type,
                                      const jerry_value_t *arguments_list_p,
                                      jerry_length_t arguments_list_len);
jerry_container_type_t jerry_get_container_type (const jerry_value_t value);
jerry_value_t jerry_get_array_from_container (jerry_value_t value, bool *is_key_value_p);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRYSCRIPT_CORE_H */

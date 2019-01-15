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

#ifndef ECMA_GC_H
#define ECMA_GC_H

#include "ecma-globals.h"
#include "jerryscript-heap-snapshot.h"
#include "jmem.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmagc Garbage collector
 * @{
 */

void ecma_init_gc_info (ecma_object_t *object_p);
void ecma_ref_object (ecma_object_t *object_p);
void ecma_deref_object (ecma_object_t *object_p);
void ecma_gc_run (jmem_free_unused_memory_severity_t severity);
void ecma_free_unused_memory (jmem_free_unused_memory_severity_t severity);

/**
 * Type of heap allocation
 */
typedef enum
{
  ECMA_GC_ALLOCATION_TYPE_NATIVE, /**< General allocation */
  ECMA_GC_ALLOCATION_TYPE_OBJECT, /**< ecma_object_t */
  ECMA_GC_ALLOCATION_TYPE_STRING, /**< ecma_string_t */
  ECMA_GC_ALLOCATION_TYPE_PROPERTY_PAIR, /**< ecma_property_header_t */
  ECMA_GC_ALLOCATION_TYPE_LIT_STORAGE, /**< ecma_lit_storage_item_t */
  ECMA_GC_ALLOCATION_TYPE_BYTECODE, /**< ecma_compiled_code_t */
} ecma_heap_gc_allocation_type_t;

/**
 * Called at least once for every allocation during an invocation of ecma_gc_walk_heap.
 */
typedef void (*ecma_gc_traverse_callback_t)(void *parent_p,
                                            void *alloc_p,
                                            ecma_heap_gc_allocation_type_t alloc_type,
                                            jerry_heap_snapshot_edge_type_t edge_type,
                                            ecma_string_t *edge_name_p,
                                            void *user_data_p);

void ecma_gc_walk_heap (ecma_gc_traverse_callback_t traverse_cb,
                        void *user_data_p);

/**
 * @}
 * @}
 */

#endif /* !ECMA_GC_H */

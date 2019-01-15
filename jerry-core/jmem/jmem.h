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

#ifndef JMEM_H
#define JMEM_H

#include "jrt.h"

/** \addtogroup mem Memory allocation
 * @{
 *
 * \addtogroup heap Heap
 * @{
 */

/**
 * Logarithm of required alignment for allocated units/blocks
 */
#define JMEM_ALIGNMENT_LOG   3

/**
 * Representation of NULL value for compressed pointers
 */
#define JMEM_CP_NULL ((jmem_cpointer_t) 0)

/**
 * Required alignment for allocated units/blocks
 */
#define JMEM_ALIGNMENT (1u << JMEM_ALIGNMENT_LOG)

/**
 * Compressed pointer representations
 *
 * 16 bit representation:
 *   The jmem_cpointer_t is defined as uint16_t
 *   and it can contain any sixteen bit value.
 *
 * 32 bit representation:
 *   The jmem_cpointer_t is defined as uint32_t.
 *   The lower JMEM_ALIGNMENT_LOG bits must be zero.
 *   The other bits can have any value.
 *
 * The 16 bit representation always encodes an offset from
 * a heap base. The 32 bit representation currently encodes
 * raw 32 bit JMEM_ALIGNMENT aligned pointers on 32 bit systems.
 * This can be extended to encode a 32 bit offset from a heap
 * base on 64 bit systems in the future. There are no plans
 * to support more than 4G address space for JerryScript.
 */

/**
 * Compressed pointer
 */
#ifdef JERRY_CPOINTER_32_BIT
typedef uint32_t jmem_cpointer_t;
#else /* !JERRY_CPOINTER_32_BIT */
typedef uint16_t jmem_cpointer_t;
#endif /* JERRY_CPOINTER_32_BIT */

/**
 * Severity of a 'try give memory back' request
 *
 * The request are posted sequentially beginning from
 * low to high until enough memory is freed.
 *
 * If not enough memory is freed upon a high request
 * then the engine is shut down with ERR_OUT_OF_MEMORY.
 */
typedef enum
{
  JMEM_FREE_UNUSED_MEMORY_SEVERITY_LOW,  /**< 'low' severity */
  JMEM_FREE_UNUSED_MEMORY_SEVERITY_HIGH, /**< 'high' severity */
} jmem_free_unused_memory_severity_t;

/**
 * Node for free chunk list
 */
typedef struct jmem_pools_chunk_t
{
  struct jmem_pools_chunk_t *next_p; /**< pointer to next pool chunk */
} jmem_pools_chunk_t;

/**
 *  Free region node
 */
typedef struct
{
  uint32_t next_offset; /**< Offset of next region in list */
  uint32_t size; /**< Size of region */
} jmem_heap_free_t;

void jmem_init (void);
void jmem_finalize (void);

void *jmem_heap_alloc_block (const size_t size);
void *jmem_heap_alloc_block_null_on_error (const size_t size);
void jmem_heap_free_block (void *ptr, const size_t size);

#ifdef JMEM_TRACK_ALLOCATION_SIZES
size_t jmem_heap_allocation_size (const void *ptr);
#endif /* JMEM_TRACK_ALLOCATION_SIZES */

#ifdef JMEM_STATS
/**
 * Heap memory usage statistics
 */
typedef struct
{
  size_t size; /**< heap total size */

  size_t allocated_bytes; /**< currently allocated bytes */
  size_t peak_allocated_bytes; /**< peak allocated bytes */

  size_t waste_bytes; /**< bytes waste due to blocks filled partially */
  size_t peak_waste_bytes; /**< peak wasted bytes */

  size_t byte_code_bytes; /**< allocated memory for byte code */
  size_t peak_byte_code_bytes; /**< peak allocated memory for byte code */

  size_t string_bytes; /**< allocated memory for strings */
  size_t peak_string_bytes; /**< peak allocated memory for strings */

  size_t object_bytes; /**< allocated memory for objects */
  size_t peak_object_bytes; /**< peak allocated memory for objects */

  size_t property_bytes; /**< allocated memory for properties */
  size_t peak_property_bytes; /**< peak allocated memory for properties */

  size_t skip_count; /**< Number of skip-aheads during insertion of free block */
  size_t nonskip_count; /**< Number of times we could not skip ahead during
                         *   free block insertion */

  size_t alloc_count; /**< number of memory allocations */
  size_t free_count; /**< number of memory frees */
  size_t alloc_iter_count; /**< Number of iterations required for allocations */
  size_t free_iter_count; /**< Number of iterations required for inserting free blocks */
} jmem_heap_stats_t;

void jmem_stats_allocate_byte_code_bytes (size_t property_size);
void jmem_stats_free_byte_code_bytes (size_t property_size);
void jmem_stats_allocate_string_bytes (size_t string_size);
void jmem_stats_free_string_bytes (size_t string_size);
void jmem_stats_allocate_object_bytes (size_t object_size);
void jmem_stats_free_object_bytes (size_t string_size);
void jmem_stats_allocate_property_bytes (size_t property_size);
void jmem_stats_free_property_bytes (size_t property_size);

void jmem_heap_get_stats (jmem_heap_stats_t *);
#endif /* JMEM_STATS */

jmem_cpointer_t JERRY_ATTR_PURE jmem_compress_pointer (const void *pointer_p);
void * JERRY_ATTR_PURE jmem_decompress_pointer (uintptr_t compressed_pointer);

/**
 * A free memory callback routine type.
 */
typedef void (*jmem_free_unused_memory_callback_t) (jmem_free_unused_memory_severity_t);

void jmem_register_free_unused_memory_callback (jmem_free_unused_memory_callback_t callback);
void jmem_unregister_free_unused_memory_callback (jmem_free_unused_memory_callback_t callback);

void jmem_run_free_unused_memory_callbacks (jmem_free_unused_memory_severity_t severity);

/**
 * Define a local array variable and allocate memory for the array on the heap.
 *
 * If requested number of elements is zero, assign NULL to the variable.
 *
 * Warning:
 *         if there is not enough memory on the heap, shutdown engine with ERR_OUT_OF_MEMORY.
 */
#define JMEM_DEFINE_LOCAL_ARRAY(var_name, number, type) \
{ \
  size_t var_name ## ___size = (size_t) (number) * sizeof (type); \
  type *var_name = (type *) (jmem_heap_alloc_block (var_name ## ___size));

/**
 * Free the previously defined local array variable, freeing corresponding block on the heap,
 * if it was allocated (i.e. if the array's size was non-zero).
 */
#define JMEM_FINALIZE_LOCAL_ARRAY(var_name) \
  if (var_name != NULL) \
  { \
    JERRY_ASSERT (var_name ## ___size != 0); \
    \
    jmem_heap_free_block (var_name, var_name ## ___size); \
  } \
  else \
  { \
    JERRY_ASSERT (var_name ## ___size == 0); \
  } \
}

/**
 * Get value of pointer from specified non-null compressed pointer value
 */
#define JMEM_CP_GET_NON_NULL_POINTER(type, cp_value) \
  ((type *) (jmem_decompress_pointer (cp_value)))

/**
 * Get value of pointer from specified compressed pointer value
 */
#define JMEM_CP_GET_POINTER(type, cp_value) \
  (((JERRY_UNLIKELY ((cp_value) == JMEM_CP_NULL)) ? NULL : JMEM_CP_GET_NON_NULL_POINTER (type, cp_value)))

/**
 * Set value of non-null compressed pointer so that it will correspond
 * to specified non_compressed_pointer
 */
#define JMEM_CP_SET_NON_NULL_POINTER(cp_value, non_compressed_pointer) \
  (cp_value) = jmem_compress_pointer (non_compressed_pointer)

/**
 * Set value of compressed pointer so that it will correspond
 * to specified non_compressed_pointer
 */
#define JMEM_CP_SET_POINTER(cp_value, non_compressed_pointer) \
  do \
  { \
    void *ptr_value = (void *) non_compressed_pointer; \
    \
    if (JERRY_UNLIKELY ((ptr_value) == NULL)) \
    { \
      (cp_value) = JMEM_CP_NULL; \
    } \
    else \
    { \
      JMEM_CP_SET_NON_NULL_POINTER (cp_value, ptr_value); \
    } \
  } while (false);

/**
 * @}
 * \addtogroup poolman Memory pool manager
 * @{
 */

void *jmem_pools_alloc (size_t size);
void jmem_pools_free (void *chunk_p, size_t size);

/**
 * @}
 * @}
 */

#endif /* !JMEM_H */

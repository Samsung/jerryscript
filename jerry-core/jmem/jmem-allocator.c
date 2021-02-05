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

/**
 * Allocator implementation
 */
#include "ecma-globals.h"
#include "jcontext.h"
#include "jmem.h"
#include "jrt-libc-includes.h"

#define JMEM_ALLOCATOR_INTERNAL
#include "jmem-allocator-internal.h"

#if JERRY_MEM_STATS
/**
 * Register byte code allocation.
 */
void
jmem_stats_allocate_byte_code_bytes (size_t byte_code_size)
{
  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  heap_stats->byte_code_bytes += byte_code_size;

  if (heap_stats->byte_code_bytes >= heap_stats->peak_byte_code_bytes)
  {
    heap_stats->peak_byte_code_bytes = heap_stats->byte_code_bytes;
  }
} /* jmem_stats_allocate_byte_code_bytes */

/**
 * Register byte code free.
 */
void
jmem_stats_free_byte_code_bytes (size_t byte_code_size)
{
  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  JERRY_ASSERT (heap_stats->byte_code_bytes >= byte_code_size);

  heap_stats->byte_code_bytes -= byte_code_size;
} /* jmem_stats_free_byte_code_bytes */

/**
 * Register string allocation.
 */
void
jmem_stats_allocate_string_bytes (size_t string_size)
{
  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  heap_stats->string_bytes += string_size;

  if (heap_stats->string_bytes >= heap_stats->peak_string_bytes)
  {
    heap_stats->peak_string_bytes = heap_stats->string_bytes;
  }
} /* jmem_stats_allocate_string_bytes */

/**
 * Register string free.
 */
void
jmem_stats_free_string_bytes (size_t string_size)
{
  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  JERRY_ASSERT (heap_stats->string_bytes >= string_size);

  heap_stats->string_bytes -= string_size;
} /* jmem_stats_free_string_bytes */

/**
 * Register object allocation.
 */
void
jmem_stats_allocate_object_bytes (size_t object_size)
{
  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  heap_stats->object_bytes += object_size;

  if (heap_stats->object_bytes >= heap_stats->peak_object_bytes)
  {
    heap_stats->peak_object_bytes = heap_stats->object_bytes;
  }
} /* jmem_stats_allocate_object_bytes */

/**
 * Register object free.
 */
void
jmem_stats_free_object_bytes (size_t object_size)
{
  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  JERRY_ASSERT (heap_stats->object_bytes >= object_size);

  heap_stats->object_bytes -= object_size;
} /* jmem_stats_free_object_bytes */

/**
 * Register property allocation.
 */
void
jmem_stats_allocate_property_bytes (size_t property_size)
{
  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  heap_stats->property_bytes += property_size;

  if (heap_stats->property_bytes >= heap_stats->peak_property_bytes)
  {
    heap_stats->peak_property_bytes = heap_stats->property_bytes;
  }
} /* jmem_stats_allocate_property_bytes */

/**
 * Register property free.
 */
void
jmem_stats_free_property_bytes (size_t property_size)
{
  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  JERRY_ASSERT (heap_stats->property_bytes >= property_size);

  heap_stats->property_bytes -= property_size;
} /* jmem_stats_free_property_bytes */

#endif /* JERRY_MEM_STATS */

/**
 * Initialize memory allocators.
 */
void
jmem_init (void)
{
  jmem_heap_init ();
} /* jmem_init */

/**
 * Finalize memory allocators.
 */
void
jmem_finalize (void)
{
  jmem_pools_finalize ();

#if JERRY_MEM_STATS
  if (JERRY_CONTEXT (jerry_init_flags) & ECMA_INIT_MEM_STATS)
  {
    jmem_heap_stats_print ();
  }
#endif /* JERRY_MEM_STATS */

  jmem_heap_finalize ();
} /* jmem_finalize */

/**
 * Compress pointer
 *
 * @return packed pointer
 */
extern inline jmem_cpointer_t JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
jmem_compress_pointer (const void *pointer_p) /**< pointer to compress */
{
  JERRY_ASSERT (pointer_p != NULL);
  JERRY_ASSERT (jmem_is_heap_pointer (pointer_p));

  uintptr_t uint_ptr = (uintptr_t) pointer_p;

  JERRY_ASSERT (uint_ptr % JMEM_ALIGNMENT == 0);

#if defined(ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY) && JERRY_CPOINTER_32_BIT
  JERRY_ASSERT (((jmem_cpointer_t) uint_ptr) == uint_ptr);
#else /* !ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY || !JERRY_CPOINTER_32_BIT */
  const uintptr_t heap_start = (uintptr_t) &JERRY_HEAP_CONTEXT (first);

  uint_ptr -= heap_start;
  uint_ptr >>= JMEM_ALIGNMENT_LOG;

#if JERRY_CPOINTER_32_BIT
  JERRY_ASSERT (uint_ptr <= UINT32_MAX);
#else /* !JERRY_CPOINTER_32_BIT */
  JERRY_ASSERT (uint_ptr <= UINT16_MAX);
#endif /* JERRY_CPOINTER_32_BIT */
  JERRY_ASSERT (uint_ptr != JMEM_CP_NULL);
#endif /* ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY && JERRY_CPOINTER_32_BIT */

  return (jmem_cpointer_t) uint_ptr;
} /* jmem_compress_pointer */

/**
 * Decompress pointer
 *
 * @return unpacked pointer
 */
extern inline void *JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
jmem_decompress_pointer (uintptr_t compressed_pointer) /**< pointer to decompress */
{
  JERRY_ASSERT (compressed_pointer != JMEM_CP_NULL);

  uintptr_t uint_ptr = compressed_pointer;

  JERRY_ASSERT (((jmem_cpointer_t) uint_ptr) == uint_ptr);

#if defined(ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY) && JERRY_CPOINTER_32_BIT
  JERRY_ASSERT (uint_ptr % JMEM_ALIGNMENT == 0);
#else /* !ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY || !JERRY_CPOINTER_32_BIT */
  const uintptr_t heap_start = (uintptr_t) &JERRY_HEAP_CONTEXT (first);

  uint_ptr <<= JMEM_ALIGNMENT_LOG;
  uint_ptr += heap_start;

  JERRY_ASSERT (jmem_is_heap_pointer ((void *) uint_ptr));
#endif /* ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY && JERRY_CPOINTER_32_BIT */

  return (void *) uint_ptr;
} /* jmem_decompress_pointer */

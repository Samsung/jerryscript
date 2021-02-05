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
 * Memory pool manager implementation
 */

#include "jcontext.h"
#include "jmem.h"
#include "jrt-libc-includes.h"

#define JMEM_ALLOCATOR_INTERNAL
#include "jmem-allocator-internal.h"

#if JERRY_MEM_GC_BEFORE_EACH_ALLOC
#include "ecma-gc.h"
#endif /* JERRY_MEM_GC_BEFORE_EACH_ALLOC */

/** \addtogroup mem Memory allocation
 * @{
 *
 * \addtogroup poolman Memory pool manager
 * @{
 */

/**
 * Finalize pool manager
 */
void
jmem_pools_finalize (void)
{
  jmem_pools_collect_empty ();

  JERRY_ASSERT (JERRY_CONTEXT (jmem_free_8_byte_chunk_p) == NULL);
#if JERRY_CPOINTER_32_BIT
  JERRY_ASSERT (JERRY_CONTEXT (jmem_free_16_byte_chunk_p) == NULL);
#endif /* JERRY_CPOINTER_32_BIT */
} /* jmem_pools_finalize */

/**
 * Allocate a chunk of specified size
 *
 * @return pointer to allocated chunk, if allocation was successful,
 *         or NULL - if not enough memory.
 */
extern inline void *JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_pools_alloc (size_t size) /**< size of the chunk */
{
#if JERRY_MEM_GC_BEFORE_EACH_ALLOC
  ecma_gc_run ();
#endif /* JERRY_MEM_GC_BEFORE_EACH_ALLOC */

#if JERRY_CPOINTER_32_BIT
  if (size <= 8)
  {
#else /* !JERRY_CPOINTER_32_BIT */
  JERRY_ASSERT (size <= 8);
#endif /* JERRY_CPOINTER_32_BIT */

    if (JERRY_CONTEXT (jmem_free_8_byte_chunk_p) != NULL)
    {
      const jmem_pools_chunk_t *const chunk_p = JERRY_CONTEXT (jmem_free_8_byte_chunk_p);

      JMEM_VALGRIND_DEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));
      JERRY_CONTEXT (jmem_free_8_byte_chunk_p) = chunk_p->next_p;
      JMEM_VALGRIND_UNDEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));

      JMEM_HEAP_STAT_ALLOC (8);
      return (void *) chunk_p;
    }
    else
    {
      void *chunk_p = jmem_heap_alloc_block_internal (8);
      JMEM_HEAP_STAT_ALLOC (8);
      return chunk_p;
    }

#if JERRY_CPOINTER_32_BIT
  }

  JERRY_ASSERT (size <= 16);

  if (JERRY_CONTEXT (jmem_free_16_byte_chunk_p) != NULL)
  {
    const jmem_pools_chunk_t *const chunk_p = JERRY_CONTEXT (jmem_free_16_byte_chunk_p);

    JMEM_VALGRIND_DEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));
    JERRY_CONTEXT (jmem_free_16_byte_chunk_p) = chunk_p->next_p;
    JMEM_VALGRIND_UNDEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));

    JMEM_HEAP_STAT_ALLOC (16);
    return (void *) chunk_p;
  }
  else
  {
    void *chunk_p = jmem_heap_alloc_block_internal (16);
    JMEM_HEAP_STAT_ALLOC (16);
    return chunk_p;
  }
#endif /* JERRY_CPOINTER_32_BIT */
} /* jmem_pools_alloc */

/**
 * Free the chunk
 */
extern inline void JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_pools_free (void *chunk_p, /**< pointer to the chunk */
                 size_t size) /**< size of the chunk */
{
  JERRY_ASSERT (chunk_p != NULL);
  JMEM_HEAP_STAT_FREE (size);

  jmem_pools_chunk_t *const chunk_to_free_p = (jmem_pools_chunk_t *) chunk_p;

  JMEM_VALGRIND_DEFINED_SPACE (chunk_to_free_p, size);

#if JERRY_CPOINTER_32_BIT
  if (size <= 8)
  {
#else /* !JERRY_CPOINTER_32_BIT */
  JERRY_ASSERT (size <= 8);
#endif /* JERRY_CPOINTER_32_BIT */

    chunk_to_free_p->next_p = JERRY_CONTEXT (jmem_free_8_byte_chunk_p);
    JERRY_CONTEXT (jmem_free_8_byte_chunk_p) = chunk_to_free_p;

#if JERRY_CPOINTER_32_BIT
  }
  else
  {
    JERRY_ASSERT (size <= 16);

    chunk_to_free_p->next_p = JERRY_CONTEXT (jmem_free_16_byte_chunk_p);
    JERRY_CONTEXT (jmem_free_16_byte_chunk_p) = chunk_to_free_p;
  }
#endif /* JERRY_CPOINTER_32_BIT */

  JMEM_VALGRIND_NOACCESS_SPACE (chunk_to_free_p, size);
} /* jmem_pools_free */

/**
 *  Collect empty pool chunks
 */
void
jmem_pools_collect_empty (void)
{
  jmem_pools_chunk_t *chunk_p = JERRY_CONTEXT (jmem_free_8_byte_chunk_p);
  JERRY_CONTEXT (jmem_free_8_byte_chunk_p) = NULL;

  while (chunk_p)
  {
    JMEM_VALGRIND_DEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));
    jmem_pools_chunk_t *const next_p = chunk_p->next_p;
    JMEM_VALGRIND_NOACCESS_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));

    jmem_heap_free_block_internal (chunk_p, 8);
    chunk_p = next_p;
  }

#if JERRY_CPOINTER_32_BIT
  chunk_p = JERRY_CONTEXT (jmem_free_16_byte_chunk_p);
  JERRY_CONTEXT (jmem_free_16_byte_chunk_p) = NULL;

  while (chunk_p)
  {
    JMEM_VALGRIND_DEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));
    jmem_pools_chunk_t *const next_p = chunk_p->next_p;
    JMEM_VALGRIND_NOACCESS_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));

    jmem_heap_free_block_internal (chunk_p, 16);
    chunk_p = next_p;
  }
#endif /* JERRY_CPOINTER_32_BIT */
} /* jmem_pools_collect_empty */

/**
 * @}
 * @}
 */

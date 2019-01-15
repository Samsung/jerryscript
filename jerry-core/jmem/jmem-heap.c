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
 * Heap implementation
 */

#include "jcontext.h"
#include "jmem.h"
#include "jrt-bit-fields.h"
#include "jrt-libc-includes.h"

#define JMEM_ALLOCATOR_INTERNAL
#include "jmem-allocator-internal.h"

/** \addtogroup mem Memory allocation
 * @{
 *
 * \addtogroup heap Heap
 * @{
 */

#ifndef JERRY_SYSTEM_ALLOCATOR
/**
 * End of list marker.
 */
#define JMEM_HEAP_END_OF_LIST ((uint32_t) 0xffffffff)

/**
 * @{
 */
#ifdef ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY
/* In this case we simply store the pointer, since it fits anyway. */
#define JMEM_HEAP_GET_OFFSET_FROM_ADDR(p) ((uint32_t) (p))
#define JMEM_HEAP_GET_ADDR_FROM_OFFSET(u) ((jmem_heap_free_t *) (u))
#else /* !ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */
#define JMEM_HEAP_GET_OFFSET_FROM_ADDR(p) ((uint32_t) ((uint8_t *) (p) - JERRY_HEAP_CONTEXT (area)))
#define JMEM_HEAP_GET_ADDR_FROM_OFFSET(u) ((jmem_heap_free_t *) (JERRY_HEAP_CONTEXT (area) + (u)))
#endif /* ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */
/**
 * @}
 */

/**
 * Get end of region
 *
 * @return pointer to the end of the region
 */
static inline jmem_heap_free_t *  JERRY_ATTR_ALWAYS_INLINE JERRY_ATTR_PURE
jmem_heap_get_region_end (jmem_heap_free_t *curr_p) /**< current region */
{
  return (jmem_heap_free_t *)((uint8_t *) curr_p + curr_p->size);
} /* jmem_heap_get_region_end */
#endif /* !JERRY_SYSTEM_ALLOCATOR */

/**
 * @{
 * JMEM_HEAP_STAT_xxx definitions
 */
#ifdef JMEM_STATS
static void jmem_heap_stat_init (void);
static void jmem_heap_stat_alloc (size_t num);
static void jmem_heap_stat_free (size_t num);

#define JMEM_HEAP_STAT_INIT() jmem_heap_stat_init ()
#define JMEM_HEAP_STAT_ALLOC(v1) jmem_heap_stat_alloc (v1)
#define JMEM_HEAP_STAT_FREE(v1) jmem_heap_stat_free (v1)

#ifndef JERRY_SYSTEM_ALLOCATOR
static void jmem_heap_stat_skip (void);
static void jmem_heap_stat_nonskip (void);
static void jmem_heap_stat_alloc_iter (void);
static void jmem_heap_stat_free_iter (void);

#define JMEM_HEAP_STAT_SKIP() jmem_heap_stat_skip ()
#define JMEM_HEAP_STAT_NONSKIP() jmem_heap_stat_nonskip ()
#define JMEM_HEAP_STAT_ALLOC_ITER() jmem_heap_stat_alloc_iter ()
#define JMEM_HEAP_STAT_FREE_ITER() jmem_heap_stat_free_iter ()
#endif /* !JERRY_SYSTEM_ALLOCATOR */
#else /* !JMEM_STATS */
#define JMEM_HEAP_STAT_INIT()
#define JMEM_HEAP_STAT_ALLOC(v1) JERRY_UNUSED (v1)
#define JMEM_HEAP_STAT_FREE(v1) JERRY_UNUSED (v1)

#ifndef JERRY_SYSTEM_ALLOCATOR
#define JMEM_HEAP_STAT_SKIP()
#define JMEM_HEAP_STAT_NONSKIP()
#define JMEM_HEAP_STAT_ALLOC_ITER()
#define JMEM_HEAP_STAT_FREE_ITER()
#endif /* !JERRY_SYSTEM_ALLOCATOR */
#endif /* JMEM_STATS */
/** @} */

/**
 * @{
 * JMEM_HEAP_BITMAP_xxx definitions
 */
#ifdef JMEM_TRACK_ALLOCATION_SIZES
# define JMEM_HEAP_BITMAP_IDX(ptr) ((uintptr_t)(ptr) - (uintptr_t)&JERRY_HEAP_CONTEXT (first)) / JMEM_ALIGNMENT
# define JMEM_HEAP_BITMAP_BYTE_IDX(ptr) JMEM_HEAP_BITMAP_IDX (ptr) / sizeof(*JERRY_CONTEXT (jmem_heap_bitmap))
# define JMEM_HEAP_BITMAP_BIT_IDX(ptr) JMEM_HEAP_BITMAP_IDX (ptr) % sizeof(*JERRY_CONTEXT (jmem_heap_bitmap))
# define JMEM_HEAP_BITMAP_BYTE(ptr) (JERRY_CONTEXT (jmem_heap_bitmap)[JMEM_HEAP_BITMAP_BYTE_IDX (ptr)])
# define JMEM_HEAP_BITMAP_BITMASK(ptr) (1 << JMEM_HEAP_BITMAP_BIT_IDX (ptr))

# define JMEM_HEAP_BITMAP_SET(ptr) JMEM_HEAP_BITMAP_BYTE (ptr) |= JMEM_HEAP_BITMAP_BITMASK (ptr)
# define JMEM_HEAP_BITMAP_CLEAR(ptr) JMEM_HEAP_BITMAP_BYTE (ptr) &= (uint8_t)(~JMEM_HEAP_BITMAP_BITMASK (ptr))
# define JMEM_HEAP_BITMAP_GET(ptr) (!!(JMEM_HEAP_BITMAP_BYTE (ptr) & JMEM_HEAP_BITMAP_BITMASK (ptr)))
#else /* !JMEM_TRACK_ALLOCATION_SIZES */
# define JMEM_HEAP_BITMAP_SET(ptr)
# define JMEM_HEAP_BITMAP_CLEAR(ptr)
#endif /* JMEM_TRACK_ALLOCATION_SIZES */
/** @} */

/**
 * Startup initialization of heap
 */
void
jmem_heap_init (void)
{
#ifndef JERRY_SYSTEM_ALLOCATOR
#ifndef JERRY_CPOINTER_32_BIT
  /* the maximum heap size for 16bit compressed pointers should be 512K */
  JERRY_ASSERT (((UINT16_MAX + 1) << JMEM_ALIGNMENT_LOG) >= JMEM_HEAP_SIZE);
#endif /* !JERRY_CPOINTER_32_BIT */
  JERRY_ASSERT ((uintptr_t) JERRY_HEAP_CONTEXT (area) % JMEM_ALIGNMENT == 0);

  JERRY_CONTEXT (jmem_heap_limit) = CONFIG_MEM_HEAP_DESIRED_LIMIT;

  jmem_heap_free_t *const region_p = (jmem_heap_free_t *) JERRY_HEAP_CONTEXT (area);

  region_p->size = JMEM_HEAP_AREA_SIZE;
  region_p->next_offset = JMEM_HEAP_END_OF_LIST;

  JERRY_HEAP_CONTEXT (first).size = 0;
  JERRY_HEAP_CONTEXT (first).next_offset = JMEM_HEAP_GET_OFFSET_FROM_ADDR (region_p);

  JERRY_CONTEXT (jmem_heap_list_skip_p) = &JERRY_HEAP_CONTEXT (first);

  JMEM_VALGRIND_NOACCESS_SPACE (JERRY_HEAP_CONTEXT (area), JMEM_HEAP_AREA_SIZE);

#endif /* !JERRY_SYSTEM_ALLOCATOR */

#ifdef JMEM_TRACK_ALLOCATION_SIZES
  memset (JERRY_CONTEXT (jmem_heap_bitmap), 0, sizeof (JERRY_CONTEXT (jmem_heap_bitmap)));
#endif /* JMEM_TRACK_ALLOCATION_SIZES */

  JMEM_HEAP_STAT_INIT ();
} /* jmem_heap_init */

/**
 * Finalize heap
 */
void
jmem_heap_finalize (void)
{
  JERRY_ASSERT (JERRY_CONTEXT (jmem_heap_allocated_size) == 0);
#ifndef JERRY_SYSTEM_ALLOCATOR
  JMEM_VALGRIND_NOACCESS_SPACE (&JERRY_HEAP_CONTEXT (first), JMEM_HEAP_SIZE);
#endif /* !JERRY_SYSTEM_ALLOCATOR */
} /* jmem_heap_finalize */

/**
 * Allocation of memory region.
 *
 * See also:
 *          jmem_heap_alloc_block
 *
 * @return pointer to allocated memory block - if allocation is successful,
 *         NULL - if there is not enough memory.
 */
static void * JERRY_ATTR_HOT
jmem_heap_alloc_block_internal (const size_t size) /**< size of requested block */
{
#ifndef JERRY_SYSTEM_ALLOCATOR
  /* Align size. */
  const size_t required_size = ((size + JMEM_ALIGNMENT - 1) / JMEM_ALIGNMENT) * JMEM_ALIGNMENT;
  jmem_heap_free_t *data_space_p = NULL;

  JMEM_VALGRIND_DEFINED_SPACE (&JERRY_HEAP_CONTEXT (first), sizeof (jmem_heap_free_t));

  /* Fast path for 8 byte chunks, first region is guaranteed to be sufficient. */
  if (required_size == JMEM_ALIGNMENT
      && JERRY_LIKELY (JERRY_HEAP_CONTEXT (first).next_offset != JMEM_HEAP_END_OF_LIST))
  {
    data_space_p = JMEM_HEAP_GET_ADDR_FROM_OFFSET (JERRY_HEAP_CONTEXT (first).next_offset);
    JERRY_ASSERT (jmem_is_heap_pointer (data_space_p));

    JMEM_VALGRIND_DEFINED_SPACE (data_space_p, sizeof (jmem_heap_free_t));
    JERRY_CONTEXT (jmem_heap_allocated_size) += JMEM_ALIGNMENT;
    JMEM_HEAP_STAT_ALLOC_ITER ();

    if (data_space_p->size == JMEM_ALIGNMENT)
    {
      JERRY_HEAP_CONTEXT (first).next_offset = data_space_p->next_offset;
    }
    else
    {
      JERRY_ASSERT (data_space_p->size > JMEM_ALIGNMENT);

      jmem_heap_free_t *remaining_p;
      remaining_p = JMEM_HEAP_GET_ADDR_FROM_OFFSET (JERRY_HEAP_CONTEXT (first).next_offset) + 1;

      JMEM_VALGRIND_DEFINED_SPACE (remaining_p, sizeof (jmem_heap_free_t));
      remaining_p->size = data_space_p->size - JMEM_ALIGNMENT;
      remaining_p->next_offset = data_space_p->next_offset;
      JMEM_VALGRIND_NOACCESS_SPACE (remaining_p, sizeof (jmem_heap_free_t));

      JERRY_HEAP_CONTEXT (first).next_offset = JMEM_HEAP_GET_OFFSET_FROM_ADDR (remaining_p);
    }

    JMEM_VALGRIND_UNDEFINED_SPACE (data_space_p, sizeof (jmem_heap_free_t));

    if (JERRY_UNLIKELY (data_space_p == JERRY_CONTEXT (jmem_heap_list_skip_p)))
    {
      JERRY_CONTEXT (jmem_heap_list_skip_p) = JMEM_HEAP_GET_ADDR_FROM_OFFSET (JERRY_HEAP_CONTEXT (first).next_offset);
    }
  }
  /* Slow path for larger regions. */
  else
  {
    uint32_t current_offset = JERRY_HEAP_CONTEXT (first).next_offset;
    jmem_heap_free_t *prev_p = &JERRY_HEAP_CONTEXT (first);

    while (current_offset != JMEM_HEAP_END_OF_LIST)
    {
      jmem_heap_free_t *current_p = JMEM_HEAP_GET_ADDR_FROM_OFFSET (current_offset);
      JERRY_ASSERT (jmem_is_heap_pointer (current_p));
      JMEM_VALGRIND_DEFINED_SPACE (current_p, sizeof (jmem_heap_free_t));
      JMEM_HEAP_STAT_ALLOC_ITER ();

      const uint32_t next_offset = current_p->next_offset;
      JERRY_ASSERT (next_offset == JMEM_HEAP_END_OF_LIST
                    || jmem_is_heap_pointer (JMEM_HEAP_GET_ADDR_FROM_OFFSET (next_offset)));

      if (current_p->size >= required_size)
      {
        /* Region is sufficiently big, store address. */
        data_space_p = current_p;
        JERRY_CONTEXT (jmem_heap_allocated_size) += required_size;

        /* Region was larger than necessary. */
        if (current_p->size > required_size)
        {
          /* Get address of remaining space. */
          jmem_heap_free_t *const remaining_p = (jmem_heap_free_t *) ((uint8_t *) current_p + required_size);

          /* Update metadata. */
          JMEM_VALGRIND_DEFINED_SPACE (remaining_p, sizeof (jmem_heap_free_t));
          remaining_p->size = current_p->size - (uint32_t) required_size;
          remaining_p->next_offset = next_offset;
          JMEM_VALGRIND_NOACCESS_SPACE (remaining_p, sizeof (jmem_heap_free_t));

          /* Update list. */
          JMEM_VALGRIND_DEFINED_SPACE (prev_p, sizeof (jmem_heap_free_t));
          prev_p->next_offset = JMEM_HEAP_GET_OFFSET_FROM_ADDR (remaining_p);
          JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
        }
        /* Block is an exact fit. */
        else
        {
          /* Remove the region from the list. */
          JMEM_VALGRIND_DEFINED_SPACE (prev_p, sizeof (jmem_heap_free_t));
          prev_p->next_offset = next_offset;
          JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
        }

        JERRY_CONTEXT (jmem_heap_list_skip_p) = prev_p;

        /* Found enough space. */
        break;
      }

      JMEM_VALGRIND_NOACCESS_SPACE (current_p, sizeof (jmem_heap_free_t));
      /* Next in list. */
      prev_p = current_p;
      current_offset = next_offset;
    }
  }

  while (JERRY_CONTEXT (jmem_heap_allocated_size) >= JERRY_CONTEXT (jmem_heap_limit))
  {
    JERRY_CONTEXT (jmem_heap_limit) += CONFIG_MEM_HEAP_DESIRED_LIMIT;
  }

  JMEM_VALGRIND_NOACCESS_SPACE (&JERRY_HEAP_CONTEXT (first), sizeof (jmem_heap_free_t));

  if (JERRY_UNLIKELY (!data_space_p))
  {
    return NULL;
  }

  JERRY_ASSERT ((uintptr_t) data_space_p % JMEM_ALIGNMENT == 0);
  JMEM_VALGRIND_UNDEFINED_SPACE (data_space_p, size);
  JMEM_HEAP_STAT_ALLOC (size);
  JMEM_HEAP_BITMAP_SET (((uintptr_t) data_space_p) + required_size);

  return (void *) data_space_p;
#else /* JERRY_SYSTEM_ALLOCATOR */
  JMEM_HEAP_STAT_ALLOC (size);
  return malloc (size);
#endif /* !JERRY_SYSTEM_ALLOCATOR */
} /* jmem_heap_alloc_block_internal */

/**
 * Allocation of memory block, running 'try to give memory back' callbacks, if there is not enough memory.
 *
 * Note:
 *      if there is still not enough memory after running the callbacks
 *        - NULL value will be returned if parmeter 'ret_null_on_error' is true
 *        - the engine will terminate with ERR_OUT_OF_MEMORY if 'ret_null_on_error' is false
 *
 * @return NULL, if the required memory size is 0
 *         also NULL, if 'ret_null_on_error' is true and the allocation fails because of there is not enough memory
 */
static void *
jmem_heap_gc_and_alloc_block (const size_t size,      /**< required memory size */
                              bool ret_null_on_error) /**< indicates whether return null or terminate
                                                           with ERR_OUT_OF_MEMORY on out of memory */
{
  if (JERRY_UNLIKELY (size == 0))
  {
    return NULL;
  }

#ifdef JMEM_GC_BEFORE_EACH_ALLOC
  jmem_run_free_unused_memory_callbacks (JMEM_FREE_UNUSED_MEMORY_SEVERITY_HIGH);
#endif /* JMEM_GC_BEFORE_EACH_ALLOC */

  if (JERRY_CONTEXT (jmem_heap_allocated_size) + size >= JERRY_CONTEXT (jmem_heap_limit))
  {
    jmem_run_free_unused_memory_callbacks (JMEM_FREE_UNUSED_MEMORY_SEVERITY_LOW);
  }

  void *data_space_p = jmem_heap_alloc_block_internal (size);

  if (JERRY_LIKELY (data_space_p != NULL))
  {
    JMEM_VALGRIND_MALLOCLIKE_SPACE (data_space_p, size);
    return data_space_p;
  }

  for (jmem_free_unused_memory_severity_t severity = JMEM_FREE_UNUSED_MEMORY_SEVERITY_LOW;
       severity <= JMEM_FREE_UNUSED_MEMORY_SEVERITY_HIGH;
       severity = (jmem_free_unused_memory_severity_t) (severity + 1))
  {
    jmem_run_free_unused_memory_callbacks (severity);

    data_space_p = jmem_heap_alloc_block_internal (size);

    if (JERRY_LIKELY (data_space_p != NULL))
    {
      JMEM_VALGRIND_MALLOCLIKE_SPACE (data_space_p, size);
      return data_space_p;
    }
  }

  JERRY_ASSERT (data_space_p == NULL);

  if (!ret_null_on_error)
  {
    jerry_fatal (ERR_OUT_OF_MEMORY);
  }

  return data_space_p;
} /* jmem_heap_gc_and_alloc_block */

/**
 * Allocation of memory block, running 'try to give memory back' callbacks, if there is not enough memory.
 *
 * Note:
 *      If there is still not enough memory after running the callbacks, then the engine will be
 *      terminated with ERR_OUT_OF_MEMORY.
 *
 * @return NULL, if the required memory is 0
 *         pointer to allocated memory block, otherwise
 */
inline void * JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_heap_alloc_block (const size_t size)  /**< required memory size */
{
  return jmem_heap_gc_and_alloc_block (size, false);
} /* jmem_heap_alloc_block */

/**
 * Allocation of memory block, running 'try to give memory back' callbacks, if there is not enough memory.
 *
 * Note:
 *      If there is still not enough memory after running the callbacks, NULL will be returned.
 *
 * @return NULL, if the required memory size is 0
 *         also NULL, if the allocation has failed
 *         pointer to the allocated memory block, otherwise
 */
inline void * JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_heap_alloc_block_null_on_error (const size_t size) /**< required memory size */
{
  return jmem_heap_gc_and_alloc_block (size, true);
} /* jmem_heap_alloc_block_null_on_error */

/**
 * Free the memory block.
 */
void JERRY_ATTR_HOT
jmem_heap_free_block (void *ptr, /**< pointer to beginning of data space of the block */
                      const size_t size) /**< size of allocated region */
{
#ifndef JERRY_SYSTEM_ALLOCATOR
  /* checking that ptr points to the heap */
  JERRY_ASSERT (jmem_is_heap_pointer (ptr));
  JERRY_ASSERT (size > 0);
  JERRY_ASSERT (JERRY_CONTEXT (jmem_heap_limit) >= JERRY_CONTEXT (jmem_heap_allocated_size));

  JMEM_VALGRIND_FREELIKE_SPACE (ptr);
  JMEM_VALGRIND_NOACCESS_SPACE (ptr, size);
  JMEM_HEAP_STAT_FREE_ITER ();

  jmem_heap_free_t *block_p = (jmem_heap_free_t *) ptr;
  jmem_heap_free_t *prev_p;
  jmem_heap_free_t *next_p;

  JMEM_VALGRIND_DEFINED_SPACE (&JERRY_HEAP_CONTEXT (first), sizeof (jmem_heap_free_t));

  if (block_p > JERRY_CONTEXT (jmem_heap_list_skip_p))
  {
    prev_p = JERRY_CONTEXT (jmem_heap_list_skip_p);
    JMEM_HEAP_STAT_SKIP ();
  }
  else
  {
    prev_p = &JERRY_HEAP_CONTEXT (first);
    JMEM_HEAP_STAT_NONSKIP ();
  }

  JERRY_ASSERT (jmem_is_heap_pointer (block_p));
  const uint32_t block_offset = JMEM_HEAP_GET_OFFSET_FROM_ADDR (block_p);

  JMEM_VALGRIND_DEFINED_SPACE (prev_p, sizeof (jmem_heap_free_t));
  /* Find position of region in the list. */
  while (prev_p->next_offset < block_offset)
  {
    next_p = JMEM_HEAP_GET_ADDR_FROM_OFFSET (prev_p->next_offset);
    JERRY_ASSERT (jmem_is_heap_pointer (next_p));

    JMEM_VALGRIND_DEFINED_SPACE (next_p, sizeof (jmem_heap_free_t));
    JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
    prev_p = next_p;

    JMEM_HEAP_STAT_FREE_ITER ();
  }

  next_p = JMEM_HEAP_GET_ADDR_FROM_OFFSET (prev_p->next_offset);
  JMEM_VALGRIND_DEFINED_SPACE (next_p, sizeof (jmem_heap_free_t));

  /* Realign size */
  const size_t aligned_size = (size + JMEM_ALIGNMENT - 1) / JMEM_ALIGNMENT * JMEM_ALIGNMENT;

  JMEM_VALGRIND_DEFINED_SPACE (block_p, sizeof (jmem_heap_free_t));
  JMEM_VALGRIND_DEFINED_SPACE (prev_p, sizeof (jmem_heap_free_t));
  /* Update prev. */
  if (jmem_heap_get_region_end (prev_p) == block_p)
  {
    /* Can be merged. */
    prev_p->size += (uint32_t) aligned_size;
    JMEM_VALGRIND_NOACCESS_SPACE (block_p, sizeof (jmem_heap_free_t));
    block_p = prev_p;
  }
  else
  {
    block_p->size = (uint32_t) aligned_size;
    prev_p->next_offset = block_offset;
  }

  JMEM_VALGRIND_DEFINED_SPACE (next_p, sizeof (jmem_heap_free_t));
  /* Update next. */
  if (jmem_heap_get_region_end (block_p) == next_p)
  {
    /* Can be merged. */
    block_p->size += next_p->size;
    block_p->next_offset = next_p->next_offset;
  }
  else
  {
    block_p->next_offset = JMEM_HEAP_GET_OFFSET_FROM_ADDR (next_p);
  }

  JERRY_CONTEXT (jmem_heap_list_skip_p) = prev_p;

  JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
  JMEM_VALGRIND_NOACCESS_SPACE (block_p, size);
  JMEM_VALGRIND_NOACCESS_SPACE (next_p, sizeof (jmem_heap_free_t));

  JERRY_ASSERT (JERRY_CONTEXT (jmem_heap_allocated_size) > 0);
  JERRY_CONTEXT (jmem_heap_allocated_size) -= aligned_size;

  while (JERRY_CONTEXT (jmem_heap_allocated_size) + CONFIG_MEM_HEAP_DESIRED_LIMIT <= JERRY_CONTEXT (jmem_heap_limit))
  {
    JERRY_CONTEXT (jmem_heap_limit) -= CONFIG_MEM_HEAP_DESIRED_LIMIT;
  }

  JMEM_VALGRIND_NOACCESS_SPACE (&JERRY_HEAP_CONTEXT (first), sizeof (jmem_heap_free_t));
  JERRY_ASSERT (JERRY_CONTEXT (jmem_heap_limit) >= JERRY_CONTEXT (jmem_heap_allocated_size));
  JMEM_HEAP_STAT_FREE (size);
  JMEM_HEAP_BITMAP_CLEAR ((uintptr_t) ptr + aligned_size);
#else /* JERRY_SYSTEM_ALLOCATOR */
  JMEM_HEAP_STAT_FREE (size);
  free (ptr);
#endif /* !JERRY_SYSTEM_ALLOCATOR */
} /* jmem_heap_free_block */

#ifndef JERRY_NDEBUG
/**
 * Check whether the pointer points to the heap
 *
 * Note:
 *      the routine should be used only for assertion checks
 *
 * @return true - if pointer points to the heap,
 *         false - otherwise
 */
bool
jmem_is_heap_pointer (const void *pointer) /**< pointer */
{
#ifndef JERRY_SYSTEM_ALLOCATOR
  return ((uint8_t *) pointer >= JERRY_HEAP_CONTEXT (area)
          && (uint8_t *) pointer <= (JERRY_HEAP_CONTEXT (area) + JMEM_HEAP_AREA_SIZE));
#else /* JERRY_SYSTEM_ALLOCATOR */
  JERRY_UNUSED (pointer);
  return true;
#endif /* !JERRY_SYSTEM_ALLOCATOR */
} /* jmem_is_heap_pointer */
#endif /* !JERRY_NDEBUG */

#ifdef JMEM_STATS
/**
 * Get heap memory usage statistics
 */
void
jmem_heap_get_stats (jmem_heap_stats_t *out_heap_stats_p) /**< [out] heap stats */
{
  JERRY_ASSERT (out_heap_stats_p != NULL);

  *out_heap_stats_p = JERRY_CONTEXT (jmem_heap_stats);
} /* jmem_heap_get_stats */

/**
 * Print heap memory usage statistics
 */
void
jmem_heap_stats_print (void)
{
  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  JERRY_DEBUG_MSG ("Heap stats:\n");
#ifndef JERRY_SYSTEM_ALLOCATOR
  JERRY_DEBUG_MSG ("  Heap size = %zu bytes\n",
                   heap_stats->size);
#endif /* !JERRY_SYSTEM_ALLOCATOR */
  JERRY_DEBUG_MSG ("  Allocated = %zu bytes\n"
                   "  Peak allocated = %zu bytes\n"
                   "  Waste = %zu bytes\n"
                   "  Peak waste = %zu bytes\n"
                   "  Allocated byte code data = %zu bytes\n"
                   "  Peak allocated byte code data = %zu bytes\n"
                   "  Allocated string data = %zu bytes\n"
                   "  Peak allocated string data = %zu bytes\n"
                   "  Allocated object data = %zu bytes\n"
                   "  Peak allocated object data = %zu bytes\n"
                   "  Allocated property data = %zu bytes\n"
                   "  Peak allocated property data = %zu bytes\n",
                   heap_stats->allocated_bytes,
                   heap_stats->peak_allocated_bytes,
                   heap_stats->waste_bytes,
                   heap_stats->peak_waste_bytes,
                   heap_stats->byte_code_bytes,
                   heap_stats->peak_byte_code_bytes,
                   heap_stats->string_bytes,
                   heap_stats->peak_string_bytes,
                   heap_stats->object_bytes,
                   heap_stats->peak_object_bytes,
                   heap_stats->property_bytes,
                   heap_stats->peak_property_bytes);
#ifndef JERRY_SYSTEM_ALLOCATOR
  JERRY_DEBUG_MSG ("  Skip-ahead ratio = %zu.%04zu\n"
                   "  Average alloc iteration = %zu.%04zu\n"
                   "  Average free iteration = %zu.%04zu\n",
                   heap_stats->skip_count / heap_stats->nonskip_count,
                   heap_stats->skip_count % heap_stats->nonskip_count * 10000 / heap_stats->nonskip_count,
                   heap_stats->alloc_iter_count / heap_stats->alloc_count,
                   heap_stats->alloc_iter_count % heap_stats->alloc_count * 10000 / heap_stats->alloc_count,
                   heap_stats->free_iter_count / heap_stats->free_count,
                   heap_stats->free_iter_count % heap_stats->free_count * 10000 / heap_stats->free_count);
#endif /* !JERRY_SYSTEM_ALLOCATOR */
} /* jmem_heap_stats_print */

/**
 * Initalize heap memory usage statistics account structure
 */
static void
jmem_heap_stat_init (void)
{
#ifndef JERRY_SYSTEM_ALLOCATOR
  JERRY_CONTEXT (jmem_heap_stats).size = JMEM_HEAP_AREA_SIZE;
#endif /* !JERRY_SYSTEM_ALLOCATOR */
} /* jmem_heap_stat_init */

/**
 * Account allocation
 */
static void
jmem_heap_stat_alloc (size_t size) /**< Size of allocated block */
{
  const size_t aligned_size = (size + JMEM_ALIGNMENT - 1) / JMEM_ALIGNMENT * JMEM_ALIGNMENT;
  const size_t waste_bytes = aligned_size - size;

  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  heap_stats->allocated_bytes += aligned_size;
  heap_stats->waste_bytes += waste_bytes;
  heap_stats->alloc_count++;

  if (heap_stats->allocated_bytes > heap_stats->peak_allocated_bytes)
  {
    heap_stats->peak_allocated_bytes = heap_stats->allocated_bytes;
  }

  if (heap_stats->waste_bytes > heap_stats->peak_waste_bytes)
  {
    heap_stats->peak_waste_bytes = heap_stats->waste_bytes;
  }
} /* jmem_heap_stat_alloc */

/**
 * Account freeing
 */
static void
jmem_heap_stat_free (size_t size) /**< Size of freed block */
{
  const size_t aligned_size = (size + JMEM_ALIGNMENT - 1) / JMEM_ALIGNMENT * JMEM_ALIGNMENT;
  const size_t waste_bytes = aligned_size - size;

  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  heap_stats->free_count++;
  heap_stats->allocated_bytes -= aligned_size;
  heap_stats->waste_bytes -= waste_bytes;
} /* jmem_heap_stat_free */

#ifndef JERRY_SYSTEM_ALLOCATOR
/**
 * Counts number of skip-aheads during insertion of free block
 */
static void
jmem_heap_stat_skip (void)
{
  JERRY_CONTEXT (jmem_heap_stats).skip_count++;
} /* jmem_heap_stat_skip  */

/**
 * Counts number of times we could not skip ahead during free block insertion
 */
static void
jmem_heap_stat_nonskip (void)
{
  JERRY_CONTEXT (jmem_heap_stats).nonskip_count++;
} /* jmem_heap_stat_nonskip */

/**
 * Count number of iterations required for allocations
 */
static void
jmem_heap_stat_alloc_iter (void)
{
  JERRY_CONTEXT (jmem_heap_stats).alloc_iter_count++;
} /* jmem_heap_stat_alloc_iter */

/**
 * Counts number of iterations required for inserting free blocks
 */
static void
jmem_heap_stat_free_iter (void)
{
  JERRY_CONTEXT (jmem_heap_stats).free_iter_count++;
} /* jmem_heap_stat_free_iter */
#endif /* !JERRY_SYSTEM_ALLOCATOR */
#endif /* JMEM_STATS */

#ifdef JMEM_TRACK_ALLOCATION_SIZES
/**
 * Returns the size of the specified heap allocation.
 *
 * @return byte size of allocation on heap.
 */
size_t
jmem_heap_allocation_size (const void *pointer) /**< Address of allocation */
{
  size_t size = 0;

  if ((uint8_t *) pointer < JERRY_HEAP_CONTEXT (area))
  {
    return 0;
  }

  do
  {
    size += JMEM_ALIGNMENT;
    pointer = (void *) ((uintptr_t) pointer + JMEM_ALIGNMENT);
  }
  while ((uint8_t *) pointer <= (JERRY_HEAP_CONTEXT (area) + JMEM_HEAP_AREA_SIZE) &&
         !JMEM_HEAP_BITMAP_GET (pointer));

  return size;
} /* jmem_heap_allocation_size */
#endif /* JMEM_TRACK_ALLOCATION_SIZES */

/**
 * @}
 * @}
 */

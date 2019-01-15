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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "jmem.h"

#define JMEM_ALLOCATOR_INTERNAL
#include "jmem-allocator-internal.h"

#include "test-common.h"

/* Heap size is 32K. */
#define test_heap_size (32 * 1024)

/* Iterations count. */
#define test_iters (4 * 1024)

/* Subiterations count. */
#define test_sub_iters 32

/* Threshold size of block to allocate. */
#define test_threshold_block_size 8192

uint8_t *ptrs[test_sub_iters];
size_t sizes[test_sub_iters];
bool is_one_chunked[test_sub_iters];

static void
test_heap_give_some_memory_back (jmem_free_unused_memory_severity_t severity)
{
  int p;

  if (severity == JMEM_FREE_UNUSED_MEMORY_SEVERITY_LOW)
  {
    p = 8;
  }
  else
  {
    TEST_ASSERT (severity == JMEM_FREE_UNUSED_MEMORY_SEVERITY_HIGH);

    p = 1;
  }

  for (int i = 0; i < test_sub_iters; i++)
  {
    if (rand () % p == 0)
    {
      if (ptrs[i] != NULL)
      {
        for (size_t k = 0; k < sizes[i]; k++)
        {
          TEST_ASSERT (ptrs[i][k] == 0);
        }

        jmem_heap_free_block (ptrs[i], sizes[i]);
        ptrs[i] = NULL;
      }
    }
  }
} /* test_heap_give_some_memory_back */

int
main (void)
{
  TEST_INIT ();

  jmem_heap_init ();

  jmem_register_free_unused_memory_callback (test_heap_give_some_memory_back);

#ifdef JMEM_STATS
  // TODO: uncomment it after we solve the divide zero issue.
  // jmem_heap_stats_print ();
#endif /* JMEM_STATS */

  for (uint32_t i = 0; i < test_iters; i++)
  {
    for (uint32_t j = 0; j < test_sub_iters; j++)
    {
      size_t size = (size_t) rand () % test_threshold_block_size;
      ptrs[j] = (uint8_t *) jmem_heap_alloc_block (size);
      sizes[j] = size;

      TEST_ASSERT (sizes[j] == 0 || ptrs[j] != NULL);
#ifdef JMEM_TRACK_ALLOCATION_SIZES
      TEST_ASSERT ((jmem_heap_allocation_size (ptrs[j]) - sizes[j]) < JMEM_ALIGNMENT);
#endif /* JMEM_TRACK_ALLOCATION_SIZES */
      memset (ptrs[j], 0, sizes[j]);
    }

    /* jmem_heap_print (true); */

    for (uint32_t j = 0; j < test_sub_iters; j++)
    {
      if (ptrs[j] != NULL)
      {
        for (size_t k = 0; k < sizes[j]; k++)
        {
          TEST_ASSERT (ptrs[j][k] == 0);
        }

        jmem_heap_free_block (ptrs[j], sizes[j]);

        ptrs[j] = NULL;
      }
    }
  }

#ifdef JMEM_STATS
  jmem_heap_stats_print ();
#endif /* JMEM_STATS */

  return 0;
} /* main */

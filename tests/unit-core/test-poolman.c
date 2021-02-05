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
 * Unit test for pool manager.
 */

#include "jmem.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define JMEM_ALLOCATOR_INTERNAL
#include "jmem-allocator-internal.h"
#include "test-common.h"

/* Iterations count. */
const uint32_t test_iters = 1024;

/* Subiterations count. */
#define TEST_MAX_SUB_ITERS 1024
#define TEST_CHUNK_SIZE    8

uint8_t *ptrs[TEST_MAX_SUB_ITERS];
uint8_t data[TEST_MAX_SUB_ITERS][TEST_CHUNK_SIZE];

int
main (void)
{
  TEST_INIT ();

  jmem_init ();

  for (uint32_t i = 0; i < test_iters; i++)
  {
    const size_t subiters = ((size_t) rand () % TEST_MAX_SUB_ITERS) + 1;

    for (size_t j = 0; j < subiters; j++)
    {
      ptrs[j] = (uint8_t *) jmem_pools_alloc (TEST_CHUNK_SIZE);

      if (ptrs[j] != NULL)
      {
        for (size_t k = 0; k < TEST_CHUNK_SIZE; k++)
        {
          ptrs[j][k] = (uint8_t) (rand () % 256);
        }

        memcpy (data[j], ptrs[j], TEST_CHUNK_SIZE);
      }
    }

    /* jmem_heap_print (false); */

    for (size_t j = 0; j < subiters; j++)
    {
      if (rand () % 256 == 0)
      {
        jmem_pools_collect_empty ();
      }

      if (ptrs[j] != NULL)
      {
        TEST_ASSERT (!memcmp (data[j], ptrs[j], TEST_CHUNK_SIZE));

        jmem_pools_free (ptrs[j], TEST_CHUNK_SIZE);
      }
    }
  }

#ifdef JMEM_STATS
  jmem_heap_stats_print ();
#endif /* JMEM_STATS */

  jmem_finalize ();

  return 0;
} /* main */

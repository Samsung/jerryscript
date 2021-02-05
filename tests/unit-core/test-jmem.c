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

#include "ecma-init-finalize.h"
#include "jmem.h"
#include "test-common.h"

#define BASIC_SIZE (64)

int
main (void)
{
  TEST_INIT ();

  jmem_init ();
  ecma_init ();

  {
    uint8_t *block1_p = (uint8_t *) jmem_heap_alloc_block (BASIC_SIZE);
    uint8_t *block2_p = (uint8_t *) jmem_heap_alloc_block (BASIC_SIZE);
    uint8_t *block3_p = (uint8_t *) jmem_heap_alloc_block (BASIC_SIZE);

    /* [block1 64] [block2 64] [block3 64] [...] */

    for (uint8_t i = 0; i < BASIC_SIZE; i++)
    {
      block2_p[i] = i;
    }

    /* Realloc by moving */
    block2_p = jmem_heap_realloc_block (block2_p, BASIC_SIZE, BASIC_SIZE * 2);

    /* [block1 64] [free 64] [block3 64] [block2 128] [...] */

    for (uint8_t i = 0; i < BASIC_SIZE; i++)
    {
      TEST_ASSERT (block2_p[i] == i);
    }

    for (uint8_t i = BASIC_SIZE; i < BASIC_SIZE * 2; i++)
    {
      block2_p[i] = i;
    }

    uint8_t *block4_p = (uint8_t *) jmem_heap_alloc_block (BASIC_SIZE * 2);

    /* [block1 64] [free 64] [block3 64] [block2 128] [block4 128] [...] */

    jmem_heap_free_block (block3_p, BASIC_SIZE);

    /* [block1 64] [free 128] [block2 128] [block4 128] [...] */

    /* Realloc by extending front */
    block2_p = (uint8_t *) jmem_heap_realloc_block (block2_p, BASIC_SIZE * 2, BASIC_SIZE * 3);

    /* [block1 64] [free 64] [block2 192] [block4 128] [...] */

    for (uint8_t i = 0; i < BASIC_SIZE * 2; i++)
    {
      TEST_ASSERT (block2_p[i] == i);
    }

    /* Shrink */
    block2_p = (uint8_t *) jmem_heap_realloc_block (block2_p, BASIC_SIZE * 3, BASIC_SIZE);

    /* [block1 64] [free 64] [block2 64] [free 128] [block4 128] [...] */

    for (uint8_t i = 0; i < BASIC_SIZE; i++)
    {
      TEST_ASSERT (block2_p[i] == i);
    }

    for (uint8_t i = 0; i < BASIC_SIZE; i++)
    {
      block1_p[i] = i;
    }

    /* Grow in place */
    block1_p = (uint8_t *) jmem_heap_realloc_block (block1_p, BASIC_SIZE, BASIC_SIZE * 2);

    /* [block1 128] [block2 64] [free 128] [block4 128] [...] */

    for (uint8_t i = 0; i < BASIC_SIZE; i++)
    {
      TEST_ASSERT (block1_p[i] == i);
    }

    jmem_heap_free_block (block1_p, BASIC_SIZE * 2);
    jmem_heap_free_block (block2_p, BASIC_SIZE);
    jmem_heap_free_block (block4_p, BASIC_SIZE * 2);
  }

  ecma_finalize ();
  jmem_finalize ();

  return 0;
} /* main */

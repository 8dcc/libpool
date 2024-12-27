/*
 * Copyright 2024 8dcc
 *
 * This program is part of libpool, a tiny (ANSI) C library for pool allocation.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* NOTE: Remember to change this path if you move the header */
#include "libpool.h"

#if defined(LIBPOOL_NO_STDLIB)
PoolAllocFuncPtr pool_ext_alloc = NULL;
PoolFreeFuncPtr pool_ext_free   = NULL;
#else
#include <stdlib.h>
PoolAllocFuncPtr pool_ext_alloc = malloc;
PoolFreeFuncPtr pool_ext_free   = free;
#endif /* LIBPOOL_NO_STDLIB */

/*----------------------------------------------------------------------------*/

/*
 * Linked list of pointers, used to store the start of the chunk arrays inside a
 * pool.
 *
 * We need to store them as a linked list, since there can be an arbitrary
 * number of them, one for each call to `pool_resize' plus the initial one from
 * `pool_new'. New pointers will be prepended to the linked list.
 */
typedef struct LinkedPtr LinkedPtr;
struct LinkedPtr {
    LinkedPtr* next;
    void* ptr;
};

/*
 * The actual pool structure, which contains a pointer to the first chunk, and
 * a pointer to the start of the linked list of free chunks.
 *
 * We need to store the first chunk for freeing the actual `Chunk' array once
 * the user is done with the pool.
 *
 * The user is able to allocate with O(1) time, because the `Pool.free_chunk'
 * pointer always points to a free chunk without needing to iterate anything.
 */
struct Pool {
    void* free_chunk;
    LinkedPtr* array_starts;
    size_t chunk_sz;
};

/*----------------------------------------------------------------------------*/

/*
 * We use an exteran allocation function (by default `malloc', but can be
 * overwritten by user) to allocate a `Pool' structure, and the array of
 * chunks. You can think of a chunk as the following union:
 *
 *     union Chunk {
 *         union Chunk* next_free;
 *         char user_data[CHUNK_SZ];
 *     };
 *
 * In this hypothetical union, the data in a non-free chunk will be overwritten
 * by the user, in the `user_data' array, where `CHUNK_SZ' was specified by the
 * caller of `pool_new'. However, if the chunk is free, the union uses the
 * `Chunk.next_free' pointer to build a linked list of available chunks, shown
 * below.
 *
 * This is explained in more detail (and with diagrams) in my blog article:
 * https://8dcc.github.io/programming/pool-allocator.html
 */
Pool* pool_new(size_t pool_sz, size_t chunk_sz) {
    Pool* pool;
    char* arr;
    size_t i;

    if (pool_sz == 0 || chunk_sz < sizeof(void*))
        return NULL;

    pool = pool_ext_alloc(sizeof(Pool));
    if (pool == NULL)
        return NULL;

    pool->array_starts = malloc(sizeof(LinkedPtr));
    if (pool->array_starts == NULL) {
        pool_ext_free(pool);
        return NULL;
    }

    arr = pool_ext_alloc(pool_sz * chunk_sz);
    if (arr == NULL) {
        pool_ext_free(pool->array_starts);
        pool_ext_free(pool);
        return NULL;
    }

    /*
     * Build the linked list. Use the first N bytes of the free chunks for
     * storing the (hypothetical) `.next' pointer. This is why `chunk_sz' must
     * be greater or equal than `sizeof(void*)'.
     */
    for (i = 0; i < pool_sz - 1; i++)
        *(void**)(arr + i * chunk_sz) = arr + (i + 1) * chunk_sz;
    *(void**)(arr + (pool_sz - 1) * chunk_sz) = NULL;

    pool->free_chunk         = arr;
    pool->array_starts->next = NULL;
    pool->array_starts->ptr  = arr;
    pool->chunk_sz           = chunk_sz;

    return pool;
}

/*
 * Resizing the pool simply means allocating a new chunk array, and prepending
 * it to the `pool->free_chunk' linked list.
 *
 * 1. Allocate a new `LinkedPtr' structure.
 * 2. Allocate a new chunk array with the specified size.
 * 3. Link the new array together, just like in `pool_new'.
 * 4. Prepend the new chunk array to the existing linked list of free chunks.
 * 5. Prepend the new `LinkedPtr' to the existing linked list of array starts.
 */
bool pool_resize(Pool* pool, size_t extra_chunk_num) {
    LinkedPtr* array_start;
    char* extra_arr;
    size_t i;

    if (pool == NULL || extra_chunk_num <= 0)
        return false;

    array_start = pool_ext_alloc(sizeof(LinkedPtr));
    if (array_start == NULL)
        return false;

    extra_arr = pool_ext_alloc(extra_chunk_num * pool->chunk_sz);
    if (extra_arr == NULL) {
        pool_ext_free(array_start);
        return false;
    }

    for (i = 0; i < extra_chunk_num - 1; i++)
        *(void**)(extra_arr + i * pool->chunk_sz) =
          extra_arr + (i + 1) * pool->chunk_sz;

    *(void**)(extra_arr + (extra_chunk_num - 1) * pool->chunk_sz) =
      pool->free_chunk;
    pool->free_chunk = extra_arr;

    array_start->ptr   = extra_arr;
    array_start->next  = pool->array_starts;
    pool->array_starts = array_start;

    return true;
}

/*
 * When closing the pool, we traverse the list of `LinkedPtr' structures, which
 * contain the base address of each chunk array. We free the array, and then the
 * `LinkedPtr' structure itself. Lastly, we free the `Pool' structure.
 */
void pool_close(Pool* pool) {
    LinkedPtr* linkedptr;
    LinkedPtr* next;

    if (pool == NULL)
        return;

    linkedptr = pool->array_starts;
    while (linkedptr != NULL) {
        next = linkedptr->next;
        pool_ext_free(linkedptr->ptr);
        pool_ext_free(linkedptr);
        linkedptr = next;
    }

    pool_ext_free(pool);
}

/*----------------------------------------------------------------------------*/

/*
 * The allocation process is very simple and fast. Since the `pool' has a
 * pointer to the start of a linked list of free (hypothetical) `Chunk'
 * structures, we can just return that pointer, and set the new start of the
 * linked list to the second item of the old list.
 */
void* pool_alloc(Pool* pool) {
    void* result;

    if (pool == NULL || pool->free_chunk == NULL)
        return NULL;

    result           = pool->free_chunk;
    pool->free_chunk = *(void**)pool->free_chunk;
    return result;
}

/*
 * Note that, since we are using a linked list, the caller doesn't need to free
 * in the same order that used when allocating.
 */
void pool_free(Pool* pool, void* ptr) {
    if (pool == NULL || ptr == NULL)
        return;

    *(void**)ptr     = pool->free_chunk;
    pool->free_chunk = ptr;
}

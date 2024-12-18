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
    void* chunk_arr;
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
 * caller of `pool_new':
 *
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *   | <user-data> |  | <user-data> |  | <user-data> |  | <user-data> |
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *
 * However, if the chunk is free, the union uses the `Chunk.next_free' pointer
 * to build a linked list of available chunks, shown below. This linked list is
 * built once, inside this `pool_new' function, and it is the only time when the
 * library has to iterate the `Chunk' array. The linked list will be modified by
 * `pool_alloc' and `pool_free', in O(1) time.
 *
 * Therefore, after the linked list is built, a pointer to the first `Chunk' is
 * stored inside the `Pool.free_chunk' member:
 *
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *   | * |         |  | * |         |  | * |         |  | X |         |
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *   ^ |              ^ |              ^ |              ^
 *   | '--------------' '--------------' '--------------'
 *   |
 *   '-- (Pool.free_chunk)
 *
 * Where '*' represents a valid pointer and 'X' represents NULL (the end of the
 * list). For more information on how this `free_chunk' pointer is used, see
 * `pool_alloc'.
 */
Pool* pool_new(size_t pool_sz, size_t chunk_sz) {
    Pool* pool;
    char* chunk_arr;
    size_t i;

    if (pool_sz == 0 || chunk_sz < sizeof(void*))
        return NULL;

    pool = pool_ext_alloc(sizeof(Pool));
    if (pool == NULL)
        return NULL;

    pool->chunk_arr = pool->free_chunk = pool_ext_alloc(pool_sz * chunk_sz);
    if (pool->chunk_arr == NULL) {
        pool_ext_free(pool);
        return NULL;
    }

    /*
     * Build the linked list. Use the first N bytes of the free chunks for
     * storing the (hypothetical) `.next' pointer. This is why `chunk_sz' must
     * be greater or equal than `sizeof(void*)'.
     */
    chunk_arr = pool->chunk_arr;
    for (i = 0; i < pool_sz - 1; i++)
        *(void**)(chunk_arr + i * chunk_sz) = chunk_arr + (i + 1) * chunk_sz;
    *(void**)(chunk_arr + (pool_sz - 1) * chunk_sz) = NULL;

    return pool;
}

void pool_close(Pool* pool) {
    if (pool == NULL)
        return;

    pool_ext_free(pool->chunk_arr);
    pool_ext_free(pool);
}

/*----------------------------------------------------------------------------*/

/*
 * The allocation process is very simple and fast. Since the `pool' has a
 * pointer to the start of a linked list of free (hypothetical) `Chunk'
 * structures, we can just return that pointer, and set the new start of the
 * linked list to the second item of the old list. For example, this is how the
 * linked list would look before the allocation:
 *
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *   | * |         |  | * |         |  | * |         |  | X |         |
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *   ^ |              ^ |              ^ |              ^
 *   | '--------------' '--------------' '--------------'
 *   |
 *   '-- (pool.free_chunk)
 *
 * And this is how it would look after the allocation:
 *
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *   | <user-data> |  | * |         |  | * |         |  | X |         |
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *                    ^ |              ^ |              ^
 *                    | '--------------' '--------------'
 *                    |
 *                    '-- (pool.free_chunk)
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
 * in the same order that used when allocating. For example, this is how the
 * linked list would look before the free:
 *
 *   (A)              (B)              (C)              (D)
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *   | <user-data> |  | <user-data> |  | * |         |  | X |         |
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *                                     ^ |              ^
 *                                     | '--------------'
 *                                     |
 *                                     '-- (pool.free_chunk)
 *
 * And this is how it would look after freeing chunk A:
 *
 *   (A)              (B)              (C)              (D)
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *   | * |         |  | <user-data> |  | * |         |  | X |         |
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *   ^ |                               ^ |              ^
 *   | '-------------------------------' '--------------'
 *   |
 *   '-- (pool.free_chunk)
 *
 * Note how chunk B remains unchanged. If we wanted to free it, we would just
 * have to set chunk A as the (hypothetical) `.next' pointer of chunk B.
 */
void pool_free(Pool* pool, void* ptr) {
    if (pool == NULL || ptr == NULL)
        return;

    *(void**)ptr     = pool->free_chunk;
    pool->free_chunk = ptr;
}

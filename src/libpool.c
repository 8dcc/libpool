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
PoolAllocFuncPtr pool_ext_alloc   = NULL;
PoolFreeFuncPtr pool_ext_free     = NULL;
PoolMemcpyFuncPtr pool_ext_memcpy = NULL;
#else
#include <stdlib.h>
#include <string.h>
PoolAllocFuncPtr pool_ext_alloc   = malloc;
PoolFreeFuncPtr pool_ext_free     = free;
PoolMemcpyFuncPtr pool_ext_memcpy = memcpy;
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
    size_t pool_sz;
    size_t chunk_sz;
};

/*----------------------------------------------------------------------------*/

/*
 * We use an exteran allocation function (by default `malloc', but can be
 * overwritten by user) to allocate a `Pool' structure, and the array of
 * chunks. You can think of a chunk as the following structure:
 *
 *     struct Chunk {
 *         union {
 *             struct Chunk* next_free;
 *             char user_data[CHUNK_SZ];
 *         } val;
 *     };
 *
 * In this hypothetical struct, the data in a non-free chunk will be overwritten
 * by the user, in the `user_data' array, where `CHUNK_SZ' was specified by the
 * caller of `pool_new':
 *
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *   | <user-data> |  | <user-data> |  | <user-data> |  | <user-data> |
 *   +-------------+  +-------------+  +-------------+  +-------------+
 *
 * However, if the chunk is free, the structure uses the `val.next_free' pointer
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

    pool->chunk_arr = pool_ext_alloc(pool_sz * chunk_sz);
    if (pool->chunk_arr == NULL) {
        pool_ext_free(pool);
        return NULL;
    }

    pool->free_chunk = pool->chunk_arr;
    pool->pool_sz    = pool_sz;
    pool->chunk_sz   = chunk_sz;

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

/*
 * Assume this is the original array, from `pool.chunk_arr':
 *
 *   +--------+  +--------+  +--------+  +--------+
 *   | * |    |  | <data> |  | * |    |  | X |    |
 *   +--------+  +--------+  +--------+  +--------+
 *   ^ |                     ^ |         ^
 *   | '---------------------' '---------'
 *   |
 *   '-- (pool.free_chunk)
 *
 * After allocating the new array, assuming the user increased the pool size by
 * two, we get:
 *
 *   +--------+  +--------+  +--------+  +--------+  +--------+  +--------+
 *   | ? |    |  | <data> |  | ? |    |  | X |    |  |        |  |        |
 *   +--------+  +--------+  +--------+  +--------+  +--------+  +--------+
 *
 * Where '?' represents invalid pointers, that still have addresses from the old
 * array.
 *
 * Now we should link the new free chunks we allocated, in this case the last
 * two:
 *
 *   (A)         (B)         (C)         (D)         (E)         (F)
 *   +--------+  +--------+  +--------+  +--------+  +--------+  +--------+
 *   | ? |    |  | <data> |  | ? |    |  | X |    |  | * |    |  | X |    |
 *   +--------+  +--------+  +--------+  +--------+  +--------+  +--------+
 *                                                     |         ^
 *                                                     '---------'
 *
 * Now we need to check if the old pool had free chunks left. If it didn't, we
 * just set `pool.free_chunk' to the new linked list we just created (in the
 * previous example, chunk F).
 *
 * However, if the old list had free chunks, we need to update the old pointers
 * of the linked list:
 *
 *   +--------+  +--------+  +--------+  +--------+  +--------+  +--------+
 *   | * |    |  | <data> |  | * |    |  | X |    |  | * |    |  | X |    |
 *   +--------+  +--------+  +--------+  +--------+  +--------+  +--------+
 *     |                     ^ |         ^            |          ^
 *     '---------------------' '---------'            '----------'
 *
 * And append the new chunks to the end of the linked list of free chunks. We
 * also update `pool.free_chunk' to the new array:
 *
 *   +--------+  +--------+  +--------+  +--------+  +--------+  +--------+
 *   | * |    |  | <data> |  | * |    |  | * |    |  | * |    |  | X |    |
 *   +--------+  +--------+  +--------+  +--------+  +--------+  +--------+
 *   ^ |                     ^ |         ^ |         ^ |         ^
 *   | '---------------------' '---------' '---------' '---------'
 *   |
 *   '-- (pool.free_chunk)
 *
 * Finally, we free the old chunk array, and update the array pointer inside the
 * `Pool' structure.
 */
bool pool_resize(Pool* pool, size_t new_pool_sz) {
    char* new_arr;
    char* cur_free;
    void* first_new_chunk;
    size_t i, off_cur, off_next;
    const size_t old_byte_size = pool->pool_sz * pool->chunk_sz;

    if (new_pool_sz < pool->pool_sz)
        return false;
    else if (new_pool_sz == pool->pool_sz)
        return true;

    new_arr = pool_ext_alloc(new_pool_sz * pool->chunk_sz);
    if (new_arr == NULL)
        return false;

    pool_ext_memcpy(new_arr, pool->chunk_arr, pool->pool_sz * pool->chunk_sz);

    for (i = pool->pool_sz; i < new_pool_sz - 1; i++)
        *(void**)(new_arr + i * pool->chunk_sz) =
          new_arr + (i + 1) * pool->chunk_sz;
    *(void**)(new_arr + (new_pool_sz - 1) * pool->chunk_sz) = NULL;

    first_new_chunk = new_arr + old_byte_size;

    if (pool->free_chunk == NULL) {
        pool->free_chunk = first_new_chunk;
    } else {
        for (cur_free = pool->free_chunk; *(void**)cur_free != NULL;
             cur_free = *(void**)cur_free) {
            off_cur = (uintptr_t)cur_free - (uintptr_t)pool->chunk_arr;
            off_next =
              (uintptr_t)(*(void**)cur_free) - (uintptr_t)pool->chunk_arr;
            *(void**)(new_arr + off_cur) = (void*)(new_arr + off_next);
        }

        *(void**)(new_arr + off_next) = first_new_chunk;

        off_cur = (uintptr_t)pool->free_chunk - (uintptr_t)pool->chunk_arr;
        pool->free_chunk = new_arr + off_cur;
    }

    pool_ext_free(pool->chunk_arr);
    pool->chunk_arr = new_arr;

    return true;
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

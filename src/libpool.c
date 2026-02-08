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

#include "libpool.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * External allocation functions.
 */
#if defined(LIBPOOL_NO_STDLIB)
PoolAllocFuncPtr pool_ext_alloc = NULL;
PoolFreeFuncPtr pool_ext_free   = NULL;
#else /* !defined(LIBPOOL_NO_STDLIB) */
#include <stdlib.h>
PoolAllocFuncPtr pool_ext_alloc = malloc;
PoolFreeFuncPtr pool_ext_free   = free;
#endif /* !defined(LIBPOOL_NO_STDLIB) */

/*
 * External multithreading functions.
 */
#if defined(LIBPOOL_THREAD_SAFE)
#if defined(LIBPOOL_NO_STDLIB)
PoolMutexInitFuncPtr pool_ext_mutex_init       = NULL;
PoolMutexLockFuncPtr pool_ext_mutex_lock       = NULL;
PoolMutexUnlockFuncPtr pool_ext_mutex_unlock   = NULL;
PoolMutexDestroyFuncPtr pool_ext_mutex_destroy = NULL;
#else /* !defined(LIBPOOL_NO_STDLIB) */
#include <pthread.h>
static bool pool_ext_mutex_init_impl(pool_ext_mutex_t* mutex) {
    return pthread_mutex_init(mutex, NULL) == 0;
}
static bool pool_ext_mutex_lock_impl(pool_ext_mutex_t* mutex) {
    return pthread_mutex_lock(mutex) == 0;
}
static bool pool_ext_mutex_unlock_impl(pool_ext_mutex_t* mutex) {
    return pthread_mutex_unlock(mutex) == 0;
}
static bool pool_ext_mutex_destroy_impl(pool_ext_mutex_t* mutex) {
    return pthread_mutex_destroy(mutex) == 0;
}
PoolMutexInitFuncPtr pool_ext_mutex_init       = pool_ext_mutex_init_impl;
PoolMutexLockFuncPtr pool_ext_mutex_lock       = pool_ext_mutex_lock_impl;
PoolMutexUnlockFuncPtr pool_ext_mutex_unlock   = pool_ext_mutex_unlock_impl;
PoolMutexDestroyFuncPtr pool_ext_mutex_destroy = pool_ext_mutex_destroy_impl;
#endif /* !defined(LIBPOOL_NO_STDLIB) */
#endif /* defined(LIBPOOL_THEAD_SAFE) */

/*
 * External valgrind macros.
 */
#if defined(LIBPOOL_NO_VALGRIND)
#define VALGRIND_CREATE_MEMPOOL(A, B, C) ((void)0)
#define VALGRIND_DESTROY_MEMPOOL(A)      ((void)0)
#define VALGRIND_MEMPOOL_ALLOC(A, B, C)  ((void)0)
#define VALGRIND_MEMPOOL_FREE(A, B)      ((void)0)
#define VALGRIND_MAKE_MEM_DEFINED(A, B)  ((void)0)
#define VALGRIND_MAKE_MEM_NOACCESS(A, B) ((void)0)
#else /* !defined(LIBPOOL_NO_VALGRIND) */
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>
#endif /* !defined(LIBPOOL_NO_VALGRIND) */

/*
 * Alignment macros.
 *
 * If alignment is enabled, the 'ALIGN2BOUNDARY' macro will align the specified
 * size to the specified boundary. For more information, see:
 * https://8dcc.github.io/reversing/challenge10.html#c-translation
 */
#if defined(LIBPOOL_NO_ALIGNMENT)
#define ALIGN2BOUNDARY(ADDR, BOUND) (ADDR)
#else /* !defined(LIBPOOL_NO_ALIGNMENT) */
#define ALIGN2BOUNDARY(SIZE, BOUNDARY)                                         \
    (((SIZE) + (BOUNDARY)-1) & ~((BOUNDARY)-1))
#endif /* !defined(LIBPOOL_NO_ALIGNMENT) */

#if !defined(LIBPOOL_LOG)
#define LIBPOOL_LOG(STR) ((void)0)
#endif /* !defined(LIBPOOL_LOG) */

/*----------------------------------------------------------------------------*/

/*
 * Linked list of pointers, used to store the start of the chunk arrays inside a
 * pool.
 *
 * We need to store them as a linked list, since there can be an arbitrary
 * number of them, one for each call to `pool_expand' plus the initial one from
 * `pool_new'. New pointers will be prepended to the linked list.
 */
typedef struct ArrayStart ArrayStart;
struct ArrayStart {
    ArrayStart* next;
    void* arr;
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
    ArrayStart* array_starts;
    size_t chunk_sz;

#if defined(LIBPOOL_THREAD_SAFE)
    pool_ext_mutex_t lock;
#endif /* defined(LIBPOOL_THREAD_SAFE) */
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

    if (pool_sz == 0) {
        LIBPOOL_LOG("Invalid pool size.");
        return NULL;
    }

#if defined(LIBPOOL_NO_ALIGNMENT)
    if (chunk_sz < sizeof(void*)) {
        LIBPOOL_LOG("No alignment, and small pool size.");
        return NULL;
    }
#else  /* !defined(LIBPOOL_NO_ALIGNMENT) */
    chunk_sz = ALIGN2BOUNDARY(chunk_sz, sizeof(void*));
#endif /* !defined(LIBPOOL_NO_ALIGNMENT) */

    pool = pool_ext_alloc(sizeof(Pool));
    if (pool == NULL) {
        LIBPOOL_LOG("Failed to allocate 'Pool' structure.");
        return NULL;
    }

    pool->array_starts = pool_ext_alloc(sizeof(ArrayStart));
    if (pool->array_starts == NULL) {
        LIBPOOL_LOG("Failed to allocate 'ArrayStart' structure.");
        pool_ext_free(pool);
        return NULL;
    }

    arr = pool_ext_alloc(pool_sz * chunk_sz);
    if (arr == NULL) {
        LIBPOOL_LOG("Failed to allocate actual data array.");
        pool_ext_free(pool->array_starts);
        pool_ext_free(pool);
        return NULL;
    }

#if defined(LIBPOOL_THREAD_SAFE)
    if (!pool_ext_mutex_init(&pool->lock)) {
        LIBPOOL_LOG("Failed to initialize mutex.");
        pool_ext_free(arr);
        pool_ext_free(pool->array_starts);
        pool_ext_free(pool);
        return NULL;
    }
#endif /* defined(LIBPOOL_THREAD_SAFE) */

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
    pool->array_starts->arr  = arr;
    pool->chunk_sz           = chunk_sz;

    VALGRIND_MAKE_MEM_NOACCESS(arr, pool_sz * chunk_sz);
    VALGRIND_MAKE_MEM_NOACCESS(pool->array_starts, sizeof(ArrayStart));
    VALGRIND_MAKE_MEM_NOACCESS(pool, sizeof(Pool));
    VALGRIND_CREATE_MEMPOOL(pool, 0, 0);

    return pool;
}

/*
 * Expanding the pool simply means allocating a new chunk array, and prepending
 * it to the `pool->free_chunk' linked list.
 *
 * 1. Allocate a new `ArrayStart' structure.
 * 2. Allocate a new chunk array with the specified size.
 * 3. Link the new array together, just like in `pool_new'.
 * 4. Prepend the new chunk array to the existing linked list of free chunks.
 * 5. Prepend the new `ArrayStart' to the existing linked list of array starts.
 */
bool pool_expand(Pool* pool, size_t extra_sz) {
    bool result = true;
    ArrayStart* array_start;
    char* extra_arr;
    size_t i;

    if (pool == NULL || extra_sz <= 0) {
        LIBPOOL_LOG("Invalid pool pointer or extra size.");
        return false;
    }

#if defined(LIBPOOL_THREAD_SAFE)
    if (!pool_ext_mutex_lock(&pool->lock)) {
        LIBPOOL_LOG("Failed to lock mutex.");
        return false;
    }
#endif /* defined(LIBPOOL_THREAD_SAFE) */

    VALGRIND_MAKE_MEM_DEFINED(pool, sizeof(Pool));

    array_start = pool_ext_alloc(sizeof(ArrayStart));
    if (array_start == NULL) {
        LIBPOOL_LOG("Failed to allocate additional 'ArrayStart' structure.");
        result = false;
        goto alloc_err;
    }

    extra_arr = pool_ext_alloc(extra_sz * pool->chunk_sz);
    if (extra_arr == NULL) {
        LIBPOOL_LOG("Failed to allocate additional data array.");
        pool_ext_free(array_start);
        result = false;
        goto alloc_err;
    }

    for (i = 0; i < extra_sz - 1; i++)
        *(void**)(extra_arr + i * pool->chunk_sz) =
          extra_arr + (i + 1) * pool->chunk_sz;

    *(void**)(extra_arr + (extra_sz - 1) * pool->chunk_sz) = pool->free_chunk;
    pool->free_chunk                                       = extra_arr;

    array_start->arr   = extra_arr;
    array_start->next  = pool->array_starts;
    pool->array_starts = array_start;

    VALGRIND_MAKE_MEM_NOACCESS(extra_arr, extra_sz * pool->chunk_sz);
    VALGRIND_MAKE_MEM_NOACCESS(array_start, sizeof(ArrayStart));
alloc_err:
    VALGRIND_MAKE_MEM_NOACCESS(pool, sizeof(Pool));

#if defined(LIBPOOL_THREAD_SAFE)
    pool_ext_mutex_unlock(&pool->lock);
#endif /* defined(LIBPOOL_THREAD_SAFE) */

    return result;
}

/*
 * When closing the pool, we traverse the list of `ArrayStart' structures, which
 * contain the base address of each chunk array. We free the array, and then the
 * `ArrayStart' structure itself. Lastly, we free the `Pool' structure.
 */
void pool_destroy(Pool* pool) {
    ArrayStart* array_start;
    ArrayStart* next;

    if (pool == NULL) {
        LIBPOOL_LOG("Invalid pool pointer.");
        return;
    }

#if defined(LIBPOOL_THREAD_SAFE)
    if (!pool_ext_mutex_lock(&pool->lock)) {
        LIBPOOL_LOG("Failed to lock mutex.");
        return;
    }
#endif /* defined(LIBPOOL_THREAD_SAFE) */

    VALGRIND_MAKE_MEM_DEFINED(pool, sizeof(Pool));

    array_start = pool->array_starts;
    while (array_start != NULL) {
        VALGRIND_MAKE_MEM_DEFINED(array_start, sizeof(ArrayStart));

        next = array_start->next;
        pool_ext_free(array_start->arr);
        pool_ext_free(array_start);
        array_start = next;
    }

#if defined(LIBPOOL_THREAD_SAFE)
    pool_ext_mutex_unlock(&pool->lock);
    pool_ext_mutex_destroy(&pool->lock);
#endif /* defined(LIBPOOL_THREAD_SAFE) */

    VALGRIND_DESTROY_MEMPOOL(pool);
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
    void* result = NULL;

    if (pool == NULL) {
        LIBPOOL_LOG("Invalid pool pointer.");
        return NULL;
    }

#if defined(LIBPOOL_THREAD_SAFE)
    if (!pool_ext_mutex_lock(&pool->lock)) {
        LIBPOOL_LOG("Failed to lock mutex.");
        return NULL;
    }
#endif /* defined(LIBPOOL_THREAD_SAFE) */

    VALGRIND_MAKE_MEM_DEFINED(pool, sizeof(Pool));

    if (pool->free_chunk == NULL) {
        LIBPOOL_LOG("No free chunks in pool.");
        goto done;
    }
    VALGRIND_MAKE_MEM_DEFINED(pool->free_chunk, sizeof(void**));

    result           = pool->free_chunk;
    pool->free_chunk = *(void**)pool->free_chunk;

    VALGRIND_MEMPOOL_ALLOC(pool, result, pool->chunk_sz);
    VALGRIND_MAKE_MEM_NOACCESS(pool->free_chunk, sizeof(void**));
    VALGRIND_MAKE_MEM_NOACCESS(pool, sizeof(Pool));

done:
#if defined(LIBPOOL_THREAD_SAFE)
    pool_ext_mutex_unlock(&pool->lock);
#endif /* defined(LIBPOOL_THREAD_SAFE) */

    return result;
}

/*
 * Note that, since we are using a linked list, the caller doesn't need to free
 * in the same order that used when allocating.
 */
void pool_free(Pool* pool, void* ptr) {
    if (pool == NULL || ptr == NULL) {
        LIBPOOL_LOG("Invalid pool or data pointer.");
        return;
    }

#if defined(LIBPOOL_THREAD_SAFE)
    if (!pool_ext_mutex_lock(&pool->lock)) {
        LIBPOOL_LOG("Failed to lock mutex.");
        return;
    }
#endif /* defined(LIBPOOL_THREAD_SAFE) */

    VALGRIND_MAKE_MEM_DEFINED(pool, sizeof(Pool));

    *(void**)ptr     = pool->free_chunk;
    pool->free_chunk = ptr;

    VALGRIND_MAKE_MEM_NOACCESS(pool, sizeof(Pool));
    VALGRIND_MEMPOOL_FREE(pool, ptr);

#if defined(LIBPOOL_THREAD_SAFE)
    pool_ext_mutex_unlock(&pool->lock);
#endif /* defined(LIBPOOL_THREAD_SAFE) */
}

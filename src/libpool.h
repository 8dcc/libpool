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

#ifndef POOL_H_
#define POOL_H_ 1

#include <stddef.h>
#include <stdbool.h>

typedef struct Pool Pool;

/*
 * External functions for allocating and freeing system memory. Used by
 * `pool_new' and `pool_detroy'.
 *
 * If `LIBPOOL_NO_STDLIB' is defined, they are set to NULL, so the user must
 * initialize them. Otherwise, their default value is `malloc' and `free', from
 * the <stdlib.h> header.
 */
typedef void* (*PoolAllocFuncPtr)(size_t);
typedef void (*PoolFreeFuncPtr)(void*);
extern PoolAllocFuncPtr pool_ext_alloc;
extern PoolFreeFuncPtr pool_ext_free;

/*
 * External functions for thread-safe operations, only used if
 * 'LIBPOOL_THEAD_SAFE' is defined.
 *
 * If `LIBPOOL_NO_STDLIB' is defined, they are set to NULL, so the user must
 * initialize them. Otherwise, their default values are wrappers to POSIX
 * Thread (pthread) functions.
 */
#if defined(LIBPOOL_THREAD_SAFE)
typedef void* (*PoolMutexNewFuncPtr)(void);
typedef bool (*PoolMutexLockFuncPtr)(void* mutex);
typedef bool (*PoolMutexUnlockFuncPtr)(void* mutex);
typedef bool (*PoolMutexDestroyFuncPtr)(void* mutex);
extern PoolMutexNewFuncPtr pool_ext_mutex_new;
extern PoolMutexLockFuncPtr pool_ext_mutex_lock;
extern PoolMutexUnlockFuncPtr pool_ext_mutex_unlock;
extern PoolMutexDestroyFuncPtr pool_ext_mutex_destroy;
#endif /* defined(LIBPOOL_THREAD_SAFE) */

/*----------------------------------------------------------------------------*/

/*
 * Allocate and initialize a new `Pool' structure, with the specified number of
 * chunks, each with the specified size.
 *
 * Notes:
 *   - If the initialization fails, NULL is returned.
 *   - The caller must free the returned pointer using `pool_destroy'.
 *   - The `chunk_sz' must be greater or equal than `sizeof(void*)'.
 *   - The pool size can be updated with `pool_expand', but the chunk size
 *     cannot be changed.
 */
Pool* pool_new(size_t pool_sz, size_t chunk_sz);

/*
 * Expand the specified `pool', adding `extra_sz' free chunks.
 *
 * On success, it returns true; otherwise, it returns false and leaves the pool
 * unchanged.
 */
bool pool_expand(Pool* pool, size_t extra_sz);

/*
 * Free all data in a `Pool' structure, along with the structure itself. All
 * data allocated from this the pool becomes unusable. Allows NULL as the
 * `pool' parameter.
 */
void pool_destroy(Pool* pool);

/*
 * Allocate a fixed-size chunk from the specified pool. If no chunks are
 * available, NULL is returned.
 */
void* pool_alloc(Pool* pool);

/*
 * Free a fixed-size chunk from the specified pool. Allows NULL as both
 * arguments.
 */
void pool_free(Pool* pool, void* ptr);

#endif /* POOL_H_ */

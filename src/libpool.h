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
 * `pool_new' and `pool_close'.
 *
 * If `LIBPOOL_NO_STDLIB' is defined, they are set to NULL, so the user must
 * initialize them. Otherwise, their default value is `malloc' and `free', from
 * the <stdlib.h> header.
 */
typedef void* (*PoolAllocFuncPtr)(size_t);
typedef void (*PoolFreeFuncPtr)(void*);
typedef void* (*PoolMemcpyFuncPtr)(void*, const void*, size_t);
extern PoolAllocFuncPtr pool_ext_alloc;
extern PoolFreeFuncPtr pool_ext_free;
extern PoolMemcpyFuncPtr pool_ext_memcpy;

/*
 * Allocate and initialize a new `Pool' structure, with the specified number of
 * chunks, each with the specified size.
 *
 * Notes:
 *   - If the initialization fails, NULL is returned.
 *   - The caller must free the returned pointer using `pool_close'.
 *   - The `chunk_sz' must be greater or equal than `sizeof(void*)'.
 *   - The pool size can be updated with `pool_resize', but the chunk size
 *     cannot be changed.
 */
Pool* pool_new(size_t pool_sz, size_t chunk_sz);

/*
 * Change the number of chunks in the specified pool. Allocates a new chunk
 * array, and moves the old data to the next one.
 *
 * Notes:
 *   - Returs true on success, false otherwise.
 *   - The new pool size must be greater or equal than the old one.
 */
bool pool_resize(Pool* pool, size_t new_pool_sz);

/*
 * Free all data in a `Pool' structure, along with the structure itself. All
 * data allocated from this the pool becomes unusable. Allows NULL as the
 * `pool' parameter.
 */
void pool_close(Pool* pool);

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

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

typedef struct Pool Pool;

/*
 * Allocate and initialize a new `Pool' structure, with the specified number of
 * chunks, each with the specified size. If the initialization fails, NULL is
 * returned.
 *
 * The caller must free the returned pointer using `pool_close'.
 *
 * Note that the `chunk_sz' must be greater or equal than `sizeof(void*)'.
 */
Pool* pool_new(size_t pool_sz, size_t chunk_sz);

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

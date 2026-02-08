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
 *
 * Multithreading tests for libpool. Must be compiled with
 * -DLIBPOOL_THREAD_SAFE.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

#include "../src/libpool.h"
#include "test.h"

#if !defined(LIBPOOL_THREAD_SAFE)
#error "This test must be compiled with -DLIBPOOL_THREAD_SAFE"
#endif /* !defined(LIBPOOL_THREAD_SAFE) */

/*----------------------------------------------------------------------------*/
/* Shared state */

#define NUM_THREADS    4
#define ALLOCS_PER_THR 50

static Pool* shared_pool               = NULL;
static unsigned long successful_allocs = 0;
static pthread_mutex_t counter_lock;

/*----------------------------------------------------------------------------*/
/* Test: basic alloc/free from multiple threads */

static void* basic_thread_func(void* arg) {
    void* chunks[ALLOCS_PER_THR];
    int allocated = 0;
    size_t i;

    (void)arg; /* Unused */

    for (i = 0; i < ALLOCS_PER_THR; i++) {
        chunks[i] = pool_alloc(shared_pool);
        if (chunks[i] != NULL)
            allocated++;
    }

    for (i = 0; i < ALLOCS_PER_THR; i++)
        if (chunks[i] != NULL)
            pool_free(shared_pool, chunks[i]);

    pthread_mutex_lock(&counter_lock);
    successful_allocs += allocated;
    pthread_mutex_unlock(&counter_lock);

    return NULL;
}

TEST_DECL(basic_alloc_free) {
    pthread_t threads[NUM_THREADS];
    size_t i;

    shared_pool       = pool_new(NUM_THREADS * ALLOCS_PER_THR, sizeof(long));
    successful_allocs = 0;
    TEST_ASSERT_NOT_NULL(shared_pool);

    pthread_mutex_init(&counter_lock, NULL);

    for (i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, basic_thread_func, NULL);

    for (i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    pthread_mutex_destroy(&counter_lock);

    TEST_ASSERT_EQ(successful_allocs, NUM_THREADS * ALLOCS_PER_THR);

    pool_destroy(shared_pool);
}

/*----------------------------------------------------------------------------*/
/* Test: threads competing for limited pool capacity */

static void* contention_thread_func(void* arg) {
    void* chunks[ALLOCS_PER_THR];
    int allocated = 0;
    size_t i;

    (void)arg; /* Unused */

    for (i = 0; i < ALLOCS_PER_THR; i++) {
        chunks[i] = pool_alloc(shared_pool);
        if (chunks[i] != NULL)
            allocated++;
    }

    for (i = 0; i < ALLOCS_PER_THR; i++)
        if (chunks[i] != NULL)
            pool_free(shared_pool, chunks[i]);

    pthread_mutex_lock(&counter_lock);
    successful_allocs += allocated;
    pthread_mutex_unlock(&counter_lock);

    return NULL;
}

TEST_DECL(contention) {
    const size_t pool_size = 25;
    pthread_t threads[NUM_THREADS];
    size_t i;

    /* Pool smaller than total demand: threads must compete */
    shared_pool       = pool_new(pool_size, sizeof(long));
    successful_allocs = 0;
    TEST_ASSERT_NOT_NULL(shared_pool);

    pthread_mutex_init(&counter_lock, NULL);

    for (i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, contention_thread_func, NULL);

    for (i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    pthread_mutex_destroy(&counter_lock);

    /* Total successful allocations should not exceed pool capacity */
    TEST_ASSERT_TRUE(successful_allocs >= pool_size);

    pool_destroy(shared_pool);
}

/*----------------------------------------------------------------------------*/
/* Test: rapid alloc/free cycles to stress locking */

static void* rapid_cycle_thread_func(void* arg) {
    void* chunk;
    size_t i;

    (void)arg; /* Unused */

    /* Rapidly allocate and free 100 times from each thread */
    for (i = 0; i < 100; i++) {
        chunk = pool_alloc(shared_pool);
        if (chunk != NULL)
            pool_free(shared_pool, chunk);
    }

    return NULL;
}

TEST_DECL(rapid_cycles) {
    pthread_t threads[NUM_THREADS];
    size_t i;

    shared_pool = pool_new(NUM_THREADS, sizeof(long));
    TEST_ASSERT_NOT_NULL(shared_pool);

    for (i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, rapid_cycle_thread_func, NULL);

    for (i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    pool_destroy(shared_pool);
}

/*----------------------------------------------------------------------------*/
/* Test: concurrent expand while allocating/freeing */

static void* expand_alloc_thread_func(void* arg) {
    void* chunks[ALLOCS_PER_THR];
    size_t i, round;

    (void)arg; /* Unused */

    for (round = 0; round < 10; round++) {
        for (i = 0; i < ALLOCS_PER_THR; i++)
            chunks[i] = pool_alloc(shared_pool);

        for (i = 0; i < ALLOCS_PER_THR; i++)
            if (chunks[i] != NULL)
                pool_free(shared_pool, chunks[i]);
    }

    return NULL;
}

static void* expand_thread_func(void* arg) {
    size_t i;

    (void)arg; /* Unused */

    for (i = 0; i < 10; i++)
        pool_expand(shared_pool, 10);

    return NULL;
}

TEST_DECL(concurrent_expand) {
    pthread_t alloc_threads[NUM_THREADS];
    pthread_t expand_thread;
    size_t i;

    shared_pool = pool_new(NUM_THREADS * ALLOCS_PER_THR, sizeof(long));
    TEST_ASSERT_NOT_NULL(shared_pool);

    /* Create N allocation threads, and one expansion thread */
    for (i = 0; i < NUM_THREADS; i++)
        pthread_create(&alloc_threads[i], NULL, expand_alloc_thread_func, NULL);
    pthread_create(&expand_thread, NULL, expand_thread_func, NULL);

    /* Wait for the threads to finish */
    for (i = 0; i < NUM_THREADS; i++)
        pthread_join(alloc_threads[i], NULL);
    pthread_join(expand_thread, NULL);

    pool_destroy(shared_pool);
}

/*----------------------------------------------------------------------------*/
/* Main */

int main(void) {
    printf("Running libpool multithreading tests...\n\n");

    TEST_RUN(basic_alloc_free);
    TEST_RUN(contention);
    TEST_RUN(rapid_cycles);
    TEST_RUN(concurrent_expand);

    putchar('\n');
    TEST_PRINT_RESULTS(stdout);

    return (TEST_NUM_FAILED > 0) ? 1 : 0;
}

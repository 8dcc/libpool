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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/libpool.h"
#include "test.h"

/*----------------------------------------------------------------------------*/
/* Tests for pool_new */

TEST_DECL(pool_new_basic) {
    Pool* pool = pool_new(10, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);
    pool_destroy(pool);
}

TEST_DECL(pool_new_larger_chunk) {
    Pool* pool = pool_new(5, 128);
    TEST_ASSERT_NOT_NULL(pool);
    pool_destroy(pool);
}

TEST_DECL(pool_new_zero_size) {
    Pool* pool = pool_new(0, sizeof(void*));
    TEST_ASSERT_NULL(pool);
}

TEST_DECL(pool_new_single_chunk) {
    Pool* pool;
    void* chunk;
    void* chunk2;

    pool = pool_new(1, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    chunk = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(chunk);

    chunk2 = pool_alloc(pool);
    TEST_ASSERT_NULL(chunk2);

    pool_destroy(pool);
}

/*----------------------------------------------------------------------------*/
/* Tests for pool_alloc */

TEST_DECL(pool_alloc_basic) {
    Pool* pool;
    void* chunk;

    pool = pool_new(10, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    chunk = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(chunk);

    pool_destroy(pool);
}

TEST_DECL(pool_alloc_null_pool) {
    void* chunk = pool_alloc(NULL);
    TEST_ASSERT_NULL(chunk);
}

TEST_DECL(pool_alloc_multiple) {
    Pool* pool;
    void* chunks[5];
    size_t i, j;

    pool = pool_new(5, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    for (i = 0; i < 5; i++) {
        chunks[i] = pool_alloc(pool);
        TEST_ASSERT_NOT_NULL(chunks[i]);
    }

    for (i = 0; i < 5; i++)
        for (j = i + 1; j < 5; j++)
            TEST_ASSERT_NE(chunks[i], chunks[j]);

    pool_destroy(pool);
}

TEST_DECL(pool_alloc_exhaustion) {
    Pool* pool = pool_new(3, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NULL(pool_alloc(pool));
    TEST_ASSERT_NULL(pool_alloc(pool));

    pool_destroy(pool);
}

TEST_DECL(pool_alloc_write_data) {
    typedef struct {
        int x;
        int y;
        char name[24];
    } TestStruct;

    Pool* pool;
    TestStruct* obj1;
    TestStruct* obj2;
    char name_buf[24];

    pool = pool_new(10, sizeof(TestStruct));
    TEST_ASSERT_NOT_NULL(pool);

    obj1 = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(obj1);

    obj2 = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(obj2);

    memset(name_buf, 0x00, sizeof(name_buf));
    strcpy(name_buf, "foo");

    /* Write data to first object */
    obj1->x = 42;
    obj1->y = -100;
    memcpy(obj1->name, name_buf, sizeof(name_buf));

    /* Copy first object to second object */
    memcpy(obj2, obj1, sizeof(TestStruct));

    /* Modify them both */
    strcpy(obj1->name, "bar");
    obj2->x = 0xFFFF;

    /* Ensure both objects have their own data */
    TEST_ASSERT_EQ(obj1->x, 42);
    TEST_ASSERT_EQ(obj1->y, -100);
    TEST_ASSERT_EQ(strcmp(obj1->name, "bar"), 0);
    TEST_ASSERT_EQ(obj2->x, 0xFFFF);
    TEST_ASSERT_EQ(obj2->y, -100);
    TEST_ASSERT_EQ(strcmp(obj2->name, "foo"), 0);

    pool_destroy(pool);
}

/*----------------------------------------------------------------------------*/
/* Tests for pool_free */

TEST_DECL(pool_free_basic) {
    Pool* pool;
    void* chunk;

    pool = pool_new(10, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    chunk = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(chunk);

    pool_free(pool, chunk);
    pool_destroy(pool);
}

TEST_DECL(pool_free_null_pool) {
    int dummy;
    pool_free(NULL, &dummy);
}

TEST_DECL(pool_free_null_ptr) {
    Pool* pool = pool_new(10, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    pool_free(pool, NULL);
    pool_destroy(pool);
}

TEST_DECL(pool_free_both_null) {
    pool_free(NULL, NULL);
}

TEST_DECL(pool_free_reuse) {
    Pool* pool;
    void* chunk1;
    void* chunk2;

    pool = pool_new(1, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    chunk1 = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(chunk1);

    /* No elements left */
    TEST_ASSERT_NULL(pool_alloc(pool));

    pool_free(pool, chunk1);

    /* Should return the same pointer as before */
    chunk2 = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(chunk2);
    TEST_ASSERT_EQ(chunk1, chunk2);

    pool_destroy(pool);
}

TEST_DECL(pool_free_out_of_order) {
    Pool* pool;
    void *c1, *c2, *c3, *c4, *c5;
    void *n1, *n2, *n3, *n4, *n5;

    pool = pool_new(5, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    c1 = pool_alloc(pool);
    c2 = pool_alloc(pool);
    c3 = pool_alloc(pool);
    c4 = pool_alloc(pool);
    c5 = pool_alloc(pool);

    TEST_ASSERT_NOT_NULL(c1);
    TEST_ASSERT_NOT_NULL(c2);
    TEST_ASSERT_NOT_NULL(c3);
    TEST_ASSERT_NOT_NULL(c4);
    TEST_ASSERT_NOT_NULL(c5);

    pool_free(pool, c3);
    pool_free(pool, c1);
    pool_free(pool, c5);
    pool_free(pool, c2);
    pool_free(pool, c4);

    n1 = pool_alloc(pool);
    n2 = pool_alloc(pool);
    n3 = pool_alloc(pool);
    n4 = pool_alloc(pool);
    n5 = pool_alloc(pool);

    TEST_ASSERT_NOT_NULL(n1);
    TEST_ASSERT_NOT_NULL(n2);
    TEST_ASSERT_NOT_NULL(n3);
    TEST_ASSERT_NOT_NULL(n4);
    TEST_ASSERT_NOT_NULL(n5);

    TEST_ASSERT_NULL(pool_alloc(pool));

    pool_destroy(pool);
}

/*----------------------------------------------------------------------------*/
/* Tests for pool_expand */

TEST_DECL(pool_expand_basic) {
    Pool* pool = pool_new(2, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NULL(pool_alloc(pool));

    TEST_ASSERT_TRUE(pool_expand(pool, 3));

    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NULL(pool_alloc(pool));

    pool_destroy(pool);
}

TEST_DECL(pool_expand_null_pool) {
    TEST_ASSERT_FALSE(pool_expand(NULL, 10));
}

TEST_DECL(pool_expand_zero_size) {
    Pool* pool = pool_new(5, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    TEST_ASSERT_FALSE(pool_expand(pool, 0));

    pool_destroy(pool);
}

TEST_DECL(pool_expand_multiple) {
    Pool* pool = pool_new(1, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NULL(pool_alloc(pool));

    TEST_ASSERT_TRUE(pool_expand(pool, 1));
    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NULL(pool_alloc(pool));

    TEST_ASSERT_TRUE(pool_expand(pool, 2));
    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NOT_NULL(pool_alloc(pool));
    TEST_ASSERT_NULL(pool_alloc(pool));

    pool_destroy(pool);
}

TEST_DECL(pool_expand_preserves_allocated) {
    typedef struct {
        long value;
    } TestData;

    Pool* pool;
    TestData *d1, *d2;

    pool = pool_new(2, sizeof(TestData));
    TEST_ASSERT_NOT_NULL(pool);

    d1 = pool_alloc(pool);
    d2 = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(d1);
    TEST_ASSERT_NOT_NULL(d2);

    d1->value = 111;
    d2->value = 222;

    TEST_ASSERT_TRUE(pool_expand(pool, 5));

    TEST_ASSERT_EQ(d1->value, 111);
    TEST_ASSERT_EQ(d2->value, 222);

    pool_destroy(pool);
}

/*----------------------------------------------------------------------------*/
/* Tests for pool_destroy */

TEST_DECL(pool_destroy_null) {
    pool_destroy(NULL);
}

TEST_DECL(pool_destroy_with_allocations) {
    Pool* pool = pool_new(10, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    pool_alloc(pool);
    pool_alloc(pool);
    pool_alloc(pool);

    pool_destroy(pool);
}

TEST_DECL(pool_destroy_after_expand) {
    Pool* pool = pool_new(5, sizeof(void*));
    TEST_ASSERT_NOT_NULL(pool);

    pool_expand(pool, 10);
    pool_expand(pool, 5);

    pool_destroy(pool);
}

/*----------------------------------------------------------------------------*/
/* Integration tests */

TEST_DECL(mixed_alloc_free_expand) {
    Pool* pool;
    long *a, *b, *c, *d, *e, *f;

    pool = pool_new(3, sizeof(long));
    TEST_ASSERT_NOT_NULL(pool);

    /* Fill pool */
    a = pool_alloc(pool);
    b = pool_alloc(pool);
    c = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);
    *a = 1;
    *b = 2;
    *c = 3;
    TEST_ASSERT_NULL(pool_alloc(pool));

    /* Free an element from the full pool, and fill it again */
    pool_free(pool, b);
    d = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(d);
    *d = 4;
    TEST_ASSERT_NULL(pool_alloc(pool));

    /* Increase capacity from 3 to 5 */
    TEST_ASSERT_TRUE(pool_expand(pool, 2));

    /* Fill the pool again */
    e = pool_alloc(pool);
    f = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_NOT_NULL(f);
    *e = 5;
    *f = 6;
    TEST_ASSERT_NULL(pool_alloc(pool));

    /* Ensure the results are as expected */
    TEST_ASSERT_EQ(*a, 1);
    TEST_ASSERT_EQ(*c, 3);
    TEST_ASSERT_EQ(*d, 4);
    TEST_ASSERT_EQ(*e, 5);
    TEST_ASSERT_EQ(*f, 6);

    pool_destroy(pool);
}

TEST_DECL(large_pool) {
    const size_t pool_sz = 1000;
    Pool* pool;
    long* chunks[1000];
    size_t i;

    pool = pool_new(pool_sz, sizeof(long));
    TEST_ASSERT_NOT_NULL(pool);

    /* Allocate the entire pool */
    for (i = 0; i < pool_sz; i++) {
        chunks[i] = pool_alloc(pool);
        TEST_ASSERT_NOT_NULL(chunks[i]);
        *chunks[i] = 0xABCD;
    }
    TEST_ASSERT_NULL(pool_alloc(pool));

    /* Free the entire pool */
    for (i = 0; i < pool_sz; i++)
        pool_free(pool, chunks[i]);

    /* Allocate the entire pool again */
    for (i = 0; i < pool_sz; i++)
        TEST_ASSERT_NOT_NULL(pool_alloc(pool));

    pool_destroy(pool);
}

#if defined(LIBPOOL_NO_ALIGNMENT)
TEST_DECL(pool_new_small_chunk) {
    /* Should not be able to create pools with chunks smaller than a pointer */
    Pool* pool = pool_new(5, sizeof(char));
    TEST_ASSERT_NULL(pool);
}
#else  /* !defined(LIBPOOL_NO_ALIGNMENT) */
TEST_DECL(alignment) {
    Pool* pool;
    void *c1, *c2;

    pool = pool_new(10, sizeof(char));
    TEST_ASSERT_NOT_NULL(pool);

    c1 = pool_alloc(pool);
    c2 = pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(c1);
    TEST_ASSERT_NOT_NULL(c2);

    /* Ensure the returned addresses are aligned to the size of a pointer */
    TEST_ASSERT_EQ((uintptr_t)c1 % sizeof(void*), 0);
    TEST_ASSERT_EQ((uintptr_t)c2 % sizeof(void*), 0);

    pool_destroy(pool);
}
#endif /* !defined(LIBPOOL_NO_ALIGNMENT) */

/*----------------------------------------------------------------------------*/
/* Main */

int main(void) {
    printf("Running libpool tests...\n\n");

    TEST_RUN(pool_new_basic);
    TEST_RUN(pool_new_larger_chunk);
    TEST_RUN(pool_new_zero_size);
    TEST_RUN(pool_new_single_chunk);
    TEST_RUN(pool_alloc_basic);
    TEST_RUN(pool_alloc_null_pool);
    TEST_RUN(pool_alloc_multiple);
    TEST_RUN(pool_alloc_exhaustion);
    TEST_RUN(pool_alloc_write_data);
    TEST_RUN(pool_free_basic);
    TEST_RUN(pool_free_null_pool);
    TEST_RUN(pool_free_null_ptr);
    TEST_RUN(pool_free_both_null);
    TEST_RUN(pool_free_reuse);
    TEST_RUN(pool_free_out_of_order);
    TEST_RUN(pool_expand_basic);
    TEST_RUN(pool_expand_null_pool);
    TEST_RUN(pool_expand_zero_size);
    TEST_RUN(pool_expand_multiple);
    TEST_RUN(pool_expand_preserves_allocated);
    TEST_RUN(pool_destroy_null);
    TEST_RUN(pool_destroy_with_allocations);
    TEST_RUN(pool_destroy_after_expand);
    TEST_RUN(mixed_alloc_free_expand);
    TEST_RUN(large_pool);

#if defined(LIBPOOL_NO_ALIGNMENT)
    TEST_RUN(pool_new_small_chunk);
#else  /* !defined(LIBPOOL_NO_ALIGNMENT) */
    TEST_RUN(alignment);
#endif /* !defined(LIBPOOL_NO_ALIGNMENT) */

    putchar('\n');
    TEST_PRINT_RESULTS(stdout);

    return (TEST_NUM_FAILED > 0) ? 1 : 0;
}

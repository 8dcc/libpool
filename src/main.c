
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "libpool.h"

/*
 * We just have to make sure the items we store in the returned pointer are
 * small enough to fit in a chunk. In this case, smaller than CHUNK_SZ bytes,
 * since it's what we will specify when calling `pool_new' from `main'.
 */
typedef struct {
    long n;
    double f;
} MyObject;

static void test_pool(Pool* pool) {
    MyObject *obj1, *obj2, *obj3;
    size_t i;
    bool failed_alloc = false;

    /*
     * Example allocation. Since the chunks have a fixed type (specified when
     * calling `pool_new'), we just have to pass the pool pointer to
     * `pool_alloc'.
     */
    obj1 = pool_alloc(pool);
    if (obj1 == NULL) {
        fprintf(stderr, "Could not allocate a new chunk from pool.\n");
        exit(1);
    }
    obj1->n = 123;
    obj1->f = 5.0;
    printf("Data of allocated object: %lu, %f\n", obj1->n, obj1->f);
    pool_free(pool, obj1);

    /*
     * Some extra allocations, to illustrate that the user can free chunks in
     * any order. Note that the user should always check if `pool_alloc'
     * returned NULL, but some checks are skipped here for readablity.
     */
    obj1 = pool_alloc(pool);
    obj2 = pool_alloc(pool);
    obj3 = pool_alloc(pool);
    pool_free(pool, obj1);
    pool_free(pool, obj3);
    pool_free(pool, obj2);

    /*
     * Keep allocating until we run out of chunks, to illustrate what happens
     * after too many allocations. We are "leaking" pool memory in this loop,
     * but it's not really leaked to the system because we will `close' the pool
     * later.
     */
    for (i = 0; i < 35; i++) {
        if (pool_alloc(pool) == NULL) {
            failed_alloc = true;
            break;
        }
    }

    printf((failed_alloc) ? "Failed to allocate chunk at iteration: %lu\n"
                          : "Successfully allocated %lu chunks.\n",
           i);
}

int main(void) {
    Pool *pool1, *pool2;
    size_t pool1_sz, pool2_sz, pool1_chunksz, pool2_chunksz;

    /*
     * Initialize the pool once. The user doesn't even need to understand how
     * the pool structure is implemented. Just how many chunks it has, and how
     * big is each chunk. Note that, in a pool allocator, each chunk has a fixed
     * size.
     *
     * It's common to use many pools of different chunk sizes at the same time.
     */
    pool1_sz      = 50;
    pool1_chunksz = 64;
    pool1         = pool_new(pool1_sz, pool1_chunksz);
    if (pool1 == NULL) {
        fprintf(stderr, "Could not create a new pool.\n");
        exit(1);
    }

    pool2_sz      = 30;
    pool2_chunksz = 100;
    pool2         = pool_new(pool2_sz, pool2_chunksz);
    if (pool2 == NULL) {
        fprintf(stderr, "Could not create a new pool.\n");
        exit(1);
    }

    /*
     * Do do some tests on each pool.
     */
    printf("Testing first pool, of size %lu:\n", pool1_sz);
    test_pool(pool1);
    printf("\nTesting second pool, of size %lu:\n", pool2_sz);
    test_pool(pool2);

    printf("\nExpanding first pool by 10 (total %lu) and testing:\n",
           pool1_sz + 10);
    pool_resize(pool1, 10);
    test_pool(pool1);

    /*
     * When we are done, we "close" each pool. All previously allocated data
     * from the pool becomes unusable, and the necessary resources allocated by
     * `pool_new' are freed.
     */
    pool_close(pool2);
    pool_close(pool1);

    return 0;
}

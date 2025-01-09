
#include <stddef.h>
#include <assert.h>
#include <inttypes.h> /* strtoumax */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libpool.h"

#define BUFFERED_PTRS 1000

static void* ptrs[BUFFERED_PTRS];
static size_t ptrs_pos = 0;

static void benchmark_libpool(size_t nmemb, size_t size) {
    Pool* pool = pool_new(nmemb, size);
    assert(pool != NULL);

    while (nmemb-- > 0) {
        ptrs[ptrs_pos++] = pool_alloc(pool);
        if (ptrs_pos >= BUFFERED_PTRS)
            while (ptrs_pos > 0)
                pool_free(pool, ptrs[--ptrs_pos]);
    }

    while (ptrs_pos > 0)
        pool_free(pool, ptrs[--ptrs_pos]);
    pool_close(pool);
}

static void benchmark_malloc(size_t nmemb, size_t size) {
    while (nmemb-- > 0) {
        ptrs[ptrs_pos++] = malloc(size);
        if (ptrs_pos >= BUFFERED_PTRS)
            while (ptrs_pos > 0)
                free(ptrs[--ptrs_pos]);
    }

    while (ptrs_pos > 0)
        free(ptrs[--ptrs_pos]);
}

int main(int argc, char** argv) {
    size_t nmemb, size;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <libpool|malloc> NMEMB SIZE\n", argv[0]);
        return 1;
    }

    nmemb = strtoumax(argv[2], NULL, 10);
    size  = strtoumax(argv[3], NULL, 10);
    if (nmemb == 0 || nmemb == UINTMAX_MAX || size == 0 ||
        size == UINTMAX_MAX) {
        fprintf(stderr, "Invalid NMEMB or SIZE arguments.\n");
        return 1;
    }

    if (!strcmp(argv[1], "libpool")) {
        benchmark_libpool(nmemb, size);
    } else if (!strcmp(argv[1], "malloc")) {
        benchmark_malloc(nmemb, size);
    } else {
        fprintf(stderr, "The first argument must be 'libpool' or 'malloc'.\n");
        return 1;
    }

    return 0;
}

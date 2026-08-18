
/**
 * @file    test_bignum_to_string_mt.c
 * @brief   Multithreaded tests for bignum_to_string.
 * @version 0.5.0
 * @details Revision 0.5.0: independent read-only workers covering optimized four-base paths.
 */

#include "bignum_to_string.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct worker_args {
    const bignum_t *value;
    int base;
    const char *expected;
    size_t iterations;
};

static void *worker(void *opaque)
{
    const struct worker_args *args = (const struct worker_args *)opaque;
    char output[2050];
    for (size_t i = 0U; i < args->iterations; ++i) {
        assert(bignum_to_string(output, sizeof(output), args->value, args->base) ==
               BIGNUM_TO_STRING_SUCCESS);
        assert(strcmp(output, args->expected) == 0);
    }
    return NULL;
}

static void run_case(const bignum_t *value, int base, const char *expected)
{
    const struct worker_args args = { value, base, expected, 10000U };
    pthread_t threads[4];
    for (size_t i = 0U; i < 4U; ++i) {
        assert(pthread_create(&threads[i], NULL, worker, (void *)&args) == 0);
    }
    for (size_t i = 0U; i < 4U; ++i) {
        assert(pthread_join(threads[i], NULL) == 0);
    }
}

int main(void)
{
    bignum_t value = { { UINT64_MAX, 1U }, 2U };
    run_case(&value, 2,
             "11111111111111111111111111111111111111111111111111111111111111111");
    run_case(&value, 8, "3777777777777777777777");
    run_case(&value, 10, "36893488147419103231");
    run_case(&value, 16, "1ffffffffffffffff");
    puts("--- Multithreaded bignum_to_string test passed ---");
    return 0;
}

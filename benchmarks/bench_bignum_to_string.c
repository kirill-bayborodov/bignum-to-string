
/**
 * @file    bench_bignum_to_string.c
 * @brief   Single-thread benchmark for bignum_to_string.
 * @version 0.5.0
 * @details Revision 0.5.0: four-base conversion timing including optimized ASM paths.
 */
#define _POSIX_C_SOURCE 200809L

#include "bignum_to_string.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

int main(int argc, char **argv)
{
    size_t iterations = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 100000U;
    const int bases[] = { 2, 8, 10, 16 };
    bignum_t value = { { UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX }, 4U };
    char output[2050];
    uint64_t checksum = 0U;
    uint64_t start = now_ns();
    for (size_t i = 0U; i < iterations; ++i) {
        int base = bases[i % (sizeof(bases) / sizeof(bases[0]))];
        if (bignum_to_string(output, sizeof(output), &value, base) !=
            BIGNUM_TO_STRING_SUCCESS) {
            return 1;
        }
        checksum ^= (uint64_t)strlen(output);
        checksum ^= (uint64_t)(unsigned char)output[0];
    }
    uint64_t elapsed = now_ns() - start;
    printf("iterations=%zu bases=2,8,10,16 elapsed_ns=%llu ns_per_call=%.3f checksum=%llu\n",
           iterations, (unsigned long long)elapsed,
           (double)elapsed / (double)iterations,
           (unsigned long long)checksum);
    return 0;
}

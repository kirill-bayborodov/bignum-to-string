
/**
 * @file    bench_bignum_to_string_mt.c
 * @brief   Multithread benchmark for bignum_to_string.
 * @version 0.5.0
 * @details Revision 0.5.0: four-base conversion with deterministic inputs and optimized ASM paths.
 */
#define _POSIX_C_SOURCE 200809L

#include "bignum_to_string.h"

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { DATA_ALL_ZERO, DATA_ALL_NONZERO, DATA_MIXED } data_mode_t;

typedef struct {
    uint64_t iterations;
    uint64_t warmup;
    uint64_t total_iterations;
    size_t threads;
    size_t data_count;
    size_t src_len;
    uint64_t seed;
    data_mode_t mode;
} options_t;

typedef struct {
    const bignum_t *data;
    const options_t *options;
    size_t thread_id;
    uint64_t checksum;
    uint64_t successful;
    int failed;
} worker_arg_t;

static uint64_t next_value(uint64_t *state)
{
    *state ^= *state << 7U;
    *state ^= *state >> 9U;
    *state ^= *state << 8U;
    return *state;
}

static const char *mode_name(data_mode_t mode)
{
    if (mode == DATA_ALL_ZERO) return "all_zero";
    if (mode == DATA_MIXED) return "mixed";
    return "all_nonzero";
}

static int parse_u64(const char *text, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed;
    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') return -1;
    *value = (uint64_t)parsed;
    return 0;
}

static int parse_mode(const char *text, data_mode_t *mode)
{
    if (strcmp(text, "all_zero") == 0) *mode = DATA_ALL_ZERO;
    else if (strcmp(text, "all_nonzero") == 0) *mode = DATA_ALL_NONZERO;
    else if (strcmp(text, "mixed") == 0) *mode = DATA_MIXED;
    else return -1;
    return 0;
}

static int parse_options(int argc, char **argv, options_t *options)
{
    *options = (options_t){
        .iterations = UINT64_C(1000000), .warmup = UINT64_C(10000),
        .total_iterations = 0U, .threads = 2U, .data_count = 4096U,
        .src_len = BIGNUM_CAPACITY, .seed = UINT64_C(0x9E3779B97F4A7C15),
        .mode = DATA_ALL_NONZERO
    };
    for (int i = 1; i < argc; ++i) {
        uint64_t value;
        const char *option = argv[i];
        if (strcmp(option, "--threads") == 0 || strcmp(option, "--iterations") == 0 ||
            strcmp(option, "--total-iterations") == 0 || strcmp(option, "--warmup") == 0 ||
            strcmp(option, "--data-count") == 0 || strcmp(option, "--src-len") == 0 ||
            strcmp(option, "--seed") == 0) {
            if (i + 1 >= argc || parse_u64(argv[++i], &value) != 0) return -1;
            if (strcmp(option, "--threads") == 0) options->threads = (size_t)value;
            else if (strcmp(option, "--iterations") == 0) options->iterations = value;
            else if (strcmp(option, "--total-iterations") == 0) options->total_iterations = value;
            else if (strcmp(option, "--warmup") == 0) options->warmup = value;
            else if (strcmp(option, "--data-count") == 0) options->data_count = (size_t)value;
            else if (strcmp(option, "--src-len") == 0) options->src_len = (size_t)value;
            else options->seed = value;
        } else if (strcmp(option, "--data-mode") == 0) {
            if (i + 1 >= argc || parse_mode(argv[++i], &options->mode) != 0) return -1;
        } else if (strcmp(option, "--help") == 0) {
            printf("usage: %s [--threads N] [--iterations N|--total-iterations N] [--warmup N] [--data-count N] [--src-len N] [--seed N] [--data-mode all_zero|all_nonzero|mixed]\n", argv[0]);
            exit(EXIT_SUCCESS);
        } else return -1;
    }
    if (options->threads == 0U || options->data_count == 0U ||
        options->src_len > BIGNUM_CAPACITY) return -1;
    if (options->total_iterations != 0U) {
        if (options->total_iterations % options->threads != 0U) return -1;
        options->iterations = options->total_iterations / options->threads;
    }
    return options->iterations != 0U ? 0 : -1;
}

static void fill_data(bignum_t *data, const options_t *options)
{
    uint64_t state = options->seed;
    for (size_t row = 0U; row < options->data_count; ++row) {
        int zero = options->mode == DATA_ALL_ZERO ||
                   (options->mode == DATA_MIXED && (row % 2U) == 0U);
        data[row].len = options->src_len;
        for (size_t word = 0U; word < options->src_len; ++word) {
            data[row].words[word] = zero ? 0U : next_value(&state);
        }
        for (size_t word = options->src_len; word < BIGNUM_CAPACITY; ++word) {
            data[row].words[word] = 0U;
        }
        while (data[row].len != 0U && data[row].words[data[row].len - 1U] == 0U) {
            --data[row].len;
        }
    }
}

static uint64_t fingerprint(const bignum_t *data, const options_t *options)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t row = 0U; row < options->data_count; ++row) {
        for (size_t word = 0U; word < options->src_len; ++word) {
            hash ^= data[row].words[word];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static void *worker(void *opaque)
{
    worker_arg_t *arg = opaque;
    char output[618];
    uint64_t total = arg->options->warmup + arg->options->iterations;
    for (uint64_t i = 0U; i < total; ++i) {
        const bignum_t *source = &arg->data[(i + arg->thread_id) % arg->options->data_count];
        int base = (i & 1U) == 0U ? 10 : 16;
        if (bignum_to_string(output, sizeof(output), source, base) != BIGNUM_TO_STRING_SUCCESS) {
            arg->failed = 1;
            return NULL;
        }
        if (i >= arg->options->warmup) {
            arg->checksum ^= (uint64_t)(unsigned char)output[0] + (uint64_t)strlen(output) + i;
            ++arg->successful;
        }
    }
    return NULL;
}

static double seconds_between(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

int main(int argc, char **argv)
{
    options_t options;
    bignum_t *data;
    pthread_t *threads;
    worker_arg_t *args;
    struct timespec start, end;
    uint64_t checksum = 0U, successful = 0U;
    uint64_t total_iterations;
    double elapsed;

    if (parse_options(argc, argv, &options) != 0) {
        fprintf(stderr, "invalid benchmark arguments; use --help\n");
        return EXIT_FAILURE;
    }
    data = calloc(options.data_count, sizeof(*data));
    threads = calloc(options.threads, sizeof(*threads));
    args = calloc(options.threads, sizeof(*args));
    if (data == NULL || threads == NULL || args == NULL) {
        perror("calloc"); free(data); free(threads); free(args); return EXIT_FAILURE;
    }
    fill_data(data, &options);
    total_iterations = options.iterations * (uint64_t)options.threads;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (size_t i = 0U; i < options.threads; ++i) {
        args[i] = (worker_arg_t){ data, &options, i, 0U, 0U, 0 };
        if (pthread_create(&threads[i], NULL, worker, &args[i]) != 0) {
            fprintf(stderr, "pthread_create failed\n"); return EXIT_FAILURE;
        }
    }
    for (size_t i = 0U; i < options.threads; ++i) {
        if (pthread_join(threads[i], NULL) != 0 || args[i].failed) {
            fprintf(stderr, "worker failed\n"); return EXIT_FAILURE;
        }
        checksum ^= args[i].checksum;
        successful += args[i].successful;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = seconds_between(start, end);
    printf("benchmark=bignum_to_string_mt data_mode=%s seed=%" PRIu64
           " threads=%zu iterations_per_thread=%" PRIu64
           " total_iterations=%" PRIu64 " data_count=%zu src_len=%zu"
           " successful=%" PRIu64 " fingerprint=%" PRIu64
           " checksum=%" PRIu64 " elapsed_seconds=%.9f ns_per_call=%.3f\n",
           mode_name(options.mode), options.seed, options.threads,
           options.iterations, total_iterations, options.data_count, options.src_len,
           successful, fingerprint(data, &options), checksum, elapsed,
           elapsed * 1000000000.0 / (double)total_iterations);
    free(data); free(threads); free(args); return EXIT_SUCCESS;
}

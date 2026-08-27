/**
 * @file bench_bignum_to_string.c
 * @brief Single-thread benchmark-framework entrypoint for bignum_to_string.
 */
#include <benchmark_framework.h>
#include "bignum_to_string_benchmark_adapter.h"
int main(int argc, char **argv)
{
    benchmark_adapter_t adapter;
    if (bignum_to_string_benchmark_adapter_init(&adapter) != BIGNUM_TO_STRING_BENCHMARK_STATUS_SUCCESS) return 2;
    benchmark_core_status_t status = benchmark_core_run_st(argc, argv, &adapter);
    return status == BENCHMARK_CORE_STATUS_SUCCESS || status == BENCHMARK_CORE_STATUS_HELP ? 0 : 1;
}

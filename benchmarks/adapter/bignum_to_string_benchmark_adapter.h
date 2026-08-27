/**
 * @file bignum_to_string_benchmark_adapter.h
 * @brief Public benchmark-framework adapter contract for bignum_to_string.
 * @details The adapter owns only per-invocation bignum fixture state. The
 * framework owns lifecycle, timing, worker scheduling, and result publication.
 */
#ifndef BIGNUM_TO_STRING_BENCHMARK_ADAPTER_H
#define BIGNUM_TO_STRING_BENCHMARK_ADAPTER_H
#include <benchmark_framework.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Named validation and callback-binding status. */
typedef enum bignum_to_string_benchmark_status {
    BIGNUM_TO_STRING_BENCHMARK_STATUS_SUCCESS = 0,
    BIGNUM_TO_STRING_BENCHMARK_STATUS_NULL_ARGUMENT = 1,
    BIGNUM_TO_STRING_BENCHMARK_STATUS_INVALID_PROFILE = 2
} bignum_to_string_benchmark_status_t;
/**
 * @brief Validates the framework workload vocabulary for this module.
 * @param[in] workload Borrowed framework descriptor; not modified or retained.
 * @return Named status; invalid inputs never start a callback run.
 */
bignum_to_string_benchmark_status_t bignum_to_string_benchmark_validate_workload(
    const benchmark_workload_t *workload);
/**
 * @brief Installs deterministic callbacks for the module benchmark.
 * @param[out] adapter Caller-allocated adapter storage written on success.
 * @return Named status; caller retains adapter ownership and lifetime.
 */
bignum_to_string_benchmark_status_t bignum_to_string_benchmark_adapter_init(
    benchmark_adapter_t *adapter);
#ifdef __cplusplus
}
#endif
#endif /* BIGNUM_TO_STRING_BENCHMARK_ADAPTER_H */

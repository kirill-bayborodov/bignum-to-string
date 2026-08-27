/**
 * @file bignum_to_string_benchmark_adapter.c
 * @brief Deterministic benchmark-framework callbacks for bignum_to_string.
 * @details Inputs are fixed from the framework seed and the operation result is
 * reduced into a nonzero checksum. No global state or heap ownership is used.
 */
#include "bignum_to_string_benchmark_adapter.h"
#include "bignum_to_string.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define FNV_OFFSET UINT64_C(1469598103934665603)
#define FNV_PRIME UINT64_C(1099511628211)
typedef struct bignum_to_string_benchmark_state {
    bignum_t a;
    bignum_t b;
    bignum_t result;
    uint64_t words[2];
    char text[128];
    uint64_t observation;
} bignum_to_string_benchmark_state_t;
/** @brief Validates non-NULL framework strings and this module operation kind. */
bignum_to_string_benchmark_status_t bignum_to_string_benchmark_validate_workload(
    const benchmark_workload_t *workload)
{
    if (workload == NULL) return BIGNUM_TO_STRING_BENCHMARK_STATUS_NULL_ARGUMENT;
    if (workload->operation_kind == NULL || workload->size_profile == NULL ||
        workload->capacity_profile == NULL) return BIGNUM_TO_STRING_BENCHMARK_STATUS_INVALID_PROFILE;
    if (strcmp(workload->operation_kind, "to_string") != 0 &&
        strcmp(workload->operation_kind, "mixed") != 0) return BIGNUM_TO_STRING_BENCHMARK_STATUS_INVALID_PROFILE;
    return BIGNUM_TO_STRING_BENCHMARK_STATUS_SUCCESS;
}
/** @brief Initializes a deterministic one-word and two-word fixture. */
static benchmark_adapter_status_t initialize(void *opaque, uint64_t index,
    const benchmark_workload_t *workload, void *context)
{
    bignum_to_string_benchmark_state_t *state = opaque;
    (void)context;
    if (state == NULL || bignum_to_string_benchmark_validate_workload(workload) != BIGNUM_TO_STRING_BENCHMARK_STATUS_SUCCESS)
        return BENCHMARK_ADAPTER_STATUS_INPUT_ERROR;
    memset(state, 0, sizeof(*state));
    state->a.len = 1U;
    state->a.words[0] = workload->seed ^ (index + UINT64_C(0x9e3779b97f4a7c15));
    if (state->a.words[0] == 0U) state->a.words[0] = 1U;
    state->b.len = 1U;
    state->b.words[0] = 3U;
    state->words[0] = state->a.words[0];
    state->words[1] = 5U;
    return BENCHMARK_ADAPTER_STATUS_SUCCESS;
}
/** @brief Executes one module operation and maps its status into framework status. */
static benchmark_adapter_status_t operation(void *opaque, uint64_t iteration,
    const benchmark_workload_t *workload, void *context)
{
    bignum_to_string_benchmark_state_t *state = opaque;
    int rc;
    (void)iteration; (void)workload; (void)context;
    if (state == NULL) return BENCHMARK_ADAPTER_STATUS_INPUT_ERROR;
    rc = (int)(bignum_to_string(state->text, sizeof(state->text), &state->a, 10));
    if (strcmp("to_string", "is_zero") == 0) {
        if (rc < 0) return BENCHMARK_ADAPTER_STATUS_OPERATION_ERROR;
        state->observation = (uint64_t)rc;
    } else if (rc != 0) {
        return BENCHMARK_ADAPTER_STATUS_OPERATION_ERROR;
    }
    return BENCHMARK_ADAPTER_STATUS_SUCCESS;
}
/** @brief Produces a result-sensitive FNV-1a checksum for observability. */
static uint64_t checksum(const void *opaque, uint64_t iteration, void *context)
{
    const bignum_to_string_benchmark_state_t *state = opaque;
    uint64_t hash = FNV_OFFSET;
    (void)context;
    if (state == NULL) return 0U;
    for (size_t i = 0U; i < BIGNUM_CAPACITY; ++i) {
        hash ^= state->result.words[i] ^ state->a.words[i];
        hash *= FNV_PRIME;
    }
    for (size_t i = 0U; i < sizeof(state->text) && state->text[i] != '\0'; ++i) {
        hash ^= (unsigned char)state->text[i];
        hash *= FNV_PRIME;
    }
    return (hash ^ state->observation ^ iteration) | UINT64_C(1);
}
/** @brief Binds the deterministic lifecycle callbacks and state size. */
bignum_to_string_benchmark_status_t bignum_to_string_benchmark_adapter_init(benchmark_adapter_t *adapter)
{
    if (adapter == NULL) return BIGNUM_TO_STRING_BENCHMARK_STATUS_NULL_ARGUMENT;
    *adapter = (benchmark_adapter_t){
        .benchmark_name = "bignum_to_string", .state_size = sizeof(bignum_to_string_benchmark_state_t),
        .success_code = BENCHMARK_ADAPTER_STATUS_SUCCESS, .adapter_context = NULL,
        .initialize = initialize, .operation = operation, .checksum = checksum
    };
    return BIGNUM_TO_STRING_BENCHMARK_STATUS_SUCCESS;
}

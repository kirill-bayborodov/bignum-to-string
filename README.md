# bignum-init-from-array

[![C/ASM CI](https://github.com/kirill-bayborodov/bignum-init-from-array/actions/workflows/ci.yml/badge.svg)](https://github.com/kirill-bayborodov/bignum-init-from-array/actions/workflows/ci.yml)
[![GitHub release](https://img.shields.io/github/v/release/kirill-bayborodov/bignum-init-from-array?label=release)](https://github.com/kirill-bayborodov/bignum-init-from-array/releases/latest)

`bignum-init-from-array` is a standalone C/ASM module that initializes a `bignum_t` from an array of 64-bit words. The input uses little-endian word order: `src[0]` is the least significant word. The production path is an x86-64 YASM implementation conforming to the System V AMD64 ABI; a portable C implementation is retained as a reference and fallback.

The operation validates its pointers and length before accessing memory, clears the complete destination on successful calls, copies the requested words, and normalizes `len` by removing high zero words. It does not support overlapping `src` and `dst` regions.

## Distribution

The module is intended to be used as a standalone component of the `bignum-lib` family. The required `bignum-core` component is included as a Git submodule at `libs/bignum-core`.

## Features

- **Dual implementation:** x86-64 YASM is the primary implementation and C11 is the reference implementation.
- **Dedicated status enum:** the public API uses `bignum_init_from_array_status_t` rather than the core library status type.
- **Safe error handling:** NULL arguments and capacity overflow return before modifying `dst`.
- **Normalized representation:** copied high zero words are removed from `len`, while the complete word buffer remains zero-filled beyond the copied data.
- **Deterministic verification:** tests cover normal arrays, empty arrays, all-zero arrays, normalization, errors, overflow, and the exact capacity boundary.
- **Extended verification:** canaries, adjacent objects, state transitions, destination preservation, and 100,000 deterministic fuzz/reference cases are covered.
- **Thread-safety testing:** independent destinations are initialized concurrently.
- **Reproducible benchmarks:** ST and MT benchmarks support deterministic seeds, fingerprints, checksums, warm-up calls, source lengths, and `all_zero`, `all_nonzero`, and `mixed` modes.
- **Perf workflow:** the unchanged template Makefile provides `perf record`, repeated `perf stat`, raw `perf.data` retention, runtime validation, and comparison targets.

## Dependencies

| Dependency | Purpose |
|---|---|
| `make` | Build, test, lint, benchmark, and distribution targets |
| `gcc` | C compilation and linking |
| `yasm` | x86-64 assembly compilation |
| `cppcheck` | Static analysis |
| `perf` | Performance counters and sampling profiles |
| `taskset` | CPU affinity control |
| `pthread` | Multithreaded tests and benchmarks |

Clone the repository with its submodule:

```bash
git clone --recurse-submodules https://github.com/kirill-bayborodov/bignum-init-from-array.git
cd bignum-init-from-array
```

For an existing clone, initialize the submodule with:

```bash
git submodule update --init --recursive
```

## API

The public API is declared in `include/bignum_init_from_array.h`:

```c
typedef enum {
    BIGNUM_INIT_FROM_ARRAY_SUCCESS        = 0,
    BIGNUM_INIT_FROM_ARRAY_ERROR_NULL_ARG = -1,
    BIGNUM_INIT_FROM_ARRAY_ERROR_OVERFLOW = -2
} bignum_init_from_array_status_t;

bignum_init_from_array_status_t bignum_init_from_array(
    bignum_t *restrict dst,
    const uint64_t *restrict src,
    size_t src_len);
```

### Contract

| Condition | Return value | Destination behavior |
|---|---|---|
| `dst == NULL` | `BIGNUM_INIT_FROM_ARRAY_ERROR_NULL_ARG` | No memory access; unchanged by contract |
| `src == NULL` | `BIGNUM_INIT_FROM_ARRAY_ERROR_NULL_ARG` | No memory access; unchanged by contract |
| `src_len > BIGNUM_CAPACITY` | `BIGNUM_INIT_FROM_ARRAY_ERROR_OVERFLOW` | Unchanged by contract |
| `src != NULL`, `src_len == 0` | `BIGNUM_INIT_FROM_ARRAY_SUCCESS` | Fully cleared; `len == 0` |
| Valid non-empty array | `BIGNUM_INIT_FROM_ARRAY_SUCCESS` | Words copied, tail cleared, high zero words excluded from `len` |
| Overlapping `src` and `dst` | Not supported | The caller must provide non-overlapping regions |

The NULL and overflow checks occur before any destination write. A successful call always leaves the complete `bignum_t` in a deterministic state. For example:

```c
#include <stdint.h>
#include "bignum_init_from_array.h"

int initialize_value(bignum_t *dst)
{
    const uint64_t words[] = {
        UINT64_C(0x0123456789abcdef),
        UINT64_C(0x10),
        UINT64_C(0)
    };

    bignum_init_from_array_status_t status =
        bignum_init_from_array(dst, words, 3U);

    return status == BIGNUM_INIT_FROM_ARRAY_SUCCESS ? 0 : -1;
}
```

After this call, the value has `len == 2`; `words[2]` and all following words are zero.

## Build and test

Build the release object and submodule:

```bash
make build CONFIG=release
```

The production object is generated at:

```text
build/bignum_init_from_array.o
```

Run the full deterministic, extended, multithreaded, and integration-runner suite against the ASM implementation:

```bash
make test CONFIG=release
```

The expected summary is:

```text
=== Summary: 0 / 4 failed ===
```

To test the portable C reference implementation instead of the ASM implementation:

```bash
make clean
make test CONFIG=release USE_ASM=no
```

Run static analysis:

```bash
make lint
```

The test files are organized as follows:

| File | Scope |
|---|---|
| `tests/test_bignum_init_from_array.c` | Deterministic contract and boundary tests |
| `tests/test_bignum_init_from_array_extra.c` | Canaries, preservation, transitions, fuzz/reference checks, adjacent objects |
| `tests/test_bignum_init_from_array_mt.c` | Concurrent independent-object checks |
| `tests/test_bignum_init_from_array_runner.c` | Integration smoke test |

## Benchmarks

The benchmark sources are:

```text
benchmarks/bench_bignum_init_from_array.c
benchmarks/bench_bignum_init_from_array_mt.c
```

Each benchmark generates deterministic arrays of up to `BIGNUM_CAPACITY` words and reports the data mode, seed, fingerprint, checksum, successful-call count, elapsed time, and nanoseconds per call.

| Mode | Input pattern | Purpose |
|---|---|---|
| `all_zero` | Every supplied word is zero | Measures the full normalization scan and zero result path |
| `all_nonzero` | Supplied words are nonzero, including the highest supplied word | Measures the nonzero copy path without high-zero trimming |
| `mixed` | Alternating zero and nonzero rows | Measures a mixed workload and branch behavior |

### Single-thread CLI

```text
bin/bench_bignum_init_from_array \
  [--iterations N] \
  [--warmup N] \
  [--data-count N] \
  [--src-len N] \
  [--seed N] \
  [--data-mode all_zero|all_nonzero|mixed]
```

Example:

```bash
./bin/bench_bignum_init_from_array \
  --iterations 1000000 \
  --warmup 10000 \
  --data-count 8192 \
  --src-len 32 \
  --seed 0x9e3779b97f4a7c15 \
  --data-mode mixed
```

### Multithread CLI

```text
bin/bench_bignum_init_from_array_mt \
  [--threads N] \
  [--iterations N|--total-iterations N] \
  [--warmup N] \
  [--data-count N] \
  [--src-len N] \
  [--seed N] \
  [--data-mode all_zero|all_nonzero|mixed]
```

`--iterations` means iterations per thread. `--total-iterations` specifies total work and must be nonzero and divisible by `--threads`; the benchmark derives the per-thread count internally.

For a fair one-thread/two-thread comparison, hold total work constant:

```bash
./bin/bench_bignum_init_from_array_mt \
  --threads 1 \
  --total-iterations 3200000000 \
  --src-len 32 \
  --data-mode mixed

./bin/bench_bignum_init_from_array_mt \
  --threads 2 \
  --total-iterations 3200000000 \
  --src-len 32 \
  --data-mode mixed
```

## Perf workflow

The current environment provides two logical CPUs. The corresponding MT settings are:

```make
MT_THREADS=2
MT_CPU_LIST=0-1
MT_TOTAL_ITERATIONS=3200000000
```

Run the complete ST/MT workflow for the supported data modes:

```bash
make bench_full CONFIG=release \
  REPORT_NAME=baseline \
  PERF_RUNS=7 \
  KEEP_PERF=1
```

For targeted repeated counter measurements:

```bash
make bench_stat_st CONFIG=release \
  REPORT_NAME=baseline_st_mixed \
  DATA_MODE=mixed \
  PERF_RUNS=7

make bench_stat_mt CONFIG=release \
  REPORT_NAME=baseline_mt_mixed \
  DATA_MODE=mixed \
  MT_THREADS=2 \
  MT_CPU_LIST=0-1 \
  MT_TOTAL_ITERATIONS=3200000000 \
  PERF_RUNS=7
```

Reports are written to `benchmarks/reports/`. With `KEEP_PERF=1`, raw profiles are retained as `.perf.data` files. Runtime validation checks the dynamic benchmark identifier generated from `LIB_NAME`, the selected data mode, and the elapsed-time field.

A reproducible optimization comparison should keep `CONFIG`, `PERF_RUNS`, `DATA_MODE`, `src_len`, seed, thread count, CPU affinity, and total iterations constant:

```bash
make clean
make test CONFIG=release
make bench_full CONFIG=release REPORT_NAME=baseline PERF_RUNS=7 KEEP_PERF=1

# Change implementation, then repeat the verification.
make clean
make test CONFIG=release
make bench_full CONFIG=release REPORT_NAME=opt_v1 PERF_RUNS=7 KEEP_PERF=1
```

Compare matching reports only:

```bash
diff -u \
  benchmarks/reports/baseline_all_nonzero_st_stat.csv \
  benchmarks/reports/opt_v1_all_nonzero_st_stat.csv
```

## Installation and distribution

Build the object-file distribution:

```bash
make install CONFIG=release
```

Build the single-header and static-library distribution:

```bash
make dist CONFIG=release
```

Remove generated artifacts:

```bash
make clean
```

## Linking the object file

```bash
make build CONFIG=release

gcc your_app.c \
  build/bignum_init_from_array.o \
  -I./include \
  -I./libs/bignum-core/include \
  -o your_app \
  -no-pie
```

The application must use the same System V AMD64 ABI and include the `bignum_t` definition supplied by `bignum-core`.

## Contributing

Contributions should preserve the C/ASM API contract, add or update deterministic and extended tests, and run both `make test CONFIG=release` and `make lint`. Performance changes should include reproducible benchmark parameters and matching ST/MT evidence.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.


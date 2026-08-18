# bignum-to-string

[![C/ASM CI](https://github.com/kirill-bayborodov/bignum-to-string/actions/workflows/ci.yml/badge.svg)](https://github.com/kirill-bayborodov/bignum-to-string/actions/workflows/ci.yml)
[![GitHub release](https://img.shields.io/github/v/release/kirill-bayborodov/bignum-to-string?label=release)](https://github.com/kirill-bayborodov/bignum-to-string/releases/latest)

`bignum-to-string` is a standalone C/ASM module that converts an unsigned `bignum_t` magnitude to a NUL-terminated string in base 2, 8, 10, or 16. The input uses little-endian word order: `src->words[0]` is the least significant word. The production path is an x86-64 YASM implementation conforming to the System V AMD64 ABI; a portable C implementation is retained as a reference and fallback.

The operation validates pointers, the selected base, capacity, and canonical `bignum_t` representation before accessing the value. It reports the required destination size before writing, never modifies the source object, and does not use dynamic allocation or external library calls in the ASM implementation. Signs are not part of the contract: the value is always treated as an unsigned magnitude.

## Distribution

The module is intended to be used as a standalone component of the `bignum-lib` family. The required `bignum-core` component is included as a Git submodule at `libs/bignum-core`.

## Features

- **Dual implementation:** x86-64 YASM is the primary implementation and C11 is the reference implementation.
- **Dedicated status enum:** the public API uses `bignum_to_string_status_t` rather than an unrelated core-library result type.
- **Four supported bases:** base 2, base 8, base 10, and base 16 are supported explicitly; no automatic base selection or prefix parsing is performed.
- **Unsigned-magnitude contract:** leading signs are not accepted as input because the API receives a `bignum_t`, not a textual signed value.
- **Canonical input validation:** `len == 0` represents zero; for nonzero values the highest used word must be nonzero; lengths above `BIGNUM_CAPACITY` are rejected.
- **Size-query API:** `bignum_to_string_size()` calculates the exact required destination size, including the terminating NUL byte, before conversion.
- **P0 ASM optimizations:** the production path uses BSR-based high-bit counting and avoids the legacy `LOOP` instruction in the decimal chunk writer.
- **Base2 byte LUT:** the optimized binary path emits the leading partial group and converts subsequent complete bytes through a read-only `binary_byte_lut`.
- **Base8 reservoir extraction:** the optimized octal path extracts three-bit digits directly and handles the offset-0 and offset-1 cross-word cases explicitly.
- **Base16 byte-pair LUT:** the optimized hexadecimal path emits a leading nibble when necessary and converts complete byte pairs through a read-only `hex_pair_lut`.
- **Decimal conversion:** base 10 operates on a local copy and repeatedly divides by `10^9`, preserving the source value and avoiding temporary heap allocation.
- **Safe buffer behavior:** insufficient destination space returns before any partial result is written.
- **Deterministic verification:** tests cover zero, one, word boundaries, full capacity, all four bases, invalid values, invalid bases, null arguments, and insufficient buffers.
- **Extended verification:** 10,000 deterministic randomized values cover Base2, Base8, and Base16 conversion, canary buffers, boundary values, and reference equivalence.
- **Thread-safety testing:** independent read-only source values are converted concurrently into independent destination buffers.
- **Reproducible benchmarks:** single-thread and multithread benchmark programs report deterministic inputs, successful calls, fingerprints, checksums, elapsed time, and nanoseconds per call.
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
git clone --recurse-submodules https://github.com/kirill-bayborodov/bignum-to-string.git
cd bignum-to-string
```

For an existing clone, initialize the submodule with:

```bash
git submodule update --init --recursive
```

## API

The public API is declared in `include/bignum_to_string.h`:

```c
typedef enum {
    BIGNUM_TO_STRING_SUCCESS        = 0,
    BIGNUM_TO_STRING_ERROR_NULL_ARG = -1,
    BIGNUM_TO_STRING_ERROR_BAD_BASE = -2,
    BIGNUM_TO_STRING_ERROR_NO_SPACE = -3,
    BIGNUM_TO_STRING_ERROR_INVALID   = -4
} bignum_to_string_status_t;

bignum_to_string_status_t bignum_to_string_size(
    const bignum_t *src,
    int base,
    size_t *required_size);

bignum_to_string_status_t bignum_to_string(
    char *dst,
    size_t dst_size,
    const bignum_t *src,
    int base);
```

The public header exposes a dedicated `bignum_to_string_status_t` enum. The module-specific status names are `BIGNUM_TO_STRING_SUCCESS`, `BIGNUM_TO_STRING_ERROR_NULL_ARG`, `BIGNUM_TO_STRING_ERROR_BAD_BASE`, `BIGNUM_TO_STRING_ERROR_NO_SPACE`, and `BIGNUM_TO_STRING_ERROR_INVALID`.

### Contract

| Condition | Return value | Destination behavior |
|---|---|---|
| `src == NULL` | `BIGNUM_TO_STRING_ERROR_NULL_ARG` | No source or destination access |
| `dst == NULL` for `bignum_to_string()` | `BIGNUM_TO_STRING_ERROR_NULL_ARG` | No destination access |
| `required_size == NULL` for `bignum_to_string_size()` | `BIGNUM_TO_STRING_ERROR_NULL_ARG` | No size result is written |
| `base` is not 2, 8, 10, or 16 | `BIGNUM_TO_STRING_ERROR_BAD_BASE` | No conversion is performed |
| `src->len > BIGNUM_CAPACITY` | `BIGNUM_TO_STRING_ERROR_INVALID` | No conversion is performed |
| `src->len != 0` and the highest used word is zero | `BIGNUM_TO_STRING_ERROR_INVALID` | No conversion is performed |
| `dst_size < required_size` | `BIGNUM_TO_STRING_ERROR_NO_SPACE` | No partial result is written |
| `src->len == 0` | `BIGNUM_TO_STRING_SUCCESS` | The string `"0"` is written |
| Valid normalized value that fits | `BIGNUM_TO_STRING_SUCCESS` | A complete NUL-terminated unsigned string is written |

The size returned by `bignum_to_string_size()` includes the terminating NUL byte. A zero value requires two bytes. For a 2048-bit maximum-capacity value, the maximum required sizes are 2049 bytes for Base2, 684 bytes for Base8, 618 bytes for Base10, and 513 bytes for Base16.

The conversion never changes `src`. Base16 uses lowercase hexadecimal digits. No sign, prefix, or signed interpretation is applied because the API operates on an already constructed unsigned `bignum_t` value.

Example:

```c
#include "bignum_to_string.h"

int stringify_value(const bignum_t *src, char *buffer, size_t buffer_size)
{
    size_t required_size = 0U;

    if (bignum_to_string_size(src, 16, &required_size) !=
        BIGNUM_TO_STRING_SUCCESS) {
        return -1;
    }
    if (buffer_size < required_size) {
        return -1;
    }
    return bignum_to_string(buffer, buffer_size, src, 16) ==
                   BIGNUM_TO_STRING_SUCCESS
               ? 0
               : -1;
}
```

For a normalized value with one word containing `0x75bcd15`, the resulting Base16 string is `75bcd15`, the Base10 string is `123456789`, the Base8 string is `726746425`, and the Base2 string is `111010110111100110100010101`.

## Build and test

Build the release object and submodule:

```bash
make build CONFIG=release
```

The production object is generated at:

```text
build/bignum_to_string.o
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

The optional sanitizer and race-oriented checks are available through the unchanged Makefile:

```bash
make test_sanitize CONFIG=release SAN=address
make test_sanitize CONFIG=release SAN=undefined
make test_helgrind CONFIG=release
```

The test files are organized as follows:

| File | Scope |
|---|---|
| `tests/test_bignum_to_string.c` | Deterministic API, status, buffer, base, and word-boundary tests |
| `tests/test_bignum_to_string_extra.c` | Canaries, invalid values, boundary cases, randomized Base2/Base8/Base16 checks, and buffer safety |
| `tests/test_bignum_to_string_mt.c` | Concurrent independent-source and destination checks |
| `tests/test_bignum_to_string_runner.c` | Integration smoke test for all four bases |

The extended suite performs 10,000 reproducible randomized values for the power-of-two bases and checks the generated strings against an independent reference model. The multithreaded suite uses independent destination buffers and verifies that the read-only source value remains safe under concurrent access.

The portable C fallback has been instrumented separately with GCC coverage. The direct `gcov -b -c` report for revision 0.5.0 records 173/173 executed lines, 120/120 executed branches, 120/120 branches taken, and 9/9 executed calls. The detailed report is retained in `coverage/coverage_report.md` and the accompanying `coverage/` HTML and text files.

## Benchmarks

The benchmark sources are:

```text
benchmarks/bench_bignum_to_string.c
benchmarks/bench_bignum_to_string_mt.c
```

The benchmark programs exercise Base2, Base8, Base10, and Base16 conversion and report elapsed time, successful-call count, checksum, and nanoseconds per call. The single-thread benchmark uses a deterministic fixed 2048-bit input and cycles through all four bases automatically. Its optional positional argument specifies the iteration count.

The multithread benchmark generates deterministic `bignum_t` data and supports configurable thread count, iteration count, warm-up calls, data count, source length, seed, and data mode. Its worker workload alternates Base10 and Base16 conversions, matching the current benchmark source; use the dedicated four-base single-thread benchmark when comparing all power-of-two paths directly.

| Mode | Input pattern | Purpose |
|---|---|---|
| `all_zero` | Every generated word is zero | Measures the zero-result and normalization paths |
| `all_nonzero` | Supplied words are nonzero, including the highest supplied word | Measures the regular nonzero conversion path without high-zero trimming |
| `mixed` | Deterministic mixture of zero and nonzero words | Measures mixed input and branch behavior |

### Single-thread CLI

The current single-thread benchmark accepts an optional positional iteration count and cycles through all supported bases:

```text
bin/bench_bignum_to_string [iterations]
```

Example:

```bash
./bin/bench_bignum_to_string 1000000
```

The output includes the iteration count, the fixed base sequence `2,8,10,16`, elapsed nanoseconds, nanoseconds per call, and a checksum.

### Multithread CLI

```text
bin/bench_bignum_to_string_mt \
  [--threads N] \
  [--iterations N|--total-iterations N] \
  [--warmup N] \
  [--data-count N] \
  [--src-len N] \
  [--seed N] \
  [--data-mode all_zero|all_nonzero|mixed]
```

`--iterations` means iterations per thread. `--total-iterations` specifies total work and should be chosen as a multiple of `--threads` for fair comparisons; the benchmark derives the per-thread count internally. `--warmup` controls warm-up calls, `--data-count` controls the generated input set, `--src-len` controls the number of used words, and `--seed` makes data generation reproducible.

For a fair one-thread/two-thread comparison, hold total work, source length, data count, seed, warm-up count, and data mode constant:

```bash
./bin/bench_bignum_to_string_mt \
  --threads 1 \
  --total-iterations 3200000 \
  --warmup 10000 \
  --data-count 8192 \
  --src-len 32 \
  --seed 0x9e3779b97f4a7c15 \
  --data-mode mixed

./bin/bench_bignum_to_string_mt \
  --threads 2 \
  --total-iterations 3200000 \
  --warmup 10000 \
  --data-count 8192 \
  --src-len 32 \
  --seed 0x9e3779b97f4a7c15 \
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

Reports are written to `benchmarks/reports/`. With `KEEP_PERF=1`, raw profiles are retained as `.perf.data` files. Runtime validation checks the dynamic benchmark identifier generated from `LIB_NAME`, the selected data mode, and the elapsed-time field. The ASM workflow filters the report to the symbols generated by `src/bignum_to_string.asm`.

A reproducible optimization comparison should keep `CONFIG`, `PERF_RUNS`, `DATA_MODE`, source length, seed, thread count, CPU affinity, warm-up count, and total iterations constant:

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

A full `perf stat` or `perf record` comparison depends on PMU access in the execution environment. Correctness is determined by the test suites, while performance conclusions should use retained benchmark parameters and matching reports.

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

The object-file distribution contains the public header, bundled `bignum-core` declarations, and the object files required by the module. The `dist` target additionally creates a single-header distribution and `libbignum_to_string.a`, together with the license, README, and integration runner. The generated distribution runner is compiled and executed as part of the distribution smoke check.

## Linking the object file

```bash
make build CONFIG=release

gcc your_app.c \
  build/bignum_to_string.o \
  -I./include \
  -I./libs/bignum-core/include \
  -o your_app \
  -no-pie
```

The application must use the same System V AMD64 ABI and include the `bignum_t` definition supplied by `bignum-core`. The ASM object is self-contained and does not require libc conversion routines.

## Contributing

Contributions should preserve the typed status contract, the unsigned-magnitude semantics, the normalized-input requirements, and the no-partial-write behavior on insufficient buffers. New behavior must include deterministic tests and, where appropriate, independent reference-model fuzz coverage. Every change should run both `make test CONFIG=release` and `make test CONFIG=release USE_ASM=no`, together with `make lint`.

Performance changes should include reproducible benchmark parameters, matching single-thread and multithread evidence where applicable, and an explanation of the affected ASM hot path. The Makefile is part of the repository template and must not be modified without direct authorization.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

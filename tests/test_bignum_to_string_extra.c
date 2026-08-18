
/**
 * @file    test_bignum_to_string_extra.c
 * @brief   Extended and fuzz-style tests for bignum_to_string.
 * @version 0.5.0
 * @details Revision 0.5.0: canaries, randomized four-base values, LUT boundaries and buffer checks.
 */

#include "bignum_to_string.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t next_random(uint64_t *state)
{
    *state = *state * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
    return *state;
}

static char hex_digit_ref(unsigned value)
{
    return (char)(value < 10U ? '0' + value : 'a' + value - 10U);
}

static void reference_hex(char *out, const bignum_t *value)
{
    size_t pos = 0U;
    if (value->len == 0U) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    size_t i = value->len;
    uint64_t word = value->words[i - 1U];
    unsigned shift = 60U;
    int started = 0;
    for (;;) {
        unsigned nibble = (unsigned)((word >> shift) & UINT64_C(0xF));
        if (started || nibble != 0U || shift == 0U) {
            out[pos++] = hex_digit_ref(nibble);
            started = 1;
        }
        if (shift == 0U) break;
        shift -= 4U;
    }
    while (i > 1U) {
        --i;
        word = value->words[i - 1U];
        for (shift = 60U;; shift -= 4U) {
            out[pos++] = hex_digit_ref((unsigned)((word >> shift) & UINT64_C(0xF)));
            if (shift == 0U) break;
        }
    }
    out[pos] = '\0';
}

static void reference_power2(char *out, const bignum_t *value, unsigned shift)
{
    size_t bits = 0U;
    size_t pos = 0U;
    if (value->len == 0U) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    uint64_t high = value->words[value->len - 1U];
    while (high != 0U) {
        ++bits;
        high >>= 1U;
    }
    bits += 64U * (value->len - 1U);
    size_t digits = (bits + shift - 1U) / shift;
    unsigned first_width = (unsigned)(bits % shift);
    if (first_width == 0U) first_width = shift;
    size_t top = bits - 1U;
    for (size_t digit = 0U; digit < digits; ++digit) {
        unsigned width = digit == 0U ? first_width : shift;
        unsigned current = 0U;
        for (unsigned bit = 0U; bit < width; ++bit) {
            size_t position = top - bit;
            current = (current << 1U) |
                      (unsigned)((value->words[position / 64U] >>
                                  (position % 64U)) & UINT64_C(1));
        }
        out[pos++] = (char)('0' + current);
        top -= width;
    }
    out[pos] = '\0';
}

static void test_fuzz_power2_and_hex(void)
{
    uint64_t state = UINT64_C(0x123456789abcdef0);
    for (size_t iteration = 0U; iteration < 10000U; ++iteration) {
        bignum_t value = { { 0U }, 0U };
        char expected[2050];
        char actual[2050];
        value.len = (size_t)(next_random(&state) % (BIGNUM_CAPACITY + 1U));
        for (size_t i = 0U; i < value.len; ++i) {
            value.words[i] = next_random(&state);
        }
        if (value.len != 0U && value.words[value.len - 1U] == 0U) {
            value.words[value.len - 1U] = 1U;
        }
        reference_power2(expected, &value, 1U);
        assert(bignum_to_string(actual, sizeof(actual), &value, 2) ==
               BIGNUM_TO_STRING_SUCCESS);
        assert(strcmp(actual, expected) == 0);
        reference_power2(expected, &value, 3U);
        assert(bignum_to_string(actual, sizeof(actual), &value, 8) ==
               BIGNUM_TO_STRING_SUCCESS);
        assert(strcmp(actual, expected) == 0);
        reference_hex(expected, &value);
        assert(bignum_to_string(actual, sizeof(actual), &value, 16) ==
               BIGNUM_TO_STRING_SUCCESS);
        assert(strcmp(actual, expected) == 0);
    }
}

static void test_canary(void)
{
    struct {
        uint64_t before;
        char output[2050];
        uint64_t after;
    } guarded = { UINT64_C(0x1111222233334444), { 0 }, UINT64_C(0xaaaabbbbccccdddd) };
    bignum_t value = { { UINT64_MAX }, 1U };
    assert(bignum_to_string(guarded.output, sizeof(guarded.output), &value, 2) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(guarded.before == UINT64_C(0x1111222233334444));
    assert(guarded.after == UINT64_C(0xaaaabbbbccccdddd));
}

int main(void)
{
    test_fuzz_power2_and_hex();
    test_canary();
    puts("--- All extended bignum_to_string tests passed ---");
    return 0;
}

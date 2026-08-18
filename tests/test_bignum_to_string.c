
/**
 * @file    test_bignum_to_string.c
 * @brief   Deterministic tests for bignum_to_string.
 * @version 0.5.0
 * @details Revision 0.5.0: deterministic four-base, status, word-boundary and LUT regression coverage.
 */

#include "bignum_to_string.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void expect_string(const bignum_t *value, int base, const char *expected)
{
    char output[618];
    size_t required = 0U;
    assert(bignum_to_string_size(value, base, &required) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(required == strlen(expected) + 1U);
    assert(bignum_to_string(output, sizeof(output), value, base) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(strcmp(output, expected) == 0);
}

static void test_zero_and_one(void)
{
    bignum_t zero = { { 0U }, 0U };
    bignum_t one = { { 1U }, 1U };
    expect_string(&zero, 2, "0");
    expect_string(&zero, 8, "0");
    expect_string(&zero, 10, "0");
    expect_string(&zero, 16, "0");
    expect_string(&one, 2, "1");
    expect_string(&one, 8, "1");
    expect_string(&one, 10, "1");
    expect_string(&one, 16, "1");
}

static void test_word_boundaries(void)
{
    bignum_t value = { { UINT64_MAX, 1U }, 2U };
    expect_string(&value, 2, "11111111111111111111111111111111111111111111111111111111111111111");
    expect_string(&value, 8, "3777777777777777777777");
    expect_string(&value, 10, "36893488147419103231");
    expect_string(&value, 16, "1ffffffffffffffff");
}

static void test_hex_full_capacity(void)
{
    bignum_t value = { { 0U }, BIGNUM_CAPACITY };
    char expected[513];
    char output[513];
    size_t pos = 0U;

    for (size_t i = 0U; i < BIGNUM_CAPACITY; ++i) {
        value.words[i] = UINT64_MAX;
    }
    for (size_t i = 0U; i < 512U; ++i) {
        expected[pos++] = 'f';
    }
    expected[pos] = '\0';
    assert(bignum_to_string(output, sizeof(output), &value, 16) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(strcmp(output, expected) == 0);
}

static void test_buffer_and_status_errors(void)
{
    bignum_t value = { { 0x1234U }, 1U };
    char output[32];
    size_t required = 0U;

    assert(bignum_to_string_size(&value, 16, &required) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(required == 5U);
    assert(bignum_to_string(output, 4U, &value, 16) ==
           BIGNUM_TO_STRING_ERROR_NO_SPACE);
    assert(bignum_to_string(output, sizeof(output), &value, 3) ==
           BIGNUM_TO_STRING_ERROR_BAD_BASE);
    assert(bignum_to_string(output, sizeof(output), &value, 2) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(strcmp(output, "1001000110100") == 0);
    assert(bignum_to_string(output, sizeof(output), &value, 8) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(strcmp(output, "11064") == 0);
    assert(bignum_to_string(output, sizeof(output), NULL, 16) ==
           BIGNUM_TO_STRING_ERROR_NULL_ARG);
    assert(bignum_to_string(NULL, sizeof(output), &value, 16) ==
           BIGNUM_TO_STRING_ERROR_NULL_ARG);
    assert(bignum_to_string_size(&value, 16, NULL) ==
           BIGNUM_TO_STRING_ERROR_NULL_ARG);
    assert(bignum_to_string_size(NULL, 16, &required) ==
           BIGNUM_TO_STRING_ERROR_NULL_ARG);
}


static void test_invalid_bignum(void)
{
    bignum_t invalid = { { 1U, 0U }, 2U };
    bignum_t oversized = { { 1U }, BIGNUM_CAPACITY + 1U };
    char output[32];
    assert(bignum_to_string(output, sizeof(output), &invalid, 16) ==
           BIGNUM_TO_STRING_ERROR_INVALID);
    assert(bignum_to_string(output, sizeof(output), &oversized, 16) ==
           BIGNUM_TO_STRING_ERROR_INVALID);
}

int main(void)
{
    puts("--- Starting deterministic bignum_to_string tests ---");
    test_zero_and_one();
    test_word_boundaries();
    test_hex_full_capacity();
    test_buffer_and_status_errors();
    test_invalid_bignum();
    puts("--- All deterministic bignum_to_string tests passed ---");
    return 0;
}


/**
 * @file    test_bignum_to_string_runner.c
 * @brief   Integration runner for bignum_to_string.
 * @version 0.5.0
 * @details Revision 0.5.0: end-to-end four-base coverage for optimized ASM paths.
 */

#include "bignum_to_string.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    bignum_t value = { { 123456789U }, 1U };
    char output[64];
    size_t required = 0U;

    assert(bignum_to_string_size(&value, 10, &required) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(bignum_to_string(output, required, &value, 10) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(strcmp(output, "123456789") == 0);
    assert(bignum_to_string(output, sizeof(output), &value, 16) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(strcmp(output, "75bcd15") == 0);
    assert(bignum_to_string(output, sizeof(output), &value, 2) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(strcmp(output, "111010110111100110100010101") == 0);
    assert(bignum_to_string(output, sizeof(output), &value, 8) ==
           BIGNUM_TO_STRING_SUCCESS);
    assert(strcmp(output, "726746425") == 0);

    puts("--- Integration bignum_to_string runner passed ---");
    return 0;
}

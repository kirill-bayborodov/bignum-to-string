
/**
 * @file    bignum_to_string.c
 * @brief   Portable conversion of bignum_t to Base2, Base8, Base10 or Base16.
 * @version 0.5.0
 * @details
 *   Revision 0.5.0: portable fallback remains unchanged and fully covered;
 *   optimized ASM adds Base2 byte LUT, Base8 reservoir extraction, and Base16
 *   byte-pair LUT paths.
 */

#include "bignum_to_string.h"

#include <stdint.h>

#define DECIMAL_CHUNK_BASE UINT64_C(1000000000)
#define DECIMAL_CHUNK_CAPACITY 70U

/**
 * @brief Проверяет допустимость основания и входного bignum_t.
 * @details Допускаются основания 2, 8, 10 и 16; ненормализованные значения отклоняются.
 */
static bignum_to_string_status_t validate_input(const bignum_t *src, int base)
{
    if (src == NULL) {
        return BIGNUM_TO_STRING_ERROR_NULL_ARG;
    }
    if (base != 2 && base != 8 && base != 10 && base != 16) {
        return BIGNUM_TO_STRING_ERROR_BAD_BASE;
    }
    if (src->len > BIGNUM_CAPACITY) {
        return BIGNUM_TO_STRING_ERROR_INVALID;
    }
    if (src->len != 0U && src->words[src->len - 1U] == UINT64_C(0)) {
        return BIGNUM_TO_STRING_ERROR_INVALID;
    }
    return BIGNUM_TO_STRING_SUCCESS;
}

/**
 * @brief Возвращает количество десятичных цифр беззнакового bignum_t.
 * @details Работает с локальной копией и делением на 10^9.
 */
static size_t decimal_digit_count(const bignum_t *src)
{
    uint64_t work[BIGNUM_CAPACITY];
    size_t len = src->len;
    size_t digits = 0U;

    if (len == 0U) {
        return 1U;
    }
    for (size_t i = 0U; i < len; ++i) {
        work[i] = src->words[i];
    }
    uint32_t most_significant_chunk = 0U;
    while (len != 0U) {
        uint64_t remainder = UINT64_C(0);
        for (size_t i = len; i != 0U; --i) {
            __uint128_t current = ((__uint128_t)remainder << 64U) |
                                  (__uint128_t)work[i - 1U];
            work[i - 1U] = (uint64_t)(current / DECIMAL_CHUNK_BASE);
            remainder = (uint64_t)(current % DECIMAL_CHUNK_BASE);
        }
        most_significant_chunk = (uint32_t)remainder;
        while (len != 0U && work[len - 1U] == UINT64_C(0)) {
            --len;
        }
        digits += 9U;
    }
    digits -= 9U;
    size_t tail_digits = 1U;
    while (most_significant_chunk >= 10U) {
        most_significant_chunk /= 10U;
        ++tail_digits;
    }
    return digits + tail_digits;
}

/**
 * @brief Преобразует nibble в строчную hexadecimal digit.
 * @details Функция не использует libc и не имеет изменяемого состояния.
 */
static char hex_digit(unsigned value)
{
    return (char)(value < 10U ? ('0' + value) : ('a' + value - 10U));
}

/**
 * @brief Записывает validated bignum_t в hexadecimal string.
 * @details Старшее слово выводится без ведущих нулей, остальные — по 16 digits.
 */
static void write_hex(char *dst, const bignum_t *src)
{
    size_t out = 0U;
    if (src->len == 0U) {
        dst[0] = '0';
        dst[1] = '\0';
        return;
    }
    size_t word = src->len;
    uint64_t value = src->words[word - 1U];
    unsigned shift = 60U;
    int started = 0;
    for (;;) {
        unsigned nibble = (unsigned)((value >> shift) & UINT64_C(0xF));
        if (started || nibble != 0U || shift == 0U) {
            dst[out++] = hex_digit(nibble);
            started = 1;
        }
        if (shift == 0U) {
            break;
        }
        shift -= 4U;
    }
    while (word > 1U) {
        --word;
        value = src->words[word - 1U];
        for (shift = 60U;; shift -= 4U) {
            dst[out++] = hex_digit((unsigned)((value >> shift) & UINT64_C(0xF)));
            if (shift == 0U) {
                break;
            }
        }
    }
    dst[out] = '\0';
}

/**
 * @brief Делит локальную binary copy на 10^9.
 * @details Возвращает остаток и обновляет только work и len.
 */
static void write_power_of_two(char *dst, const bignum_t *src, unsigned shift)
{
    const unsigned mask = (1U << shift) - 1U;
    size_t bits = 0U;
    size_t digits;
    size_t out = 0U;

    if (src->len == 0U) {
        dst[0] = '0';
        dst[1] = '\0';
        return;
    }
    uint64_t high = src->words[src->len - 1U];
    while (high != 0U) {
        ++bits;
        high >>= 1U;
    }
    bits += 64U * (src->len - 1U);
    digits = (bits + shift - 1U) / shift;
    unsigned first_width = (unsigned)(bits % shift);
    if (first_width == 0U) first_width = shift;
    size_t top_bit = bits - 1U;
    for (size_t digit = 0U; digit < digits; ++digit) {
        unsigned width = digit == 0U ? first_width : shift;
        unsigned value = 0U;
        for (unsigned bit = 0U; bit < width; ++bit) {
            size_t position = top_bit - bit;
            unsigned current = (unsigned)((src->words[position / 64U] >>
                                           (position % 64U)) & UINT64_C(1));
            value = (value << 1U) | current;
        }
        value &= mask;
        dst[out++] = (char)(value < 10U ? ('0' + value) : ('a' + value - 10U));
        top_bit -= width;
    }
    dst[out] = '\0';
}

/**
 * @brief Делит локальную binary copy на 10^9.
 * @details Возвращает остаток и обновляет только work и len.
 */
static uint32_t divide_by_decimal_chunk(uint64_t *work, size_t *len)
{
    uint64_t remainder = UINT64_C(0);
    for (size_t i = *len; i != 0U; --i) {
        __uint128_t current = ((__uint128_t)remainder << 64U) |
                              (__uint128_t)work[i - 1U];
        work[i - 1U] = (uint64_t)(current / DECIMAL_CHUNK_BASE);
        remainder = (uint64_t)(current % DECIMAL_CHUNK_BASE);
    }
    while (*len != 0U && work[*len - 1U] == UINT64_C(0)) {
        --(*len);
    }
    return (uint32_t)remainder;
}

/**
 * @brief Записывает validated bignum_t в decimal string.
 * @details Делит копию на 10^9 и выводит chunks в обратном порядке.
 */
static void write_decimal(char *dst, const bignum_t *src)
{
    uint64_t work[BIGNUM_CAPACITY];
    uint32_t chunks[DECIMAL_CHUNK_CAPACITY];
    size_t len = src->len;
    size_t chunk_count = 0U;
    size_t out = 0U;

    if (len == 0U) {
        dst[0] = '0';
        dst[1] = '\0';
        return;
    }
    for (size_t i = 0U; i < len; ++i) {
        work[i] = src->words[i];
    }
    while (len != 0U) {
        chunks[chunk_count++] = divide_by_decimal_chunk(work, &len);
    }

    uint32_t chunk = chunks[chunk_count - 1U];
    char reverse[9];
    size_t count = 0U;
    do {
        reverse[count++] = (char)('0' + (chunk % 10U));
        chunk /= 10U;
    } while (chunk != 0U);
    while (count != 0U) {
        dst[out++] = reverse[--count];
    }

    while (chunk_count > 1U) {
        chunk = chunks[--chunk_count - 1U];
        count = 0U;
        for (size_t i = 0U; i < 9U; ++i) {
            reverse[count++] = (char)('0' + (chunk % 10U));
            chunk /= 10U;
        }
        while (count != 0U) {
            dst[out++] = reverse[--count];
        }
    }
    dst[out] = '\0';
}

/**
 * @brief Вычисляет минимальный размер строки для Base2, Base8, Base10 или Base16.
 * @details Revision 0.5.0 portable fallback implementation.
 */
bignum_to_string_status_t bignum_to_string_size(
    const bignum_t *src,
    int base,
    size_t *required_size)
{
    bignum_to_string_status_t status = validate_input(src, base);
    if (status != BIGNUM_TO_STRING_SUCCESS) {
        return status;
    }
    if (required_size == NULL) {
        return BIGNUM_TO_STRING_ERROR_NULL_ARG;
    }

    if (base == 2 || base == 8) {
        unsigned shift = base == 2 ? 1U : 3U;
        size_t bits = 0U;
        if (src->len != 0U) {
            uint64_t high = src->words[src->len - 1U];
            while (high != 0U) {
                ++bits;
                high >>= 1U;
            }
            bits += 64U * (src->len - 1U);
        }
        *required_size = (bits == 0U ? 1U : (bits + shift - 1U) / shift) + 1U;
    } else if (base == 16) {
        size_t digits = 1U;
        if (src->len != 0U) {
            uint64_t high = src->words[src->len - 1U];
            unsigned high_digits = 0U;
            do {
                ++high_digits;
                high >>= 4U;
            } while (high != 0U);
            digits = (size_t)high_digits + 16U * (src->len - 1U);
        }
        *required_size = digits + 1U;
    } else {
        *required_size = decimal_digit_count(src) + 1U;
    }
    return BIGNUM_TO_STRING_SUCCESS;
}

/**
 * @brief Преобразует bignum_t в строку Base2, Base8, Base10 или Base16.
 * @details Revision 0.5.0 portable fallback implementation.
 */
bignum_to_string_status_t bignum_to_string(
    char *dst,
    size_t dst_size,
    const bignum_t *src,
    int base)
{
    size_t required_size = 0U;
    bignum_to_string_status_t status;

    if (dst == NULL || src == NULL) {
        return BIGNUM_TO_STRING_ERROR_NULL_ARG;
    }
    status = bignum_to_string_size(src, base, &required_size);
    if (status != BIGNUM_TO_STRING_SUCCESS) {
        return status;
    }
    if (dst_size < required_size) {
        return BIGNUM_TO_STRING_ERROR_NO_SPACE;
    }
    if (base == 2 || base == 8) {
        write_power_of_two(dst, src, base == 2 ? 1U : 3U);
    } else if (base == 16) {
        write_hex(dst, src);
    } else {
        write_decimal(dst, src);
    }
    return BIGNUM_TO_STRING_SUCCESS;
}

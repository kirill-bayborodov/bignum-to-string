
/**
 * @file    bignum_to_string.h
 * @brief   Конвертация беззнакового bignum_t в строку.
 * @version 0.5.0
 * @details
 *   Ревизия 0.5.0: public contract без изменений; ASM получил P1 paths
 *   для Base2 byte LUT, Base8 reservoir и Base16 byte-pair LUT.
 */
/* ------------------------------------------------------------------ */
#pragma once
#ifndef BIGNUM_TO_STRING_H
#define BIGNUM_TO_STRING_H

#include <stddef.h>

#include "bignum.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Результаты выполнения операций bignum_to_string. */
typedef enum {
    BIGNUM_TO_STRING_SUCCESS        = 0,
    BIGNUM_TO_STRING_ERROR_NULL_ARG = -1,
    BIGNUM_TO_STRING_ERROR_BAD_BASE = -2,
    BIGNUM_TO_STRING_ERROR_NO_SPACE = -3,
    BIGNUM_TO_STRING_ERROR_INVALID   = -4
} bignum_to_string_status_t;

/**
 * @brief Вычисляет размер буфера для строкового представления bignum_t.
 * @details
 *   Возвращаемый размер включает завершающий нулевой байт. Поддерживаются
 *   основания 2, 8, 10 и 16. Для нулевого значения требуется два байта.
 *   Функция не изменяет src и не использует глобальное изменяемое состояние.
 *
 * @param[in]  src           Нормализованное беззнаковое число.
 * @param[in]  base          Основание строки: 2, 8, 10 или 16.
 * @param[out] required_size Минимальный размер буфера вместе с NUL.
 * @return Статус операции.
 */
bignum_to_string_status_t bignum_to_string_size(
    const bignum_t *src,
    int base,
    size_t *required_size);

/**
 * @brief Преобразует bignum_t в строку указанного основания.
 * @details
 *   Поддерживаются основания 2, 8, 10 и 16. Результат всегда беззнаковый,
 *   поскольку bignum_t не содержит знакового поля. Для Base16 используются
 *   строчные hexadecimal digits. При недостаточном буфере функция возвращает
 *   BIGNUM_TO_STRING_ERROR_NO_SPACE и не записывает частичный результат.
 *
 * @param[out] dst      Буфер назначения.
 * @param[in]  dst_size Размер dst в байтах.
 * @param[in]  src      Нормализованное беззнаковое число.
 * @param[in]  base     Основание строки: 2, 8, 10 или 16.
 * @return Статус операции.
 */
bignum_to_string_status_t bignum_to_string(
    char *dst,
    size_t dst_size,
    const bignum_t *src,
    int base);

#ifdef __cplusplus
}
#endif

#endif /* BIGNUM_TO_STRING_H */

/* SPDX-License-Identifier: MIT */

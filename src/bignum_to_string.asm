
; -----------------------------------------------------------------------------
; @file    bignum_to_string.asm
; @brief   Оптимизированная x86-64 конвертация bignum_t в Base2/Base8/Base10/Base16.
; @version 0.5.0
; @details
;   Revision 0.5.0: P1 power-of-two writers. Base2 emits complete bytes
;   through binary_byte_lut; Base8 uses a 3-bit reservoir-style extractor with
;   explicit cross-word handling; Base16 emits byte pairs through hex_pair_lut.
;   Base10 uses repeated multiword division by 10^9. No external calls and no
;   mutable global state are used.
; -----------------------------------------------------------------------------
; SPDX-License-Identifier: MIT
; -----------------------------------------------------------------------------

default rel

BIGNUM_CAPACITY                     equ 32
BIGNUM_WORD_SIZE                    equ 8
BIGNUM_OFFSET_LEN                   equ 256
BIGNUM_TO_STRING_SUCCESS            equ 0
BIGNUM_TO_STRING_ERROR_NULL_ARG     equ -1
BIGNUM_TO_STRING_ERROR_BAD_BASE     equ -2
BIGNUM_TO_STRING_ERROR_NO_SPACE     equ -3
BIGNUM_TO_STRING_ERROR_INVALID      equ -4

LOCAL_REQUIRED                      equ -8
LOCAL_DST_SIZE                      equ -16
LOCAL_SRC                            equ -24
LOCAL_BASE                           equ -32
LOCAL_LEN                            equ -40
LOCAL_DST                            equ -48
LOCAL_TEMP                           equ -56
LOCAL_BITPOS                         equ -64
LOCAL_LUT                            equ -72
WORK_BASE                            equ -1000
TEMP_BASE                            equ -3200
TEMP_END                             equ -1152
FRAME_SIZE                           equ 4000

section .rodata
align 16
hex_digits:
    db '0123456789abcdef'
binary_byte_lut:
    db '0', '0', '0', '0', '0', '0', '0', '0'
    db '0', '0', '0', '0', '0', '0', '0', '1'
    db '0', '0', '0', '0', '0', '0', '1', '0'
    db '0', '0', '0', '0', '0', '0', '1', '1'
    db '0', '0', '0', '0', '0', '1', '0', '0'
    db '0', '0', '0', '0', '0', '1', '0', '1'
    db '0', '0', '0', '0', '0', '1', '1', '0'
    db '0', '0', '0', '0', '0', '1', '1', '1'
    db '0', '0', '0', '0', '1', '0', '0', '0'
    db '0', '0', '0', '0', '1', '0', '0', '1'
    db '0', '0', '0', '0', '1', '0', '1', '0'
    db '0', '0', '0', '0', '1', '0', '1', '1'
    db '0', '0', '0', '0', '1', '1', '0', '0'
    db '0', '0', '0', '0', '1', '1', '0', '1'
    db '0', '0', '0', '0', '1', '1', '1', '0'
    db '0', '0', '0', '0', '1', '1', '1', '1'
    db '0', '0', '0', '1', '0', '0', '0', '0'
    db '0', '0', '0', '1', '0', '0', '0', '1'
    db '0', '0', '0', '1', '0', '0', '1', '0'
    db '0', '0', '0', '1', '0', '0', '1', '1'
    db '0', '0', '0', '1', '0', '1', '0', '0'
    db '0', '0', '0', '1', '0', '1', '0', '1'
    db '0', '0', '0', '1', '0', '1', '1', '0'
    db '0', '0', '0', '1', '0', '1', '1', '1'
    db '0', '0', '0', '1', '1', '0', '0', '0'
    db '0', '0', '0', '1', '1', '0', '0', '1'
    db '0', '0', '0', '1', '1', '0', '1', '0'
    db '0', '0', '0', '1', '1', '0', '1', '1'
    db '0', '0', '0', '1', '1', '1', '0', '0'
    db '0', '0', '0', '1', '1', '1', '0', '1'
    db '0', '0', '0', '1', '1', '1', '1', '0'
    db '0', '0', '0', '1', '1', '1', '1', '1'
    db '0', '0', '1', '0', '0', '0', '0', '0'
    db '0', '0', '1', '0', '0', '0', '0', '1'
    db '0', '0', '1', '0', '0', '0', '1', '0'
    db '0', '0', '1', '0', '0', '0', '1', '1'
    db '0', '0', '1', '0', '0', '1', '0', '0'
    db '0', '0', '1', '0', '0', '1', '0', '1'
    db '0', '0', '1', '0', '0', '1', '1', '0'
    db '0', '0', '1', '0', '0', '1', '1', '1'
    db '0', '0', '1', '0', '1', '0', '0', '0'
    db '0', '0', '1', '0', '1', '0', '0', '1'
    db '0', '0', '1', '0', '1', '0', '1', '0'
    db '0', '0', '1', '0', '1', '0', '1', '1'
    db '0', '0', '1', '0', '1', '1', '0', '0'
    db '0', '0', '1', '0', '1', '1', '0', '1'
    db '0', '0', '1', '0', '1', '1', '1', '0'
    db '0', '0', '1', '0', '1', '1', '1', '1'
    db '0', '0', '1', '1', '0', '0', '0', '0'
    db '0', '0', '1', '1', '0', '0', '0', '1'
    db '0', '0', '1', '1', '0', '0', '1', '0'
    db '0', '0', '1', '1', '0', '0', '1', '1'
    db '0', '0', '1', '1', '0', '1', '0', '0'
    db '0', '0', '1', '1', '0', '1', '0', '1'
    db '0', '0', '1', '1', '0', '1', '1', '0'
    db '0', '0', '1', '1', '0', '1', '1', '1'
    db '0', '0', '1', '1', '1', '0', '0', '0'
    db '0', '0', '1', '1', '1', '0', '0', '1'
    db '0', '0', '1', '1', '1', '0', '1', '0'
    db '0', '0', '1', '1', '1', '0', '1', '1'
    db '0', '0', '1', '1', '1', '1', '0', '0'
    db '0', '0', '1', '1', '1', '1', '0', '1'
    db '0', '0', '1', '1', '1', '1', '1', '0'
    db '0', '0', '1', '1', '1', '1', '1', '1'
    db '0', '1', '0', '0', '0', '0', '0', '0'
    db '0', '1', '0', '0', '0', '0', '0', '1'
    db '0', '1', '0', '0', '0', '0', '1', '0'
    db '0', '1', '0', '0', '0', '0', '1', '1'
    db '0', '1', '0', '0', '0', '1', '0', '0'
    db '0', '1', '0', '0', '0', '1', '0', '1'
    db '0', '1', '0', '0', '0', '1', '1', '0'
    db '0', '1', '0', '0', '0', '1', '1', '1'
    db '0', '1', '0', '0', '1', '0', '0', '0'
    db '0', '1', '0', '0', '1', '0', '0', '1'
    db '0', '1', '0', '0', '1', '0', '1', '0'
    db '0', '1', '0', '0', '1', '0', '1', '1'
    db '0', '1', '0', '0', '1', '1', '0', '0'
    db '0', '1', '0', '0', '1', '1', '0', '1'
    db '0', '1', '0', '0', '1', '1', '1', '0'
    db '0', '1', '0', '0', '1', '1', '1', '1'
    db '0', '1', '0', '1', '0', '0', '0', '0'
    db '0', '1', '0', '1', '0', '0', '0', '1'
    db '0', '1', '0', '1', '0', '0', '1', '0'
    db '0', '1', '0', '1', '0', '0', '1', '1'
    db '0', '1', '0', '1', '0', '1', '0', '0'
    db '0', '1', '0', '1', '0', '1', '0', '1'
    db '0', '1', '0', '1', '0', '1', '1', '0'
    db '0', '1', '0', '1', '0', '1', '1', '1'
    db '0', '1', '0', '1', '1', '0', '0', '0'
    db '0', '1', '0', '1', '1', '0', '0', '1'
    db '0', '1', '0', '1', '1', '0', '1', '0'
    db '0', '1', '0', '1', '1', '0', '1', '1'
    db '0', '1', '0', '1', '1', '1', '0', '0'
    db '0', '1', '0', '1', '1', '1', '0', '1'
    db '0', '1', '0', '1', '1', '1', '1', '0'
    db '0', '1', '0', '1', '1', '1', '1', '1'
    db '0', '1', '1', '0', '0', '0', '0', '0'
    db '0', '1', '1', '0', '0', '0', '0', '1'
    db '0', '1', '1', '0', '0', '0', '1', '0'
    db '0', '1', '1', '0', '0', '0', '1', '1'
    db '0', '1', '1', '0', '0', '1', '0', '0'
    db '0', '1', '1', '0', '0', '1', '0', '1'
    db '0', '1', '1', '0', '0', '1', '1', '0'
    db '0', '1', '1', '0', '0', '1', '1', '1'
    db '0', '1', '1', '0', '1', '0', '0', '0'
    db '0', '1', '1', '0', '1', '0', '0', '1'
    db '0', '1', '1', '0', '1', '0', '1', '0'
    db '0', '1', '1', '0', '1', '0', '1', '1'
    db '0', '1', '1', '0', '1', '1', '0', '0'
    db '0', '1', '1', '0', '1', '1', '0', '1'
    db '0', '1', '1', '0', '1', '1', '1', '0'
    db '0', '1', '1', '0', '1', '1', '1', '1'
    db '0', '1', '1', '1', '0', '0', '0', '0'
    db '0', '1', '1', '1', '0', '0', '0', '1'
    db '0', '1', '1', '1', '0', '0', '1', '0'
    db '0', '1', '1', '1', '0', '0', '1', '1'
    db '0', '1', '1', '1', '0', '1', '0', '0'
    db '0', '1', '1', '1', '0', '1', '0', '1'
    db '0', '1', '1', '1', '0', '1', '1', '0'
    db '0', '1', '1', '1', '0', '1', '1', '1'
    db '0', '1', '1', '1', '1', '0', '0', '0'
    db '0', '1', '1', '1', '1', '0', '0', '1'
    db '0', '1', '1', '1', '1', '0', '1', '0'
    db '0', '1', '1', '1', '1', '0', '1', '1'
    db '0', '1', '1', '1', '1', '1', '0', '0'
    db '0', '1', '1', '1', '1', '1', '0', '1'
    db '0', '1', '1', '1', '1', '1', '1', '0'
    db '0', '1', '1', '1', '1', '1', '1', '1'
    db '1', '0', '0', '0', '0', '0', '0', '0'
    db '1', '0', '0', '0', '0', '0', '0', '1'
    db '1', '0', '0', '0', '0', '0', '1', '0'
    db '1', '0', '0', '0', '0', '0', '1', '1'
    db '1', '0', '0', '0', '0', '1', '0', '0'
    db '1', '0', '0', '0', '0', '1', '0', '1'
    db '1', '0', '0', '0', '0', '1', '1', '0'
    db '1', '0', '0', '0', '0', '1', '1', '1'
    db '1', '0', '0', '0', '1', '0', '0', '0'
    db '1', '0', '0', '0', '1', '0', '0', '1'
    db '1', '0', '0', '0', '1', '0', '1', '0'
    db '1', '0', '0', '0', '1', '0', '1', '1'
    db '1', '0', '0', '0', '1', '1', '0', '0'
    db '1', '0', '0', '0', '1', '1', '0', '1'
    db '1', '0', '0', '0', '1', '1', '1', '0'
    db '1', '0', '0', '0', '1', '1', '1', '1'
    db '1', '0', '0', '1', '0', '0', '0', '0'
    db '1', '0', '0', '1', '0', '0', '0', '1'
    db '1', '0', '0', '1', '0', '0', '1', '0'
    db '1', '0', '0', '1', '0', '0', '1', '1'
    db '1', '0', '0', '1', '0', '1', '0', '0'
    db '1', '0', '0', '1', '0', '1', '0', '1'
    db '1', '0', '0', '1', '0', '1', '1', '0'
    db '1', '0', '0', '1', '0', '1', '1', '1'
    db '1', '0', '0', '1', '1', '0', '0', '0'
    db '1', '0', '0', '1', '1', '0', '0', '1'
    db '1', '0', '0', '1', '1', '0', '1', '0'
    db '1', '0', '0', '1', '1', '0', '1', '1'
    db '1', '0', '0', '1', '1', '1', '0', '0'
    db '1', '0', '0', '1', '1', '1', '0', '1'
    db '1', '0', '0', '1', '1', '1', '1', '0'
    db '1', '0', '0', '1', '1', '1', '1', '1'
    db '1', '0', '1', '0', '0', '0', '0', '0'
    db '1', '0', '1', '0', '0', '0', '0', '1'
    db '1', '0', '1', '0', '0', '0', '1', '0'
    db '1', '0', '1', '0', '0', '0', '1', '1'
    db '1', '0', '1', '0', '0', '1', '0', '0'
    db '1', '0', '1', '0', '0', '1', '0', '1'
    db '1', '0', '1', '0', '0', '1', '1', '0'
    db '1', '0', '1', '0', '0', '1', '1', '1'
    db '1', '0', '1', '0', '1', '0', '0', '0'
    db '1', '0', '1', '0', '1', '0', '0', '1'
    db '1', '0', '1', '0', '1', '0', '1', '0'
    db '1', '0', '1', '0', '1', '0', '1', '1'
    db '1', '0', '1', '0', '1', '1', '0', '0'
    db '1', '0', '1', '0', '1', '1', '0', '1'
    db '1', '0', '1', '0', '1', '1', '1', '0'
    db '1', '0', '1', '0', '1', '1', '1', '1'
    db '1', '0', '1', '1', '0', '0', '0', '0'
    db '1', '0', '1', '1', '0', '0', '0', '1'
    db '1', '0', '1', '1', '0', '0', '1', '0'
    db '1', '0', '1', '1', '0', '0', '1', '1'
    db '1', '0', '1', '1', '0', '1', '0', '0'
    db '1', '0', '1', '1', '0', '1', '0', '1'
    db '1', '0', '1', '1', '0', '1', '1', '0'
    db '1', '0', '1', '1', '0', '1', '1', '1'
    db '1', '0', '1', '1', '1', '0', '0', '0'
    db '1', '0', '1', '1', '1', '0', '0', '1'
    db '1', '0', '1', '1', '1', '0', '1', '0'
    db '1', '0', '1', '1', '1', '0', '1', '1'
    db '1', '0', '1', '1', '1', '1', '0', '0'
    db '1', '0', '1', '1', '1', '1', '0', '1'
    db '1', '0', '1', '1', '1', '1', '1', '0'
    db '1', '0', '1', '1', '1', '1', '1', '1'
    db '1', '1', '0', '0', '0', '0', '0', '0'
    db '1', '1', '0', '0', '0', '0', '0', '1'
    db '1', '1', '0', '0', '0', '0', '1', '0'
    db '1', '1', '0', '0', '0', '0', '1', '1'
    db '1', '1', '0', '0', '0', '1', '0', '0'
    db '1', '1', '0', '0', '0', '1', '0', '1'
    db '1', '1', '0', '0', '0', '1', '1', '0'
    db '1', '1', '0', '0', '0', '1', '1', '1'
    db '1', '1', '0', '0', '1', '0', '0', '0'
    db '1', '1', '0', '0', '1', '0', '0', '1'
    db '1', '1', '0', '0', '1', '0', '1', '0'
    db '1', '1', '0', '0', '1', '0', '1', '1'
    db '1', '1', '0', '0', '1', '1', '0', '0'
    db '1', '1', '0', '0', '1', '1', '0', '1'
    db '1', '1', '0', '0', '1', '1', '1', '0'
    db '1', '1', '0', '0', '1', '1', '1', '1'
    db '1', '1', '0', '1', '0', '0', '0', '0'
    db '1', '1', '0', '1', '0', '0', '0', '1'
    db '1', '1', '0', '1', '0', '0', '1', '0'
    db '1', '1', '0', '1', '0', '0', '1', '1'
    db '1', '1', '0', '1', '0', '1', '0', '0'
    db '1', '1', '0', '1', '0', '1', '0', '1'
    db '1', '1', '0', '1', '0', '1', '1', '0'
    db '1', '1', '0', '1', '0', '1', '1', '1'
    db '1', '1', '0', '1', '1', '0', '0', '0'
    db '1', '1', '0', '1', '1', '0', '0', '1'
    db '1', '1', '0', '1', '1', '0', '1', '0'
    db '1', '1', '0', '1', '1', '0', '1', '1'
    db '1', '1', '0', '1', '1', '1', '0', '0'
    db '1', '1', '0', '1', '1', '1', '0', '1'
    db '1', '1', '0', '1', '1', '1', '1', '0'
    db '1', '1', '0', '1', '1', '1', '1', '1'
    db '1', '1', '1', '0', '0', '0', '0', '0'
    db '1', '1', '1', '0', '0', '0', '0', '1'
    db '1', '1', '1', '0', '0', '0', '1', '0'
    db '1', '1', '1', '0', '0', '0', '1', '1'
    db '1', '1', '1', '0', '0', '1', '0', '0'
    db '1', '1', '1', '0', '0', '1', '0', '1'
    db '1', '1', '1', '0', '0', '1', '1', '0'
    db '1', '1', '1', '0', '0', '1', '1', '1'
    db '1', '1', '1', '0', '1', '0', '0', '0'
    db '1', '1', '1', '0', '1', '0', '0', '1'
    db '1', '1', '1', '0', '1', '0', '1', '0'
    db '1', '1', '1', '0', '1', '0', '1', '1'
    db '1', '1', '1', '0', '1', '1', '0', '0'
    db '1', '1', '1', '0', '1', '1', '0', '1'
    db '1', '1', '1', '0', '1', '1', '1', '0'
    db '1', '1', '1', '0', '1', '1', '1', '1'
    db '1', '1', '1', '1', '0', '0', '0', '0'
    db '1', '1', '1', '1', '0', '0', '0', '1'
    db '1', '1', '1', '1', '0', '0', '1', '0'
    db '1', '1', '1', '1', '0', '0', '1', '1'
    db '1', '1', '1', '1', '0', '1', '0', '0'
    db '1', '1', '1', '1', '0', '1', '0', '1'
    db '1', '1', '1', '1', '0', '1', '1', '0'
    db '1', '1', '1', '1', '0', '1', '1', '1'
    db '1', '1', '1', '1', '1', '0', '0', '0'
    db '1', '1', '1', '1', '1', '0', '0', '1'
    db '1', '1', '1', '1', '1', '0', '1', '0'
    db '1', '1', '1', '1', '1', '0', '1', '1'
    db '1', '1', '1', '1', '1', '1', '0', '0'
    db '1', '1', '1', '1', '1', '1', '0', '1'
    db '1', '1', '1', '1', '1', '1', '1', '0'
    db '1', '1', '1', '1', '1', '1', '1', '1'
hex_pair_lut:
    db '0', '0'
    db '0', '1'
    db '0', '2'
    db '0', '3'
    db '0', '4'
    db '0', '5'
    db '0', '6'
    db '0', '7'
    db '0', '8'
    db '0', '9'
    db '0', 'a'
    db '0', 'b'
    db '0', 'c'
    db '0', 'd'
    db '0', 'e'
    db '0', 'f'
    db '1', '0'
    db '1', '1'
    db '1', '2'
    db '1', '3'
    db '1', '4'
    db '1', '5'
    db '1', '6'
    db '1', '7'
    db '1', '8'
    db '1', '9'
    db '1', 'a'
    db '1', 'b'
    db '1', 'c'
    db '1', 'd'
    db '1', 'e'
    db '1', 'f'
    db '2', '0'
    db '2', '1'
    db '2', '2'
    db '2', '3'
    db '2', '4'
    db '2', '5'
    db '2', '6'
    db '2', '7'
    db '2', '8'
    db '2', '9'
    db '2', 'a'
    db '2', 'b'
    db '2', 'c'
    db '2', 'd'
    db '2', 'e'
    db '2', 'f'
    db '3', '0'
    db '3', '1'
    db '3', '2'
    db '3', '3'
    db '3', '4'
    db '3', '5'
    db '3', '6'
    db '3', '7'
    db '3', '8'
    db '3', '9'
    db '3', 'a'
    db '3', 'b'
    db '3', 'c'
    db '3', 'd'
    db '3', 'e'
    db '3', 'f'
    db '4', '0'
    db '4', '1'
    db '4', '2'
    db '4', '3'
    db '4', '4'
    db '4', '5'
    db '4', '6'
    db '4', '7'
    db '4', '8'
    db '4', '9'
    db '4', 'a'
    db '4', 'b'
    db '4', 'c'
    db '4', 'd'
    db '4', 'e'
    db '4', 'f'
    db '5', '0'
    db '5', '1'
    db '5', '2'
    db '5', '3'
    db '5', '4'
    db '5', '5'
    db '5', '6'
    db '5', '7'
    db '5', '8'
    db '5', '9'
    db '5', 'a'
    db '5', 'b'
    db '5', 'c'
    db '5', 'd'
    db '5', 'e'
    db '5', 'f'
    db '6', '0'
    db '6', '1'
    db '6', '2'
    db '6', '3'
    db '6', '4'
    db '6', '5'
    db '6', '6'
    db '6', '7'
    db '6', '8'
    db '6', '9'
    db '6', 'a'
    db '6', 'b'
    db '6', 'c'
    db '6', 'd'
    db '6', 'e'
    db '6', 'f'
    db '7', '0'
    db '7', '1'
    db '7', '2'
    db '7', '3'
    db '7', '4'
    db '7', '5'
    db '7', '6'
    db '7', '7'
    db '7', '8'
    db '7', '9'
    db '7', 'a'
    db '7', 'b'
    db '7', 'c'
    db '7', 'd'
    db '7', 'e'
    db '7', 'f'
    db '8', '0'
    db '8', '1'
    db '8', '2'
    db '8', '3'
    db '8', '4'
    db '8', '5'
    db '8', '6'
    db '8', '7'
    db '8', '8'
    db '8', '9'
    db '8', 'a'
    db '8', 'b'
    db '8', 'c'
    db '8', 'd'
    db '8', 'e'
    db '8', 'f'
    db '9', '0'
    db '9', '1'
    db '9', '2'
    db '9', '3'
    db '9', '4'
    db '9', '5'
    db '9', '6'
    db '9', '7'
    db '9', '8'
    db '9', '9'
    db '9', 'a'
    db '9', 'b'
    db '9', 'c'
    db '9', 'd'
    db '9', 'e'
    db '9', 'f'
    db 'a', '0'
    db 'a', '1'
    db 'a', '2'
    db 'a', '3'
    db 'a', '4'
    db 'a', '5'
    db 'a', '6'
    db 'a', '7'
    db 'a', '8'
    db 'a', '9'
    db 'a', 'a'
    db 'a', 'b'
    db 'a', 'c'
    db 'a', 'd'
    db 'a', 'e'
    db 'a', 'f'
    db 'b', '0'
    db 'b', '1'
    db 'b', '2'
    db 'b', '3'
    db 'b', '4'
    db 'b', '5'
    db 'b', '6'
    db 'b', '7'
    db 'b', '8'
    db 'b', '9'
    db 'b', 'a'
    db 'b', 'b'
    db 'b', 'c'
    db 'b', 'd'
    db 'b', 'e'
    db 'b', 'f'
    db 'c', '0'
    db 'c', '1'
    db 'c', '2'
    db 'c', '3'
    db 'c', '4'
    db 'c', '5'
    db 'c', '6'
    db 'c', '7'
    db 'c', '8'
    db 'c', '9'
    db 'c', 'a'
    db 'c', 'b'
    db 'c', 'c'
    db 'c', 'd'
    db 'c', 'e'
    db 'c', 'f'
    db 'd', '0'
    db 'd', '1'
    db 'd', '2'
    db 'd', '3'
    db 'd', '4'
    db 'd', '5'
    db 'd', '6'
    db 'd', '7'
    db 'd', '8'
    db 'd', '9'
    db 'd', 'a'
    db 'd', 'b'
    db 'd', 'c'
    db 'd', 'd'
    db 'd', 'e'
    db 'd', 'f'
    db 'e', '0'
    db 'e', '1'
    db 'e', '2'
    db 'e', '3'
    db 'e', '4'
    db 'e', '5'
    db 'e', '6'
    db 'e', '7'
    db 'e', '8'
    db 'e', '9'
    db 'e', 'a'
    db 'e', 'b'
    db 'e', 'c'
    db 'e', 'd'
    db 'e', 'e'
    db 'e', 'f'
    db 'f', '0'
    db 'f', '1'
    db 'f', '2'
    db 'f', '3'
    db 'f', '4'
    db 'f', '5'
    db 'f', '6'
    db 'f', '7'
    db 'f', '8'
    db 'f', '9'
    db 'f', 'a'
    db 'f', 'b'
    db 'f', 'c'
    db 'f', 'd'
    db 'f', 'e'
    db 'f', 'f'

section .text
align 16
global bignum_to_string
global bignum_to_string_size

; bignum_to_string_status_t bignum_to_string_size(
;     const bignum_t *src, int base, size_t *required_size)
bignum_to_string_size:
    push rbp
    mov rbp, rsp
    sub rsp, FRAME_SIZE
    mov [LOCAL_REQUIRED + rbp], rdx
    test rdi, rdi
    jz .size_null
    cmp esi, 2
    je .size_base_ok
    cmp esi, 8
    je .size_base_ok
    cmp esi, 10
    je .size_base_ok
    cmp esi, 16
    jne .size_bad_base
.size_base_ok:
    mov rax, [rdi + BIGNUM_OFFSET_LEN]
    cmp rax, BIGNUM_CAPACITY
    ja .size_invalid
    test rax, rax
    jz .size_valid
    cmp qword [rdi + rax * BIGNUM_WORD_SIZE - BIGNUM_WORD_SIZE], 0
    je .size_invalid
.size_valid:
    test rdx, rdx
    jz .size_null
    mov [LOCAL_SRC + rbp], rdi
    mov [LOCAL_BASE + rbp], rsi
    lea rdi, [TEMP_BASE + rbp]
    mov rdx, [LOCAL_SRC + rbp]
    mov ecx, [LOCAL_BASE + rbp]
    call convert_validated
    inc rax
    mov rdx, [LOCAL_REQUIRED + rbp]
    mov [rdx], rax
    xor eax, eax
    leave
    ret
.size_null:
    mov eax, BIGNUM_TO_STRING_ERROR_NULL_ARG
    leave
    ret
.size_bad_base:
    mov eax, BIGNUM_TO_STRING_ERROR_BAD_BASE
    leave
    ret
.size_invalid:
    mov eax, BIGNUM_TO_STRING_ERROR_INVALID
    leave
    ret

; bignum_to_string_status_t bignum_to_string(
;     char *dst, size_t dst_size, const bignum_t *src, int base)
bignum_to_string:
    push rbp
    mov rbp, rsp
    sub rsp, FRAME_SIZE
    mov [LOCAL_DST_SIZE + rbp], rsi
    mov [LOCAL_SRC + rbp], rdx
    mov [LOCAL_BASE + rbp], rcx
    test rdi, rdi
    jz .convert_null
    test rdx, rdx
    jz .convert_null
    cmp ecx, 2
    je .convert_base_ok
    cmp ecx, 8
    je .convert_base_ok
    cmp ecx, 10
    je .convert_base_ok
    cmp ecx, 16
    jne .convert_bad_base
.convert_base_ok:
    mov rax, [rdx + BIGNUM_OFFSET_LEN]
    cmp rax, BIGNUM_CAPACITY
    ja .convert_invalid
    test rax, rax
    jz .convert_valid
    cmp qword [rdx + rax * BIGNUM_WORD_SIZE - BIGNUM_WORD_SIZE], 0
    je .convert_invalid
.convert_valid:
    mov [LOCAL_DST + rbp], rdi
    lea rdi, [TEMP_BASE + rbp]
    mov rdx, [LOCAL_SRC + rbp]
    mov ecx, [LOCAL_BASE + rbp]
    call convert_validated
    mov r8, rax
    inc r8
    cmp [LOCAL_DST_SIZE + rbp], r8
    jb .convert_no_space
    lea rsi, [TEMP_BASE + rbp]
    mov rdi, [LOCAL_DST + rbp]
    mov rcx, r8
    rep movsb
    xor eax, eax
    leave
    ret
.convert_null:
    mov eax, BIGNUM_TO_STRING_ERROR_NULL_ARG
    leave
    ret
.convert_bad_base:
    mov eax, BIGNUM_TO_STRING_ERROR_BAD_BASE
    leave
    ret
.convert_invalid:
    mov eax, BIGNUM_TO_STRING_ERROR_INVALID
    leave
    ret
.convert_no_space:
    mov eax, BIGNUM_TO_STRING_ERROR_NO_SPACE
    leave
    ret

; Internal conversion: rdi = temporary output, rdx = validated source,
; ecx = base. Returns rax = character count and writes trailing NUL.
convert_validated:
    cmp ecx, 2
    je .convert_power2
    cmp ecx, 8
    je .convert_power2
    cmp ecx, 16
    je .convert_hex

.convert_decimal:
    mov r8, [rdx + BIGNUM_OFFSET_LEN]
    test r8, r8
    jnz .decimal_copy
    mov byte [rdi], '0'
    mov byte [rdi + 1], 0
    mov eax, 1
    ret
.decimal_copy:
    mov [LOCAL_LEN + rbp], r8
    lea rsi, [rdx]
    lea rdi, [WORK_BASE + rbp]
    mov rcx, r8
    rep movsq
    lea rdi, [TEMP_BASE + rbp]
    mov byte [TEMP_END + rbp], 0
    lea r8, [TEMP_END + rbp]
    mov r11, 1000000000

.decimal_division:
    xor r9d, r9d
    mov r10, [LOCAL_LEN + rbp]
    lea rsi, [WORK_BASE + rbp]
.decimal_div_loop:
    mov rax, [rsi + r10 * BIGNUM_WORD_SIZE - BIGNUM_WORD_SIZE]
    mov rdx, r9
    div r11
    mov [rsi + r10 * BIGNUM_WORD_SIZE - BIGNUM_WORD_SIZE], rax
    mov r9, rdx
    dec r10
    jnz .decimal_div_loop

    mov r10, [LOCAL_LEN + rbp]
    lea rsi, [WORK_BASE + rbp]
.trim_work:
    test r10, r10
    jz .decimal_chunk_ready
    cmp qword [rsi + r10 * BIGNUM_WORD_SIZE - BIGNUM_WORD_SIZE], 0
    jne .decimal_chunk_ready
    dec r10
    jmp .trim_work
.decimal_chunk_ready:
    mov eax, r9d
    mov r9d, 10
    mov ecx, 9
.decimal_write_chunk:
    xor edx, edx
    div r9d
    dec r8
    add dl, '0'
    mov [r8], dl
    dec ecx
    jnz .decimal_write_chunk
    mov [LOCAL_LEN + rbp], r10
    cmp r10, 0
    jne .decimal_division

    mov rsi, r8
.decimal_skip_leading_zero:
    cmp byte [rsi], '0'
    jne .decimal_output_ready
    inc rsi
    jmp .decimal_skip_leading_zero
.decimal_output_ready:
    lea rax, [TEMP_END + 1 + rbp]
    sub rax, rsi
    mov rcx, rax
    lea rdi, [TEMP_BASE + rbp]
    rep movsb
    dec rax
    ret

.convert_power2:
    mov [LOCAL_SRC + rbp], rdx
    mov [LOCAL_TEMP + rbp], rdi
    mov r8, [rdx + BIGNUM_OFFSET_LEN]
    test r8, r8
    jnz .power2_nonzero
    mov byte [rdi], '0'
    mov byte [rdi + 1], 0
    mov eax, 1
    ret
.power2_nonzero:
    mov r11, r8
    dec r11
    mov rax, [rdx + r11 * BIGNUM_WORD_SIZE]
    bsr rax, rax
    inc eax
    mov r10d, eax
    shl r11, 6
    add r10, r11
    cmp ecx, 2
    je .power2_base2
    mov r9d, 3
    mov eax, r10d
    add eax, 2
    xor edx, edx
    mov ecx, 3
    div ecx
    mov r8, rax
    jmp .power2_output
; Base8 P1 reservoir writer. A digit normally comes from one word; only
; offsets 0 and 1 cross into the next lower word and are handled explicitly.
.power2_base8_reservoir:
    mov rdx, [LOCAL_SRC + rbp]
    mov r11, r10
    dec r11
    mov eax, r10d
    xor edx, edx
    mov ecx, 3
    div ecx
    mov r8, rax
    mov r9d, edx
    mov rdx, [LOCAL_SRC + rbp]
    test r9d, r9d
    jz .base8_count_ready
    inc r8
.base8_count_ready:
    jnz .base8_width_ready
    mov r9d, 3
.base8_width_ready:
    cmp r9d, 3
    je .base8_emit_digit
    xor r10d, r10d
    mov [LOCAL_LEN + rbp], r9d
.base8_partial_bit:
    mov rcx, r11
    shr rcx, 6
    mov rax, [rdx + rcx * BIGNUM_WORD_SIZE]
    mov ecx, r11d
    and ecx, 63
    shr rax, cl
    and eax, 1
    shl r10d, 1
    or r10d, eax
    dec r11
    dec dword [LOCAL_LEN + rbp]
    jnz .base8_partial_bit
    add r10b, '0'
    mov [rdi], r10b
    inc rdi
    dec r8
    jz .base8_finish
    mov r9d, 3
.base8_emit_digit:
    mov rax, r11
    mov rcx, rax
    shr rcx, 6
    mov rsi, [rdx + rcx * BIGNUM_WORD_SIZE]
    and eax, 63
    cmp eax, 2
    jae .base8_same_word
    test eax, eax
    jz .base8_cross_zero
    ; offset 1: two low bits of this word and one high bit of the next.
    and esi, 3
    shl esi, 1
    mov rax, [rdx + rcx * BIGNUM_WORD_SIZE - BIGNUM_WORD_SIZE]
    shr rax, 63
    and eax, 1
    or esi, eax
    mov r10d, esi
    jmp .base8_digit_ready
.base8_cross_zero:
    and esi, 1
    shl esi, 2
    mov rax, [rdx + rcx * BIGNUM_WORD_SIZE - BIGNUM_WORD_SIZE]
    shr rax, 62
    and eax, 3
    or esi, eax
    mov r10d, esi
    jmp .base8_digit_ready
.base8_same_word:
    sub eax, 2
    mov ecx, eax
    shr rsi, cl
    and esi, 7
    mov r10d, esi
.base8_digit_ready:
    add r10b, '0'
    mov [rdi], r10b
    inc rdi
    sub r11, r9
    dec r8
    jz .base8_finish
    mov r9d, 3
    jmp .base8_emit_digit
.base8_finish:
    mov byte [rdi], 0
    mov rax, rdi
    sub rax, [LOCAL_TEMP + rbp]
    ret

.power2_base2:
    ; Base2 P1: emit the partial leading group, then use one byte LUT per 8 bits.
    mov rdx, [LOCAL_SRC + rbp]
    mov r11, r10
    dec r11
    mov [LOCAL_BITPOS + rbp], r11
    mov eax, r10d
    add eax, 7
    shr eax, 3
    mov r8, rax
    mov eax, r10d
    and eax, 7
    jnz .binary_first_width_ready
    mov eax, 8
.binary_first_width_ready:
    mov r9d, eax
    mov [LOCAL_LEN + rbp], eax
.binary_first_bit:
    mov r11, [LOCAL_BITPOS + rbp]
    mov rcx, r11
    shr rcx, 6
    mov rax, [rdx + rcx * BIGNUM_WORD_SIZE]
    mov esi, r11d
    and esi, 63
    mov ecx, esi
    shr rax, cl
    and eax, 1
    add al, '0'
    mov [rdi], al
    inc rdi
    dec r11
    mov [LOCAL_BITPOS + rbp], r11
    dec dword [LOCAL_LEN + rbp]
    jnz .binary_first_bit
    dec r8
    jz .binary_finish
    lea r11, [rel binary_byte_lut]
    mov [LOCAL_LUT + rbp], r11
.binary_byte_group:
    xor r10d, r10d
    mov dword [LOCAL_LEN + rbp], 8
.binary_byte_bit:
    mov rax, [LOCAL_BITPOS + rbp]
    mov r11, rax
    mov rcx, rax
    shr rcx, 6
    mov rsi, [rdx + rcx * BIGNUM_WORD_SIZE]
    and eax, 63
    mov ecx, eax
    shr rsi, cl
    and esi, 1
    shl r10d, 1
    or r10d, esi
    dec r11
    mov [LOCAL_BITPOS + rbp], r11
    dec dword [LOCAL_LEN + rbp]
    jnz .binary_byte_bit
    mov eax, r10d
    shl eax, 3
    mov r11, [LOCAL_LUT + rbp]
    lea rsi, [r11 + rax]
    mov rax, [rsi]
    mov [rdi], rax
    add rdi, 8
    dec r8
    jnz .binary_byte_group
.binary_finish:
    mov byte [rdi], 0
    mov rax, rdi
    sub rax, [LOCAL_TEMP + rbp]
    ret
.power2_output:
    cmp r9d, 3
    je .power2_base8_reservoir
    mov r11, r10
    dec r11
    mov [LOCAL_BITPOS + rbp], r11
    mov rdx, [LOCAL_SRC + rbp]
    mov eax, r10d
    xor edx, edx
    div r9d
    test edx, edx
    jnz .power2_first_width
    mov edx, r9d
.power2_first_width:
    mov [LOCAL_LEN + rbp], edx
    mov rdx, [LOCAL_SRC + rbp]
.power2_digit:
    xor r10d, r10d
.power2_bit:
    mov r11, [LOCAL_BITPOS + rbp]
    mov rcx, r11
    shr rcx, 6
    mov rax, [rdx + rcx * BIGNUM_WORD_SIZE]
    mov esi, r11d
    and esi, 63
    mov ecx, esi
    shr rax, cl
    and eax, 1
    shl r10d, 1
    or r10d, eax
    dec r11
    mov [LOCAL_BITPOS + rbp], r11
    dec dword [LOCAL_LEN + rbp]
    jnz .power2_bit
    mov eax, r10d
    add al, '0'
    mov [rdi], al
    inc rdi
    mov [LOCAL_LEN + rbp], r9d
    dec r8
    jnz .power2_digit
    mov byte [rdi], 0
    mov rax, rdi
    sub rax, [LOCAL_TEMP + rbp]
    ret

.convert_hex:
    lea r8, [rel hex_digits]
    lea r11, [rel hex_pair_lut]
    mov r9, [rdx + BIGNUM_OFFSET_LEN]
    test r9, r9
    jnz .hex_nonzero
    mov byte [rdi], '0'
    mov byte [rdi + 1], 0
    mov eax, 1
    ret
.hex_nonzero:
    dec r9
    mov r10, [rdx + r9 * BIGNUM_WORD_SIZE]
    bsr rcx, r10
    shr ecx, 2
    test ecx, 1
    jnz .hex_even_nibbles
    ; Odd nibble count: emit the leading nibble, then continue by bytes.
    shl ecx, 2
    mov rax, r10
    shr rax, cl
    and eax, 15
    movzx eax, byte [r8 + rax]
    mov [rdi], al
    inc rdi
    sub ecx, 8
    js .hex_lower_words
    jmp .hex_pair_loop
.hex_even_nibbles:
    shl ecx, 2
    sub ecx, 4
.hex_pair_loop:
    mov rax, r10
    shr rax, cl
    and eax, 255
    movzx eax, word [r11 + rax * 2]
    mov [rdi], ax
    add rdi, 2
    sub ecx, 8
    jns .hex_pair_loop
.hex_lower_words:
    test r9, r9
    jz .hex_finish
.hex_word_loop:
    dec r9
    mov r10, [rdx + r9 * BIGNUM_WORD_SIZE]
    mov ecx, 56
.hex_word_pair:
    mov rax, r10
    shr rax, cl
    and eax, 255
    movzx eax, word [r11 + rax * 2]
    mov [rdi], ax
    add rdi, 2
    sub ecx, 8
    jns .hex_word_pair
    test r9, r9
    jnz .hex_word_loop
.hex_finish:
    mov byte [rdi], 0
    lea rax, [rdi]
    lea rcx, [TEMP_BASE + rbp]
    sub rax, rcx
    ret

section .note.GNU-stack noalloc noexec nowrite progbits

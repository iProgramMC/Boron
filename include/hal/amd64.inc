; Boron64 - HAL include file for assembly functions
%ifndef NS64_AMD64_INC
%define NS64_AMD64_INC
bits 64

; Match the enum in hal.h.
INT_UNKNOWN      equ 0
INT_DOUBLE_FAULT equ 1
INT_PAGE_FAULT   equ 2
INT_IPI          equ 3
INT_COUNT        equ 4



%endif ;NS64_AMD64_INC

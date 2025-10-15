;
;    The Boron Operating System
;    Copyright (C) 2025 iProgramInCpp
; 
; Module name:
;    ke/i386/trap.asm
; 	
; Abstract:
;    This module contains the implementation for each of the
;    individual trap handlers, which call into the common
;    trap handler.
; 	
; Author:
;    iProgramInCpp - 14 October 2025
;
bits 32
section .text

; NOTE: This uses 5120 bytes of data. Is that bad? I don't know.

; Arguments:
; 0 - Interrupt number in hexadecimal
; 1 - If the interrupt has an error code, Y, otherwise, N
; %macro INT 2

extern KiTrapCommon

%macro INT 2
global KiTestTrap%1
KiTestTrap%1:
	%ifidn %2, N
	push dword 0          ; Push a fake error code
	%endif
	push dword 0x%1       ; Push the interrupt number
	jmp  KiTrapCommon      ; Jump to the common trap handler
%endmacro

%include "ke/i386/intlist.inc"

%unmacro INT 2

%macro INT 2
extern KiTestTrap%1
%endmacro
%include "ke/i386/intlist.inc"
%unmacro INT 2


section .rodata

global KiTrapList
KiTrapList:
%macro INT 2
	dd KiTestTrap%1
%endmacro
%include "ke/i386/intlist.inc"
%unmacro INT 2

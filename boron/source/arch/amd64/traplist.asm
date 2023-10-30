;
;    The Boron Operating System
;    Copyright (C) 2023 iProgramInCpp
; 
; Module name:
;    arch/amd64/trap.asm
; 	
; Abstract:
;    This module contains the implementation for each of the
;    individual trap handlers, which call into the common
;    trap handler.
; 	
; Author:
;    iProgramInCpp - 27 October 2023
;
bits 64
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
	push qword 0          ; Push a fake error code
	%endif
	push qword 0x%1       ; Push the interrupt number
	jmp KiTrapCommon      ; Jump to the common trap handler
%endmacro

%include "arch/amd64/intlist.inc"

%unmacro INT 2

%macro INT 2
extern KiTestTrap%1
%endmacro
%include "arch/amd64/intlist.inc"
%unmacro INT 2


section .rodata

global KiTrapList
KiTrapList:
%macro INT 2
	dq KiTestTrap%1
%endmacro
%include "arch/amd64/intlist.inc"
%unmacro INT 2

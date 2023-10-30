;
;    The Boron Operating System
;    Copyright (C) 2023 iProgramInCpp
; 
; Module name:
;    arch/amd64/trap.asm
; 	
; Abstract:
;    This module implements certain utility functions for
;    the AMD64 platform.
; 	
; Author:
;    iProgramInCpp - 28 August 2023
;

bits 64

%include "arch/amd64.inc"

; these functions get the RIP and RBP respectively
global KiGetRIP
global KiGetRBP

KiGetRIP:
	mov rax, [rsp]
	ret

KiGetRBP:
	mov rax, rbp
	ret

; these functions set the CR3
global KeSetCurrentPageTable
global KeGetCurrentPageTable

; void* KeGetCurrentPageTable()
KeGetCurrentPageTable:
	mov rax, cr3
	ret

; void KeSetCurrentPageTable(void* pt)
KeSetCurrentPageTable:
	mov cr3, rdi
	ret

global KeOnUpdateIPL

; void KeOnUpdateIPL(eIPL newIPL, eIPL oldIPL)
KeOnUpdateIPL:
	mov cr8, rdi
	ret

; void KepLoadGdt(GdtDescriptor* desc);
global KepLoadGdt
KepLoadGdt:
	mov rax, rdi
	lgdt [rax]
	
	; update the code segment
	push 0x08           ; code segment
	lea rax, [rel .a]   ; jump address
	push rax
	retfq               ; return far - will go to .a now
.a:
	; update the segments
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	ret

; void KepLoadTss(int descriptor)
global KepLoadTss
KepLoadTss:
	mov ax, di
	ltr ax
	ret

section .bss

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

; int MmProbeAddressSub(void* Address, size_t Length, bool ProbeWrite);
global MmProbeAddressSub
MmProbeAddressSub:
; This function performs the actual work of probing the addresses. It is
; written in assembly to allow for predictability of the stack layout. This
; makes using the alternative exit, returning the STATUS_FAULT code, after
; an invalid page fault, much easier, since we can know the stack layout the
; entire time we're probing.
	
	; Note: We don't need to preserve the System V ABI callee saved registers
	; because we aren't using them!
	
	; rdi - Address
	; rsi - Length
	; rcx - ProbeWrite
	and  rsi, ~0x7        ; Forcefully align the length to 8 bytes.
.Continue:
	mov  rax, [rdi]       ; Attempt to read a single qword from RDI.
	test rcx, rcx         ; Check if we need to probe for writes.
	jne  .SkipProbeWrite
	mov  [rdi], rax       ; Write the qword back.
.SkipProbeWrite:
	add  rdi, 8           ; Increment the pointer by 8.
	sub  rsi, 8           ; Subtract 8 from the length.
	jnz  .Continue        ; Continue if it didn't hit zero.
	xor  rax, rax         ; Return STATUS_SUCCESS.
	; The "return" is just below...
	
; void MmProbeAddressSubEarlyReturn()
; Returns early from MmProbeAddressSub. Called by the invalid page fault handler.
global MmProbeAddressSubEarlyReturn
MmProbeAddressSubEarlyReturn:
	ret

section .bss

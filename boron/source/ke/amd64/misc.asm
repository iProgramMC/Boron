;
;    The Boron Operating System
;    Copyright (C) 2023 iProgramInCpp
; 
; Module name:
;    ke/amd64/trap.asm
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
	;  dl - ProbeWrite
	mov  r8, rdi          ; Load the address into r8, a scratch register.
	add  r8, rsi          ; Add the length to r8.
	dec  r8               ; Decrement r8.  This will be the final byte of the range we're probing.
	mov  al, [r8]         ; Load a byte from that address.
	test dl, dl           ; Check if ProbeWrite is zero.
	jz   .StartLoop
	mov  [r8], al         ; If not, then store that byte to that address.
.StartLoop:
	mov  al, [rdi]        ; Load a byte from the start address.
	test dl, dl           ; Check if ProbeWrite is zero
	jz   .DontWrite
	mov  [rdi], al        ; If not, then store that byte to that address.
.DontWrite:
	add  rdi, 4096        ; Add a page size to rdi.
	cmp  rdi, r8          ; Check if the end address has been reached.
	jb   .StartLoop       ; If not, then probe another page.
	xor  rax, rax         ; Return with an exit code of zero (STATUS_SUCCESS)
	ret
	
; void MmProbeAddressSubEarlyReturn()
; Returns early from MmProbeAddressSub and MmSafeCopySub. Called by the invalid page fault handler.
global MmProbeAddressSubEarlyReturn
MmProbeAddressSubEarlyReturn:
	ret

; int MmSafeCopySub(void* Address, void* Source, size_t Length);
global MmSafeCopySub
MmSafeCopySub:
	cld               ; we don't want the copy the other way
	mov rcx, rdx      ; set the count register
	rep movsb         ; repeats "move single byte" RCX times
	xor rax, rax      ; return zero for no error
	ret

; void KiSwitchThreadStack(void** OldStack, void** NewStack)
; NOTE: Function is run at IPL_DPC with the dispatcher lock held.
global KiSwitchThreadStack
KiSwitchThreadStack:
	; NOTE: There's no need to create a fake iretq frame to use iretq
	; to return to.  This function can only be run from kernel mode
	; where ss == ds == SEG_RING_0_DATA and cs == SEG_RING_0_CODE.
	
	; Push System V ABI callee saved registers.
	; TODO: Do we really need to push and pop rflags?
	pushfq
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	
	; Store the current rsp into *OldStack.
	mov  qword [rdi], rsp
	
	; Load the current rsp from NewStack.
	mov  rsp, qword [rsi]
	
	; Pop everything and return as usual.
KiPopEverythingAndReturn:
	pop  r15
	pop  r14
	pop  r13
	pop  r12
	pop  rbx
	pop  rbp
	xor  rax, rax
	
	; Pop RFLAGS and return
	popfq
	ret

extern KiUnlockDispatcher

; Arguments:
; rbx - Thread entry point.
; r12 - Thread context.
global KiThreadEntryPoint
KiThreadEntryPoint:
	xor  rdi, rdi
	call KiUnlockDispatcher
	mov  rdi, r12
	jmp  rbx

section .text.init

; Used for the init phase of the scheduler, before a thread
; is scheduled in.
global KiSwitchThreadStackForever
KiSwitchThreadStackForever:
	mov rsp, rdi
	jmp KiPopEverythingAndReturn


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

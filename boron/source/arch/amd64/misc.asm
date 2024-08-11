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

; void KiSwitchThreadStack(void** OldStack, void* NewStack)
; NOTE: Function is run at IPL_DPC with the dispatcher lock held.
global KiSwitchThreadStack
KiSwitchThreadStack:
	; NOTE: There's no need to create a fake iretq frame to use iretq
	; to return to.  This function can only be run from kernel mode
	; where ss == ds == SEG_RING_0_DATA and cs == SEG_RING_0_CODE.
	
	; Push System V ABI callee saved registers.
	; TODO: Do we really need to push and pop rflags?
	; TODO: Do we really need to push the segment registers?
	pushfq
	mov  rax, ds
	push rax
	mov  rax, es
	push rax
	mov  rax, fs
	push rax
	mov  rax, gs
	push rax
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	
	; Store the current rsp into *OldStack.
	mov  qword [rdi], rsp
	
	; Load the current rsp from NewStack.
	mov  rsp, rsi
	
	; Pop everything and return as usual.
KiPopEverythingAndReturn:
	pop  r15
	pop  r14
	pop  r13
	pop  r12
	pop  rbx
	pop  rbp
	pop  rax
	mov  gs, rax
	pop  rax
	mov  fs, rax
	pop  rax
	mov  es, rax
	pop  rax
	mov  ds, rax
	xor  rax, rax
	
	; Pop RFLAGS and return
	popfq
	ret

; Used for the init phase of the scheduler, when no thread
; was selected.
global KiSwitchThreadStackForever
KiSwitchThreadStackForever:
	mov rsp, rdi
	jmp KiPopEverythingAndReturn

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

section .bss

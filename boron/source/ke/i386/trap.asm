;
;    The Boron Operating System
;    Copyright (C) 2025 iProgramInCpp
; 
; Module name:
;    ke/i386/trap.asm
; 	
; Abstract:
;    This module contains the implementation for the common
;    trap handler and the list of functions to enter when
;    a certain interrupt vector is triggered.
; 	
; Author:
;    iProgramInCpp - 14 October 2025
;

bits 32

%include "arch/i386.inc"

extern KiTrapList
global KiTrapCallList

section .text

; int KiEnterHardwareInterrupt(int IntNo);
extern KiEnterHardwareInterrupt
; void KiExitHardwareInterrupt(int OldIpl);
extern KiExitHardwareInterrupt

; Return Value:
; rax - Old interrupt state
global KeDisableInterrupts
KeDisableInterrupts:
	pushfd               ; push RFLAGS register
	pop  eax             ; pop it into RAX
	and  eax, 0x200      ; check the previous interrupt flag (1 << 9)
	shr  eax, 9          ; shift right by 9
	cli                  ; disable interrupts
	ret                  ; return rax

; Arguments:
; 1 - Old interrupt state
global KeRestoreInterrupts
KeRestoreInterrupts:
	mov  eax, [esp + 4]
	test eax, eax        ; check if old state is zero
	jz   .ret            ; if it is, return immediately
	sti                  ; restore interrupts
.ret:
	ret                  ; done

; Push the entire state except EAX, EBX, ECX ,and EDX
%macro PUSH_STATE 0
	push esi
	push edi
	mov  eax, cr2
	push eax
%endmacro

; Pop the entire state except EAX, EBX, ECX, and EDX
%macro POP_STATE 0
	add  esp, 4    ; the space occupied by the cr2 register
	pop  edi
	pop  esi
%endmacro

global KiTrapCommon
KiTrapCommon:
	push  eax
	push  ebx
	push  ecx
	push  edx
	lea   ebx, [esp + 16]                  ; Get the pointer to the value after rbx and rax on the stack
	lea   ecx, [esp + 24]                  ; Get the pointer to the EIP from the interrupt frame.
	lea   edx, [esp + 28]                  ; Get the pointer to the CS from the interrupt frame.
	mov   eax, [edx]                       ; Load the CS.
	push  eax
	; Note that LEA doesn't actually perform any memory accesses, all it
	; does it load the address of certain things into a register. We then
	; defer actually loading those until after DS was changed.
	PUSH_STATE                             ; Push the state, except for the old ipl
	mov   ebx, [ebx]                       ; Retrieve the interrupt number and RIP from interrupt frame. These were deferred
	mov   ecx, [ecx]                       ; so that we wouldn't attempt to access the kernel stack using the user's data segment.
	mov   edx, [edx]                       ; Load CS, to determine the previous mode when entering a hardware interrupt
	push  ecx                              ; Enter a stack frame so that stack printing doesn't skip over anything
	push  ebp
	mov   ebp, esp
	cld                                    ; Clear direction flag, will be restored by iretq
	movsx edi, byte [KiTrapIplList + ebx]  ; Get the IPL for the respective interrupt vector
	push  edi
	call  KiEnterHardwareInterrupt         ; Tell the kernel we entered a hardware interrupt
	add   esp, 4
	push  eax                              ; Push the old IPL that we obtained from the function
	mov   edi, esp                         ; Retrieve the PKREGISTERS to call the trap handler
	push  edi
	call  [KiTrapCallList + 8 * ebx]       ; Call the trap handler. It returns the new RSP.
	; add esp, 4 skipped because redundant
	mov   esp, eax                         ; Use the new PKREGISTERS instance as what to pull
	mov   edi, eax                         ; Get the pointer to the register context
	push  edi
	call  KiExitHardwareInterrupt          ; Tell the kernel we're exiting the hardware interrupt
	add   esp, 4
	pop   edi                              ; Pop the old IPL because we don't need it any more
	pop   ebp                              ; Leave the stack frame
	pop   ecx                              ; Skip over the RIP duplicate that we pushed
	POP_STATE                              ; Pop the state
	pop   eax                              ; Pop the RAX register - it has the old value of CS which we can check
	pop   edx                              ; Pop the EDX register
	pop   ecx                              ; Pop the ECX register
	pop   ebx                              ; Pop the EBX register
	pop   eax                              ; Pop the EAX register
	add   esp, 8                           ; Pop the interrupt number and the error code
	iretd

%macro CLEAR_REGS 0
	xor ebx, ebx
	xor ecx, ecx
	xor edx, edx
	xor esi, esi
%endmacro

global KeDescendIntoUserMode
KeDescendIntoUserMode:
	mov edi, [esp + 4]
	mov esi, [esp + 8]
	mov edx, [esp + 12]
	
	; EDI - Initial program counter
	; ESI - Initial stack pointer
	; EDX - User context
	push dword SEG_RING_3_DATA | 3 ; push SS
	push esi                       ; push RSP
	push dword 0x202               ; push RFLAGS
	push dword SEG_RING_3_CODE | 3 ; push CS
	push edi                       ; push RIP
	
	; clear all the registers
	xor eax, eax
	xor ebp, ebp
	mov edi, edx
	CLEAR_REGS
	
	; finally, swap gs and return to user mode.
	cli
	iretd

section .bss
global KiTrapIplList
KiTrapIplList:
	resb 256              ; Reserve a byte for each vector

global KiTrapCallList
KiTrapCallList:
	resq 256              ; Reserve a pointer for each vector

;
;    The Boron Operating System
;    Copyright (C) 2023 iProgramInCpp
; 
; Module name:
;    arch/amd64/trap.asm
; 	
; Abstract:
;    This module contains the implementation for the common
;    trap handler and the list of functions to enter when
;    a certain interrupt vector is triggered.
; 	
; Author:
;    iProgramInCpp - 27 October 2023
;

bits 64

%include "arch/amd64.inc"

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
	pushfq               ; push RFLAGS register
	pop  rax             ; pop it into RAX
	and  rax, 0x200      ; check the previous interrupt flag (1 << 9)
	shr  rax, 9          ; shift right by 9
	cli                  ; disable interrupts
	ret                  ; return rax

; Arguments:
; rdi - Old interrupt state
global KeRestoreInterrupts
KeRestoreInterrupts:
	test rdi, rdi        ; check if old state is zero
	jz   .ret            ; if it is, return immediately
	sti                  ; restore interrupts
.ret:
	ret                  ; done

; Push the entire state except RAX and RBX
%macro PUSH_STATE 0
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	push rsi
	push rdi
	mov  rax, cr2
	push rax
%endmacro

; Pop the entire state except RAX and RBX
%macro POP_STATE 0
	add  rsp, 8    ; the space occupied by the cr2 register
	pop  rdi
	pop  rsi
	pop  r15
	pop  r14
	pop  r13
	pop  r12
	pop  r11
	pop  r10
	pop  r9
	pop  r8
%endmacro

global KiTrapCommon
KiTrapCommon:
	push  rax
	push  rbx
	push  rcx
	push  rdx
	lea   rbx, [rsp + 32]                  ; Get the pointer to the value after rbx and rax on the stack
	lea   rcx, [rsp + 48]                  ; Get the pointer to the RIP from the interrupt frame.
	lea   rdx, [rsp + 56]                  ; Get the pointer to the CS from the interrupt frame.
	; Note that LEA doesn't actually perform any memory accesses, all it
	; does it load the address of certain things into a register. We then
	; defer actually loading those until after DS was changed.
	PUSH_STATE                             ; Push the state, except for the old ipl
	mov   rbx, [rbx]                       ; Retrieve the interrupt number and RIP from interrupt frame. These were deferred
	mov   rcx, [rcx]                       ; so that we wouldn't attempt to access the kernel stack using the user's data segment.
	mov   rdx, [rdx]                       ; Load CS, to determine the previous mode when entering a hardware interrupt
	push  rcx                              ; Enter a stack frame so that stack printing doesn't skip over anything
	push  rbp
	mov   rbp, rsp
	cld                                    ; Clear direction flag, will be restored by iretq
	lea   rax, [KiTrapIplList + rbx]       ; Retrieve the IPL for the respective interrupt vector
	movsx rdi, byte [rax]                  ; Get the IPL itself
	mov   rsi, rdx                         ; Pass in as second parameter the previous CS. This will determine the PreviousMode.
	call  KiEnterHardwareInterrupt         ; Tell the kernel we entered a hardware interrupt
	push  rax                              ; Push the old IPL that we obtained from the function
	mov   rdi, rsp                         ; Retrieve the PKREGISTERS to call the trap handler
	lea   rax, [KiTrapCallList + 8 * rbx]  ; Retrieve the address to the pointer to the function to call
	mov   rax, [rax]                       ; Retrieve the pointer to the function to call...
	call  rax                              ; And call the function!!
	mov   rsp, rax                         ; Use the new PKREGISTERS instance as what to pull
	pop   rdi                              ; Pop the old IPL that was pushed before
	call  KiExitHardwareInterrupt          ; Tell the kernel we're exiting the hardware interrupt
	pop   rbp                              ; Leave the stack frame
	pop   rcx                              ; Skip over the RIP duplicate that we pushed
	POP_STATE                              ; Pop the state
	pop   rdx                              ; Pop the RDX register
	pop   rcx                              ; Pop the RCX register
	pop   rbx                              ; Pop the RBX register
	pop   rax                              ; Pop the RAX register
	add   rsp, 16                          ; Pop the interrupt number and the error code
	iretq

section .bss
global KiTrapIplList
KiTrapIplList:
	resb 256              ; Reserve a byte for each vector

global KiTrapCallList
KiTrapCallList:
	resq 256              ; Reserve a pointer for each vector

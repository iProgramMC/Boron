;
;    The Boron Operating System
;    Copyright (C) 2023 iProgramInCpp
; 
; Module name:
;    ke/amd64/trap.asm
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
	mov   rax, [rdx]                       ; Load the CS.
	push  rax
	cmp   rax, SEG_RING_0_CODE             ; Check if it is kernel mode.
	je    .a                               ; If not, then run swapgs.
	swapgs
.a: ; Note that LEA doesn't actually perform any memory accesses, all it
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
	movsx rdi, byte [KiTrapIplList + rbx]  ; Get the IPL for the respective interrupt vector
	call  KiEnterHardwareInterrupt         ; Tell the kernel we entered a hardware interrupt
	push  rax                              ; Push the old IPL that we obtained from the function
	mov   rdi, rsp                         ; Retrieve the PKREGISTERS to call the trap handler
	call  [KiTrapCallList + 8 * rbx]       ; Call the trap handler. It returns the new RSP.
	mov   rsp, rax                         ; Use the new PKREGISTERS instance as what to pull
	pop   rdi                              ; Pop the old IPL that was pushed before
	call  KiExitHardwareInterrupt          ; Tell the kernel we're exiting the hardware interrupt
	pop   rbp                              ; Leave the stack frame
	pop   rcx                              ; Skip over the RIP duplicate that we pushed
	POP_STATE                              ; Pop the state
	pop   rax                              ; Pop the RAX register - it has the old value of CS which we can check
	cmp   rax, SEG_RING_0_CODE             ; Check if this is kernel mode.
	je    .b                               ; If not, then run swapgs.
	swapgs
.b:	pop   rdx                              ; Pop the RDX register
	pop   rcx                              ; Pop the RCX register
	pop   rbx                              ; Pop the RBX register
	pop   rax                              ; Pop the RAX register
	add   rsp, 16                          ; Pop the interrupt number and the error code
	iretq

extern MmHHDMBase
global KiHandleTlbShootdownIpiA
KiHandleTlbShootdownIpiA:
	; rdi - registers
	; gs:0x00 - TLB shootdown start address
	; gs:0x08 - Page count
	; gs:0x10 - Spinlock (char)
	; Return the registers in rax.
	mov  r15, rdi
	mov  rax, [gs:0x00]
	
	; note: count may never be zero - ensured by KeIssueTLBShootDown
	mov  rcx, [gs:0x08]
.loop:
	invlpg [rax]
	add  rax, 4096
	dec  rcx
	jnz  .loop
	
	; done invalidating, clear the spinlock to 0
	mov  byte [gs:0x10], 0
	
	; also mark the end of the interrupt
	mov  rax, [MmHHDMBase]
	xor  rcx, rcx
	mov  rcx, 0x00000000FEE00000
	add  rax, rcx
	mov  dword [rax + 0xB0], 0
	
	mov  rax, r15
	ret

global KeDescendIntoUserMode
KeDescendIntoUserMode:
	; RDI - Initial program counter
	; RSI - Initial stack pointer
	; RDX - User context
	push qword SEG_RING_3_DATA | 3 ; push SS
	push rsi                       ; push RSP
	push qword 0x202               ; push RFLAGS
	push qword SEG_RING_3_CODE | 3 ; push CS
	push rdi                       ; push RIP
	
	; clear all the registers
	xor rax, rax
	xor rbp, rbp
	mov rdi, rdx
	CLEAR_REGS
	
	; finally, swap gs and return to user mode.
	cli
	swapgs
	iretq

section .bss
global KiTrapIplList
KiTrapIplList:
	resb 256              ; Reserve a byte for each vector

global KiTrapCallList
KiTrapCallList:
	resq 256              ; Reserve a pointer for each vector

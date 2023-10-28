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
	push rcx
	push rdx
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
	push rbp
	mov  rax, cr2
	push rax
	mov  ax,  gs
	push ax
	mov  ax,  fs
	push ax
	mov  ax,  es
	push ax
	mov  ax,  ds
	push ax
	cmp  ax,  SEG_RING_3_DATA     ; If the data segment is in user mode...
	jne  .a1
	swapgs                        ; Swap to the kernel GS.
.a1:
	nop
%endmacro

; Pop the entire state except RAX and RBX
%macro POP_STATE 0
	pop  ax
	mov  ds, ax
	cmp  ax,  SEG_RING_3_DATA     ; If the data segment is in user mode...
	jne  .a2
	swapgs                        ; Swap to the user's GS.
.a2:
	pop  ax
	mov  es, ax
	pop  ax
	mov  fs, ax
	pop  ax
	mov  gs, ax
	add  rsp, 8    ; the space occupied by the cr2 register
	pop  rbp
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
	pop  rdx
	pop  rcx
%endmacro

global KiTrapCommon
KiTrapCommon:
	push  rax
	push  rbx
	lea   rax, [rsp + 16]                  ; Get the pointer to the value after rbx and rax on the stack
	mov   rbx, [rax]                       ; Retrieve the interrupt number located there
	PUSH_STATE                             ; Push the state, except for the old ipl
	cld                                    ; Clear direction flag, will be restored by iretq
	lea   rax, [KiTrapIplList + rbx]       ; Retrieve the IPL for the respective interrupt vector
	movsx rdi, byte [rax]                  ; Get the IPL itself
	call  KiEnterHardwareInterrupt         ; Tell the kernel we entered a hardware interrupt
	push  rax                              ; Push the old IPL that we obtained from the function
	mov   rdi, rsp                         ; Retrieve the PKREGISTERS to call the trap handler
	lea   rax, [KiTrapCallList + 8 * rbx]  ; Retrieve the address to the pointer to the function to call
	mov   rax, [rax]                       ; Retrieve the pointer to the function to call...
	call  rax                              ; And call the function!!
	mov   rsp, rax                         ; Use the new PKREGISTERS instance as what to pull
	pop   rdi                              ; Pop the old IPL that was pushed before
	call  KiExitHardwareInterrupt          ; Tell the kernel we're exiting the hardware interrupt
	POP_STATE                              ; Pop the state
	pop   rbx                              ; Pop the RBX register
	pop   rax                              ; Pop the RAX register
	add   rsp, 16                          ; Pop the interrupt number and the error code
	iretq

; Arguments:
; rdi - Register state as in KREGISTERS
; global KeJumpContext
; KeJumpContext:
; 	mov  rsp, rdi
; 	POPSTATE
; 	add  rsp, 8    ; the KREGISTERS struct contains a dummy error code which we don't use..
; 	iretq

section .bss
global KiTrapIplList
KiTrapIplList:
	resb 256              ; Reserve a byte for each vector

global KiTrapCallList
KiTrapCallList:
	resq 256              ; Reserve a pointer for each vector

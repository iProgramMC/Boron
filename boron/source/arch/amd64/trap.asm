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
; NO_RETURN KiPopFullFrame(PKREGISTERS Regs@rax)
KiPopFullFrame:
	pop   rdi                              ; Pop the old IPL that was pushed before
	cmp   rdi, MAGIC_IPL                   ; Check if it's the magic IPL
	je    KeExitUsingPartialFrame          ; If yes, exit with the partial frame instead
	call  KiExitHardwareInterrupt          ; Tell the kernel we're exiting the hardware interrupt
	POP_STATE                              ; Pop the state
	pop   rbx                              ; Pop the RBX register
	pop   rax                              ; Pop the RAX register
	add   rsp, 16                          ; Pop the interrupt number and the error code
	iretq

; void KeSetPendingEvent(int PendingEvent);
extern KeSetPendingEvent

; PKREGISTERS KiHandleSoftIpi(PKREGISTERS Regs);
extern KiHandleSoftIpi

; void KeYieldCurrentThreadSub()
global KeYieldCurrentThreadSub
KeYieldCurrentThreadSub:
	; Push the state of the thread.
	mov  rcx, rsp                   ; Store the old contents of RSP into RCX, a scratch register
	xor  rax, rax                   ; Clear RAX, we'll use it to push segment registers
	mov  ax, ss                     ; Push the stack segment number, because we perform ...
	push rax                        ; ... an 'iretq' when exiting yield, this is important
	push rcx                        ; Push the old value of RSP
	pushfq                          ; Push the RFLAGS
	mov  ax, cs                     ; Push the code segment number for the same reason
	push rax
	lea  rax, [rel KeExitYield]     ; Load the address of KeExitYield so that we return there when we're scheduled in again
	push rax                        ; 
	sub  rsp, 8 * 3                 ; Push Error Code, Int Number, RAX.
	push rbx                        ; Push RBX, a callee saved register
	sub  rsp, 8 * 6                 ; Push RCX, RDX, R8, R9, R10 and R11
	push r12                        ; All of these are callee saved registers
	push r13
	push r14
	push r15
	sub  rsp, 8 * 2                 ; RSI and RDI are scratch registers
	push rbp                        ; RBP is a callee saved register
	sub  rsp, 8                     ; CR2
	xor  rax, rax                   ; Clear RAX once again to push some more segment registers
	mov  ax, gs
	push ax
	mov  ax, fs
	push ax
	mov  ax, es
	push ax
	mov  ax, ds
	push ax
	push qword MAGIC_IPL            ; Push the magic IPL - the IPL is saved by the caller
	mov  rdi, rsp                   ; Call the soft IPI handler
	call KiHandleSoftIpi
	mov  rsp, rax                   ; New interrupt frame is in RAX
	pop  rax                        ; Pop the "OldIPL" field
	cmp  rax, MAGIC_IPL             ; If the OldIPL we pushed is MAGIC_IPL, that means we pushed this frame in KeYieldCurrentThread
	jne  KeExitUsingFullFrame       ; Otherwise, we need to exit using the full frame
KeExitUsingPartialFrame:
	pop  ax                         ; Restore the state as pushed by KeYieldCurrentThreadSub
	mov  ds, ax
	pop  ax
	mov  es, ax
	pop  ax
	mov  fs, ax
	pop  ax
	mov  gs, ax
	add  rsp, 8                     ; CR2
	pop  rbp
	add  rsp, 8 * 2                 ; RDI and RSI
	pop  r15
	pop  r14
	pop  r13
	pop  r12
	add  rsp, 8 * 6                 ; R11, R10, R9, R8, RDX, RCX
	pop  rbx
	add  rsp, 8 * 3                 ; RAX, Int Number, Error Code
	iretq                           ; Pop the RIP, CS, RFLAGS, RSP and SS all in one, and return
KeExitUsingFullFrame:
	push rax                        ; Push IPL back on the stack
	jmp  KiPopFullFrame             ; Pop the full frame as if we were returning from an interrupt
KeExitYield:
	ret

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

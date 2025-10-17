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

; int KiTryEnterHardwareInterrupt(int IntNum);
extern KiTryEnterHardwareInterrupt
; void KiExitHardwareInterrupt(PKREGISTERS Registers);
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

global KiTrapCommon
KiTrapCommon:
	push  eax
	push  ecx
	push  edx
	
	; Check if we can enter the interrupt handler, based on the
	; current interrupt number.  This interrupt number has an
	; IPL associated with it; if it's lower than the current one,
	; this function returns -1.
	mov   eax, [esp + 12]
	push  eax
	call  KiTryEnterHardwareInterrupt
	add   esp, 4
	
	cmp   eax, 0                           ; If it returned -1, it means we're not allowed to enter yet.
	jl   .returnEarly
	
	; Push the old IPL.
	push  eax
	
	; Get the pointer to the IP, because we want to construct a
	; fake stack frame that includes it.
	mov   ecx, [esp + 24]
	push  ecx
	push  ebp
	mov   ebp, esp
	
	; Push the rest of the state.
	push  ebx
	push  esi
	push  edi
	mov   eax, cr2
	push  eax
	
	; Then, find the current interrupt number again and run it.
	; The interrupt handler will, if it returns, return the pointer
	; to the new stack, so we'll load that instead of restoring state
	; like usual.
	
	; Note: this 40 is carefully counted!
	mov   ecx, [esp + 40]
	push  esp
	call  [KiTrapCallList + ecx * 4]
	mov   esp, eax
	
	; Exit the hardware interrupt now.
	push  esp
	call  KiExitHardwareInterrupt
	
	; Pop the state and return.
	add   esp, 8 ; pop the argument we pushed for KiExitHardwareInterrupt + the CR2
	pop   edi
	pop   esi
	pop   ebx
	pop   ebp
	add   esp, 8 ; pop SFRA + old IPL
	
.returnEarly:
	pop   edx
	pop   ecx
	pop   eax
	add   esp, 8 ; pop IntNum + ErrorCode
KeIretdBreakpoint:
	iretd

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
	mov edi, edx
	
	; clear all the registers
	xor eax, eax
	xor ebp, ebp
	xor ebx, ebx
	xor ecx, ecx
	xor edx, edx
	xor esi, esi
	
	; finally, swap gs and return to user mode.
	cli
	iretd

global KiCallSoftwareInterrupt
KiCallSoftwareInterrupt:
	mov eax, [esp + 4]
	mov ecx, [esp]
	pushfd                         ; push eflags
	push dword SEG_RING_0_CODE     ; push cs
	push ecx                       ; push eip - return value
	cli                            ; ensure interrupts are disabled
	jmp [KiTrapCallList + eax * 4] ; jump to the specific trap
	; note: the function will return directly to the return address

section .bss
global KiTrapIplList
KiTrapIplList:
	resb 256              ; Reserve a byte for each vector

global KiTrapCallList
KiTrapCallList:
	resq 256              ; Reserve a pointer for each vector

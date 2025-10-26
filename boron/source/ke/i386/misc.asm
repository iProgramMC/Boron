;
;    The Boron Operating System
;    Copyright (C) 2025 iProgramInCpp
; 
; Module name:
;    ke/i386/misc.asm
; 	
; Abstract:
;    This module implements certain utility functions for
;    the i386 platform.
; 	
; Author:
;    iProgramInCpp - 14 October 2025
;
bits 32

%include "arch/i386.inc"

; these functions get the EIP and EBP respectively
global KiGetEIP
global KiGetEBP

KiGetEIP:
	mov eax, [esp]
	ret

KiGetEBP:
	mov eax, ebp
	ret

; these functions set the CR3
global KeSetCurrentPageTable
global KeGetCurrentPageTable

; void* KeGetCurrentPageTable()
KeGetCurrentPageTable:
	mov eax, cr3
	ret

; void KeSetCurrentPageTable(void* pt)
KeSetCurrentPageTable:
	mov eax, [esp + 4]
	mov cr3, eax
	ret

; int MmProbeAddressSub(void* Address, size_t Length, bool ProbeWrite);
global MmProbeAddressSub
MmProbeAddressSub:
; This function performs the actual work of probing the addresses. It is
; written in assembly to allow for predictability of the stack layout. This
; makes using the alternative exit, returning the STATUS_FAULT code, after
; an invalid page fault, much easier, since we can know the stack layout the
; entire time we're probing.
	
	push ebp
	mov  ebp, esp
	push edi
	push esi
	
	mov  edi, [ebp + 8]
	mov  esi, [ebp + 12]
	mov   dl, [ebp + 16]
	
	; edi - Address
	; esi - Length
	;  dl - ProbeWrite
	mov  ecx, edi          ; Load the address into ecx, a scratch register.
	add  ecx, esi          ; Add the length to ecx.
	dec  ecx               ; Decrement ecx.  This will be the final byte of the range we're probing.
	mov  al, [ecx]         ; Load a byte from that address.
	test dl, dl            ; Check if ProbeWrite is zero.
	jz   .StartLoop
	mov  [ecx], al         ; If not, then store that byte to that address.
.StartLoop:
	mov  al, [edi]         ; Load a byte from the start address.
	test dl, dl            ; Check if ProbeWrite is zero
	jz   .DontWrite
	mov  [edi], al         ; If not, then store that byte to that address.
.DontWrite:
	add  edi, 4096        ; Add a page size to rdi.
	cmp  edi, ecx         ; Check if the end address has been reached.
	jb   .StartLoop       ; If not, then probe another page.
	
	xor  eax, eax         ; Return with an exit code of zero (STATUS_SUCCESS)
	pop  esi
	pop  edi
	pop  ebp
	ret

; int MmSafeCopySub(void* Destination, void* Source, size_t Length);
global MmSafeCopySub
MmSafeCopySub:
	push ebp
	mov  ebp, esp
	
	cld               ; we don't want the copy the other way
	push edi
	push esi
	mov  edi, [ebp + 8]
	mov  esi, [ebp + 12]
	mov  ecx, [ebp + 16]
	rep  movsb
	xor  eax, eax
	; fall through
	
; void MmProbeAddressSubEarlyReturn()
; Returns early from MmProbeAddressSub and MmSafeCopySub. Called by the invalid page fault handler.
global MmProbeAddressSubEarlyReturn
MmProbeAddressSubEarlyReturn:
	pop  esi
	pop  edi
	pop  ebp
	ret

; void KiSwitchThreadStack(void** OldStack, void** NewStack)
global KiSwitchThreadStack
KiSwitchThreadStack:
	push ebp
	mov  ebp, esp
	
	mov  eax, [ebp + 8]
	mov  ecx, [ebp + 12]
	
	; push System V ABI callee saved registers
	pushfd
	push ebx
	push esi
	push edi
	
	; Store the current rsp into *OldStack.
	mov  dword [eax], esp
	
	; Load the current rsp from NewStack.
	mov  esp, dword [ecx]
	
	; Pop everything and return as usual.
KiPopEverythingAndReturn:
	pop  edi
	pop  esi
	pop  ebx
	xor  eax, eax
	
	; Pop EFLAGS and return
	popfd
	pop ebp
	ret

; Arguments:
; edi - Thread entry point.
; esi - Thread context.
extern KiUnlockDispatcher
global KiThreadEntryPoint
KiThreadEntryPoint:
	push esi
	
	push dword 0
	call KiUnlockDispatcher
	add  esp, 4
	
	; ebx saved because KiUnlockDispatcher is SysV compliant
	; eax still pushed-- will be used as the argument
	call edi

; Used for the init phase of the scheduler, before a thread is scheduled in.
global KiSwitchThreadStackForever
KiSwitchThreadStackForever:
	mov esp, [esp + 4]
	jmp KiPopEverythingAndReturn

; void KepLoadGdt(GdtDescriptor* desc);
global KepLoadGdt
KepLoadGdt:
	mov  eax, [esp + 4]
	lgdt [eax]
	
	; update the code segment
	push dword 0x08     ; code segment
	lea  eax, [rel .a]  ; jump address
	push eax
	retfd               ; return far - will go to .a now
.a:
	; update the segments
	mov  ax, 0x10
	mov  ds, ax
	mov  es, ax
	mov  fs, ax
	mov  gs, ax
	mov  ss, ax
	ret

; void KepLoadTss(int descriptor)
global KepLoadTss
KepLoadTss:
	mov  ax, [esp + 4]
	ltr  ax
	ret

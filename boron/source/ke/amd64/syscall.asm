;
;    The Boron Operating System
;    Copyright (C) 2025 iProgramInCpp
; 
; Module name:
;    ke/amd64/syscall.asm
; 	
; Abstract:
;    This module contains the implementation for the
;    system service ("syscall") handler.
; 	
; Author:
;    iProgramInCpp - 22 April 2025
;

bits 64
%include "arch/amd64.inc"

;%define ENABLE_SYSCALL_TRACE

; *** SYSTEM SERVICE TABLE ***
extern OSAllocateVirtualMemory
extern OSClose
extern OSCreateEvent
extern OSCreateMutex
extern OSCreatePipe
extern OSCreateProcess
extern OSCreateTerminal
extern OSCreateTerminalIoHandles
extern OSCreateThread
extern OSDeviceIoControl
extern OSDuplicateHandle
extern OSExitProcess
extern OSExitThread
extern OSFreeVirtualMemory
extern OSGetAlignmentFile
extern OSGetCurrentPeb
extern OSGetCurrentTeb
extern OSGetExitCodeProcess
extern OSGetLengthFile
extern OSGetMappedFileHandle
extern OSGetTickCount
extern OSGetTickFrequency
extern OSGetVersionNumber
extern OSMapViewOfObject
extern OSOpenEvent
extern OSOpenFile
extern OSOpenMutex
extern OSOutputDebugString
extern OSPulseEvent
extern OSQueryEvent
extern OSQueryMutex
extern OSReadDirectoryEntries
extern OSReadFile
extern OSReleaseMutex
extern OSResetDirectoryReadHead
extern OSResetEvent
extern OSSetCurrentPeb
extern OSSetCurrentTeb
extern OSSetEvent
extern OSSetExitCode
extern OSSetPebProcess
extern OSSetSuspendedThread
extern OSSleep
extern OSTerminateThread
extern OSTouchFile
extern OSWaitForMultipleObjects
extern OSWaitForSingleObject
extern OSWriteFile
extern OSWriteVirtualMemory

global KiSystemServiceTable
global KiSystemServiceTableEnd
KiSystemServiceTable:
	dq OSAllocateVirtualMemory
	dq OSClose
	dq OSCreateEvent
	dq OSCreateMutex
	dq OSCreatePipe
	dq OSCreateProcess
	dq OSCreateTerminal
	dq OSCreateTerminalIoHandles
	dq OSCreateThread
	dq OSDeviceIoControl
	dq OSDuplicateHandle
	dq OSExitProcess
	dq OSExitThread
	dq OSFreeVirtualMemory
	dq OSGetAlignmentFile
	dq OSGetCurrentPeb
	dq OSGetCurrentTeb
	dq OSGetExitCodeProcess
	dq OSGetLengthFile
	dq OSGetMappedFileHandle
	dq OSGetTickCount
	dq OSGetTickFrequency
	dq OSGetVersionNumber
	dq OSMapViewOfObject
	dq OSOpenEvent
	dq OSOpenFile
	dq OSOpenMutex
	dq OSOutputDebugString
	dq OSPulseEvent
	dq OSQueryEvent
	dq OSQueryMutex
	dq OSReadDirectoryEntries
	dq OSReadFile
	dq OSReleaseMutex
	dq OSResetDirectoryReadHead
	dq OSResetEvent
	dq OSSetCurrentPeb
	dq OSSetCurrentTeb
	dq OSSetEvent
	dq OSSetExitCode
	dq OSSetPebProcess
	dq OSSetSuspendedThread
	dq OSSleep
	dq OSTerminateThread
	dq OSTouchFile
	dq OSWaitForMultipleObjects
	dq OSWaitForSingleObject
	dq OSWriteFile
	dq OSWriteVirtualMemory
KiSystemServiceTableEnd:
	nop

; When calling the system call handler, the following registers take on the following tasks:
;
; RAX - System Call Number
; RDI - Argument 1
; RSI - Argument 2
; RDX - Argument 3
; R10 - Argument 4
; R8  - Argument 5
; R9  - Argument 6
; R12 - Argument 7
; R13 - Argument 8
; R14 - Argument 9
;
; RBX, RCX, and R15 are used by this handler and can't be provided.

extern KiCheckTerminatedUserMode
extern KiSystemServices
global KiSystemServiceHandler
KiSystemServiceHandler:
	cmp  rax, (KiSystemServiceTableEnd - KiSystemServiceTable) / 8
	jge  .invalidCall
	
	; The old RIP is saved into RCX, and the old RFLAGS is saved into R11.
	; The RFLAGS have then been masked with the IA32_FMASK MSR.
	; CS and SS are loaded from bits 47:32 of the IA32_STAR MSR.
	swapgs
	mov  r15, [gs:0x18]
	mov  rbx, rsp
	mov  rsp, r15
	push rbx
	sti
	
	push rcx
	push r11

	push rbp
	mov  rbp, rsp
	
	; Push the extra arguments on the stack.  When the system service returns,
	; these are cleaned up by the caller.
	push r14
	push r13
	push r12
	
	; Fix up argument #4
	mov  rcx, r10
	
%ifdef ENABLE_SYSCALL_TRACE
	call KiTraceSystemServiceCall
%endif
	
	call [KiSystemServiceTable + 8 * rax]
	
	push rax
	call KiCheckTerminatedUserMode
	pop rax
	
	; Clean up the 3 extra parameters.
	add  rsp, 24
	
	; Currently we don't support returning through multiple registers.
	; As such, every register other than RAX is cleared.
	;
	; TODO: Add a table to see whether or not RAX needs to be cleared or not.
	; RBP is preserved.
	
	CLEAR_REGS
	
	pop  rbp
	pop  r11
	pop  rcx
	
	cli
	pop  rsp
	swapgs
	o64 sysret

.invalidCall:
	; Invalid system call, return with the status of STATUS_INVALID_PARAMETER (1)
	mov  rax, 1
	o64 sysret

%ifdef ENABLE_SYSCALL_TRACE

extern KePrintSystemServiceDebug
global KiTraceSystemServiceCall
KiTraceSystemServiceCall:
	push rbp
	mov  rbp, rsp
	push rax
	push rbx
	push rcx
	push rdx
	push rdi
	push rsi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	
	mov rdi, rax
	call KePrintSystemServiceDebug
	
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rsi
	pop rdi
	pop rdx
	pop rcx
	pop rbx
	pop rax
	pop rbp
	ret

%endif

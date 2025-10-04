; The Boron Operating System
; Copyright (C) 2025 iProgramInCpp

; Parameters:
;  #1 - Call Number (RAX)
;  #2 - System Call
%macro CALL 2
global %2
%2:
	push rbp
	mov  rbp, rsp
	push rbx
	push r12
	push r13
	push r14
	push r15
	mov  r10, rcx
	mov  rax, %1
	syscall
	pop  r15
	pop  r14
	pop  r13
	pop  r12
	pop  rbx
	pop  rbp
	ret
%endmacro

; Unlike the previous call macro, this loads 3 extra arguments on the stack into r12, r13, and r14.
%macro CALLEX 2
global %2
%2:
	push rbp
	mov  rbp, rsp
	push rbx
	push r12
	push r13
	push r14
	push r15
	mov  r12, [rbp + 16]
	mov  r13, [rbp + 24]
	mov  r14, [rbp + 32]
	mov  r10, rcx
	mov  rax, %1
	syscall
	pop  r15
	pop  r14
	pop  r13
	pop  r12
	pop  rbx
	pop  rbp
	ret
%endmacro

CALL 0, OSAllocateVirtualMemory
CALL 1, OSClose
CALL 2, OSCreateEvent
CALL 3, OSCreateMutex
CALL 4, OSCreatePipe
CALL 5, OSCreateProcessInternal
CALL 6, OSCreateThread
CALL 7, OSDuplicateHandle
CALL 8, OSExitProcess
CALL 9, OSExitThread
CALL 10, OSFreeVirtualMemory
CALL 11, OSGetAlignmentFile
CALL 12, OSGetCurrentPeb
CALL 13, OSGetCurrentTeb
CALL 14, OSGetExitCodeProcess
CALL 15, OSGetLengthFile
CALL 16, OSGetMappedFileHandle
CALLEX 17, OSMapViewOfObject
CALL 18, OSOpenEvent
CALL 19, OSOpenFile
CALL 20, OSOpenMutex
CALL 21, OSOutputDebugString
CALL 22, OSPulseEvent
CALL 23, OSQueryEvent
CALL 24, OSQueryMutex
CALL 25, OSReadDirectoryEntries
CALL 26, OSReadFile
CALL 27, OSReleaseMutex
CALL 28, OSResetDirectoryReadHead
CALL 29, OSResetEvent
CALL 30, OSSetCurrentPeb
CALL 31, OSSetCurrentTeb
CALL 32, OSSetEvent
CALL 33, OSSetExitCode
CALL 34, OSSetSuspendedThread
CALL 35, OSSleep
CALL 36, OSTerminateThread
CALL 37, OSTouchFile
CALL 38, OSWaitForMultipleObjects
CALL 39, OSWaitForSingleObject
CALLEX 40, OSWriteFile
CALL 41, OSWriteVirtualMemory

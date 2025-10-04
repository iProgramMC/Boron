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
CALL 12, OSGetCurrentTeb
CALL 13, OSGetExitCodeProcess
CALL 14, OSGetLengthFile
CALL 15, OSGetMappedFileHandle
CALLEX 16, OSMapViewOfObject
CALL 17, OSOpenEvent
CALL 18, OSOpenFile
CALL 19, OSOpenMutex
CALL 20, OSOutputDebugString
CALL 21, OSPulseEvent
CALL 22, OSQueryEvent
CALL 23, OSQueryMutex
CALL 24, OSReadDirectoryEntries
CALL 25, OSReadFile
CALL 26, OSReleaseMutex
CALL 27, OSResetDirectoryReadHead
CALL 28, OSResetEvent
CALL 29, OSSetCurrentTeb
CALL 30, OSSetEvent
CALL 31, OSSetExitCode
CALL 32, OSSetSuspendedThread
CALL 33, OSSleep
CALL 34, OSTerminateThread
CALL 35, OSTouchFile
CALL 36, OSWaitForMultipleObjects
CALL 37, OSWaitForSingleObject
CALLEX 38, OSWriteFile
CALL 39, OSWriteVirtualMemory

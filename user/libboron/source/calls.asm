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
CALL 24, OSReadFile
CALL 25, OSReleaseMutex
CALL 26, OSResetEvent
CALL 27, OSSetCurrentTeb
CALL 28, OSSetEvent
CALL 29, OSSetExitCode
CALL 30, OSSetSuspendedThread
CALL 31, OSSleep
CALL 32, OSTerminateThread
CALL 33, OSTouchFile
CALL 34, OSWaitForMultipleObjects
CALL 35, OSWaitForSingleObject
CALLEX 36, OSWriteFile
CALL 37, OSWriteVirtualMemory

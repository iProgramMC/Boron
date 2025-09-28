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
CALL 5, OSCreateProcess
CALL 6, OSCreateThread
CALL 7, OSDummy
CALL 8, OSExitProcess
CALL 9, OSExitThread
CALL 10, OSFreeVirtualMemory
CALL 11, OSGetAlignmentFile
CALL 12, OSGetCurrentTeb
CALL 13, OSGetExitCodeProcess
CALL 14, OSGetLengthFile
CALLEX 15, OSMapViewOfObject
CALL 16, OSOpenEvent
CALL 17, OSOpenFile
CALL 18, OSOpenMutex
CALL 19, OSOutputDebugString
CALL 20, OSPulseEvent
CALL 21, OSQueryEvent
CALL 22, OSQueryMutex
CALL 23, OSReadFile
CALL 24, OSReleaseMutex
CALL 25, OSResetEvent
CALL 26, OSSetCurrentTeb
CALL 27, OSSetEvent
CALL 28, OSSetExitCode
CALL 29, OSSetSuspendedThread
CALL 30, OSSleep
CALL 31, OSTerminateThread
CALL 32, OSTouchFile
CALL 33, OSWaitForMultipleObjects
CALL 34, OSWaitForSingleObject
CALLEX 35, OSWriteFile

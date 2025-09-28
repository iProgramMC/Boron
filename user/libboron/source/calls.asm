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
CALL 13, OSGetLengthFile
CALLEX 14, OSMapViewOfObject
CALL 15, OSOpenEvent
CALL 16, OSOpenFile
CALL 17, OSOpenMutex
CALL 18, OSOutputDebugString
CALL 19, OSPulseEvent
CALL 20, OSQueryEvent
CALL 21, OSQueryMutex
CALL 22, OSReadFile
CALL 23, OSReleaseMutex
CALL 24, OSResetEvent
CALL 25, OSSetCurrentTeb
CALL 26, OSSetEvent
CALL 27, OSSetSuspendedThread
CALL 28, OSSleep
CALL 29, OSTerminateThread
CALL 30, OSTouchFile
CALL 31, OSWaitForMultipleObjects
CALL 32, OSWaitForSingleObject
CALLEX 33, OSWriteFile

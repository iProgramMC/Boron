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
CALL 12, OSGetLengthFile
CALLEX 13, OSMapViewOfObject
CALL 14, OSOpenEvent
CALL 15, OSOpenFile
CALL 16, OSOpenMutex
CALL 17, OSOutputDebugString
CALL 18, OSPulseEvent
CALL 19, OSQueryEvent
CALL 20, OSQueryMutex
CALL 21, OSReadFile
CALL 22, OSReleaseMutex
CALL 23, OSResetEvent
CALL 24, OSSetEvent
CALL 25, OSSetSuspendedThread
CALL 26, OSSleep
CALL 27, OSTerminateThread
CALL 28, OSTouchFile
CALL 29, OSWaitForMultipleObjects
CALL 30, OSWaitForSingleObject
CALLEX 31, OSWriteFile

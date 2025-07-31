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

CALL 0, OSAllocateVirtualMemory
CALL 1, OSClose
CALL 2, OSCreateEvent
CALL 3, OSCreateMutex
CALL 4, OSCreateProcess
CALL 5, OSCreateThread
CALL 6, OSDummy
CALL 7, OSExitThread
CALL 8, OSFreeVirtualMemory
CALL 9, OSGetAlignmentFile
CALL 10, OSGetLengthFile
CALL 11, OSMapViewOfObject
CALL 12, OSOpenEvent
CALL 13, OSOpenFile
CALL 14, OSOpenMutex
CALL 15, OSOutputDebugString
CALL 16, OSPulseEvent
CALL 17, OSQueryEvent
CALL 18, OSQueryMutex
CALL 19, OSReadFile
CALL 20, OSReleaseMutex
CALL 21, OSResetEvent
CALL 22, OSSetEvent
CALL 23, OSSetSuspendedThread
CALL 24, OSSleep
CALL 25, OSTerminateThread
CALL 26, OSTouchFile
CALL 27, OSWaitForMultipleObjects
CALL 28, OSWaitForSingleObject

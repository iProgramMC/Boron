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
CALL 7, OSExitProcess
CALL 8, OSExitThread
CALL 9, OSFreeVirtualMemory
CALL 10, OSGetAlignmentFile
CALL 11, OSGetLengthFile
CALL 12, OSMapViewOfObject
CALL 13, OSOpenEvent
CALL 14, OSOpenFile
CALL 15, OSOpenMutex
CALL 16, OSOutputDebugString
CALL 17, OSPulseEvent
CALL 18, OSQueryEvent
CALL 19, OSQueryMutex
CALL 20, OSReadFile
CALL 21, OSReleaseMutex
CALL 22, OSResetEvent
CALL 23, OSSetEvent
CALL 24, OSSetSuspendedThread
CALL 25, OSSleep
CALL 26, OSTerminateThread
CALL 27, OSTouchFile
CALL 28, OSWaitForMultipleObjects
CALL 29, OSWaitForSingleObject

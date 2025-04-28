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

CALL 0, OSClose
CALL 1, OSCreateThread
CALL 2, OSCreateProcess
CALL 3, OSDummy
CALL 4, OSExitThread
CALL 5, OSOutputDebugString
CALL 6, OSWaitForSingleObject
CALL 7, OSWaitForMultipleObjects

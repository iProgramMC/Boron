// The Boron Operating System
// Copyright (C) 2025 iProgramInCpp

.section .text
.att_syntax

.macro CALL number, name
	.globl \name
	.type  \name, @function
\name:
	pushq %rbp
	movq  %rsp, %rbp
	pushq %rbx
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	movq  %rcx, %r10
	movq  $\number, %rax
	syscall
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbx
	popq %rbp
	ret
.endm

// Unlike the previous call macro, this loads 3 extra arguments on the stack into r12, r13, and r14.
.macro CALLEX number, name
	.globl \name
	.type  \name, @function
\name:
	pushq %rbp
	movq  %rsp, %rbp
	pushq %rbx
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	movq  16(%rbp), %r12
	movq  24(%rbp), %r13
	movq  32(%rbp), %r14
	movq  %rcx, %r10
	movq  $\number, %rax
	syscall
	popq  %r15
	popq  %r14
	popq  %r13
	popq  %r12
	popq  %rbx
	popq  %rbp
	ret
.endm

CALL   0,  OSAllocateVirtualMemory
CALL   1,  OSClose
CALL   2,  OSCreateEvent
CALL   3,  OSCreateMutex
CALL   4,  OSCreatePipe
CALL   5,  OSCreateProcessInternal
CALL   6,  OSCreateThread
CALL   7,  OSDuplicateHandle
CALL   8,  OSExitProcess
CALL   9,  OSExitThread
CALL   10, OSFreeVirtualMemory
CALL   11, OSGetAlignmentFile
CALL   12, OSGetCurrentPeb
CALL   13, OSGetCurrentTeb
CALL   14, OSGetExitCodeProcess
CALL   15, OSGetLengthFile
CALL   16, OSGetMappedFileHandle
CALLEX 17, OSMapViewOfObject
CALL   18, OSOpenEvent
CALL   19, OSOpenFile
CALL   20, OSOpenMutex
CALL   21, OSOutputDebugString
CALL   22, OSPulseEvent
CALL   23, OSQueryEvent
CALL   24, OSQueryMutex
CALL   25, OSReadDirectoryEntries
CALL   26, OSReadFile
CALL   27, OSReleaseMutex
CALL   28, OSResetDirectoryReadHead
CALL   29, OSResetEvent
CALL   30, OSSetCurrentPeb
CALL   31, OSSetCurrentTeb
CALL   32, OSSetEvent
CALL   33, OSSetExitCode
CALL   34, OSSetSuspendedThread
CALL   35, OSSleep
CALL   36, OSTerminateThread
CALL   37, OSTouchFile
CALL   38, OSWaitForMultipleObjects
CALL   39, OSWaitForSingleObject
CALLEX 40, OSWriteFile
CALL   41, OSWriteVirtualMemory

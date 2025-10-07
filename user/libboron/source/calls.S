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
CALL   17, OSGetTickCount
CALL   18, OSGetTickFrequency
CALL   19, OSGetVersionNumber
CALLEX 20, OSMapViewOfObject
CALL   21, OSOpenEvent
CALL   22, OSOpenFile
CALL   23, OSOpenMutex
CALL   24, OSOutputDebugString
CALL   25, OSPulseEvent
CALL   26, OSQueryEvent
CALL   27, OSQueryMutex
CALL   28, OSReadDirectoryEntries
CALL   29, OSReadFile
CALL   30, OSReleaseMutex
CALL   31, OSResetDirectoryReadHead
CALL   32, OSResetEvent
CALL   33, OSSetCurrentPeb
CALL   34, OSSetCurrentTeb
CALL   35, OSSetEvent
CALL   36, OSSetExitCode
CALL   37, OSSetPebProcess
CALL   38, OSSetSuspendedThread
CALL   39, OSSleep
CALL   40, OSTerminateThread
CALL   41, OSTouchFile
CALL   42, OSWaitForMultipleObjects
CALL   43, OSWaitForSingleObject
CALLEX 44, OSWriteFile
CALL   45, OSWriteVirtualMemory

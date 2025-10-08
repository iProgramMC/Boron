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
CALL   7,  OSDeviceIoControl
CALL   8,  OSDuplicateHandle
CALL   9,  OSExitProcess
CALL   10, OSExitThread
CALL   11, OSFreeVirtualMemory
CALL   12, OSGetAlignmentFile
CALL   13, OSGetCurrentPeb
CALL   14, OSGetCurrentTeb
CALL   15, OSGetExitCodeProcess
CALL   16, OSGetLengthFile
CALL   17, OSGetMappedFileHandle
CALL   18, OSGetTickCount
CALL   19, OSGetTickFrequency
CALL   20, OSGetVersionNumber
CALLEX 21, OSMapViewOfObject
CALL   22, OSOpenEvent
CALL   23, OSOpenFile
CALL   24, OSOpenMutex
CALL   25, OSOutputDebugString
CALL   26, OSPulseEvent
CALL   27, OSQueryEvent
CALL   28, OSQueryMutex
CALL   29, OSReadDirectoryEntries
CALL   30, OSReadFile
CALL   31, OSReleaseMutex
CALL   32, OSResetDirectoryReadHead
CALL   33, OSResetEvent
CALL   34, OSSetCurrentPeb
CALL   35, OSSetCurrentTeb
CALL   36, OSSetEvent
CALL   37, OSSetExitCode
CALL   38, OSSetPebProcess
CALL   39, OSSetSuspendedThread
CALL   40, OSSleep
CALL   41, OSTerminateThread
CALL   42, OSTouchFile
CALL   43, OSWaitForMultipleObjects
CALL   44, OSWaitForSingleObject
CALLEX 45, OSWriteFile
CALL   46, OSWriteVirtualMemory

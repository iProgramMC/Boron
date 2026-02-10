// The Boron Operating System
// Copyright (C) 2025 iProgramInCpp

#ifdef TARGET_AMD64

.section .text
.att_syntax

.macro CALL number, argcount, name
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
	
	.if \argcount > 6
		movq 16(%rbp), %r12
	.endif
	.if \argcount > 7
		movq 24(%rbp), %r13
	.endif
	.if \argcount > 8
		movq 32(%rbp), %r14
	.endif
	.if \argcount > 3
		movq  %rcx, %r10
	.endif
	
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

#include "calltbl.h"

#endif

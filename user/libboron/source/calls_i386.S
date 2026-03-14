// The Boron Operating System
// Copyright (C) 2025 iProgramInCpp

#ifdef TARGET_I386

.section .text
.att_syntax

.macro CALL number, argcount, name
	.globl \name
	.type  \name, @function
\name:
	pushl %ebp
	movl  %esp, %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	
	movl $\number, %eax
	
	.if \argcount > 0
		movl 8(%ebp), %ebx
	.endif
	.if \argcount > 1
		movl 12(%ebp), %ecx
	.endif
	.if \argcount > 2
		movl 16(%ebp), %edx
	.endif
	.if \argcount > 3
		movl 20(%ebp), %esi
	.endif
	.if \argcount > 4
		movl 24(%ebp), %edi
	.endif
	
	.if \argcount == 6
		/* if we have *exactly* 6 arguments, then just pass the */
		/* last one into EBP */
		movl 28(%ebp), %ebp
	.elseif \argcount > 6
		/* if we have *more* than 6 arguments, we need to pass a */
		/* pointer to the last arguments which we'll copy from */
		/* the stack */
		leal 28(%ebp), %ebp
	.endif
	
	int $0x80
	
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret
.endmacro

#include "calltbl.h"

#endif

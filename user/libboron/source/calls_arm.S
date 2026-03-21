// The Boron Operating System
// Copyright (C) 2026 iProgramInCpp

#ifdef TARGET_ARM

.macro CALL number, argcount, name
	.global \name
	.type   \name, %function
\name:
	.if \argcount > 4
		mov  fp, sp
	.endif
	.if \argcount <= 4
		/* R0-R3 are already set up with parameters. No need to do anything. */
		/* In fact, the kernel helpfully backs up r4-r12 for us too! */
		svc #\number
	.elseif \argcount == 5
		push {r4}
		ldr  r4, [fp, #0]
		svc  #\number
		pop  {r4}
	.elseif \argcount == 6
		push {r4-r5}
		ldr  r4, [fp, #0]
		ldr  r5, [fp, #4]
		svc  #\number
		pop  {r4-r5}
	.elseif \argcount == 7
		push {r4-r6}
		ldr  r4, [fp, #0]
		ldr  r5, [fp, #4]
		ldr  r6, [fp, #8]
		svc  #\number
		pop  {r4-r6}
	.elseif \argcount == 8
		push {r4-r7}
		ldr  r4, [fp, #0]
		ldr  r5, [fp, #4]
		ldr  r6, [fp, #8]
		ldr  r7, [fp, #12]
		svc  #\number
		pop  {r4-r7}
	.elseif \argcount == 9
		push {r4-r8}
		ldr  r4, [fp, #0]
		ldr  r5, [fp, #4]
		ldr  r6, [fp, #8]
		ldr  r7, [fp, #12]
		ldr  r8, [fp, #16]
		svc  #\number
		pop  {r4-r8}
	.else
		.error Not supported!
	.endif
	bx lr
.endm

#include "calltbl.h"

#endif

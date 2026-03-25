// The Boron Operating System
// Copyright (C) 2026 iProgramInCpp

#ifdef TARGET_ARM

.macro CALL number, argcount, name
	.global \name
	.type   \name, %function
\name:
	.if \argcount > 4
		push {fp, lr}
		mov  fp, sp
	.endif
	.if \argcount <= 4
		/* R0-R3 are already set up with parameters. No need to do anything. */
		/* In fact, the kernel helpfully backs up r4-r12 for us too! */
		svc #\number
		bx  lr
	.elseif \argcount == 5
		push {r4}
		ldr  r4, [fp, #8]
		svc  #\number
		pop  {r4}
		pop  {fp, pc}
	.elseif \argcount == 6
		push {r4-r5}
		ldr  r4, [fp, #8]
		ldr  r5, [fp, #12]
		svc  #\number
		pop  {r4-r5}
		pop  {fp, pc}
	.elseif \argcount == 7
		push {r4-r6}
		ldr  r4, [fp, #8]
		ldr  r5, [fp, #12]
		ldr  r6, [fp, #16]
		svc  #\number
		pop  {r4-r6}
		pop  {fp, pc}
	.elseif \argcount == 8
		push {r4-r7}
		ldr  r4, [fp, #8]
		ldr  r5, [fp, #12]
		ldr  r6, [fp, #16]
		ldr  r7, [fp, #20]
		svc  #\number
		pop  {r4-r7}
		pop  {fp, pc}
	.elseif \argcount == 9
		push {r4-r8}
		ldr  r4, [fp, #8]
		ldr  r5, [fp, #12]
		ldr  r6, [fp, #16]
		ldr  r7, [fp, #20]
		ldr  r8, [fp, #24]
		svc  #\number
		pop  {r4-r8}
		pop  {fp, pc}
	.else
		.error Not supported!
	.endif
.endm

#include "calltbl.h"

#endif

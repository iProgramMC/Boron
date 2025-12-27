/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/trap.S
	
Abstract:
    This module implements functions related to interrupt management
	for the armv6 architecture.
	
Author:
	iProgramInCpp - 26 December 2025
***/

	.section .text

	@ bool KeDisableInterrupts();
	.global KeDisableInterrupts
KeDisableInterrupts:
	mrs   r0, cpsr
	tst   r0, #(1 << 7)   @ test I bit
	moveq r0, #1          @ 1 if enabled
	movne r0, #0          @ 0 if disabled
	cpsid i
	bx    lr

	@ void KeRestoreInterrupts(bool OldState);
	.global KeRestoreInterrupts
KeRestoreInterrupts:
	cmp   r0, #0
	beq   1f
	cpsie i
1:	bx    lr

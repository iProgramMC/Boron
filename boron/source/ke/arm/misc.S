/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/misc.S
	
Abstract:
    This module implements miscellaneous hardware functions
	for the armv6 architecture.
	
Author:
	iProgramInCpp - 28 December 2025
***/

	.section .text

	@ void KeSetCurrentPageTable(HPAGEMAP PageMap);
	.global KeSetCurrentPageTable
KeSetCurrentPageTable:
	mcr p15, 0, r0, c2, c0, 0    @ Write TTBR0
	bx  lr

	@ HPAGEMAP KeSetCurrentPageTable();
	.global KeGetCurrentPageTable
KeGetCurrentPageTable:
	mrc p15, 0, r0, c2, c0, 0    @ Read TTBR0
	bx  lr

	@ void KeFlushTLB(void);
	.global KeFlushTLB
KeFlushTLB:
	mcr p15, 0, r0, c8, c7, 0    @ Invalidate Unified TLB Register
	bx  lr

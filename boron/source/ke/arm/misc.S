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
	bl  KeFlushTLB
	bx  lr

	@ HPAGEMAP KeSetCurrentPageTable();
	.global KeGetCurrentPageTable
KeGetCurrentPageTable:
	mrc p15, 0, r0, c2, c0, 0    @ Read TTBR0
	bx  lr

	@ void KeFlushTLB(void);
	.global KeFlushTLB
KeFlushTLB:
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 0   @ clean dcache
	mcr p15, 0, r0, c7, c10, 4   @ DSB - data sync barrier
	mcr p15, 0, r0, c8, c7, 0    @ invalidate unified TLB
	mcr p15, 0, r0, c7, c10, 4   @ DSB again
	mcr p15, 0, r0, c7, c5, 4    @ flush prefetch buffer
	bx  lr

	@void KeSweepIcache(void);
	.global KeSweepIcache
KeSweepIcache:
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0    @ flush icache + branch predictor cache
	mcr p15, 0, r0, c7, c5, 4    @ flush prefetch buffer
	bx  lr

	@void KeSweepDcache(void);
	.global KeSweepDcache
KeSweepDcache:
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 0   @ clean dcache
	mcr p15, 0, r0, c7, c10, 4   @ DSB
	mcr p15, 0, r0, c7, c10, 5   @ DMB
	mcr p15, 0, r0, c7, c6,  0   @ invalidate dcache
	bx  lr
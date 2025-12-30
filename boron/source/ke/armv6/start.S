/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/start.S
	
Abstract:
    This module implements the entry point of the Boron kernel
    for the armv6 architecture.
	
Author:
	iProgramInCpp - 26 December 2025
***/
.global KiBeforeSystemStartup
.extern KiSystemStartup
.extern KiUndefinedInstructionHandler
.extern KiSoftwareInterruptHandler
.extern KiPrefetchAbortHandler
.extern KiDataAbortHandler
.extern KiIrqHandler
.extern KiFiqHandler

/* define a Level 1 "Section" PTE */
/*#define L1PTE(Address) (((Address) & 0xFFF00000) | 0b111010000001110)*/

/* TEX = 0b111, AP = 0b01, P = 0, Domain = 0, CB = 0b11 */
.equ L1PTE_FLAGS_SEC, 0b111010000001110

.equ L1PTE_FLAGS_CPT, 0b111010000001101

.section .ipltext, "ax", %progbits
KiBeforeSystemStartup:
	/* set up the root page table */
	ldr r2, =L1PTE_FLAGS_SEC
	
	/* map first 2MB with identity */
	ldr r0, =KiRootPageTable
	ldr r1, =0x000000
	orr r1, r1, r2
	str r1, [r0, #0x0000]
	ldr r1, =0x100000
	orr r1, r1, r2
	str r1, [r0, #0x0004]
	
	/* map first 2MB with 0xC0000000 offset */
	ldr r0, =(KiRootPageTable + 0x3000)
	ldr r1, =0x000000
	orr r1, r1, r2
	str r1, [r0, #0x0000]
	ldr r1, =0x100000
	orr r1, r1, r2
	str r1, [r0, #0x0004]
	
	/* map PL011 for debugging purposes */
	/* versatilepb machine features it at offset 0x101F1000 */
	/* we need to map it to 0xD1800000 */
	ldr r0, =(KiRootPageTable + 0x3460)
	ldr r1, =0x10100000
	orr r1, r1, r2
	str r1, [r0, #0x0008]
	
	/* map the exception handlers to 0xFFF'F0'000 */
	ldr r0, =(KiRootPageTable + 0x3FFC)
	ldr r1, =KiHigh1MPageTable
	orr r1, r1, r2
	str r1, [r0, #0x0000]
	ldr r0, =(KiHigh1MPageTable + 0x3C0) /* 240th entry */
	ldr r1, =KiExceptionHandlerTable
	ldr r2, =L1PTE_FLAGS_CPT
	orr r1, r1, #(3 << 0) /* small page NX */
	orr r1, r1, #(1 << 4) /* super read/write */
	str r1, [r0, #0x0000]
	
	ldr r0, =KiRootPageTable
	mcr p15, 0, r0, c2, c0, 0   /* write TTBR0 */
	
	mov r0, #0x1
	mcr p15, 0, r0, c3, c0, 0   /* Domain Access Control: Domain 0 = Client */
	
	mov r0, #0x0                /* clear TTBCR to always use TTBR0 */
	mcr p15, 0, r0, c2, c0, 2
	
	mrc p15, 0, r0, c1, c0, 0   /* read SCTLR */
	orr r0, r0, #1              /* enable MMU */
	orr r0, r0, #(1<<2)         /* enable dcache, icache */
	orr r0, r0, #(1<<12)        /* enable icache */
	orr r0, r0, #(1<<13)        /* enable V - interrupt vectors start at 0xFFFF0000 */
	mcr p15, 0, r0, c1, c0, 0   /* write SCTLR */
	
    /* Set up a simple stack */
    ldr sp, =KiInitStackBottom
	
	/* Initialize the exception handler table */
	/* Note: Reset (0) and Reserved (0x14) not set */
	ldr r0, =KiExceptionHandlerTable
	ldr r1, =KiUndefinedInstructionHandler
	str r1, [r0, #4]
	ldr r1, =KiSoftwareInterruptHandler
	str r1, [r0, #8]
	ldr r1, =KiPrefetchAbortHandler
	str r1, [r0, #12]
	ldr r1, =KiDataAbortHandler
	str r1, [r0, #16]
	ldr r1, =KiIrqHandler
	str r1, [r0, #24]
	ldr r1, =KiFiqHandler
	str r1, [r0, #28]
	
	/* Initialize each exception's stack register.  These are temporary until threads start running. */
	cps #0x11 /* FIQ */
	ldr sp, =KiFiqStackBottom
	cps #0x12 /* IRQ */
	ldr sp, =KiIrqStackBottom
	cps #0x17 /* ABT */
	ldr sp, =KiAbortStackBottom
	cps #0x1B /* UND */
	ldr sp, =KiUndefinedInstructionStackBottom
	cps #0x13 /* switch back to SVC mode */

	/* jump to KiSystemStartup in .text at 0xC0000000 */
	ldr r0, =KiSystemStartup
	bx  r0

.section .iplbss
.global KiRootPageTable
.global KiExceptionHandlerTable
.global KiHigh1MPageTable

/* KEEP THIS at the start of iplbss! */
.align 16
KiRootPageTable:
	.space 16384

/* The exception handler table is mapped to 0xFFFF0000. Must be aligned to 4KB */
KiExceptionHandlerTable:
	.space 0x20

/* Page table used to map KiExceptionHandlerTable. Must be aligned to 1K */
KiHigh1MPageTable:
	.space 1024

.section bss
.align 16
KiInitStack:
	.space 2048
KiInitStackBottom:

/* define each exception's separate stack */
	.space 64
KiUndefinedInstructionStackBottom:
	.space 64
KiSoftwareInterruptStackBottom:
	.space 64
KiAbortStackBottom:
	.space 64
KiIrqStackBottom:
	.space 64
KiFiqStackBottom:

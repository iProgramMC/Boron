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

/* TEX = 0b001, AP = 0b01, P = 0, Domain = 0, CB = 0b11 */
/* .equ L1PTE_FLAGS_SEC, 0b111010000001110 */
.equ L1PTE_FLAGS_SEC, 0b000010000000010

/* P = 0, Domain = 0, Type = 0b01 */
.equ L1PTE_FLAGS_CPT, 0b0000000001

.section .ipltext, "ax", %progbits

/* The exception handler table is mapped to 0xFFFF0000. Must be aligned to 4KB */
.global KiExceptionHandlerTable
KiExceptionHandlerTable:
	b KiBeforeSystemStartup
	b KiUndefinedInstructionHandler
	b KiSoftwareInterruptHandler
	b KiPrefetchAbortHandler
	b KiDataAbortHandler
	.word 0
	b KiIrqHandler
	b KiFiqHandler

KiBeforeSystemStartup:
	cpsid if

	/* store the LPB pointer from r0 */
	ldr r1, =(KiBootloaderLpb - 0xC0000000)
	str r0, [r1, #0x0000]

	/* set up the root page table */
	ldr r2, =L1PTE_FLAGS_SEC
	
	/* map first 4MB with identity */
	ldr r0, =KiRootPageTable
	ldr r1, =0x000000
	orr r1, r1, r2
	str r1, [r0, #0x0000]
	ldr r1, =0x100000
	orr r1, r1, r2
	str r1, [r0, #0x0004]
	ldr r1, =0x200000
	orr r1, r1, r2
	str r1, [r0, #0x0008]
	ldr r1, =0x300000
	orr r1, r1, r2
	str r1, [r0, #0x000C]
	
	/* TEMP: map framebuffer to 0x400000 */
	ldr r1, =0x0FD00000
	orr r1, r1, r2
	str r1, [r0, #0x0010]
	
	/* map first 4MB with 0xC0000000 offset */
	ldr r0, =(KiRootPageTable + 0x3000)
	ldr r1, =0x000000
	orr r1, r1, r2
	str r1, [r0, #0x0000]
	ldr r1, =0x100000
	orr r1, r1, r2
	str r1, [r0, #0x0004]
	ldr r1, =0x200000
	orr r1, r1, r2
	str r1, [r0, #0x0008]
	ldr r1, =0x300000
	orr r1, r1, r2
	str r1, [r0, #0x000C]

/*
	@ map PL011 for debugging purposes
	@ raspi1ap machine features it at offset 0x101f1000
	@ we need to map it to 0xD1800000
	ldr r0, =(KiRootPageTable + 0x3460)
	ldr r1, =0x10100000
	orr r1, r1, r2
	str r1, [r0, #0x0000]
*/

	/* map UART0 for debugging purposes */
	/* iPod touch 1G maps it at 0x3CC00000 */
	/* TODO: DO NOT hardcode this */
	ldr r0, =(KiRootPageTable + 0x3460)
	ldr r1, =0x3CC00000
	orr r1, r1, r2
	str r1, [r0, #0x0000]
	
	mcr p15, 0, r1, c7, c10, 4  /* data sync buffer */
	
	ldr r0, =KiRootPageTable
	mcr p15, 0, r0, c2, c0, 0   /* write TTBR0 */
	mcr p15, 0, r1, c8, c7, 0   /* TLB invalidate */
	
	mov r0, #0x1
	mcr p15, 0, r0, c3, c0, 0   /* Domain Access Control: Domain 0 = Client */
	
	mov r1, #0x0
	mcr p15, 0, r1, c2, c0, 2   /* clear TTBCR to always use TTBR0 */
	mcr p15, 0, r1, c7, c5, 0   /* i-cache sweep */
	mcr p15, 0, r1, c7, c6, 0   /* d-cache sweep */
	mcr p15, 0, r1, c7, c10, 4  /* data sync buffer */
	mcr p15, 0, r1, c8, c7, 0   /* TLB invalidate */
	
	mrc p15, 0, r0, c1, c0, 0   /* read SCTLR */
	orr r0, r0, #1              /* enable MMU */
	orr r0, r0, #(1<<2)         /* enable dcache, icache */
	orr r0, r0, #(1<<12)        /* enable icache */
	bic r0, r0, #(1<<28)        /* clear TEX remap, if needed */
	orr r0, r0, #(1<<13)        /* enable V - interrupt vectors start at 0xFFFF0000 */
	
#ifndef TARGET_ARMV5
	orr r0, r0, #(1<<23)        /* enable XP - disables ARMv5 backwards compatible mode */
#endif

	mcr p15, 0, r0, c1, c0, 0   /* write SCTLR */
	
	mcr p15, 0, r1, c7, c10, 4  /* data sync buffer */
	mcr p15, 0, r1, c8, c7, 0   /* TLB invalidate */
	mcr p15, 0, r1, c7, c5, 0   /* i-cache sweep */
	mcr p15, 0, r1, c7, c6, 0   /* d-cache sweep */
	mcr p15, 0, r1, c7, c10, 4  /* data sync buffer */
	
    /* Set up a simple stack */
    ldr sp, =KiInitStackBottom
	
#ifdef TARGET_ARMV5
	/* Initialize each exception's stack register.  These are temporary until threads start running. */
	mrs r12, cpsr
	bic r12, r12, #0x1F
	orr r12, r12, #0x11 @ FIQ
	msr cpsr, r12
	ldr sp, =KiFiqStackBottom
	
	mrs r12, cpsr
	bic r12, r12, #0x1F
	orr r12, r12, #0x12 @ IRQ
	msr cpsr, r12
	ldr sp, =KiIrqStackBottom
	
	mrs r12, cpsr
	bic r12, r12, #0x1F
	orr r12, r12, #0x17 @ ABT
	msr cpsr, r12
	ldr sp, =KiAbortStackBottom
	
	mrs r12, cpsr
	bic r12, r12, #0x1F
	orr r12, r12, #0x1B @ UND
	msr cpsr, r12
	ldr sp, =KiUndefinedInstructionStackBottom
	
	mrs r12, cpsr
	bic r12, r12, #0x1F
	orr r12, r12, #0x13 @ SVC
	msr cpsr, r12
#else
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
#endif

	/* jump to KiSystemStartup in .text at 0xC0000000 */
	ldr r0, =KiSystemStartup
	bx  r0

.section .iplbss, "aw", %nobits
.global KiRootPageTable
.global KiRootPageTableDebbie
.global KiRootPageTableJibbie
.global KiPoolHeadersPageTables

/* KEEP THIS at the start of iplbss! */
.align 16
KiRootPageTable:
	.space 16384

/* Root page table Debbie */
KiRootPageTableDebbie:
	.space 4096

/* Root page table Jibbie */
KiRootPageTableJibbie:
	.space 4096

/* Page tables for pool headers */
KiPoolHeadersPageTables:
	.space 4 * 2048

/* ^^^ I promise I will find better names for them soon. */

.section .bss
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

	.global KiBootloaderLpb
KiBootloaderLpb:
	.space 4

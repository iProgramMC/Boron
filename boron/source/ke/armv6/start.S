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
.extern kernel_main

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
	
	mrc p15, 0, r0, c1, c0, 0   /* read SCTLR */
	orr r0, r0, #1              /* enable MMU */
	orr r0, r0, #(1<<2)         /* enable dcache, icache */
	orr r0, r0, #(1<<12)        /* enable icache */
	orr r0, r0, #(1<<13)        /* enable V - interrupt vectors start at 0xFFFF0000 */
	mcr p15, 0, r0, c1, c0, 0   /* write SCTLR */
	
    /* Set up a simple stack */
    ldr sp, =KiInitStackTop

	/* TODO: jump to kernel_main in .text at 0xC0000000 */
	ldr r0, =KiSystemStartup
	bx  r0

.section .iplbss
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
KiInitStackTop:
	.space 2048
stack_top:

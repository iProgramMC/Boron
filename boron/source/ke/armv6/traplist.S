/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/traplist.S
	
Abstract:
    This module contains the implementation for each of the
    individual trap handlers, which call into the common
    trap handler.
	
Author:
	iProgramInCpp - 29 December 2025
***/
.section .text

.extern KiTrapCallList
	
.macro IRQ number
	.global KiIrq\number
	.type   KiIrq\number, %function
KiIrq\number:
	push {r0}
	mov  r0, #\number      @ store interrupt number on the stack
	push {r0}
	b    KiCommonIrqHandler
.endm

KiCommonIrqHandler:
	push {r1-r3, lr}          @ preserve caller saved registers
	mrs  r1, spsr             @ preserve old CPSR
	push {r1}
	ldr  r1, =KiTrapCallList
	ldr  r2, [r1, r0, LSL #2] @ load correct function pointer
	mov  r0, sp               @ provide pointer to KREGISTERS
	blx  r2
	mov  sp, r0               @ reload SP. this behavior isn't really used anymore
	pop  {r1}                 @ restore old CPSR
	msr  spsr_cxsf, r1
	pop  {r1-r3, lr}          @ restore caller saved registers
	pop  {r0}                 @ restore interrupt number
	pop  {r0}                 @ restore r0 which is a caller saved register
	subs pc, lr, #4           @ return from IRQ

@ IRQ list here
IRQ 0
IRQ 1
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15
IRQ 16
IRQ 17
IRQ 18
IRQ 19
IRQ 20
IRQ 21
IRQ 22
IRQ 23
IRQ 24
IRQ 25
IRQ 26
IRQ 27
IRQ 28
IRQ 29
IRQ 30
IRQ 31

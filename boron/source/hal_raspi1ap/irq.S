/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/irq.S
	
Abstract:
	This module contains the IRQ handlers.
	
Author:
	iProgramInCpp - 31 December 2025
***/
	.section .text
	.extern KeDispatchInterruptRequest

.macro IRQ number
	.global HalIrq\number
	.type   HalIrq\number, %function
HalIrq\number:
	mov  r1, r0
	mov  r0, #\number
	b    KeDispatchInterruptRequest
.endm

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

.macro IRQD number
	.word HalIrq\number
.endm

	.section .data
	.global HalIrqList
HalIrqList:
	IRQD 0
	IRQD 1
	IRQD 2
	IRQD 3
	IRQD 4
	IRQD 5
	IRQD 6
	IRQD 7
	IRQD 8
	IRQD 9
	IRQD 10
	IRQD 11
	IRQD 12
	IRQD 13
	IRQD 14
	IRQD 15
	IRQD 16
	IRQD 17
	IRQD 18
	IRQD 19
	IRQD 20
	IRQD 21
	IRQD 22
	IRQD 23
	IRQD 24
	IRQD 25
	IRQD 26
	IRQD 27
	IRQD 28
	IRQD 29
	IRQD 30
	IRQD 31

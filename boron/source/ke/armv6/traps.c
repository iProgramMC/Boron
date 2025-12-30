/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/traps.c
	
Abstract:
	This header file implements support for the IDT (Interrupt
	Dispatch Table).
	
Author:
	iProgramInCpp - 28 December 2025
***/
#include <arch.h>
#include <string.h>
#include <ke.h>
#include <hal.h>
#include <except.h>

#define MAX_IRQS (32) // PL192 has 32 IRQs, PL190 has only 16

static KSPIN_LOCK KiTrapLock;

extern void* const KiTrapList[];
int8_t KiTrapIplList[MAX_IRQS];
void*  KiTrapCallList[MAX_IRQS];

void KeRegisterInterrupt(int Vector, PKINTERRUPT_HANDLER Handler)
{
	KIPL Ipl;
	KeAcquireSpinLock(&KiTrapLock, &Ipl);
	KiTrapCallList[Vector] = Handler;
	KeReleaseSpinLock(&KiTrapLock, Ipl);
}

void KeSetInterruptIPL(int Vector, KIPL Ipl)
{
	KiTrapIplList[Vector] = Ipl;
}

void KiHandleInstructionFault(PKREGISTERS Registers)
{
	KeGetCurrentThread()->HandlingInstructionFault = true;
	KeOnPageFault(Registers);
}

void KiHandleDataFault(PKREGISTERS Registers)
{
	KeGetCurrentThread()->HandlingInstructionFault = false;
	KeOnPageFault(Registers);
}

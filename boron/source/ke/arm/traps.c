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

bool KiInitHandlingInstructionFault;

bool KiHandlingInstructionFault()
{
	if (!KeGetCurrentThread())
		return KiInitHandlingInstructionFault;
	
	return KeGetCurrentThread()->HandlingInstructionFault;
}

static void KiSetHandlingInstructionFault(bool If)
{
	if (!KeGetCurrentThread()) {
		KiInitHandlingInstructionFault = If;
		return;
	}
	
	KeGetCurrentThread()->HandlingInstructionFault = If;
}

PKREGISTERS KiHandleInstructionFault(PKREGISTERS Registers)
{
	KiSetHandlingInstructionFault(true);
	KeOnPageFault(Registers);
	return Registers;
}

PKREGISTERS KiHandleDataFault(PKREGISTERS Registers)
{
	KiSetHandlingInstructionFault(false);
	KeOnPageFault(Registers);
	return Registers;
}

PKREGISTERS KiHandleUndefinedInstructionFault(PKREGISTERS Registers)
{
	KeOnUndefinedInstruction(Registers);
	return Registers;
}

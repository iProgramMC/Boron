/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/i386/thredsup.c
	
Abstract:
	This module implements the architecture specific
	thread state setup routine.
	
Author:
	iProgramInCpp - 14 October 2025
***/
#include <arch.h>
#include <ke.h>
#include <string.h>

NO_RETURN void KiThreadEntryPoint();

void KiSetupRegistersThread(PKTHREAD Thread)
{
	// Subtract 10 from the stack pointer to keep the final stack frame valid.
	uintptr_t StackBottom = ((uintptr_t) Thread->Stack.Top + (uintptr_t) Thread->Stack.Size - 0x10) & ~0xF;
	uint32_t* StackPointer = (uint32_t*) StackBottom;
	
	// KiPopEverythingAndReturn pops these
	*(--StackPointer) = (uint32_t) KiThreadEntryPoint; // Set return address
	*(--StackPointer) = 0; // Set EBP
	*(--StackPointer) = 0x200; // Set IF when entering the thread
	*(--StackPointer) = 0; // Set EBX
	*(--StackPointer) = (uint32_t) Thread->StartContext; // Set ESI
	*(--StackPointer) = (uint32_t) Thread->StartRoutine; // Set EDI
	
	Thread->StackPointer = StackPointer;
}

/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/amd64/thredsup.c
	
Abstract:
	This module implements the architecture specific
	thread state setup routine.
	
Author:
	iProgramInCpp - 9 October 2023
***/
#include <arch.h>
#include <ke.h>
#include <string.h>
#include "../ki.h"

NO_RETURN void KiThreadEntryPoint();

void KiSetupRegistersThread(PKTHREAD Thread)
{
	// Subtract 10 from the stack pointer to keep the final stack frame valid.
	uintptr_t StackBottom = ((uintptr_t) Thread->Stack.Top + (uintptr_t) Thread->Stack.Size - 0x10) & ~0xF;
	
	uint64_t* StackPointer = (uint64_t*) StackBottom;
	
	*(--StackPointer) = (uint64_t) KiThreadEntryPoint; // Set return address
	*(--StackPointer) = 0x200; // Set IF when entering the thread
	*(--StackPointer) = 0; // Set RBP
	*(--StackPointer) = (uint64_t) Thread->StartRoutine; // Set RBX
	*(--StackPointer) = (uint64_t) Thread->StartContext; // Set R12
	*(--StackPointer) = 0; // Set R13
	*(--StackPointer) = 0; // Set R14
	*(--StackPointer) = 0; // Set R15
	
	Thread->StackPointer = StackPointer;
	
	memset(&Thread->ArchContext, 0, sizeof Thread->ArchContext);
}

void KiFxsave();
void KiFxrstor();

extern uint8_t KiFxsaveData[];

void KiSwitchArchSpecificContext(PKTHREAD NewThread, PKTHREAD OldThread)
{
	if (!OldThread)
		return;
	
	KiAssertOwnDispatcherLock();
	KiFxsave();
	memcpy(OldThread->ArchContext.Data, KiFxsaveData, 512);
	memcpy(KiFxsaveData, NewThread->ArchContext.Data, 512);
	KiFxrstor();
}

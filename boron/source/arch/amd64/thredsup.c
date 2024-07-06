/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	arch/amd64/thredsup.c
	
Abstract:
	This module implements the architecture specific
	thread state setup routine.
	
Author:
	iProgramInCpp - 9 October 2023
***/
#include <arch.h>
#include <ke.h>
#include <string.h>

NO_RETURN void KiThreadEntryPoint();

void KiSetupRegistersThread(PKTHREAD Thread)
{
	uintptr_t StackBottom = ((uintptr_t) Thread->Stack.Top + (uintptr_t) Thread->Stack.Size) & ~0xF;
	
	uint64_t* StackPointer = (uint64_t*) StackBottom;
	
	*(--StackPointer) = (uint64_t) KiThreadEntryPoint; // Set return address
	*(--StackPointer) = 0x200; // Set IF when entering the thread
	*(--StackPointer) = SEG_RING_0_DATA; // Set DS
	*(--StackPointer) = SEG_RING_0_DATA; // Set ES
	*(--StackPointer) = SEG_RING_0_DATA; // Set FS
	*(--StackPointer) = SEG_RING_0_DATA; // Set GS
	*(--StackPointer) = 0; // Set RBP
	*(--StackPointer) = (uint64_t) Thread->StartRoutine; // Set RBX
	*(--StackPointer) = (uint64_t) Thread->StartContext; // Set R12
	*(--StackPointer) = 0; // Set R13
	*(--StackPointer) = 0; // Set R14
	*(--StackPointer) = 0; // Set R15
	
	Thread->StackPointer = StackPointer;
}

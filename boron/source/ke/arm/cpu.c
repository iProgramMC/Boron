/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/cpu.c
	
Abstract:
	This module implements certain utility functions,
	as well as certain parts of UP initialization code.
	
Author:
	iProgramInCpp - 28 December 2025
***/
#include <main.h>
#include <arch.h>
#include <mm.h>
#include <string.h>
#include "../../ke/ki.h"

#ifdef CONFIG_SMP
#error SMP not supported for this platform!
#endif

static void* KiCpuPointer;

void* KeGetCPUPointer()
{
	return KiCpuPointer;
}

void KeSetCPUPointer(void* Ptr)
{
	KiCpuPointer = Ptr;
}

INIT
void KeInitCPU()
{
	KiSwitchToAddressSpaceProcess(KeGetSystemProcess());
	
	// TODO
}

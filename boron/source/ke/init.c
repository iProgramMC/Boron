/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/init.c
	
Abstract:
	This module implements the system startup function.
	
Author:
	iProgramInCpp - 28 August 2023
***/

#include <main.h>
#include <hal.h>
#include <mm.h>
#include <ldr.h>
#include "ki.h"
#include <rtl/symdefs.h>

// The entry point to our kernel.
NO_RETURN INIT
void KiSystemStartup(void)
{
	DbgInit();
	KiInitLoaderParameterBlock();
	MiInitPMM();
	MmInitAllocators();
	KeSchedulerInitUP();
	KeInitArchUP();
	LdrInit();
	LdrInitializeHal();
	HalInitSystemUP();
	LdrInitAfterHal();
	KeInitSMP(); // no return
}

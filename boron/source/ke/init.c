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

#include <limreq.h>
#include <main.h>
#include <hal.h>
#include <mm.h>
#include <ldr.h>
#include "ki.h"
#include <rtl/symdefs.h>
//#include <cpuid.h>

// The entry point to our kernel.
void KiSystemStartup(void)
{
	DbgPrint("Boron is starting up");
	
	HalTerminalInit();
	LogMsg("Boron (TM), October 2023 - V0.004");
	
	DbgPrint("%p %d", KiSymbolTable, KiSymbolTableSize);
	
	MiInitPMM();
	MmInitAllocators();
	KeSchedulerInitUP();
	
	LdrInit();
	
	// phase 1 of HAL initialization on the BSP:
	HalUPInit();
	
	// initialize SMP. This function doesn't return
	KeInitSMP();
}


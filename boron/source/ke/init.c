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

// The entry point to our kernel.
NO_RETURN void KiSystemStartup(void)
{
	DbgSetProgressBarSteps(10);
	MiInitPMM();
	MmInitAllocators();
	KeSchedulerInitUP();
	KeInitArchUP();
	LdrInit();
	KeInitSMP(); // no return
}


/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/stop.c
	
Abstract:
	This module implements the function to stop the current CPU.
	
Author:
	iProgramInCpp - 20 August 2023
***/

#include <ke.h>
#include <arch.h>

// Stops the current CPU
void KeStopCurrentCPU()
{
	KeSetInterruptsEnabled(false);
	
	while (true)
		KeWaitForNextInterrupt();
}

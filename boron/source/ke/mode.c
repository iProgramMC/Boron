/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/mode.c
	
Abstract:
	This module implements the code regarding processor
	execution modes.
	
Author:
	iProgramInCpp - 27 November 2023
***/
#include <ke.h>

KPROCESSOR_MODE KeGetPreviousMode()
{
	// Only true during initialization
	if (!KeGetCurrentThread())
		return MODE_KERNEL;
	
	return KeGetCurrentThread()->Mode;
}
